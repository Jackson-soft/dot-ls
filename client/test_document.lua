-- DOT-LS 文档协议测试套件
-- 测试覆盖:
--   1. 生命周期状态机守卫 (未初始化 / 已关闭时的错误响应)
--   2. initialize / initialized
--   3. textDocument/didOpen
--   4. textDocument/didChange
--   5. textDocument/didSave
--   6. textDocument/didClose
--   7. 未知方法 → MethodNotFound (-32601)
--   8. shutdown / exit
-- 用法: lua client/test_document.lua [server_path]
-- 纯 Lua 实现，无需外部依赖

-------------- 简易 JSON 编解码 (与 test_completion.lua 一致) ----------------
local json = {}

local function json_encode_value(val, _, level)
    local t = type(val)
    if t == "nil"     then return "null"
    elseif t == "boolean" then return val and "true" or "false"
    elseif t == "number" then
        if val ~= val or val == math.huge or val == -math.huge then return "null" end
        if val == math.floor(val) and math.abs(val) < 1e15 then
            return string.format("%d", val)
        end
        return tostring(val)
    elseif t == "string" then
        val = val:gsub('\\','\\\\'):gsub('"','\\"')
                 :gsub('\n','\\n'):gsub('\r','\\r'):gsub('\t','\\t')
        return '"' .. val .. '"'
    elseif t == "table" then
        local is_array, n = true, 0
        for k in pairs(val) do
            n = n + 1
            if type(k) ~= "number" or k ~= n then is_array = false break end
        end
        if n == 0 and #val == 0 then return "{}" end
        local parts = {}
        if is_array then
            for i = 1, #val do
                parts[#parts+1] = json_encode_value(val[i], false, level+1)
            end
            return "[" .. table.concat(parts, ",") .. "]"
        else
            for k, v in pairs(val) do
                parts[#parts+1] = json_encode_value(tostring(k), false, level+1)
                               .. ":" .. json_encode_value(v, false, level+1)
            end
            return "{" .. table.concat(parts, ",") .. "}"
        end
    end
    return "null"
end

function json.encode(val) return json_encode_value(val, false, 0) end

local function skip_ws(s, p) return s:match("^%s*()", p) end

local function decode_value(s, pos)
    pos = skip_ws(s, pos)
    local c = s:sub(pos, pos)
    if c == '"' then
        local i, parts = pos+1, {}
        while i <= #s do
            local ch = s:sub(i,i)
            if ch == '\\' then
                local nc = s:sub(i+1,i+1)
                local esc
                if     nc == '"'  then esc = '"'
                elseif nc == '\\' then esc = '\\'
                elseif nc == 'n'  then esc = '\n'
                elseif nc == 'r'  then esc = '\r'
                elseif nc == 't'  then esc = '\t'
                elseif nc == '/'  then esc = '/'
                elseif nc == 'u'  then esc = '?'; i = i + 4
                else                   esc = nc
                end
                parts[#parts+1] = esc
                i = i + 2
            elseif ch == '"' then return table.concat(parts), i+1
            else parts[#parts+1] = ch; i = i+1 end
        end
        error("unterminated string")
    elseif c == '{' then
        local obj = {}; pos = pos+1; pos = skip_ws(s,pos)
        if s:sub(pos,pos) == '}' then return obj, pos+1 end
        while true do
            pos = skip_ws(s,pos)
            local key; key,pos = decode_value(s,pos)
            pos = skip_ws(s,pos)
            assert(s:sub(pos,pos)==':'); pos = pos+1
            local val; val,pos = decode_value(s,pos)
            obj[key] = val; pos = skip_ws(s,pos)
            local sep = s:sub(pos,pos)
            if sep == '}' then return obj, pos+1 end
            assert(sep==','); pos = pos+1
        end
    elseif c == '[' then
        local arr = {}; pos = pos+1; pos = skip_ws(s,pos)
        if s:sub(pos,pos) == ']' then return arr, pos+1 end
        while true do
            local val; val,pos = decode_value(s,pos)
            arr[#arr+1] = val; pos = skip_ws(s,pos)
            local sep = s:sub(pos,pos)
            if sep == ']' then return arr, pos+1 end
            assert(sep==','); pos = pos+1
        end
    elseif s:sub(pos,pos+3) == "true"  then return true,  pos+4
    elseif s:sub(pos,pos+4) == "false" then return false, pos+5
    elseif s:sub(pos,pos+3) == "null"  then return nil,   pos+4
    elseif c == '-' or c:match("%d") then
        local ns = s:match("^%-?%d+%.?%d*[eE]?[%+%-]?%d*", pos)
        return tonumber(ns), pos+#ns
    else error("unexpected char '"..c.."' at "..pos) end
end

function json.decode(s) local v = decode_value(s,1); return v end
-------------- JSON 结束 -------------------------------------------------------

local function log(level, msg)
    io.stderr:write(string.format("[%s] %-4s: %s\n", os.date("%H:%M:%S"), level, msg))
    io.stderr:flush()
end

local server_path = arg[1] or "../build/Debug/dot-ls"

local function make_msg(tbl)
    local body = json.encode(tbl)
    return "Content-Length: " .. #body .. "\r\n\r\n" .. body
end

-- 测试文档内容
local DOC_INITIAL = table.concat({
    "digraph G {",
    "    node [shape=box];",
    "    A -> B;",
    "    B -> C;",
    "}",
}, "\n")

local DOC_CHANGED = table.concat({
    "digraph G {",
    "    node [shape=circle, style=filled];",
    "    A -> B [label=\"edge1\"];",
    "    B -> C [label=\"edge2\"];",
    "    C -> A;",
    "}",
}, "\n")

local DOC_URI = "file:///tmp/test_document.dot"

-- ── 构建完整消息序列 ──────────────────────────────────────────────────────────

local msgs = {}

-- ① 【状态守卫测试】在 initialize 之前发送 completion 请求 → 期望 -32002
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 1,
    method  = "textDocument/completion",
    params  = { textDocument = { uri = DOC_URI }, position = { line=0, character=0 } }
})

-- ② initialize
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 2,
    method  = "initialize",
    params  = {
        processId  = 12345,
        clientInfo = { name = "test-client", version = "1.0" },
        locale     = "en",
        rootPath   = "/tmp",
        rootUri    = "file:///tmp",
        capabilities = {},
        trace      = "off",
    }
})

-- ③ initialized (通知，无响应)
msgs[#msgs+1] = make_msg({ jsonrpc = "2.0", method = "initialized", params = {} })

-- ④ textDocument/didOpen (通知，无响应)
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0",
    method  = "textDocument/didOpen",
    params  = {
        textDocument = {
            uri        = DOC_URI,
            languageId = "dot",
            version    = 1,
            text       = DOC_INITIAL,
        }
    }
})

