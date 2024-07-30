#pragma once

// 用户界面层

#include "app/app.hpp"
#include "domain/entity/session.hpp"
#include "jsonrpc/request.hpp"
#include "jsonrpc/response.hpp"

#include <array>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/cobalt.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/spawn.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace server {

class Server {
public:
    Server(std::shared_ptr<domain::entity::Session> session) : session_(session), ioContext_(1) {
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

private:
    // 注册处理接口
    void enroll() {
        handler_.emplace("initialize", std::bind(&app::App::Initialize, &app_, std::placeholders::_1));
        handler_.emplace("initialized", std::bind(&app::App::Initialized, &app_, std::placeholders::_1));
        handler_.emplace("textDocument/didOpen", std::bind(&app::App::DidOpen, &app_, std::placeholders::_1));
        handler_.emplace("textDocument/didChange", std::bind(&app::App::DidChange, &app_, std::placeholders::_1));
        handler_.emplace("textDocument/didClose", std::bind(&app::App::DidClose, &app_, std::placeholders::_1));
    }

    auto run() -> boost::cobalt::task<void> {
        while (true) {
            // 读取数据
            std::size_t length = co_await read();
            // 解析数据
            std::string_view message(buffer_.data(), length);
            auto             success = request_.Parse(message);
            // 消息分发
            co_await dispatch(request_.Method(), request_.Params());
        }
        co_return;
    }

    auto read() -> boost::cobalt::promise<std::size_t> {
        co_return session_->Read(buffer_);
    }

    auto dispatch(std::string_view method, nlohmann::json params) -> boost::cobalt::task<void> {
        if (auto handler = handler_.find(method.data()); handler != handler_.end()) {
            session_->Write(handler->second(params));
        }

        co_return;
    }

    std::shared_ptr<domain::entity::Session> session_;
    boost::asio::io_context                  ioContext_;
    app::App                                 app_{};
    std::unordered_map<std::string, std::function<const std::string(nlohmann::json params)>> handler_{};  // 接口处理
    std::array<char, 1024>    buffer_{};                                                                  // 数据
    uranus::jsonrpc::Request  request_{};                                                                 // 请求
    uranus::jsonrpc::Response response_{};                                                                // 响应
};

}  // namespace server