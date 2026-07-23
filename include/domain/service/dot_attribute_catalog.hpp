#pragma once

// DOT 属性/关键字参考数据（补全候选、Hover 文档）
//
// 这些内容是纯粹的静态参考数据，不依赖 Language 服务的任何实例状态（ts_ /
// 缓存等），此前却和真正有状态的语言特性实现（Hover/Definition/Diagnostics
// ...）混在同一个 800+ 行的 Language 类里，让该类承担了过多职责。将其拆分到
// 独立模块后，Language 只需在需要处调用这里的纯函数，职责更清晰，也方便单独
// 维护/扩充这份属性表而不影响其余逻辑。

#include "infra/config/flag.hpp"
#include "infra/parser/tree_sitter_adapter.hpp"
#include "protocol/lsp/language.hpp"

#include <string>
#include <string_view>
#include <tree_sitter/api.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace domain::service::catalog {

// ── 属性名补全列表 ────────────────────────────────────────────────────────────
inline lsp::CompletionList AttributeNameCompletions() {
    static const std::vector<std::pair<std::string, std::string>> kAttrs = {
        {"label", "Display label"},
        {"color", "Border / line color"},
        {"fillcolor", "Fill color (requires style=filled)"},
        {"bgcolor", "Background color"},
        {"shape", "Node shape"},
        {"style", "Drawing style"},
        {"fontname", "Font family"},
        {"fontsize", "Font size in points"},
        {"fontcolor", "Font color"},
        {"width", "Node width (inches)"},
        {"height", "Node height (inches)"},
        {"penwidth", "Line width"},
        {"arrowhead", "Arrow head style"},
        {"arrowtail", "Arrow tail style"},
        {"dir", "Edge direction"},
        {"rankdir", "Layout direction (TB, BT, LR, RL)"},
        {"rank", "Node rank"},
        {"splines", "Edge routing style"},
        {"concentrate", "Merge parallel edges"},
        {"compound", "Allow edges between clusters"},
        {"nodesep", "Node separation (inches)"},
        {"ranksep", "Rank separation (inches)"},
        {"margin", "Graph margin (inches)"},
        {"URL", "URL for image maps / SVG"},
        {"tooltip", "Tooltip text"},
        {"weight", "Edge routing weight"},
        {"constraint", "If false, edge doesn't affect ranking"},
        {"lhead", "Logical head cluster"},
        {"ltail", "Logical tail cluster"},
        {"image", "Embedded image path"},
        {"fixedsize", "Fix node size to width×height"},
        {"ordering", "Edge ordering (out, in)"},
        {"peripheries", "Number of border polygons"},
        {"labeljust", "Label justification"},
        {"labelloc", "Label location"},
        {"minlen", "Minimum edge length in ranks"},
        {"headlabel", "Label at edge head"},
        {"taillabel", "Label at edge tail"},
        {"headport", "Entry point at head"},
        {"tailport", "Exit point at tail"},
        {"pos", "Node position (x,y)"},
    };
    lsp::CompletionList result;
    result.isIncomplete = false;
    for (const auto &[name, desc] : kAttrs) {
        lsp::CompletionItem item;
        item.label            = name;
        item.kind             = lsp::CompletionItemKind::Property;
        item.detail           = desc;
        item.insertText       = name;
        item.insertTextFormat = lsp::InsertTextFormat::PlainText;
        result.items.push_back(std::move(item));
    }
    return result;
}

