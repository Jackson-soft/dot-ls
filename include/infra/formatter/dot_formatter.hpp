#pragma once

// DOT 语言格式化器（基于 tree-sitter AST，对损坏文件不格式化）

#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <tree_sitter/api.h>
#include <vector>

extern "C" const TSLanguage *tree_sitter_dot(void);

namespace infra::formatter {

struct FormattingOptions {
    uint32_t tabSize{4};
    bool     insertSpaces{true};  // true = 空格，false = 制表符
};

class DotFormatter {
public:
    // 格式化 DOT 源码。若存在语法错误则返回空字符串（保留原文不变）。
    static auto Format(std::string_view source, const FormattingOptions &opts) -> std::string {
        struct ParserDel {
            void operator()(TSParser *p) const { ts_parser_delete(p); }
        };
        struct TreeDel {
            void operator()(TSTree *t) const { ts_tree_delete(t); }
        };

        auto parser = std::unique_ptr<TSParser, ParserDel>(ts_parser_new());
        ts_parser_set_language(parser.get(), tree_sitter_dot());
        auto tree = std::unique_ptr<TSTree, TreeDel>(ts_parser_parse_string(
            parser.get(), nullptr, source.data(), static_cast<uint32_t>(source.size())));
        if (!tree)
            return {};

        TSNode root = ts_tree_root_node(tree.get());
        if (ts_node_has_error(root))
            return {};  // 有语法错误则不格式化

        std::string out;
        out.reserve(source.size() + 64);
        fmtSourceFile(root, source, opts, out);
        if (!out.empty() && out.back() != '\n')
            out += '\n';
        return out;
    }

private:
    // ── 工具函数 ─────────────────────────────────────────────────────────────

    static auto ind(int depth, const FormattingOptions &opts) -> std::string {
        if (opts.insertSpaces)
            return std::string(static_cast<std::size_t>(depth) * opts.tabSize, ' ');
        return std::string(static_cast<std::size_t>(depth), '\t');
    }

    static auto raw(std::string_view src, TSNode node) -> std::string {
        auto s = ts_node_start_byte(node);
        auto e = ts_node_end_byte(node);
        if (e > src.size())
            e = static_cast<uint32_t>(src.size());
        return std::string(src.substr(s, e - s));
    }

    // ── attribute: name=value ─────────────────────────────────────────────────
    static auto fmtAttribute(TSNode node, std::string_view src) -> std::string {
        TSNode nameNode  = ts_node_child_by_field_name(node, "name", 4);
        TSNode valueNode = ts_node_child_by_field_name(node, "value", 5);
        std::string out;
        if (!ts_node_is_null(nameNode))
            out += raw(src, nameNode);
        out += '=';
        if (!ts_node_is_null(valueNode))
            out += raw(src, valueNode);
        return out;
    }

    // ── attr_list: 合并所有 [...] 组为 [k=v, k=v] ─────────────────────────────
    static auto fmtAttrList(TSNode node, std::string_view src) -> std::string {
        std::vector<std::string> attrs;
        uint32_t                 count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (ts_node_is_named(child) && std::strcmp(ts_node_type(child), "attribute") == 0)
                attrs.push_back(fmtAttribute(child, src));
        }
        if (attrs.empty())
            return "[]";
        std::string out = "[";
        for (std::size_t i = 0; i < attrs.size(); ++i) {
            if (i > 0)
                out += ", ";
            out += attrs[i];
        }
        out += ']';
        return out;
    }

