-- DOT-LS completion 功能测试脚本
-- 用法: lua client/test_completion.lua [server_path]
-- 依赖: lua-cjson (luarocks install lua-cjson)

------------ JSON 编解码 (cjson) ------------
local json = require("cjson")
------------ JSON 结束 ------------

local function log(level, msg)
    io.stderr:write(string.format("[%s] %s: %s\n", os.date("%H:%M:%S"), level, msg))
    io.stderr:flush()
end

local server_path = arg[1] or "../build/Debug/dot-ls"

-- 构建所有请求消息
local function make_lsp_message(tbl)
    local body = json.encode(tbl)
    return "Content-Length: " .. #body .. "\r\n\r\n" .. body
end

-- 构建完整的请求序列
local messages = {}

-- 1. initialize
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    id = 1,
    method = "initialize",
    params = {
        processId = 12345,
        clientInfo = { name = "test-client", version = "1.0" },
        locale = "en",
        rootPath = "/tmp",
        rootUri = "file:///tmp",
        capabilities = {},
        trace = "off"
    }
})

-- 2. initialized (notification)
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    method = "initialized",
    params = {}
})

-- 3. didOpen
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    method = "textDocument/didOpen",
    params = {
        textDocument = {
            uri = "file:///tmp/test.dot",
            languageId = "dot",
            version = 1,
            text = "digraph G {\n    node [shape=box];\n    A -> B;\n    B -> C;\n}"
        }
    }
})

-- 4a. top-level completion (position 0,0 — TopLevel context → snippets + keywords)
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    id = 2,
    method = "textDocument/completion",
    params = {
        textDocument = { uri = "file:///tmp/test.dot" },
        position = { line = 0, character = 0 }
    }
})

-- 4b. attribute-name completion (inside [...] — AttributeName context)
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    id = 4,
    method = "textDocument/completion",
    params = {
        textDocument = { uri = "file:///tmp/test.dot" },
        position = { line = 1, character = 15 }  -- inside "[shape=box]", after "["
    }
})

-- 5. shutdown
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    id = 3,
    method = "shutdown",
    params = {}
})

-- 6. exit
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    method = "exit",
    params = {}
})

-- 写入临时文件作为 stdin
local input_data = table.concat(messages)
local tmp_input = os.tmpname()
local f = io.open(tmp_input, "wb")
f:write(input_data)
f:close()

