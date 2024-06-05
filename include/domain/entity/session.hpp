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
    virtual void Stop()                         = 0;
};

// 标准输入输出流实现
class IOSession : public Session, std::enable_shared_from_this<IOSession> {
public:
    IOSession() = default;

    ~IOSession() override {}

    void Run() override {
        while (true) {
            std::cin.peek();
            char *buffer{nullptr};
            std::cin.readsome(buffer, 128);

            std::cout.write("sfsfs\n", 7);

            std::cout.flush();
        }
    }

    void Send(std::string_view content) override {}

    void Stop() override {}
};
}  // namespace domain::entity