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

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <optional>
#include <sstream>
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

// ── 颜色支持（documentColor / colorPresentation）──────────────────────────────
//
// Graphviz 默认使用 X11 颜色方案。绝大多数颜色名在 X11 / SVG(CSS) 方案下取值一致，
// 但少数几个名字（green / gray|grey / maroon / purple）两套方案的 RGB 不同，这里
// 按 X11（即 Graphviz 默认行为）取值，与 graphviz.org/doc/info/colors.html 一致。
inline const std::unordered_map<std::string, std::array<int, 3>> &NamedColorTable() {
    static const std::unordered_map<std::string, std::array<int, 3>> table = {
        {"aliceblue", {240, 248, 255}},       {"antiquewhite", {250, 235, 215}},
        {"aqua", {0, 255, 255}},              {"aquamarine", {127, 255, 212}},
        {"azure", {240, 255, 255}},           {"beige", {245, 245, 220}},
        {"bisque", {255, 228, 196}},          {"black", {0, 0, 0}},
        {"blanchedalmond", {255, 235, 205}},  {"blue", {0, 0, 255}},
        {"blueviolet", {138, 43, 226}},       {"brown", {165, 42, 42}},
        {"burlywood", {222, 184, 135}},       {"cadetblue", {95, 158, 160}},
        {"chartreuse", {127, 255, 0}},        {"chocolate", {210, 105, 30}},
        {"coral", {255, 127, 80}},            {"cornflowerblue", {100, 149, 237}},
        {"cornsilk", {255, 248, 220}},        {"crimson", {220, 20, 60}},
        {"cyan", {0, 255, 255}},              {"darkblue", {0, 0, 139}},
        {"darkcyan", {0, 139, 139}},          {"darkgoldenrod", {184, 134, 11}},
        {"darkgray", {169, 169, 169}},        {"darkgreen", {0, 100, 0}},
        {"darkgrey", {169, 169, 169}},        {"darkkhaki", {189, 183, 107}},
        {"darkmagenta", {139, 0, 139}},       {"darkolivegreen", {85, 107, 47}},
        {"darkorange", {255, 140, 0}},        {"darkorchid", {153, 50, 204}},
        {"darkred", {139, 0, 0}},             {"darksalmon", {233, 150, 122}},
        {"darkseagreen", {143, 188, 143}},    {"darkslateblue", {72, 61, 139}},
        {"darkslategray", {47, 79, 79}},      {"darkturquoise", {0, 206, 209}},
        {"darkviolet", {148, 0, 211}},        {"deeppink", {255, 20, 147}},
        {"deepskyblue", {0, 191, 255}},       {"dimgray", {105, 105, 105}},
        {"dimgrey", {105, 105, 105}},         {"dodgerblue", {30, 144, 255}},
        {"firebrick", {178, 34, 34}},         {"floralwhite", {255, 250, 240}},
        {"forestgreen", {34, 139, 34}},       {"fuchsia", {255, 0, 255}},
        {"gainsboro", {220, 220, 220}},       {"ghostwhite", {248, 248, 255}},
        {"gold", {255, 215, 0}},              {"goldenrod", {218, 165, 32}},
        // X11 gray/grey (与 CSS 的 128,128,128 不同)
        {"gray", {190, 190, 190}},            {"grey", {190, 190, 190}},
        {"green", {0, 255, 0}},  // X11 green（与 CSS 的 0,128,0 不同）
        {"greenyellow", {173, 255, 47}},      {"honeydew", {240, 255, 240}},
        {"hotpink", {255, 105, 180}},         {"indianred", {205, 92, 92}},
        {"indigo", {75, 0, 130}},             {"ivory", {255, 255, 240}},
        {"khaki", {240, 230, 140}},           {"lavender", {230, 230, 250}},
        {"lavenderblush", {255, 240, 245}},   {"lawngreen", {124, 252, 0}},
        {"lemonchiffon", {255, 250, 205}},    {"lightblue", {173, 216, 230}},
        {"lightcoral", {240, 128, 128}},      {"lightcyan", {224, 255, 255}},
        {"lightgoldenrod", {238, 221, 130}},  {"lightgray", {211, 211, 211}},
        {"lightgreen", {144, 238, 144}},      {"lightgrey", {211, 211, 211}},
        {"lightpink", {255, 182, 193}},       {"lightsalmon", {255, 160, 122}},
        {"lightseagreen", {32, 178, 170}},    {"lightskyblue", {135, 206, 250}},
        {"lightslategray", {119, 136, 153}},  {"lightsteelblue", {176, 196, 222}},
        {"lightyellow", {255, 255, 224}},     {"lime", {0, 255, 0}},
        {"limegreen", {50, 205, 50}},         {"linen", {250, 240, 230}},
        {"magenta", {255, 0, 255}},
        // X11 maroon（与 CSS 的 128,0,0 不同）
        {"maroon", {176, 48, 96}},
        {"mediumaquamarine", {102, 205, 170}},{"mediumblue", {0, 0, 205}},
        {"mediumorchid", {186, 85, 211}},     {"mediumpurple", {147, 112, 219}},
        {"mediumseagreen", {60, 179, 113}},   {"mediumslateblue", {123, 104, 238}},
        {"mediumspringgreen", {0, 250, 154}}, {"mediumturquoise", {72, 209, 204}},
        {"mediumvioletred", {199, 21, 133}},  {"midnightblue", {25, 25, 112}},
        {"mintcream", {245, 255, 250}},       {"mistyrose", {255, 228, 225}},
        {"moccasin", {255, 228, 181}},        {"navajowhite", {255, 222, 173}},
        {"navy", {0, 0, 128}},                {"navyblue", {0, 0, 128}},
        {"oldlace", {253, 245, 230}},         {"olive", {128, 128, 0}},
        {"olivedrab", {107, 142, 35}},        {"orange", {255, 165, 0}},
        {"orangered", {255, 69, 0}},          {"orchid", {218, 112, 214}},
        {"palegoldenrod", {238, 232, 170}},   {"palegreen", {152, 251, 152}},
        {"paleturquoise", {175, 238, 238}},   {"palevioletred", {219, 112, 147}},
        {"papayawhip", {255, 239, 213}},      {"peachpuff", {255, 218, 185}},
        {"peru", {205, 133, 63}},             {"pink", {255, 192, 203}},
        {"plum", {221, 160, 221}},            {"powderblue", {176, 224, 230}},
        // X11 purple（与 CSS 的 128,0,128 不同）
        {"purple", {160, 32, 240}},
        {"red", {255, 0, 0}},                 {"rosybrown", {188, 143, 143}},
        {"royalblue", {65, 105, 225}},        {"saddlebrown", {139, 69, 19}},
        {"salmon", {250, 128, 114}},          {"sandybrown", {244, 164, 96}},
        {"seagreen", {46, 139, 87}},          {"seashell", {255, 245, 238}},
        {"sienna", {160, 82, 45}},            {"silver", {192, 192, 192}},
        {"skyblue", {135, 206, 235}},         {"slateblue", {106, 90, 205}},
        {"slategray", {112, 128, 144}},       {"slategrey", {112, 128, 144}},
        {"snow", {255, 250, 250}},            {"springgreen", {0, 255, 127}},
        {"steelblue", {70, 130, 180}},        {"tan", {210, 180, 140}},
        {"teal", {0, 128, 128}},              {"thistle", {216, 191, 216}},
        {"tomato", {255, 99, 71}},            {"transparent", {255, 255, 255}},
        {"turquoise", {64, 224, 208}},        {"violet", {238, 130, 238}},
        {"violetred", {208, 32, 144}},        {"wheat", {245, 222, 179}},
        {"white", {255, 255, 255}},           {"whitesmoke", {245, 245, 245}},
        {"yellow", {255, 255, 0}},            {"yellowgreen", {154, 205, 50}},
    };
    return table;
}

