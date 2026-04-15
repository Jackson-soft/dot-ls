#pragma once

// tree-sitter DOT 语法解析适配器（内联实现）

#include "protocol/lsp/basic.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tree_sitter/api.h>
#include <vector>

// DOT 语言 grammar，由 tree-sitter-dot/src/parser.c 提供
extern "C" const TSLanguage *tree_sitter_dot(void);

namespace infra::parser {

// 语义 token 类型（索引必须与 ServerCapabilities.semanticTokensProvider.legend 顺序一致）
enum class TokenType : uint32_t {
    Keyword   = 0,  // strict graph digraph subgraph node edge
    Variable  = 1,  // 节点 / 图标识符
    Namespace = 2,  // 子图 id
    Type      = 3,  // 属性名称
    String    = 4,
    Number    = 5,
    Operator  = 6,  // -> --
};

// 文档中的一个标识符出现位置
struct IdentifierOccurrence {
    std::string name;
    uint32_t    startLine;
    uint32_t    startChar;
    uint32_t    length;
};

// 补全上下文
enum class CompletionKind { TopLevel, AttributeName, AttributeValue, NodeReference };
struct CompletionInfo {
    CompletionKind kind{CompletionKind::TopLevel};
    std::string    attrName;  // 当 kind == AttributeValue 时有效
};

// 文档符号条目（供 Language 服务转为 lsp::DocumentSymbol）
struct SymbolEntry {
    std::string              name;
    int                      kind;                                      // lsp::SymbolKind integer
    uint32_t                 startLine, startChar, endLine, endChar;    // 完整范围
    uint32_t                 selLine, selChar, selEndLine, selEndChar;  // 名称高亮范围
    std::vector<SymbolEntry> children;
};

// 折叠范围条目
struct FoldingEntry {
    uint32_t startLine, endLine;
    bool     isComment{false};
};

// 选区祖先链的一环
struct AncestorRange {
    uint32_t startLine, startChar, endLine, endChar;
};

// 可点击链接（image / URL 属性值）
struct LinkEntry {
    uint32_t    startLine, startChar, endLine, endChar;
    std::string value;    // 原始值（去引号）
    std::string attrName; // "image" 或 "URL"
};

// 图统计（用于 inlay hint / code lens）
struct GraphStats {
    uint32_t    nodeCount{0};
    uint32_t    edgeCount{0};
    uint32_t    headerLine{0};
    uint32_t    headerChar{0};
    std::string graphType;  // "graph" 或 "digraph"
    std::string graphId;
};

class TreeSitter {
public:
    // ── RAII 语法树 ──────────────────────────────────────────────────────────
    struct TreeDeleter {
        void operator()(TSTree *t) const noexcept {
            ts_tree_delete(t);
        }
    };

    using TreePtr = std::unique_ptr<TSTree, TreeDeleter>;

    TreeSitter() : parser_(ts_parser_new()) {
        ts_parser_set_language(parser_, tree_sitter_dot());
    }

    ~TreeSitter() {
        ts_parser_delete(parser_);
    }

    TreeSitter(const TreeSitter &)            = delete;
    TreeSitter &operator=(const TreeSitter &) = delete;

    // ── 解析 ─────────────────────────────────────────────────────────────────
    [[nodiscard]] TreePtr Parse(std::string_view source) const {
        return TreePtr(ts_parser_parse_string(parser_, nullptr, source.data(), static_cast<uint32_t>(source.size())));
    }

    // ── 提取所有 identifier 叶子节点（用于 highlight / definition）────────────
    [[nodiscard]] std::vector<IdentifierOccurrence> GetIdentifiers(const TSTree *tree, std::string_view source) const {
        std::vector<IdentifierOccurrence> result;
        TSNode                            root = ts_tree_root_node(tree);
        walk(root, [&](TSNode node) {
            if (std::strcmp(ts_node_type(node), "identifier") == 0 && ts_node_child_count(node) == 0) {
                auto sp = ts_node_start_point(node);
                auto ep = ts_node_end_point(node);
                result.push_back({nodeText(source, node), sp.row, sp.column, ep.column - sp.column});
            }
        });
        return result;
    }

