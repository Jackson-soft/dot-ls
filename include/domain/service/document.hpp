#pragma once

#include "domain/model/document.hpp"

// 文本相关
namespace domain::service {
class Document {
public:
    Document()  = default;
    ~Document() = default;

    auto DidOpen(const domain::model::DidOpenTextDocumentParams &params) -> domain::model::Protocol {
        return {};
    }

    auto DidSave(const domain::model::SaveOptions &params) -> domain::model::Protocol {
        return {};
    }

    auto DidChange(const domain::model::DidChangeTextDocumentParams &params) -> domain::model::Protocol {
        return {};
    }

    auto DidClose(const domain::model::DidCloseTextDocumentParams &params) -> domain::model::Protocol {
        return {};
    }

    auto Rename(const domain::model::RenameParams &params) -> domain::model::Protocol {
        return {};
    }
};
}  // namespace domain::service