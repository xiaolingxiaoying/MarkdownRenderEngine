// Security regression tests for MWRender (Phase 8).
//
// Covers all XSS / injection / path-traversal vectors described in the
// development guide §7, §8, §9, §10, and §15.4.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/engine.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool contains(std::string_view text, std::string_view sub) {
    return !sub.empty() && text.find(sub) != std::string_view::npos;
}

bool hasDiagnostic(const mwrender::RenderResult& result, std::string_view code) {
    for (const auto& d : result.diagnostics) {
        if (d.code == code) {
            return true;
        }
    }
    return false;
}

mwrender::RenderResult renderFragment(
    mwrender::Engine& engine,
    std::string_view markdown,
    mwrender::HtmlPolicy policy = mwrender::HtmlPolicy::Disabled) {
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    request.options.htmlPolicy = policy;
    return engine.render(request);
}

// ---------------------------------------------------------------------------
// 1. Script injection via raw HTML block
// ---------------------------------------------------------------------------
void testScriptHtmlBlock() {
    std::cout << "  testScriptHtmlBlock...\n";
    mwrender::Engine engine;

    const std::string markdown = "<script>alert(1)</script>\n";
    const auto result = renderFragment(engine, markdown);

    require(result.ok, "script html block should render (disabled mode)");
    require(!contains(result.html, "<script>"),
            "script tag must not appear in disabled HTML output");
    require(contains(result.html, "&lt;script&gt;"),
            "script tag must be escaped as text in disabled HTML output");
}

// ---------------------------------------------------------------------------
// 2. Script injection via event handler attribute
// ---------------------------------------------------------------------------
void testEventHandlerAttribute() {
    std::cout << "  testEventHandlerAttribute...\n";
    mwrender::Engine engine;

    const std::string markdown = "<img src=x onerror=alert(1)>\n";
    const auto result = renderFragment(engine, markdown);

    require(result.ok, "event handler html should render (disabled mode)");
    // In disabled mode, raw HTML is escaped as text – the tag itself must not appear
    // in executable form. Check the < is escaped so the tag can't run.
    require(!contains(result.html, "<img "),
            "img tag must not appear unescaped in disabled output");
    require(contains(result.html, "&lt;img"),
            "img tag must appear as escaped text in disabled output");
}

// ---------------------------------------------------------------------------
// 3. Script in SVG block
// ---------------------------------------------------------------------------
void testSvgScriptBlock() {
    std::cout << "  testSvgScriptBlock...\n";
    mwrender::Engine engine;

    const std::string markdown = "<svg><script>alert(1)</script></svg>\n";
    const auto result = renderFragment(engine, markdown);

    require(result.ok, "SVG script should render in disabled mode");
    require(!contains(result.html, "<script>"),
            "script inside SVG must not appear in disabled output");
}

// ---------------------------------------------------------------------------
// 4. javascript: URL in link
// ---------------------------------------------------------------------------
void testJavascriptLinkUrl() {
    std::cout << "  testJavascriptLinkUrl...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "[click](javascript:alert(1))\n");

    require(result.ok, "javascript link should render (with URL removed)");
    require(!contains(result.html, "href=\"javascript:"),
            "javascript: URL must be stripped from link href");
    require(hasDiagnostic(result, "MW3002"), "unsafe link should produce MW3002");
}

// ---------------------------------------------------------------------------
// 5. javascript: URL with uppercase
// ---------------------------------------------------------------------------
void testJavascriptUrlUppercase() {
    std::cout << "  testJavascriptUrlUppercase...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "[bad](JAVASCRIPT:alert(1))\n");

    require(!contains(result.html, "href=\"JAVASCRIPT:"),
            "uppercase JAVASCRIPT: URL must be stripped");
}

// ---------------------------------------------------------------------------
// 6. javascript: URL with mixed case
// ---------------------------------------------------------------------------
void testJavascriptUrlMixedCase() {
    std::cout << "  testJavascriptUrlMixedCase...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "[bad](JaVaScRiPt:alert(1))\n");

    require(!contains(result.html, "href=\"JaVaScRiPt:"),
            "mixed-case JavaScript URL must be stripped");
}