    // ── 光标处的标识符 ────────────────────────────────────────────────────────
    [[nodiscard]] std::optional<IdentifierOccurrence>
    IdentifierAt(const TSTree *tree, std::string_view source, uint32_t line, uint32_t col) const {
        for (const auto &id : GetIdentifiers(tree, source)) {
            if (id.startLine == line && col >= id.startChar && col < id.startChar + id.length)
                return id;
        }
        return std::nullopt;
    }

    // ── 语法错误 + 语义诊断 → 诊断列表 ────────────────────────────────────────
    [[nodiscard]] std::vector<lsp::Diagnostic> GetErrors(const TSTree *tree,
                                                          std::string_view source) const {
        std::vector<lsp::Diagnostic> result;
        TSNode                       root = ts_tree_root_node(tree);

        auto addDiag = [&](TSNode n, lsp::DiagnosticSeverity sev, std::string msg) {
            auto            sp = ts_node_start_point(n);
            auto            ep = ts_node_end_point(n);
            lsp::Diagnostic d;
            d.range.start.line      = sp.row;
            d.range.start.character = sp.column;
            d.range.end.line        = ep.row;
            d.range.end.character   = ep.column;
            d.severity              = sev;
            d.source                = "dot-ls";
            d.message               = std::move(msg);
            result.push_back(std::move(d));
        };

        // ── 语法错误 ──────────────────────────────────────────────────────────
        walk(root, [&](TSNode node) {
            if (ts_node_is_error(node)) {
                addDiag(node, lsp::DiagnosticSeverity::Error, "Syntax error");
            } else if (ts_node_is_missing(node) && ts_node_is_named(node)) {
                addDiag(node, lsp::DiagnosticSeverity::Error,
                        std::string("Missing ") + ts_node_type(node));
            }
        });

        // ── 语义：有向/无向图与边操作符不匹配 ───────────────────────────────────
        if (!source.empty()) {
            bool isDirected = false;
            for (uint32_t i = 0; i < ts_node_child_count(root); ++i) {
                TSNode child = ts_node_child(root, i);
                if (!ts_node_is_named(child)) {
                    const char *t = ts_node_type(child);
                    if (std::strcmp(t, "digraph") == 0) {
                        isDirected = true;
                        break;
                    }
                    if (std::strcmp(t, "graph") == 0)
                        break;
                }
            }
            walk(root, [&](TSNode node) {
                if (std::strcmp(ts_node_type(node), "edgeop") != 0)
                    return;
                auto        s    = ts_node_start_byte(node);
                auto        e    = ts_node_end_byte(node);
                std::string text(source.substr(s, e - s));
                bool        isArrow = (text == "->");
                if (isDirected && !isArrow) {
                    addDiag(node, lsp::DiagnosticSeverity::Error,
                            "Undirected edge '--' in directed graph; use '->'");
                } else if (!isDirected && isArrow) {
                    addDiag(node, lsp::DiagnosticSeverity::Error,
                            "Directed edge '->' in undirected graph; use '--'");
                }
            });
        }

        return result;
    }

