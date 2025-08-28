local json = require("cjson")

-- LSP 测试客户端
local LSPClient = {}
LSPClient.__index = LSPClient

function LSPClient.new(server_path)
    local self = setmetatable({}, LSPClient)
    self.message_id = 0
    local current_dir = io.popen("pwd"):read("*l")
    self.server_path = server_path or (current_dir .. "/../build/Debug/dot-ls")
    self.responses = {}
    return self
end

function LSPClient:next_id()
    self.message_id = self.message_id + 1
    return self.message_id
end

function LSPClient:log(level, message)
    local timestamp = os.date("%H:%M:%S")
    print(string.format("[%s] %s: %s", timestamp, level, message))
end

function LSPClient:send_message(process, message)
    local message_json = json.encode(message)
    local content_length = string.len(message_json)

    local header = "Content-Length: " .. content_length .. "\r\n\r\n"
    local full_message = header .. message_json

    process:write(full_message)
    process:flush()

    if message.method then
        if message.id then
            self:log("SEND", string.format("Request: %s (ID: %s)", message.method, message.id))
        else
            self:log("SEND", string.format("Notification: %s", message.method))
        end
    end
    self:log("DEBUG", "Message: " .. message_json)

    -- 输出原始消息到 stderr，这样可以同时看到发送和接收的内容
    io.stderr:write("CLIENT_MSG: " .. full_message .. "\n")
    io.stderr:flush()
end

function LSPClient:send_request(process, method, params)
    local request = {
        jsonrpc = "2.0",
        id = self:next_id(),
        method = method,
        params = params or {}
    }

    self:send_message(process, request)
    return request.id
end

function LSPClient:send_notification(process, method, params)
    local notification = {
        jsonrpc = "2.0",
        method = method,
        params = params or {}
    }

    self:send_message(process, notification)
end

function LSPClient:read_response_from_stdout()
    -- 读取服务端标准输出的响应
    local line = io.read("*line")
    if not line then
        return nil
    end

    -- 查找 Content-Length 头
    local content_length = 0
    while line do
        if line:match("^Content%-Length:%s*(%d+)") then
            content_length = tonumber(line:match("^Content%-Length:%s*(%d+)"))
            break
        end
        line = io.read("*line")
        if not line then return nil end
    end

    if content_length == 0 then
        return nil
    end

    -- 跳过空行
    io.read("*line")

    -- 读取消息体
    local response = io.read(content_length)
    if response then
        self:log("RECV", "Response received")
        self:log("DEBUG", "Response: " .. response)

        -- 尝试解析 JSON 并显示格式化信息
        local success, parsed = pcall(json.decode, response)
        if success then
            if parsed.id then
                self:log("RECV", string.format("Response ID: %s", parsed.id))
            end
            if parsed.method then
                self:log("RECV", string.format("Notification: %s", parsed.method))
            end
            if parsed.error then
                self:log("ERROR", string.format("Error: %s", parsed.error.message or "Unknown error"))
            end
        end
    end

    return response
end

function LSPClient:wait_for_response(process)
    -- 简单的等待实现，实际项目中应该使用更复杂的响应处理
    os.execute("sleep 0.5")

    -- 尝试读取响应（非阻塞）
    local response = self:read_response_from_stdout()
    if response then
        return response
    end
end

