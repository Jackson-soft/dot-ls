#pragma once

// 应用模块

#include "domain/entity/document.hpp"
#include "domain/service/language.hpp"
#include "domain/service/lifecycle.hpp"
#include "infra/formatter/dot_formatter.hpp"
#include "protocol/lsp/basic.hpp"
#include "protocol/lsp/language.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace application {
class App : public std::enable_shared_from_this<application::App> {
public:
    App()  = default;
    ~App() = default;

    auto Initialize(const nlohmann::json &params) -> nlohmann::json {
        auto input = lsp::InitializeParams();
        input.Decode(params);
        return lifecycle_->Initialize(input).Encode();
    }

    auto Initialized(const nlohmann::json &params) -> nlohmann::json {
        return nlohmann::json::object();
    }

    auto DidOpen(const nlohmann::json &params) -> nlohmann::json {
        auto input = lsp::DidOpenTextDocumentParams();
        input.Decode(params);
        return document_->DidOpen(input).Encode();
    }

    auto DidSave(const nlohmann::json &params) -> nlohmann::json {
        auto input = lsp::DidSaveTextDocumentParams();
        input.Decode(params);
        return document_->DidSave(input).Encode();
    }

    auto DidChange(const nlohmann::json &params) -> nlohmann::json {
        auto input = lsp::DidChangeTextDocumentParams();
        input.Decode(params);
        return document_->DidChange(input).Encode();
    }

    auto DidClose(const nlohmann::json &params) -> nlohmann::json {
        auto input = lsp::DidCloseTextDocumentParams();
        input.Decode(params);
        return document_->DidClose(input).Encode();
    }

    auto Rename(const nlohmann::json &params) -> nlohmann::json {
        lsp::RenameParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        return language_
            ->Rename(static_cast<uint32_t>(input.position.line),
                     static_cast<uint32_t>(input.position.character),
                     input.textDocument.uri,
                     input.newName,
                     doc->text)
            .Encode();
    }

    // textDocument/prepareRename
    auto PrepareRename(const nlohmann::json &params) -> nlohmann::json {
        lsp::PrepareRenameParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        auto result = language_->PrepareRename(static_cast<uint32_t>(input.position.line),
                                               static_cast<uint32_t>(input.position.character),
                                               doc->text);
        if (!result)
            return nullptr;
        // 返回 { range, placeholder }
        return nlohmann::json{{"range", result->first.Encode()},
                              {"placeholder", result->second}};
    }

    // textDocument/documentSymbol
    auto DocumentSymbol(const nlohmann::json &params) -> nlohmann::json {
        lsp::DocumentSymbolParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        auto           syms = language_->DocumentSymbols(doc->text);
        nlohmann::json arr  = nlohmann::json::array();
        for (auto &sym : syms)
            arr.push_back(sym.Encode());
        return arr;
    }

    // textDocument/foldingRange
    auto FoldingRange(const nlohmann::json &params) -> nlohmann::json {
        lsp::FoldingRangeParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        auto           ranges = language_->FoldingRanges(doc->text);
        nlohmann::json arr    = nlohmann::json::array();
        for (auto &r : ranges)
            arr.push_back(r.Encode());
        return arr;
    }

    // textDocument/selectionRange
    auto SelectionRange(const nlohmann::json &params) -> nlohmann::json {
        lsp::SelectionRangeParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        auto           ranges = language_->SelectionRanges(input.positions, doc->text);
        nlohmann::json arr    = nlohmann::json::array();
        for (auto &r : ranges)
            arr.push_back(r.Encode());
        return arr;
    }

    auto Hover(const nlohmann::json &params) -> nlohmann::json {
        lsp::HoverParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        return language_
            ->Hover(static_cast<uint32_t>(input.position.line),
                    static_cast<uint32_t>(input.position.character),
                    doc->text)
            .Encode();
    }

    auto Definition(const nlohmann::json &params) -> nlohmann::json {
        lsp::DefinitionParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        auto locs = language_->Definition(static_cast<uint32_t>(input.position.line),
                                          static_cast<uint32_t>(input.position.character),
                                          input.textDocument.uri,
                                          doc->text);
        if (locs.empty())
            return nullptr;
        nlohmann::json arr = nlohmann::json::array();
        for (auto &loc : locs)
            arr.push_back(loc.Encode());
        return arr;
    }

