// CommonMark 0.31.2 & GFM conformance tests for MWRender.
//
// Each test case is labeled with one of:
//   PASS            – this implementation is expected to pass
//   EXPECTED_FAIL   – spec-correct output differs, tracked deviation
//   NOT_SUPPORTED   – feature not in v0.1 scope
//
// Results are tallied and printed as a compatibility report.
// The executable exits 0 even when expected-fail/not-supported cases occur;
// it exits 1 only when a PASS case fails.

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/engine.hpp>

namespace {

enum class Expectation {
    Pass,
    ExpectedFail,
    NotSupported,
};

struct ConformanceCase {
    std::string_view id;
    std::string_view description;
    std::string_view markdown;
    std::string_view expectedFragment; // sub-string that must appear in output
    std::string_view forbiddenFragment; // sub-string that must NOT appear (empty = skip)
    Expectation expectation = Expectation::Pass;
};

bool contains(std::string_view text, std::string_view sub) {
    return !sub.empty() && text.find(sub) != std::string_view::npos;
}

struct Stats {
    int total = 0;
    int passed = 0;
    int failedUnexpected = 0;
    int expectedFail = 0;
    int notSupported = 0;
};

void runCase(
    mwrender::Engine& engine,
    const ConformanceCase& tc,
    Stats& stats) {
    ++stats.total;

    mwrender::RenderRequest request;
    request.markdown = tc.markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    request.options.sourceMap = mwrender::SourceMapMode::None;
    const auto result = engine.render(request);

    const bool fragmentOk =
        result.ok &&
        (tc.expectedFragment.empty() || contains(result.html, tc.expectedFragment)) &&
        (tc.forbiddenFragment.empty() || !contains(result.html, tc.forbiddenFragment));

    switch (tc.expectation) {
    case Expectation::Pass:
        if (fragmentOk) {
            ++stats.passed;
            std::cout << "  [PASS]     " << tc.id << ": " << tc.description << '\n';
        } else {
            ++stats.failedUnexpected;
            std::cerr << "  [FAIL]     " << tc.id << ": " << tc.description << '\n';
            std::cerr << "             expected fragment: \"" << tc.expectedFragment << "\"\n";
            if (!tc.forbiddenFragment.empty()) {
                std::cerr << "             forbidden fragment: \"" << tc.forbiddenFragment << "\"\n";
            }
            std::cerr << "             actual output:\n" << result.html << '\n';
        }
        break;
    case Expectation::ExpectedFail:
        if (!fragmentOk) {
            ++stats.expectedFail;
            std::cout << "  [EXP-FAIL] " << tc.id << ": " << tc.description << '\n';
        } else {
            // Unexpectedly passing – count as pass and note it
            ++stats.passed;
            std::cout << "  [PASS*]    " << tc.id << ": " << tc.description
                      << " (was expected-fail, now passing)\n";
        }
        break;
    case Expectation::NotSupported:
        ++stats.notSupported;
        std::cout << "  [N/S]      " << tc.id << ": " << tc.description << '\n';
        break;
    }
}

// ---------------------------------------------------------------------------
// CommonMark spec cases
// ---------------------------------------------------------------------------

const std::vector<ConformanceCase> kAtxHeadings = {
    // CM spec §4.2
    {"CM-ATX-001", "level 1 heading",
        "# foo\n", "<h1", "", Expectation::Pass},
    {"CM-ATX-002", "level 2 heading",
        "## foo\n", "<h2", "", Expectation::Pass},
    {"CM-ATX-003", "level 6 heading",
        "###### foo\n", "<h6", "", Expectation::Pass},
    {"CM-ATX-004", "7 hashes not a heading → paragraph",
        "####### foo\n", "<p", "<h7", Expectation::Pass},
    {"CM-ATX-005", "heading with trailing hashes stripped",
        "# foo ##\n", "<h1", "##", Expectation::Pass},
    {"CM-ATX-006", "empty heading allowed",
        "#\n", "<h1", "", Expectation::Pass},
    {"CM-ATX-007", "heading with leading spaces (up to 3)",
        "   # foo\n", "<h1", "", Expectation::Pass},
    {"CM-ATX-008", "4 leading spaces → not a heading (code block or para)",
        "    # foo\n", "", "<h1", Expectation::Pass},
};

const std::vector<ConformanceCase> kParagraphs = {
    // CM spec §4.8
    {"CM-PARA-001", "simple paragraph",
        "hello world\n", "<p", "", Expectation::Pass},
    {"CM-PARA-002", "paragraph with blank line",
        "foo\n\nbar\n", "<p", "", Expectation::Pass},
    {"CM-PARA-003", "paragraph continues over soft break",
        "foo\nbar\n", "<p", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kFencedCode = {
    // CM spec §4.5
    {"CM-FENCE-001", "basic backtick fence",
        "```\ncode\n```\n", "<pre", "", Expectation::Pass},
    {"CM-FENCE-002", "fence with info string",
        "```python\nprint()\n```\n", "language-python", "", Expectation::Pass},
    {"CM-FENCE-003", "tilde fence",
        "~~~\ncode\n~~~\n", "<pre", "", Expectation::Pass},
    {"CM-FENCE-004", "unclosed fence extends to EOF",
        "```\ncontent\n", "<pre", "", Expectation::Pass},
    {"CM-FENCE-005", "nested backtick in content",
        "```\nfoo`bar\n```\n", "foo`bar", "", Expectation::Pass},
    {"CM-FENCE-006", "fence with longer closing marker OK",
        "```\ncode\n````\n", "<pre", "", Expectation::Pass},
    {"CM-FENCE-007", "info string controls language class",
        "```cpp\nint x;\n```\n", "language-cpp", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kBlockquotes = {
    // CM spec §5.1
    {"CM-BQ-001", "simple blockquote",
        "> foo\n", "<blockquote", "", Expectation::Pass},
    {"CM-BQ-002", "nested blockquote",
        "> > foo\n", "<blockquote", "", Expectation::Pass},
    {"CM-BQ-003", "blockquote with multiple lines",
        "> foo\n> bar\n", "<blockquote", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kLists = {
    // CM spec §5.2/5.3
    {"CM-LIST-001", "unordered list with dash",
        "- foo\n- bar\n", "<ul", "", Expectation::Pass},
    {"CM-LIST-002", "unordered list with star",
        "* foo\n* bar\n", "<ul", "", Expectation::Pass},
    {"CM-LIST-003", "unordered list with plus",
        "+ foo\n+ bar\n", "<ul", "", Expectation::Pass},
    {"CM-LIST-004", "ordered list starting at 1",
        "1. foo\n2. bar\n", "<ol", "", Expectation::Pass},
    {"CM-LIST-005", "ordered list starting at 3",
        "3. foo\n4. bar\n", "start=\"3\"", "", Expectation::Pass},
    {"CM-LIST-006", "nested lists",
        "- foo\n  - bar\n", "<ul", "", Expectation::Pass},
    {"CM-LIST-007", "list item content as paragraph in loose list",
        "- foo\n\n- bar\n", "<ul", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kThematicBreaks = {
    // CM spec §4.1
    {"CM-HR-001", "three dashes",
        "---\n", "<hr", "", Expectation::Pass},
    {"CM-HR-002", "three stars",
        "***\n", "<hr", "", Expectation::Pass},
    {"CM-HR-003", "three underscores",
        "___\n", "<hr", "", Expectation::Pass},
    {"CM-HR-004", "more than three dashes",
        "-----\n", "<hr", "", Expectation::Pass},
    {"CM-HR-005", "spaces between dashes allowed",
        "- - -\n", "<hr", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kEmphasis = {
    // CM spec §6.4
    {"CM-EM-001", "star emphasis",
        "*foo*\n", "<em>foo</em>", "", Expectation::Pass},
    {"CM-EM-002", "underscore emphasis",
        "_foo_\n", "<em>foo</em>", "", Expectation::Pass},
    {"CM-EM-003", "strong with stars",
        "**foo**\n", "<strong>foo</strong>", "", Expectation::Pass},
    {"CM-EM-004", "strong with underscores",
        "__foo__\n", "<strong>foo</strong>", "", Expectation::Pass},
    {"CM-EM-005", "nested emphasis",
        "*foo **bar** baz*\n", "<em>", "", Expectation::Pass},
    {"CM-EM-006", "emphasis not closed stays as text",
        "*unclosed\n", "*unclosed", "<em>", Expectation::Pass},
};

const std::vector<ConformanceCase> kInlineCode = {
    // CM spec §6.1
    {"CM-CODE-001", "single backtick code span",
        "`code`\n", "<code", "", Expectation::Pass},
    {"CM-CODE-002", "double backtick code span",
        "``foo ` bar``\n", "foo ` bar", "", Expectation::Pass},
    {"CM-CODE-003", "HTML inside code span is escaped",
        "`<div>`\n", "&lt;div&gt;", "", Expectation::Pass},
    {"CM-CODE-004", "backtick in code span preserved",
        "``foo`bar``\n", "foo`bar", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kLinks = {
    // CM spec §6.6
    {"CM-LINK-001", "simple inline link",
        "[foo](https://example.com)\n", "href=\"https://example.com\"", "", Expectation::Pass},
    {"CM-LINK-002", "link with title",
        "[foo](https://example.com \"title\")\n", "title=\"title\"", "", Expectation::Pass},
    {"CM-LINK-003", "relative link",
        "[foo](./page.html)\n", "href=\"./page.html\"", "", Expectation::Pass},
    {"CM-LINK-004", "fragment link",
        "[foo](#section)\n", "href=\"#section\"", "", Expectation::Pass},
    {"CM-LINK-005", "link with mailto",
        "[email](mailto:test@example.com)\n", "href=\"mailto:test@example.com\"", "", Expectation::Pass},
    {"CM-LINK-006", "javascript link is removed",
        "[bad](javascript:alert(1))\n", "", "href=\"javascript:", Expectation::Pass},
    {"CM-LINK-007", "link reference (not supported in v0.1)",
        "[foo][ref]\n\n[ref]: https://example.com\n",
        "", "", Expectation::NotSupported},
};

const std::vector<ConformanceCase> kImages = {
    // CM spec §6.7
    {"CM-IMG-001", "inline image",
        "![alt](./image.png)\n", "src=\"./image.png\"", "", Expectation::Pass},
    {"CM-IMG-002", "image with title",
        "![alt](./img.png \"t\")\n", "title=\"t\"", "", Expectation::Pass},
    {"CM-IMG-003", "image alt is attribute-escaped",
        "![a\"b](./x.png)\n", "alt=\"a&quot;b\"", "", Expectation::Pass},
    {"CM-IMG-004", "unsafe image URL is removed",
        "![bad](javascript:alert(1))\n", "", "src=\"javascript:", Expectation::Pass},
};

const std::vector<ConformanceCase> kHardSoftBreak = {
    // CM spec §6.12
    {"CM-BREAK-001", "soft break within paragraph",
        "foo\nbar\n", "<p", "", Expectation::Pass},
    {"CM-BREAK-002", "hard break with trailing spaces",
        "foo  \nbar\n", "<br>", "", Expectation::Pass},
    {"CM-BREAK-003", "backslash hard break",
        "foo\\\nbar\n", "<br>", "", Expectation::ExpectedFail},
};

const std::vector<ConformanceCase> kRawHtml = {
    // CM spec §4.6 / 6.11
    {"CM-HTML-001", "raw HTML block is escaped in Disabled mode",
        "<div>foo</div>\n", "&lt;div&gt;", "<div>foo</div>", Expectation::Pass},
    {"CM-HTML-002", "raw HTML inline is escaped in Disabled mode",
        "foo <em>bar</em> baz\n", "&lt;em&gt;", "<em>bar", Expectation::Pass},
    {"CM-HTML-003", "raw HTML passes through in Trusted mode",
        "<mark>trusted</mark>\n", "<mark>trusted</mark>", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kSlugAndId = {
    {"CM-SLUG-001", "heading generates id attribute",
        "# Hello World\n", "id=\"hello-world\"", "", Expectation::Pass},
    {"CM-SLUG-002", "duplicate headings get deduplicated ids",
        "# Foo\n# Foo\n", "id=\"foo-1\"", "", Expectation::Pass},
    {"CM-SLUG-003", "Unicode letters preserved in slug",
        "# 标题\n", "id=\"", "", Expectation::Pass},
    {"CM-SLUG-004", "empty heading uses 'section' slug",
        "# \n", "id=\"section\"", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kEscaping = {
    // CM spec §2.4
    {"CM-ESC-001", "backslash escapes punctuation",
        "\\*not emphasis\\*\n", "*not emphasis*", "<em>", Expectation::Pass},
    {"CM-ESC-002", "backslash before non-punctuation is literal",
        "\\a\n", "\\a", "", Expectation::Pass},
    {"CM-ESC-003", "HTML entities are preserved as text",
        "&amp;\n", "&amp;amp;", "", Expectation::ExpectedFail}, // entity not decoded
};

// ---------------------------------------------------------------------------
// GFM extension cases
// ---------------------------------------------------------------------------

const std::vector<ConformanceCase> kGfmTables = {
    {"GFM-TABLE-001", "basic table",
        "| A | B |\n| - | - |\n| 1 | 2 |\n",
        "<table", "", Expectation::Pass},
    {"GFM-TABLE-002", "table with alignment",
        "| Left | Right |\n| :--- | ---: |\n| a | b |\n",
        "align=\"right\"", "", Expectation::Pass},
    {"GFM-TABLE-003", "table header rendered as th",
        "| H |\n| - |\n| D |\n",
        "<th>", "", Expectation::Pass},
    {"GFM-TABLE-004", "table body rendered as td",
        "| H |\n| - |\n| D |\n",
        "<td>", "", Expectation::Pass},
    {"GFM-TABLE-005", "disabled tables fall back to text",
        "| A |\n| - |\n", "", "<table", Expectation::Pass},
};

const std::vector<ConformanceCase> kGfmTaskLists = {
    {"GFM-TASK-001", "checked task",
        "- [x] done\n", "checked", "", Expectation::Pass},
    {"GFM-TASK-002", "unchecked task",
        "- [ ] todo\n", "type=\"checkbox\"", "", Expectation::Pass},
    {"GFM-TASK-003", "task list class applied",
        "- [x] done\n", "mw-task-list", "", Expectation::Pass},
};

const std::vector<ConformanceCase> kGfmStrikethrough = {
    {"GFM-STRIKE-001", "double tilde strikethrough",
        "~~deleted~~\n", "<del>deleted</del>", "", Expectation::Pass},
    {"GFM-STRIKE-002", "single tilde is not strikethrough",
        "~not~\n", "~not~", "<del>", Expectation::Pass},
    {"GFM-STRIKE-003", "strikethrough disabled falls back to text",
        "~~deleted~~\n", "~~deleted~~", "<del>", Expectation::Pass},
};

const std::vector<ConformanceCase> kGfmAutoLinks = {
    {"GFM-AUTO-001", "http autolink",
        "<https://example.com>\n", "href=\"https://example.com\"", "", Expectation::Pass},
    {"GFM-AUTO-002", "mailto autolink",
        "<mailto:test@example.com>\n", "href=\"mailto:test@example.com\"", "", Expectation::Pass},
    {"GFM-AUTO-003", "autolink gets mw-autolink class",
        "<https://example.com>\n", "mw-autolink", "", Expectation::Pass},
};

// ---------------------------------------------------------------------------
// Setext headings (CM §4.3) – not supported
// ---------------------------------------------------------------------------
const std::vector<ConformanceCase> kSetextHeadings = {
    {"CM-SETEXT-001", "setext h1 (not supported)",
        "Foo\n===\n", "", "", Expectation::NotSupported},
    {"CM-SETEXT-002", "setext h2 (not supported)",
        "Bar\n---\n", "", "", Expectation::NotSupported},
};

// ---------------------------------------------------------------------------
// Indented code blocks (CM §4.4) – not supported
// ---------------------------------------------------------------------------
const std::vector<ConformanceCase> kIndentedCode = {
    {"CM-INDENT-001", "indented code block (not supported)",
        "    code\n", "", "", Expectation::NotSupported},
};

// ---------------------------------------------------------------------------
// Link reference definitions – not supported
// ---------------------------------------------------------------------------
const std::vector<ConformanceCase> kLinkRefs = {
    {"CM-LINKREF-001", "link reference definition (not supported)",
        "[foo]: /url\n\n[foo]\n", "", "", Expectation::NotSupported},
};

// ---------------------------------------------------------------------------
// HTML entities (CM §2.5) – partial support
// ---------------------------------------------------------------------------
const std::vector<ConformanceCase> kHtmlEntities = {
    {"CM-ENT-001", "named entity &amp; (passthrough as text)",
        "&amp;\n", "&amp;", "", Expectation::ExpectedFail},
    {"CM-ENT-002", "numeric entity &#42; (passthrough as text)",
        "&#42;\n", "&#42;", "", Expectation::NotSupported},
};

void runGroup(
    mwrender::Engine& engine,
    const std::string& groupName,
    const std::vector<ConformanceCase>& cases,
    Stats& stats) {
    std::cout << "\n=== " << groupName << " ===\n";
    for (const auto& tc : cases) {
        // For GFM table disabled test, use a different request
        if (tc.id == std::string_view("GFM-TABLE-005")) {
            mwrender::RenderRequest req;
            req.markdown = tc.markdown;
            req.options.outputMode = mwrender::OutputMode::Fragment;
            req.options.sourceMap = mwrender::SourceMapMode::None;
            req.options.extensions.tables = false;
            ++stats.total;
            const auto result = engine.render(req);
            const bool ok = result.ok && !contains(result.html, "<table");
            if (ok) {
                ++stats.passed;
                std::cout << "  [PASS]     " << tc.id << ": " << tc.description << '\n';
            } else {
                ++stats.failedUnexpected;
                std::cerr << "  [FAIL]     " << tc.id << ": " << tc.description << '\n';
            }
            continue;
        }
        // GFM-STRIKE-003 – test with strikethrough disabled
        if (tc.id == std::string_view("GFM-STRIKE-003")) {
            mwrender::RenderRequest req;
            req.markdown = tc.markdown;
            req.options.outputMode = mwrender::OutputMode::Fragment;
            req.options.sourceMap = mwrender::SourceMapMode::None;
            req.options.extensions.strikethrough = false;
            ++stats.total;
            const auto result = engine.render(req);
            const bool ok = result.ok
                && contains(result.html, "~~deleted~~")
                && !contains(result.html, "<del>");
            if (ok) {
                ++stats.passed;
                std::cout << "  [PASS]     " << tc.id << ": " << tc.description << '\n';
            } else {
                ++stats.failedUnexpected;
                std::cerr << "  [FAIL]     " << tc.id << ": " << tc.description << '\n';
            }
            continue;
        }
        // CM-HTML-003 – trusted HTML mode
        if (tc.id == std::string_view("CM-HTML-003")) {
            mwrender::RenderRequest req;
            req.markdown = tc.markdown;
            req.options.outputMode = mwrender::OutputMode::Fragment;
            req.options.sourceMap = mwrender::SourceMapMode::None;
            req.options.htmlPolicy = mwrender::HtmlPolicy::Trusted;
            ++stats.total;
            const auto result = engine.render(req);
            const bool ok = result.ok && contains(result.html, "<mark>trusted</mark>");
            if (ok) {
                ++stats.passed;
                std::cout << "  [PASS]     " << tc.id << ": " << tc.description << '\n';
            } else {
                ++stats.failedUnexpected;
                std::cerr << "  [FAIL]     " << tc.id << ": " << tc.description << '\n';
            }
            continue;
        }
        runCase(engine, tc, stats);
    }
}

} // namespace

int main() {
    mwrender::Engine engine;
    Stats stats;

    runGroup(engine, "ATX Headings (CM §4.2)", kAtxHeadings, stats);
    runGroup(engine, "Paragraphs (CM §4.8)", kParagraphs, stats);
    runGroup(engine, "Fenced Code Blocks (CM §4.5)", kFencedCode, stats);
    runGroup(engine, "Blockquotes (CM §5.1)", kBlockquotes, stats);
    runGroup(engine, "Lists (CM §5.2-5.3)", kLists, stats);
    runGroup(engine, "Thematic Breaks (CM §4.1)", kThematicBreaks, stats);
    runGroup(engine, "Emphasis & Strong (CM §6.4)", kEmphasis, stats);
    runGroup(engine, "Inline Code (CM §6.1)", kInlineCode, stats);
    runGroup(engine, "Links (CM §6.6)", kLinks, stats);
    runGroup(engine, "Images (CM §6.7)", kImages, stats);
    runGroup(engine, "Soft/Hard Breaks (CM §6.12)", kHardSoftBreak, stats);
    runGroup(engine, "Raw HTML (CM §4.6/6.11)", kRawHtml, stats);
    runGroup(engine, "Heading Slugs & IDs", kSlugAndId, stats);
    runGroup(engine, "Backslash Escaping (CM §2.4)", kEscaping, stats);
    runGroup(engine, "GFM Tables", kGfmTables, stats);
    runGroup(engine, "GFM Task Lists", kGfmTaskLists, stats);
    runGroup(engine, "GFM Strikethrough", kGfmStrikethrough, stats);
    runGroup(engine, "GFM Autolinks", kGfmAutoLinks, stats);
    runGroup(engine, "Setext Headings (not supported)", kSetextHeadings, stats);
    runGroup(engine, "Indented Code Blocks (not supported)", kIndentedCode, stats);
    runGroup(engine, "Link Reference Definitions (not supported)", kLinkRefs, stats);
    runGroup(engine, "HTML Entities (partial)", kHtmlEntities, stats);

    std::cout << "\n===================================================\n";
    std::cout << "CommonMark/GFM Conformance Report\n";
    std::cout << "===================================================\n";
    std::cout << "  Total cases:      " << stats.total << '\n';
    std::cout << "  Passed:           " << stats.passed << '\n';
    std::cout << "  Expected fail:    " << stats.expectedFail << '\n';
    std::cout << "  Not supported:    " << stats.notSupported << '\n';
    std::cout << "  Unexpected fail:  " << stats.failedUnexpected << '\n';
    std::cout << "===================================================\n";

    if (stats.failedUnexpected > 0) {
        std::cerr << stats.failedUnexpected
                  << " PASS case(s) failed. See output above.\n";
        return 1;
    }
    return 0;
}
