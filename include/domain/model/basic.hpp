#pragma once

#include <any>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace domain::model {
struct Protocol {
    Protocol()          = default;
    virtual ~Protocol() = default;

    Protocol(Protocol &&) = default;

    virtual auto Encode() -> nlohmann::json {
        return nlohmann::json::object();
    }

    virtual void Decode(const nlohmann::json &json) {}
};

struct Position : public Protocol {
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

enum class DiagnosticSeverity { Error = 1, Warning = 2, Information = 3, Hint = 4 };

enum class DiagnosticTag { Unnecessary = 1, Deprecated = 2 };

struct DiagnosticRelatedInformation : public Protocol {
    Location    location;
    std::string message;
};

struct Diagnostic : public Protocol {
    Range                      range;
    DiagnosticSeverity         severity;
    std::string                code;
    std::string                codeDescription;
    std::string                source;
    std::string                message;
    std::vector<DiagnosticTag> tags;
    std::string                data;
};

struct TextEdit : public Protocol {
    Range       range;
    std::string newText;
};

struct Command : public Protocol {
    std::string           title;
    std::string           command;
    std::vector<std::any> arguments;
};

struct DocumentFilter : public Protocol {
    std::string language;  // A language id, like `typescript`.
    std::string scheme;    // A Uri scheme, like `file` or `untitled`.
    std::string pattern;   // A glob pattern, like `*.{ts,js}`.
};

struct TextDocumentIdentifier : public Protocol {
    std::string uri;
};

struct VersionedTextDocumentIdentifier : public TextDocumentIdentifier {
    std::int64_t version{0};
};

struct TextDocumentContentChangeEvent : public Protocol {
    Range         range;
    std::uint64_t rangeLength;
    std::string   text;
};
}  // namespace domain::model