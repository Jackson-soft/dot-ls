#pragma once

// 用户界面层

#include "application/app.hpp"
#include "jsonrpc/request.hpp"
#include "jsonrpc/response.hpp"
#include "protocol/lsp/basic.hpp"
#include "protocol/lsp/flag.hpp"

#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <boost/pool/object_pool.hpp>
#include <format>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <print>
#include <string>
#include <unordered_map>
#include <utility>

namespace interface_ {
class Server {
public:
  explicit Server(boost::asio::io_context &ioCtx)
      : stdin_(ioCtx, ::dup(STDIN_FILENO)),
        stdout_(ioCtx, ::dup(STDOUT_FILENO)) {
    enroll();
  }

  ~Server() { Close(); }

  auto Run() -> boost::cobalt::task<void> {
    while (true) {
      if (auto message = co_await read(); !message.empty()) {
        co_await dispatch(message);
      }
    }
  }

  void Close() {
    stdin_.close();
    stdout_.close();
  }

  auto Shutdown(const nlohmann::json &params) -> nlohmann::json {
    // shutdown 只返回确认，不关闭连接（等 exit 通知再关闭）
    return nlohmann::json::object();
  }

  auto Exit(const nlohmann::json &params) -> nlohmann::json {
    Close();
    return nlohmann::json::object();
  }

private:
  // 注册处理接口
  void enroll() {
    handler_.emplace("initialize", [app = app_](const nlohmann::json &params) {
      return app->Initialize(params);
    });
    handler_.emplace("initialized", [app = app_](const nlohmann::json &params) {
      return app->Initialized(params);
    });
    handler_.emplace("textDocument/didOpen",
                     [app = app_](const nlohmann::json &params) {
                       return app->DidOpen(params);
                     });
    handler_.emplace("textDocument/didSave",
                     [app = app_](const nlohmann::json &params) {
                       return app->DidSave(params);
                     });
    handler_.emplace("textDocument/didChange",
                     [app = app_](const nlohmann::json &params) {
                       return app->DidChange(params);
                     });
    handler_.emplace("textDocument/didClose",
                     [app = app_](const nlohmann::json &params) {
                       return app->DidClose(params);
                     });
    handler_.emplace("textDocument/rename",
                     [app = app_](const nlohmann::json &params) {
                       return app->Rename(params);
                     });
    handler_.emplace("textDocument/completion",
                     [app = app_](const nlohmann::json &params) {
                       return app->Completion(params);
                     });
    handler_.emplace("completionItem/resolve",
                     [app = app_](const nlohmann::json &params) {
                       return app->Resolve(params);
                     });
    handler_.emplace("shutdown", [this](const nlohmann::json &params) {
      return Shutdown(params);
    });
    handler_.emplace(
        "exit", [this](const nlohmann::json &params) { return Exit(params); });
  }

  auto read() -> boost::cobalt::task<std::string> {
    while (true) {
      // 一次性读取直到头部结束标记 "\r\n\r\n"
      co_await boost::asio::async_read_until(stdin_, buffer_, lsp::Delimiter,
                                             boost::cobalt::use_task);

      // 解析头部
      std::istream is(&buffer_);
      std::string header;
      std::getline(is, header);

      if (!header.empty() && header.back() == '\r') {
        header.pop_back();
      }

      // 跳过空行 (delimiter 的第二个 \r\n)
      if (header.empty()) {
        continue;
      }

      if (header.starts_with(lsp::HeaderLen)) {
        const std::size_t length = std::stoul(header.substr(16));

        // 跳过剩余的头部空行 (\r\n)
        std::string skip_line;
        std::getline(is, skip_line);

        // 先从 buffer_ 中取已缓存的数据
        const std::size_t buffered = buffer_.size();
        std::string content(length, '\0');

        if (buffered >= length) {
          // buffer 中已有完整消息体
          is.read(content.data(), static_cast<std::streamsize>(length));
        } else {
          // 先取 buffer 中的部分
          if (buffered > 0) {
            is.read(content.data(), static_cast<std::streamsize>(buffered));
          }
          // 再从 stdin 读取剩余部分
          const std::size_t remaining = length - buffered;
          const std::size_t readLen = co_await boost::asio::async_read(
              stdin_, boost::asio::buffer(content.data() + buffered, remaining),
              boost::cobalt::use_task);
          if (readLen != remaining) {
            co_return "";
          }
        }

        co_return content;
      }
    }
  }

  auto dispatch(const std::string &body) -> boost::cobalt::task<void> {
    // 解析数据
    const auto request = request_.construct();
    if (const auto success = request->Parse(body); success) {
      if (const auto handler = handler_.find(request->Method().data());
          handler != handler_.end()) {
        auto result = handler->second(request->Params());

        // 仅对有 id 的请求发送响应 (通知没有 id，不需要响应)
        auto id = request->Id();
        if (id.index() != 2) { // index 2 = void* (nullptr) = notification
          const auto output = response_.construct(std::get<0>(id));
          output->SetResult(result);
          auto message =
              std::format("{}{}{}{}", lsp::HeaderLen, output->String().size(),
                          lsp::Delimiter, output->String());
          co_await boost::asio::async_write(
              stdout_, boost::asio::buffer(message), boost::cobalt::use_task);
          response_.destroy(output);
        }
      }
    }

    request_.destroy(request);
  }

  boost::asio::posix::stream_descriptor stdin_;  // 标准输入
  boost::asio::posix::stream_descriptor stdout_; // 标准输出
  boost::asio::streambuf buffer_{};              // 可自动扩容的缓冲区
  std::shared_ptr<application::App> app_{std::make_shared<application::App>()};
  std::unordered_map<
      std::string, std::function<nlohmann::json(const nlohmann::json &params)>>
      handler_{};
  // 接口处理
  boost::object_pool<uranus::jsonrpc::Request> request_{};   // 请求池
  boost::object_pool<uranus::jsonrpc::Response> response_{}; // 响应池
};
} // namespace interface_
