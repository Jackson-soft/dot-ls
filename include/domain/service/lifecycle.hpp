#pragma once

#include "infra/boot/flag.hpp"
#include "lsp/lifecycle.hpp"

// 生命周期相关服务
namespace domain::service {
class Lifecycle {
public:
    Lifecycle()  = default;
    ~Lifecycle() = default;

    auto Initialize(const lsp::InitializeParams &param) -> lsp::InitializeResult {
        lsp::InitializeResult result;
        result.serverInfo.name    = basic::boot::Name;
        result.serverInfo.version = basic::boot::Version;
        return result;
    }
};
}  // namespace domain::service