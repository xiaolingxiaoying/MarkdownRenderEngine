#include <mwrender/sanitizer.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mwrender {
namespace {

struct Attribute {
    std::string name;
    std::string value;
    bool hasValue = false;
};

struct Tag {
    std::string name;
    std::vector<Attribute> attributes;
    bool closing = false;
    bool selfClosing = false;
};

char asciiLower(char value) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(value)));
}

std::string lowerAscii(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), asciiLower);
    return result;
}

bool isNameCharacter(char value) {
    const auto byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '-' || value == '_' ||
        value == ':';
}

void appendEscapedAttribute(std::string_view value, std::string& output) {
    for (const char character : value) {
        switch (character) {
        case '&':
            output += "&amp;";
            break;
        case '"':
            output += "&quot;";
            break;
        case '<':
            output += "&lt;";
            break;
        case '>':
            output += "&gt;";
            break;
        default:
            output += character;
            break;
        }
    }
}

std::optional<Tag> parseTag(std::string_view source) {
    if (source.size() < 3 || source.front() != '<' || source.back() != '>') {
        return std::nullopt;
    }

    Tag tag;
    std::size_t index = 1;
    while (index < source.size() &&
           std::isspace(static_cast<unsigned char>(source[index])) != 0) {
        ++index;
    }
    if (index < source.size() && source[index] == '/') {
        tag.closing = true;
        ++index;
    }
    const auto nameStart = index;
    while (index < source.size() && isNameCharacter(source[index])) {
        ++index;
    }
    if (index == nameStart) {
        return std::nullopt;
    }
    tag.name = lowerAscii(source.substr(nameStart, index - nameStart));

    while (index + 1 < source.size()) {
        while (index + 1 < source.size() &&
               std::isspace(static_cast<unsigned char>(source[index])) != 0) {
            ++index;
        }
        if (source[index] == '>') {
            break;
        }
        if (source[index] == '/' && source[index + 1] == '>') {
            tag.selfClosing = true;
            break;
        }
        if (tag.closing) {
            return tag;
        }

        const auto attributeStart = index;
        while (index + 1 < source.size() && isNameCharacter(source[index])) {
            ++index;
        }
        if (index == attributeStart) {
            return std::nullopt;
        }

        Attribute attribute;
        attribute.name =
            lowerAscii(source.substr(attributeStart, index - attributeStart));
        while (index + 1 < source.size() &&
               std::isspace(static_cast<unsigned char>(source[index])) != 0) {
            ++index;
        }
        if (source[index] == '=') {
            attribute.hasValue = true;
            ++index;
            while (index + 1 < source.size() &&
                   std::isspace(static_cast<unsigned char>(source[index])) != 0) {
                ++index;
            }
            if (source[index] == '"' || source[index] == '\'') {
                const char quote = source[index++];
                const auto valueStart = index;
                while (index + 1 < source.size() && source[index] != quote) {
                    ++index;
                }
                if (source[index] != quote) {
                    return std::nullopt;
                }
                attribute.value =
                    std::string(source.substr(valueStart, index - valueStart));
                ++index;
            } else {
                const auto valueStart = index;
                while (index + 1 < source.size() &&
                       std::isspace(
                           static_cast<unsigned char>(source[index])) == 0 &&
                       source[index] != '>') {
                    ++index;
                }
                attribute.value =
                    std::string(source.substr(valueStart, index - valueStart));
            }
        }
        tag.attributes.push_back(std::move(attribute));
    }
    return tag;
}

std::size_t findTagEnd(std::string_view html, std::size_t start) {
    char quote = '\0';
    for (std::size_t index = start + 1; index < html.size(); ++index) {
        const char character = html[index];
        if (quote != '\0') {
            if (character == quote) {
                quote = '\0';
            }
        } else if (character == '"' || character == '\'') {
            quote = character;
        } else if (character == '>') {
            return index;
        }
    }
    return std::string_view::npos;
}

std::string decodeSchemePrefix(std::string_view value) {
    std::string normalized;
    for (std::size_t index = 0; index < value.size() && normalized.size() < 32;) {
        const auto byte = static_cast<unsigned char>(value[index]);
        if (std::isspace(byte) != 0 || byte < 0x20U || byte == 0x7FU) {
            ++index;
            continue;
        }
        if (value[index] == '&' && index + 2 < value.size() &&
            value[index + 1] == '#') {
            const bool hexadecimal =
                index + 2 < value.size() &&
                (value[index + 2] == 'x' || value[index + 2] == 'X');
            const auto digitsStart = index + (hexadecimal ? 3 : 2);
            auto digitsEnd = digitsStart;
            while (digitsEnd < value.size() &&
                   (hexadecimal
                        ? std::isxdigit(
                              static_cast<unsigned char>(value[digitsEnd])) != 0
                        : std::isdigit(
                              static_cast<unsigned char>(value[digitsEnd])) != 0)) {
                ++digitsEnd;
            }
            if (digitsEnd > digitsStart) {
                unsigned int codePoint = 0;
                const auto conversion = std::from_chars(
                    value.data() + digitsStart,
                    value.data() + digitsEnd,
                    codePoint,
                    hexadecimal ? 16 : 10);
                if (conversion.ec == std::errc{} && codePoint < 128U) {
                    normalized += asciiLower(static_cast<char>(codePoint));
                    index = digitsEnd +
                        (digitsEnd < value.size() && value[digitsEnd] == ';');
                    continue;
                }
            }
        }
        normalized += asciiLower(value[index++]);
    }
    return normalized;
}

