#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mwrender {

enum class ThemeOrigin : std::uint8_t {
    BuiltIn = 0,
    User = 1,
    Workspace = 2,
    Explicit = 3
};

struct ThemeInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string appearance;
    ThemeOrigin origin = ThemeOrigin::BuiltIn;
    std::filesystem::path root;
    bool available = false;
};

} // namespace mwrender

