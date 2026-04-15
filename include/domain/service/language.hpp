#pragma once

#include "infra/config/flag.hpp"
#include "infra/formatter/dot_formatter.hpp"
#include "infra/parser/tree_sitter_adapter.hpp"
#include "protocol/lsp/language.hpp"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// 语言功能服务
namespace domain::service {

class Language {
public:
    Language()  = default;
    ~Language() = default;

    // ── 上下文感知代码补全 ─────────────────────────────────────────────────────
    auto Completion(const lsp::CompletionParams &param, const std::string &text)
        -> lsp::CompletionList {
        auto tree = ts_.Parse(text);
        auto ctx  = infra::parser::TreeSitter::DetectCompletionContext(
            text,
            static_cast<uint32_t>(param.position.line),
            static_cast<uint32_t>(param.position.character));

        switch (ctx.kind) {
        case infra::parser::CompletionKind::AttributeName:
            return attrNameCompletions();
        case infra::parser::CompletionKind::AttributeValue:
            return attrValueCompletions(ctx.attrName);
        case infra::parser::CompletionKind::NodeReference:
            return nodeNameCompletions(tree.get(), text);
        case infra::parser::CompletionKind::TopLevel:
            return topLevelCompletions(tree.get(), text);
        }
        return {};
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
            return {std::move(loc)};
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

    // ── References（所有同名标识符，返回 Location[]）────────────────────────
    auto References(uint32_t line, uint32_t col, const std::string &uri, const std::string &text)
        -> std::vector<lsp::Location> {
        auto tree  = ts_.Parse(text);
        auto atPos = ts_.IdentifierAt(tree.get(), text, line, col);
        if (!atPos)
            return {};
        std::vector<lsp::Location> result;
        for (const auto &id : ts_.GetIdentifiers(tree.get(), text)) {
            if (id.name != atPos->name)
                continue;
            lsp::Location loc;
            loc.uri                   = uri;
            loc.range.start.line      = id.startLine;
            loc.range.start.character = id.startChar;
            loc.range.end.line        = id.startLine;
            loc.range.end.character   = id.startChar + id.length;
            result.push_back(std::move(loc));
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

    // ── Semantic Tokens（Range 子集）─────────────────────────────────────────
    auto SemanticTokensRange(const std::string &text, const lsp::Range &range)
        -> lsp::SemanticTokens {
        auto tree = ts_.Parse(text);
        auto data = ts_.SemanticTokens(tree.get(), text);

        // 解码为绝对位置
        struct RawTok {
            uint32_t line, col, len, type, mod;
        };
        std::vector<RawTok> tokens;
        uint32_t            prevLine = 0, prevChar = 0;
        for (std::size_t i = 0; i + 4 < data.size(); i += 5) {
            uint32_t dl = data[i], dc = data[i + 1], len = data[i + 2];
            uint32_t tp = data[i + 3], md = data[i + 4];
            uint32_t ln = prevLine + dl;
            uint32_t cl = (dl == 0) ? prevChar + dc : dc;
            tokens.push_back({ln, cl, len, tp, md});
            prevLine = ln;
            prevChar = cl;
        }

        auto startLine = static_cast<uint32_t>(range.start.line);
        auto endLine   = static_cast<uint32_t>(range.end.line);

        lsp::SemanticTokens result;
        uint32_t            rPrevLine = startLine, rPrevChar = 0;
        for (const auto &tok : tokens) {
            if (tok.line < startLine || tok.line > endLine)
                continue;
            if (tok.line == endLine
                && tok.col >= static_cast<uint32_t>(range.end.character))
                continue;
            uint32_t dl = tok.line - rPrevLine;
            uint32_t dc = (dl == 0) ? tok.col - rPrevChar : tok.col;
            result.data.insert(result.data.end(), {dl, dc, tok.len, tok.type, tok.mod});
            rPrevLine = tok.line;
            rPrevChar = tok.col;
        }
        return result;
    }

    // ── 诊断（语法 + 语义错误）──────────────────────────────────────────────
    auto Diagnostics(const std::string &text) -> std::vector<lsp::Diagnostic> {
        auto tree = ts_.Parse(text);
        return ts_.GetErrors(tree.get(), text);
    }

    // ── 文档符号 ─────────────────────────────────────────────────────────────
    auto DocumentSymbols(const std::string &text) -> std::vector<lsp::DocumentSymbol> {
        auto tree    = ts_.Parse(text);
        auto entries = ts_.GetDocumentSymbols(tree.get(), text);
        std::vector<lsp::DocumentSymbol> result;
        result.reserve(entries.size());
        for (auto &e : entries)
            result.push_back(toDocSymbol(e));
        return result;
    }

    // ── 重命名预检 ─────────────────────────────────────────────────────────────
    auto PrepareRename(uint32_t line, uint32_t col, const std::string &text)
        -> std::optional<std::pair<lsp::Range, std::string>> {
        auto       tree  = ts_.Parse(text);
        const auto atPos = ts_.IdentifierAt(tree.get(), text, line, col);
        if (!atPos)
            return std::nullopt;
        lsp::Range range;
        range.start.line      = atPos->startLine;
        range.start.character = atPos->startChar;
        range.end.line        = atPos->startLine;
        range.end.character   = atPos->startChar + atPos->length;
        return std::make_pair(range, atPos->name);
    }

    // ── 重命名 ─────────────────────────────────────────────────────────────────
    auto Rename(uint32_t line, uint32_t col, const std::string &uri,
                const std::string &newName, const std::string &text) -> lsp::WorkspaceEdit {
        lsp::WorkspaceEdit edit;
        auto               tree  = ts_.Parse(text);
        const auto         atPos = ts_.IdentifierAt(tree.get(), text, line, col);
        if (!atPos)
            return edit;
        std::vector<lsp::TextEdit> edits;
        for (const auto &id : ts_.GetIdentifiers(tree.get(), text)) {
            if (id.name != atPos->name)
                continue;
            lsp::TextEdit te;
            te.range.start.line      = id.startLine;
            te.range.start.character = id.startChar;
            te.range.end.line        = id.startLine;
            te.range.end.character   = id.startChar + id.length;
            te.newText               = newName;
            edits.push_back(std::move(te));
        }
        if (!edits.empty())
            edit.changes[uri] = std::move(edits);
        return edit;
    }

    // ── 折叠范围 ─────────────────────────────────────────────────────────────
    auto FoldingRanges(const std::string &text) -> std::vector<lsp::FoldingRange> {
        auto                           tree    = ts_.Parse(text);
        auto                           entries = ts_.GetFoldingRanges(tree.get(), text);
        std::vector<lsp::FoldingRange> result;
        result.reserve(entries.size());
        for (const auto &e : entries) {
            lsp::FoldingRange fr;
            fr.startLine = e.startLine;
            fr.endLine   = e.endLine;
            fr.kind = e.isComment ? lsp::FoldingRangeKind::Comment : lsp::FoldingRangeKind::Region;
            result.push_back(fr);
        }
        return result;
    }

    // ── 选区扩展 ─────────────────────────────────────────────────────────────
    auto SelectionRanges(const std::vector<lsp::Position> &positions, const std::string &text)
        -> std::vector<lsp::SelectionRange> {
        auto                              tree = ts_.Parse(text);
        std::vector<lsp::SelectionRange>  result;
        result.reserve(positions.size());
        for (const auto &pos : positions) {
            auto chain = ts_.GetAncestorChain(
                tree.get(), static_cast<uint32_t>(pos.line), static_cast<uint32_t>(pos.character));
            std::shared_ptr<lsp::SelectionRange> cur;
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                auto next                      = std::make_shared<lsp::SelectionRange>();
                next->range.start.line         = it->startLine;
                next->range.start.character    = it->startChar;
                next->range.end.line           = it->endLine;
                next->range.end.character      = it->endChar;
                next->parent                   = cur;
                cur                            = next;
            }
            if (cur) {
                result.push_back(*cur);
            } else {
                lsp::SelectionRange sr;
                sr.range.start = pos;
                sr.range.end   = pos;
                result.push_back(sr);
            }
        }
        return result;
    }

    // ── Code Action（格式化 + 关键字规范化）────────────────────────────────────
    auto CodeActions(const std::string &uri, const std::string &text,
                     const lsp::Range & /*range*/) -> nlohmann::json {
        nlohmann::json arr = nlohmann::json::array();

        infra::formatter::FormattingOptions opts;
        auto formatted = infra::formatter::DotFormatter::Format(text, opts);
        if (!formatted.empty() && formatted != text) {
            // "Format Document" action
            lsp::WorkspaceEdit edit;
            lsp::TextEdit      te;
            te.range   = wholeDocRange(text);
            te.newText = std::move(formatted);
            edit.changes[uri] = {std::move(te)};

            nlohmann::json action;
            action["title"]       = "Format Document";
            action["kind"]        = "source";
            action["isPreferred"] = true;
            action["edit"]        = edit.Encode();
            arr.push_back(std::move(action));
        }
        return arr;
    }

    // ── InlayHints（图统计：节点数 + 边数）────────────────────────────────────
    auto InlayHints(const std::string &text, const lsp::Range &range)
        -> std::vector<lsp::InlayHint> {
        auto tree  = ts_.Parse(text);
        auto stats = ts_.GetGraphStats(tree.get(), text);
        std::vector<lsp::InlayHint> result;
        if (stats.graphType.empty())
            return result;
        auto startLine = static_cast<uint32_t>(range.start.line);
        auto endLine   = static_cast<uint32_t>(range.end.line);
        if (stats.headerLine < startLine || stats.headerLine > endLine)
            return result;
        auto lines = splitLines(text);
        if (stats.headerLine >= lines.size())
            return result;
        lsp::InlayHint hint;
        hint.position.line      = stats.headerLine;
        hint.position.character = static_cast<uint64_t>(lines[stats.headerLine].size());
        hint.label = "  // " + std::to_string(stats.nodeCount) + " node" +
                     (stats.nodeCount != 1 ? "s" : "") + ", " +
                     std::to_string(stats.edgeCount) + " edge" +
                     (stats.edgeCount != 1 ? "s" : "");
        hint.kind        = lsp::InlayHintKind::Parameter;
        hint.paddingLeft = true;
        result.push_back(std::move(hint));
        return result;
    }

    // ── CodeLens（Validate + Format 按钮）─────────────────────────────────────
    auto CodeLens(const std::string &text) -> std::vector<lsp::CodeLens> {
        auto tree  = ts_.Parse(text);
        auto stats = ts_.GetGraphStats(tree.get(), text);
        std::vector<lsp::CodeLens> result;
        if (stats.graphType.empty())
            return result;
        auto makeRange = [&]() -> lsp::Range {
            lsp::Range r;
            r.start.line      = stats.headerLine;
            r.start.character = stats.headerChar;
            r.end.line        = stats.headerLine;
            r.end.character   = stats.headerChar + static_cast<uint32_t>(stats.graphType.size());
            return r;
        };
        {
            lsp::CodeLens lens;
            lens.range           = makeRange();
            lens.command.title   = "✓ Validate";
            lens.command.command = "dot-ls.validate";
            result.push_back(lens);
        }
        {
            lsp::CodeLens lens;
            lens.range           = makeRange();
            lens.command.title   = "⚙ Format";
            lens.command.command = "dot-ls.formatDocument";
            result.push_back(lens);
        }
        return result;
    }

    // ── DocumentLinks（image / URL 属性值可点击）──────────────────────────────
    auto DocumentLinks(const std::string &text) -> std::vector<lsp::DocumentLink> {
        auto tree    = ts_.Parse(text);
        auto entries = ts_.GetAttributeLinks(tree.get(), text);
        std::vector<lsp::DocumentLink> result;
        for (const auto &e : entries) {
            lsp::DocumentLink dl;
            dl.range.start.line      = e.startLine;
            dl.range.start.character = e.startChar;
            dl.range.end.line        = e.endLine;
            dl.range.end.character   = e.endChar;
            if (e.attrName == "URL" || e.attrName == "url") {
                dl.target  = e.value;
                dl.tooltip = "Open URL: " + e.value;
            } else {
                dl.target  = (e.value.find("://") == std::string::npos)
                                 ? ("file://" + e.value)
                                 : e.value;
                dl.tooltip = "Open image: " + e.value;
            }
            result.push_back(std::move(dl));
        }
        return result;
    }

    // ── OnTypeFormatting（输入 } 时重排当前行缩进）───────────────────────────
    auto OnTypeFormatting(const std::string &text, uint32_t line, uint32_t /*col*/,
                          const std::string &ch,
                          const infra::formatter::FormattingOptions &opts)
        -> std::vector<lsp::TextEdit> {
        if (ch != "}" && ch != "\n")
            return {};
        auto formatted = infra::formatter::DotFormatter::Format(text, opts);
        if (formatted.empty() || formatted == text)
            return {};
        auto origLines = splitLines(text);
        auto fmtLines  = splitLines(formatted);
        if (line >= origLines.size() || line >= fmtLines.size())
            return {};
        if (origLines[line] == fmtLines[line])
            return {};
        lsp::TextEdit edit;
        edit.range.start.line      = line;
        edit.range.start.character = 0;
        edit.range.end.line        = line;
        edit.range.end.character   = static_cast<uint64_t>(origLines[line].size());
        edit.newText               = fmtLines[line];
        return {std::move(edit)};
    }

    // ── ExecuteCommand（返回 workspace/applyEdit 参数，null 表示无操作）──────
    auto ExecuteCommand(const std::string &command, const std::string &uri,
                        const std::string &text) -> nlohmann::json {
        if (command == "dot-ls.formatDocument") {
            infra::formatter::FormattingOptions opts;
            auto formatted = infra::formatter::DotFormatter::Format(text, opts);
            if (formatted.empty() || formatted == text)
                return nullptr;
            lsp::TextEdit edit;
            edit.range   = wholeDocRange(text);
            edit.newText = std::move(formatted);
            nlohmann::json editsArr = nlohmann::json::array();
            editsArr.push_back(edit.Encode());
            nlohmann::json changes;
            changes[uri] = editsArr;
            return nlohmann::json{{"edit", nlohmann::json{{"changes", changes}}}};
        }
        // dot-ls.validate: diagnostics are already pushed via publishDiagnostics
        return nullptr;
    }

    // ── 全文格式化 → TextEdit[] ──────────────────────────────────────────────
    auto Formatting(const std::string &text, const infra::formatter::FormattingOptions &opts)
        -> std::vector<lsp::TextEdit> {
        auto formatted = infra::formatter::DotFormatter::Format(text, opts);
        if (formatted.empty() || formatted == text)
            return {};
        lsp::TextEdit edit;
        edit.range   = wholeDocRange(text);
        edit.newText = std::move(formatted);
        return {std::move(edit)};
    }

    // ── 区间格式化 ─────────────────────────────────────────────────────────────
    auto RangeFormatting(const std::string &text, const lsp::Range &range,
                         const infra::formatter::FormattingOptions &opts)
        -> std::vector<lsp::TextEdit> {
        auto formatted = infra::formatter::DotFormatter::Format(text, opts);
        if (formatted.empty())
            return {};
        auto origLines = splitLines(text);
        auto fmtLines  = splitLines(formatted);
        auto startLine = static_cast<std::size_t>(range.start.line);
        auto endLine   = static_cast<std::size_t>(range.end.line);
        if (startLine >= origLines.size() || startLine >= fmtLines.size())
            return Formatting(text, opts);
        auto fmtEndLine = std::min(endLine, fmtLines.size() > 0 ? fmtLines.size() - 1 : 0);
        std::string newText;
        for (std::size_t i = startLine; i <= fmtEndLine; ++i) {
            newText += fmtLines[i];
            if (i < fmtEndLine)
                newText += '\n';
        }
        lsp::Range editRange;
        editRange.start.line      = startLine;
        editRange.start.character = 0;
        auto actualEnd            = std::min(endLine, origLines.size() > 0 ? origLines.size() - 1 : 0);
        editRange.end.line        = actualEnd;
        editRange.end.character   = static_cast<uint64_t>(origLines[actualEnd].size());
        lsp::TextEdit edit;
        edit.range   = editRange;
        edit.newText = std::move(newText);
        return {std::move(edit)};
    }

private:
    infra::parser::TreeSitter ts_;

    // ── SymbolEntry → lsp::DocumentSymbol ────────────────────────────────────
    static auto toDocSymbol(const infra::parser::SymbolEntry &e) -> lsp::DocumentSymbol {
        lsp::DocumentSymbol sym;
        sym.name                           = e.name;
        sym.kind                           = static_cast<lsp::SymbolKind>(e.kind);
        sym.range.start.line               = e.startLine;
        sym.range.start.character          = e.startChar;
        sym.range.end.line                 = e.endLine;
        sym.range.end.character            = e.endChar;
        sym.selectionRange.start.line      = e.selLine;
        sym.selectionRange.start.character = e.selChar;
        sym.selectionRange.end.line        = e.selEndLine;
        sym.selectionRange.end.character   = e.selEndChar;
        for (const auto &child : e.children)
            sym.children.push_back(toDocSymbol(child));
        return sym;
    }

    // ── 计算文档末尾位置 ─────────────────────────────────────────────────────
    static auto wholeDocRange(const std::string &text) -> lsp::Range {
        lsp::Range range;
        range.start       = {0, 0};
        uint64_t line = 0, col = 0;
        for (char c : text) {
            if (c == '\n') { ++line; col = 0; } else { ++col; }
        }
        range.end.line      = line;
        range.end.character = col;
        return range;
    }

    // ── 按行分割 ─────────────────────────────────────────────────────────────
    static auto splitLines(const std::string &text) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string              cur;
        for (char c : text) {
            if (c == '\n') { lines.push_back(std::move(cur)); cur.clear(); }
            else if (c != '\r') { cur += c; }
        }
        if (!cur.empty() || (!text.empty() && text.back() == '\n'))
            lines.push_back(std::move(cur));
        return lines;
    }

    // ── 光标下的单词 ─────────────────────────────────────────────────────────
    static std::string wordAt(const std::string &text, uint32_t line, uint32_t col) {
        std::string_view sv = text;
        uint32_t         cur = 0;
        std::string_view lineView;
        while (!sv.empty()) {
            auto             pos = sv.find('\n');
            std::string_view row = (pos == std::string_view::npos) ? sv : sv.substr(0, pos);
            if (!row.empty() && row.back() == '\r')
                row.remove_suffix(1);
            if (cur == line) { lineView = row; break; }
            sv.remove_prefix(pos == std::string_view::npos ? sv.size() : pos + 1);
            ++cur;
        }
        if (lineView.empty() || col >= static_cast<uint32_t>(lineView.size()))
            return {};
        auto isIdChar = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        };
        uint32_t s = col, e = col;
        while (s > 0 && isIdChar(lineView[s - 1])) --s;
        while (e < static_cast<uint32_t>(lineView.size()) && isIdChar(lineView[e])) ++e;
        return std::string(lineView.substr(s, e - s));
    }

