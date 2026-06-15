#pragma once

#include <cstdint>
#include <string>

namespace mwrender {

enum class InvalidUtf8Policy : std::uint8_t {
    Reject,
    Replace
};

enum class OutputMode : std::uint8_t {
    Fragment,
    FullDocument
};

enum class SourceMapMode : std::uint8_t {
    None,
    Line,
    Full
};

enum class HtmlPolicy : std::uint8_t {
    Disabled,
    Sanitized,
    Trusted
};

struct MarkdownExtensions {
    bool tables = true;
    bool taskLists = true;
    bool strikethrough = true;
    bool autoLinks = true;
    bool latexMath = true;
    bool mermaid = true;
    bool toc = true;
    bool footnotes = true;
    bool highlight = true;
};

struct ParseOptions {
    MarkdownExtensions extensions;
};

struct EngineOptions {
    std::size_t maxInputBytes = 16ULL * 1024ULL * 1024ULL;
    std::size_t maxThemeFileBytes = 2ULL * 1024ULL * 1024ULL;
    std::size_t maxNestingDepth = 256;
    InvalidUtf8Policy invalidUtf8Policy = InvalidUtf8Policy::Reject;
};

struct RenderOptions {
    OutputMode outputMode = OutputMode::FullDocument;
    SourceMapMode sourceMap = SourceMapMode::Line;
    HtmlPolicy htmlPolicy = HtmlPolicy::Disabled;
    MarkdownExtensions extensions;

    std::string themeId = "github-light";
    std::string language = "zh-CN";
    std::string title = "Markdown Document";

    bool includeCss = true;
    bool includeAst = true;
    bool strictTheme = false;
    bool strictHtmlPolicy = false;
    bool allowDocumentTheme = true;
    bool allowDocumentCss = false;
    bool allowRemoteCssResources = false;
};

} // namespace mwrender
