#pragma once

#include "protocol/lsp/flag.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <cstddef>
#include <format>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace infra::transport {
// 接口
class Session {
public:
    Session() = default;

    virtual ~Session() = default;

    virtual auto Read() -> boost::asio::awaitable<std::string>              = 0;
    virtual auto Write(const std::string &response) -> boost::asio::awaitable<void> = 0;
    virtual void Close()                                                            = 0;
};

// 标准输入输出流实现（目前由 interface::Server 直接处理，此类保留供未来扩展）
class IOSession final : public Session, std::enable_shared_from_this<IOSession> {
public:
#ifdef _WIN32
    using StdioStream = boost::asio::windows::stream_handle;
#else
    using StdioStream = boost::asio::posix::stream_descriptor;
#endif

    explicit IOSession(boost::asio::io_context &ioCtx)
#ifdef _WIN32
        : stdin_(ioCtx, ::GetStdHandle(STD_INPUT_HANDLE)),
          stdout_(ioCtx, ::GetStdHandle(STD_OUTPUT_HANDLE))
#else
        : stdin_(ioCtx, ::dup(STDIN_FILENO)),
          stdout_(ioCtx, ::dup(STDOUT_FILENO))
#endif
    {
    }

    ~IOSession() override {
        Close();
    }

    auto Read() -> boost::asio::awaitable<std::string> override {
        // 一次性读取直到头部结束标记 "\r\n\r\n"
        std::size_t headerLen
            = co_await boost::asio::async_read_until(stdin_, buffer_, lsp::Delimiter, boost::asio::use_awaitable);

        // 解析头部
        const std::string header = boost::beast::buffers_to_string(buffer_.data());

        const std::size_t contentLen = doLength(header);
        buffer_.consume(headerLen);  // 移除已处理的头部数据

        // 读取消息体
        if (contentLen > 0) {
            auto readLen = co_await boost::asio::async_read(stdin_,
                                                            buffer_,
                                                            boost::asio::transfer_exactly(contentLen),
                                                            boost::asio::use_awaitable);

            if (readLen != contentLen) {
                throw std::runtime_error("Incomplete body read");
            }

            std::string body = boost::beast::buffers_to_string(buffer_.data());
            buffer_.consume(contentLen);
            co_return body;
        }
        co_return std::string{};
    }

    auto Write(const std::string &response) -> boost::asio::awaitable<void> override {
        auto message = std::format("{}{}{}{}", lsp::HeaderLen, response.size(), lsp::Delimiter, response);
        co_await boost::asio::async_write(stdout_, boost::asio::buffer(message), boost::asio::use_awaitable);
    }

    void Close() override {
        if (stdin_.is_open())
            stdin_.close();
        if (stdout_.is_open())
            stdout_.close();
    }

private:
    static auto doLength(const std::string &header) -> std::size_t {
        std::istringstream stream(header);
        std::string        line;

        while (std::getline(stream, line)) {
            if (line.substr(0, lsp::HeaderLen.size()) == lsp::HeaderLen) {
                std::string valueStr = line.substr(lsp::HeaderLen.size());

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

    StdioStream               stdin_;
    StdioStream               stdout_;
    boost::beast::flat_buffer buffer_{};
};
}  // namespace infra::transport