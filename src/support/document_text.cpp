#include "support/document_text.hpp"

#include <cctype>

namespace mwrender::detail {

std::string plainText(const Node& node) {
    if (node.type == NodeType::Text ||
        node.type == NodeType::InlineCode ||
        node.type == NodeType::Image) {
        return node.literal;
    }

    std::string text;
    for (const auto& child : node.children) {
        text += plainText(*child);
    }
    return text;
}

std::string slugBase(std::string_view text) {
    std::string slug;
    bool pendingDash = false;

    for (std::size_t index = 0; index < text.size();) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte >= 0x80U) {
            if (pendingDash && !slug.empty()) {
                slug += '-';
            }
            pendingDash = false;
            slug += text[index++];
            while (index < text.size() &&
                   (static_cast<unsigned char>(text[index]) & 0xC0U) == 0x80U) {
                slug += text[index++];
            }
            continue;
        }

        if (std::isalnum(byte) != 0) {
            if (pendingDash && !slug.empty()) {
                slug += '-';
            }
            pendingDash = false;
            slug += static_cast<char>(std::tolower(byte));
        } else if (std::isspace(byte) != 0 || text[index] == '-') {
            pendingDash = true;
        }
        ++index;
    }

    return slug.empty() ? "section" : slug;
}

} // namespace mwrender::detail

