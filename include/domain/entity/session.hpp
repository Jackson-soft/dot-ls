#pragma once

#include "domain/model/flag.hpp"
#include "utils/log.hpp"

#include <boost/asio.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/cobalt.hpp>
#include <cstddef>
#include <format>
#include <iostream>

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
    explicit IOSession(boost::asio::io_context ioCtx)
        : stdin_(ioCtx, ::dup(STDIN_FILENO)), stdout_(ioCtx, ::dup(STDOUT_FILENO)) {}

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
            const size_t headerLen = co_await boost::asio::async_read_until(
                stdin_,
                buffer,
                "\r\n\r\n",
                boost::cobalt::use_op);

            // 解析头部
            std::string header{static_cast<const char *>(buffer.data().data()), headerLen};

            const std::size_t contentLen = doLength(header);
            buffer.consume(headerLen); // 移除已处理的头部数据

            // 读取消息体
            if (contentLen > 0) {
                if (buffer.size() < contentLen) {
                    buffer.reserve(contentLen);
                }
                // 精确读取指定长度内容
                const size_t readLen = co_await boost::asio::async_read(stdin_,
                                                                        buffer.prepare(contentLen - buffer.size()),
                                                                        boost::asio::transfer_exactly(
                                                                            contentLen - buffer.size()),
                                                                        boost::cobalt::use_op);

                if (readLen != contentLen) {
                    throw std::runtime_error("Incomplete body read");
                }
                buffer.commit(readLen);

                // 直接返回缓冲区视图
                std::string_view bodyView{
                    static_cast<const char *>(buffer.data().data()),
                    contentLen
                };
                co_yield bodyView;

                buffer.consume(contentLen);
            }
        }
    }

    auto Write(const std::string &response) -> boost::cobalt::promise<void> override {
        auto message = std::format("{}{}{}", domain::model::HeaderLen, response.size(), domain::model::Delimiter);
        co_await boost::asio::async_write(stdout_, boost::asio::buffer(message), boost::cobalt::use_op);
    }

    void Close() override {}

private:
    auto doLength(const std::string &header) -> std::size_t {
        std::istringstream stream(header);
        std::string        line;

        while (std::getline(stream, line)) {
            if (line.substr(0, 15) == domain::model::HeaderLen) {
                std::string valueStr = line.substr(15);

                // 去除可能的回车符
                const size_t cr_pos = valueStr.find('\r');
                if (cr_pos != std::string::npos)
                    valueStr = valueStr.substr(0, cr_pos);

                try {
                    return std::stoul(valueStr);
                } catch (...) {
                    return 0; // 无效长度
                }
            }
        }
        return 0; // 未找到 Content-Length
    }

    boost::asio::posix::stream_descriptor stdin_;  // 标准输入
    boost::asio::posix::stream_descriptor stdout_; // 标准输出
};
} // namespace domain::entity