function LSPClient:run_test_sequence()
    self:log("INFO", "Starting LSP server test sequence...")

    -- 启动 LSP 服务器进程（使用双向管道）
    local server_cmd = self.server_path
    self:log("INFO", "Starting server: " .. server_cmd)

    -- 创建临时文件用于双向通信
    local temp_input = os.tmpname()
    local temp_output = os.tmpname()

    -- 创建命名管道用于双向通信
    os.execute("mkfifo " .. temp_input .. " 2>/dev/null")
    os.execute("mkfifo " .. temp_output .. " 2>/dev/null")

    -- 在后台启动服务器，重定向输入输出
    local server_pid_cmd = string.format("(%s < %s > %s 2>&1) & echo $!", server_cmd, temp_input, temp_output)
    local pid_handle = io.popen(server_pid_cmd)
    local server_pid = pid_handle:read("*line")
    pid_handle:close()

    self:log("INFO", "Server started with PID: " .. (server_pid or "unknown"))

    -- 打开写入管道
    local lsp_server = io.open(temp_input, "w")
    if not lsp_server then
        self:log("ERROR", "Failed to open input pipe")
        return false
    end

    -- 打开读取管道（非阻塞）
    local function read_server_output()
        local output_file = io.open(temp_output, "r")
        if output_file then
            local content = output_file:read("*all")
            output_file:close()
            if content and content ~= "" then
                self:log("RECV", "Server output:")
                print(content)
            end
        end
    end

    -- 等待服务器启动
    os.execute("sleep 1")

    -- 1. 发送初始化请求
    self:log("INFO", "--- Test 1: Initialize ---")
    local init_id = self:send_request(lsp_server, "initialize", {
        processId = 12345,
        rootUri = "file:///Users/yalla/code/cpp/dot-ls",
        locale = "en",
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
                        snippetSupport = true,
                        documentationFormat = { "markdown", "plaintext" }
                    }
                },
                hover = {
                    dynamicRegistration = true,
                    contentFormat = { "markdown", "plaintext" }
                },
                definition = {
                    dynamicRegistration = true
                }
            },
            workspace = {
                workspaceFolders = true,
                configuration = true
            }
        },
        trace = "verbose",
        workspaceFolders = {
            {
                uri = "file:///Users/yalla/code/cpp/dot-ls",
                name = "dot-ls"
            }
        }
    })

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 2. 发送 initialized 通知
    self:log("INFO", "--- Test 2: Initialized ---")
    self:send_notification(lsp_server, "initialized", {})

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 3. 发送 textDocument/didOpen 通知
    self:log("INFO", "--- Test 3: Document Open ---")
    local test_document = {
        uri = "file:///Users/yalla/code/cpp/dot-ls/test/graph.dot",
        languageId = "dot",
        version = 1,
        text = [[digraph G {
    rankdir=LR;
    node [shape=box];

    A -> B [label="edge1"];
    B -> C [label="edge2"];
    C -> A [label="edge3"];

    subgraph cluster_0 {
        label="Cluster";
        D -> E;
    }
}]]
    }

    self:send_notification(lsp_server, "textDocument/didOpen", {
        textDocument = test_document
    })

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 4. 发送 textDocument/completion 请求
    self:log("INFO", "--- Test 4: Completion ---")
    self:send_request(lsp_server, "textDocument/completion", {
        textDocument = {
            uri = test_document.uri
        },
        position = {
            line = 5,
            character = 8
        },
        context = {
            triggerKind = 1
        }
    })

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 5. 发送 textDocument/hover 请求
    self:log("INFO", "--- Test 5: Hover ---")
    self:send_request(lsp_server, "textDocument/hover", {
        textDocument = {
            uri = test_document.uri
        },
        position = {
            line = 4,
            character = 4
        }
    })

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 6. 发送 textDocument/didChange 通知
    self:log("INFO", "--- Test 6: Document Change ---")
    self:send_notification(lsp_server, "textDocument/didChange", {
        textDocument = {
            uri = test_document.uri,
            version = 2
        },
        contentChanges = {
            {
                range = {
                    start = { line = 6, character = 0 },
                    ["end"] = { line = 6, character = 0 }
                },
                text = "    F -> G;\n"
            }
        }
    })

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 7. 发送 textDocument/didClose 通知
    self:log("INFO", "--- Test 7: Document Close ---")
    self:send_notification(lsp_server, "textDocument/didClose", {
        textDocument = {
            uri = test_document.uri
        }
    })

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 8. 发送 shutdown 请求
    self:log("INFO", "--- Test 8: Shutdown ---")
    self:send_request(lsp_server, "shutdown", {})

    self:wait_for_response(lsp_server)
    read_server_output()

    -- 9. 发送 exit 通知
    self:log("INFO", "--- Test 9: Exit ---")
    self:send_notification(lsp_server, "exit")

    -- 等待一会儿让服务器处理退出
    os.execute("sleep 1")
    read_server_output()

    -- 关闭连接
    lsp_server:close()

    -- 清理临时文件
    os.execute("rm -f " .. temp_input .. " " .. temp_output)

    -- 确保服务器进程已终止
    if server_pid then
        os.execute("kill " .. server_pid .. " 2>/dev/null")
    end

    self:log("INFO", "Test sequence completed!")
    return true
end

-- 主函数
function main()
    local client = LSPClient.new()

    print("=== DOT-LS LSP Server Test Client ===")
    print("This client will test the LSP server functionality")
    print("Press Ctrl+C to interrupt at any time")
    print("")

    local success = client:run_test_sequence()

    if success then
        print("\n✅ All tests completed successfully!")
    else
        print("\n❌ Tests failed!")
        os.exit(1)
    end
end

-- 运行测试
main()
