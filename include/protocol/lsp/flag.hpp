#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lsp {
// inline 避免多 TU 包含时的 ODR 违规
// string_view 常量：constexpr，不涉及堆分配，消除 cert-err58 警告
inline const std::vector<std::string> EOL{"\n", "\r\n", "\r"};
constexpr std::string_view            HeaderLen{"Content-Length: "};
constexpr std::string_view            HeaderType{"Content-Type: "};
constexpr std::string_view            Delimiter{"\r\n\r\n"};
}  // namespace lsp