#pragma once

#include <string>

#include <mwrender/ast.hpp>
#include <mwrender/options.hpp>
#include <mwrender/sanitizer.hpp>

namespace mwrender::detail {

struct HtmlRenderResult {
    bool ok = false;
    std::string fragment;
    std::vector<Diagnostic> diagnostics;
};

class HtmlRenderer {
public:
    [[nodiscard]] HtmlRenderResult render(
        const Node& document,
        const RenderOptions& options,
        const HtmlSanitizer* sanitizer) const;
};

[[nodiscard]] std::string assembleDocument(
    std::string_view fragment,
    std::string_view css,
    const RenderOptions& options);

} // namespace mwrender::detail

