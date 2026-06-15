#pragma once

#include <cstddef>
#include <cstdint>

namespace mwrender {

struct SourcePosition {
    std::size_t offset = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;

    friend bool operator==(const SourcePosition&, const SourcePosition&) = default;
};

struct SourceRange {
    SourcePosition begin;
    SourcePosition end;

    friend bool operator==(const SourceRange&, const SourceRange&) = default;
};

} // namespace mwrender

