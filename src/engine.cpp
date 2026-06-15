#include <mwrender/engine.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include <mwrender/parser.hpp>

#include "analysis/document_analysis.hpp"
#include "render/html_renderer.hpp"
#include "theme/theme_manager.hpp"

namespace mwrender {
namespace {

bool containsRemoteCssResource(std::string_view css) {
    std::string normalized;
    normalized.reserve(css.size());
    for (const auto character : css) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::isspace(byte) == 0) {
            normalized += static_cast<char>(std::tolower(byte));
        }
    }
    return normalized.find("@import") != std::string::npos ||
        normalized.find("url(http:") != std::string::npos ||
        normalized.find("url(https:") != std::string::npos ||
        normalized.find("url(\"http:") != std::string::npos ||
        normalized.find("url(\"https:") != std::string::npos ||
        normalized.find("url('http:") != std::string::npos ||
        normalized.find("url('https:") != std::string::npos ||
        normalized.find("url(//") != std::string::npos ||
        normalized.find("url(\"//") != std::string::npos ||
        normalized.find("url('//") != std::string::npos;
}

bool isWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart) {
        if (candidatePart == candidate.end() || *rootPart != *candidatePart) {
            return false;
        }
    }
    return true;
}

} // namespace

class Engine::Impl {
public:
    explicit Impl(EngineOptions engineOptions)
        : options(std::move(engineOptions)),
          parser(options),
          themeManager(options.maxThemeFileBytes) {}

    EngineOptions options;
    Parser parser;
    detail::HtmlRenderer htmlRenderer;
    detail::ThemeManager themeManager;
    std::shared_ptr<const HtmlSanitizer> sanitizer;
    mutable std::mutex resourcesMutex;
};

Engine::Engine()
    : Engine(EngineOptions{}) {}

