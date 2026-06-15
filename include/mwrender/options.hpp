#pragma once

#include <cstddef>
#include <string>

namespace mwrender {

enum class InvalidUtf8Policy {
    Reject,
    Replace
};

enum class OutputMode {
    Fragment,
    FullDocument
};

enum class SourceMapMode {
    None,
    Line,
    Full
};

enum class HtmlPolicy {
    Disabled,
    Sanitized,
    Trusted
};

struct MarkdownExtensions {
    bool tables = true;
    bool taskLists = true;
    bool strikethrough = true;
    bool autoLinks = true;
};

struct ParseOptions {
    MarkdownExtensions extensions;
};

struct EngineOptions {
    std::size_t maxInputBytes = 16U * 1024U * 1024U;
    std::size_t maxThemeFileBytes = 2U * 1024U * 1024U;
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
