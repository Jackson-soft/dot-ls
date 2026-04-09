#pragma once

#include "protocol/lsp/document.hpp"

// 文本相关
namespace domain::entity {
class Document {
public:
  Document() = default;
  ~Document() = default;

  auto DidOpen(const lsp::DidOpenTextDocumentParams &params) -> lsp::Protocol {
    return {};
  }

  auto DidSave(const lsp::SaveOptions &params) -> lsp::Protocol { return {}; }

  auto DidChange(const lsp::DidChangeTextDocumentParams &params)
      -> lsp::Protocol {
    return {};
  }

  auto DidClose(const lsp::DidCloseTextDocumentParams &params)
      -> lsp::Protocol {
    return {};
  }

  auto Rename(const lsp::RenameParams &params) -> lsp::Protocol { return {}; }
};
} // namespace domain::entity