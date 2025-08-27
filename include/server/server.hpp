#pragma once

// 用户界面层

#include "app/app.hpp"
#include "domain/model/basic.hpp"
#include "domain/model/flag.hpp"
#include "jsonrpc/request.hpp"
#include "jsonrpc/response.hpp"

#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/cobalt.hpp>
#include <boost/pool/object_pool.hpp>
#include <format>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>

namespace server {
class Server {
public:
    explicit Server(boost::asio::io_context &ioCtx)
        : ioCtx_(ioCtx), stdin_(ioCtx, ::dup(STDIN_FILENO)), stdout_(ioCtx, ::dup(STDOUT_FILENO)) {
        enroll();
    }

    ~Server() {
        Close();
    }

    auto Run() -> boost::cobalt::task<void> {
        while (true) {
            if (auto body = co_await read(); !body.empty()) {
                co_await dispatch(body);
            }
        }
    }

    void Close() {
        stdin_.close();
        stdout_.close();
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

    auto read() -> boost::cobalt::task<std::string> {
        std::string line;
        while (true) {
            // 一次性读取直到头部结束标记 "\r\n\r\n"
            co_await boost::asio::async_read_until(stdin_, buffer_, domain::model::Delimiter, boost::cobalt::use_task);

            // 解析头部
            std::istream is(&buffer_);
            std::string  header;
            std::getline(is, header);

            if (header.starts_with(domain::model::HeaderLen)) {
                const std::size_t length = std::stoul(header.substr(16));

                // 读取消息体
                std::string content(length, '\0');
                // 精确读取指定长度内容
                const std::size_t readLen
                    = co_await boost::asio::async_read(stdin_,
                                                       boost::asio::buffer(content.data(), length),
                                                       boost::cobalt::use_task);

                if (readLen != length) {
                    co_return "";  // 读取长度不匹配
                }

                co_return content;
            }
        }
    }

    auto dispatch(const std::string &body) -> boost::cobalt::task<void> {
        // 解析数据
        auto request = request_.construct();
        if (auto success = request->Parse(body); success) {
            if (const auto handler = handler_.find(request->Method().data()); handler != handler_.end()) {
                auto result = handler->second(request->Params());

                auto output = response_.construct(std::get<0>(request->Id()));
                output->SetResult(result.Encode());
                auto message = std::format("{}{}{}{}",
                                           domain::model::HeaderLen,
                                           output->String().size(),
                                           domain::model::Delimiter,
                                           output->String());
                co_await boost::asio::async_write(stdout_, boost::asio::buffer(message), boost::cobalt::use_task);
                response_.destroy(output);
            }
        }

        request_.destroy(request);
    }

    boost::asio::io_context              &ioCtx_;
    boost::asio::posix::stream_descriptor stdin_;     // 标准输入
    boost::asio::posix::stream_descriptor stdout_;    // 标准输出
    boost::asio::streambuf                buffer_{};  // 可自动扩容的缓冲区
    std::shared_ptr<app::App>             app_{std::make_shared<app::App>()};
    std::unordered_map<std::string, std::function<domain::model::Protocol(const nlohmann::json &params)>> handler_{};
    // 接口处理
    boost::object_pool<uranus::jsonrpc::Request>  request_{};   // 请求池
    boost::object_pool<uranus::jsonrpc::Response> response_{};  // 响应池
};
}  // namespace server
