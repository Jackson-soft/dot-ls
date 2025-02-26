local lsp_server = io.popen("dot-ls", "w")

print("lsp server start")

lsp_server:write("lsp_server")
lsp_server:flush()

-- 关闭连接
lsp_server:close()

print("lsp server end")
