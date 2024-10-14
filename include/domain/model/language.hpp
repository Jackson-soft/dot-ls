#pragma once

#include "domain/model/basic.hpp"

#include <any>
#include <optional>
#include <string>
#include <vector>

namespace domain::model {
struct CompletionContext : public Protocol {
    CompletionTriggerKind triggerKind;
    std::string           triggerCharacter;
};

// 补全的入参
struct CompletionParams : public TextDocumentPositionParams, WorkDoneProgressParams, PartialResultParams {
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
    bool                        isIncomplete;
    std::optional<ItemDefault>  itemDefaults;
    std::vector<CompletionItem> items;
};

}  // namespace domain::model