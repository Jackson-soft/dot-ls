#pragma once

// 用户界面层

#include "app/app.hpp"
#include "domain/entity/session.hpp"

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace server {

class Server {
public:
    Server(std::shared_ptr<domain::entity::Session> session) : session_(session) {
        enroll();
    }

    ~Server() = default;

    void Dispatch(std::string_view method, nlohmann::json params) {
        if (auto handler = handler_.find(method.data()); handler != handler_.end()) {
            session_->Write(handler->second(params));
        }
    }

    void Run() {
        while (true) {
            session_->Read(nullptr, 1024);
        }
    }

    void Close() {
        session_->Close();
    }

private:
    // 注册处理接口
    void enroll() {
        handler_.emplace("initialize", std::bind(&app::App::Initialize, &app_, std::placeholders::_1));
        handler_.emplace("initialized", std::bind(&app::App::Initialized, &app_, std::placeholders::_1));
        handler_.emplace("textDocument/didOpen", std::bind(&app::App::DidOpen, &app_, std::placeholders::_1));
        handler_.emplace("textDocument/didChange", std::bind(&app::App::DidChange, &app_, std::placeholders::_1));
        handler_.emplace("textDocument/didClose", std::bind(&app::App::DidClose, &app_, std::placeholders::_1));
    }

    std::shared_ptr<domain::entity::Session> session_;
    app::App                                 app_{};
    std::unordered_map<std::string, std::function<const std::string(nlohmann::json params)>> handler_{};  // 接口处理
};

}  // namespace server