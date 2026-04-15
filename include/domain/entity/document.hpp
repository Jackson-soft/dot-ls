#pragma once

#include "protocol/lsp/document.hpp"

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

    // 内容变更：Full 同步模式下用第一个 contentChange 替换全文
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
            if (!params.contentChanges.empty()) {
                it->second.text = params.contentChanges.front().text;
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

    auto Rename(const lsp::RenameParams &params) -> lsp::Protocol { return {}; }

    // 供其他服务查询单个文档
    [[nodiscard]] auto Get(const std::string &uri) const -> const DocumentEntry * {
        const auto it = store_.find(uri);
        return it != store_.end() ? &it->second : nullptr;
    }

    // 遍历所有打开的文档（供 workspace/symbol 等功能使用）
    [[nodiscard]] auto GetAllEntries() const
        -> const std::unordered_map<std::string, DocumentEntry> & {
        return store_;
    }

private:
    std::unordered_map<std::string, DocumentEntry> store_;
};

}  // namespace domain::entity