    // ── LSP 语义 token 编码数组（deltaLine, deltaStart, length, type, modifiers）──
    [[nodiscard]] std::vector<uint32_t> SemanticTokens(const TSTree *tree, std::string_view source) const {
        // Pre-pass：收集属性名位置和子图 id 位置，以区分 Variable / Type / Namespace
        using Pos = std::pair<uint32_t, uint32_t>;
        std::set<Pos> attrNamePos;
        std::set<Pos> namespacePos;
        TSNode        root = ts_tree_root_node(tree);

        walk(root, [&](TSNode node) {
            const char *type = ts_node_type(node);
            if (std::strcmp(type, "attribute") == 0) {
                TSNode nameId = ts_node_child_by_field_name(node, "name", 4);
                if (!ts_node_is_null(nameId))
                    collectIdentPos(nameId, attrNamePos);
            } else if (std::strcmp(type, "subgraph") == 0) {
                TSNode idNode = ts_node_child_by_field_name(node, "id", 2);
                if (!ts_node_is_null(idNode))
                    collectIdentPos(idNode, namespacePos);
            }
        });

        static const std::set<std::string_view> kKeywords = {
            "strict",
            "graph",
            "digraph",
            "subgraph",
            "node",
            "edge",
        };
        static const std::set<std::string_view> kOperators = {"->", "--"};

        struct RawToken {
            uint32_t  line, col, len;
            TokenType type;
        };

        std::vector<RawToken> tokens;

        walk(root, [&](TSNode node) {
            if (ts_node_child_count(node) > 0)
                return;  // 只处理叶子节点
            auto sp = ts_node_start_point(node);
            auto ep = ts_node_end_point(node);
            if (ep.row != sp.row)
                return;  // 跳过跨行 token（HTML 字符串等）
            uint32_t len = ep.column - sp.column;
            if (len == 0)
                return;

            if (!ts_node_is_named(node)) {
                auto text = nodeText(source, node);
                if (kKeywords.count(text))
                    tokens.push_back({sp.row, sp.column, len, TokenType::Keyword});
                else if (kOperators.count(text))
                    tokens.push_back({sp.row, sp.column, len, TokenType::Operator});
            } else {
                const char *type = ts_node_type(node);
                if (std::strcmp(type, "identifier") == 0) {
                    Pos       pos{sp.row, sp.column};
                    TokenType tt = attrNamePos.count(pos)  ? TokenType::Type
                                 : namespacePos.count(pos) ? TokenType::Namespace
                                                           : TokenType::Variable;
                    tokens.push_back({sp.row, sp.column, len, tt});
                } else if (std::strcmp(type, "string_literal") == 0) {
                    tokens.push_back({sp.row, sp.column, len, TokenType::String});
                } else if (std::strcmp(type, "number_literal") == 0) {
                    tokens.push_back({sp.row, sp.column, len, TokenType::Number});
                }
            }
        });

        // tree-sitter 通常按文档顺序遍历，保险起见排序
        std::sort(tokens.begin(), tokens.end(), [](const RawToken &a, const RawToken &b) {
            return a.line < b.line || (a.line == b.line && a.col < b.col);
        });

        // Delta 编码成 LSP 要求的格式
        std::vector<uint32_t> encoded;
        encoded.reserve(tokens.size() * 5);
        uint32_t prevLine = 0, prevChar = 0;
        for (const auto &tok : tokens) {
            uint32_t dl = tok.line - prevLine;
            uint32_t dc = (dl == 0) ? (tok.col - prevChar) : tok.col;
            encoded.insert(encoded.end(), {dl, dc, tok.len, static_cast<uint32_t>(tok.type), 0u});
            prevLine = tok.line;
            prevChar = tok.col;
        }
        return encoded;
    }

    // Legend token type names（顺序必须与 TokenType 枚举一致）
    static std::vector<std::string> TokenTypeNames() {
        return {"keyword", "variable", "namespace", "type", "string", "number", "operator"};
    }

