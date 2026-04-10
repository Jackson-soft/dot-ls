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
        return output;
    }

    bool                 openClose = false;
    TextDocumentSyncKind change    = TextDocumentSyncKind::None;
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
    std::string newName;
};
}  // namespace lsp