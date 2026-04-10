#pragma once

#include "infra/config/flag.hpp"
#include "infra/parser/tree_sitter_adapter.hpp"
#include "protocol/lsp/language.hpp"

#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

// 语言功能服务（补全、Hover、高亮、语义 token、定义、诊断）
namespace domain::service {

class Language {
public:
    Language()  = default;
    ~Language() = default;

    // ── 代码补全 ─────────────────────────────────────────────────────────────
    auto Completion(const lsp::CompletionParams & /*param*/) -> lsp::CompletionList {
        lsp::CompletionList result;
        result.isIncomplete = false;

        for (const auto &kw : infra::config::Keywords) {
            lsp::CompletionItem item;
            item.label            = kw;
            item.kind             = lsp::CompletionItemKind::Keyword;
            item.detail           = "DOT keyword";
            item.insertText       = kw;
            item.insertTextFormat = lsp::InsertTextFormat::PlainText;
            result.items.push_back(std::move(item));
        }

        static const std::vector<std::pair<std::string, std::string>> kAttrs = {
            {"label", "Display label"},
            {"color", "Border / line color"},
            {"fillcolor", "Fill color (requires style=filled)"},
            {"bgcolor", "Background color"},
            {"shape", "Node shape"},
            {"style", "Drawing style (filled, dashed, bold, rounded, …)"},
            {"fontname", "Font family"},
            {"fontsize", "Font size in points"},
            {"fontcolor", "Font color"},
            {"width", "Node width (inches)"},
            {"height", "Node height (inches)"},
            {"penwidth", "Line width"},
            {"arrowhead", "Arrow head style"},
            {"arrowtail", "Arrow tail style"},
            {"dir", "Edge direction (forward, back, both, none)"},
            {"rankdir", "Layout direction (TB, BT, LR, RL)"},
            {"rank", "Node rank (same, min, max, source, sink)"},
            {"splines", "Edge routing (line, polyline, curved, ortho, spline)"},
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
            {"labeljust", "Label justification (l, c, r)"},
            {"labelloc", "Label location (t, b, c)"},
            {"minlen", "Minimum edge length in ranks"},
            {"headlabel", "Label at edge head"},
            {"taillabel", "Label at edge tail"},
            {"headport", "Entry point at head"},
            {"tailport", "Exit point at tail"},
            {"pos", "Node position (x,y)"},
        };
        for (const auto &[name, desc] : kAttrs) {
            lsp::CompletionItem item;
            item.label            = name;
            item.kind             = lsp::CompletionItemKind::Property;
            item.detail           = desc;
            item.insertText       = name;
            item.insertTextFormat = lsp::InsertTextFormat::PlainText;
            result.items.push_back(std::move(item));
        }

        static const std::vector<std::string> kShapes = {
            "box",     "circle",  "ellipse",       "diamond",       "triangle",      "plaintext", "point",
            "record",  "Mrecord", "doublecircle",  "doubleoctagon", "tripleoctagon", "house",     "pentagon",
            "hexagon", "octagon", "parallelogram", "trapezium",     "egg",           "none",
        };
        for (const auto &s : kShapes) {
            lsp::CompletionItem item;
            item.label            = s;
            item.kind             = lsp::CompletionItemKind::Value;
            item.detail           = "DOT shape";
            item.insertText       = s;
            item.insertTextFormat = lsp::InsertTextFormat::PlainText;
            result.items.push_back(std::move(item));
        }
        return result;
    }

    // ── Hover ────────────────────────────────────────────────────────────────
    auto Hover(uint32_t line, uint32_t col, const std::string &text) -> lsp::Hover {
        const std::string word = wordAt(text, line, col);
        if (word.empty())
            return {};
        const auto &docs = hoverDocs();
        const auto  it   = docs.find(word);
        if (it == docs.end())
            return {};
        lsp::Hover result;
        result.contents.kind  = "markdown";
        result.contents.value = it->second;
        return result;
    }

