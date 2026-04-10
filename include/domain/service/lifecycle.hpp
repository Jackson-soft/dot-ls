#pragma once

#include "infra/config/flag.hpp"
#include "infra/parser/tree_sitter_adapter.hpp"
#include "protocol/lsp/lifecycle.hpp"

// 生命周期相关服务
namespace domain::service {
class Lifecycle {
public:
    Lifecycle()  = default;
    ~Lifecycle() = default;

    auto Initialize(const lsp::InitializeParams &param) -> lsp::InitializeResult {
        lsp::InitializeResult result;
        result.serverInfo.name    = infra::config::Name;
        result.serverInfo.version = infra::config::Version;

        // 启用文档同步
        result.capabilities.textDocumentSync.openClose = true;
        result.capabilities.textDocumentSync.change    = lsp::TextDocumentSyncKind::Full;

        // 启用代码补全
        result.capabilities.completionProvider.resolveProvider   = false;
        result.capabilities.completionProvider.triggerCharacters = {".", "[", "="};

        // 启用 Hover
        result.capabilities.hoverProvider = true;

        // 启用 Definition / Declaration
        result.capabilities.definitionProvider  = true;
        result.capabilities.declarationProvider = true;

        // 启用 Document Highlight
        result.capabilities.documentHighlightProvider = true;

        // 启用 References
        result.capabilities.referencesProvider = true;

        // 启用 Semantic Tokens
        result.capabilities.semanticTokensProvider.legend.tokenTypes = infra::parser::TreeSitter::TokenTypeNames();
        result.capabilities.semanticTokensProvider.full              = true;
        result.capabilities.semanticTokensProvider.range             = false;

        return result;
    }
};
}  // namespace domain::service