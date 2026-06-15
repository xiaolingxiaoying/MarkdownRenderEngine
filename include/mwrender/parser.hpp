#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/ast.hpp>
#include <mwrender/diagnostics.hpp>
#include <mwrender/options.hpp>

namespace mwrender {

struct DocumentMetadata {
    std::optional<std::string> title;
    std::optional<std::string> theme;
    std::vector<std::string> css;
};

struct ParseResult {
    bool ok = false;
    std::unique_ptr<Node> document;
    std::vector<Diagnostic> diagnostics;
    DocumentMetadata metadata;
};

class Parser {
public:
    explicit Parser(EngineOptions options = {});

    [[nodiscard]] ParseResult parse(
        std::string_view markdown,
        const ParseOptions& options = {}) const;

private:
    EngineOptions options_;
};

} // namespace mwrender