    // ── Definition（首次出现位置）────────────────────────────────────────────
    auto Definition(uint32_t line, uint32_t col, const std::string &uri, const std::string &text)
        -> std::vector<lsp::Location> {
        auto tree  = ts_.Parse(text);
        auto atPos = ts_.IdentifierAt(tree.get(), text, line, col);
        if (!atPos)
            return {};
        for (const auto &id : ts_.GetIdentifiers(tree.get(), text)) {
            if (id.name != atPos->name)
                continue;
            lsp::Location loc;
            loc.uri                   = uri;
            loc.range.start.line      = id.startLine;
            loc.range.start.character = id.startChar;
            loc.range.end.line        = id.startLine;
            loc.range.end.character   = id.startChar + id.length;
            std::vector<lsp::Location> r;
            r.push_back(std::move(loc));
            return r;
        }
        return {};
    }

    // ── Document Highlight（所有同名标识符）─────────────────────────────────
    auto DocumentHighlight(uint32_t line, uint32_t col, const std::string &text)
        -> std::vector<lsp::DocumentHighlight> {
        auto tree  = ts_.Parse(text);
        auto atPos = ts_.IdentifierAt(tree.get(), text, line, col);
        if (!atPos)
            return {};
        std::vector<lsp::DocumentHighlight> result;
        for (const auto &id : ts_.GetIdentifiers(tree.get(), text)) {
            if (id.name != atPos->name)
                continue;
            lsp::DocumentHighlight hl;
            hl.kind                  = lsp::DocumentHighlightKind::Text;
            hl.range.start.line      = id.startLine;
            hl.range.start.character = id.startChar;
            hl.range.end.line        = id.startLine;
            hl.range.end.character   = id.startChar + id.length;
            result.push_back(std::move(hl));
        }
        return result;
    }

    // ── Semantic Tokens（全文）──────────────────────────────────────────────
    auto SemanticTokensFull(const std::string &text) -> lsp::SemanticTokens {
        auto                tree = ts_.Parse(text);
        lsp::SemanticTokens result;
        result.data = ts_.SemanticTokens(tree.get(), text);
        return result;
    }

    // ── 诊断（语法错误）────────────────────────────────────────────────────
    auto Diagnostics(const std::string &text) -> std::vector<lsp::Diagnostic> {
        auto tree = ts_.Parse(text);
        return ts_.GetErrors(tree.get());
    }

private:
    infra::parser::TreeSitter ts_;

    // ── 光标下的单词（ASCII 标识符字符）──────────────────────────────────────
    static std::string wordAt(const std::string &text, uint32_t line, uint32_t col) {
        std::string_view sv  = text;
        uint32_t         cur = 0;
        std::string_view lineView;
        while (!sv.empty()) {
            auto             pos = sv.find('\n');
            std::string_view row = (pos == std::string_view::npos) ? sv : sv.substr(0, pos);
            if (!row.empty() && row.back() == '\r')
                row.remove_suffix(1);
            if (cur == line) {
                lineView = row;
                break;
            }
            sv.remove_prefix(pos == std::string_view::npos ? sv.size() : pos + 1);
            ++cur;
        }
        if (lineView.empty() || col >= static_cast<uint32_t>(lineView.size()))
            return {};
        auto isIdChar = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        };
        uint32_t s = col, e = col;
        while (s > 0 && isIdChar(lineView[s - 1]))
            --s;
        while (e < static_cast<uint32_t>(lineView.size()) && isIdChar(lineView[e]))
            ++e;
        return std::string(lineView.substr(s, e - s));
    }

