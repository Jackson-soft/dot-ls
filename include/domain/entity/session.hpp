#pragma once

#include "utils/log.hpp"

#include <boost/cobalt/promise.hpp>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>

namespace domain::entity {
// 接口
class Session {
public:
    Session() = default;

    virtual ~Session() = default;

    virtual auto Read(std::span<char> buffer) -> std::size_t = 0;
    virtual void Write(std::string_view content)             = 0;
    virtual void Close()                                     = 0;
};

// 标准输入输出流实现
class IOSession : public Session, std::enable_shared_from_this<IOSession> {
public:
    IOSession() = default;

    ~IOSession() override = default;

    auto Read(std::span<char> buffer) -> std::size_t override {
        char buffers[1024];
        std::cin.peek();
        auto length = static_cast<std::size_t>(std::cin.readsome(buffers, std::size(buffers)));
        uranus::utils::LogHelper::Instance().Info("read message length: {}\n", length);

        return length;
    }

    void Write(std::string_view content) override {
        std::cout.write(content.data(), content.size());
        std::cout.flush();
    }

    void Close() override {
        std::cout.flush();
    }
};
}  // namespace domain::entity