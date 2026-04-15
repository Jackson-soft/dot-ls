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

        // 启用文档同步（Full 模式 + save 时附带文本）
        result.capabilities.textDocumentSync.openClose      = true;
        result.capabilities.textDocumentSync.change         = lsp::TextDocumentSyncKind::Full;
        result.capabilities.textDocumentSync.save           = true;
        result.capabilities.textDocumentSync.saveIncludeText = true;

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

        // 启用格式化
        result.capabilities.documentFormattingProvider      = true;
        result.capabilities.documentRangeFormattingProvider = true;

        // 启用文档符号
        result.capabilities.documentSymbolProvider = true;

        // 启用重命名
        result.capabilities.renameProvider = true;

        // 启用折叠范围
        result.capabilities.foldingRangeProvider = true;

        // 启用选区扩展
        result.capabilities.selectionRangeProvider = true;

        // 启用 Semantic Tokens（full + range）
        result.capabilities.semanticTokensProvider.legend.tokenTypes = infra::parser::TreeSitter::TokenTypeNames();
        result.capabilities.semanticTokensProvider.full              = true;
        result.capabilities.semanticTokensProvider.range             = true;

        // 启用 Code Action
        result.capabilities.codeActionProvider = true;

        // 启用 Workspace Symbol
        result.capabilities.workspaceSymbolProvider = true;

        // 启用 Code Lens
        result.capabilities.codeLensProvider = true;

        // 启用 Document Link
        result.capabilities.documentLinkProvider = true;

        // 启用 Inlay Hint
        result.capabilities.inlayHintProvider = true;

        // 启用输入时格式化（输入 } 或换行时）
        result.capabilities.documentOnTypeFormattingProvider.firstTriggerCharacter = "}";
        result.capabilities.documentOnTypeFormattingProvider.moreTriggerCharacter  = {"\n"};

        // 启用服务端命令执行
        result.capabilities.executeCommandProvider.commands = {
            "dot-ls.formatDocument",
            "dot-ls.validate",
        };

        return result;
    }
};
}  // namespace domain::service