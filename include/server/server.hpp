#pragma once

// 用户界面层

#include "domain/entity/session.hpp"

#include <memory>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace server {

class Server {
public:
    Server(std::shared_ptr<domain::entity::Session> session) : session_(session) {}

    ~Server() = default;

    void Dispatch(std::string_view method, nlohmann::json params) {}

    void Run() {
        while (true) {
            session_->Read(nullptr, 1024);
        }
    }

    void Close() {
        session_->Close();
    }

private:
    std::shared_ptr<domain::entity::Session>                                           session_;
    std::unordered_map<std::string, std::function<std::string(nlohmann::json params)>> handler_{};  // 接口处理
};

}  // namespace server