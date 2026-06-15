#pragma once

#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>

#include <mwrender/diagnostics.hpp>
#include <mwrender/theme.hpp>

namespace mwrender::detail {

struct ThemeRoot {
    std::filesystem::path path;
    ThemeOrigin origin = ThemeOrigin::Explicit;
    std::size_t registrationOrder = 0;
};

struct ThemeResolution {
    bool ok = false;
    std::string id;
    std::string css;
    std::vector<Diagnostic> diagnostics;
};

class ThemeManager {
public:
    explicit ThemeManager(
        std::size_t maxFileBytes = 2U * 1024U * 1024U);

    void addRoot(std::filesystem::path path, ThemeOrigin origin);

    [[nodiscard]] std::vector<ThemeInfo> listThemes() const;
    [[nodiscard]] ThemeResolution resolve(
        std::string_view themeId,
        bool strict) const;

private:
    std::size_t maxFileBytes_;
    std::vector<ThemeRoot> roots_;
};

} // namespace mwrender::detail
