#pragma once

#include <cstddef>

namespace mwrender {

struct WordStats {
    std::size_t characters = 0;
    std::size_t charactersNoSpaces = 0;
    std::size_t cjkCharacters = 0;
    std::size_t englishWords = 0;
    std::size_t paragraphs = 0;
    std::size_t headings = 0;
    std::size_t codeLines = 0;
    std::size_t images = 0;
    std::size_t links = 0;
};

} // namespace mwrender

