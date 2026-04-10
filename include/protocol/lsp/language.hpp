#pragma once

#include "protocol/lsp/basic.hpp"

#include <any>
#include <optional>
#include <string>
#include <vector>

namespace lsp {
struct CompletionContext : public Protocol {
    CompletionTriggerKind triggerKind;
    std::string           triggerCharacter;
};

// 补全的入参
struct CompletionParams : public TextDocumentPositionParams, WorkDoneProgressParams, PartialResultParams {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
    }

    CompletionContext context;
};

struct ItemDefault : public Protocol {
    std::vector<std::string> commitCharacters;
    Range                    editRange;
    InsertTextFormat         insertTextFormat;
    InsertTextMode           insertTextMode;
    std::optional<std::any>  data;
};

struct CompletionList : public Protocol {
    auto Encode() -> nlohmann::json override {
        auto output            = nlohmann::json::object();
        output["isIncomplete"] = isIncomplete;
        auto arr               = nlohmann::json::array();
        for (auto &item : items) {
            arr.push_back(item.Encode());
        }
        output["items"] = arr;
        return output;
    }

    bool                        isIncomplete{false};
    std::optional<ItemDefault>  itemDefaults;
    std::vector<CompletionItem> items;
};

// ── Hover ────────────────────────────────────────────────────────────────────

struct MarkupContent : public Protocol {
    auto Encode() -> nlohmann::json override {
        return nlohmann::json{{"kind", kind}, {"value", value}};
    }

    std::string kind{"markdown"};
    std::string value;
};

struct HoverParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
    }

    TextDocumentIdentifier textDocument;
    Position               position;
};

struct Hover : public Protocol {
    auto Encode() -> nlohmann::json override {
        if (contents.value.empty())
            return nullptr;
        return nlohmann::json{{"contents", contents.Encode()}};
    }

    MarkupContent contents;
};

// ── Document Highlight ───────────────────────────────────────────────────────

enum class DocumentHighlightKind { Text = 1, Read = 2, Write = 3 };

struct DocumentHighlight : public Protocol {
    auto Encode() -> nlohmann::json override {
        return nlohmann::json{{"range", range.Encode()}, {"kind", static_cast<int>(kind)}};
    }

    Range                 range;
    DocumentHighlightKind kind{DocumentHighlightKind::Text};
};

struct DocumentHighlightParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
    }

    TextDocumentIdentifier textDocument;
    Position               position;
};

// ── Semantic Tokens ──────────────────────────────────────────────────────────

struct SemanticTokensParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
    }

    TextDocumentIdentifier textDocument;
};

struct SemanticTokens : public Protocol {
    auto Encode() -> nlohmann::json override {
        return nlohmann::json{{"data", data}};
    }

    std::vector<uint32_t> data;
};

// ── Definition / Declaration ─────────────────────────────────────────────────

struct DefinitionParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
    }

    TextDocumentIdentifier textDocument;
    Position               position;
};

}  // namespace lsp