    // ── 上下文感知补全：基于光标前文本判断补全类型（无需完整解析树）────────────
    static CompletionInfo DetectCompletionContext(std::string_view source, uint32_t line,
                                                   uint32_t col) {
        // 找到当前行
        std::string_view lineText;
        uint32_t         curLine = 0;
        auto             rem     = source;
        while (!rem.empty()) {
            auto nlPos  = rem.find('\n');
            auto rowTxt = (nlPos == std::string_view::npos) ? rem : rem.substr(0, nlPos);
            if (!rowTxt.empty() && rowTxt.back() == '\r')
                rowTxt.remove_suffix(1);
            if (curLine == line) {
                lineText = rowTxt;
                break;
            }
            rem.remove_prefix(nlPos == std::string_view::npos ? rem.size() : nlPos + 1);
            ++curLine;
        }
        // 光标前的前缀
        auto prefix = lineText.substr(0, std::min(static_cast<std::size_t>(col), lineText.size()));
        // 去掉行内注释
        if (auto cp = prefix.find("//"); cp != std::string_view::npos)
            prefix = prefix.substr(0, cp);

        // ── 边引用上下文：-> 或 -- 之后，还没遇到 [ 或 { ──────────────────────
        {
            auto arrow = prefix.rfind("->");
            auto dash  = prefix.rfind("--");
            std::size_t opEnd = std::string_view::npos;
            if (arrow != std::string_view::npos
                && (dash == std::string_view::npos || arrow > dash))
                opEnd = arrow + 2;
            else if (dash != std::string_view::npos)
                opEnd = dash + 2;
            if (opEnd != std::string_view::npos) {
                auto after = prefix.substr(opEnd);
                if (after.find('[') == std::string_view::npos
                    && after.find('{') == std::string_view::npos)
                    return {CompletionKind::NodeReference, {}};
            }
        }

        // ── 属性上下文：未闭合的 [ ──────────────────────────────────────────────
        {
            int depth = 0;
            for (char c : prefix) {
                if (c == '[') ++depth;
                else if (c == ']') --depth;
            }
            if (depth > 0) {
                // 找到最后一个未匹配的 [
                std::size_t lastOpen = 0;
                int         d        = 0;
                for (std::size_t i = prefix.size(); i > 0; --i) {
                    char c = prefix[i - 1];
                    if (c == ']') ++d;
                    else if (c == '[') {
                        if (d == 0) {
                            lastOpen = i;
                            break;
                        }
                        --d;
                    }
                }
                auto inside = prefix.substr(lastOpen);
                // 找当前属性片段（逗号/分号分隔）
                auto lastComma = inside.rfind(',');
                auto lastSemi  = inside.rfind(';');
                std::size_t attrStart = 0;
                if (lastComma != std::string_view::npos
                    && (lastSemi == std::string_view::npos || lastComma > lastSemi))
                    attrStart = lastComma + 1;
                else if (lastSemi != std::string_view::npos)
                    attrStart = lastSemi + 1;
                auto cur   = inside.substr(attrStart);
                auto eqPos = cur.find('=');
                if (eqPos != std::string_view::npos) {
                    auto name = cur.substr(0, eqPos);
                    // trim
                    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
                        name.remove_prefix(1);
                    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
                        name.remove_suffix(1);
                    return {CompletionKind::AttributeValue, std::string(name)};
                }
                return {CompletionKind::AttributeName, {}};
            }
        }

        return {CompletionKind::TopLevel, {}};
    }

    // ── 提取文档中所有 node_id 处的唯一标识符（用于补全时建议节点名）────────────
    [[nodiscard]] std::vector<std::string> GetUniqueNodeNames(const TSTree *tree,
                                                               std::string_view source) const {
        std::set<std::string> names;
        TSNode                root = ts_tree_root_node(tree);
        walk(root, [&](TSNode node) {
            if (std::strcmp(ts_node_type(node), "node_id") == 0
                && ts_node_named_child_count(node) > 0) {
                TSNode idNode = ts_node_named_child(node, 0);
                names.insert(nodeText(source, idNode));
            }
        });
        return std::vector<std::string>(names.begin(), names.end());
    }

    // ── 文档符号（node_stmt / subgraph 层次结构）───────────────────────────────
    [[nodiscard]] std::vector<SymbolEntry> GetDocumentSymbols(const TSTree *tree,
                                                               std::string_view source) const {
        TSNode root = ts_tree_root_node(tree);

        // 顶层：graph / digraph 本身
        SymbolEntry top;
        top.kind = 2;  // Module
        {
            std::string typeName, idText;
            uint32_t    count = ts_node_child_count(root);
            for (uint32_t i = 0; i < count; ++i) {
                TSNode      child = ts_node_child(root, i);
                const char *t     = ts_node_type(child);
                if (!ts_node_is_named(child)
                    && (std::strcmp(t, "graph") == 0 || std::strcmp(t, "digraph") == 0)) {
                    typeName = t;
                } else if (ts_node_is_named(child) && std::strcmp(t, "id") == 0) {
                    idText = nodeText(source, child);
                }
            }
            top.name = typeName.empty() ? "<graph>"
                                        : (idText.empty() ? typeName : typeName + " " + idText);
        }
        auto rSp       = ts_node_start_point(root);
        auto rEp       = ts_node_end_point(root);
        top.startLine  = rSp.row;  top.startChar  = rSp.column;
        top.endLine    = rEp.row;  top.endChar    = rEp.column;
        top.selLine    = rSp.row;  top.selChar    = rSp.column;
        top.selEndLine = rSp.row;  top.selEndChar = rSp.column + static_cast<uint32_t>(top.name.size());

        collectSymbolsFromNode(root, source, top.children);
        return {top};
    }