    // ── node_stmt: node_id (attr_list)? ──────────────────────────────────────
    static auto fmtNodeStmt(TSNode node, std::string_view src, int depth,
                             const FormattingOptions &opts) -> std::string {
        std::string out = ind(depth, opts);
        uint32_t    nc  = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < nc; ++i) {
            TSNode      child = ts_node_named_child(node, i);
            const char *type  = ts_node_type(child);
            if (std::strcmp(type, "node_id") == 0) {
                out += raw(src, child);
            } else if (std::strcmp(type, "attr_list") == 0) {
                out += ' ';
                out += fmtAttrList(child, src);
            }
        }
        return out;
    }

    // ── attr_stmt: (graph|node|edge) attr_list ────────────────────────────────
    static auto fmtAttrStmt(TSNode node, std::string_view src, int depth,
                             const FormattingOptions &opts) -> std::string {
        std::string out   = ind(depth, opts);
        uint32_t    count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_named(child)) {
                out += raw(src, child);  // 关键字：graph / node / edge
            } else if (std::strcmp(ts_node_type(child), "attr_list") == 0) {
                out += ' ';
                out += fmtAttrList(child, src);
            }
        }
        return out;
    }

    // 前向声明（互相调用）
    static auto fmtEdgeStmt(TSNode node, std::string_view src, int depth,
                             const FormattingOptions &opts) -> std::string;
    static auto fmtSubgraph(TSNode node, std::string_view src, int depth,
                             const FormattingOptions &opts, bool addIndent) -> std::string;

    // ── block: { stmt_list? } ─────────────────────────────────────────────────
    // tree-sitter extras（comment / preproc）可能附加在 block 层级而非 stmt_list 内，
    // 因此需要在 block 的 children 中统一处理。
    static auto fmtBlock(TSNode node, std::string_view src, int depth,
                         const FormattingOptions &opts) -> std::string {
        std::string out   = "{\n";
        bool        hasContent = false;
        uint32_t    count = ts_node_child_count(node);

        for (uint32_t i = 0; i < count; ++i) {
            TSNode      child = ts_node_child(node, i);
            if (!ts_node_is_named(child))
                continue;  // 跳过 `{` 和 `}`

            const char *type = ts_node_type(child);
            if (std::strcmp(type, "stmt_list") == 0) {
                fmtStmtList(child, src, depth + 1, opts, out, hasContent);
            } else if (std::strcmp(type, "comment") == 0
                       || std::strcmp(type, "preproc") == 0) {
                // comment 附加在 block 层级（出现在 `{` 之后 stmt_list 之前/之后）
                if (hasContent)
                    out += '\n';
                out += ind(depth + 1, opts) + raw(src, child);
                hasContent = true;
            }
        }

        if (hasContent)
            out += '\n';
        out += ind(depth, opts);
        out += '}';
        return out;
    }

    // ── stmt_list ─────────────────────────────────────────────────────────────
    static void fmtStmtList(TSNode node, std::string_view src, int depth,
                             const FormattingOptions &opts, std::string &out,
                             bool &hasContent) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_named(child))
                continue;  // 跳过 `;`

            const char *type = ts_node_type(child);
            std::string stmt;
            if (std::strcmp(type, "node_stmt") == 0)
                stmt = fmtNodeStmt(child, src, depth, opts);
            else if (std::strcmp(type, "edge_stmt") == 0)
                stmt = fmtEdgeStmt(child, src, depth, opts);
            else if (std::strcmp(type, "attr_stmt") == 0)
                stmt = fmtAttrStmt(child, src, depth, opts);
            else if (std::strcmp(type, "attribute") == 0)
                stmt = ind(depth, opts) + fmtAttribute(child, src);
            else if (std::strcmp(type, "subgraph") == 0)
                stmt = fmtSubgraph(child, src, depth, opts, true);
            else if (std::strcmp(type, "comment") == 0
                     || std::strcmp(type, "preproc") == 0)
                stmt = ind(depth, opts) + raw(src, child);
            else
                stmt = ind(depth, opts) + raw(src, child);  // 未知节点保留原文

            if (!stmt.empty()) {
                if (hasContent)
                    out += '\n';
                out += stmt;
                hasContent = true;
            }
        }
    }

    // ── source_file: strict? (graph|digraph) id? block ──────────────────────
    static void fmtSourceFile(TSNode node, std::string_view src,
                               const FormattingOptions &opts, std::string &out) {
        bool        hasStrict = false;
        std::string graphType;
        TSNode      idNode{};
        bool        hasId = false;
        TSNode      blockNode{};
        bool        hasBlock = false;

        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode      child = ts_node_child(node, i);
            const char *type  = ts_node_type(child);
            if (!ts_node_is_named(child)) {
                if (std::strcmp(type, "strict") == 0)
                    hasStrict = true;
                else if (std::strcmp(type, "graph") == 0 || std::strcmp(type, "digraph") == 0)
                    graphType = type;  // 保持小写（alias 已规范化）
            } else if (std::strcmp(type, "id") == 0) {
                idNode = child;
                hasId  = true;
            } else if (std::strcmp(type, "block") == 0) {
                blockNode = child;
                hasBlock  = true;
            }
        }

        if (hasStrict)
            out += "strict ";
        out += graphType;
        if (hasId) {
            out += ' ';
            out += raw(src, idNode);
        }
        out += ' ';
        if (hasBlock)
            out += fmtBlock(blockNode, src, 0, opts);
    }
};

// ── 互相调用的函数，在类体之后定义 ───────────────────────────────────────────────

// edge_stmt: (node_id|subgraph) (edgeop (node_id|subgraph))+ (attr_list)?
inline auto DotFormatter::fmtEdgeStmt(TSNode node, std::string_view src, int depth,
                                       const FormattingOptions &opts) -> std::string {
    std::string out = ind(depth, opts);
    uint32_t    nc  = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode      child = ts_node_named_child(node, i);
        const char *type  = ts_node_type(child);
        if (i > 0)
            out += ' ';
        if (std::strcmp(type, "node_id") == 0)
            out += raw(src, child);
        else if (std::strcmp(type, "subgraph") == 0)
            out += fmtSubgraph(child, src, depth, opts, false);
        else if (std::strcmp(type, "edgeop") == 0)
            out += raw(src, child);
        else if (std::strcmp(type, "attr_list") == 0)
            out += fmtAttrList(child, src);
    }
    return out;
}

// subgraph: (subgraph id?)? block
// addIndent=true 表示作为顶层语句输出缩进；false 表示内联于 edge_stmt 中
inline auto DotFormatter::fmtSubgraph(TSNode node, std::string_view src, int depth,
                                       const FormattingOptions &opts, bool addIndent) -> std::string {
    std::string out;
    if (addIndent)
        out += ind(depth, opts);

    bool   hasSub  = false;
    TSNode idNode{};
    bool   hasId   = false;
    TSNode blockNode{};
    bool   hasBlock = false;

    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode      child = ts_node_child(node, i);
        const char *type  = ts_node_type(child);
        if (!ts_node_is_named(child) && std::strcmp(type, "subgraph") == 0) {
            hasSub = true;
        } else if (ts_node_is_named(child) && std::strcmp(type, "id") == 0) {
            idNode = child;
            hasId  = true;
        } else if (ts_node_is_named(child) && std::strcmp(type, "block") == 0) {
            blockNode = child;
            hasBlock  = true;
        }
    }

    if (hasSub) {
        out += "subgraph";
        if (hasId) {
            out += ' ';
            out += raw(src, idNode);
        }
        out += ' ';
    }
    if (hasBlock)
        out += fmtBlock(blockNode, src, depth, opts);
    return out;
}

}  // namespace infra::formatter