bool isSafeUrl(std::string_view value, bool image) {
    const auto normalized = decodeSchemePrefix(value);
    const auto colon = normalized.find(':');
    const auto delimiter = normalized.find_first_of("/?#");
    if (colon == std::string::npos ||
        (delimiter != std::string::npos && delimiter < colon)) {
        return true;
    }
    const auto scheme = normalized.substr(0, colon);
    return scheme == "http" || scheme == "https" ||
        (!image && (scheme == "mailto" || scheme == "tel"));
}

bool isAllowedAttribute(std::string_view tag, std::string_view attribute) {
    if (attribute == "class" || attribute == "id" ||
        attribute == "title" || attribute == "lang" ||
        attribute == "dir" || attribute.starts_with("aria-") ||
        attribute.starts_with("data-")) {
        return true;
    }
    if (tag == "a") {
        return attribute == "href" || attribute == "rel" ||
            attribute == "target";
    }
    if (tag == "img") {
        return attribute == "src" || attribute == "alt" ||
            attribute == "width" || attribute == "height";
    }
    if (tag == "ol") {
        return attribute == "start" || attribute == "reversed";
    }
    if (tag == "td" || tag == "th") {
        return attribute == "colspan" || attribute == "rowspan" ||
            attribute == "align";
    }
    return tag == "details" && attribute == "open";
}

bool isVoidTag(std::string_view tag) {
    return tag == "br" || tag == "hr" || tag == "img";
}

} // namespace

SanitizeResult SafeHtmlSanitizer::sanitize(
    std::string_view html,
    const SourceRange& range) const {
    static const std::unordered_set<std::string> allowedTags{
        "a", "abbr", "b", "blockquote", "br", "code", "del", "details",
        "div", "em", "h1", "h2", "h3", "h4", "h5", "h6", "hr", "i",
        "img", "kbd", "li", "mark", "ol", "p", "pre", "s", "span",
        "strong", "sub", "summary", "sup", "table", "tbody", "td",
        "tfoot", "th", "thead", "tr", "u", "ul",
    };
    static const std::unordered_set<std::string> blockedContentTags{
        "script", "style", "iframe", "object", "embed", "svg", "math",
        "template", "noscript",
    };

    SanitizeResult result;
    result.accepted = true;
    std::vector<std::string> blocked;
    bool changed = false;

    for (std::size_t index = 0; index < html.size();) {
        if (html[index] != '<') {
            if (blocked.empty()) {
                result.html += html[index];
            }
            ++index;
            continue;
        }
        if (html.substr(index).starts_with("<!--")) {
            const auto end = html.find("-->", index + 4);
            index = end == std::string_view::npos ? html.size() : end + 3;
            changed = true;
            continue;
        }

        const auto end = findTagEnd(html, index);
        if (end == std::string_view::npos) {
            if (blocked.empty()) {
                result.html += "&lt;";
            }
            ++index;
            changed = true;
            continue;
        }
        const auto parsed = parseTag(html.substr(index, end - index + 1));
        index = end + 1;
        if (!parsed) {
            if (blocked.empty()) {
                result.html += "&lt;";
            }
            changed = true;
            continue;
        }

        if (!blocked.empty()) {
            if (!parsed->closing &&
                blockedContentTags.contains(parsed->name)) {
                blocked.push_back(parsed->name);
            } else if (parsed->closing && parsed->name == blocked.back()) {
                blocked.pop_back();
            }
            changed = true;
            continue;
        }
        if (blockedContentTags.contains(parsed->name)) {
            if (!parsed->closing && !parsed->selfClosing) {
                blocked.push_back(parsed->name);
            }
            changed = true;
            continue;
        }
        if (!allowedTags.contains(parsed->name)) {
            changed = true;
            continue;
        }

        if (parsed->closing) {
            if (!isVoidTag(parsed->name)) {
                result.html += "</" + parsed->name + ">";
            }
            continue;
        }

        result.html += '<' + parsed->name;
        for (const auto& attribute : parsed->attributes) {
            if (attribute.name.starts_with("on") ||
                attribute.name == "style" ||
                !isAllowedAttribute(parsed->name, attribute.name)) {
                changed = true;
                continue;
            }
            if ((attribute.name == "href" || attribute.name == "src") &&
                (!attribute.hasValue ||
                 !isSafeUrl(attribute.value, attribute.name == "src"))) {
                changed = true;
                continue;
            }
            result.html += ' ' + attribute.name;
            if (attribute.hasValue) {
                result.html += "=\"";
                appendEscapedAttribute(attribute.value, result.html);
                result.html += '"';
            }
        }
        result.html += '>';
    }

    if (changed) {
        result.diagnostics.push_back({
            DiagnosticSeverity::Warning,
            "MW3004",
            "Unsafe or unsupported raw HTML was removed by the built-in sanitizer.",
            range,
        });
    }
    return result;
}

} // namespace mwrender
