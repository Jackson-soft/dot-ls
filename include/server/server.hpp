#pragma once

// 用户界面层

#include "app/app.hpp"
#include "domain/model/basic.hpp"
#include "domain/model/flag.hpp"
#include "jsonrpc/request.hpp"
#include "jsonrpc/response.hpp"

#include <boost/asio.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/pool/object_pool.hpp>
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
        : stdin_(ioCtx, ::dup(STDIN_FILENO)), stdout_(ioCtx, ::dup(STDOUT_FILENO)) {
        enroll();
    }

    ~Server() {
        Close();
    }

    auto Run() -> boost::asio::awaitable<void> {
        while (true) {
            auto body = co_await read();
            if (!body.empty()) {
                co_await dispatch(body);
            }
        }
    }

    void Close() {
        stdin_.close();
        stdout_.close();
        buffer_.clear();
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

    auto read() -> boost::asio::awaitable<std::string> {
        // 一次性读取直到头部结束标记 "\r\n\r\n"
        auto headerLen = co_await boost::asio::async_read_until(stdin_,
                                                                buffer_,
                                                                domain::model::Delimiter,
                                                                boost::asio::use_awaitable);

        // 解析头部
        const std::string header = boost::beast::buffers_to_string(buffer_.data());

        const std::size_t contentLen = doLength(header);
        buffer_.consume(headerLen);  // 移除已处理的头部数据

        // 读取消息体
        if (contentLen > 0) {
            if (buffer_.size() < contentLen) {
                buffer_.reserve(contentLen);
            }
            // 精确读取指定长度内容
            std::size_t readLen
                = co_await stdin_.async_read_some(buffer_.prepare(contentLen), boost::asio::use_awaitable);

            if (readLen != contentLen) {
                co_return "";  // 读取长度不匹配
            }

            // 直接返回缓冲区视图
            const std::string body = boost::beast::buffers_to_string(buffer_.data());

            buffer_.consume(contentLen);

            co_return body;
        }

        co_return "";
    }

    auto doLength(const std::string &header) -> std::size_t {
        std::istringstream stream(header);
        std::string        line;

        while (std::getline(stream, line)) {
            if (line.substr(0, 15) == domain::model::HeaderLen) {
                std::string valueStr = line.substr(15);

                // 去除可能的回车符
                if (const size_t cr_pos = valueStr.find('\r'); cr_pos != std::string::npos)
                    valueStr = valueStr.substr(0, cr_pos);

                try {
                    return std::stoul(valueStr);
                } catch (...) {
                    return 0;  // 无效长度
                }
            }
        }
        return 0;  // 未找到 Content-Length
    }

    auto dispatch(std::string &body) -> boost::asio::awaitable<void> {
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
                co_await boost::asio::async_write(stdout_, boost::asio::buffer(message), boost::asio::use_awaitable);
                response_.destroy(output);
            }
        }

        request_.destroy(request);
    }

    boost::asio::posix::stream_descriptor stdin_;     // 标准输入
    boost::asio::posix::stream_descriptor stdout_;    // 标准输出
    boost::beast::flat_buffer             buffer_{};  // 可自动扩容的缓冲区
    std::shared_ptr<app::App>             app_{std::make_shared<app::App>()};
    std::unordered_map<std::string, std::function<domain::model::Protocol(const nlohmann::json &params)>> handler_{};
    // 接口处理
    boost::object_pool<uranus::jsonrpc::Request>  request_{};   // 请求池
    boost::object_pool<uranus::jsonrpc::Response> response_{};  // 响应池
};
}  // namespace server
