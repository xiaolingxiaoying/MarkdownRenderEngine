#pragma once

#include <string_view>

namespace mwrender {

inline constexpr int versionMajor = MWRENDER_VERSION_MAJOR;
inline constexpr int versionMinor = MWRENDER_VERSION_MINOR;
inline constexpr int versionPatch = MWRENDER_VERSION_PATCH;
inline constexpr std::string_view versionString = "0.1.0";

} // namespace mwrender

