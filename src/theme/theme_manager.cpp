#include "theme/theme_manager.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

#include <mwrender/embed_github_auto_css.hpp>
#include <mwrender/embed_github_dark_cb_css.hpp>
#include <mwrender/embed_github_dark_css.hpp>
#include <mwrender/embed_github_dark_dimmed_css.hpp>
#include <mwrender/embed_github_dark_hc_css.hpp>
#include <mwrender/embed_github_light_cb_css.hpp>
#include <mwrender/embed_github_light_css.hpp>
#include "support/json.hpp"

namespace mwrender::detail {
namespace {

std::optional<std::string> readFile(
    const std::filesystem::path& path,
    std::size_t maxFileBytes) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maxFileBytes) {
        return std::nullopt;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

bool isThemeId(std::string_view id) {
    if (id.empty() || id.front() == '-' || id.back() == '-') {
        return false;
    }
    for (const char character : id) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::islower(byte) == 0 &&
            std::isdigit(byte) == 0 &&
            character != '-') {
            return false;
        }
    }
    return true;
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

struct ExternalTheme {
    ThemeInfo info;
    std::vector<std::filesystem::path> cssPaths;
    std::string variablesCss;
};

const std::string* requiredString(
    const JsonValue& object,
    std::string_view key) {
    const auto* value = object.get(key);
    return value ? value->asString() : nullptr;
}

std::string buildVariablesCss(const JsonValue& manifest) {
    const auto* variablesValue = manifest.get("variables");
    const auto* variables =
        variablesValue ? variablesValue->asObject() : nullptr;
    if (!variables) {
        return {};
    }

    static const std::map<std::string_view, std::string_view> names{
        {"color.background", "--mw-color-bg"},
        {"color.foreground", "--mw-color-fg"},
        {"color.muted", "--mw-color-muted"},
        {"color.border", "--mw-color-border"},
        {"color.accent", "--mw-color-accent"},
        {"color.codeBackground", "--mw-color-code-bg"},
        {"font.body", "--mw-font-body"},
        {"font.code", "--mw-font-code"},
        {"layout.maxWidth", "--mw-layout-max-width"},
        {"layout.padding", "--mw-layout-padding"},
        {"layout.lineHeight", "--mw-line-height"},
    };

    std::string css;
    for (const auto& [key, value] : *variables) {
        const auto name = names.find(key);
        const auto* stringValue = value.asString();
        if (name == names.end() || !stringValue) {
            continue;
        }
        if (css.empty()) {
            css = ".mw-document {\n";
        }
        css += "  ";
        css += name->second;
        css += ": ";
        css += *stringValue;
        css += ";\n";
    }
    if (!css.empty()) {
        css += "}\n";
    }
    return css;
}

std::optional<ExternalTheme> loadExternalTheme(
    const std::filesystem::path& themePath,
    ThemeOrigin origin,
    std::size_t maxFileBytes,
    std::string* errorMessage = nullptr) {
    const auto manifestText =
        readFile(themePath / "theme.json", maxFileBytes);
    if (!manifestText) {
        if (errorMessage) {
            *errorMessage = "theme.json is missing or unreadable";
        }
        return std::nullopt;
    }
    const auto parsed = parseJson(*manifestText);
    if (!parsed.value || !parsed.value->asObject()) {
        if (errorMessage) {
            *errorMessage = "theme.json is invalid JSON: " + parsed.error;
        }
        return std::nullopt;
    }
    const auto* schemaVersionValue = parsed.value->get("schemaVersion");
    const auto* schemaVersion =
        schemaVersionValue ? schemaVersionValue->asNumber() : nullptr;
    const auto* id = requiredString(*parsed.value, "id");
    const auto* name = requiredString(*parsed.value, "name");
    const auto* version = requiredString(*parsed.value, "version");
    const auto* appearance = requiredString(*parsed.value, "appearance");
    const auto* entryObjectValue = parsed.value->get("entry");
    const auto* entryObject =
        entryObjectValue ? entryObjectValue->asObject() : nullptr;
    const auto* contentValue =
        entryObjectValue ? entryObjectValue->get("content") : nullptr;
    const auto* contentEntry =
        contentValue ? contentValue->asString() : nullptr;
    if (!schemaVersion || *schemaVersion != 1.0 ||
        !id || !name || !version || !appearance || !entryObject ||
        !contentEntry || !isThemeId(*id)) {
        if (errorMessage) {
            *errorMessage = "theme.json does not satisfy schemaVersion 1";
        }
        return std::nullopt;
    }
    if (*id != themePath.filename().string()) {
        if (errorMessage) {
            *errorMessage = "theme ID does not match its directory name";
        }
        return std::nullopt;
    }
    if (*appearance != "light" &&
        *appearance != "dark" &&
        *appearance != "auto") {
        if (errorMessage) {
            *errorMessage = "theme appearance is invalid";
        }
        return std::nullopt;
    }

    std::error_code error;
    const auto canonicalRoot =
        std::filesystem::weakly_canonical(themePath, error);
    std::vector<std::filesystem::path> cssPaths;
    for (const std::string_view entryName : {"content", "code"}) {
        const auto* entryValue = entryObjectValue->get(entryName);
        if (!entryValue) {
            continue;
        }
        const auto* entry = entryValue->asString();
        if (!entry) {
            if (errorMessage) {
                *errorMessage =
                    "theme CSS entry '" + std::string(entryName) +
                    "' must be a string";
            }
            return std::nullopt;
        }
        const std::filesystem::path entryPath(*entry);
        if (entryPath.is_absolute()) {
            if (errorMessage) {
                *errorMessage = "theme CSS entries must be relative";
            }
            return std::nullopt;
        }
        const auto canonicalEntry =
            std::filesystem::weakly_canonical(themePath / entryPath, error);
        if (error || !isWithin(canonicalRoot, canonicalEntry) ||
            !std::filesystem::is_regular_file(canonicalEntry, error)) {
            if (errorMessage) {
                *errorMessage =
                    "theme CSS entry '" + std::string(entryName) +
                    "' escapes the theme or is missing";
            }
            return std::nullopt;
        }
        cssPaths.push_back(canonicalEntry);
    }

    return ExternalTheme{
        {
            *id,
            *name,
            *version,
            *appearance,
            origin,
            canonicalRoot,
            true,
        },
        std::move(cssPaths),
        buildVariablesCss(*parsed.value),
    };
}

} // namespace

