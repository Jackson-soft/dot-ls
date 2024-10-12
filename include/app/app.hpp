#pragma once

// 应用模块

#include "domain/model/basic.hpp"
#include "domain/service/lifecycle.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace app {
class App : public std::enable_shared_from_this<app::App> {
public:
    App() : lifecycle_(std::make_unique<domain::service::Lifecycle>()) {}

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
        return {};
    }

    auto DidChange(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

    auto DidClose(const nlohmann::json &params) -> domain::model::Protocol {
        return {};
    }

private:
    std::unique_ptr<domain::service::Lifecycle> lifecycle_;
};
}  // namespace app