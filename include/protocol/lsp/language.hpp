#pragma once

#include "protocol/lsp/basic.hpp"

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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

// ── References ───────────────────────────────────────────────────────────────

struct ReferenceContext : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("includeDeclaration") && input["includeDeclaration"].is_boolean())
            includeDeclaration = input["includeDeclaration"].template get<bool>();
    }

    bool includeDeclaration{false};
};

struct ReferenceParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
        if (input.contains("context"))
            context.Decode(input["context"]);
    }

    TextDocumentIdentifier textDocument;
    Position               position;
    ReferenceContext       context;
};

// ── Document Symbol ──────────────────────────────────────────────────────────

enum class SymbolKind {
    File          = 1,
    Module        = 2,
    Namespace     = 3,
    Package       = 4,
    Class         = 5,
    Method        = 6,
    Property      = 7,
    Field         = 8,
    Constructor   = 9,
    Enum          = 10,
    Interface     = 11,
    Function      = 12,
    Variable      = 13,
    Constant      = 14,
    String        = 15,
    Number        = 16,
    Boolean       = 17,
    Array         = 18,
    Object        = 19,
    Key           = 20,
    Null          = 21,
    EnumMember    = 22,
    Struct        = 23,
    Event         = 24,
    Operator      = 25,
    TypeParameter = 26,
};

struct DocumentSymbol : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["name"]            = name;
        j["kind"]            = static_cast<int>(kind);
        j["range"]           = range.Encode();
        j["selectionRange"]  = selectionRange.Encode();
        if (!detail.empty())
            j["detail"] = detail;
        if (!children.empty()) {
            auto arr = nlohmann::json::array();
            for (auto &c : children)
                arr.push_back(c.Encode());
            j["children"] = arr;
        }
        return j;
    }

    std::string                  name;
    std::string                  detail;
    SymbolKind                   kind{SymbolKind::Variable};
    Range                        range;
    Range                        selectionRange;
    std::vector<DocumentSymbol>  children;
};

struct DocumentSymbolParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
    }

    TextDocumentIdentifier textDocument;
};

// ── Rename ───────────────────────────────────────────────────────────────────

struct WorkspaceEdit : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        nlohmann::json changesObj = nlohmann::json::object();
        for (auto &[uri, edits] : changes) {
            auto arr = nlohmann::json::array();
            for (auto &e : edits)
                arr.push_back(e.Encode());
            changesObj[uri] = arr;
        }
        j["changes"] = changesObj;
        return j;
    }

    std::unordered_map<std::string, std::vector<TextEdit>> changes;
};

struct PrepareRenameParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
    }

    TextDocumentIdentifier textDocument;
    Position               position;
};

// ── Folding Range ─────────────────────────────────────────────────────────────

enum class FoldingRangeKind { Region, Comment };

struct FoldingRange : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["startLine"] = startLine;
        j["endLine"]   = endLine;
        j["kind"]      = (kind == FoldingRangeKind::Comment) ? "comment" : "region";
        return j;
    }

    uint32_t        startLine{0};
    uint32_t        endLine{0};
    FoldingRangeKind kind{FoldingRangeKind::Region};
};

struct FoldingRangeParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
    }

    TextDocumentIdentifier textDocument;
};

// ── Selection Range ───────────────────────────────────────────────────────────

struct SelectionRange : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["range"] = range.Encode();
        if (parent)
            j["parent"] = parent->Encode();
        return j;
    }

    Range                        range;
    std::shared_ptr<SelectionRange> parent;
};

struct SelectionRangeParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("positions") && input["positions"].is_array()) {
            for (const auto &p : input["positions"]) {
                Position pos;
                pos.Decode(p);
                positions.push_back(pos);
            }
        }
    }

    TextDocumentIdentifier textDocument;
    std::vector<Position>  positions;
};

// ── Semantic Tokens (range) ──────────────────────────────────────────────────

struct SemanticTokensRangeParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("range"))
            range.Decode(input["range"]);
    }

    TextDocumentIdentifier textDocument;
    Range                  range;
};

// ── Code Action ───────────────────────────────────────────────────────────────

struct CodeActionContext : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("only") && input["only"].is_array()) {
            for (const auto &k : input["only"])
                if (k.is_string())
                    only.push_back(k.template get<std::string>());
        }
    }

    std::vector<std::string> only;  // requested CodeActionKind filter
};

struct CodeActionParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("range"))
            range.Decode(input["range"]);
        if (input.contains("context"))
            context.Decode(input["context"]);
    }

    TextDocumentIdentifier textDocument;
    Range                  range;
    CodeActionContext       context;
};

// ── Workspace Symbol ──────────────────────────────────────────────────────────

struct WorkspaceSymbolParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("query") && input["query"].is_string())
            query = input["query"].template get<std::string>();
    }

    std::string query;
};

