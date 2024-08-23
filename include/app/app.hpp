#pragma once

// 应用模块

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace app {

class App : public std::enable_shared_from_this<app::App> {
public:
    App()  = default;
    ~App() = default;

    auto Initialize(nlohmann::json params) -> const std::string {
        return "init";
    }

    auto Initialized(nlohmann::json params) -> const std::string {
        return "inited";
    }

    auto DidOpen(nlohmann::json params) -> const std::string {
        return "open";
    }

    auto DidChange(nlohmann::json params) -> const std::string {
        return "change";
    }

    auto DidClose(nlohmann::json params) -> const std::string {
        return "close";
    }
};
}  // namespace app