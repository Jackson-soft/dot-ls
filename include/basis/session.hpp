#pragma once

#include <memory>
#include <string_view>

namespace hesiod::basis {
// 接口
class Session {
public:
    Session()          = default;
    virtual ~Session() = 0;

    virtual void Run()                          = 0;
    virtual void Send(std::string_view content) = 0;
    virtual void Stop()                         = 0;
};

// 标准输入输出流实现
class IOSession : public Session, public std::enable_shared_from_this<IOSession> {};
}  // namespace hesiod::basis