#pragma once

// 应用模块

#include "domain/model/basic.hpp"
#include "domain/service/document.hpp"
#include "domain/service/lifecycle.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace app {
class App : public std::enable_shared_from_this<app::App> {
public:
    App()
        : lifecycle_(std::make_unique<domain::service::Lifecycle>()),
          document_(std::make_unique<domain::service::Document>()) {}

    ~App() = default;

    auto Initialize(const nlohmann::json &params) -> domain::model::Protocol {
        auto input = domain::model::InitializeParams();
        input.Decode(params);
        return lifecycle_->Initialize(input);
    }

    auto Initialized(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

    auto DidOpen(const nlohmann::json &params) -> domain::model::Protocol {
        auto input = domain::model::DidOpenTextDocumentParams();
        input.Decode(params);
        return document_->DidOpen(input);
    }

    auto DidSave(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

    auto DidChange(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

    auto DidClose(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

    auto Rename(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

    auto Completion(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

    auto Resolve(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

private:
    std::unique_ptr<domain::service::Lifecycle> lifecycle_;
    std::unique_ptr<domain::service::Document>  document_;
};
}  // namespace app