log("INFO", "===== DOT-LS Completion Test Suite =====")
log("INFO", "Input size: " .. #input_data .. " bytes")

-- 运行服务器，stdin 从文件读取，stdout 捕获
local cmd = string.format("%s < %s 2>/dev/null", server_path, tmp_input)
local ph = io.popen(cmd, "r")
local raw_output = ph:read("*a")
ph:close()
os.remove(tmp_input)

log("INFO", "Output size: " .. #raw_output .. " bytes")

-- 解析所有 LSP 响应
local responses = {}
local pos = 1
while pos <= #raw_output do
    -- 跳过非 Content-Length 前缀 (如 debug 输出)
    local cl_start = raw_output:find("Content%-Length: ", pos)
    if not cl_start then break end

    local cl_end = raw_output:find("\r\n", cl_start)
    if not cl_end then break end

    local len_str = raw_output:sub(cl_start + 16, cl_end - 1)
    local content_len = tonumber(len_str)
    if not content_len then break end

    -- 跳过 \r\n\r\n
    local body_start = raw_output:find("\r\n\r\n", cl_start)
    if not body_start then break end
    body_start = body_start + 4

    local body = raw_output:sub(body_start, body_start + content_len - 1)
    local ok, parsed = pcall(json.decode, body)
    if ok then
        responses[#responses + 1] = parsed
        log("INFO", "Parsed response id=" .. tostring(parsed.id or "nil"))
    else
        log("WARN", "Failed to parse response: " .. tostring(parsed))
    end
    pos = body_start + content_len
end

log("INFO", "Got " .. #responses .. " responses")

-- ==================== 验证测试 ====================
local test_count = 0
local pass_count = 0
local fail_count = 0

local function assert_true(desc, cond)
    test_count = test_count + 1
    if cond then
        pass_count = pass_count + 1
        log("PASS", desc)
    else
        fail_count = fail_count + 1
        log("FAIL", desc)
    end
end

-- 查找响应 by id
local function find_response(id)
    for _, r in ipairs(responses) do
        if r.id == id then return r end
    end
    return nil
end

-- Test 1: Initialize response
log("INFO", "--- Test: Initialize ---")
local init_resp = find_response(1)
assert_true("initialize returns response", init_resp ~= nil)
assert_true("initialize has result", init_resp and init_resp.result ~= nil)

if init_resp and init_resp.result then
    local caps = init_resp.result.capabilities
    assert_true("server has completionProvider", caps and caps.completionProvider ~= nil)
    if caps and caps.completionProvider then
        assert_true("completionProvider has triggerCharacters",
            caps.completionProvider.triggerCharacters ~= nil)
        log("INFO", "triggerCharacters: " .. json.encode(caps.completionProvider.triggerCharacters or {}))
    end
    local info = init_resp.result.serverInfo
    assert_true("serverInfo.name == 'dot-ls'", info and info.name == "dot-ls")
    assert_true("serverInfo.version exists", info and info.version ~= nil)
    log("INFO", "Server: " .. (info and info.name or "?") .. " " .. (info and info.version or "?"))

    -- 验证 textDocumentSync
    local sync = caps and caps.textDocumentSync
    assert_true("textDocumentSync.openClose == true", sync and sync.openClose == true)
end

-- Test 2a: Top-level completion response (snippets + keywords)
log("INFO", "--- Test: Top-level Completion (snippets + keywords) ---")
local comp_resp = find_response(2)
assert_true("top-level completion returns response", comp_resp ~= nil)
assert_true("top-level completion has result", comp_resp and comp_resp.result ~= nil)

if comp_resp and comp_resp.result then
    local result = comp_resp.result
    assert_true("result has items", result.items ~= nil)
    assert_true("result.isIncomplete is false", result.isIncomplete == false)

    if result.items then
        local item_count = #result.items
        assert_true("items count > 0", item_count > 0)
        log("INFO", "Got " .. item_count .. " top-level completion items")

        local found_keywords = {}
        local found_snippets = {}

        for _, item in ipairs(result.items) do
            if item.kind == 14 then found_keywords[item.label] = true end  -- Keyword
            if item.kind == 15 then found_snippets[item.label] = true end  -- Snippet
        end

        -- 验证关键字
        local expected_keywords = { "node", "edge", "graph", "digraph", "subgraph", "strict" }
        for _, kw in ipairs(expected_keywords) do
            assert_true("keyword '" .. kw .. "' present", found_keywords[kw] == true)
        end

        -- 验证 Snippet 模板
        assert_true("snippet 'digraph' present",  found_snippets["digraph"]  == true)
        assert_true("snippet 'graph' present",    found_snippets["graph"]    == true)
        assert_true("snippet 'subgraph' present", found_snippets["subgraph"] == true)

        -- 验证 item 结构完整性
        local first = result.items[1]
        assert_true("item has label",      first.label ~= nil and first.label ~= "")
        assert_true("item has kind",       first.kind ~= nil)
        assert_true("item has insertText", first.insertText ~= nil)
    end
end

-- Test 2b: Attribute-name completion response (inside [...])
log("INFO", "--- Test: Attribute-name Completion ---")
local attr_resp = find_response(4)
assert_true("attr completion returns response", attr_resp ~= nil)
if attr_resp and attr_resp.result then
    local result = attr_resp.result
    assert_true("attr result has items", result.items ~= nil)
    if result.items then
        local found_props = {}
        for _, item in ipairs(result.items) do
            if item.kind == 10 then found_props[item.label] = true end  -- Property
        end
        assert_true("attr 'label' present",   found_props["label"]   == true)
        assert_true("attr 'color' present",   found_props["color"]   == true)
        assert_true("attr 'shape' present",   found_props["shape"]   == true)
        assert_true("attr 'style' present",   found_props["style"]   == true)
        assert_true("attr 'rankdir' present", found_props["rankdir"] == true)
        log("INFO", "Got " .. #result.items .. " attribute completion items")
    end
end

-- Test 3: Shutdown response
log("INFO", "--- Test: Shutdown ---")
local shut_resp = find_response(3)
assert_true("shutdown returns response", shut_resp ~= nil)

-- ==================== 结果汇总 ====================
log("INFO", "")
log("INFO", "===== Test Results =====")
log("INFO", string.format("Total: %d | Pass: %d | Fail: %d", test_count, pass_count, fail_count))

if fail_count > 0 then
    log("ERROR", "SOME TESTS FAILED!")
    os.exit(1)
else
    log("INFO", "ALL TESTS PASSED!")
    os.exit(0)
end
