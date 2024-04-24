#pragma once

namespace hesiod::lsp {
class LSP {
public:
    LSP()          = default;
    virtual ~LSP() = 0;

    virtual void Register() = 0;  // 注册处理接口
};
}  // namespace hesiod::lsp