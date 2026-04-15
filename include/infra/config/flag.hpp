#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace infra::config {
// 使用 string_view 避免 constexpr std::string 的跨编译器兼容问题
constexpr std::string_view Version{"v0.1.0"};
constexpr std::string_view Name{"dot-ls"};

// inline 避免多 TU 包含时的 ODR 违规
inline const std::vector<std::string> Keywords{"node", "edge", "graph", "digraph", "subgraph", "strict"};
}  // namespace infra::config