    // ── 折叠范围（block 节点 + 多行注释）────────────────────────────────────────
    [[nodiscard]] std::vector<FoldingEntry> GetFoldingRanges(const TSTree *tree,
                                                              std::string_view /*source*/) const {
        std::vector<FoldingEntry> result;
        TSNode                    root = ts_tree_root_node(tree);
        walk(root, [&](TSNode node) {
            const char *type = ts_node_type(node);
            if (std::strcmp(type, "block") == 0) {
                auto sp = ts_node_start_point(node);
                auto ep = ts_node_end_point(node);
                if (ep.row > sp.row)
                    result.push_back({sp.row, ep.row, false});
            } else if (std::strcmp(type, "comment") == 0) {
                auto sp = ts_node_start_point(node);
                auto ep = ts_node_end_point(node);
                if (ep.row > sp.row)
                    result.push_back({sp.row, ep.row, true});
            }
        });
        return result;
    }

    // ── 选区扩展链（从光标位置向上遍历 AST）──────────────────────────────────────
    [[nodiscard]] std::vector<AncestorRange> GetAncestorChain(const TSTree *tree,
                                                               uint32_t line,
                                                               uint32_t col) const {
        TSNode     root  = ts_tree_root_node(tree);
        TSPoint    pt    = {line, col};
        TSNode     node  = ts_node_named_descendant_for_point_range(root, pt, pt);
        std::vector<AncestorRange> chain;
        while (!ts_node_is_null(node)) {
            auto sp = ts_node_start_point(node);
            auto ep = ts_node_end_point(node);
            chain.push_back({sp.row, sp.column, ep.row, ep.column});
            node = ts_node_parent(node);
        }
        return chain;
    }

    // ── 提取 image / URL 属性值（用于 documentLink）──────────────────────────
    [[nodiscard]] std::vector<LinkEntry> GetAttributeLinks(const TSTree *tree,
                                                            std::string_view source) const {
        std::vector<LinkEntry> result;
        TSNode                 root = ts_tree_root_node(tree);
        walk(root, [&](TSNode node) {
            if (std::strcmp(ts_node_type(node), "attribute") != 0)
                return;
            TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
            TSNode valNode  = ts_node_child_by_field_name(node, "value", 5);
            if (ts_node_is_null(nameNode) || ts_node_is_null(valNode))
                return;
            auto name = nodeText(source, nameNode);
            if (name != "image" && name != "URL" && name != "url")
                return;
            auto        raw     = nodeText(source, valNode);
            std::string cleaned = raw;
            if (cleaned.size() >= 2 && cleaned.front() == '"' && cleaned.back() == '"')
                cleaned = cleaned.substr(1, cleaned.size() - 2);
            if (cleaned.empty())
                return;
            auto sp = ts_node_start_point(valNode);
            auto ep = ts_node_end_point(valNode);
            result.push_back({sp.row, sp.column, ep.row, ep.column, cleaned, name});
        });
        return result;
    }

    // ── 图统计（节点数、边数、声明位置）──────────────────────────────────────
    [[nodiscard]] GraphStats GetGraphStats(const TSTree *tree, std::string_view source) const {
        GraphStats stats;
        TSNode     root  = ts_tree_root_node(tree);
        uint32_t   count = ts_node_child_count(root);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode      child = ts_node_child(root, i);
            const char *t     = ts_node_type(child);
            if (!ts_node_is_named(child)) {
                if (std::strcmp(t, "digraph") == 0 || std::strcmp(t, "graph") == 0) {
                    stats.graphType = t;
                    auto sp         = ts_node_start_point(child);
                    stats.headerLine = sp.row;
                    stats.headerChar = sp.column;
                }
            } else if (std::strcmp(t, "id") == 0 && !ts_node_is_null(child)) {
                stats.graphId = nodeText(source, child);
            }
        }
        walk(root, [&](TSNode node) {
            if (!ts_node_is_named(node))
                return;
            const char *t = ts_node_type(node);
            if (std::strcmp(t, "node_stmt") == 0)
                ++stats.nodeCount;
            else if (std::strcmp(t, "edge_stmt") == 0)
                ++stats.edgeCount;
        });
        return stats;
    }

private:
    TSParser *parser_;

    static std::string nodeText(std::string_view source, TSNode node) {
        uint32_t s = ts_node_start_byte(node);
        uint32_t e = ts_node_end_byte(node);
        if (e > source.size())
            e = static_cast<uint32_t>(source.size());
        return std::string(source.substr(s, e - s));
    }

