#pragma once

#include "protocol/lsp/basic.hpp"

#include <string>
#include <vector>

namespace lsp {
enum class TextDocumentSyncKind : int { None = 0, Full = 1, Incremental = 2 };

struct TextDocumentSyncOptions : public Protocol {
    auto Encode() -> nlohmann::json override {
        auto output         = nlohmann::json::object();
        output["openClose"] = openClose;
        output["change"]    = static_cast<int>(change);
        if (save)
            output["save"] = nlohmann::json{{"includeText", saveIncludeText}};
        return output;
    }

    bool                 openClose{false};
    TextDocumentSyncKind change{TextDocumentSyncKind::None};
    bool                 save{false};            // 是否发送 save 通知
    bool                 saveIncludeText{false}; // save 通知中是否附带文本
};

struct DidOpenTextDocumentParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        textDocument.Decode(input["textDocument"].template get<nlohmann::json>());
    }

    auto Encode() -> nlohmann::json override {
        auto output            = nlohmann::json::object();
        output["textDocument"] = textDocument.Encode();
        return output;
    }

    TextDocumentItem textDocument;
};

struct DidCloseTextDocumentParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        textDocument.Decode(input["textDocument"].template get<nlohmann::json>());
    }

    auto Encode() -> nlohmann::json override {
        auto output            = nlohmann::json::object();
        output["textDocument"] = textDocument.Encode();
        return output;
    }

    TextDocumentIdentifier textDocument;
};

struct NotebookDocumentSyncOptions : public Protocol {
    bool save;
};

struct TextDocumentRegistrationOptions : public Protocol {
    std::vector<DocumentFilter> documentSelector;
};

struct TextDocumentChangeRegistrationOptions : public TextDocumentRegistrationOptions {
    TextDocumentSyncKind syncKind{TextDocumentSyncKind::None};
};

struct DidChangeTextDocumentParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        textDocument.Decode(input["textDocument"].template get<nlohmann::json>());
        for (const auto &change : input["contentChanges"]) {
            TextDocumentContentChangeEvent ev;
            ev.Decode(change);
            contentChanges.push_back(std::move(ev));
        }
    }

    VersionedTextDocumentIdentifier             textDocument;
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};

// textDocument/didSave 的入参
// text 字段仅在 SaveOptions::includeText == true 时由客户端发送
struct DidSaveTextDocumentParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        textDocument.Decode(input["textDocument"].template get<nlohmann::json>());
        if (input.contains("text") && input["text"].is_string()) {
            text = input["text"].template get<std::string>();
        }
    }

    TextDocumentIdentifier textDocument;
    std::string            text;  // 可选
};

enum class TextDocumentSaveReason { Manual = 1, AfterDelay = 2, FocusOut = 3 };

struct WillSaveTextDocumentParams : public Protocol {
    TextDocumentIdentifier textDocument;
    TextDocumentSaveReason reason;
};

struct SaveOptions : public Protocol {
    void Decode(const nlohmann::json &input) override {
        includeText = input["includeText"].template get<bool>();
    }

    auto Encode() -> nlohmann::json override {
        auto output           = nlohmann::json::object();
        output["includeText"] = includeText;
        return output;
    }

    bool includeText{false};
};

struct RenameParams : public TextDocumentPositionParams, WorkDoneProgressParams {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
        if (input.contains("newName") && input["newName"].is_string())
            newName = input["newName"].template get<std::string>();
    }

    std::string newName;
};
}  // namespace lsp