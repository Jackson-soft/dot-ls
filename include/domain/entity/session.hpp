#pragma once

#include "utils/log.hpp"

#include <iostream>
#include <memory>
#include <span>
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/buffers_to_string.hpp>

namespace domain::entity {
// 接口
class Session {
public:
    Session() = default;

    virtual ~Session() = default;

    virtual auto Read() -> boost::cobalt::generator<std::string> = 0;
    virtual auto Write(const std::string &response) -> boost::cobalt::promise<void> = 0;
    virtual void Close() = 0;
};

// 标准输入输出流实现
class IOSession final : public Session, std::enable_shared_from_this<IOSession> {
public:
    explicit IOSession(boost::asio::io_context ioCtx): stdin_(ioCtx, ::dup(STDIN_FILENO)),
                                                       stdout_(ioCtx, ::dup(STDOUT_FILENO)) {}

    ~IOSession() override = default;


    /*
    Content-Length: ...\r\n\r\n
    {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "textDocument/completion",
        "params": {
            ...
        }
    }
    */
    auto Read() -> boost::cobalt::generator<std::string> override {
        boost::beast::flat_buffer buffer; // 可自动扩容的缓冲区

        while (true) {
            // 一次性读取直到头部结束标记 "\r\n\r\n"
            size_t header_end = co_await boost::asio::async_read_until(
                stdin_,
                buffer,
                "\r\n\r\n",
                boost::cobalt::use_op
                );

            // 解析头部
            std::string_view headers_view = boost::beast::buffers_to_string(buffer);

            size_t content_length = 0;
            buffer.consume(header_end); // 移除已处理的头部数据

            // 读取消息体
            if (content_length > 0) {
                std::string body;
                body.resize(content_length);

                // 精确读取指定长度内容
                const size_t bytes_read = co_await boost::asio::async_read(
                    stdin_,
                    boost::asio::buffer(body),
                    boost::asio::transfer_exactly(content_length),
                    boost::cobalt::use_op
                    );

                if (bytes_read != content_length) {
                    throw std::runtime_error("Incomplete body read");
                }

                co_yield std::move(body);
            }
        }
    }

    auto Write(const std::string &response) -> boost::cobalt::promise<void> override {
        co_await boost::asio::async_write(stdout_, boost::asio::buffer(response), boost::cobalt::use_op);
    }

    void Close() override {
        std::cout.flush();
    }

private:
    boost::asio::posix::stream_descriptor stdin_;  // 标准输入
    boost::asio::posix::stream_descriptor stdout_; // 标准输出
};
} // namespace domain::entity