#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/incremental.hpp>
#include <mwrender/options.hpp>
#include <mwrender/parser.hpp>
#include <mwrender/result.hpp>
#include <mwrender/sanitizer.hpp>
#include <mwrender/serializer.hpp>
#include <mwrender/theme.hpp>

namespace mwrender {

class Engine {
public:
    Engine();
    explicit Engine(EngineOptions options);
    ~Engine();

    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void addThemeRoot(
        std::filesystem::path path,
        ThemeOrigin origin = ThemeOrigin::Explicit);

    void setHtmlSanitizer(std::shared_ptr<const HtmlSanitizer> sanitizer);

    [[nodiscard]] ParseResult parse(
        std::string_view markdown,
        const ParseOptions& options = {}) const;

    [[nodiscard]] IncrementalParseResult update(
        const Node& oldDocument,
        const TextChange& change) const;

    [[nodiscard]] RenderResult render(const RenderRequest& request) const;

    [[nodiscard]] RenderResult renderNode(
        const NodeRenderRequest& request) const;

    [[nodiscard]] std::string serialize(
        const Node& document,
        const MarkdownStyle& style = {}) const;

    [[nodiscard]] std::unique_ptr<Node> applyCommand(
        const Node& document,
        const Command& command) const;

    [[nodiscard]] std::vector<ThemeInfo> listThemes() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mwrender

