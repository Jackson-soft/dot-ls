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
    json["triggerCharacters"] = triggerCharacters;
    json["retriggerCharacters"] = retriggerCharacters;
    return json;
  }

  std::vector<std::string> triggerCharacters;
  std::vector<std::string> retriggerCharacters;
};

struct DocumentOnTypeFormattingOptions : public Protocol {
  std::string firstTriggerCharacter;
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
  bool range = false;
  bool full = false;
};

struct DiagnosticOptions : public WorkDoneProgressOptions {
  std::string identifier;
  bool interFileDependencies = false;
  bool workspaceDiagnostics = false;
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

    json["glob"] = glob;
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
  std::string glob;
  std::string matches{"file"}; // 'file' | 'folder'
  FileOperationPatternOptions options;
};

struct FileOperationFilter : public Protocol {
  auto Encode() -> nlohmann::json override {
    nlohmann::json json;

    json["pattern"] = pattern.Encode();
    json["scheme"] = scheme;

    return json;
  }

  std::string scheme;
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

    json["didCreate"] = didCreate.Encode();
    json["willCreate"] = willCreate.Encode();
    json["didRename"] = didRename.Encode();
    json["willRename"] = willRename.Encode();
    json["didDelete"] = didDelete.Encode();
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

    json["supported"] = supported;
    json["changeNotifications"] = changeNotifications;

    return json;
  }

  bool supported = false;
  bool changeNotifications = false;
};

struct Workspace : public Protocol {
  auto Encode() -> nlohmann::json override {
    nlohmann::json json;

    json["workspaceFolders"] = workspaceFolders.Encode();
    json["fileOperations"] = fileOperations.Encode();

    return json;
  }

  WorkspaceFoldersServerCapabilities workspaceFolders;
  FileOperations fileOperations;
};

struct ServerCapabilities : public Protocol {
  auto Encode() -> nlohmann::json override {
    nlohmann::json json;
    json["positionEncoding"] = positionEncoding;
    json["textDocumentSync"] = textDocumentSync.Encode();

    json["completionProvider"] = completionProvider.Encode();

    json["hoverProvider"] = hoverProvider;
    json["signatureHelpProvider"] = signatureHelpProvider.Encode();
    json["declarationProvider"] = declarationProvider;
    json["definitionProvider"] = definitionProvider;
    json["typeDefinitionProvider"] = typeDefinitionProvider;
    json["implementationProvider"] = implementationProvider;

    json["workspace"] = workspace.Encode();

    return json;
  }

  std::string positionEncoding{"utf-16"}; // utf-8, utf-16, utf-32
  TextDocumentSyncOptions textDocumentSync;
  NotebookDocumentSyncOptions notebookDocumentSync;
  CompletionOptions completionProvider;
  bool hoverProvider = false;
  SignatureHelpOptions signatureHelpProvider;
  bool declarationProvider = false;
  bool definitionProvider = false;
  bool typeDefinitionProvider = false;
  bool implementationProvider = false;
  bool referencesProvider = false;
  bool documentHighlightProvider = false;
  bool documentSymbolProvider = false;
  bool codeActionProvider = false;
  bool codeLensProvider = false;
  bool documentLinkProvider = false;
  bool colorProvider = false;
  bool documentFormattingProvider = false;
  bool documentRangeFormattingProvider = false;
  DocumentOnTypeFormattingOptions documentOnTypeFormattingProvider;
  bool renameProvider = false;
  bool foldingRangeProvider = false;
  ExecuteCommandOptions executeCommandProvider;
  bool selectionRangeProvider = false;
  bool linkedEditingRangeProvider = false;
  bool callHierarchyProvider = false;
  SemanticTokensLegend semanticTokensProvider;
  bool monikerProvider{false};
  bool typeHierarchyProvider = false;
  bool inlineValueProvider = false;
  bool inlayHintProvider = false;
  DiagnosticOptions diagnosticProvider;
  bool workspaceSymbolProvider = false;
  Workspace workspace;
  std::any experimental;
};

struct ClientCapabilities : public Protocol {};

struct WorkspaceFolder : public Protocol {
  void Decode(const nlohmann::json &json) override {
    uri = json["uri"].template get<std::string>();
    name = json["name"].template get<std::string>();
  }

  auto Encode() -> nlohmann::json override {
    nlohmann::json json;
    json["uri"] = uri;
    json["name"] = name;

    return json;
  }

  std::string uri;
  std::string name;
};

struct BaseInfo : public Protocol {
  BaseInfo(std::string_view name = "", std::string_view version = "")
      : name(name), version(version) {}

  void Decode(const nlohmann::json &json) override {
    name = json["name"].template get<std::string>();
    version = json["version"].template get<std::string>();
  }

  auto Encode() -> nlohmann::json override {
    nlohmann::json json;
    json["name"] = name;
    json["version"] = version;

    return json;
  }

  std::string name;
  std::string version;
};

struct InitializationOptions : public Protocol {};

struct InitializeParams : public WorkDoneProgressParams {
  void Decode(const nlohmann::json &json) override {
    processId = json["processId"].template get<std::int64_t>();

    clientInfo.Decode(json["clientInfo"].template get<nlohmann::json>());

    locale = json["locale"].template get<std::string>();

    rootPath = json["rootPath"].template get<std::string>();

    rootUri = json["rootUri"].template get<std::string>();

    capabilities.Decode(json["capabilities"].template get<nlohmann::json>());

    trace = json["trace"].template get<std::string>();
  }

  auto Encode() -> nlohmann::json override {
    nlohmann::json json;

    json["processId"] = processId;
    json["clientInfo"] = clientInfo.Encode();
    json["locale"] = locale;
    json["rootPath"] = rootPath;
    json["rootUri"] = rootUri;

    return json;
  }

  std::int64_t processId;
  BaseInfo clientInfo;
  std::string locale{"zh"};
  std::string rootPath;
  std::string rootUri;
  InitializationOptions initializationOptions;
  ClientCapabilities capabilities;
  std::string trace{"off"}; // 'off' | 'messages' | 'verbose'
  std::vector<WorkspaceFolder> workspaceFolders;
};

struct InitializeResult : public Protocol {
  auto Encode() -> nlohmann::json override {
    nlohmann::json json;
    json["capabilities"] = capabilities.Encode();
    json["serverInfo"] = serverInfo.Encode();

    return json;
  }

  ServerCapabilities capabilities;
  BaseInfo serverInfo;
};

struct Registration : public Protocol {
  std::string id;
  std::string method;
  std::any registerOptions;
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
  std::string value; // 'off' | 'messages' | 'verbose'
};

struct LogTraceParams : public Protocol {
  std::string message;
  std::string verbose;
};
} // namespace lsp