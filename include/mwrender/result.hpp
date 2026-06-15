#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/ast.hpp>
#include <mwrender/diagnostics.hpp>
#include <mwrender/options.hpp>
#include <mwrender/outline.hpp>
#include <mwrender/stats.hpp>

namespace mwrender {

struct CssRequest {
    std::vector<std::filesystem::path> files;
    std::vector<std::string> text;
};

struct RenderRequest {
    std::string_view markdown;
    std::optional<std::filesystem::path> sourcePath;
    RenderOptions options;
    CssRequest css;
};

struct RenderResult {
    bool ok = false;
    std::string html;
    std::string fragment;
    std::string css;
    std::unique_ptr<Node> document;
    std::vector<Diagnostic> diagnostics;
    std::string resolvedThemeId;
    std::vector<OutlineItem> outline;
    WordStats stats;
};

} // namespace mwrender