    auto DocumentHighlight(const nlohmann::json &params) -> nlohmann::json {
        lsp::DocumentHighlightParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        auto           hls = language_->DocumentHighlight(static_cast<uint32_t>(input.position.line),
                                                static_cast<uint32_t>(input.position.character),
                                                doc->text);
        nlohmann::json arr = nlohmann::json::array();
        for (auto &hl : hls)
            arr.push_back(hl.Encode());
        return arr;
    }

    // textDocument/references：返回 Location[]（而非 DocumentHighlight[]）
    auto References(const nlohmann::json &params) -> nlohmann::json {
        lsp::ReferenceParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        auto           locs = language_->References(static_cast<uint32_t>(input.position.line),
                                                     static_cast<uint32_t>(input.position.character),
                                                     input.textDocument.uri,
                                                     doc->text);
        nlohmann::json arr  = nlohmann::json::array();
        for (auto &loc : locs)
            arr.push_back(loc.Encode());
        return arr;
    }

    auto SemanticTokensFull(const nlohmann::json &params) -> nlohmann::json {
        lsp::SemanticTokensParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        return language_->SemanticTokensFull(doc->text).Encode();
    }

    // 推送诊断通知参数（供 Server 调用）
    auto Diagnose(const nlohmann::json &params) -> nlohmann::json {
        std::string uri;
        if (params.contains("textDocument") && params["textDocument"].contains("uri")) {
            uri = params["textDocument"]["uri"].get<std::string>();
        }
        if (uri.empty())
            return nullptr;
        const auto *doc = document_->Get(uri);
        if (!doc)
            return nullptr;
        auto           diags = language_->Diagnostics(doc->text);
        nlohmann::json arr   = nlohmann::json::array();
        for (auto &d : diags)
            arr.push_back(d.Encode());
        return nlohmann::json{{"uri", uri}, {"diagnostics", arr}};
    }

    auto Completion(const nlohmann::json &params) -> nlohmann::json {
        auto input = lsp::CompletionParams();
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        // 传入文档文本以支持上下文感知补全
        return language_->Completion(input, doc ? doc->text : std::string{}).Encode();
    }

    auto Resolve(const nlohmann::json &params) -> nlohmann::json {
        return params;  // pass-through: resolve detail on demand
    }

    // ── textDocument/inlayHint ────────────────────────────────────────────────
    auto InlayHint(const nlohmann::json &params) -> nlohmann::json {
        lsp::InlayHintParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        auto           hints = language_->InlayHints(doc->text, input.range);
        nlohmann::json arr   = nlohmann::json::array();
        for (auto &h : hints)
            arr.push_back(h.Encode());
        return arr;
    }

    // ── textDocument/codeLens ─────────────────────────────────────────────────
    auto CodeLens(const nlohmann::json &params) -> nlohmann::json {
        lsp::CodeLensParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        auto           lenses = language_->CodeLens(doc->text);
        nlohmann::json arr    = nlohmann::json::array();
        for (auto &lens : lenses) {
            // 将 URI 注入命令参数，供 workspace/executeCommand 使用
            if (!lens.command.command.empty())
                lens.command.arguments = {input.textDocument.uri};
            arr.push_back(lens.Encode());
        }
        return arr;
    }

    // ── codeLens/resolve ──────────────────────────────────────────────────────
    auto CodeLensResolve(const nlohmann::json &params) -> nlohmann::json {
        return params;  // already resolved at request time
    }

    // ── textDocument/documentLink ─────────────────────────────────────────────
    auto DocumentLink(const nlohmann::json &params) -> nlohmann::json {
        lsp::DocumentLinkParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        auto           links = language_->DocumentLinks(doc->text);
        nlohmann::json arr   = nlohmann::json::array();
        for (auto &l : links)
            arr.push_back(l.Encode());
        return arr;
    }

    // ── documentLink/resolve ──────────────────────────────────────────────────
    auto DocumentLinkResolve(const nlohmann::json &params) -> nlohmann::json {
        return params;  // target already embedded
    }

    // ── textDocument/onTypeFormatting ─────────────────────────────────────────
    auto OnTypeFormatting(const nlohmann::json &params) -> nlohmann::json {
        lsp::DocumentOnTypeFormattingParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        infra::formatter::FormattingOptions opts{
            .tabSize      = input.options.tabSize,
            .insertSpaces = input.options.insertSpaces,
        };
        auto edits = language_->OnTypeFormatting(
            doc->text,
            static_cast<uint32_t>(input.position.line),
            static_cast<uint32_t>(input.position.character),
            input.ch, opts);
        nlohmann::json arr = nlohmann::json::array();
        for (auto &e : edits)
            arr.push_back(e.Encode());
        return arr;
    }

