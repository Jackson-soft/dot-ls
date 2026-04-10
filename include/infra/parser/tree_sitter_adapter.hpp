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

    // ── 语法错误 → 诊断列表 ───────────────────────────────────────────────────
    [[nodiscard]] std::vector<lsp::Diagnostic> GetErrors(const TSTree *tree) const {
        std::vector<lsp::Diagnostic> result;
        TSNode                       root = ts_tree_root_node(tree);
        walk(root, [&](TSNode node) {
            auto addDiag = [&](TSNode n, std::string msg) {
                auto            sp = ts_node_start_point(n);
                auto            ep = ts_node_end_point(n);
                lsp::Diagnostic d;
                d.range.start.line      = sp.row;
                d.range.start.character = sp.column;
                d.range.end.line        = ep.row;
                d.range.end.character   = ep.column;
                d.severity              = lsp::DiagnosticSeverity::Error;
                d.source                = "dot-ls";
                d.message               = std::move(msg);
                result.push_back(std::move(d));
            };
            if (ts_node_is_error(node)) {
                addDiag(node, "Syntax error");
            } else if (ts_node_is_missing(node) && ts_node_is_named(node)) {
                addDiag(node, std::string("Missing ") + ts_node_type(node));
            }
        });
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
};

}  // namespace infra::parser
