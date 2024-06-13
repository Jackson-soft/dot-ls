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

    virtual void Run()                          = 0;
    virtual void Send(std::string_view content) = 0;
    virtual void Close()                        = 0;
};

// 标准输入输出流实现
class IOSession : public Session, std::enable_shared_from_this<IOSession> {
public:
    IOSession() = default;

    ~IOSession() override {}

    void Run() override {}

    void Read() {
        std::cin.peek();
        char *buffer{nullptr};
        std::cin.readsome(buffer, 128);
    }

    void Send(std::string_view content) override {
        std::cout.write(content.data(), content.size());
    }

    void Close() override {
        std::cout.flush();
    }

private:
    char *buffer_{};
};
}  // namespace domain::entity