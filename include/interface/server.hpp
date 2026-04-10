#pragma once

// 用户界面层

#include "application/app.hpp"
#include "infra/common/ring_buffer.hpp"
#include "jsonrpc/request.hpp"
#include "jsonrpc/response.hpp"
#include "protocol/lsp/basic.hpp"
#include "protocol/lsp/flag.hpp"

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/pool/object_pool.hpp>
#include <cstdlib>
#include <format>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

namespace interface {

// LSP 生命周期状态
// Uninitialized ──(initialize)──► Running ──(shutdown)──► ShuttingDown
//                                                              │
//                                                         (exit) ▼
//                                                         std::exit(0)
enum class State { Uninitialized, Running, ShuttingDown };

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
            if (auto message = co_await read(); !message.empty()) {
                dispatch(message);
                co_await flushTx();
            }
        }
    }

    void Close() {
        if (stdin_.is_open())
            stdin_.close();
        if (stdout_.is_open())
            stdout_.close();
    }

private:
    // ── 生命周期处理器 ───────────────────────────────────────────────────────────

    // shutdown: 标记服务端准备退出，返回 null（等 exit 通知再真正退出）
    auto Shutdown(const nlohmann::json & /*params*/) -> nlohmann::json {
        state_ = State::ShuttingDown;
        return nullptr;  // LSP 规范要求 shutdown 结果为 null
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

    // ── 辅助：发送服务端主动通知 ────────────────────────────────────────────────────────
    void sendNotification(const std::string &method, const nlohmann::json &params) {
        nlohmann::json notif;
        notif["jsonrpc"]       = "2.0";
        notif["method"]        = method;
        notif["params"]        = params;
        const std::string body = notif.dump();
        const auto        msg  = std::format("{}{}{}{}", lsp::HeaderLen, body.size(), lsp::Delimiter, body);
        txBuf_.push(std::span<const char>(msg.data(), msg.size()));
    }

    // ── 将格式化响应追加到发送环形缓冲区 ──────────────────────────────────────
    void sendResponse(uranus::jsonrpc::Response *output) {
        const auto msg
            = std::format("{}{}{}{}", lsp::HeaderLen, output->String().size(), lsp::Delimiter, output->String());
        txBuf_.push(std::span<const char>(msg.data(), msg.size()));
    }

    // ── 辅助：发送错误响应 ────────────────────────────────────────────────────────
    void sendError(const uranus::jsonrpc::Id &id, int code, std::string_view message) {
        const auto output = response_.construct();
        output->SetId(id);
        output->SetError(makeError(code, message));
        sendResponse(output);
        response_.destroy(output);
    }

    // ── 辅助：将 txBuf_ 中的字节刷入 stdout ──────────────────────────────────────
    auto flushTx() -> boost::asio::awaitable<void> {
        while (!txBuf_.empty()) {
            const std::size_t toSend = std::min(txBuf_.size(), std::size_t{65536});
            std::string       chunk(toSend, '\0');
            txBuf_.copy_out(toSend, chunk.data());
            txBuf_.consume(toSend);
            co_await boost::asio::async_write(stdout_, boost::asio::buffer(chunk), boost::asio::use_awaitable);
        }
    }

    // ── 注册处理接口 ──────────────────────────────────────────────────────────────
    void enroll() {
        handler_.emplace("initialize", [app = app_](const nlohmann::json &params) {
            return app->Initialize(params);
        });
        handler_.emplace("initialized", [app = app_](const nlohmann::json &params) {
            return app->Initialized(params);
        });
        handler_.emplace("textDocument/didOpen", [app = app_](const nlohmann::json &params) {
            return app->DidOpen(params);
        });
        handler_.emplace("textDocument/didSave", [app = app_](const nlohmann::json &params) {
            return app->DidSave(params);
        });
        handler_.emplace("textDocument/didChange", [app = app_](const nlohmann::json &params) {
            return app->DidChange(params);
        });
        handler_.emplace("textDocument/didClose", [app = app_](const nlohmann::json &params) {
            return app->DidClose(params);
        });
        handler_.emplace("textDocument/rename", [app = app_](const nlohmann::json &params) {
            return app->Rename(params);
        });
        handler_.emplace("textDocument/completion", [app = app_](const nlohmann::json &params) {
            return app->Completion(params);
        });
        handler_.emplace("completionItem/resolve", [app = app_](const nlohmann::json &params) {
            return app->Resolve(params);
        });
        handler_.emplace("shutdown", [this](const nlohmann::json &params) {
            return Shutdown(params);
        });
        handler_.emplace("exit", [this](const nlohmann::json &params) {
            return Exit(params);
        });
        // ── 新功能 handler ────────────────────────────────────────────────────────────────────
        handler_.emplace("textDocument/hover", [app = app_](const nlohmann::json &params) {
            return app->Hover(params);
        });
        handler_.emplace("textDocument/definition", [app = app_](const nlohmann::json &params) {
            return app->Definition(params);
        });
        handler_.emplace("textDocument/declaration", [app = app_](const nlohmann::json &params) {
            return app->Definition(params);
        });
        handler_.emplace("textDocument/references", [app = app_](const nlohmann::json &params) {
            return app->DocumentHighlight(params);  // reuse highlight as reference list
        });
        handler_.emplace("textDocument/documentHighlight", [app = app_](const nlohmann::json &params) {
            return app->DocumentHighlight(params);
        });
        handler_.emplace("textDocument/semanticTokens/full", [app = app_](const nlohmann::json &params) {
            return app->SemanticTokensFull(params);
        });
    }

    // ── 读取一条完整的 LSP 消息体 ─────────────────────────────────────────────────
    auto read() -> boost::asio::awaitable<std::string> {
        while (true) {
            // 从 stdin 读取可用字节，追加到接收环形缓冲区
            std::array<char, 4096> tmp{};
            const std::size_t n = co_await stdin_.async_read_some(boost::asio::buffer(tmp), boost::asio::use_awaitable);
            rxBuf_.push(std::span<const char>(tmp.data(), n));

            // 尝试从缓冲区解析完整 LSP 消息
            if (auto msg = tryParseMessage()) {
                co_return std::move(*msg);
            }
        }
    }

    // ── 从接收环形缓冲区解析一条完整消息（无副作用地探测，成功后消费） ────────────────
    auto tryParseMessage() -> std::optional<std::string> {
        // 查找头部结束标记 "\r\n\r\n"
        constexpr std::array<char, 4> kDelim   = {'\r', '\n', '\r', '\n'};
        const std::size_t             delimPos = rxBuf_.find(std::span<const char>(kDelim.data(), kDelim.size()));
        if (delimPos == infra::common::RingBuffer<char>::npos)
            return std::nullopt;

        // 提取并解析头部
        std::string header(delimPos, '\0');
        rxBuf_.copy_out(delimPos, header.data());

        const auto prefixPos = header.find(lsp::HeaderLen);
        if (prefixPos == std::string::npos) {
            rxBuf_.consume(delimPos + kDelim.size());  // 跳过格式错误的头部
            return std::nullopt;
        }
        const std::size_t bodyLen = std::stoul(header.substr(prefixPos + lsp::HeaderLen.size()));

        // body 尚未完全到达，继续读
        if (rxBuf_.size() < delimPos + kDelim.size() + bodyLen)
            return std::nullopt;

        // 消费头部 + 分隔符，提取 body
        rxBuf_.consume(delimPos + kDelim.size());
        std::string body(bodyLen, '\0');
        for (std::size_t i = 0; i < bodyLen; ++i)
            rxBuf_.pop(body[i]);
        return body;
    }

    // ── 分发并处理一条消息（同步；响应写入 txBuf_，由 flushTx() 统一刷出） ──────────
    void dispatch(const std::string &body) {
        const auto request = request_.construct();

        if (!request->Parse(body)) {
            // 解析失败：-32700 ParseError
            // 无法确定 id，按规范返回 null id
            const uranus::jsonrpc::Id nullId{nullptr};
            sendError(nullId, -32700, "Parse error");
            request_.destroy(request);
            return;
        }

        const auto &method         = request->Method();
        const auto &id             = request->GetId();
        const bool  isNotification = request->IsNotification();

        // ── 生命周期守卫 ──────────────────────────────────────────────────────────

        // 服务端未初始化时只允许 initialize
        if (state_ == State::Uninitialized && method != "initialize") {
            if (!isNotification) {
                sendError(id, -32002, "Server not initialized");
            }
            request_.destroy(request);
            return;
        }

        // shutdown 之后只允许 exit
        if (state_ == State::ShuttingDown && method != "exit") {
            if (!isNotification) {
                sendError(id, -32600, "Invalid request: server is shutting down");
            }
            request_.destroy(request);
            return;
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
                sendResponse(output);
                response_.destroy(output);
            }
        } else if (!isNotification) {
            // 未知方法：-32601 MethodNotFound
            sendError(id, -32601, "Method not found: " + method);
        }

        // 文档变更后推送诊断（在 request 销毁前调用，method / Params() 此时仍有效）
        if (method == "textDocument/didOpen" || method == "textDocument/didChange"
            || method == "textDocument/didSave") {
            const auto diagParams = app_->Diagnose(request->Params());
            if (!diagParams.is_null()) {
                sendNotification("textDocument/publishDiagnostics", diagParams);
            }
        }

        request_.destroy(request);
    }

    // ── 成员 ──────────────────────────────────────────────────────────────────────
    boost::asio::posix::stream_descriptor stdin_;           // 标准输入
    boost::asio::posix::stream_descriptor stdout_;          // 标准输出
    infra::common::RingBuffer<char>       rxBuf_{1 << 20};  // 1 MB 接收环形缓冲区
    infra::common::RingBuffer<char>       txBuf_{1 << 20};  // 1 MB 发送环形缓冲区
    std::shared_ptr<application::App>     app_{std::make_shared<application::App>()};
    std::unordered_map<std::string, std::function<nlohmann::json(const nlohmann::json &)>> handler_{};
    boost::object_pool<uranus::jsonrpc::Request>                                           request_{};   // 请求池
    boost::object_pool<uranus::jsonrpc::Response>                                          response_{};  // 响应池
    State state_{State::Uninitialized};                                                                  // 生命周期状态
};
}  // namespace interface
