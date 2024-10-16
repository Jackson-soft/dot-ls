#pragma once

#include "domain/model/basic.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace domain::model {
enum class TextDocumentSyncKind { None = 0, Full = 1, Incremental = 2 };

struct TextDocumentSyncOptions : public Protocol {
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
    VersionedTextDocumentIdentifier             textDocument;
    std::vector<TextDocumentContentChangeEvent> contentChanges;
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
}  // namespace domain::model