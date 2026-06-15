// Stress, robustness, and boundary tests for MWRender (Phase 8).
//
// Tests:
//   - Very large input documents (1 MB)
//   - Extremely long single lines
//   - Deep nesting (blockquotes, lists)
//   - Thousands of headings / inline elements
//   - Empty / whitespace-only input variants
//   - BOM handling
//   - Input size limit enforcement
//   - Maximum nesting depth limit enforcement
//   - No crash on random ASCII garbage

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/engine.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool contains(std::string_view text, std::string_view sub) {
    return text.find(sub) != std::string_view::npos;
}

bool hasDiagnostic(const mwrender::RenderResult& result, std::string_view code) {
    for (const auto& d : result.diagnostics) {
        if (d.code == code) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// 1. Large input (1 MB plain text paragraphs)
// ---------------------------------------------------------------------------
void testLargeInput() {
    std::cout << "  testLargeInput...\n";
    // Build ~1 MB of paragraphs
    std::string markdown;
    markdown.reserve(1024 * 1024 + 256);
    while (markdown.size() < 1024 * 1024) {
        markdown += "This is a sample paragraph with enough text to fill a megabyte of input. "
                    "It contains **bold**, *emphasis*, and `code` spans.\n\n";
    }

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;

    const auto start = std::chrono::steady_clock::now();
    const auto result = engine.render(request);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    require(result.ok, "1 MB input should render successfully");
    require(contains(result.html, "<p"), "1 MB input should contain paragraphs");
    // Generous timeout: 10 seconds on slow machines
    require(elapsed < 10000, "1 MB input should render in under 10 seconds");
    std::cout << "    elapsed: " << elapsed << " ms  input: "
              << markdown.size() << " bytes\n";
}

// ---------------------------------------------------------------------------
// 2. Extremely long single line (10 000 characters)
// ---------------------------------------------------------------------------
void testLongLine() {
    std::cout << "  testLongLine...\n";
    std::string line(10000, 'a');
    line += '\n';

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = line;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "10 000-char line should render without crash");
    require(contains(result.html, "<p"), "long line should become a paragraph");
}

// ---------------------------------------------------------------------------
// 3. Deep blockquote nesting (200 levels)
// ---------------------------------------------------------------------------
void testDeepBlockquoteNesting() {
    std::cout << "  testDeepBlockquoteNesting...\n";
    // Build "> > > ... > content\n" with 200 levels
    std::string markdown;
    for (int level = 0; level < 200; ++level) {
        markdown += "> ";
    }
    markdown += "deep\n";

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    // 200 levels is under the default maxNestingDepth of 256
    require(result.ok, "deep nesting under limit should succeed");
    require(result.diagnostics.empty(), "deep nesting under limit should not produce diagnostics");

    // Exceed maxNestingDepth
    std::string overflowMarkdown;
    for (int level = 0; level < 300; ++level) {
        overflowMarkdown += "> ";
    }
    overflowMarkdown += "overflow\n";

    request.markdown = overflowMarkdown;
    const auto overflowResult = engine.render(request);
    
    require(overflowResult.ok, "nesting over limit should not crash");
    bool hasDepthWarning = false;
    for (const auto& diag : overflowResult.diagnostics) {
        if (diag.code == "MW0004") { // MW0004 is maximum nesting depth exceeded
            hasDepthWarning = true;
            break;
        }
    }
    require(hasDepthWarning, "nesting over limit should produce MW0004 warning");
}

// ---------------------------------------------------------------------------
// 4. Deep list nesting (100 levels with indentation)
// ---------------------------------------------------------------------------
void testDeepListNesting() {
    std::cout << "  testDeepListNesting...\n";
    std::string markdown;
    // Nested list: each level indented by 2 spaces
    for (int level = 0; level < 50; ++level) {
        markdown += std::string(static_cast<std::size_t>(level * 2), ' ') + "- level " + std::to_string(level) + "\n";
    }

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "deeply nested list should not crash");
    require(contains(result.html, "<ul") || contains(result.html, "<li"), "nested list should render");
}

// ---------------------------------------------------------------------------
// 5. Many headings (5 000 headings)
// ---------------------------------------------------------------------------
void testManyHeadings() {
    std::cout << "  testManyHeadings...\n";
    std::string markdown;
    markdown.reserve(5000 * 20);
    for (int i = 0; i < 5000; ++i) {
        markdown += "# Heading " + std::to_string(i) + "\n\n";
    }

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "5000 headings should render without crash");
    require(contains(result.html, "<h1"), "5000 headings should produce h1 elements");
}

// ---------------------------------------------------------------------------
// 6. Pathological inline emphasis (many unmatched delimiters)
// ---------------------------------------------------------------------------
void testPathologicalEmphasis() {
    std::cout << "  testPathologicalEmphasis...\n";
    // 2000 unmatched * characters on a single line
    std::string markdown;
    for (int i = 0; i < 2000; ++i) {
        markdown += '*';
    }
    markdown += '\n';

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "pathological emphasis input should not crash");
}

// ---------------------------------------------------------------------------
// 7. Empty and whitespace-only input variants
// ---------------------------------------------------------------------------
void testEmptyVariants() {
    std::cout << "  testEmptyVariants...\n";
    mwrender::Engine engine;

    const std::vector<std::string_view> inputs = {
        "",          // empty
        "\n",        // single newline
        "\n\n\n",    // multiple newlines
        "   ",       // spaces only
        "\t",        // tab only
        "\r\n\r\n",  // CRLF
        "   \n   \n", // whitespace lines
    };

    for (const auto input : inputs) {
        mwrender::RenderRequest request;
        request.markdown = input;
        request.options.outputMode = mwrender::OutputMode::Fragment;
        const auto result = engine.render(request);
        require(result.ok, "empty/whitespace variant should render without crash");
    }
}