// ---------------------------------------------------------------------------
// 7. javascript: URL with whitespace around scheme
// ---------------------------------------------------------------------------
void testJavascriptUrlWhitespace() {
    std::cout << "  testJavascriptUrlWhitespace...\n";
    mwrender::Engine engine;

    // Leading whitespace before the scheme
    const auto result = renderFragment(engine, "[bad]( javascript:alert(1))\n");

    require(!contains(result.html, "javascript:"),
            "whitespace-prefixed javascript: URL must be stripped");
}

// ---------------------------------------------------------------------------
// 8. vbscript: URL
// ---------------------------------------------------------------------------
void testVbscriptUrl() {
    std::cout << "  testVbscriptUrl...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "[bad](vbscript:msgbox(1))\n");

    require(!contains(result.html, "href=\"vbscript:"),
            "vbscript: URL must be stripped from link");
}

// ---------------------------------------------------------------------------
// 9. data: URL in link (should be blocked by default)
// ---------------------------------------------------------------------------
void testDataUrlInLink() {
    std::cout << "  testDataUrlInLink...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "[data](data:text/html,<h1>XSS</h1>)\n");

    require(!contains(result.html, "href=\"data:"),
            "data: URL must be stripped from link");
}

// ---------------------------------------------------------------------------
// 10. data: URL in image (should be blocked by default)
// ---------------------------------------------------------------------------
void testDataUrlInImage() {
    std::cout << "  testDataUrlInImage...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "![alt](data:image/png;base64,abc)\n");

    require(!contains(result.html, "src=\"data:"),
            "data: URL must be stripped from image src");
}

// ---------------------------------------------------------------------------
// 11. javascript: URL in image
// ---------------------------------------------------------------------------
void testJavascriptUrlInImage() {
    std::cout << "  testJavascriptUrlInImage...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "![alt](javascript:alert(1))\n");

    require(!contains(result.html, "src=\"javascript:"),
            "javascript: URL must be stripped from image src");
    require(hasDiagnostic(result, "MW3003"), "unsafe image should produce MW3003");
}

// ---------------------------------------------------------------------------
// 12. Injection via link title attribute (attribute-escaping verification)
// ---------------------------------------------------------------------------
void testLinkTitleInjection() {
    std::cout << "  testLinkTitleInjection...\n";
    mwrender::Engine engine;

    // Title contains characters that could break attribute context if not escaped
    // Use single-quote-free title (double-quote inside gets attribute-escaped)
    const auto result = renderFragment(engine,
        "[foo](https://example.com \"title with &amp; and special chars\")\n");

    // If title appears, it must be attribute-escaped
    // &amp; in source → &amp;amp; in attribute (double-escaped)
    // The key check: no raw & that would break attribute parsing
    require(!contains(result.html, "onmouseover="),
            "no event handler attribute must appear in output");
    // Verify escaping happened if title was parsed
    if (contains(result.html, "title=")) {
        require(!contains(result.html, "title=\"title with &amp; and special chars\""),
                "unescaped ampersand must not appear raw in attribute");
    }
}

// ---------------------------------------------------------------------------
// 13. Injection via heading text (HTML entities in attribute context)
// ---------------------------------------------------------------------------
void testHeadingAttrInjection() {
    std::cout << "  testHeadingAttrInjection...\n";
    mwrender::Engine engine;

    // Heading whose text includes characters that could break an attribute
    const auto result = renderFragment(engine, "# Heading<script>alert(1)</script>\n");

    require(!contains(result.html, "<script>"),
            "script in heading must be escaped in attribute context");
}

// ---------------------------------------------------------------------------
// 14. Code span should never render HTML
// ---------------------------------------------------------------------------
void testCodeSpanHtmlEscape() {
    std::cout << "  testCodeSpanHtmlEscape...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine, "`<script>alert(1)</script>`\n");

    require(!contains(result.html, "<script>"),
            "code span must HTML-escape its content");
    require(contains(result.html, "&lt;script&gt;"),
            "code span content must be escaped as &lt;script&gt;");
}

