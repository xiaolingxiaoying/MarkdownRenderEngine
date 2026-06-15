#include <cstdlib>
#include <filesystem>
#include <fstream>
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

bool contains(std::string_view text, std::string_view expected) {
    return text.find(expected) != std::string_view::npos;
}

bool hasDiagnostic(
    const mwrender::RenderResult& result,
    std::string_view code) {
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

void testEmptyDocument(mwrender::Engine& engine) {
    mwrender::RenderRequest request;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "empty fragment render should succeed");
    require(
        result.fragment ==
            "<article class=\"mw-document markdown-body\">\n</article>\n",
        "empty fragment output should be deterministic");
    require(result.html == result.fragment, "fragment should be exposed as html");
    require(result.document != nullptr, "AST should be returned by default");
}

void testCoreMarkdown(mwrender::Engine& engine) {
    const std::string markdown =
        "# Hello\n"
        "\n"
        "This is **strong**, *emphasized*, [safe](https://example.com), "
        "![image](./image.png), and `code`.\n"
        "\n"
        "> quote\n"
        "\n"
        "3. third\n"
        "4. fourth\n"
        "\n"
        "---\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n";

    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    request.options.sourceMap = mwrender::SourceMapMode::Full;
    const auto result = engine.render(request);

    require(result.ok, "core Markdown render should succeed");
    require(contains(result.html, "<h1"), "heading should render");
    require(contains(result.html, "id=\"hello\""), "heading slug should render");
    require(contains(result.html, "<strong>strong</strong>"), "strong should render");
    require(contains(result.html, "<em>emphasized</em>"), "emphasis should render");
    require(
        contains(result.html, "href=\"https://example.com\""),
        "safe link should render");
    require(
        contains(result.html, "src=\"./image.png\""),
        "relative image should render");
    require(
        contains(result.html, "<code class=\"mw-inline-code\">code</code>"),
        "inline code should render");
    require(contains(result.html, "<blockquote"), "blockquote should render");
    require(contains(result.html, "<ol class=\"mw-list\" start=\"3\""), "ordered list start should render");
    require(contains(result.html, "<hr"), "thematic break should render");
    require(
        contains(result.html, "class=\"language-cpp\""),
        "code language should render");
    require(
        contains(result.html, "data-source-start=\""),
        "full source attributes should render");

    require(result.document != nullptr, "core render should return AST");
    require(
        result.document->children.size() == 6,
        "core document should contain six block nodes");
    require(
        result.document->children.front()->type == mwrender::NodeType::Heading,
        "first block should be a heading");
    require(result.outline.size() == 1, "heading should appear in outline");
    require(result.outline.front().id == "hello", "outline ID should match HTML");
    require(result.stats.headings == 1, "heading count should be available");
    require(
        result.stats.paragraphs == 4,
        "paragraph count should include quote and list item text");
    require(result.stats.images == 1, "image count should be available");
    require(result.stats.links == 1, "link count should be available");
    require(result.stats.codeLines == 1, "code line count should be available");
}

void testHtmlAndUrlSafety(mwrender::Engine& engine) {
    const std::string markdown =
        "<script>alert(1)</script>\n"
        "\n"
        "[unsafe](javascript:alert(1))\n";

    mwrender::RenderRequest disabled;
    disabled.markdown = markdown;
    disabled.options.outputMode = mwrender::OutputMode::Fragment;
    auto result = engine.render(disabled);
    require(result.ok, "disabled HTML render should succeed");
    require(
        contains(result.html, "&lt;script&gt;"),
        "disabled HTML should be escaped");
    require(
        !contains(result.html, "href=\"javascript:"),
        "unsafe link destination should be removed");
    require(hasDiagnostic(result, "MW3002"), "unsafe link warning should exist");

    mwrender::RenderRequest trusted;
    trusted.markdown = "<mark>trusted</mark>\n";
    trusted.options.outputMode = mwrender::OutputMode::Fragment;
    trusted.options.htmlPolicy = mwrender::HtmlPolicy::Trusted;
    result = engine.render(trusted);
    require(result.ok, "trusted HTML render should succeed");
    require(
        contains(result.html, "<mark>trusted</mark>"),
        "trusted HTML should pass through");

    mwrender::RenderRequest sanitized;
    sanitized.markdown = "<mark>safe</mark>\n";
    sanitized.options.outputMode = mwrender::OutputMode::Fragment;
    sanitized.options.htmlPolicy = mwrender::HtmlPolicy::Sanitized;
    result = engine.render(sanitized);
    require(result.ok, "missing sanitizer should degrade in non-strict mode");
    require(hasDiagnostic(result, "MW3001"), "missing sanitizer warning should exist");
    require(contains(result.html, "&lt;mark&gt;"), "missing sanitizer should escape HTML");

    sanitized.options.strictHtmlPolicy = true;
    result = engine.render(sanitized);
    require(!result.ok, "strict missing sanitizer should fail");
}

void testThemeAndCss(mwrender::Engine& engine) {
    mwrender::RenderRequest request;
    request.markdown = "# Styled\n";
    request.css.text.push_back(".mw-document { outline: 0; }");
    auto result = engine.render(request);

    require(result.ok, "default themed render should succeed");
    require(
        result.resolvedThemeId == "github-light",
        "default theme should resolve to github-light");
    require(
        contains(result.css, ".markdown-body"),
        "GitHub Markdown CSS should be embedded");
    require(
        contains(result.css, "outline: 0"),
        "request CSS should be appended");
    require(
        contains(result.html, "<style>"),
        "full document should inline composed CSS");

    request.options.themeId = "missing-theme";
    result = engine.render(request);
    require(result.ok, "missing theme should fall back in non-strict mode");
    require(hasDiagnostic(result, "MW2001"), "theme fallback warning should exist");
    require(
        result.resolvedThemeId == "github-light",
        "missing theme should resolve to github-light");

    request.options.strictTheme = true;
    result = engine.render(request);
    require(!result.ok, "strict missing theme should fail");
}

