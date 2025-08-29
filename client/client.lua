local json = require("cjson")
local io = io
local os = os
local tonumber = tonumber
local tostring = tostring
local time = os.time
local date = os.date

local function log(level, msg)
    io.write(string.format("[%s] %s: %s\n", date("%H:%M:%S"), level, msg))
    io.flush()
end

local function pretty_json(s)
    local ok, tbl = pcall(json.decode, s)
    if ok then
        -- cjson does not pretty print; produce compact stable string
        return json.encode(tbl)
    end
    return s
end

-- create unique fifo paths
local pid = tostring(os.getpid and os.getpid() or math.random(10000))
local in_fifo = "/tmp/lsp_in_" .. pid
local out_fifo = "/tmp/lsp_out_" .. pid

os.execute("mkfifo " .. in_fifo .. " 2>/dev/null")
os.execute("mkfifo " .. out_fifo .. " 2>/dev/null")

-- change this to your running server binary if you need to restart it here
local server_path = arg[1] or "../build/Debug/dot-ls"

-- start server redirecting stdio to fifos, get pid
local start_cmd = string.format("(%s < %s > %s 2>&1) & echo $!", server_path, in_fifo, out_fifo)
local ph = io.popen(start_cmd)
local server_pid = ph:read("*l")
ph:close()
log("INFO", "Started server pid=" .. (server_pid or "unknown"))

-- open pipes
local in_fd = io.open(in_fifo, "w")
if not in_fd then
    log("ERROR", "failed to open input fifo: " .. in_fifo)
    os.exit(1)
end

-- open output fifo for reading (blocking read)
local out_fd = io.open(out_fifo, "r")
if not out_fd then
    log("ERROR", "failed to open output fifo: " .. out_fifo)
    in_fd:close()
    os.exit(1)
end

local function send_message(tbl)
    local j = json.encode(tbl)
    local header = "Content-Length: " .. #j .. "\r\n\r\n"
    local full = header .. j
    in_fd:write(full)
    in_fd:flush()

    -- print request to stdout
    print("\n----- REQUEST -----")
    io.write("Content-Length: " .. #j .. "\n")
    print(pretty_json(j))
    io.flush()
end

local function read_response(timeout_seconds)
    timeout_seconds = timeout_seconds or 5
    local start = time()
    -- read headers
    local content_len = nil
    while true do
        if (time() - start) > timeout_seconds then
            return nil, "timeout"
        end
        local line = out_fd:read("*l")
        if not line then
            -- no data yet; small sleep then continue
            os.execute("sleep 0.05")
            goto continue
        end
        ::continue::
        if not line then
            -- try again
        else
            -- strip trailing CR if present
            line = line:gsub("\r$", "")
            if line == "" then
                -- blank line -> headers finished
                if content_len then break end
            else
                local v = line:match("^Content%-Length:%s*(%d+)")
                if v then content_len = tonumber(v) end
            end
        end
    end

    if not content_len or content_len <= 0 then
        return nil, "no-content"
    end

    -- read body exactly content_len bytes
    local body = out_fd:read(content_len)
    if not body then
        return nil, "eof"
    end

    -- print response to stdout
    print("\n----- RESPONSE -----")
    io.write("Content-Length: " .. tostring(content_len) .. "\n")
    print(pretty_json(body))
    io.flush()

    local ok, parsed = pcall(json.decode, body)
    if ok then return parsed end
    return body
end

-- helper: send and wait (for requests expecting response)
local function send_and_pair(tbl)
    send_message(tbl)
    local resp, err = read_response(5)
    if not resp then
        log("WARN", "no response or error: " .. tostring(err))
    else
        -- if parsed and has id, print pairing info
        if type(resp) == "table" and resp.id then
            log("PAIR", string.format("request id=%s <-> response id=%s", tostring(tbl.id or "-"), tostring(resp.id)))
        end
    end
end

-- test sequence
-- 1 initialize (expect response)
local init = {
    jsonrpc = "2.0",
    id = 1,
    method = "initialize",
    params = {
        processId = 12345,
        rootUri = "file:///tmp",
        capabilities = {}
    }
}
send_and_pair(init)

-- 2 initialized (notification, no response expected)
local initialized = { jsonrpc = "2.0", method = "initialized", params = {} }
send_message(initialized)
-- small pause to allow server logs to appear
os.execute("sleep 0.2")
-- try to read any server unsolicited output (non-LSP) quickly
local success, parsed = pcall(read_response, 0.1)
if not success then end

-- 3 didOpen (notification)
local doc = [[digraph G { A -> B; }]]
local didOpen = {
    jsonrpc = "2.0",
    method = "textDocument/didOpen",
    params = {
        textDocument = {
            uri = "file:///tmp/test.dot",
            languageId = "dot",
            version = 1,
            text = doc
        }
    }
}
send_message(didOpen)
os.execute("sleep 0.2")

-- 4 completion (expect response)
local comp = {
    jsonrpc = "2.0",
    id = 2,
    method = "textDocument/completion",
    params = {
        textDocument = { uri = "file:///tmp/test.dot" },
        position = { line = 0, character = 5 }
    }
}
send_and_pair(comp)

-- 5 shutdown (expect response)
local shutdown_req = { jsonrpc = "2.0", id = 3, method = "shutdown", params = {} }
send_and_pair(shutdown_req)

-- 6 exit (notification)
local exit_n = { jsonrpc = "2.0", method = "exit" }
send_message(exit_n)

-- cleanup
in_fd:close()
out_fd:close()
os.execute("rm -f " .. in_fifo .. " " .. out_fifo)
if server_pid then
    os.execute("kill " .. server_pid .. " 2>/dev/null")
end

log("INFO", "done")