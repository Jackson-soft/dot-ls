#pragma once

#include "domain/model/basic.hpp"

#include <any>
#include <optional>
#include <string>
#include <vector>

namespace domain::model {

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

struct CompletionContext : public Protocol {
    CompletionTriggerKind triggerKind;
    std::string           triggerCharacter;
};

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