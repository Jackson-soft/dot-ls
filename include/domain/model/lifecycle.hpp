#pragma once

// lsp lifecycle 相关的数据结构

#include "domain/model/basic.hpp"
#include "domain/model/document.hpp"

#include <any>
#include <cstdint>
#include <string>
#include <vector>

namespace domain::model {
struct SignatureHelpOptions : public WorkDoneProgressOptions {
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
    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;
};

struct SemanticTokensOptions : public WorkDoneProgressOptions {
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
    bool ignoreCase = false;
};

struct FileOperationPattern : public Protocol {
    std::string                 glob;
    std::string                 matches;  // 'file' | 'folder'
    FileOperationPatternOptions options;
};

struct FileOperationFilter : public Protocol {
    std::string          scheme;
    FileOperationPattern pattern;
};

struct FileOperationRegistrationOptions : public Protocol {
    std::vector<FileOperationFilter> filters;
};

struct FileOperations : public Protocol {
    FileOperationRegistrationOptions didCreate;
    FileOperationRegistrationOptions willCreate;
    FileOperationRegistrationOptions didRename;
    FileOperationRegistrationOptions willRename;
    FileOperationRegistrationOptions didDelete;
    FileOperationRegistrationOptions willDelete;
};

struct WorkspaceFoldersServerCapabilities : public Protocol {
    bool        supported = false;
    std::string changeNotifications;
};

struct Workspace : public Protocol {
    WorkDoneProgressOptions workspaceFolders;
    FileOperations          fileOperations;
};

struct ServerCapabilities : public Protocol {
    std::string                     positionEncoding;  // utff-8, utf-16, utf-32
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
    SemanticTokensLegend            semanticTokensProvider;
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
    std::string uri;
    std::string name;
};

struct BaseInfo : public Protocol {
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
        locale = json["locale"].template get<std::string>();

        rootPath = json["rootPath"].template get<std::string>();

        rootUri = json["rootUri"].template get<std::string>();
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

    std::int64_t processId;
    BaseInfo     clientInfo;
    std::string  locale;
    std::string  rootPath;
    std::string  rootUri;

    InitializationOptions initializationOptions;

    ClientCapabilities           capabilities;
    std::string                  trace{"off"};  // 'off' | 'messages' | 'verbose'
    std::vector<WorkspaceFolder> workspaceFolders;
};

struct InitializeResult : public Protocol {
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
}  // namespace domain::model