// ── 属性值补全列表 ────────────────────────────────────────────────────────────
inline lsp::CompletionList AttributeValueCompletions(const std::string &attrName) {
    static const std::unordered_map<std::string, std::vector<std::string>> kValues = {
        {"shape",
         {"box",
          "circle",
          "ellipse",
          "diamond",
          "triangle",
          "plaintext",
          "point",
          "record",
          "Mrecord",
          "doublecircle",
          "doubleoctagon",
          "house",
          "pentagon",
          "hexagon",
          "octagon",
          "parallelogram",
          "trapezium",
          "egg",
          "none"}},
        {"style", {"filled", "dashed", "dotted", "bold", "rounded", "solid", "invis", "diagonals"}},
        {"dir", {"forward", "back", "both", "none"}},
        {"rankdir", {"TB", "BT", "LR", "RL"}},
        {"rank", {"same", "min", "max", "source", "sink"}},
        {"arrowhead", {"normal", "vee", "dot", "odot", "none", "box", "diamond", "open", "tee", "inv"}},
        {"arrowtail", {"normal", "vee", "dot", "odot", "none", "box", "diamond", "open", "tee", "inv"}},
        {"splines", {"none", "line", "polyline", "curved", "ortho", "spline"}},
        {"concentrate", {"true", "false"}},
        {"compound", {"true", "false"}},
        {"fixedsize", {"true", "false"}},
        {"constraint", {"true", "false"}},
        {"ordering", {"out", "in"}},
        {"labeljust", {"l", "c", "r"}},
        {"labelloc", {"t", "b", "c"}},
        {"color", {"black",      "white",       "red",       "green",    "blue",      "yellow",  "orange",
                   "purple",     "pink",        "brown",     "gray",     "cyan",      "magenta", "lightblue",
                   "lightgreen", "lightyellow", "lightgray", "darkblue", "darkgreen", "darkred", "navy",
                   "olive",      "teal",        "silver",    "gold",     "coral",     "salmon",  "violet"}},
        {"fillcolor", {"black",      "white",       "red",       "green",    "blue",      "yellow",  "orange",
                       "purple",     "pink",        "brown",     "gray",     "cyan",      "magenta", "lightblue",
                       "lightgreen", "lightyellow", "lightgray", "darkblue", "darkgreen", "darkred", "navy",
                       "olive",      "teal",        "silver",    "gold",     "coral",     "salmon",  "violet"}},
        {"fontcolor", {"black", "white", "red", "green", "blue", "yellow", "gray", "darkblue"}},
        {"bgcolor", {"white", "lightgray", "lightyellow", "lightblue", "transparent"}},
        {"fontname",
         {"Helvetica",
          "Arial",
          "Times-Roman",
          "Courier",
          "Courier-Bold",
          "Impact",
          "Georgia",
          "\"Helvetica-Bold\""}},
    };
    lsp::CompletionList result;
    auto                it = kValues.find(attrName);
    if (it != kValues.end()) {
        for (const auto &val : it->second) {
            lsp::CompletionItem item;
            item.label            = val;
            item.kind             = lsp::CompletionItemKind::Value;
            item.insertText       = val;
            item.insertTextFormat = lsp::InsertTextFormat::PlainText;
            result.items.push_back(std::move(item));
        }
    }
    return result;
}

// ── 节点名补全（用于边引用上下文）────────────────────────────────────────────
inline lsp::CompletionList
NodeNameCompletions(const infra::parser::TreeSitter &ts, const TSTree *tree, std::string_view text) {
    lsp::CompletionList result;
    for (const auto &name : ts.GetUniqueNodeNames(tree, text)) {
        lsp::CompletionItem item;
        item.label            = name;
        item.kind             = lsp::CompletionItemKind::Variable;
        item.detail           = "node";
        item.insertText       = name;
        item.insertTextFormat = lsp::InsertTextFormat::PlainText;
        result.items.push_back(std::move(item));
    }
    return result;
}

// ── 顶层补全（关键字 + Snippet 模板 + 已有节点名）──────────────────────────────
inline lsp::CompletionList
TopLevelCompletions(const infra::parser::TreeSitter &ts, const TSTree *tree, std::string_view text) {
    lsp::CompletionList result;
    result.isIncomplete = false;

    // ── Snippet 模板 ──────────────────────────────────────────────────────────
    static const std::vector<std::pair<std::string, std::string>> kSnippets = {
        {"digraph", "digraph ${1:G} {\n\t$0\n}"},
        {"graph", "graph ${1:G} {\n\t$0\n}"},
        {"subgraph", "subgraph cluster_${1:name} {\n\tlabel=\"${2:cluster}\"\n\t$0\n}"},
        {"node []", "node [shape=${1:box}, style=${2:filled}, fillcolor=\"${3:lightblue}\"]\n$0"},
        {"edge []", "edge [color=${1:black}, style=${2:solid}]\n$0"},
        {"->", "${1:A} -> ${2:B} [label=\"${3}\"]"},
        {"--", "${1:A} -- ${2:B}"},
        {"node stmt", "${1:name} [shape=${2:box}, label=\"${3:$1}\"]"},
    };
    for (const auto &[label, snippet] : kSnippets) {
        lsp::CompletionItem item;
        item.label            = label;
        item.kind             = lsp::CompletionItemKind::Snippet;
        item.detail           = "DOT snippet";
        item.insertText       = snippet;
        item.insertTextFormat = lsp::InsertTextFormat::Snippet;
        result.items.push_back(std::move(item));
    }

    // ── 关键字 ────────────────────────────────────────────────────────────────
    for (const auto &kw : infra::config::Keywords) {
        lsp::CompletionItem item;
        item.label            = std::string(kw);
        item.kind             = lsp::CompletionItemKind::Keyword;
        item.detail           = "DOT keyword";
        item.insertText       = std::string(kw);
        item.insertTextFormat = lsp::InsertTextFormat::PlainText;
        result.items.push_back(std::move(item));
    }
    for (const auto &name : ts.GetUniqueNodeNames(tree, text)) {
        lsp::CompletionItem item;
        item.label            = name;
        item.kind             = lsp::CompletionItemKind::Variable;
        item.detail           = "node";
        item.insertText       = name;
        item.insertTextFormat = lsp::InsertTextFormat::PlainText;
        result.items.push_back(std::move(item));
    }
    return result;
}

