#pragma once

#include "basis/boot/flag.hpp"
#include "domain/model/lifecycle.hpp"

// 生命周期相关服务
namespace domain::service {
class Lifecycle {
public:
    Lifecycle()  = default;
    ~Lifecycle() = default;

    auto Initialize(const domain::model::InitializeParams &param) -> domain::model::InitializeResult {
        domain::model::InitializeResult result;
        result.serverInfo.name    = basic::boot::Name;
        result.serverInfo.version = basic::boot::Version;
        return result;
    }
};
}  // namespace domain::service