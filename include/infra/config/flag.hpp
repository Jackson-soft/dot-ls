#pragma once

#include <string>
#include <vector>

namespace infra::config {
constexpr const std::string Version{"v0.1.0"};
constexpr const std::string Name{"dot-ls"};

const std::vector<std::string> Keywords{"node",    "edge",     "graph",
                                        "digraph", "subgraph", "strict"};
} // namespace infra::config