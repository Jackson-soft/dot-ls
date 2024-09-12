#pragma once

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

    virtual ~Session() {}

    virtual auto Read(std::span<char> buffer) -> boost::cobalt::promise<std::size_t> = 0;
    virtual void Write(std::string_view content)                                     = 0;
    virtual void Close()                                                             = 0;
};

// 标准输入输出流实现
class IOSession : public Session, std::enable_shared_from_this<IOSession> {
public:
    IOSession() = default;

    ~IOSession() override {}

    auto Read(std::span<char> buffer) -> boost::cobalt::promise<std::size_t> override {
        std::cin.peek();
        co_return static_cast<std::size_t>(std::cin.readsome(buffer.data(), buffer.size()));
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