namespace detail {

inline std::string trim(const std::string &s) {
    std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return {};
    std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

inline std::optional<int> hexDigit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return std::nullopt;
}

// HSV (每分量 0..1，Graphviz 惯例) → RGB
inline lsp::Color hsvToColor(double h, double s, double v) {
    h = h - std::floor(h);  // 归一化到 [0,1)
    double i = std::floor(h * 6.0);
    double f = h * 6.0 - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);
    double r = 0, g = 0, b = 0;
    switch (static_cast<int>(i) % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    lsp::Color c;
    c.red   = r;
    c.green = g;
    c.blue  = b;
    c.alpha = 1.0;
    return c;
}

}  // namespace detail

// ── 解析 DOT 颜色取值（hex / HSV / 命名色 / grayNN）──────────────────────────
inline std::optional<lsp::Color> TryParseColor(const std::string &raw) {
    std::string value = detail::trim(raw);
    if (value.empty())
        return std::nullopt;
    // 多色列表（edge 的 "c1:c2"）或调色板渐变，范围替换语义不明确，直接跳过
    if (value.find(':') != std::string::npos || value.find(';') != std::string::npos)
        return std::nullopt;

    // "#RRGGBB" / "#RRGGBBAA"
    if (value.front() == '#') {
        std::string hex = value.substr(1);
        if (hex.size() != 6 && hex.size() != 8)
            return std::nullopt;
        auto byte = [&](std::size_t i) -> std::optional<double> {
            auto hi = detail::hexDigit(hex[i]);
            auto lo = detail::hexDigit(hex[i + 1]);
            if (!hi || !lo)
                return std::nullopt;
            return (*hi * 16 + *lo) / 255.0;
        };
        auto r = byte(0), g = byte(2), b = byte(4);
        if (!r || !g || !b)
            return std::nullopt;
        lsp::Color c;
        c.red   = *r;
        c.green = *g;
        c.blue  = *b;
        c.alpha = (hex.size() == 8) ? byte(6).value_or(1.0) : 1.0;
        return c;
    }

    // "H,S,V" / "H S V"（每分量 0..1，Graphviz HSV 记法）
    {
        std::string normalized = value;
        std::replace(normalized.begin(), normalized.end(), ',', ' ');
        std::istringstream  iss(normalized);
        std::vector<double> parts;
        double              v;
        bool                ok = true;
        while (iss >> v) {
            parts.push_back(v);
        }
        if (!iss.eof())
            ok = false;
        if (ok && parts.size() == 3
            && std::all_of(parts.begin(), parts.end(), [](double d) { return d >= 0.0 && d <= 1.0; })) {
            return detail::hsvToColor(parts[0], parts[1], parts[2]);
        }
    }

    std::string lower = detail::toLower(value);

    // grayNN / greyNN（0..100 灰度百分比）；限制位数避免 stoi 对超长数字串抛出
    // std::out_of_range（例如恶意/畸形输入 "gray99999999999999999999"）。
    if (lower.rfind("gray", 0) == 0 || lower.rfind("grey", 0) == 0) {
        std::string suffix = lower.substr(4);
        if (!suffix.empty() && suffix.size() <= 3
            && std::all_of(suffix.begin(), suffix.end(), [](unsigned char c) { return std::isdigit(c); })) {
            int        level = std::clamp(std::stoi(suffix), 0, 100);
            double     v     = level / 100.0;
            lsp::Color c;
            c.red   = v;
            c.green = v;
            c.blue  = v;
            c.alpha = 1.0;
            return c;
        }
    }

    const auto &table = NamedColorTable();
    if (auto it = table.find(lower); it != table.end()) {
        lsp::Color c;
        c.red   = it->second[0] / 255.0;
        c.green = it->second[1] / 255.0;
        c.blue  = it->second[2] / 255.0;
        c.alpha = 1.0;
        return c;
    }
    return std::nullopt;
}