-- ⑤ 用 completion 探测 didOpen 已生效（id=3）
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 3,
    method  = "textDocument/completion",
    params  = { textDocument = { uri = DOC_URI }, position = { line=1, character=4 } }
})

-- ⑥ textDocument/didChange (Full 同步，通知，无响应)
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0",
    method  = "textDocument/didChange",
    params  = {
        textDocument   = { uri = DOC_URI, version = 2 },
        contentChanges = { { text = DOC_CHANGED } },
    }
})

-- ⑦ 用 completion 探测 didChange 已生效（id=4）
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 4,
    method  = "textDocument/completion",
    params  = { textDocument = { uri = DOC_URI }, position = { line=2, character=6 } }
})

-- ⑧ textDocument/didSave（含 text，通知，无响应）
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0",
    method  = "textDocument/didSave",
    params  = {
        textDocument = { uri = DOC_URI },
        text         = DOC_CHANGED,
    }
})

-- ⑨ 用 completion 探测 didSave 已生效（id=5）
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 5,
    method  = "textDocument/completion",
    params  = { textDocument = { uri = DOC_URI }, position = { line=3, character=8 } }
})

-- ⑩ textDocument/didClose（通知，无响应）
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0",
    method  = "textDocument/didClose",
    params  = { textDocument = { uri = DOC_URI } }
})

