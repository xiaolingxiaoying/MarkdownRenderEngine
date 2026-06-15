#include "analysis/document_analysis.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

#include "support/document_text.hpp"

namespace mwrender::detail {
namespace {

void appendOutlineItem(
    std::vector<OutlineItem>& roots,
    OutlineItem item) {
    if (roots.empty() || item.level <= roots.back().level) {
        roots.push_back(std::move(item));
        return;
    }

    auto* children = &roots.back().children;
    while (!children->empty() && item.level > children->back().level) {
        children = &children->back().children;
    }
    children->push_back(std::move(item));
}

std::uint32_t decodeCodePoint(
    std::string_view text,
    std::size_t& index) {
    const auto first = static_cast<unsigned char>(text[index++]);
    if (first < 0x80U) {
        return first;
    }
    int continuationCount = 0;
    std::uint32_t codePoint = 0;
    if ((first & 0xE0U) == 0xC0U) {
        continuationCount = 1;
        codePoint = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
        continuationCount = 2;
        codePoint = first & 0x0FU;
    } else {
        continuationCount = 3;
        codePoint = first & 0x07U;
    }
    for (int count = 0;
         count < continuationCount && index < text.size();
         ++count) {
        codePoint =
            (codePoint << 6U) |
            (static_cast<unsigned char>(text[index++]) & 0x3FU);
    }
    return codePoint;
}

bool isCjk(std::uint32_t codePoint) {
    return (codePoint >= 0x3400U && codePoint <= 0x4DBFU) ||
        (codePoint >= 0x4E00U && codePoint <= 0x9FFFU) ||
        (codePoint >= 0xF900U && codePoint <= 0xFAFFU) ||
        (codePoint >= 0x3040U && codePoint <= 0x30FFU) ||
        (codePoint >= 0xAC00U && codePoint <= 0xD7AFU);
}

void countText(std::string_view text, WordStats& stats) {
    bool inEnglishWord = false;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto codePoint = decodeCodePoint(text, index);
        ++stats.characters;
        const bool whitespace =
            codePoint < 128U &&
            std::isspace(static_cast<unsigned char>(codePoint)) != 0;
        if (!whitespace) {
            ++stats.charactersNoSpaces;
        }
        if (isCjk(codePoint)) {
            ++stats.cjkCharacters;
        }

        const bool english =
            codePoint < 128U &&
            (std::isalnum(static_cast<unsigned char>(codePoint)) != 0 ||
             codePoint == '\'' || codePoint == '-');
        if (english && !inEnglishWord) {
            ++stats.englishWords;
        }
        inEnglishWord = english;
    }
}

void collectStats(const Node& node, WordStats& stats, bool inCode) {
    if (node.type == NodeType::Paragraph) {
        ++stats.paragraphs;
    } else if (node.type == NodeType::Heading) {
        ++stats.headings;
    } else if (node.type == NodeType::Image) {
        ++stats.images;
    } else if (node.type == NodeType::Link ||
               node.type == NodeType::AutoLink) {
        ++stats.links;
    } else if (node.type == NodeType::CodeBlock) {
        if (!node.literal.empty()) {
            stats.codeLines += static_cast<std::size_t>(
                std::count(node.literal.begin(), node.literal.end(), '\n'));
            if (node.literal.back() != '\n') {
                ++stats.codeLines;
            }
        }
        return;
    }

    if (!inCode && node.type == NodeType::Text) {
        countText(node.literal, stats);
    }
    for (const auto& child : node.children) {
        collectStats(*child, stats, inCode);
    }
}

} // namespace

std::vector<OutlineItem> buildOutline(const Node& document) {
    std::vector<OutlineItem> outline;
    std::unordered_map<std::string, std::size_t> slugCounts;
    for (const auto& node : document.children) {
        if (node->type != NodeType::Heading) {
            continue;
        }
        const auto* heading = std::get_if<HeadingData>(&node->payload);
        const auto title = plainText(*node);
        auto id = slugBase(title);
        auto& count = slugCounts[id];
        if (count > 0) {
            id += '-' + std::to_string(count);
        }
        ++count;
        appendOutlineItem(outline, {
            heading ? heading->level : 1,
            title,
            id,
            static_cast<int>(node->range.begin.line),
            {},
        });
    }
    return outline;
}

WordStats buildWordStats(const Node& document) {
    WordStats stats;
    collectStats(document, stats, false);
    return stats;
}

} // namespace mwrender::detail
