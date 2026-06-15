#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include <mwrender/options.hpp>
#include <mwrender/result.hpp>
#include <mwrender/sanitizer.hpp>
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

    [[nodiscard]] RenderResult render(const RenderRequest& request) const;
    [[nodiscard]] std::vector<ThemeInfo> listThemes() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mwrender