Engine::Engine(EngineOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

void Engine::addThemeRoot(std::filesystem::path path, ThemeOrigin origin) {
    std::scoped_lock lock(impl_->resourcesMutex);
    impl_->themeManager.addRoot(std::move(path), origin);
}

void Engine::setHtmlSanitizer(
    std::shared_ptr<const HtmlSanitizer> sanitizer) {
    std::scoped_lock lock(impl_->resourcesMutex);
    impl_->sanitizer = std::move(sanitizer);
}

RenderResult Engine::render(const RenderRequest& request) const {
    RenderResult result;

    auto parseResult = impl_->parser.parse(
        request.markdown,
        ParseOptions{request.options.extensions});
    result.diagnostics = std::move(parseResult.diagnostics);
    if (!parseResult.ok || !parseResult.document) {
        return result;
    }
    result.outline = detail::buildOutline(*parseResult.document);
    result.stats = detail::buildWordStats(*parseResult.document);

    RenderOptions effectiveOptions = request.options;
    if (effectiveOptions.allowDocumentTheme &&
        parseResult.metadata.theme &&
        effectiveOptions.themeId == "github-light") {
        effectiveOptions.themeId = *parseResult.metadata.theme;
    }
    if (parseResult.metadata.title &&
        effectiveOptions.title == "Markdown Document") {
        effectiveOptions.title = *parseResult.metadata.title;
    }

    std::shared_ptr<const HtmlSanitizer> sanitizer;
    detail::ThemeManager themeManagerSnapshot(
        impl_->options.maxThemeFileBytes);
    {
        std::scoped_lock lock(impl_->resourcesMutex);
        sanitizer = impl_->sanitizer;
        themeManagerSnapshot = impl_->themeManager;
    }
    auto theme = themeManagerSnapshot.resolve(
        effectiveOptions.themeId,
        effectiveOptions.strictTheme);
    result.diagnostics.insert(
        result.diagnostics.end(),
        std::make_move_iterator(theme.diagnostics.begin()),
        std::make_move_iterator(theme.diagnostics.end()));
    if (!theme.ok) {
        return result;
    }
    if (!effectiveOptions.allowRemoteCssResources &&
        containsRemoteCssResource(theme.css)) {
        result.diagnostics.push_back({
            effectiveOptions.strictTheme
                ? DiagnosticSeverity::Error
                : DiagnosticSeverity::Warning,
            "MW2005",
            "Theme '" + theme.id +
                "' contains remote CSS resources." +
                (effectiveOptions.strictTheme
                     ? std::string{}
                     : " Falling back to github-light."),
            std::nullopt,
        });
        if (effectiveOptions.strictTheme) {
            return result;
        }
        theme = themeManagerSnapshot.resolve("github-light", true);
        if (!theme.ok) {
            return result;
        }
    }

    auto renderResult = impl_->htmlRenderer.render(
        *parseResult.document,
        effectiveOptions,
        sanitizer.get());
    result.diagnostics.insert(
        result.diagnostics.end(),
        std::make_move_iterator(renderResult.diagnostics.begin()),
        std::make_move_iterator(renderResult.diagnostics.end()));
    if (!renderResult.ok) {
        return result;
    }

    result.fragment = std::move(renderResult.fragment);
    result.resolvedThemeId = std::move(theme.id);
    result.css = std::move(theme.css);

    const auto appendCss = [&](std::string_view css, std::string_view name) {
        if (!effectiveOptions.allowRemoteCssResources &&
            containsRemoteCssResource(css)) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW4002",
                "CSS source '" + std::string(name) +
                    "' was rejected because it contains remote resources.",
                std::nullopt,
            });
            return;
        }
        result.css += "\n";
        result.css += css;
    };

    if (effectiveOptions.allowDocumentCss &&
        request.sourcePath &&
        !parseResult.metadata.css.empty()) {
        std::error_code error;
        const auto sourceRoot = std::filesystem::weakly_canonical(
            request.sourcePath->parent_path(),
            error);
        if (!error) {
            for (const auto& relativeCss : parseResult.metadata.css) {
                const std::filesystem::path relativePath(relativeCss);
                if (relativePath.is_absolute()) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4003",
                        "Absolute document CSS path was rejected: " +
                            relativeCss,
                        std::nullopt,
                    });
                    continue;
                }
                const auto cssPath = std::filesystem::weakly_canonical(
                    sourceRoot / relativePath,
                    error);
                if (error || !isWithin(sourceRoot, cssPath)) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4004",
                        "Document CSS path escapes the document directory: " +
                            relativeCss,
                        std::nullopt,
                    });
                    error.clear();
                    continue;
                }
                std::ifstream input(cssPath, std::ios::binary);
                if (!input) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4001",
                        "Document CSS could not be read: " + cssPath.string(),
                        std::nullopt,
                    });
                    continue;
                }
                const auto cssSize =
                    std::filesystem::file_size(cssPath, error);
                if (error || cssSize > impl_->options.maxThemeFileBytes) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4005",
                        "Document CSS exceeds the configured maximum size: " +
                            cssPath.string(),
                        std::nullopt,
                    });
                    error.clear();
                    continue;
                }
                const std::string css{
                    std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>()};
                appendCss(css, cssPath.string());
            }
        }
    }

    for (const auto& cssPath : request.css.files) {
        std::error_code error;
        const auto cssSize = std::filesystem::file_size(cssPath, error);
        if (error || cssSize > impl_->options.maxThemeFileBytes) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW4005",
                "Custom CSS exceeds the configured maximum size or is unreadable: " +
                    cssPath.string(),
                std::nullopt,
            });
            continue;
        }
        std::ifstream input(cssPath, std::ios::binary);
        if (!input) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW4001",
                "Custom CSS could not be read: " + cssPath.string(),
                std::nullopt,
            });
            continue;
        }
        const std::string css{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        appendCss(css, cssPath.string());
    }
    for (const auto& css : request.css.text) {
        appendCss(css, "request");
    }
    result.html = effectiveOptions.outputMode == OutputMode::Fragment
        ? result.fragment
        : detail::assembleDocument(
              result.fragment,
              result.css,
              effectiveOptions);

    if (effectiveOptions.includeAst) {
        result.document = std::move(parseResult.document);
    }

    result.ok = std::none_of(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [](const Diagnostic& diagnostic) {
            return diagnostic.severity == DiagnosticSeverity::Error;
        });
    return result;
}

std::vector<ThemeInfo> Engine::listThemes() const {
    detail::ThemeManager snapshot(impl_->options.maxThemeFileBytes);
    {
        std::scoped_lock lock(impl_->resourcesMutex);
        snapshot = impl_->themeManager;
    }
    return snapshot.listThemes();
}

} // namespace mwrender