// ── 由 lsp::Color 反向生成候选表示（用于 colorPresentation）──────────────────
// 始终提供 "#RRGGBB"/"#RRGGBBAA" 十六进制表示；若与已知命名色完全一致，则额外
// 提供该颜色名作为候选（编辑器颜色选择器里通常会展示为下拉列表）。
inline std::vector<lsp::ColorPresentation> ColorPresentations(const lsp::Color &color, bool quoted) {
    auto clamp255 = [](double v) {
        return std::clamp(static_cast<int>(std::lround(v * 255.0)), 0, 255);
    };
    int r = clamp255(color.red), g = clamp255(color.green), b = clamp255(color.blue);
    int a = clamp255(color.alpha);

    char buf[16];
    if (a >= 255)
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    else
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", r, g, b, a);
    std::string hex = buf;

    auto wrap = [&](const std::string &s) {
        return quoted ? ("\"" + s + "\"") : s;
    };

    std::vector<lsp::ColorPresentation> result;
    lsp::ColorPresentation              hexPresentation;
    hexPresentation.label = wrap(hex);
    result.push_back(std::move(hexPresentation));

    // 精确匹配到命名颜色时，附加该名字作为候选（不影响 hex 始终可用）。
    // 多个名字可能对应同一 RGB（如 green/lime、aqua/cyan、navy/navyblue、gray/grey），
    // unordered_map 的遍历顺序是实现细节而非有意为之的优先级，这里收集全部匹配项后按
    // 字典序取最小者，保证结果确定、可复现（恰好也让 green/gray/navy 等更常用的别名胜出）。
    if (a >= 255) {
        std::optional<std::string> best;
        for (const auto &[name, rgb] : NamedColorTable()) {
            if (rgb[0] == r && rgb[1] == g && rgb[2] == b) {
                if (!best || name < *best)
                    best = name;
            }
        }
        if (best) {
            lsp::ColorPresentation namedPresentation;
            namedPresentation.label = wrap(*best);
            result.push_back(std::move(namedPresentation));
        }
    }
    return result;
}

}  // namespace domain::service::catalog