ThemeManager::ThemeManager(std::size_t maxFileBytes)
    : maxFileBytes_(maxFileBytes) {}

void ThemeManager::addRoot(std::filesystem::path path, ThemeOrigin origin) {
    roots_.push_back({std::move(path), origin, roots_.size()});
    std::stable_sort(
        roots_.begin(),
        roots_.end(),
        [](const ThemeRoot& left, const ThemeRoot& right) {
            if (left.origin != right.origin) {
                return left.origin < right.origin;
            }
            return left.registrationOrder < right.registrationOrder;
        });
}

std::vector<ThemeInfo> ThemeManager::listThemes() const {
    std::vector<ThemeInfo> themes{
        {"github-light", "GitHub Light", "5.9.0", "light", ThemeOrigin::BuiltIn, {}, true},
        {"github-dark", "GitHub Dark", "5.9.0", "dark", ThemeOrigin::BuiltIn, {}, true},
        {"github-dark-dimmed", "GitHub Dark Dimmed", "5.9.0", "dark", ThemeOrigin::BuiltIn, {}, true},
        {"github-dark-high-contrast", "GitHub Dark High Contrast", "5.9.0", "dark", ThemeOrigin::BuiltIn, {}, true},
        {"github-dark-colorblind", "GitHub Dark Colorblind", "5.9.0", "dark", ThemeOrigin::BuiltIn, {}, true},
        {"github-light-colorblind", "GitHub Light Colorblind", "5.9.0", "light", ThemeOrigin::BuiltIn, {}, true},
        {"github-auto", "GitHub Auto (System Preference)", "5.9.0", "auto", ThemeOrigin::BuiltIn, {}, true},
    };

    for (const auto& root : roots_) {
        std::error_code error;
        if (!std::filesystem::is_directory(root.path, error)) {
            continue;
        }
        for (const auto& entry :
             std::filesystem::directory_iterator(root.path, error)) {
            if (error || !entry.is_directory()) {
                continue;
            }
            const auto externalTheme =
                loadExternalTheme(
                    entry.path(),
                    root.origin,
                    maxFileBytes_);
            if (!externalTheme) {
                continue;
            }
            themes.erase(
                std::remove_if(
                    themes.begin(),
                    themes.end(),
                    [&](const ThemeInfo& existingTheme) {
                        return existingTheme.id == externalTheme->info.id;
                    }),
                themes.end());
            themes.push_back(externalTheme->info);
        }
    }
    return themes;
}