    // ── workspace/executeCommand ──────────────────────────────────────────────
    // 返回 workspace/applyEdit 参数（含 "edit" 字段），null 表示无需编辑
    auto ExecuteCommand(const nlohmann::json &params) -> nlohmann::json {
        lsp::ExecuteCommandParams input;
        input.Decode(params);
        std::string uri;
        if (!input.arguments.empty() && input.arguments[0].is_string())
            uri = input.arguments[0].template get<std::string>();
        const auto *doc = uri.empty() ? nullptr : document_->Get(uri);
        if (!doc)
            return nullptr;
        return language_->ExecuteCommand(input.command, uri, doc->text);
    }

    // ── textDocument/codeAction ───────────────────────────────────────────────
    auto CodeAction(const nlohmann::json &params) -> nlohmann::json {
        lsp::CodeActionParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();
        return language_->CodeActions(input.textDocument.uri, doc->text, input.range);
    }

    // ── textDocument/semanticTokens/range ────────────────────────────────────
    auto SemanticTokensRange(const nlohmann::json &params) -> nlohmann::json {
        lsp::SemanticTokensRangeParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nullptr;
        return language_->SemanticTokensRange(doc->text, input.range).Encode();
    }

    // ── workspace/symbol ─────────────────────────────────────────────────────
    auto WorkspaceSymbol(const nlohmann::json &params) -> nlohmann::json {
        lsp::WorkspaceSymbolParams input;
        input.Decode(params);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto &[uri, entry] : document_->GetAllEntries()) {
            auto syms = language_->DocumentSymbols(entry.text);
            flattenSymbols(syms, uri, input.query, arr);
        }
        return arr;
    }

    // ── textDocument/formatting ───────────────────────────────────────────────
    auto Formatting(const nlohmann::json &params) -> nlohmann::json {
        lsp::DocumentFormattingParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();

        infra::formatter::FormattingOptions opts{
            .tabSize      = input.options.tabSize,
            .insertSpaces = input.options.insertSpaces,
        };
        auto           edits = language_->Formatting(doc->text, opts);
        nlohmann::json arr   = nlohmann::json::array();
        for (auto &edit : edits)
            arr.push_back(edit.Encode());
        return arr;
    }

    // ── textDocument/rangeFormatting ──────────────────────────────────────────
    auto RangeFormatting(const nlohmann::json &params) -> nlohmann::json {
        lsp::DocumentRangeFormattingParams input;
        input.Decode(params);
        const auto *doc = document_->Get(input.textDocument.uri);
        if (!doc)
            return nlohmann::json::array();

        infra::formatter::FormattingOptions opts{
            .tabSize      = input.options.tabSize,
            .insertSpaces = input.options.insertSpaces,
        };
        auto           edits = language_->RangeFormatting(doc->text, input.range, opts);
        nlohmann::json arr   = nlohmann::json::array();
        for (auto &edit : edits)
            arr.push_back(edit.Encode());
        return arr;
    }

private:
    std::unique_ptr<domain::service::Lifecycle> lifecycle_{std::make_unique<domain::service::Lifecycle>()};
    std::unique_ptr<domain::service::Language>  language_{std::make_unique<domain::service::Language>()};
    std::unique_ptr<domain::entity::Document>   document_{std::make_unique<domain::entity::Document>()};

    // 递归将 DocumentSymbol 层次展平为 SymbolInformation（供 workspace/symbol 使用）
    static void flattenSymbols(const std::vector<lsp::DocumentSymbol> &syms,
                               const std::string &uri, const std::string &query,
                               nlohmann::json &out) {
        for (auto sym : syms) {  // 按值拷贝以调用非 const Encode()
            bool match = query.empty()
                         || sym.name.find(query) != std::string::npos;
            if (match) {
                nlohmann::json info;
                info["name"]     = sym.name;
                info["kind"]     = static_cast<int>(sym.kind);
                info["location"] = nlohmann::json{{"uri", uri},
                                                  {"range", sym.range.Encode()}};
                out.push_back(std::move(info));
            }
            flattenSymbols(sym.children, uri, query, out);
        }
    }
};
}  // namespace application