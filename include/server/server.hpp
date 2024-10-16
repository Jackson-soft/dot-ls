#pragma once

// 用户界面层

#include "app/app.hpp"
#include "domain/entity/session.hpp"
#include "domain/model/basic.hpp"
#include "jsonrpc/request.hpp"
#include "jsonrpc/response.hpp"
#include "utils/log.hpp"

#include <array>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/cobalt.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <utility>

namespace server {

class Server {
public:
    explicit Server(std::shared_ptr<domain::entity::Session> session)
        : ioContext_(1), session_(std::move(session)), app_(std::make_shared<app::App>()) {
        enroll();
    }

    ~Server() = default;

    void Run() {
        boost::cobalt::spawn(ioContext_, run(), boost::asio::detached);
        ioContext_.run();
    }

    void Close() {
        session_->Close();
        ioContext_.stop();
    }

    auto Shutdown(const nlohmann::json &params) -> domain::model::Protocol {
        Close();
        return {};
    }

    auto Exit(const nlohmann::json &params) -> domain::model::Protocol {
        Close();
        return {};
    }

private:
    // 注册处理接口
    void enroll() {
        handler_.emplace("initialize", std::bind(&app::App::Initialize, app_.get(), std::placeholders::_1));
        handler_.emplace("initialized", std::bind(&app::App::Initialized, app_.get(), std::placeholders::_1));
        handler_.emplace("textDocument/didOpen", std::bind(&app::App::DidOpen, app_.get(), std::placeholders::_1));
        handler_.emplace("textDocument/didSave", std::bind(&app::App::DidSave, app_.get(), std::placeholders::_1));
        handler_.emplace("textDocument/didChange", std::bind(&app::App::DidChange, app_.get(), std::placeholders::_1));
        handler_.emplace("textDocument/didClose", std::bind(&app::App::DidClose, app_.get(), std::placeholders::_1));
        handler_.emplace("textDocument/rename", std::bind(&app::App::Rename, app_.get(), std::placeholders::_1));
        handler_.emplace("textDocument/completion",
                         std::bind(&app::App::Completion, app_.get(), std::placeholders::_1));
        handler_.emplace("completionItem/resolve", std::bind(&app::App::Resolve, app_.get(), std::placeholders::_1));
        handler_.emplace("shutdown", std::bind(&Server::Shutdown, this, std::placeholders::_1));
        handler_.emplace("exit", std::bind(&Server::Exit, this, std::placeholders::_1));
    }

    auto run() -> boost::cobalt::task<void> {
        while (true) {
            // 读取数据
            if (auto success = co_await read(); success) {
                // 消息分发
                co_await dispatch(request_.Method(), request_.Params());
            }
        }
        co_return;
    }

    auto read() -> boost::cobalt::task<bool> {
        auto length = session_->Read(buffer_);
        // 解析数据
        std::string_view message(buffer_.data(), length);

        uranus::utils::LogHelper::Instance().Info(message.data());

        auto success = request_.Parse(message);
        co_return success;
    }

    auto dispatch(std::string_view method, nlohmann::json params) -> boost::cobalt::task<void> {
        if (auto handler = handler_.find(method.data()); handler != handler_.end()) {
            auto result = handler->second(params);
            response_.AddResult(result.Encode());
            session_->Write(response_.LspString());
        }

        co_return;
    }

    boost::asio::io_context                  ioContext_;
    std::shared_ptr<domain::entity::Session> session_;
    std::shared_ptr<app::App>                app_;
    std::unordered_map<std::string, std::function<domain::model::Protocol(const nlohmann::json &params)>>
                              handler_{};   // 接口处理
    std::array<char, 1024>    buffer_{};    // 数据
    uranus::jsonrpc::Request  request_{};   // 请求
    uranus::jsonrpc::Response response_{};  // 响应
};

}  // namespace server