// ---------------------------------------------------------------------------
// 15. Code block should never render HTML
// ---------------------------------------------------------------------------
void testCodeBlockHtmlEscape() {
    std::cout << "  testCodeBlockHtmlEscape...\n";
    mwrender::Engine engine;

    const auto result = renderFragment(engine,
        "```\n<script>alert(1)</script>\n```\n");

    require(!contains(result.html, "<script>"),
            "code block must HTML-escape its content");
    require(contains(result.html, "&lt;script&gt;"),
            "code block content must be escaped as &lt;script&gt;");
}

// ---------------------------------------------------------------------------
// 16. Fenced code info string injection
// ---------------------------------------------------------------------------
void testCodeInfoStringInjection() {
    std::cout << "  testCodeInfoStringInjection...\n";
    mwrender::Engine engine;

    // Info string that tries to break out of data-lang attribute
    const auto result = renderFragment(engine,
        "```cpp\" onmouseover=\"alert(1)\ncode\n```\n");

    require(!contains(result.html, "onmouseover="),
            "code info string injection must be sanitized");
}

// ---------------------------------------------------------------------------
// 17. CSS @import injection via request CSS text
// ---------------------------------------------------------------------------
void testCssImportInjection() {
    std::cout << "  testCssImportInjection...\n";
    mwrender::Engine engine;

    mwrender::RenderRequest request;
    request.markdown = "# Test\n";
    request.options.outputMode = mwrender::OutputMode::FullDocument;
    request.css.text.push_back("@import url(https://evil.example.com/style.css);");

    const auto result = engine.render(request);

    require(result.ok, "CSS @import rejection should be non-fatal");
    require(hasDiagnostic(result, "MW4002"),
            "CSS @import via request must produce MW4002");
    require(!contains(result.css, "evil.example.com"),
            "malicious CSS @import must be stripped from output");
}

// ---------------------------------------------------------------------------
// 18. CSS remote url() injection
// ---------------------------------------------------------------------------
void testCssRemoteUrlInjection() {
    std::cout << "  testCssRemoteUrlInjection...\n";
    mwrender::Engine engine;

    mwrender::RenderRequest request;
    request.markdown = "# Test\n";
    request.options.outputMode = mwrender::OutputMode::FullDocument;
    request.css.text.push_back(".x { background: url(https://evil.com/track.gif); }");

    const auto result = engine.render(request);

    require(result.ok, "CSS remote url() rejection should be non-fatal");
    require(hasDiagnostic(result, "MW4002"),
            "CSS remote url() must produce MW4002");
    require(!contains(result.css, "evil.com"),
            "remote CSS url() must not appear in composed CSS");
}

// ---------------------------------------------------------------------------
// 19. CSS protocol-relative url() injection
// ---------------------------------------------------------------------------
void testCssProtocolRelativeUrlInjection() {
    std::cout << "  testCssProtocolRelativeUrlInjection...\n";
    mwrender::Engine engine;

    mwrender::RenderRequest request;
    request.markdown = "# Test\n";
    request.options.outputMode = mwrender::OutputMode::FullDocument;
    request.css.text.push_back(".x { background: url(//evil.com/track.gif); }");

    const auto result = engine.render(request);

    require(!contains(result.css, "evil.com"),
            "protocol-relative CSS url() must not appear in composed CSS");
}

// ---------------------------------------------------------------------------
// 20. Theme entry path traversal
// ---------------------------------------------------------------------------
void writeFile(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
    if (!out) {
        std::cerr << "Could not write test fixture: " << path.string() << '\n';
        std::exit(1);
    }
}

