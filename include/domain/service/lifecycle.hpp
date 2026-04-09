#pragma once

#include "infra/config/flag.hpp"
#include "protocol/lsp/lifecycle.hpp"

// 生命周期相关服务
namespace domain::service {
class Lifecycle {
public:
  Lifecycle() = default;
  ~Lifecycle() = default;

  auto Initialize(const lsp::InitializeParams &param) -> lsp::InitializeResult {
    lsp::InitializeResult result;
    result.serverInfo.name = infra::config::Name;
    result.serverInfo.version = infra::config::Version;

    // 启用文档同步
    result.capabilities.textDocumentSync.openClose = true;
    result.capabilities.textDocumentSync.change =
        lsp::TextDocumentSyncKind::Full;

    // 启用代码补全
    result.capabilities.completionProvider.resolveProvider = false;
    result.capabilities.completionProvider.triggerCharacters = {".", "[", "="};

    return result;
  }
};
} // namespace domain::service