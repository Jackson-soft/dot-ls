#include "infra/formatter/dot_formatter.hpp"
#include "infra/parser/tree_sitter_adapter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

// ── DotFormatter 单元测试 ─────────────────────────────────────────────────────

TEST_CASE("formatter: basic digraph", "[formatter]") {
    const std::string src = "DIGRAPH G{A->B}";
    infra::formatter::FormattingOptions opts{.tabSize = 4, .insertSpaces = true};
    auto result = infra::formatter::DotFormatter::Format(src, opts);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.find("digraph") != std::string::npos);
    REQUIRE(result.find("A -> B") != std::string::npos);
    REQUIRE(result.find('\n') != std::string::npos);
}

TEST_CASE("formatter: attr_list normalized", "[formatter]") {
    const std::string src = "graph G { node [shape=box;color=red] }";
    infra::formatter::FormattingOptions opts;
    auto result = infra::formatter::DotFormatter::Format(src, opts);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.find("[shape=box, color=red]") != std::string::npos);
}

TEST_CASE("formatter: strict keyword preserved", "[formatter]") {
    const std::string src = "strict digraph G { A -> B }";
    infra::formatter::FormattingOptions opts;
    auto result = infra::formatter::DotFormatter::Format(src, opts);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.find("strict") != std::string::npos);
    REQUIRE(result.find("digraph") != std::string::npos);
}

TEST_CASE("formatter: subgraph indented", "[formatter]") {
    const std::string src = "digraph G{subgraph cluster_0{A B}}";
    infra::formatter::FormattingOptions opts{.tabSize = 4, .insertSpaces = true};
    auto result = infra::formatter::DotFormatter::Format(src, opts);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.find("subgraph cluster_0") != std::string::npos);
    REQUIRE(result.find("        A") != std::string::npos);
}

TEST_CASE("formatter: comment preserved", "[formatter]") {
    const std::string src = "digraph G {\n// hello\nA -> B\n}";
    infra::formatter::FormattingOptions opts;
    auto result = infra::formatter::DotFormatter::Format(src, opts);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.find("// hello") != std::string::npos);
}

TEST_CASE("formatter: broken source returns empty", "[formatter]") {
    const std::string src = "digraph G { A ->  ";
    infra::formatter::FormattingOptions opts;
    REQUIRE(infra::formatter::DotFormatter::Format(src, opts).empty());
}

TEST_CASE("formatter: tab indentation option", "[formatter]") {
    const std::string src = "digraph G { A -> B }";
    infra::formatter::FormattingOptions opts{.tabSize = 4, .insertSpaces = false};
    auto result = infra::formatter::DotFormatter::Format(src, opts);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.find('\t') != std::string::npos);
}

TEST_CASE("formatter: final newline added", "[formatter]") {
    const std::string src = "graph G{}";
    infra::formatter::FormattingOptions opts;
    auto result = infra::formatter::DotFormatter::Format(src, opts);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.back() == '\n');
}

// ── TreeSitter 新功能测试 ──────────────────────────────────────────────────────

static const std::string kSrc =
    "digraph G {\n"
    "    node [shape=box]\n"
    "    A -> B -> C\n"
    "    A [label=\"start\"]\n"
    "    subgraph cluster_0 {\n"
    "        D\n"
    "        E -> F\n"
    "    }\n"
    "}";

TEST_CASE("document symbol: top-level is Module", "[symbol]") {
    infra::parser::TreeSitter ts;
    auto                      tree = ts.Parse(kSrc);
    auto                      syms = ts.GetDocumentSymbols(tree.get(), kSrc);
    REQUIRE(syms.size() == 1);
    REQUIRE(syms[0].kind == 2);  // Module
    REQUIRE(syms[0].name.find("digraph") != std::string::npos);
}

TEST_CASE("document symbol: node_stmt as Variable child", "[symbol]") {
    infra::parser::TreeSitter ts;
    auto                      tree     = ts.Parse(kSrc);
    auto                      syms     = ts.GetDocumentSymbols(tree.get(), kSrc);
    const auto               &children = syms[0].children;
    REQUIRE_FALSE(children.empty());
    bool foundA = false;
    for (const auto &c : children) {
        if (c.name == "A" && c.kind == 13) foundA = true;
    }
    REQUIRE(foundA);
}

TEST_CASE("document symbol: subgraph as Namespace child", "[symbol]") {
    infra::parser::TreeSitter ts;
    auto                      tree = ts.Parse(kSrc);
    auto                      syms = ts.GetDocumentSymbols(tree.get(), kSrc);
    bool foundCluster = false;
    for (const auto &c : syms[0].children) {
        if (c.kind == 3 && c.name.find("cluster") != std::string::npos) {
            foundCluster = true;
            bool foundD = false;
            for (const auto &gc : c.children) {
                if (gc.name == "D") foundD = true;
            }
            REQUIRE(foundD);
        }
    }
    REQUIRE(foundCluster);
}