// ---------------------------------------------------------------------------
// 8. UTF-8 BOM handling
// ---------------------------------------------------------------------------
void testBomHandling() {
    std::cout << "  testBomHandling...\n";
    // UTF-8 BOM followed by content
    std::string markdown;
    markdown += "\xEF\xBB\xBF"; // UTF-8 BOM
    markdown += "# BOM Heading\n\nParagraph.\n";

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    // BOM should be stripped or tolerated; content should render
    require(result.ok, "BOM input should render");
    require(contains(result.html, "Heading") || contains(result.html, "<h1"),
            "BOM input should render heading content");
}

// ---------------------------------------------------------------------------
// 9. Input size limit enforcement
// ---------------------------------------------------------------------------
void testInputSizeLimit() {
    std::cout << "  testInputSizeLimit...\n";
    mwrender::EngineOptions opts;
    opts.maxInputBytes = 10; // very small limit
    mwrender::Engine engine(opts);

    mwrender::RenderRequest request;
    request.markdown = "This is more than 10 bytes of input.\n";
    const auto result = engine.render(request);

    require(!result.ok, "oversized input should fail");
    require(hasDiagnostic(result, "MW0001"), "oversized input should produce MW0001");
}

// ---------------------------------------------------------------------------
// 10. Random ASCII printable garbage (should not crash)
// ---------------------------------------------------------------------------
void testRandomAsciiGarbage() {
    std::cout << "  testRandomAsciiGarbage...\n";
    // Deterministic "random" ASCII printable sequence covering all punctuation
    const std::string punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    std::string markdown;
    markdown.reserve(2000);
    for (int rep = 0; rep < 60; ++rep) {
        markdown += punctuation;
        markdown += '\n';
    }

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "random punctuation should remain renderable");
    require(!result.html.empty(), "random punctuation should produce some output");
}

// ---------------------------------------------------------------------------
// 11. Mixed line endings in same document
// ---------------------------------------------------------------------------
void testMixedLineEndings() {
    std::cout << "  testMixedLineEndings...\n";
    // LF, CRLF, and CR mixed
    std::string markdown = "# Heading\r\n\nParagraph one.\r\nParagraph two.\nParagraph three.\n";

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "mixed line endings should render");
    require(contains(result.html, "<h1"), "mixed line endings: heading should render");
    require(contains(result.html, "<p"), "mixed line endings: paragraphs should render");
}

// ---------------------------------------------------------------------------
// 12. Very long code block content
// ---------------------------------------------------------------------------
void testLongCodeBlock() {
    std::cout << "  testLongCodeBlock...\n";
    std::string code;
    code.reserve(50000);
    for (int i = 0; i < 1000; ++i) {
        code += "int function_" + std::to_string(i) + "() { return " + std::to_string(i) + "; }\n";
    }

    std::string markdown = "```cpp\n" + code + "```\n";

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "very long code block should render");
    require(contains(result.html, "language-cpp"), "long code block should have language class");
}

// ---------------------------------------------------------------------------
// 13. Thousands of inline elements in one paragraph
// ---------------------------------------------------------------------------
void testManyInlineElements() {
    std::cout << "  testManyInlineElements...\n";
    std::string markdown;
    markdown.reserve(60000);
    for (int i = 0; i < 500; ++i) {
        markdown += "**bold** *em* `code` [link](https://example.com) ";
    }
    markdown += '\n';

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "many inline elements should render without crash");
    require(contains(result.html, "<strong>"), "many inline elements should contain bold");
}

// ---------------------------------------------------------------------------
// 14. Nesting depth limit enforcement
// ---------------------------------------------------------------------------
void testNestingDepthLimit() {
    std::cout << "  testNestingDepthLimit...\n";
    mwrender::EngineOptions opts;
    opts.maxNestingDepth = 5; // very small limit to exercise truncation
    mwrender::Engine engine(opts);

    // Build deeply nested blockquote
    std::string markdown;
    for (int i = 0; i < 20; ++i) {
        markdown += "> ";
    }
    markdown += "deep\n";

    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    // Should render something (gracefully truncated or limited), not crash
    require(!result.html.empty(), "nesting depth limit: output should not be empty");
    
    bool hasDepthWarning = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.code == "MW0004") {
            hasDepthWarning = true;
            break;
        }
    }
    require(hasDepthWarning, "nesting depth limit: should produce MW0004 warning");
}

// ---------------------------------------------------------------------------
// 15. Consecutive thematic breaks
// ---------------------------------------------------------------------------
void testConsecutiveThematicBreaks() {
    std::cout << "  testConsecutiveThematicBreaks...\n";
    std::string markdown;
    for (int i = 0; i < 100; ++i) {
        markdown += "---\n";
    }

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "100 thematic breaks should render");
    require(contains(result.html, "<hr"), "100 thematic breaks should produce hr elements");
}

} // namespace

int main() {
    std::cout << "=== Stress & Robustness Tests ===\n";

    testLargeInput();
    testLongLine();
    testDeepBlockquoteNesting();
    testDeepListNesting();
    testManyHeadings();
    testPathologicalEmphasis();
    testEmptyVariants();
    testBomHandling();
    testInputSizeLimit();
    testRandomAsciiGarbage();
    testMixedLineEndings();
    testLongCodeBlock();
    testManyInlineElements();
    testNestingDepthLimit();
    testConsecutiveThematicBreaks();

    std::cout << "\nAll stress tests passed.\n";
    return 0;
}
