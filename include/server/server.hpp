#pragma once

// 用户界面层

#include "domain/entity/session.hpp"

#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace server {

class Server {
public:
    Server(std::shared_ptr<domain::entity::Session> session) : session_(session) {}

    ~Server() = default;

    void Join() {}

    void Dispatch(std::string_view method, nlohmann::json params) {}

    void Run() {}

private:
    std::shared_ptr<domain::entity::Session>                                 session_;
    std::map<std::string, std::function<std::string(nlohmann::json params)>> handler_{};  // 接口处理
};

}  // namespace server