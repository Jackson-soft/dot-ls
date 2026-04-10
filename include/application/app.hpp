#pragma once

// 应用模块

#include "domain/entity/document.hpp"
#include "domain/service/language.hpp"
#include "domain/service/lifecycle.hpp"
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
        return nlohmann::json::object();
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
        return language_->Completion(input).Encode();
    }

    auto Resolve(const nlohmann::json &params) -> nlohmann::json {
        return nlohmann::json::object();
    }

private:
    std::unique_ptr<domain::service::Lifecycle> lifecycle_{std::make_unique<domain::service::Lifecycle>()};
    std::unique_ptr<domain::service::Language>  language_{std::make_unique<domain::service::Language>()};
    std::unique_ptr<domain::entity::Document>   document_{std::make_unique<domain::entity::Document>()};
};
}  // namespace application