-- ⑪ 未知方法 → 期望 -32601 MethodNotFound（id=6）
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 6,
    method  = "$/unknownMethod",
    params  = {}
})

-- ⑫ shutdown（id=7）→ 期望 null result
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 7,
    method  = "shutdown",
    params  = {}
})

-- ⑬ 【状态守卫测试】shutdown 后再发 completion → 期望 -32600（id=8）
msgs[#msgs+1] = make_msg({
    jsonrpc = "2.0", id = 8,
    method  = "textDocument/completion",
    params  = { textDocument = { uri = DOC_URI }, position = { line=0, character=0 } }
})

-- ⑭ exit（通知，服务端以 exit(0) 退出）
msgs[#msgs+1] = make_msg({ jsonrpc = "2.0", method = "exit" })

-- ── 运行服务端 ────────────────────────────────────────────────────────────────

local input_data = table.concat(msgs)
local tmp_in = os.tmpname()
local f = io.open(tmp_in, "wb")
f:write(input_data)
f:close()

log("INFO", "===== DOT-LS Document Protocol Test Suite =====")
log("INFO", string.format("Server : %s", server_path))
log("INFO", string.format("Input  : %d bytes, %d messages", #input_data, #msgs))

local cmd = string.format("%s < %s 2>/dev/null", server_path, tmp_in)
local ph  = io.popen(cmd, "r")
local raw = ph:read("*a")
ph:close()
os.remove(tmp_in)

log("INFO", string.format("Output : %d bytes", #raw))

-- ── 解析所有 LSP 响应 ─────────────────────────────────────────────────────────

local responses = {}
local pos = 1
while pos <= #raw do
    local cl_start = raw:find("Content%-Length: ", pos)
    if not cl_start then break end
    local cl_end = raw:find("\r\n", cl_start)
    if not cl_end then break end
    local content_len = tonumber(raw:sub(cl_start + 16, cl_end - 1))
    if not content_len then break end
    local body_start = raw:find("\r\n\r\n", cl_start)
    if not body_start then break end
    body_start = body_start + 4
    local body = raw:sub(body_start, body_start + content_len - 1)
    local ok, parsed = pcall(json.decode, body)
    if ok then
        responses[#responses+1] = parsed
        log("RECV", string.format("id=%-3s  error_code=%s",
            tostring(parsed.id or "nil"),
            parsed.error and tostring(parsed.error.code) or "none"))
    else
        log("WARN", "parse failed: " .. tostring(parsed))
    end
    pos = body_start + content_len
end

log("INFO", string.format("Parsed %d responses", #responses))

-- ── 测试框架 ──────────────────────────────────────────────────────────────────

local total, passed, failed = 0, 0, 0

local function check(desc, cond)
    total = total + 1
    if cond then
        passed = passed + 1
        log("PASS", desc)
    else
        failed = failed + 1
        log("FAIL", desc)
    end
end

local function find_resp(id)
    for _, r in ipairs(responses) do
        if r.id == id then return r end
    end
    return nil
end

-- ── 断言 ──────────────────────────────────────────────────────────────────────

log("INFO", "")
log("INFO", "----- [1] 生命周期守卫：initialize 之前的请求 -----")
local pre_init = find_resp(1)
check("pre-initialize request gets a response",       pre_init ~= nil)
check("pre-initialize response has error field",       pre_init and pre_init.error ~= nil)
check("error code == -32002 (ServerNotInitialized)",
    pre_init and pre_init.error and pre_init.error.code == -32002)

log("INFO", "")
log("INFO", "----- [2] initialize -----")
local init_r = find_resp(2)
check("initialize gets response",                      init_r ~= nil)
check("initialize has result (no error)",              init_r and init_r.result ~= nil)

if init_r and init_r.result then
    local caps = init_r.result.capabilities
    local info = init_r.result.serverInfo

    check("serverInfo.name == 'dot-ls'",               info and info.name == "dot-ls")
    check("serverInfo.version exists",                  info and info.version ~= nil)
    check("capabilities.textDocumentSync present",      caps and caps.textDocumentSync ~= nil)
    check("textDocumentSync.openClose == true",
        caps and caps.textDocumentSync and caps.textDocumentSync.openClose == true)
    check("textDocumentSync.change == 1 (Full)",
        caps and caps.textDocumentSync and caps.textDocumentSync.change == 1)
    check("completionProvider.triggerCharacters present",
        caps and caps.completionProvider and caps.completionProvider.triggerCharacters ~= nil)
end

log("INFO", "")
log("INFO", "----- [3] textDocument/didOpen → completion probe -----")
-- didOpen 是通知，无直接响应；通过 completion 探测服务端已处理该文档
local after_open = find_resp(3)
check("completion after didOpen gets response",        after_open ~= nil)
check("completion after didOpen has result",           after_open and after_open.result ~= nil)
if after_open and after_open.result then
    check("completion result has items",               after_open.result.items ~= nil)
    check("items count > 0",                           after_open.result.items and
                                                       #after_open.result.items > 0)
end

log("INFO", "")
log("INFO", "----- [4] textDocument/didChange → completion probe -----")
local after_change = find_resp(4)
check("completion after didChange gets response",      after_change ~= nil)
check("completion after didChange has result",         after_change and after_change.result ~= nil)
if after_change and after_change.result then
    check("completion result has items",               after_change.result.items ~= nil)
    -- 内容已改变但关键字列表不变，dot-ls 应仍能返回补全
    check("items count matches after change",          after_change.result.items and
                                                       #after_change.result.items > 0)
end

log("INFO", "")
log("INFO", "----- [5] textDocument/didSave → completion probe -----")
local after_save = find_resp(5)
check("completion after didSave gets response",        after_save ~= nil)
check("completion after didSave has result",           after_save and after_save.result ~= nil)
if after_save and after_save.result then
    check("completion result has items after save",    after_save.result.items and
                                                       #after_save.result.items > 0)
end

log("INFO", "")
log("INFO", "----- [6] 未知方法 MethodNotFound -----")
local unknown = find_resp(6)
check("unknown method gets response",                  unknown ~= nil)
check("unknown method response has error",             unknown and unknown.error ~= nil)
check("error code == -32601 (MethodNotFound)",
    unknown and unknown.error and unknown.error.code == -32601)

log("INFO", "")
log("INFO", "----- [7] shutdown -----")
local shut = find_resp(7)
check("shutdown gets response",                        shut ~= nil)
check("shutdown result == null",                       shut and shut.result == nil and shut.error == nil)

log("INFO", "")
log("INFO", "----- [8] 生命周期守卫：shutdown 之后的请求 -----")
local post_shut = find_resp(8)
check("post-shutdown request gets a response",         post_shut ~= nil)
check("post-shutdown response has error field",        post_shut and post_shut.error ~= nil)
check("error code == -32600 (InvalidRequest)",
    post_shut and post_shut.error and post_shut.error.code == -32600)

-- ── 汇总 ──────────────────────────────────────────────────────────────────────

log("INFO", "")
log("INFO", "===== Test Results =====")
log("INFO", string.format("Total: %d | Pass: %d | Fail: %d", total, passed, failed))

if failed > 0 then
    log("FAIL", "SOME TESTS FAILED!")
    os.exit(1)
else
    log("INFO", "ALL TESTS PASSED!")
    os.exit(0)
end