    // ── 属性名补全列表 ────────────────────────────────────────────────────────
    static lsp::CompletionList attrNameCompletions() {
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

    // ── 属性值补全列表 ────────────────────────────────────────────────────────
    static lsp::CompletionList attrValueCompletions(const std::string &attrName) {
        static const std::unordered_map<std::string, std::vector<std::string>> kValues = {
            {"shape",      {"box","circle","ellipse","diamond","triangle","plaintext","point",
                            "record","Mrecord","doublecircle","doubleoctagon","house","pentagon",
                            "hexagon","octagon","parallelogram","trapezium","egg","none"}},
            {"style",      {"filled","dashed","dotted","bold","rounded","solid","invis","diagonals"}},
            {"dir",        {"forward","back","both","none"}},
            {"rankdir",    {"TB","BT","LR","RL"}},
            {"rank",       {"same","min","max","source","sink"}},
            {"arrowhead",  {"normal","vee","dot","odot","none","box","diamond","open","tee","inv"}},
            {"arrowtail",  {"normal","vee","dot","odot","none","box","diamond","open","tee","inv"}},
            {"splines",    {"none","line","polyline","curved","ortho","spline"}},
            {"concentrate",{"true","false"}},
            {"compound",   {"true","false"}},
            {"fixedsize",  {"true","false"}},
            {"constraint", {"true","false"}},
            {"ordering",   {"out","in"}},
            {"labeljust",  {"l","c","r"}},
            {"labelloc",   {"t","b","c"}},
            {"color",      {"black","white","red","green","blue","yellow","orange","purple","pink",
                            "brown","gray","cyan","magenta","lightblue","lightgreen","lightyellow",
                            "lightgray","darkblue","darkgreen","darkred","navy","olive","teal",
                            "silver","gold","coral","salmon","violet"}},
            {"fillcolor",  {"black","white","red","green","blue","yellow","orange","purple","pink",
                            "brown","gray","cyan","magenta","lightblue","lightgreen","lightyellow",
                            "lightgray","darkblue","darkgreen","darkred","navy","olive","teal",
                            "silver","gold","coral","salmon","violet"}},
            {"fontcolor",  {"black","white","red","green","blue","yellow","gray","darkblue"}},
            {"bgcolor",    {"white","lightgray","lightyellow","lightblue","transparent"}},
            {"fontname",   {"Helvetica","Arial","Times-Roman","Courier","Courier-Bold","Impact",
                            "Georgia","\"Helvetica-Bold\""}},
        };
        lsp::CompletionList result;
        auto it = kValues.find(attrName);
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

    // ── 节点名补全（用于边引用上下文）────────────────────────────────────────
    lsp::CompletionList nodeNameCompletions(const TSTree *tree, std::string_view text) {
        lsp::CompletionList result;
        for (const auto &name : ts_.GetUniqueNodeNames(tree, text)) {
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

    // ── 顶层补全（关键字 + Snippet 模板 + 已有节点名）──────────────────────
    lsp::CompletionList topLevelCompletions(const TSTree *tree, std::string_view text) {
        lsp::CompletionList result;
        result.isIncomplete = false;

        // ── Snippet 模板 ────────────────────────────────────────────────────
        static const std::vector<std::pair<std::string, std::string>> kSnippets = {
            {"digraph",  "digraph ${1:G} {\n\t$0\n}"},
            {"graph",    "graph ${1:G} {\n\t$0\n}"},
            {"subgraph", "subgraph cluster_${1:name} {\n\tlabel=\"${2:cluster}\"\n\t$0\n}"},
            {"node []",  "node [shape=${1:box}, style=${2:filled}, fillcolor=\"${3:lightblue}\"]\n$0"},
            {"edge []",  "edge [color=${1:black}, style=${2:solid}]\n$0"},
            {"->",       "${1:A} -> ${2:B} [label=\"${3}\"]"},
            {"--",       "${1:A} -- ${2:B}"},
            {"node stmt","${1:name} [shape=${2:box}, label=\"${3:$1}\"]"},
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

        // ── 关键字 ──────────────────────────────────────────────────────────
        for (const auto &kw : infra::config::Keywords) {
            lsp::CompletionItem item;
            item.label            = std::string(kw);
            item.kind             = lsp::CompletionItemKind::Keyword;
            item.detail           = "DOT keyword";
            item.insertText       = std::string(kw);
            item.insertTextFormat = lsp::InsertTextFormat::PlainText;
            result.items.push_back(std::move(item));
        }
        for (const auto &name : ts_.GetUniqueNodeNames(tree, text)) {
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

    // ── DOT 关键字 / 属性文档 ────────────────────────────────────────────────
    static const std::unordered_map<std::string, std::string> &hoverDocs() {
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
            {"style", "**style**\n\nNode: `filled` `dashed` `dotted` `bold` `rounded`.\nEdge: `solid` `dashed` `dotted` `bold` `invis`."},
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
};
}  // namespace domain::service
