#pragma once

#include "protocol/lsp/basic.hpp"

#include <any>
#include <optional>
#include <string>
#include <vector>

namespace lsp {
struct CompletionContext : public Protocol {
  CompletionTriggerKind triggerKind;
  std::string triggerCharacter;
};

// 补全的入参
struct CompletionParams : public TextDocumentPositionParams,
                          WorkDoneProgressParams,
                          PartialResultParams {
  CompletionContext context;
};

struct ItemDefault : public Protocol {
  std::vector<std::string> commitCharacters;
  Range editRange;
  InsertTextFormat insertTextFormat;
  InsertTextMode insertTextMode;
  std::optional<std::any> data;
};

struct CompletionList : public Protocol {
  auto Encode() -> nlohmann::json override {
    auto output = nlohmann::json::object();
    output["isIncomplete"] = isIncomplete;
    auto arr = nlohmann::json::array();
    for (auto &item : items) {
      arr.push_back(item.Encode());
    }
    output["items"] = arr;
    return output;
  }

  bool isIncomplete{false};
  std::optional<ItemDefault> itemDefaults;
  std::vector<CompletionItem> items;
};

} // namespace lsp