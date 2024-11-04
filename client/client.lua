local json = require("cjson")

-- 启动 LSP 服务端进程
local lsp_server = io.popen("dot-ls", "w")

-- 构造 JSON-RPC 初始化请求
local request = {
    jsonrpc = "2.0",
    id = 1,
    method = "initialize",
    params = {
        processId = 12345, -- 可用 os.getenv("PROCESS_ID") 替代
        rootUri = "file:///~/code/c/dot-ls/test/graph.dot",
        locale = "zh",
        capabilities = {
            textDocument = {
                synchronization = {
                    dynamicRegistration = true,
                    willSave = true,
                    willSaveWaitUntil = true,
                    didSave = true
                },
                completion = {
                    dynamicRegistration = true,
                    completionItem = {
                        snippetSupport = true
                    }
                },
                hover = {
                    dynamicRegistration = true
                }
            }
        },
        trace = "off",
        workspaceFolders = {
            {
                uri = "file:///path/to/your/project",
                name = "project"
            }
        }
    }
}

local requestData = json.encode(request)
local contentLength = string.len(requestData)

-- 发送初始化请求到 stdin
lsp_server:write("Content-Length: " .. contentLength .. "\r\n\r\n" .. requestData)
lsp_server:flush()

-- 关闭连接
lsp_server:close()
