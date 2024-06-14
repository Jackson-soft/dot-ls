#pragma once

#include <iostream>
#include <memory>
#include <string_view>

namespace domain::entity {
// 接口
class Session {
public:
    Session() = default;

    virtual ~Session() {}

    virtual auto Read(char *buffer, std::size_t maxSize) -> std::size_t = 0;
    virtual void Write(std::string_view content)                        = 0;
    virtual void Close()                                                = 0;
};

// 标准输入输出流实现
class IOSession : public Session, std::enable_shared_from_this<IOSession> {
public:
    IOSession() = default;

    ~IOSession() override {}

    auto Read(char *buffer, std::size_t maxSize) -> std::size_t override {
        std::cin.peek();
        return static_cast<std::size_t>(std::cin.readsome(buffer, maxSize));
    }

    void Write(std::string_view content) override {
        std::cout.write(content.data(), content.size());
        std::cout.flush();
    }

    void Close() override {
        std::cout.flush();
    }

private:
    char *buffer_{};
};
}  // namespace domain::entity