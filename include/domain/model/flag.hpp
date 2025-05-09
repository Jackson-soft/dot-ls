#pragma once

#include <string>
#include <vector>

namespace domain::model {
const std::vector<std::string> EOL{"\n", "\r\n", "\r"};
const std::string              HeaderLen{"Content-Length: "};  // 一个冒号和一个空格
const std::string              HeaderType{"Content-Type: "};   // Defaults to application/vscode-jsonrpc; charset=utf-8
const std::string              Delimiter{"\r\n\r\n"};          // Content-Length 分隔符
}  // namespace domain::model