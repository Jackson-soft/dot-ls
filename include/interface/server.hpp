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
#include <cstdlib>
#include <format>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <print>
#include <string>
#include <unordered_map>
#include <utility>

namespace interface_ {

// LSP 生命周期状态
// Uninitialized ──(initialize)──► Running ──(shutdown)──► ShuttingDown
//                                                              │
//                                                         (exit) ▼
//                                                         std::exit(0)
enum class State { Uninitialized, Running, ShuttingDown };

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
    if (stdin_.is_open())  stdin_.close();
    if (stdout_.is_open()) stdout_.close();
  }

private:
  // ── 生命周期处理器 ───────────────────────────────────────────────────────────

  // shutdown: 标记服务端准备退出，返回 null（等 exit 通知再真正退出）
  auto Shutdown(const nlohmann::json & /*params*/) -> nlohmann::json {
    state_ = State::ShuttingDown;
    return nullptr; // LSP 规范要求 shutdown 结果为 null
  }

  // exit: shutdown 之后收到 exit → 正常退出(0)；未经 shutdown 直接 exit → 异常退出(1)
  auto Exit(const nlohmann::json & /*params*/) -> nlohmann::json {
    Close();
    std::exit(state_ == State::ShuttingDown ? 0 : 1);
  }

  // ── 辅助：构造 JSON-RPC 错误对象 ─────────────────────────────────────────────
  static auto makeError(int code, std::string_view message) -> nlohmann::json {
    return nlohmann::json{{"code", code}, {"message", message}};
  }

  // ── 辅助：将响应写回 stdout ───────────────────────────────────────────────────
  auto sendResponse(uranus::jsonrpc::Response *output)
      -> boost::cobalt::task<void> {
    auto message = std::format("{}{}{}{}", lsp::HeaderLen,
                               output->String().size(), lsp::Delimiter,
                               output->String());
    co_await boost::asio::async_write(stdout_, boost::asio::buffer(message),
                                      boost::cobalt::use_task);
  }

  // ── 辅助：发送错误响应 ────────────────────────────────────────────────────────
  auto sendError(const uranus::jsonrpc::Id &id, int code,
                 std::string_view message) -> boost::cobalt::task<void> {
    const auto output = response_.construct();
    output->SetId(id);
    output->SetError(makeError(code, message));
    co_await sendResponse(output);
    response_.destroy(output);
  }

  // ── 注册处理接口 ──────────────────────────────────────────────────────────────
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
    handler_.emplace("exit", [this](const nlohmann::json &params) {
      return Exit(params);
    });
  }

  // ── 读取一条完整的 LSP 消息体 ─────────────────────────────────────────────────
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

  // ── 分发并处理一条消息 ────────────────────────────────────────────────────────
  auto dispatch(const std::string &body) -> boost::cobalt::task<void> {
    const auto request = request_.construct();

    if (!request->Parse(body)) {
      // 解析失败：-32700 ParseError
      // 无法确定 id，按规范返回 null id
      const uranus::jsonrpc::Id nullId{nullptr};
      co_await sendError(nullId, -32700, "Parse error");
      request_.destroy(request);
      co_return;
    }

    const auto &method = request->Method();
    const auto &id     = request->GetId();
    const bool isNotification = request->IsNotification();

    // ── 生命周期守卫 ──────────────────────────────────────────────────────────

    // 服务端未初始化时只允许 initialize
    if (state_ == State::Uninitialized && method != "initialize") {
      if (!isNotification) {
        co_await sendError(id, -32002, "Server not initialized");
      }
      request_.destroy(request);
      co_return;
    }

    // shutdown 之后只允许 exit
    if (state_ == State::ShuttingDown && method != "exit") {
      if (!isNotification) {
        co_await sendError(id, -32600, "Invalid request: server is shutting down");
      }
      request_.destroy(request);
      co_return;
    }

    // ── 查找并调用处理器 ──────────────────────────────────────────────────────
    if (const auto it = handler_.find(method); it != handler_.end()) {
      auto result = it->second(request->Params());

      // initialize 成功后推进状态
      if (method == "initialize") {
        state_ = State::Running;
      }

      // 仅对有 id 的请求（非通知）发送响应
      if (!isNotification) {
        const auto output = response_.construct();
        output->SetId(id);
        output->SetResult(result);
        co_await sendResponse(output);
        response_.destroy(output);
      }
    } else if (!isNotification) {
      // 未知方法：-32601 MethodNotFound
      co_await sendError(id, -32601, "Method not found: " + method);
    }

    request_.destroy(request);
  }

  // ── 成员 ──────────────────────────────────────────────────────────────────────
  boost::asio::posix::stream_descriptor stdin_;   // 标准输入
  boost::asio::posix::stream_descriptor stdout_;  // 标准输出
  boost::asio::streambuf buffer_{};               // 可自动扩容的缓冲区
  std::shared_ptr<application::App> app_{std::make_shared<application::App>()};
  std::unordered_map<
      std::string, std::function<nlohmann::json(const nlohmann::json &)>>
      handler_{};
  boost::object_pool<uranus::jsonrpc::Request>  request_{};   // 请求池
  boost::object_pool<uranus::jsonrpc::Response> response_{};  // 响应池
  State state_{State::Uninitialized};                         // 生命周期状态
};
} // namespace interface_