ThemeResolution ThemeManager::resolve(
    std::string_view themeId,
    bool strict) const {
    ThemeResolution resolution;
    if (!isThemeId(themeId)) {
        resolution.diagnostics.push_back({
            DiagnosticSeverity::Error,
            "MW2002",
            "Theme ID is invalid.",
            std::nullopt,
        });
        return resolution;
    }

    for (auto root = roots_.rbegin(); root != roots_.rend(); ++root) {
        const auto themePath = root->path / std::string(themeId);
        if (!std::filesystem::exists(themePath)) {
            continue;
        }
        std::string validationError;
        const auto external =
            loadExternalTheme(
                themePath,
                root->origin,
                maxFileBytes_,
                &validationError);
        if (!external) {
            resolution.diagnostics.push_back({
                DiagnosticSeverity::Error,
                "MW2004",
                "Theme '" + std::string(themeId) +
                    "' is invalid: " + validationError + '.',
                std::nullopt,
            });
            return resolution;
        }
        resolution.ok = true;
        resolution.id = themeId;
        resolution.css = external->variablesCss;
        for (const auto& cssPath : external->cssPaths) {
            const auto css = readFile(cssPath, maxFileBytes_);
            if (!css) {
                resolution.ok = false;
                resolution.diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    "MW2003",
                    "Theme CSS could not be read: " + cssPath.string(),
                    std::nullopt,
                });
                return resolution;
            }
            if (!resolution.css.empty() && !resolution.css.ends_with('\n')) {
                resolution.css += '\n';
            }
            resolution.css += *css;
        }
        return resolution;
    }

    if (themeId == "github-light") {
        resolution.ok = true; resolution.id = "github-light"; resolution.css = githubLightCss; return resolution;
    }
    if (themeId == "github-dark") {
        resolution.ok = true; resolution.id = "github-dark"; resolution.css = githubDarkCss; return resolution;
    }
    if (themeId == "github-dark-dimmed") {
        resolution.ok = true; resolution.id = "github-dark-dimmed"; resolution.css = githubDarkDimmedCss; return resolution;
    }
    if (themeId == "github-dark-high-contrast") {
        resolution.ok = true; resolution.id = "github-dark-high-contrast"; resolution.css = githubDarkHcCss; return resolution;
    }
    if (themeId == "github-dark-colorblind") {
        resolution.ok = true; resolution.id = "github-dark-colorblind"; resolution.css = githubDarkCbCss; return resolution;
    }
    if (themeId == "github-light-colorblind") {
        resolution.ok = true; resolution.id = "github-light-colorblind"; resolution.css = githubLightCbCss; return resolution;
    }
    if (themeId == "github-auto") {
        resolution.ok = true; resolution.id = "github-auto"; resolution.css = githubAutoCss; return resolution;
    }

    resolution.diagnostics.push_back({
        strict ? DiagnosticSeverity::Error : DiagnosticSeverity::Warning,
        "MW2001",
        "Theme '" + std::string(themeId) +
            "' was not found." +
            (strict ? std::string{} : " Falling back to github-light."),
        std::nullopt,
    });
    if (strict) {
        return resolution;
    }

    resolution.ok = true;
    resolution.id = "github-light";
    resolution.css = githubLightCss;
    return resolution;
}

} // namespace mwrender::detail
