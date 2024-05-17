#pragma once

// 服务

#include "domain/entity/session.hpp"

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace hesiod::server {
class LanguageServer {
public:
    LanguageServer()  = default;
    ~LanguageServer() = default;

    void Join() {}

    void Dispatch(std::string_view method, nlohmann::json params) {}

    void Run() {}

private:
    std::shared_ptr<basis::Session>                                          session_;
    std::map<std::string, std::function<std::string(nlohmann::json params)>> handler_{};  // 接口处理
};
}  // namespace hesiod::server