#pragma once

#include "domain/service/dot_attribute_catalog.hpp"
#include "infra/common/text_encoding.hpp"
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
    auto Completion(const lsp::CompletionParams &param, const std::string &text) -> lsp::CompletionList {
        auto     tree = parseCached(text);
        uint32_t line = static_cast<uint32_t>(param.position.line);
        uint32_t col  = toByteCol(text, line, static_cast<uint32_t>(param.position.character));
        auto     ctx  = infra::parser::TreeSitter::DetectCompletionContext(text, line, col);

        switch (ctx.kind) {
            case infra::parser::CompletionKind::AttributeName:
                return catalog::AttributeNameCompletions();
            case infra::parser::CompletionKind::AttributeValue:
                return catalog::AttributeValueCompletions(ctx.attrName);
            case infra::parser::CompletionKind::NodeReference:
                return catalog::NodeNameCompletions(ts_, tree, text);
            case infra::parser::CompletionKind::TopLevel:
                return catalog::TopLevelCompletions(ts_, tree, text);
        }
        return {};
    }

    // ── Hover ────────────────────────────────────────────────────────────────
    auto Hover(uint32_t line, uint32_t col, const std::string &text) -> lsp::Hover {
        const uint32_t     byteCol = toByteCol(text, line, col);
        const std::string  word    = wordAt(text, line, byteCol);
        if (word.empty())
            return {};
        const auto &docs = catalog::HoverDocs();
        const auto  it   = docs.find(word);
        if (it == docs.end())
            return {};
        lsp::Hover result;
        result.contents.kind  = "markdown";
        result.contents.value = it->second;
        return result;
    }

    // ── SignatureHelp（属性值上下文：= 触发后列出合法取值）──────────────────
    auto SignatureHelp(uint32_t line, uint32_t col, const std::string &text) -> lsp::SignatureHelp {
        const uint32_t byteCol = toByteCol(text, line, col);
        auto           ctx     = infra::parser::TreeSitter::DetectCompletionContext(text, line, byteCol);
        if (ctx.kind != infra::parser::CompletionKind::AttributeValue || ctx.attrName.empty())
            return {};
        auto completions = catalog::AttributeValueCompletions(ctx.attrName);
        if (completions.items.empty())
            return {};

        // 构造 signature label: "attrName = val1 | val2 | ..."
        std::string                            sigLabel = ctx.attrName + " =";
        std::vector<lsp::ParameterInformation> params;
        params.reserve(completions.items.size());
        for (const auto &item : completions.items) {
            // 直接 append 而非 "a + b" 拼接临时字符串，避免每个候选值都构造一次临时 std::string
            sigLabel += " | ";
            sigLabel += item.label;
            lsp::ParameterInformation p;
            p.label = item.label;
            params.push_back(std::move(p));
        }

        lsp::SignatureInformation sig;
        sig.label           = std::move(sigLabel);
        sig.documentation   = "Valid values for the `" + ctx.attrName + "` attribute";
        sig.parameters      = std::move(params);
        sig.activeParameter = 0u;

        lsp::SignatureHelp result;
        result.signatures.push_back(std::move(sig));
        result.activeSignature = 0u;
        result.activeParameter = 0u;
        return result;
    }

    // ── LinkedEditingRange（同名标识符联动编辑范围）──────────────────────────
    auto LinkedEditingRange(uint32_t line, uint32_t col, const std::string &text) -> lsp::LinkedEditingRanges {
        auto           tree    = parseCached(text);
        const uint32_t byteCol = toByteCol(text, line, col);
        auto           atPos   = ts_.IdentifierAt(tree, text, line, byteCol);
        if (!atPos)
            return {};
        lsp::LinkedEditingRanges result;
        for (const auto &id : ts_.GetIdentifiers(tree, text)) {
            if (id.name != atPos->name)
                continue;
            result.ranges.push_back(toLspRange(text, id.startLine, id.startChar, id.startLine, id.startChar + id.length));
        }
        return result;
    }

    // ── Definition（首次出现位置）────────────────────────────────────────────
    auto Definition(uint32_t line, uint32_t col, const std::string &uri, const std::string &text)
        -> std::vector<lsp::Location> {
        auto           tree    = parseCached(text);
        const uint32_t byteCol = toByteCol(text, line, col);
        auto           atPos   = ts_.IdentifierAt(tree, text, line, byteCol);
        if (!atPos)
            return {};
        for (const auto &id : ts_.GetIdentifiers(tree, text)) {
            if (id.name != atPos->name)
                continue;
            lsp::Location loc;
            loc.uri   = uri;
            loc.range = toLspRange(text, id.startLine, id.startChar, id.startLine, id.startChar + id.length);
            return {std::move(loc)};
        }
        return {};
    }

    // ── Document Highlight（所有同名标识符）─────────────────────────────────
    auto DocumentHighlight(uint32_t line, uint32_t col, const std::string &text)
        -> std::vector<lsp::DocumentHighlight> {
        auto           tree    = parseCached(text);
        const uint32_t byteCol = toByteCol(text, line, col);
        auto           atPos   = ts_.IdentifierAt(tree, text, line, byteCol);
        if (!atPos)
            return {};
        std::vector<lsp::DocumentHighlight> result;
        for (const auto &id : ts_.GetIdentifiers(tree, text)) {
            if (id.name != atPos->name)
                continue;
            lsp::DocumentHighlight hl;
            hl.kind  = lsp::DocumentHighlightKind::Text;
            hl.range = toLspRange(text, id.startLine, id.startChar, id.startLine, id.startChar + id.length);
            result.push_back(std::move(hl));
        }
        return result;
    }

    // ── References（所有同名标识符，返回 Location[]）────────────────────────
    auto References(uint32_t line, uint32_t col, const std::string &uri, const std::string &text)
        -> std::vector<lsp::Location> {
        auto           tree    = parseCached(text);
        const uint32_t byteCol = toByteCol(text, line, col);
        auto           atPos   = ts_.IdentifierAt(tree, text, line, byteCol);
        if (!atPos)
            return {};
        std::vector<lsp::Location> result;
        for (const auto &id : ts_.GetIdentifiers(tree, text)) {
            if (id.name != atPos->name)
                continue;
            lsp::Location loc;
            loc.uri   = uri;
            loc.range = toLspRange(text, id.startLine, id.startChar, id.startLine, id.startChar + id.length);
            result.push_back(std::move(loc));
        }
        return result;
    }

    // ── Semantic Tokens（全文）──────────────────────────────────────────────
    auto SemanticTokensFull(const std::string &text) -> lsp::SemanticTokens {
        auto                tree = parseCached(text);
        auto                raw  = ts_.SemanticTokens(tree, text);  // 字节偏移，delta 编码
        lsp::SemanticTokens result;
        result.data = encodeTokensUtf16(text, decodeTokens(raw));
        return result;
    }

    // ── Semantic Tokens（Range 子集）─────────────────────────────────────────
    auto SemanticTokensRange(const std::string &text, const lsp::Range &range) -> lsp::SemanticTokens {
        auto tree   = parseCached(text);
        auto raw    = ts_.SemanticTokens(tree, text);
        auto tokens = decodeTokens(raw);

        auto           startLine   = static_cast<uint32_t>(range.start.line);
        auto           endLine     = static_cast<uint32_t>(range.end.line);
        const uint32_t endByteCol  = toByteCol(text, endLine, static_cast<uint32_t>(range.end.character));

        std::vector<RawTok> filtered;
        for (const auto &tok : tokens) {
            if (tok.line < startLine || tok.line > endLine)
                continue;
            if (tok.line == endLine && tok.col >= endByteCol)
                continue;
            filtered.push_back(tok);
        }

        lsp::SemanticTokens result;
        result.data = encodeTokensUtf16(text, filtered);
        return result;
    }

    // ── 诊断（语法 + 语义错误）──────────────────────────────────────────────
    auto Diagnostics(const std::string &text) -> std::vector<lsp::Diagnostic> {
        auto tree  = parseCached(text);
        auto diags = ts_.GetErrors(tree, text);
        // GetErrors 内部使用字节偏移填充 range，这里统一转换为 LSP 要求的 UTF-16 偏移
        for (auto &d : diags) {
            d.range = toLspRange(text,
                                  static_cast<uint32_t>(d.range.start.line),
                                  static_cast<uint32_t>(d.range.start.character),
                                  static_cast<uint32_t>(d.range.end.line),
                                  static_cast<uint32_t>(d.range.end.character));
        }
        return diags;
    }

    // ── 文档符号 ─────────────────────────────────────────────────────────────
    auto DocumentSymbols(const std::string &text) -> std::vector<lsp::DocumentSymbol> {
        auto                             tree    = parseCached(text);
        auto                             entries = ts_.GetDocumentSymbols(tree, text);
        std::vector<lsp::DocumentSymbol> result;
        result.reserve(entries.size());
        for (auto &e : entries)
            result.push_back(toDocSymbol(e, text));
        return result;
    }

    // ── 重命名预检 ─────────────────────────────────────────────────────────────
    auto PrepareRename(uint32_t line, uint32_t col, const std::string &text)
        -> std::optional<std::pair<lsp::Range, std::string>> {
        auto           tree    = parseCached(text);
        const uint32_t byteCol = toByteCol(text, line, col);
        const auto     atPos   = ts_.IdentifierAt(tree, text, line, byteCol);
        if (!atPos)
            return std::nullopt;
        auto range = toLspRange(text, atPos->startLine, atPos->startChar, atPos->startLine, atPos->startChar + atPos->length);
        return std::make_pair(range, atPos->name);
    }

    // ── 重命名 ─────────────────────────────────────────────────────────────────
    auto
    Rename(uint32_t line, uint32_t col, const std::string &uri, const std::string &newName, const std::string &text)
        -> lsp::WorkspaceEdit {
        lsp::WorkspaceEdit edit;
        auto               tree    = parseCached(text);
        const uint32_t     byteCol = toByteCol(text, line, col);
        const auto         atPos   = ts_.IdentifierAt(tree, text, line, byteCol);
        if (!atPos)
            return edit;
        std::vector<lsp::TextEdit> edits;
        for (const auto &id : ts_.GetIdentifiers(tree, text)) {
            if (id.name != atPos->name)
                continue;
            lsp::TextEdit te;
            te.range   = toLspRange(text, id.startLine, id.startChar, id.startLine, id.startChar + id.length);
            te.newText = newName;
            edits.push_back(std::move(te));
        }
        if (!edits.empty())
            edit.changes[uri] = std::move(edits);
        return edit;
    }

    // ── 折叠范围 ─────────────────────────────────────────────────────────────
    auto FoldingRanges(const std::string &text) -> std::vector<lsp::FoldingRange> {
        auto                           tree    = parseCached(text);
        auto                           entries = ts_.GetFoldingRanges(tree, text);
        std::vector<lsp::FoldingRange> result;
        result.reserve(entries.size());
        for (const auto &e : entries) {
            lsp::FoldingRange fr;
            fr.startLine = e.startLine;
            fr.endLine   = e.endLine;
            fr.kind      = e.isComment ? lsp::FoldingRangeKind::Comment : lsp::FoldingRangeKind::Region;
            result.push_back(fr);
        }
        return result;
    }

    // ── 选区扩展 ─────────────────────────────────────────────────────────────
    auto SelectionRanges(const std::vector<lsp::Position> &positions, const std::string &text)
        -> std::vector<lsp::SelectionRange> {
        auto                             tree = parseCached(text);
        std::vector<lsp::SelectionRange> result;
        result.reserve(positions.size());
        for (const auto &pos : positions) {
            const uint32_t line    = static_cast<uint32_t>(pos.line);
            const uint32_t byteCol = toByteCol(text, line, static_cast<uint32_t>(pos.character));
            auto           chain   = ts_.GetAncestorChain(tree, line, byteCol);
            std::shared_ptr<lsp::SelectionRange> cur;
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                auto next    = std::make_shared<lsp::SelectionRange>();
                next->range  = toLspRange(text, it->startLine, it->startChar, it->endLine, it->endChar);
                next->parent = cur;
                cur          = next;
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
    auto CodeActions(const std::string &uri, const std::string &text, const lsp::Range & /*range*/) -> nlohmann::json {
        nlohmann::json arr = nlohmann::json::array();

        infra::formatter::FormattingOptions opts;
        auto                                formatted = infra::formatter::DotFormatter::Format(text, opts);
        if (!formatted.empty() && formatted != text) {
            // "Format Document" action
            lsp::WorkspaceEdit edit;
            lsp::TextEdit      te;
            te.range          = wholeDocRange(text);
            te.newText        = std::move(formatted);
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
    auto InlayHints(const std::string &text, const lsp::Range &range) -> std::vector<lsp::InlayHint> {
        auto                        tree  = parseCached(text);
        auto                        stats = ts_.GetGraphStats(tree, text);
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
        hint.position = toLspPos(text, stats.headerLine, static_cast<uint32_t>(lines[stats.headerLine].size()));
        hint.label = "  // " + std::to_string(stats.nodeCount) + " node" + (stats.nodeCount != 1 ? "s" : "") + ", "
                   + std::to_string(stats.edgeCount) + " edge" + (stats.edgeCount != 1 ? "s" : "");
        hint.kind        = lsp::InlayHintKind::Parameter;
        hint.paddingLeft = true;
        result.push_back(std::move(hint));
        return result;
    }

    // ── CodeLens（Validate + Format 按钮）─────────────────────────────────────
    auto CodeLens(const std::string &text) -> std::vector<lsp::CodeLens> {
        auto                       tree  = parseCached(text);
        auto                       stats = ts_.GetGraphStats(tree, text);
        std::vector<lsp::CodeLens> result;
        if (stats.graphType.empty())
            return result;
        auto makeRange = [&]() -> lsp::Range {
            return toLspRange(text,
                               stats.headerLine,
                               stats.headerChar,
                               stats.headerLine,
                               stats.headerChar + static_cast<uint32_t>(stats.graphType.size()));
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
        auto                           tree    = parseCached(text);
        auto                           entries = ts_.GetAttributeLinks(tree, text);
        std::vector<lsp::DocumentLink> result;
        for (const auto &e : entries) {
            lsp::DocumentLink dl;
            dl.range = toLspRange(text, e.startLine, e.startChar, e.endLine, e.endChar);
            if (e.attrName == "URL" || e.attrName == "url") {
                dl.target  = e.value;
                dl.tooltip = "Open URL: " + e.value;
            } else {
                dl.target  = (e.value.find("://") == std::string::npos) ? ("file://" + e.value) : e.value;
                dl.tooltip = "Open image: " + e.value;
            }
            result.push_back(std::move(dl));
        }
        return result;
    }

    // ── OnTypeFormatting（输入 } 时重排当前行缩进）───────────────────────────
    auto OnTypeFormatting(const std::string &text,
                          uint32_t           line,
                          uint32_t /*col*/,
                          const std::string                         &ch,
                          const infra::formatter::FormattingOptions &opts) -> std::vector<lsp::TextEdit> {
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
        edit.range.end             = toLspPos(text, line, static_cast<uint32_t>(origLines[line].size()));
        edit.newText                = fmtLines[line];
        return {std::move(edit)};
    }

    // ── ExecuteCommand（返回 workspace/applyEdit 参数，null 表示无操作）──────
    auto ExecuteCommand(const std::string &command, const std::string &uri, const std::string &text) -> nlohmann::json {
        if (command == "dot-ls.formatDocument") {
            infra::formatter::FormattingOptions opts;
            auto                                formatted = infra::formatter::DotFormatter::Format(text, opts);
            if (formatted.empty() || formatted == text)
                return nullptr;
            lsp::TextEdit edit;
            edit.range              = wholeDocRange(text);
            edit.newText            = std::move(formatted);
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
    auto RangeFormatting(const std::string                         &text,
                         const lsp::Range                          &range,
                         const infra::formatter::FormattingOptions &opts) -> std::vector<lsp::TextEdit> {
        auto formatted = infra::formatter::DotFormatter::Format(text, opts);
        if (formatted.empty())
            return {};
        auto origLines = splitLines(text);
        auto fmtLines  = splitLines(formatted);
        auto startLine = static_cast<std::size_t>(range.start.line);
        auto endLine   = static_cast<std::size_t>(range.end.line);
        if (startLine >= origLines.size() || startLine >= fmtLines.size())
            return Formatting(text, opts);
        auto        fmtEndLine = std::min(endLine, fmtLines.size() > 0 ? fmtLines.size() - 1 : 0);
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
        editRange.end = toLspPos(text, static_cast<uint32_t>(actualEnd), static_cast<uint32_t>(origLines[actualEnd].size()));
        lsp::TextEdit edit;
        edit.range   = editRange;
        edit.newText = std::move(newText);
        return {std::move(edit)};
    }

private:
    infra::parser::TreeSitter ts_;

    // ── 内容寻址的语法树缓存 ───────────────────────────────────────────────────
    // 此前每个请求（hover / completion / diagnostics / documentSymbol …）都会
    // 各自对整篇文档重新执行一次 tree-sitter 全量 parse；同一编辑周期内经常有
    // 多个特性请求针对同一份未变化的文本触发（例如 didChange 后自动推送诊断，
    // 紧接着客户端又请求 documentSymbol/codeLens），此时可以复用上一次的语法树，
    // 避免重复解析。仅当 text 内容发生变化时才会重新 parse。
    mutable std::string                        lastText_;
    mutable infra::parser::TreeSitter::TreePtr lastTree_;

    [[nodiscard]] const TSTree *parseCached(const std::string &text) const {
        if (!lastTree_ || lastText_ != text) {
            lastTree_ = ts_.Parse(text);
            lastText_ = text;
        }
        return lastTree_.get();
    }

    // ── SymbolEntry → lsp::DocumentSymbol（byte 偏移转换为 LSP 要求的 UTF-16）──
    static auto toDocSymbol(const infra::parser::SymbolEntry &e, const std::string &text) -> lsp::DocumentSymbol {
        lsp::DocumentSymbol sym;
        sym.name            = e.name;
        sym.kind            = static_cast<lsp::SymbolKind>(e.kind);
        sym.range           = toLspRange(text, e.startLine, e.startChar, e.endLine, e.endChar);
        sym.selectionRange  = toLspRange(text, e.selLine, e.selChar, e.selEndLine, e.selEndChar);
        for (const auto &child : e.children)
            sym.children.push_back(toDocSymbol(child, text));
        return sym;
    }

    // ── 计算文档末尾位置 ─────────────────────────────────────────────────────
    static auto wholeDocRange(const std::string &text) -> lsp::Range {
        lsp::Range range;
        range.start   = {0, 0};
        uint32_t line = 0, col = 0;  // col 为字节偏移
        for (char c : text) {
            if (c == '\n') {
                ++line;
                col = 0;
            } else {
                ++col;
            }
        }
        range.end = toLspPos(text, line, col);
        return range;
    }

    // ── UTF-16 ↔ 字节偏移转换（LSP 协议边界）────────────────────────────────
    // 将客户端传入的 UTF-16 code unit 列偏移转换为内部处理使用的 UTF-8 字节偏移。
    static uint32_t toByteCol(const std::string &text, uint32_t line, uint32_t utf16Col) {
        return infra::common::Utf16ToUtf8Byte(infra::common::LineTextAt(text, line), utf16Col);
    }

    // 将内部（tree-sitter / 字节偏移）位置转换为 LSP 要求的 UTF-16 Position。
    static lsp::Position toLspPos(const std::string &text, uint32_t line, uint32_t byteCol) {
        return lsp::Position{line, infra::common::Utf8ByteToUtf16(infra::common::LineTextAt(text, line), byteCol)};
    }

    // 将内部（字节偏移）范围转换为 LSP 要求的 UTF-16 Range。
    static lsp::Range
    toLspRange(const std::string &text, uint32_t startLine, uint32_t startByteCol, uint32_t endLine, uint32_t endByteCol) {
        lsp::Range range;
        range.start = toLspPos(text, startLine, startByteCol);
        range.end   = toLspPos(text, endLine, endByteCol);
        return range;
    }

    // ── 语义 token 的字节偏移 delta 解码 / UTF-16 重编码 ─────────────────────
    struct RawTok {
        uint32_t line, col, len, type, mod;  // col / len 均为字节偏移单位
    };

    static std::vector<RawTok> decodeTokens(const std::vector<uint32_t> &data) {
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
        return tokens;
    }

    static std::vector<uint32_t> encodeTokensUtf16(const std::string &text, const std::vector<RawTok> &tokens) {
        std::vector<uint32_t> encoded;
        encoded.reserve(tokens.size() * 5);
        uint32_t prevLine = 0, prevChar = 0;
        for (const auto &tok : tokens) {
            auto     line     = infra::common::LineTextAt(text, tok.line);
            uint32_t u16Col    = infra::common::Utf8ByteToUtf16(line, tok.col);
            uint32_t u16EndCol = infra::common::Utf8ByteToUtf16(line, tok.col + tok.len);
            uint32_t u16Len    = u16EndCol - u16Col;
            uint32_t dl        = tok.line - prevLine;
            uint32_t dc        = (dl == 0) ? u16Col - prevChar : u16Col;
            encoded.insert(encoded.end(), {dl, dc, u16Len, tok.type, tok.mod});
            prevLine = tok.line;
            prevChar = u16Col;
        }
        return encoded;
    }



    // ── 按行分割 ─────────────────────────────────────────────────────────────
    static auto splitLines(const std::string &text) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string              cur;
        for (char c : text) {
            if (c == '\n') {
                lines.push_back(std::move(cur));
                cur.clear();
            } else if (c != '\r') {
                cur += c;
            }
        }
        if (!cur.empty() || (!text.empty() && text.back() == '\n'))
            lines.push_back(std::move(cur));
        return lines;
    }

    // ── 光标下的单词 ─────────────────────────────────────────────────────────
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

};
}  // namespace domain::service
