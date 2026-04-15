#pragma once

// lsp lifecycle 相关的数据结构

#include "protocol/lsp/basic.hpp"
#include "protocol/lsp/document.hpp"

#include <any>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lsp {
struct SignatureHelpOptions : public WorkDoneProgressOptions {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;
        json["triggerCharacters"]   = triggerCharacters;
        json["retriggerCharacters"] = retriggerCharacters;
        return json;
    }

    std::vector<std::string> triggerCharacters;
    std::vector<std::string> retriggerCharacters;
};

struct DocumentOnTypeFormattingOptions : public Protocol {
    std::string              firstTriggerCharacter;
    std::vector<std::string> moreTriggerCharacter;
};

struct ExecuteCommandOptions : public Protocol {
    std::vector<std::string> commands;
};

struct SemanticTokensLegend : public Protocol {
    auto Encode() -> nlohmann::json override {
        return nlohmann::json{{"tokenTypes", tokenTypes}, {"tokenModifiers", tokenModifiers}};
    }

    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;
};

struct SemanticTokensOptions : public WorkDoneProgressOptions {
    auto Encode() -> nlohmann::json override {
        return nlohmann::json{{"legend", legend.Encode()}, {"full", full}, {"range", range}};
    }

    SemanticTokensLegend legend;
    bool                 range = false;
    bool                 full  = false;
};

struct DiagnosticOptions : public WorkDoneProgressOptions {
    std::string identifier;
    bool        interFileDependencies = false;
    bool        workspaceDiagnostics  = false;
};

struct FileOperationPatternOptions : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        json["ignore"] = ignoreCase;

        return json;
    }

    bool ignoreCase{false};
};

struct FileOperationPattern : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        json["glob"]    = glob;
        json["matches"] = matches;
        json["options"] = options.Encode();

        return json;
    }

    /**
     * The glob pattern to match. Glob patterns can have the following syntax:
     * - `*` to match zero or more characters in a path segment
     * - `?` to match on one character in a path segment
     * - `**` to match any number of path segments, including none
     * - `{}` to group sub patterns into an OR expression.
     * - `[]` to declare a range of characters to match in a path segment
     *   (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
     * - `[!...]` to negate a range of characters to match in a path segment
     *   (e.g., `example.[!0-9]` to match on `example.a`, `example.b`, but
     *   not `example.0`)
     */
    std::string                 glob;
    std::string                 matches{"file"};  // 'file' | 'folder'
    FileOperationPatternOptions options;
};

struct FileOperationFilter : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        json["pattern"] = pattern.Encode();
        json["scheme"]  = scheme;

        return json;
    }

    std::string          scheme;
    FileOperationPattern pattern;
};

struct FileOperationRegistrationOptions : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        auto output = nlohmann::json::array();

        for (auto &filter : filters) {
            output.push_back(filter.Encode());
        }

        json["filters"] = output;
        return json;
    }

    std::vector<FileOperationFilter> filters;
};

struct FileOperations : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        json["didCreate"]  = didCreate.Encode();
        json["willCreate"] = willCreate.Encode();
        json["didRename"]  = didRename.Encode();
        json["willRename"] = willRename.Encode();
        json["didDelete"]  = didDelete.Encode();
        json["willDelete"] = willDelete.Encode();

        return json;
    }

    FileOperationRegistrationOptions didCreate;
    FileOperationRegistrationOptions willCreate;
    FileOperationRegistrationOptions didRename;
    FileOperationRegistrationOptions willRename;
    FileOperationRegistrationOptions didDelete;
    FileOperationRegistrationOptions willDelete;
};

struct WorkspaceFoldersServerCapabilities : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        json["supported"]           = supported;
        json["changeNotifications"] = changeNotifications;

        return json;
    }

    bool supported           = false;
    bool changeNotifications = false;
};

struct Workspace : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        json["workspaceFolders"] = workspaceFolders.Encode();
        json["fileOperations"]   = fileOperations.Encode();

        return json;
    }

    WorkspaceFoldersServerCapabilities workspaceFolders;
    FileOperations                     fileOperations;
};

