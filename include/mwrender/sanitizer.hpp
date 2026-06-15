#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <mwrender/diagnostics.hpp>
#include <mwrender/source.hpp>

namespace mwrender {

struct SanitizeResult {
    bool accepted = false;
    std::string html;
    std::vector<Diagnostic> diagnostics;
};

class HtmlSanitizer {
public:
    virtual ~HtmlSanitizer() = default;

    [[nodiscard]] virtual SanitizeResult sanitize(
        std::string_view html,
        const SourceRange& range) const = 0;
};

class SafeHtmlSanitizer final : public HtmlSanitizer {
public:
    [[nodiscard]] SanitizeResult sanitize(
        std::string_view html,
        const SourceRange& range) const override;
};

} // namespace mwrender
