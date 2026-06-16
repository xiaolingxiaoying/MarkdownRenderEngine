#include <mwrender/source_map.hpp>

#include <algorithm>
#include <cstdint>

namespace mwrender {

void SourceMap::build(const std::string& source) {
    sourceOffsets_.clear();
    visualOffsets_.clear();

    sourceOffsets_.reserve(source.size() + 1);
    visualOffsets_.reserve(source.size() + 1);

    std::size_t visualOffset = 0;

    auto flushChar = [&](std::size_t sourcePos) {
        sourceOffsets_.push_back(sourcePos);
        visualOffsets_.push_back(visualOffset);
        ++visualOffset;
    };

    auto skipChar = [&](std::size_t sourcePos) {
        sourceOffsets_.push_back(sourcePos);
        visualOffsets_.push_back(visualOffset);
    };

    for (std::size_t i = 0; i < source.size(); ++i) {
        char c = source[i];

        if (c == '\\' && i + 1 < source.size()) {
            flushChar(i);
            ++i;
            flushChar(i);
            continue;
        }

        if (c == '*' || c == '_') {
            std::size_t count = 1;
            while (i + count < source.size() && source[i + count] == c) {
                ++count;
            }
            if (count >= 2) {
                auto closer = source.find(std::string(count, c), i + count);
                if (closer != std::string_view::npos && closer > i + count) {
                    for (std::size_t j = 0; j < count; ++j) {
                        skipChar(i + j);
                    }
                    for (std::size_t j = i + count; j < closer; ++j) {
                        flushChar(j);
                    }
                    for (std::size_t j = 0; j < count; ++j) {
                        skipChar(closer + j);
                    }
                    i = closer + count - 1;
                    continue;
                }
            } else if (count == 1) {
                auto closer = source.find(c, i + 1);
                if (closer != std::string_view::npos) {
                    skipChar(i);
                    for (std::size_t j = i + 1; j < closer; ++j) {
                        flushChar(j);
                    }
                    skipChar(closer);
                    i = closer;
                    continue;
                }
            }
        }

        if (c == '`') {
            std::size_t count = 1;
            while (i + count < source.size() && source[i + count] == '`') {
                ++count;
            }
            auto closer = source.find(std::string(count, '`'), i + count);
            if (closer != std::string_view::npos) {
                for (std::size_t j = 0; j < count; ++j) {
                    skipChar(i + j);
                }
                auto inner = source.substr(i + count, closer - i - count);
                for (std::size_t j = i + count; j < closer; ++j) {
                    if (source[j] == '\n') {
                        flushChar(j);
                    } else {
                        skipChar(j);
                    }
                }
                for (std::size_t j = 0; j < count; ++j) {
                    skipChar(closer + j);
                }
                i = closer + count - 1;
                continue;
            }
        }

        if (c == '!' && i + 1 < source.size() && source[i + 1] == '[') {
            auto labelEnd = source.find(']', i + 2);
            if (labelEnd != std::string_view::npos &&
                labelEnd + 1 < source.size() && source[labelEnd + 1] == '(') {
                auto destEnd = source.find(')', labelEnd + 2);
                if (destEnd != std::string_view::npos) {
                    skipChar(i);
                    skipChar(i + 1);
                    for (std::size_t j = i + 2; j < labelEnd; ++j) {
                        flushChar(j);
                    }
                    for (std::size_t j = labelEnd; j <= destEnd; ++j) {
                        skipChar(j);
                    }
                    i = destEnd;
                    continue;
                }
            }
        }

        if (c == '[') {
            auto labelEnd = source.find(']', i + 1);
            if (labelEnd != std::string_view::npos &&
                labelEnd + 1 < source.size() && source[labelEnd + 1] == '(') {
                auto destEnd = source.find(')', labelEnd + 2);
                if (destEnd != std::string_view::npos) {
                    skipChar(i);
                    for (std::size_t j = i + 1; j < labelEnd; ++j) {
                        flushChar(j);
                    }
                    for (std::size_t j = labelEnd; j <= destEnd; ++j) {
                        skipChar(j);
                    }
                    i = destEnd;
                    continue;
                }
            }
        }

        if (c == '~' && i + 1 < source.size() && source[i + 1] == '~') {
            auto closer = source.find("~~", i + 2);
            if (closer != std::string_view::npos) {
                skipChar(i);
                skipChar(i + 1);
                for (std::size_t j = i + 2; j < closer; ++j) {
                    flushChar(j);
                }
                skipChar(closer);
                skipChar(closer + 1);
                i = closer + 1;
                continue;
            }
        }

        if (c == '$') {
            std::size_t count = 1;
            if (i + 1 < source.size() && source[i + 1] == '$') {
                count = 2;
            }
            std::string delim(count, '$');
            auto closer = source.find(delim, i + count);
            if (closer != std::string_view::npos) {
                for (std::size_t j = 0; j < count; ++j) {
                    skipChar(i + j);
                }
                for (std::size_t j = i + count; j < closer; ++j) {
                    flushChar(j);
                }
                for (std::size_t j = 0; j < count; ++j) {
                    skipChar(closer + j);
                }
                i = closer + count - 1;
                continue;
            }
        }

        flushChar(i);
    }
    sourceOffsets_.push_back(source.size());
    visualOffsets_.push_back(visualOffset);
}

SourcePosition SourceMap::visualToSource(std::size_t visualOffset) const {
    auto it = std::upper_bound(visualOffsets_.begin(), visualOffsets_.end(),
                                visualOffset);
    if (it == visualOffsets_.begin()) {
        return {0, 1, 1};
    }
    std::size_t idx = std::distance(visualOffsets_.begin(), it) - 1;
    return {sourceOffsets_[idx], 1, 1};
}

std::size_t SourceMap::sourceToVisual(std::size_t sourceOffset) const {
    auto it = std::upper_bound(sourceOffsets_.begin(), sourceOffsets_.end(),
                                sourceOffset);
    if (it == sourceOffsets_.begin()) {
        return 0;
    }
    std::size_t idx = std::distance(sourceOffsets_.begin(), it) - 1;
    return visualOffsets_[idx];
}

} // namespace mwrender
