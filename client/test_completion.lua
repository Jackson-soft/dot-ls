-- DOT-LS completion 功能测试脚本
-- 用法: lua client/test_completion.lua [server_path]
-- 纯 Lua 实现，无需外部依赖

------------ 简易 JSON 编解码 ------------
local json = {}

local function json_encode_value(val, indent, level)
    local t = type(val)
    if t == "nil" then
        return "null"
    elseif t == "boolean" then
        return val and "true" or "false"
    elseif t == "number" then
        if val ~= val then return "null" end -- NaN
        if val == math.huge or val == -math.huge then return "null" end
        if val == math.floor(val) and math.abs(val) < 1e15 then
            return string.format("%d", val)
        end
        return tostring(val)
    elseif t == "string" then
        -- escape special characters
        val = val:gsub('\\', '\\\\'):gsub('"', '\\"')
                 :gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
        return '"' .. val .. '"'
    elseif t == "table" then
        -- detect array vs object
        local is_array = true
        local n = 0
        for k, _ in pairs(val) do
            n = n + 1
            if type(k) ~= "number" or k ~= n then
                is_array = false
                break
            end
        end
        if n == 0 then
            -- check if #val == 0 too (empty table => object for safety)
            if #val == 0 then return "{}" end
        end

        local parts = {}
        if is_array then
            for i = 1, #val do
                parts[#parts + 1] = json_encode_value(val[i], indent, level + 1)
            end
            return "[" .. table.concat(parts, ",") .. "]"
        else
            for k, v in pairs(val) do
                parts[#parts + 1] = json_encode_value(tostring(k), indent, level + 1) .. ":" .. json_encode_value(v, indent, level + 1)
            end
            return "{" .. table.concat(parts, ",") .. "}"
        end
    end
    return "null"
end

function json.encode(val)
    return json_encode_value(val, false, 0)
end

-- minimal JSON decoder
local function skip_whitespace(s, pos)
    return s:match("^%s*()", pos)
end

local function decode_value(s, pos)
    pos = skip_whitespace(s, pos)
    local c = s:sub(pos, pos)

    if c == '"' then
        -- string
        local i = pos + 1
        local parts = {}
        while i <= #s do
            local ch = s:sub(i, i)
            if ch == '\\' then
                local next_ch = s:sub(i + 1, i + 1)
                if next_ch == '"' then parts[#parts + 1] = '"'
                elseif next_ch == '\\' then parts[#parts + 1] = '\\'
                elseif next_ch == 'n' then parts[#parts + 1] = '\n'
                elseif next_ch == 'r' then parts[#parts + 1] = '\r'
                elseif next_ch == 't' then parts[#parts + 1] = '\t'
                elseif next_ch == '/' then parts[#parts + 1] = '/'
                elseif next_ch == 'u' then
                    parts[#parts + 1] = '?' -- simplified
                    i = i + 4
                else parts[#parts + 1] = next_ch end
                i = i + 2
            elseif ch == '"' then
                return table.concat(parts), i + 1
            else
                parts[#parts + 1] = ch
                i = i + 1
            end
        end
        error("unterminated string")
    elseif c == '{' then
        local obj = {}
        pos = pos + 1
        pos = skip_whitespace(s, pos)
        if s:sub(pos, pos) == '}' then return obj, pos + 1 end
        while true do
            pos = skip_whitespace(s, pos)
            local key
            key, pos = decode_value(s, pos)
            pos = skip_whitespace(s, pos)
            if s:sub(pos, pos) ~= ':' then error("expected ':' at " .. pos) end
            pos = pos + 1
            local val
            val, pos = decode_value(s, pos)
            obj[key] = val
            pos = skip_whitespace(s, pos)
            local sep = s:sub(pos, pos)
            if sep == '}' then return obj, pos + 1 end
            if sep ~= ',' then error("expected ',' or '}' at " .. pos) end
            pos = pos + 1
        end
    elseif c == '[' then
        local arr = {}
        pos = pos + 1
        pos = skip_whitespace(s, pos)
        if s:sub(pos, pos) == ']' then return arr, pos + 1 end
        while true do
            local val
            val, pos = decode_value(s, pos)
            arr[#arr + 1] = val
            pos = skip_whitespace(s, pos)
            local sep = s:sub(pos, pos)
            if sep == ']' then return arr, pos + 1 end
            if sep ~= ',' then error("expected ',' or ']' at " .. pos) end
            pos = pos + 1
        end
    elseif s:sub(pos, pos + 3) == "true" then
        return true, pos + 4
    elseif s:sub(pos, pos + 4) == "false" then
        return false, pos + 5
    elseif s:sub(pos, pos + 3) == "null" then
        return nil, pos + 4
    elseif c == '-' or c:match("%d") then
        local num_str = s:match("^%-?%d+%.?%d*[eE]?[%+%-]?%d*", pos)
        return tonumber(num_str), pos + #num_str
    else
        error("unexpected char '" .. c .. "' at pos " .. pos)
    end
end

function json.decode(s)
    local val, _ = decode_value(s, 1)
    return val
end
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

-- 4. completion
messages[#messages + 1] = make_lsp_message({
    jsonrpc = "2.0",
    id = 2,
    method = "textDocument/completion",
    params = {
        textDocument = { uri = "file:///tmp/test.dot" },
        position = { line = 1, character = 4 }
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

-- Test 2: Completion response
log("INFO", "--- Test: Completion ---")
local comp_resp = find_response(2)
assert_true("completion returns response", comp_resp ~= nil)
assert_true("completion has result", comp_resp and comp_resp.result ~= nil)

if comp_resp and comp_resp.result then
    local result = comp_resp.result
    assert_true("result has items", result.items ~= nil)
    assert_true("result.isIncomplete is false", result.isIncomplete == false)

    if result.items then
        local item_count = #result.items
        assert_true("items count > 0", item_count > 0)
        log("INFO", "Got " .. item_count .. " completion items")

        -- 按 kind 分类
        local found_keywords = {}
        local found_properties = {}
        local found_values = {}

        for _, item in ipairs(result.items) do
            if item.kind == 14 then  -- Keyword
                found_keywords[item.label] = true
            elseif item.kind == 10 then  -- Property
                found_properties[item.label] = true
            elseif item.kind == 12 then  -- Value
                found_values[item.label] = true
            end
        end

        -- 验证关键字
        local expected_keywords = { "node", "edge", "graph", "digraph", "subgraph", "strict" }
        for _, kw in ipairs(expected_keywords) do
            assert_true("keyword '" .. kw .. "' present", found_keywords[kw] == true)
        end

        -- 验证属性
        local expected_properties = { "label", "color", "shape", "style", "rankdir" }
        for _, prop in ipairs(expected_properties) do
            assert_true("property '" .. prop .. "' present", found_properties[prop] == true)
        end

        -- 验证 shape 值
        assert_true("shape value 'box' present", found_values["box"] == true)
        assert_true("shape value 'circle' present", found_values["circle"] == true)
        assert_true("shape value 'ellipse' present", found_values["ellipse"] == true)
        assert_true("shape value 'diamond' present", found_values["diamond"] == true)

        -- 验证 item 结构完整性
        local first = result.items[1]
        assert_true("item has label", first.label ~= nil and first.label ~= "")
        assert_true("item has kind", first.kind ~= nil)
        assert_true("item has detail", first.detail ~= nil)
        assert_true("item has insertText", first.insertText ~= nil)

        -- 打印所有 items
        log("INFO", "Completion items:")
        for i, item in ipairs(result.items) do
            local kind_name = "?"
            if item.kind == 14 then kind_name = "Keyword"
            elseif item.kind == 10 then kind_name = "Property"
            elseif item.kind == 12 then kind_name = "Value"
            end
            log("INFO", string.format("  [%d] %s (%s) - %s", i, item.label, kind_name, item.detail or ""))
        end
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
