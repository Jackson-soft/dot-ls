#pragma once

// 应用模块

#include "domain/service/document.hpp"
#include "domain/service/lifecycle.hpp"
#include "lsp/basic.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace app {
class App : public std::enable_shared_from_this<app::App> {
public:
    App()  = default;
    ~App() = default;

    auto Initialize(const nlohmann::json &params) -> lsp::Protocol {
        auto input = lsp::InitializeParams();
        input.Decode(params);
        return lifecycle_->Initialize(input);
    }

    auto Initialized(const nlohmann::json &params) -> lsp::Protocol {
        return {};
    }

    auto DidOpen(const nlohmann::json &params) -> lsp::Protocol {
        auto input = lsp::DidOpenTextDocumentParams();
        input.Decode(params);
        return document_->DidOpen(input);
    }

    auto DidSave(const nlohmann::json &params) -> lsp::Protocol {
        return {};
    }

    auto DidChange(const nlohmann::json &params) -> lsp::Protocol {
        return {};
    }

    auto DidClose(const nlohmann::json &params) -> lsp::Protocol {
        return {};
    }

    auto Rename(const nlohmann::json &params) -> lsp::Protocol {
        return {};
    }

    auto Completion(const nlohmann::json &params) -> lsp::Protocol {
        return {};
    }

    auto Resolve(const nlohmann::json &params) -> lsp::Protocol {
        return {};
    }

private:
    std::unique_ptr<domain::service::Lifecycle> lifecycle_{std::make_unique<domain::service::Lifecycle>()};
    std::unique_ptr<domain::service::Document>  document_{std::make_unique<domain::service::Document>()};
};
}  // namespace app