struct ServerCapabilities : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;
        json["positionEncoding"] = positionEncoding;
        json["textDocumentSync"] = textDocumentSync.Encode();

        json["completionProvider"] = completionProvider.Encode();

        if (hoverProvider)             json["hoverProvider"]             = true;
        if (declarationProvider)       json["declarationProvider"]       = true;
        if (definitionProvider)        json["definitionProvider"]        = true;
        if (typeDefinitionProvider)    json["typeDefinitionProvider"]    = true;
        if (implementationProvider)    json["implementationProvider"]    = true;
        if (documentHighlightProvider) json["documentHighlightProvider"] = true;
        if (referencesProvider)        json["referencesProvider"]        = true;
        if (documentFormattingProvider)      json["documentFormattingProvider"]      = true;
        if (documentRangeFormattingProvider) json["documentRangeFormattingProvider"] = true;
        if (documentSymbolProvider)    json["documentSymbolProvider"]    = true;
        if (renameProvider)            json["renameProvider"]            = true;
        if (foldingRangeProvider)      json["foldingRangeProvider"]      = true;
        if (selectionRangeProvider)    json["selectionRangeProvider"]    = true;
        if (codeActionProvider)        json["codeActionProvider"]        = true;
        if (workspaceSymbolProvider)   json["workspaceSymbolProvider"]   = true;

        if (codeLensProvider)
            json["codeLensProvider"] = nlohmann::json{{"resolveProvider", false}};
        if (documentLinkProvider)
            json["documentLinkProvider"] = nlohmann::json{{"resolveProvider", false}};
        if (inlayHintProvider)
            json["inlayHintProvider"] = true;
        if (!documentOnTypeFormattingProvider.firstTriggerCharacter.empty()) {
            json["documentOnTypeFormattingProvider"] = nlohmann::json{
                {"firstTriggerCharacter", documentOnTypeFormattingProvider.firstTriggerCharacter},
                {"moreTriggerCharacter",  documentOnTypeFormattingProvider.moreTriggerCharacter},
            };
        }
        if (!executeCommandProvider.commands.empty())
            json["executeCommandProvider"] = nlohmann::json{{"commands", executeCommandProvider.commands}};

        if (!semanticTokensProvider.legend.tokenTypes.empty()) {
            json["semanticTokensProvider"] = semanticTokensProvider.Encode();
        }


        return json;
    }

    std::string                     positionEncoding{"utf-16"};  // utf-8, utf-16, utf-32
    TextDocumentSyncOptions         textDocumentSync;
    NotebookDocumentSyncOptions     notebookDocumentSync;
    CompletionOptions               completionProvider;
    bool                            hoverProvider = false;
    SignatureHelpOptions            signatureHelpProvider;
    bool                            declarationProvider             = false;
    bool                            definitionProvider              = false;
    bool                            typeDefinitionProvider          = false;
    bool                            implementationProvider          = false;
    bool                            referencesProvider              = false;
    bool                            documentHighlightProvider       = false;
    bool                            documentSymbolProvider          = false;
    bool                            codeActionProvider              = false;
    bool                            codeLensProvider                = false;
    bool                            documentLinkProvider            = false;
    bool                            colorProvider                   = false;
    bool                            documentFormattingProvider      = false;
    bool                            documentRangeFormattingProvider = false;
    DocumentOnTypeFormattingOptions documentOnTypeFormattingProvider;
    bool                            renameProvider       = false;
    bool                            foldingRangeProvider = false;
    ExecuteCommandOptions           executeCommandProvider;
    bool                            selectionRangeProvider     = false;
    bool                            linkedEditingRangeProvider = false;
    bool                            callHierarchyProvider      = false;
    SemanticTokensOptions           semanticTokensProvider;
    bool                            monikerProvider{false};
    bool                            typeHierarchyProvider = false;
    bool                            inlineValueProvider   = false;
    bool                            inlayHintProvider     = false;
    DiagnosticOptions               diagnosticProvider;
    bool                            workspaceSymbolProvider = false;
    Workspace                       workspace;
    std::any                        experimental;
};

struct ClientCapabilities : public Protocol {};

