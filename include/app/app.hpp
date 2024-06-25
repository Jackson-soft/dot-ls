#pragma once

// 应用模块

#include <nlohmann/json.hpp>
#include <string>

namespace app {

class App {
public:
    App()  = default;
    ~App() = default;

    static auto Initialize(nlohmann::json params) -> const std::string {
        return "init";
    }

    static auto Initialized(nlohmann::json params) -> const std::string {
        return "inited";
    }

    static auto DidOpen(nlohmann::json params) -> const std::string {
        return "open";
    }

    static auto DidChange(nlohmann::json params) -> const std::string {
        return "change";
    }

    static auto DidClose(nlohmann::json params) -> const std::string {
        return "close";
    }
};
}  // namespace app