struct SymbolInformation : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["name"]     = name;
        j["kind"]     = static_cast<int>(kind);
        j["location"] = nlohmann::json{{"uri", location.uri}, {"range", location.range.Encode()}};
        if (!containerName.empty())
            j["containerName"] = containerName;
        return j;
    }

    std::string name;
    SymbolKind  kind{SymbolKind::Variable};
    Location    location;
    std::string containerName;
};

// ── Formatting ───────────────────────────────────────────────────────────────

struct FormattingOptions : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("tabSize") && input["tabSize"].is_number_integer())
            tabSize = input["tabSize"].template get<uint32_t>();
        if (input.contains("insertSpaces") && input["insertSpaces"].is_boolean())
            insertSpaces = input["insertSpaces"].template get<bool>();
        if (input.contains("trimTrailingWhitespace") && input["trimTrailingWhitespace"].is_boolean())
            trimTrailingWhitespace = input["trimTrailingWhitespace"].template get<bool>();
        if (input.contains("insertFinalNewline") && input["insertFinalNewline"].is_boolean())
            insertFinalNewline = input["insertFinalNewline"].template get<bool>();
        if (input.contains("trimFinalNewlines") && input["trimFinalNewlines"].is_boolean())
            trimFinalNewlines = input["trimFinalNewlines"].template get<bool>();
    }

    uint32_t tabSize{4};
    bool     insertSpaces{true};
    bool     trimTrailingWhitespace{true};
    bool     insertFinalNewline{true};
    bool     trimFinalNewlines{true};
};

struct DocumentFormattingParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("options"))
            options.Decode(input["options"]);
    }

    TextDocumentIdentifier textDocument;
    FormattingOptions      options;
};

struct DocumentRangeFormattingParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("range"))
            range.Decode(input["range"]);
        if (input.contains("options"))
            options.Decode(input["options"]);
    }

    TextDocumentIdentifier textDocument;
    Range                  range;
    FormattingOptions      options;
};

// ── InlayHint ─────────────────────────────────────────────────────────────────

enum class InlayHintKind { Type = 1, Parameter = 2 };

struct InlayHint : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["position"]     = position.Encode();
        j["label"]        = label;
        j["kind"]         = static_cast<int>(kind);
        j["paddingLeft"]  = paddingLeft;
        j["paddingRight"] = paddingRight;
        return j;
    }

    Position      position;
    std::string   label;
    InlayHintKind kind{InlayHintKind::Parameter};
    bool          paddingLeft{false};
    bool          paddingRight{false};
};

struct InlayHintParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("range"))
            range.Decode(input["range"]);
    }

    TextDocumentIdentifier textDocument;
    Range                  range;
};

// ── CodeLens ──────────────────────────────────────────────────────────────────

struct CodeLensCommand : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["title"]   = title;
        j["command"] = command;
        if (!arguments.empty())
            j["arguments"] = arguments;
        return j;
    }

    std::string                 title;
    std::string                 command;
    std::vector<nlohmann::json> arguments;
};

struct CodeLens : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["range"] = range.Encode();
        if (!command.title.empty())
            j["command"] = command.Encode();
        return j;
    }

    Range          range;
    CodeLensCommand command;
};

struct CodeLensParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
    }

    TextDocumentIdentifier textDocument;
};

// ── DocumentLink ──────────────────────────────────────────────────────────────

struct DocumentLink : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json j;
        j["range"] = range.Encode();
        if (!target.empty())  j["target"]  = target;
        if (!tooltip.empty()) j["tooltip"] = tooltip;
        return j;
    }

    Range       range;
    std::string target;
    std::string tooltip;
};

struct DocumentLinkParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
    }

    TextDocumentIdentifier textDocument;
};

// ── OnTypeFormatting ──────────────────────────────────────────────────────────

struct DocumentOnTypeFormattingParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("textDocument"))
            textDocument.Decode(input["textDocument"]);
        if (input.contains("position"))
            position.Decode(input["position"]);
        if (input.contains("ch") && input["ch"].is_string())
            ch = input["ch"].template get<std::string>();
        if (input.contains("options"))
            options.Decode(input["options"]);
    }

    TextDocumentIdentifier textDocument;
    Position               position;
    std::string            ch;
    FormattingOptions      options;
};

// ── ExecuteCommand ────────────────────────────────────────────────────────────

struct ExecuteCommandParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("command") && input["command"].is_string())
            command = input["command"].template get<std::string>();
        if (input.contains("arguments") && input["arguments"].is_array()) {
            for (const auto &arg : input["arguments"])
                arguments.push_back(arg);
        }
    }

    std::string                 command;
    std::vector<nlohmann::json> arguments;
};

}  // namespace lsp