struct WorkspaceFolder : public Protocol {
    void Decode(const nlohmann::json &json) override {
        uri  = json["uri"].template get<std::string>();
        name = json["name"].template get<std::string>();
    }

    auto Encode() -> nlohmann::json override {
        nlohmann::json json;
        json["uri"]  = uri;
        json["name"] = name;

        return json;
    }

    std::string uri;
    std::string name;
};

struct BaseInfo : public Protocol {
    BaseInfo(std::string_view name = "", std::string_view version = "") : name(name), version(version) {}

    void Decode(const nlohmann::json &json) override {
        name    = json["name"].template get<std::string>();
        version = json["version"].template get<std::string>();
    }

    auto Encode() -> nlohmann::json override {
        nlohmann::json json;
        json["name"]    = name;
        json["version"] = version;

        return json;
    }

    std::string name;
    std::string version;
};

struct InitializationOptions : public Protocol {};

struct InitializeParams : public WorkDoneProgressParams {
    void Decode(const nlohmann::json &json) override {
        // processId 可为 null（无进程客户端）
        if (json.contains("processId") && !json["processId"].is_null()) {
            processId = json["processId"].template get<std::int64_t>();
        }

        // clientInfo 是可选字段
        if (json.contains("clientInfo") && !json["clientInfo"].is_null()) {
            clientInfo.Decode(json["clientInfo"]);
        }

        // locale 是可选字段
        if (json.contains("locale") && json["locale"].is_string()) {
            locale = json["locale"].template get<std::string>();
        }

        // rootPath 已废弃且可为 null
        if (json.contains("rootPath") && json["rootPath"].is_string()) {
            rootPath = json["rootPath"].template get<std::string>();
        }

        // rootUri 可为 null（LSP 3.17 已用 workspaceFolders 替代）
        if (json.contains("rootUri") && json["rootUri"].is_string()) {
            rootUri = json["rootUri"].template get<std::string>();
        }

        // capabilities 是必填项，但防御性检查
        if (json.contains("capabilities") && !json["capabilities"].is_null()) {
            capabilities.Decode(json["capabilities"]);
        }

        // trace 是可选字段
        if (json.contains("trace") && json["trace"].is_string()) {
            trace = json["trace"].template get<std::string>();
        }

        // workspaceFolders 是可选字段
        if (json.contains("workspaceFolders") && json["workspaceFolders"].is_array()) {
            for (const auto &wf : json["workspaceFolders"]) {
                WorkspaceFolder folder;
                folder.Decode(wf);
                workspaceFolders.push_back(std::move(folder));
            }
        }
    }

    auto Encode() -> nlohmann::json override {
        nlohmann::json json;

        json["processId"]  = processId;
        json["clientInfo"] = clientInfo.Encode();
        json["locale"]     = locale;
        json["rootPath"]   = rootPath;
        json["rootUri"]    = rootUri;

        return json;
    }

    std::int64_t                 processId;
    BaseInfo                     clientInfo;
    std::string                  locale{"zh"};
    std::string                  rootPath;
    std::string                  rootUri;
    InitializationOptions        initializationOptions;
    ClientCapabilities           capabilities;
    std::string                  trace{"off"};  // 'off' | 'messages' | 'verbose'
    std::vector<WorkspaceFolder> workspaceFolders;
};

struct InitializeResult : public Protocol {
    auto Encode() -> nlohmann::json override {
        nlohmann::json json;
        json["capabilities"] = capabilities.Encode();
        json["serverInfo"]   = serverInfo.Encode();

        return json;
    }

    ServerCapabilities capabilities;
    BaseInfo           serverInfo;
};

struct Registration : public Protocol {
    std::string id;
    std::string method;
    std::any    registerOptions;
};

struct RegistrationParams : public Protocol {
    std::vector<Registration> registrations;
};

struct Unregistration : public Protocol {
    std::string id;
    std::string method;
};

struct UnregistrationParams : public Protocol {
    std::vector<Unregistration> unregisterations;
};

struct SetTraceParams : public Protocol {
    std::string value;  // 'off' | 'messages' | 'verbose'
};

struct LogTraceParams : public Protocol {
    std::string message;
    std::string verbose;
};
}  // namespace lsp