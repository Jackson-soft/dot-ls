#pragma once

// UTF-8 字节偏移 ↔ UTF-16 code unit 偏移转换工具
//
// LSP 协议中 Position.character 默认按 UTF-16 code unit 计数（可通过
// general.positionEncodings 协商为 utf-8 / utf-32，但本项目尚未做协商，始终
// 声明为 "utf-16"）。而内部实现（std::string / tree-sitter）全部基于 UTF-8
// 字节偏移。当文档中出现任何非 ASCII 字符（如中文标签）时，若不做转换，两者会
// 产生偏差，导致 hover / definition / completion / diagnostics 等功能定位错误。
//
// 本文件提供两者之间的转换，只应在 LSP 协议边界（收到 Position 时 / 构造
// Range、Position 返回给客户端时）调用；内部处理仍然全程使用字节偏移。

#include <cstdint>
#include <string_view>

namespace infra::common {

// 返回以 lead 开头的 UTF-8 序列长度（1-4 字节）。非法前导字节按 1 处理，避免越界。
inline uint32_t Utf8SequenceLength(unsigned char lead) {
    if (lead < 0x80)
        return 1;
    if ((lead & 0xE0) == 0xC0)
        return 2;
    if ((lead & 0xF0) == 0xE0)
        return 3;
    if ((lead & 0xF8) == 0xF0)
        return 4;
    return 1;  // 非法/延续字节，当作单字节处理
}

// 提取 text 中第 line 行（0-based，不含换行符）；越界返回空 view。
inline std::string_view LineTextAt(std::string_view text, uint32_t line) {
    uint32_t         cur = 0;
    std::string_view rem = text;
    while (true) {
        auto             pos = rem.find('\n');
        std::string_view row = (pos == std::string_view::npos) ? rem : rem.substr(0, pos);
        if (!row.empty() && row.back() == '\r')
            row.remove_suffix(1);
        if (cur == line)
            return row;
        if (pos == std::string_view::npos)
            return {};  // 行号越界
        rem.remove_prefix(pos + 1);
        ++cur;
    }
}

// 将一行内的 UTF-8 字节偏移转换为 UTF-16 code unit 偏移。
inline uint32_t Utf8ByteToUtf16(std::string_view line, uint32_t byteOffset) {
    uint32_t byte = 0, utf16 = 0;
    while (byte < byteOffset && byte < line.size()) {
        uint32_t len = Utf8SequenceLength(static_cast<unsigned char>(line[byte]));
        if (byte + len > line.size())
            len = 1;
        uint32_t cp = static_cast<unsigned char>(line[byte]);
        if (len > 1) {
            cp = static_cast<unsigned char>(line[byte]) & (0xFFu >> (len + 1));
            for (uint32_t k = 1; k < len; ++k)
                cp = (cp << 6) | (static_cast<unsigned char>(line[byte + k]) & 0x3Fu);
        }
        utf16 += (cp > 0xFFFF) ? 2 : 1;  // 超出 BMP 的码点在 UTF-16 中占一个代理对（2 个 code unit）
        byte += len;
    }
    return utf16;
}

// 将一行内的 UTF-16 code unit 偏移转换为 UTF-8 字节偏移。
inline uint32_t Utf16ToUtf8Byte(std::string_view line, uint32_t utf16Offset) {
    uint32_t byte = 0, utf16 = 0;
    while (utf16 < utf16Offset && byte < line.size()) {
        uint32_t len = Utf8SequenceLength(static_cast<unsigned char>(line[byte]));
        if (byte + len > line.size())
            len = 1;
        uint32_t cp = static_cast<unsigned char>(line[byte]);
        if (len > 1) {
            cp = static_cast<unsigned char>(line[byte]) & (0xFFu >> (len + 1));
            for (uint32_t k = 1; k < len; ++k)
                cp = (cp << 6) | (static_cast<unsigned char>(line[byte + k]) & 0x3Fu);
        }
        utf16 += (cp > 0xFFFF) ? 2 : 1;
        byte += len;
    }
    return byte;
}

}  // namespace infra::common