void testThemePathTraversal() {
    std::cout << "  testThemePathTraversal...\n";
    const auto root =
        std::filesystem::temp_directory_path() / "mwrender-security-theme";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "traversal-theme");

    // Create a "secret" file outside the theme directory
    writeFile(root / "secret.css", ".secret { color: red; }");

    // Theme that tries to reference ../secret.css
    writeFile(root / "traversal-theme" / "theme.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"id\": \"traversal-theme\",\n"
        "  \"name\": \"Traversal Theme\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"appearance\": \"light\",\n"
        "  \"entry\": { \"content\": \"../secret.css\" }\n"
        "}\n");

    mwrender::Engine engine;
    engine.addThemeRoot(root);

    mwrender::RenderRequest request;
    request.markdown = "# Test\n";
    request.options.themeId = "traversal-theme";
    const auto result = engine.render(request);

    // The traversal should be rejected; theme should fall back or fail
    require(
        !contains(result.css, ".secret"),
        "path traversal in theme entry must not expose files outside theme root");

    std::filesystem::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// 21. Document CSS path traversal
// ---------------------------------------------------------------------------
void testDocumentCssPathTraversal() {
    std::cout << "  testDocumentCssPathTraversal...\n";
    const auto root =
        std::filesystem::temp_directory_path() / "mwrender-security-doccss";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "docs");

    writeFile(root / "secret.css", ".secret { color: red; }");
    writeFile(root / "docs" / "doc.md",
        "---\n"
        "css:\n"
        "  - ../../secret.css\n"
        "---\n"
        "# Doc\n");

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = "---\ncss:\n  - ../../secret.css\n---\n# Doc\n";
    request.sourcePath = root / "docs" / "doc.md";
    request.options.allowDocumentCss = true;

    const auto result = engine.render(request);

    require(
        !contains(result.css, ".secret"),
        "path traversal in document CSS must be rejected");

    std::filesystem::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// 22. Absolute document CSS path rejection