    // 收集一个 id 节点下所有 identifier 子节点的起始位置
    static void collectIdentPos(TSNode idNode, std::set<std::pair<uint32_t, uint32_t>> &out) {
        for (uint32_t i = 0; i < ts_node_child_count(idNode); ++i) {
            TSNode child = ts_node_child(idNode, i);
            if (std::strcmp(ts_node_type(child), "identifier") == 0) {
                auto p = ts_node_start_point(child);
                out.insert({p.row, p.column});
            }
        }
    }

    static void walk(TSNode node, const std::function<void(TSNode)> &fn) {
        fn(node);
        for (uint32_t i = 0, n = ts_node_child_count(node); i < n; ++i)
            walk(ts_node_child(node, i), fn);
    }

    // 从节点（source_file 或 subgraph）中递归收集文档符号
    void collectSymbolsFromNode(TSNode node, std::string_view source,
                                std::vector<SymbolEntry> &out) const {
        // 找到 block → stmt_list
        TSNode stmtList = findStmtList(node);
        if (ts_node_is_null(stmtList))
            return;

        uint32_t count = ts_node_child_count(stmtList);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode      child = ts_node_child(stmtList, i);
            if (!ts_node_is_named(child))
                continue;
            const char *type = ts_node_type(child);

            if (std::strcmp(type, "node_stmt") == 0) {
                // 第一个命名子节点是 node_id
                if (ts_node_named_child_count(child) > 0) {
                    TSNode nodeId = ts_node_named_child(child, 0);
                    auto   nSp   = ts_node_start_point(nodeId);
                    auto   nEp   = ts_node_end_point(nodeId);
                    auto   sSp   = ts_node_start_point(child);
                    auto   sEp   = ts_node_end_point(child);
                    SymbolEntry sym;
                    sym.name       = nodeText(source, nodeId);
                    sym.kind       = 13;  // Variable
                    sym.startLine  = sSp.row;  sym.startChar  = sSp.column;
                    sym.endLine    = sEp.row;  sym.endChar    = sEp.column;
                    sym.selLine    = nSp.row;  sym.selChar    = nSp.column;
                    sym.selEndLine = nEp.row;  sym.selEndChar = nEp.column;
                    out.push_back(std::move(sym));
                }
            } else if (std::strcmp(type, "subgraph") == 0) {
                // subgraph 节点，可能有 id 字段
                TSNode idNode = ts_node_child_by_field_name(child, "id", 2);
                auto   sSp   = ts_node_start_point(child);
                auto   sEp   = ts_node_end_point(child);
                SymbolEntry sym;
                sym.kind      = 3;  // Namespace
                sym.startLine = sSp.row; sym.startChar = sSp.column;
                sym.endLine   = sEp.row; sym.endChar   = sEp.column;
                if (!ts_node_is_null(idNode)) {
                    auto nSp       = ts_node_start_point(idNode);
                    auto nEp       = ts_node_end_point(idNode);
                    sym.name       = nodeText(source, idNode);
                    sym.selLine    = nSp.row; sym.selChar    = nSp.column;
                    sym.selEndLine = nEp.row; sym.selEndChar = nEp.column;
                } else {
                    sym.name       = "<subgraph>";
                    sym.selLine    = sSp.row; sym.selChar    = sSp.column;
                    sym.selEndLine = sSp.row; sym.selEndChar = sSp.column;
                }
                // 递归收集子图内部符号
                collectSymbolsFromNode(child, source, sym.children);
                out.push_back(std::move(sym));
            }
        }
    }

    // 在节点内找到 stmt_list（跨 block 层）
    static TSNode findStmtList(TSNode node) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (ts_node_is_named(child) && std::strcmp(ts_node_type(child), "block") == 0) {
                // 在 block 里找 stmt_list
                uint32_t bc = ts_node_child_count(child);
                for (uint32_t j = 0; j < bc; ++j) {
                    TSNode bchild = ts_node_child(child, j);
                    if (ts_node_is_named(bchild)
                        && std::strcmp(ts_node_type(bchild), "stmt_list") == 0) {
                        return bchild;
                    }
                }
            } else if (ts_node_is_named(child)
                       && std::strcmp(ts_node_type(child), "stmt_list") == 0) {
                return child;
            }
        }
        return TSNode{};  // null node
    }
};  // class TreeSitter

}  // namespace infra::parser
