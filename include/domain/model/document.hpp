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

struct TextDocumentItem : public Protocol {
    std::string  uri;
    std::string  languageId;
    std::int64_t version;
    std::string  text;
};

struct DidOpenTextDocumentParams : public Protocol {
    TextDocumentItem textDocument;
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
}  // namespace domain::model