#pragma once

#include <any>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace domain::model {
struct Protocol {
    Protocol()          = default;
    virtual ~Protocol() = default;

    Protocol(Protocol &&) = default;

    virtual auto Encode() -> nlohmann::json {
        return nlohmann::json::object();
    }

    virtual void Decode(const nlohmann::json &input) {}
};

struct RegularExpressionsClientCapabilities : public Protocol {
    std::string                engine;
    std::optional<std::string> version;
};

const std::vector<std::string> EOL{"\n", "\r\n", "\r"};

struct Position : public Protocol {
    Position() = default;

    Position(std::uint64_t line, std::uint64_t character) : line(line), character(character) {}

    auto Encode() -> nlohmann::json override {
        auto object         = nlohmann::json::object();
        object["line"]      = line;
        object["character"] = character;

        return object;
    }

    void Decode(const nlohmann::json &json) override {
        line      = json["line"].template get<std::uint64_t>();
        character = json["character"].template get<std::uint64_t>();
    }

    std::uint64_t line{0};
    std::uint64_t character{0};
};

struct Range : public Protocol {
    auto Encode() -> nlohmann::json override {
        auto object     = nlohmann::json::object();
        object["start"] = start.Encode();
        object["end"]   = end.Encode();

        return object;
    }

    void Decode(const nlohmann::json &json) override {
        start.Decode(json["start"]);
        end.Decode(json["end"]);
    }

    Position start;
    Position end;
};

struct TextDocumentItem : public Protocol {
    void Decode(const nlohmann::json &input) override {
        uri        = input["uri"].template get<std::string>();
        languageId = input["languageId"];
        version    = input["version"];
        text       = input["text"];
    }

    auto Encode() -> nlohmann::json override {
        auto output          = nlohmann::json::object();
        output["uri"]        = uri;
        output["languageId"] = languageId;
        output["version"]    = version;
        output["text"]       = text;

        return output;
    }

    std::string  uri;
    std::string  languageId;  // dot
    std::int64_t version;
    std::string  text;
};

struct TextDocumentIdentifier : public Protocol {
    void Decode(const nlohmann::json &input) override {
        uri = input["uri"].template get<std::string>();
    }

    auto Encode() -> nlohmann::json override {
        auto output   = nlohmann::json::object();
        output["uri"] = uri;

        return output;
    }

    std::string uri;
};

struct VersionedTextDocumentIdentifier : public TextDocumentIdentifier {
    std::int64_t version{0};
};

struct TextDocumentPositionParams : public Protocol {
    TextDocumentIdentifier textDocument;
    Position               position;
};

struct DocumentFilter : public Protocol {
    std::string language;  // A language id, like `typescript`.
    std::string scheme;    // A Uri scheme, like `file` or `untitled`.
    std::string pattern;   // A glob pattern, like `*.{ts,js}`.
};

struct TextEdit : public Protocol {
    Range       range;
    std::string newText;
};

struct Location : public Protocol {
    auto Encode() -> nlohmann::json override {
        auto object     = nlohmann::json::object();
        object["range"] = range.Encode();
        object["uri"]   = uri;

        return object;
    }

    void Decode(const nlohmann::json &json) override {
        range.Decode(json["range"]);
        uri = json["uri"];
    }

    std::string uri;
    Range       range;
};

struct CodeDescription : public Protocol {
    std::string href;
};

enum class DiagnosticSeverity { Error = 1, Warning = 2, Information = 3, Hint = 4 };

enum class DiagnosticTag { Unnecessary = 1, Deprecated = 2 };

struct DiagnosticRelatedInformation : public Protocol {
    Location    location;
    std::string message;
};

struct Diagnostic : public Protocol {
    Range                                     range;
    DiagnosticSeverity                        severity;
    std::string                               code;
    CodeDescription                           codeDescription;
    std::string                               source;
    std::string                               message;
    std::vector<DiagnosticTag>                tags;
    std::vector<DiagnosticRelatedInformation> relatedInformation;
    std::string                               data;
};

struct Command : public Protocol {
    std::string           title;
    std::string           command;
    std::vector<std::any> arguments;
};

struct TextDocumentContentChangeEvent : public Protocol {
    Range         range;
    std::uint64_t rangeLength;
    std::string   text;
};

struct WorkDoneProgressOptions : public Protocol {
    std::optional<bool> workDoneProgress;
};

using ProgressToken = std::variant<int, std::string>;

struct WorkDoneProgressParams : public Protocol {
    void Decode(const nlohmann::json &input) override {
        if (input.contains("workDoneToken")) {
            if (input["workDoneToken"].is_number_integer()) {
                workDoneToken = input["workDoneToken"].template get<int>();
            } else if (input["workDoneToken"].is_string()) {
                workDoneToken = input["workDoneToken"].template get<std::string>();
            }
        }
    }

    auto Encode() -> nlohmann::json override {
        nlohmann::json output;

        if (workDoneToken.index() == 0) {
            output["workDoneToken"] = std::get<int>(workDoneToken);
        } else {
            output["workDoneToken"] = std::get<std::string>(workDoneToken);
        }

        return output;
    }

    ProgressToken workDoneToken;
};

struct PartialResultParams : public Protocol {
    std::string partialResultToken;
};

enum class CompletionItemKind {
    Text          = 1,
    Method        = 2,
    Function      = 3,
    Constructor   = 4,
    Field         = 5,
    Variable      = 6,
    Class         = 7,
    Interface     = 8,
    Module        = 9,
    Property      = 10,
    Unit          = 11,
    Value         = 12,
    Enum          = 13,
    Keyword       = 14,
    Snippet       = 15,
    Color         = 16,
    File          = 17,
    Reference     = 18,
    Folder        = 19,
    EnumMember    = 20,
    Constant      = 21,
    Struct        = 22,
    Event         = 23,
    Operator      = 24,
    TypeParameter = 25
};

struct CompletionItemLabelDetails : public Protocol {
    std::string                detail;
    std::optional<std::string> description;
};

enum class CompletionItemTag { Deprecated = 1 };

enum class InsertTextFormat { PlainText = 1, Snippet = 2 };

enum class InsertTextMode { asIs = 1, adjustIndentation = 2 };

enum class CompletionTriggerKind { Invoked = 1, TriggerCharacter = 2, TriggerForIncompleteCompletions = 3 };

struct CompletionItem : public Protocol {
    std::string                               label;
    std::optional<CompletionItemLabelDetails> labelDetails;
    CompletionItemKind                        kind;
    std::vector<CompletionItemTag>            tags;
    std::string                               detail;
    std::string                               documentation;
    bool                                      deprecated;
    bool                                      preselect;
    std::optional<std::string>                sortText;
    std::optional<std::string>                filterText;
    std::string                               insertText;
    InsertTextFormat                          insertTextFormat;
    InsertTextMode                            insertTextMode;
    TextEdit                                  textEdit;
    std::optional<std::string>                textEditText;
    std::vector<TextEdit>                     additionalTextEdits;
    std::vector<std::string>                  commitCharacters;
    std::optional<Command>                    command;
    std::optional<std::any>                   data;
};

struct CompletionOptions : public WorkDoneProgressOptions {
    std::vector<std::string> triggerCharacters;
    std::vector<std::string> allCommitCharacters;
    bool                     resolveProvider;
    CompletionItem           completionItem;
};

}  // namespace domain::model