TEST_CASE("folding range: block gives region", "[folding]") {
    infra::parser::TreeSitter ts;
    auto                      tree   = ts.Parse(kSrc);
    auto                      ranges = ts.GetFoldingRanges(tree.get(), kSrc);
    REQUIRE(ranges.size() >= 2);
    for (const auto &r : ranges) {
        REQUIRE(r.endLine > r.startLine);
    }
}

TEST_CASE("folding range: multi-line comment folding", "[folding]") {
    const std::string src =
        "digraph G {\n"
        "    /* multi\n"
        "       line\n"
        "       comment */\n"
        "    A -> B\n"
        "}";
    infra::parser::TreeSitter ts;
    auto                      tree   = ts.Parse(src);
    auto                      ranges = ts.GetFoldingRanges(tree.get(), src);
    bool hasComment = false;
    for (const auto &r : ranges) {
        if (r.isComment) hasComment = true;
    }
    REQUIRE(hasComment);
}

TEST_CASE("selection range: returns ancestor chain", "[selection]") {
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(kSrc);
    // 第 2 行第 4 列（'A' in "    A -> B -> C"）
    auto chain = ts.GetAncestorChain(tree.get(), 2, 4);
    REQUIRE(chain.size() >= 4);
    REQUIRE(chain[0].startChar == 4);
}

TEST_CASE("selection range: root reached at end of chain", "[selection]") {
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(kSrc);
    auto                      chain = ts.GetAncestorChain(tree.get(), 0, 0);
    REQUIRE_FALSE(chain.empty());
    // 最后一个是 source_file 根节点，起始 (0,0)
    REQUIRE(chain.back().startLine == 0);
    REQUIRE(chain.back().startChar == 0);
}

// ── GetGraphStats 测试 ────────────────────────────────────────────────────────

TEST_CASE("graph stats: type and id detected", "[stats]") {
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(kSrc);
    auto                      stats = ts.GetGraphStats(tree.get(), kSrc);
    REQUIRE(stats.graphType == "digraph");
    REQUIRE(stats.graphId   == "G");
    REQUIRE(stats.headerLine == 0);
}

TEST_CASE("graph stats: node and edge counts", "[stats]") {
    infra::parser::TreeSitter ts;
    // 2 node_stmt: "node [shape=box]" 和 "A [label=...]"
    // 2 edge_stmt: "A -> B -> C" 和 "E -> F"
    auto tree  = ts.Parse(kSrc);
    auto stats = ts.GetGraphStats(tree.get(), kSrc);
    REQUIRE(stats.nodeCount >= 1);
    REQUIRE(stats.edgeCount >= 2);
}

TEST_CASE("graph stats: empty source gives no type", "[stats]") {
    infra::parser::TreeSitter ts;
    const std::string         src   = "";
    auto                      tree  = ts.Parse(src);
    auto                      stats = ts.GetGraphStats(tree.get(), src);
    REQUIRE(stats.graphType.empty());
}

// ── GetAttributeLinks 测试 ────────────────────────────────────────────────────

TEST_CASE("document link: image attribute extracted", "[link]") {
    const std::string         src = "digraph G { A [image=\"/path/to/img.png\"] }";
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(src);
    auto                      links = ts.GetAttributeLinks(tree.get(), src);
    REQUIRE(links.size() == 1);
    REQUIRE(links[0].attrName == "image");
    REQUIRE(links[0].value    == "/path/to/img.png");
}

TEST_CASE("document link: URL attribute extracted", "[link]") {
    const std::string         src = "digraph G { A [URL=\"https://example.com\"] }";
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(src);
    auto                      links = ts.GetAttributeLinks(tree.get(), src);
    REQUIRE(links.size() == 1);
    REQUIRE(links[0].attrName == "URL");
    REQUIRE(links[0].value    == "https://example.com");
}

TEST_CASE("document link: no links in plain graph", "[link]") {
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(kSrc);
    auto                      links = ts.GetAttributeLinks(tree.get(), kSrc);
    REQUIRE(links.empty());
}

// ── 语义诊断测试 ──────────────────────────────────────────────────────────────

TEST_CASE("diagnostics: undirected edge in digraph is error", "[diag]") {
    const std::string         src = "digraph G { A -- B }";
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(src);
    auto                      diags = ts.GetErrors(tree.get(), src);
    REQUIRE_FALSE(diags.empty());
    REQUIRE(diags[0].severity == lsp::DiagnosticSeverity::Error);
}

TEST_CASE("diagnostics: directed edge in undirected graph is error", "[diag]") {
    const std::string         src = "graph G { A -> B }";
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(src);
    auto                      diags = ts.GetErrors(tree.get(), src);
    REQUIRE_FALSE(diags.empty());
}

TEST_CASE("diagnostics: valid digraph has no errors", "[diag]") {
    infra::parser::TreeSitter ts;
    auto                      tree  = ts.Parse(kSrc);
    auto                      diags = ts.GetErrors(tree.get(), kSrc);
    REQUIRE(diags.empty());
}