void testDiagnostics(mwrender::Engine& engine) {
    mwrender::RenderRequest request;
    request.markdown = "```cpp\nnot closed\n";
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);
    require(result.ok, "unclosed fence should remain renderable");
    require(hasDiagnostic(result, "MW1001"), "unclosed fence should warn");
}

void testGfmExtensions(mwrender::Engine& engine) {
    const std::string markdown =
        "| Name | Value |\n"
        "| :--- | ---: |\n"
        "| A | 1 |\n"
        "\n"
        "- [x] done\n"
        "- [ ] todo\n"
        "\n"
        "~~deleted~~ <https://example.com>\n";

    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    auto result = engine.render(request);
    require(result.ok, "GFM extensions should render");
    require(contains(result.html, "<table"), "GFM table should render");
    require(contains(result.html, "align=\"right\""), "table alignment should render");
    require(contains(result.html, "mw-task-list"), "task list class should render");
    require(contains(result.html, "checked"), "checked task should render");
    require(contains(result.html, "<del>deleted</del>"), "strikethrough should render");
    require(contains(result.html, "mw-autolink"), "autolink should render");

    request.options.extensions.tables = false;
    request.options.extensions.taskLists = false;
    request.options.extensions.strikethrough = false;
    request.options.extensions.autoLinks = false;
    result = engine.render(request);
    require(result.ok, "disabled GFM extensions should degrade to base Markdown");
    require(!contains(result.html, "<table"), "disabled table should not render");
    require(!contains(result.html, "type=\"checkbox\""), "disabled task list should not render");
    require(!contains(result.html, "<del>"), "disabled strikethrough should not render");
    require(!contains(result.html, "mw-autolink"), "disabled autolink should not render");
}

void writeFile(
    const std::filesystem::path& path,
    std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << content;
    require(static_cast<bool>(output), "test fixture file should be written");
}

void testFrontMatterAndExternalTheme() {
    const auto root =
        std::filesystem::temp_directory_path() / "mwrender-smoke-fixtures";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "themes" / "custom-light");

    writeFile(
        root / "themes" / "custom-light" / "theme.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"id\": \"custom-light\",\n"
        "  \"name\": \"\\uD83D\\uDCA1 Custom Light\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"appearance\": \"light\",\n"
        "  \"entry\": { \"content\": \"content.css\" },\n"
        "  \"variables\": { \"color.accent\": \"#123456\" }\n"
        "}\n");
    writeFile(
        root / "themes" / "custom-light" / "content.css",
        ".mw-document { color: rgb(1, 2, 3); }\n");
    writeFile(
        root / "note.css",
        ".note { border: 1px solid; }\n");

    const auto documentPath = root / "document.md";
    const std::string markdown =
        "---\n"
        "title: Front Matter Title\n"
        "theme: custom-light\n"
        "css:\n"
        "  - note.css\n"
        "---\n"
        "\n"
        "# Document\n";
    writeFile(documentPath, markdown);

    mwrender::Engine engine;
    engine.addThemeRoot(root / "themes");
    const auto themes = engine.listThemes();
    require(
        themes.size() == 8,
        "valid external theme should be discoverable alongside 7 builtin themes");
    require(
        themes.back().name == "\xF0\x9F\x92\xA1 Custom Light",
        "JSON unicode surrogate pairs should decode as UTF-8");

    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.sourcePath = documentPath;
    request.options.allowDocumentCss = true;
    auto result = engine.render(request);
    require(result.ok, "front matter render should succeed");
    require(
        result.resolvedThemeId == "custom-light",
        "front matter should select an allowed theme");
    require(
        contains(result.html, "<title>Front Matter Title</title>"),
        "front matter title should apply");
    require(
        contains(result.css, "rgb(1, 2, 3)"),
        "external theme CSS should apply");
    require(
        contains(result.css, "--mw-color-accent: #123456"),
        "theme variables should become CSS custom properties");
    require(
        contains(result.css, ".note"),
        "document CSS should apply when enabled");

    request.options.allowDocumentCss = false;
    result = engine.render(request);
    require(
        !contains(result.css, ".note"),
        "document CSS should not apply when disabled");

    request.css.text = {
        "@import url(https://example.com/style.css);",
    };
    result = engine.render(request);
    require(result.ok, "remote request CSS rejection should be non-fatal");
    require(
        hasDiagnostic(result, "MW4002"),
        "remote CSS should produce a diagnostic");
    require(
        !contains(result.css, "example.com/style.css"),
        "remote CSS should be omitted");

    std::filesystem::remove_all(root, error);
}

} // namespace

int main() {
    mwrender::Engine engine;
    testEmptyDocument(engine);
    testCoreMarkdown(engine);
    testHtmlAndUrlSafety(engine);
    testThemeAndCss(engine);
    testDiagnostics(engine);
    testGfmExtensions(engine);
    testFrontMatterAndExternalTheme();

    const auto themes = engine.listThemes();
    require(!themes.empty(), "at least one theme should be registered");
    require(
        themes.front().id == "github-light",
        "github-light should be available");
    return 0;
}
