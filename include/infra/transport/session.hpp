#pragma once

#include "protocol/lsp/flag.hpp"

#include <boost/asio.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <cstddef>
#include <format>
#include <iostream>

namespace infra::transport {
// 接口
class Session {
public:
  Session() = default;

  virtual ~Session() = default;

  virtual auto Read() -> boost::asio::awaitable<void> = 0;
  virtual auto Write(const std::string &response)
      -> boost::asio::awaitable<void> = 0;
  virtual void Close() = 0;
};

// 标准输入输出流实现
class IOSession final : public Session,
                        std::enable_shared_from_this<IOSession> {
public:
  explicit IOSession(boost::asio::io_context ioCtx)
      : stdin_(ioCtx, ::dup(STDIN_FILENO)),
        stdout_(ioCtx, ::dup(STDOUT_FILENO)) {}

  ~IOSession() override {
    Close();
    buffer_.clear();
  }

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
  auto Read() -> boost::asio::awaitable<void> override {
    // 一次性读取直到头部结束标记 "\r\n\r\n"
    std::size_t headerLen = co_await boost::asio::async_read_until(
        stdin_, buffer_, lsp::Delimiter, boost::asio::use_awaitable);

    // 解析头部
    const std::string header = boost::beast::buffers_to_string(buffer_.data());

    const std::size_t contentLen = doLength(header);
    buffer_.consume(headerLen); // 移除已处理的头部数据

    // 读取消息体
    if (contentLen > 0) {
      if (buffer_.size() < contentLen) {
        buffer_.reserve(contentLen);
      }
      // 精确读取指定长度内容
      auto readLen = co_await boost::asio::async_read(
          stdin_, buffer_, boost::asio::transfer_exactly(contentLen),
          boost::asio::use_awaitable);

      if (readLen != contentLen) {
        throw std::runtime_error("Incomplete body read");
      }

      // 直接返回缓冲区视图
      std::string body = boost::beast::buffers_to_string(buffer_.data());

      buffer_.consume(contentLen);

      co_return body;
    }
  }

  auto Write(const std::string &response)
      -> boost::asio::awaitable<void> override {
    auto message = std::format("{}{}{}{}", lsp::HeaderLen, response.size(),
                               lsp::Delimiter, response);
    co_await boost::asio::async_write(stdout_, boost::asio::buffer(message),
                                      boost::asio::use_awaitable);
  }

  void Close() override {
    stdin_.close();
    stdout_.close();
  }

private:
  static auto doLength(const std::string &header) -> std::size_t {
    std::istringstream stream(header);
    std::string line;

    while (std::getline(stream, line)) {
      if (line.substr(0, 15) == lsp::HeaderLen) {
        std::string valueStr = line.substr(15);

        // 去除可能的回车符
        if (const size_t cr_pos = valueStr.find('\r');
            cr_pos != std::string::npos)
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
  boost::beast::flat_buffer buffer_{};           // 可自动扩容的缓冲区
};
} // namespace infra::transport