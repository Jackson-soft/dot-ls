#pragma once

#include "infra/common/text_encoding.hpp"
#include "protocol/lsp/document.hpp"

#include <algorithm>
#include <cstdint>
#include <print>
#include <string>
#include <unordered_map>

// 文本相关
namespace domain::entity {

// 内存中的文档快照
struct DocumentEntry {
    std::string  uri;
    std::string  languageId;
    std::int64_t version{0};
    std::string  text;
};

class Document {
public:
    Document()  = default;
    ~Document() = default;

    // 打开文档：加入存储（已打开则警告并覆盖）
    auto DidOpen(const lsp::DidOpenTextDocumentParams &params) -> lsp::Protocol {
        const auto &doc = params.textDocument;
        if (store_.contains(doc.uri)) {
            std::print(stderr, "[dot-ls] warn: didOpen for already-open document {}\n", doc.uri);
        }
        store_[doc.uri] = DocumentEntry{
            .uri        = doc.uri,
            .languageId = doc.languageId,
            .version    = doc.version,
            .text       = doc.text,
        };
        return {};
    }

    // 内容变更：支持 Incremental（携带 range，原地拼接）与 Full（整篇替换）两种同步模式，
    // 与客户端实际发送的 contentChanges 形态一致，而不再强制假设整篇替换。
    auto DidChange(const lsp::DidChangeTextDocumentParams &params) -> lsp::Protocol {
        if (const auto it = store_.find(params.textDocument.uri); it != store_.end()) {
            // 版本号必须单调递增
            if (params.textDocument.version <= it->second.version) {
                std::print(stderr,
                           "[dot-ls] warn: didChange version {} <= current {} for {}\n",
                           params.textDocument.version,
                           it->second.version,
                           params.textDocument.uri);
                return {};
            }
            it->second.version = params.textDocument.version;
            // 数组中的多个 change 需要按顺序依次应用：每个 change 的 range 都是相对于
            // "应用了前一个 change 之后" 的文档状态而言的。
            for (const auto &change : params.contentChanges) {
                if (change.hasRange) {
                    applyIncrementalChange(it->second.text, change.range, change.text);
                } else {
                    it->second.text = change.text;  // Full 同步：整篇替换
                }
            }
        }
        return {};
    }

    // 保存文档：若客户端附带了文本则同步更新
    auto DidSave(const lsp::DidSaveTextDocumentParams &params) -> lsp::Protocol {
        if (!params.text.empty()) {
            if (const auto it = store_.find(params.textDocument.uri); it != store_.end()) {
                it->second.text = params.text;
            }
        }
        return {};
    }

    // 关闭文档：从存储中移除
    auto DidClose(const lsp::DidCloseTextDocumentParams &params) -> lsp::Protocol {
        store_.erase(params.textDocument.uri);
        return {};
    }

    auto Rename(const lsp::RenameParams & /*params*/) -> lsp::Protocol {
        return {};
    }

    // 供其他服务查询单个文档
    [[nodiscard]] auto Get(const std::string &uri) const -> const DocumentEntry * {
        const auto it = store_.find(uri);
        return it != store_.end() ? &it->second : nullptr;
    }

    // 遍历所有打开的文档（供 workspace/symbol 等功能使用）
    [[nodiscard]] auto GetAllEntries() const -> const std::unordered_map<std::string, DocumentEntry> & {
        return store_;
    }

private:
    std::unordered_map<std::string, DocumentEntry> store_;

    // 将 (line, utf16Character) 位置转换为文本中的绝对字节偏移
    static std::size_t absoluteByteOffset(const std::string &text, const lsp::Position &pos) {
        std::size_t byte = 0;
        std::uint64_t line = 0;
        while (line < pos.line) {
            const auto nl = text.find('\n', byte);
            if (nl == std::string::npos) {
                return text.size();  // 行号越界，钳制到文末
            }
            byte = nl + 1;
            ++line;
        }
        const auto row = infra::common::LineTextAt(text, static_cast<uint32_t>(pos.line));
        return byte + infra::common::Utf16ToUtf8Byte(row, static_cast<uint32_t>(pos.character));
    }

    // 增量编辑：用 newText 替换 [range.start, range.end) 区间（UTF-16 偏移需先转换为字节偏移）
    static void applyIncrementalChange(std::string &text, const lsp::Range &range, const std::string &newText) {
        const auto startByte = absoluteByteOffset(text, range.start);
        const auto endByte   = std::max(startByte, absoluteByteOffset(text, range.end));
        text.replace(startByte, endByte - startByte, newText);
    }
};

}  // namespace domain::entity
