#pragma once

#include <cstddef>
#include <string_view>

namespace mwrender {

// Convert UTF-8 byte offset to UTF-16 code unit offset
inline std::size_t utf8ToUtf16Offset(std::string_view utf8, std::size_t byteOffset) {
    std::size_t utf16Offset = 0;
    std::size_t i = 0;
    while (i < byteOffset && i < utf8.size()) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80) {
            i += 1;
        } else if (c < 0xE0) {
            i += 2;
        } else if (c < 0xF0) {
            i += 3;
        } else {
            i += 4;
            utf16Offset++;
        }
        utf16Offset++;
    }
    return utf16Offset;
}

// Convert UTF-16 code unit offset to UTF-8 byte offset
inline std::size_t utf16ToUtf8Offset(std::string_view utf8, std::size_t utf16Offset) {
    std::size_t byteOffset = 0;
    std::size_t units = 0;
    while (units < utf16Offset && byteOffset < utf8.size()) {
        unsigned char c = static_cast<unsigned char>(utf8[byteOffset]);
        if (c < 0x80) {
            byteOffset += 1;
            units += 1;
        } else if (c < 0xE0) {
            byteOffset += 2;
            units += 1;
        } else if (c < 0xF0) {
            byteOffset += 3;
            units += 1;
        } else {
            byteOffset += 4;
            units += 2;
        }
    }
    return byteOffset;
}

} // namespace mwrender
