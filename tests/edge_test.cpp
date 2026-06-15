#include <cstdlib>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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

class MarkSanitizer final : public mwrender::HtmlSanitizer {
public:
    mwrender::SanitizeResult sanitize(
        std::string_view html,
        const mwrender::SourceRange&) const override {
        mwrender::SanitizeResult result;
        if (html == "<mark>safe</mark>") {
            result.accepted = true;
            result.html = html;
        }
        return result;
    }
};

void testEscapingAndSlugs() {
    const std::string markdown =
        "# Same\n"
        "# Same\n"
        "\n"
        "`<tag>&` ![a\"b](image.png \"t&x\")\n";

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "escaping example should render");
    require(contains(result.html, "id=\"same\""), "first slug should exist");
    require(contains(result.html, "id=\"same-1\""), "duplicate slug should be unique");
    require(
        contains(result.html, "&lt;tag&gt;&amp;"),
        "inline code should escape HTML");
    require(
        contains(result.html, "alt=\"a&quot;b\""),
        "image alt should be attribute escaped");
    require(
        contains(result.html, "title=\"t&amp;x\""),
        "image title should be attribute escaped");
}

void testSourceRangesAndLineEndings() {
    const std::string markdown = "# A\r\n\r\nline one\r\nline two\r\n";
    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    request.options.sourceMap = mwrender::SourceMapMode::Full;
    const auto result = engine.render(request);

    require(result.ok, "CRLF input should render");
    require(result.document != nullptr, "CRLF render should return AST");
    require(
        result.document->children.size() == 2,
        "CRLF input should contain heading and paragraph");
    require(
        result.document->children[1]->range.begin.line == 3,
        "paragraph source line should be preserved");
    require(
        result.document->children[1]->range.end.offset == 25,
        "paragraph source byte end should be preserved");
}

void testUtf8PoliciesAndLimits() {
    mwrender::EngineOptions smallOptions;
    smallOptions.maxInputBytes = 4;
    mwrender::Engine smallEngine(smallOptions);
    mwrender::RenderRequest request;
    request.markdown = "12345";
    auto result = smallEngine.render(request);
    require(!result.ok, "oversized input should fail");
    require(hasDiagnostic(result, "MW0001"), "oversized input should diagnose");

    std::string invalid{"bad"};
    invalid.push_back(static_cast<char>(0xFF));

    mwrender::Engine rejectEngine;
    request.markdown = invalid;
    result = rejectEngine.render(request);
    require(!result.ok, "invalid UTF-8 should fail by default");
    require(hasDiagnostic(result, "MW0002"), "invalid UTF-8 should diagnose");

    mwrender::EngineOptions replaceOptions;
    replaceOptions.invalidUtf8Policy = mwrender::InvalidUtf8Policy::Replace;
    mwrender::Engine replaceEngine(replaceOptions);
    result = replaceEngine.render(request);
    require(result.ok, "replace UTF-8 policy should remain renderable");
    require(hasDiagnostic(result, "MW0003"), "UTF-8 replacement should warn");
    require(result.document != nullptr, "replacement render should return AST");
    require(
        result.document->range.end.offset == invalid.size(),
        "replacement ranges should use original input byte offsets");

    const std::string bomMarkdown = "\xEF\xBB\xBF# BOM";
    request.markdown = bomMarkdown;
    result = rejectEngine.render(request);
    require(result.ok, "BOM input should render");
    require(result.document != nullptr, "BOM render should return AST");
    require(
        result.document->children.front()->range.begin.offset == 3,
        "BOM ranges should retain the original three-byte prefix");
    require(
        result.document->range.end.offset == bomMarkdown.size(),
        "BOM document range should end at the original byte length");
}

void testSanitizerInjection() {
    mwrender::Engine engine;
    engine.setHtmlSanitizer(std::make_shared<MarkSanitizer>());

    mwrender::RenderRequest request;
    request.markdown = "<mark>safe</mark>\n";
    request.options.outputMode = mwrender::OutputMode::Fragment;
    request.options.htmlPolicy = mwrender::HtmlPolicy::Sanitized;
    const auto result = engine.render(request);
    require(result.ok, "configured sanitizer should render");
    require(
        contains(result.html, "<mark>safe</mark>"),
        "accepted sanitized HTML should pass through");
}

void testOutlineHierarchy() {
    const std::string markdown =
        "# Root\n"
        "## Child\n"
        "### Grandchild\n"
        "## Child\n";
    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    const auto result = engine.render(request);

    require(result.ok, "outline example should render");
    require(result.outline.size() == 1, "outline should have one root");
    require(
        result.outline.front().children.size() == 2,
        "outline should contain two child headings");
    require(
        result.outline.front().children.front().children.size() == 1,
        "outline should contain a grandchild heading");
    require(
        result.outline.front().children[1].id == "child-1",
        "duplicate outline IDs should match rendered slugs");
}

void writeFile(
    const std::filesystem::path& path,
    std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << content;
    require(static_cast<bool>(output), "fixture file should be written");
}

void testRemoteThemePolicy() {
    const auto root =
        std::filesystem::temp_directory_path() / "mwrender-edge-theme";
    std::error_code error;
    std::filesystem::remove_all(root, error);

    writeFile(
        root / "remote-theme" / "theme.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"id\": \"remote-theme\",\n"
        "  \"name\": \"Remote Theme\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"appearance\": \"light\",\n"
        "  \"entry\": { \"content\": \"content.css\" }\n"
        "}\n");
    writeFile(
        root / "remote-theme" / "content.css",
        "@import url(https://example.com/theme.css);\n");

    mwrender::Engine engine;
    engine.addThemeRoot(root);
    mwrender::RenderRequest request;
    request.markdown = "# Theme\n";
    request.options.themeId = "remote-theme";
    auto result = engine.render(request);
    require(result.ok, "remote theme should fall back in non-strict mode");
    require(hasDiagnostic(result, "MW2005"), "remote theme should diagnose");
    require(
        result.resolvedThemeId == "github-light",
        "remote theme should fall back to github-light");

    request.options.strictTheme = true;
    result = engine.render(request);
    require(!result.ok, "remote theme should fail in strict mode");

    std::filesystem::remove_all(root, error);
}

void testConcurrentRendering() {
    mwrender::Engine engine;
    std::atomic<int> failures = 0;
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 8; ++worker) {
        workers.emplace_back([&] {
            for (int iteration = 0; iteration < 50; ++iteration) {
                mwrender::RenderRequest request;
                request.markdown = "# Concurrent\n\n**render**";
                request.options.outputMode =
                    mwrender::OutputMode::Fragment;
                const auto result = engine.render(request);
                if (!result.ok ||
                    !contains(result.html, "<strong>render</strong>")) {
                    ++failures;
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    require(failures == 0, "concurrent rendering should be deterministic");
}

} // namespace

int main() {
    testEscapingAndSlugs();
    testSourceRangesAndLineEndings();
    testUtf8PoliciesAndLimits();
    testSanitizerInjection();
    testOutlineHierarchy();
    testRemoteThemePolicy();
    testConcurrentRendering();
    return 0;
}