    // ── DOT 关键字 / 属性文档 ────────────────────────────────────────────────
    static const std::unordered_map<std::string, std::string> &hoverDocs() {
        static const std::unordered_map<std::string, std::string> docs = {
            {"graph", "**graph** — Undirected graph\n\n```dot\ngraph G { A -- B }\n```"},
            {"digraph", "**digraph** — Directed graph\n\n```dot\ndigraph G { A -> B }\n```"},
            {"strict", "**strict** — Prohibits multi-edges and self-loops."},
            {"subgraph", "**subgraph** — Subgraph / cluster (prefix `cluster_` for visible border)."},
            {"node",
             "**node** — Set default attributes for all subsequent nodes.\n\n```dot\nnode [shape=box, "
             "style=filled]\n```"},
            {"edge",
             "**edge** — Set default attributes for all subsequent edges.\n\n```dot\nedge [color=blue, "
             "style=dashed]\n```"},
            {"label", "**label** `string | HTML`\n\nDisplay text. Supports HTML-like labels."},
            {"color", "**color** `colorname | #RRGGBB`\n\nBorder / line color."},
            {"fillcolor", "**fillcolor** `colorname`\n\nFill color (requires `style=filled`)."},
            {"bgcolor", "**bgcolor** `colorname`\n\nBackground color for the graph or cluster."},
            {"shape",
             "**shape** `shapename`\n\nNode shape. Common: `box` `circle` `ellipse` `diamond` `record` `doublecircle` "
             "`point` `plaintext`."},
            {"style",
             "**style** `style,...`\n\nNode: `filled` `dashed` `dotted` `bold` `rounded`.\nEdge: `solid` `dashed` "
             "`dotted` `bold` `invis`."},
            {"fontname", "**fontname** `font-family`\n\nFont for labels. E.g. `\"Helvetica\"`, `\"Times-Roman\"`."},
            {"fontsize", "**fontsize** `number`\n\nFont size in points (default: 14)."},
            {"fontcolor", "**fontcolor** `colorname`\n\nLabel font color."},
            {"width", "**width** `number`\n\nMinimum node width in inches."},
            {"height", "**height** `number`\n\nMinimum node height in inches."},
            {"penwidth", "**penwidth** `number`\n\nLine width for borders and edges (default: 1.0)."},
            {"arrowhead",
             "**arrowhead** `name`\n\nArrow at edge destination: `normal` `vee` `dot` `odot` `none` `box` `diamond` "
             "`open` `tee`."},
            {"arrowtail", "**arrowtail** `name`\n\nArrow at edge source."},
            {"arrowsize", "**arrowsize** `number`\n\nArrow head size multiplier (default: 1.0)."},
            {"dir", "**dir** `forward | back | both | none`\n\nEdge direction."},
            {"rankdir", "**rankdir** `TB | BT | LR | RL`\n\nLayout direction (default: `TB`)."},
            {"rank", "**rank** `same | min | max | source | sink`\n\nRank constraint within a subgraph."},
            {"splines", "**splines** `none | line | polyline | curved | ortho | spline`\n\nEdge routing style."},
            {"concentrate", "**concentrate** `true | false`\n\nMerge parallel edges."},
            {"compound", "**compound** `true | false`\n\nAllow edges between clusters."},
            {"nodesep",
             "**nodesep** `number`\n\nMinimum space between nodes on the same rank (inches, default: 0.25)."},
            {"ranksep", "**ranksep** `number`\n\nMinimum separation between ranks (inches, default: 0.5)."},
            {"margin", "**margin** `number | \"x,y\"`\n\nGraph margin in inches."},
            {"URL", "**URL** `url`\n\nURL for image maps / SVG output."},
            {"tooltip", "**tooltip** `string`\n\nTooltip on hover (SVG / HTML output)."},
            {"weight", "**weight** `number`\n\nEdge routing weight (default: 1)."},
            {"constraint", "**constraint** `true | false`\n\nIf false, edge does not affect rank assignment."},
            {"lhead", "**lhead** `cluster`\n\nLogical head cluster (requires `compound=true`)."},
            {"ltail", "**ltail** `cluster`\n\nLogical tail cluster (requires `compound=true`)."},
            {"image", "**image** `path`\n\nEmbed an image inside a node."},
            {"fixedsize", "**fixedsize** `true | false`\n\nFix node size to `width`×`height`."},
            {"ordering", "**ordering** `out | in`\n\nConstrain edge order."},
            {"peripheries", "**peripheries** `integer`\n\nNumber of border polygons (default: 1)."},
            {"labeljust", "**labeljust** `l | c | r`\n\nLabel justification for graphs / clusters."},
            {"labelloc", "**labelloc** `t | b | c`\n\nLabel location for nodes."},
            {"minlen", "**minlen** `integer`\n\nMinimum edge length in ranks (default: 1)."},
            {"headlabel", "**headlabel** `string`\n\nExtra label at the edge head."},
            {"taillabel", "**taillabel** `string`\n\nExtra label at the edge tail."},
            {"headport",
             "**headport** `portname | compass`\n\nEntry point at head node (n, ne, e, se, s, sw, w, nw, c)."},
            {"tailport", "**tailport** `portname | compass`\n\nExit point at tail node."},
            {"pos", "**pos** `x,y | x,y!`\n\nNode position (append `!` to pin it)."},
        };
        return docs;
    }
};
}  // namespace domain::service