// ── DOT 关键字 / 属性文档（用于 Hover）────────────────────────────────────────
inline const std::unordered_map<std::string, std::string> &HoverDocs() {
    static const std::unordered_map<std::string, std::string> docs = {
        {"graph", "**graph** — Undirected graph\n\n```dot\ngraph G { A -- B }\n```"},
        {"digraph", "**digraph** — Directed graph\n\n```dot\ndigraph G { A -> B }\n```"},
        {"strict", "**strict** — Prohibits multi-edges and self-loops."},
        {"subgraph", "**subgraph** — Subgraph / cluster (prefix `cluster_` for visible border)."},
        {"node", "**node** — Set default attributes for all subsequent nodes."},
        {"edge", "**edge** — Set default attributes for all subsequent edges."},
        {"label", "**label** `string | HTML`\n\nDisplay text."},
        {"color", "**color** `colorname | #RRGGBB`\n\nBorder / line color."},
        {"fillcolor", "**fillcolor** `colorname`\n\nFill color (requires `style=filled`)."},
        {"bgcolor", "**bgcolor** `colorname`\n\nBackground color."},
        {"shape", "**shape** `shapename`\n\nNode shape: `box` `circle` `ellipse` `diamond` …"},
        {"style",
         "**style**\n\nNode: `filled` `dashed` `dotted` `bold` `rounded`.\nEdge: `solid` `dashed` `dotted` `bold` "
         "`invis`."},
        {"fontname", "**fontname** `font-family`\n\nFont for labels."},
        {"fontsize", "**fontsize** `number`\n\nFont size in points (default: 14)."},
        {"fontcolor", "**fontcolor** `colorname`\n\nLabel font color."},
        {"width", "**width** `number`\n\nMinimum node width in inches."},
        {"height", "**height** `number`\n\nMinimum node height in inches."},
        {"penwidth", "**penwidth** `number`\n\nLine width (default: 1.0)."},
        {"arrowhead", "**arrowhead** `name`\n\nArrow at edge destination."},
        {"arrowtail", "**arrowtail** `name`\n\nArrow at edge source."},
        {"dir", "**dir** `forward | back | both | none`\n\nEdge direction."},
        {"rankdir", "**rankdir** `TB | BT | LR | RL`\n\nLayout direction (default: `TB`)."},
        {"rank", "**rank** `same | min | max | source | sink`\n\nRank constraint."},
        {"splines", "**splines** `none | line | polyline | curved | ortho | spline`\n\nEdge routing."},
        {"concentrate", "**concentrate** `true | false`\n\nMerge parallel edges."},
        {"compound", "**compound** `true | false`\n\nAllow edges between clusters."},
        {"nodesep", "**nodesep** `number`\n\nSpace between nodes on same rank (inches)."},
        {"ranksep", "**ranksep** `number`\n\nSeparation between ranks (inches)."},
        {"margin", "**margin** `number | \"x,y\"`\n\nGraph margin in inches."},
        {"URL", "**URL** `url`\n\nURL for image maps / SVG output."},
        {"tooltip", "**tooltip** `string`\n\nTooltip on hover."},
        {"weight", "**weight** `number`\n\nEdge routing weight (default: 1)."},
        {"constraint", "**constraint** `true | false`\n\nIf false, edge does not affect ranking."},
        {"lhead", "**lhead** `cluster`\n\nLogical head cluster (requires `compound=true`)."},
        {"ltail", "**ltail** `cluster`\n\nLogical tail cluster (requires `compound=true`)."},
        {"image", "**image** `path`\n\nEmbed an image inside a node."},
        {"fixedsize", "**fixedsize** `true | false`\n\nFix node size to `width`×`height`."},
        {"ordering", "**ordering** `out | in`\n\nConstrain edge order."},
        {"peripheries", "**peripheries** `integer`\n\nNumber of border polygons (default: 1)."},
        {"labeljust", "**labeljust** `l | c | r`\n\nLabel justification."},
        {"labelloc", "**labelloc** `t | b | c`\n\nLabel location."},
        {"minlen", "**minlen** `integer`\n\nMinimum edge length in ranks (default: 1)."},
        {"headlabel", "**headlabel** `string`\n\nExtra label at the edge head."},
        {"taillabel", "**taillabel** `string`\n\nExtra label at the edge tail."},
        {"headport", "**headport** `portname | compass`\n\nEntry point at head node."},
        {"tailport", "**tailport** `portname | compass`\n\nExit point at tail node."},
        {"pos", "**pos** `x,y | x,y!`\n\nNode position."},
    };
    return docs;
}

}  // namespace domain::service::catalog
