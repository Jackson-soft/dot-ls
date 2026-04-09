#pragma once

#include "infra/config/flag.hpp"
#include "protocol/lsp/language.hpp"

// 语言相关服务
namespace domain::service {
class Language {
public:
  Language() = default;
  ~Language() = default;

  auto Completion(const lsp::CompletionParams &param) -> lsp::CompletionList {
    lsp::CompletionList result;
    result.isIncomplete = false;

    // DOT 语言关键字补全
    for (const auto &kw : infra::config::Keywords) {
      lsp::CompletionItem item;
      item.label = kw;
      item.kind = lsp::CompletionItemKind::Keyword;
      item.detail = "DOT keyword";
      item.insertText = kw;
      item.insertTextFormat = lsp::InsertTextFormat::PlainText;
      result.items.push_back(std::move(item));
    }

    // DOT 语言常用属性补全
    static const std::vector<std::pair<std::string, std::string>> attributes = {
        {"label", "Node/edge label"},
        {"color", "Color attribute"},
        {"shape", "Node shape (box, circle, ellipse, ...)"},
        {"style", "Style (filled, dashed, bold, ...)"},
        {"fillcolor", "Fill color"},
        {"fontname", "Font name"},
        {"fontsize", "Font size"},
        {"fontcolor", "Font color"},
        {"width", "Node width"},
        {"height", "Node height"},
        {"arrowhead", "Arrow head style"},
        {"arrowtail", "Arrow tail style"},
        {"dir", "Edge direction (forward, back, both, none)"},
        {"rankdir", "Graph rank direction (TB, BT, LR, RL)"},
        {"bgcolor", "Background color"},
        {"concentrate", "Merge parallel edges"},
        {"splines", "Edge routing (true, false, ortho, curved, polyline)"},
    };

    for (const auto &[name, desc] : attributes) {
      lsp::CompletionItem item;
      item.label = name;
      item.kind = lsp::CompletionItemKind::Property;
      item.detail = desc;
      item.insertText = name;
      item.insertTextFormat = lsp::InsertTextFormat::PlainText;
      result.items.push_back(std::move(item));
    }

    // DOT 语言常用 shape 值补全
    static const std::vector<std::string> shapes = {
        "box",       "circle", "ellipse", "diamond",      "triangle",
        "plaintext", "point",  "record",  "doublecircle", "doubleoctagon",
    };

    for (const auto &s : shapes) {
      lsp::CompletionItem item;
      item.label = s;
      item.kind = lsp::CompletionItemKind::Value;
      item.detail = "DOT shape";
      item.insertText = s;
      item.insertTextFormat = lsp::InsertTextFormat::PlainText;
      result.items.push_back(std::move(item));
    }

    return result;
  }
};
} // namespace domain::service