// ---------------------------------------------------------------------------
void testAbsoluteDocumentCssRejection() {
    std::cout << "  testAbsoluteDocumentCssRejection...\n";
    const auto root =
        std::filesystem::temp_directory_path() / "mwrender-security-abscss";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    writeFile(root / "secret.css", ".secret { color: red; }");
    const auto secretPath = std::filesystem::canonical(root / "secret.css", ec);

    mwrender::Engine engine;
    // Compose markdown with absolute path in front matter
    const std::string markdown =
        "---\ncss:\n  - " + secretPath.string() + "\n---\n# Doc\n";

    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.sourcePath = root / "doc.md";
    request.options.allowDocumentCss = true;

    const auto result = engine.render(request);

    require(
        !contains(result.css, ".secret"),
        "absolute document CSS path must be rejected");

    std::filesystem::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// 23. HTML entity obfuscation of dangerous URL (e.g. &#106; = 'j')
// ---------------------------------------------------------------------------
void testEntityObfuscatedUrl() {
    std::cout << "  testEntityObfuscatedUrl...\n";
    mwrender::Engine engine;

    // &#106; = 'j', &#97; = 'a', etc. — tries to obfuscate javascript:
    // The engine sees literal &#106;avascript: as destination string
    const auto result = renderFragment(engine,
        "[bad](&#106;avascript:alert(1))\n");

    // The raw link destination starts with &#106; which doesn't match
    // the safe-scheme list; it should be rejected
    require(!contains(result.html, "href=\"&#106;"),
            "entity-obfuscated URL must not appear in href");
}

// ---------------------------------------------------------------------------
// 24. Strict HTML policy mode
// ---------------------------------------------------------------------------
void testStrictHtmlPolicyMode() {
    std::cout << "  testStrictHtmlPolicyMode...\n";
    mwrender::Engine engine;

    mwrender::RenderRequest request;
    request.markdown = "<div>test</div>\n";
    request.options.outputMode = mwrender::OutputMode::Fragment;
    request.options.htmlPolicy = mwrender::HtmlPolicy::Sanitized;
    request.options.strictHtmlPolicy = true;

    const auto result = engine.render(request);

    // Sanitized with no sanitizer → strict mode must fail
    require(!result.ok, "strict sanitized mode with no sanitizer must fail");
}

// ---------------------------------------------------------------------------
// 25. Trusted HTML mode requires explicit opt-in
// ---------------------------------------------------------------------------
void testTrustedHtmlExplicitOptIn() {
    std::cout << "  testTrustedHtmlExplicitOptIn...\n";
    mwrender::Engine engine;

    // Default (Disabled) mode must NOT pass raw HTML through
    const auto disabledResult = renderFragment(engine, "<mark>test</mark>\n");
    require(!contains(disabledResult.html, "<mark>test</mark>"),
            "Disabled mode must not pass raw HTML through");

    // Trusted mode must pass HTML through
    const auto trustedResult = renderFragment(engine, "<mark>test</mark>\n",
        mwrender::HtmlPolicy::Trusted);
    require(contains(trustedResult.html, "<mark>test</mark>"),
            "Trusted mode must pass raw HTML through");
}

// ---------------------------------------------------------------------------
// 26. Extremely long URL (should not cause buffer issues)
// ---------------------------------------------------------------------------
void testExtremelyLongUrl() {
    std::cout << "  testExtremelyLongUrl...\n";
    mwrender::Engine engine;

    std::string url = "https://example.com/";
    url += std::string(10000, 'a');

    const std::string markdown = "[long](" + url + ")\n";
    const auto result = renderFragment(engine, markdown);

    require(result.ok, "extremely long URL should not crash");
}

// ---------------------------------------------------------------------------
// 27. Null bytes in input
// ---------------------------------------------------------------------------
void testNullBytesInInput() {
    std::cout << "  testNullBytesInInput...\n";
    mwrender::Engine engine;

    // Null bytes are invalid UTF-8 continuation sequences
    std::string markdown = "before";
    markdown += '\0';
    markdown += "after\n";

    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;

    // Should either reject or replace, but must not crash
    const auto result = engine.render(request);
    require(!result.html.empty() || !result.ok,
            "null bytes in input should be handled without crash");
}

// ---------------------------------------------------------------------------
// 28. Remote CSS via theme should be blocked
// ---------------------------------------------------------------------------
void testThemeRemoteCssBlocked() {
    std::cout << "  testThemeRemoteCssBlocked...\n";
    const auto root =
        std::filesystem::temp_directory_path() / "mwrender-security-remotecss";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "remote-css-theme");

    writeFile(root / "remote-css-theme" / "theme.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"id\": \"remote-css-theme\",\n"
        "  \"name\": \"Remote CSS Theme\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"appearance\": \"light\",\n"
        "  \"entry\": { \"content\": \"content.css\" }\n"
        "}\n");
    writeFile(root / "remote-css-theme" / "content.css",
        "@import url(https://evil.example.com/track.css);\n"
        ".mw-document { color: red; }\n");

    mwrender::Engine engine;
    engine.addThemeRoot(root);

    mwrender::RenderRequest request;
    request.markdown = "# Test\n";
    request.options.themeId = "remote-css-theme";

    const auto result = engine.render(request);

    require(!contains(result.css, "evil.example.com"),
            "remote CSS in theme must be blocked");

    std::filesystem::remove_all(root, ec);
}

} // namespace

int main() {
    std::cout << "=== Security Regression Tests ===\n\n";

    testScriptHtmlBlock();
    testEventHandlerAttribute();
    testSvgScriptBlock();
    testJavascriptLinkUrl();
    testJavascriptUrlUppercase();
    testJavascriptUrlMixedCase();
    testJavascriptUrlWhitespace();
    testVbscriptUrl();
    testDataUrlInLink();
    testDataUrlInImage();
    testJavascriptUrlInImage();
    testLinkTitleInjection();
    testHeadingAttrInjection();
    testCodeSpanHtmlEscape();
    testCodeBlockHtmlEscape();
    testCodeInfoStringInjection();
    testCssImportInjection();
    testCssRemoteUrlInjection();
    testCssProtocolRelativeUrlInjection();
    testThemePathTraversal();
    testDocumentCssPathTraversal();
    testAbsoluteDocumentCssRejection();
    testEntityObfuscatedUrl();
    testStrictHtmlPolicyMode();
    testTrustedHtmlExplicitOptIn();
    testExtremelyLongUrl();
    testNullBytesInInput();
    testThemeRemoteCssBlocked();

    std::cout << "\nAll security regression tests passed.\n";
    return 0;
}
