#include <mwrender/parser.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace mwrender {
namespace {

struct Line {
    std::size_t startOffset = 0;
    std::size_t endOffset = 0;
    std::size_t nextOffset = 0;
    std::uint32_t number = 1;
    std::string_view text;
};

bool isAsciiSpace(char value) {
    return value == ' ' || value == '\t';
}

std::string_view trimLeft(std::string_view value) {
    while (!value.empty() && isAsciiSpace(value.front())) {
        value.remove_prefix(1);
    }
    return value;
}

std::string_view trimRight(std::string_view value) {
    while (!value.empty() && isAsciiSpace(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

std::string_view trim(std::string_view value) {
    return trimRight(trimLeft(value));
}

bool isBlank(std::string_view value) {
    return trim(value).empty();
}

SourceRange makeRange(
    std::size_t beginOffset,
    std::uint32_t beginLine,
    std::uint32_t beginColumn,
    std::size_t endOffset,
    std::uint32_t endLine,
    std::uint32_t endColumn) {
    return {
        {beginOffset, beginLine, beginColumn},
        {endOffset, endLine, endColumn},
    };
}

std::vector<Line> scanLines(std::string_view source) {
    std::vector<Line> lines;
    std::size_t offset = 0;
    std::uint32_t number = 1;

    while (offset < source.size()) {
        const std::size_t start = offset;
        while (offset < source.size() &&
               source[offset] != '\n' &&
               source[offset] != '\r') {
            ++offset;
        }

        const std::size_t end = offset;
        if (offset < source.size() && source[offset] == '\r') {
            ++offset;
            if (offset < source.size() && source[offset] == '\n') {
                ++offset;
            }
        } else if (offset < source.size()) {
            ++offset;
        }

        lines.push_back({
            start,
            end,
            offset,
            number++,
            source.substr(start, end - start),
        });
    }

    if (source.empty()) {
        return lines;
    }

    if (source.back() == '\n' || source.back() == '\r') {
        lines.push_back({
            source.size(),
            source.size(),
            source.size(),
            number,
            {},
        });
    }

    return lines;
}

bool isValidUtf8(std::string_view value) {
    std::size_t index = 0;
    while (index < value.size()) {
        const auto byte = static_cast<unsigned char>(value[index]);
        std::size_t continuationCount = 0;
        std::uint32_t codePoint = 0;

        if (byte <= 0x7F) {
            ++index;
            continue;
        }
        if ((byte & 0xE0U) == 0xC0U) {
            continuationCount = 1;
            codePoint = byte & 0x1FU;
            if (codePoint == 0) {
                return false;
            }
        } else if ((byte & 0xF0U) == 0xE0U) {
            continuationCount = 2;
            codePoint = byte & 0x0FU;
        } else if ((byte & 0xF8U) == 0xF0U) {
            continuationCount = 3;
            codePoint = byte & 0x07U;
        } else {
            return false;
        }

        if (index + continuationCount >= value.size()) {
            return false;
        }

        for (std::size_t count = 0; count < continuationCount; ++count) {
            const auto continuation =
                static_cast<unsigned char>(value[index + count + 1]);
            if ((continuation & 0xC0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }

        if ((continuationCount == 1 && codePoint < 0x80U) ||
            (continuationCount == 2 && codePoint < 0x800U) ||
            (continuationCount == 3 && codePoint < 0x10000U) ||
            codePoint > 0x10FFFFU ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
        index += continuationCount + 1;
    }
    return true;
}

std::string replaceInvalidUtf8(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    std::size_t index = 0;

    while (index < value.size()) {
        const auto byte = static_cast<unsigned char>(value[index]);
        std::size_t length = 0;
        if (byte <= 0x7F) {
            length = 1;
        } else if ((byte & 0xE0U) == 0xC0U) {
            length = 2;
        } else if ((byte & 0xF0U) == 0xE0U) {
            length = 3;
        } else if ((byte & 0xF8U) == 0xF0U) {
            length = 4;
        }

        bool valid = length != 0 && index + length <= value.size();
        for (std::size_t part = 1; valid && part < length; ++part) {
            valid =
                (static_cast<unsigned char>(value[index + part]) & 0xC0U) ==
                0x80U;
        }

        if (valid && isValidUtf8(value.substr(index, length))) {
            result.append(value.substr(index, length));
            index += length;
        } else {
            result += "\xEF\xBF\xBD";
            ++index;
        }
    }
    return result;
}

void appendText(
    std::vector<std::unique_ptr<Node>>& children,
    std::string_view text,
    const SourceRange& range) {
    if (text.empty()) {
        return;
    }
    if (!children.empty() && children.back()->type == NodeType::Text) {
        children.back()->literal.append(text);
        children.back()->range.end = range.end;
        return;
    }

    auto node = std::make_unique<Node>();
    node->type = NodeType::Text;
    node->range = range;
    node->literal = text;
    children.push_back(std::move(node));
}

std::optional<std::pair<std::string, std::string>> parseDestination(
    std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::nullopt;
    }

    std::string destination;
    std::string title;
    if (value.front() == '<') {
        const auto close = value.find('>');
        if (close == std::string_view::npos) {
            return std::nullopt;
        }
        destination = value.substr(1, close - 1);
        value.remove_prefix(close + 1);
    } else {
        std::size_t end = 0;
        while (end < value.size() && !isAsciiSpace(value[end])) {
            ++end;
        }
        destination = value.substr(0, end);
        value.remove_prefix(end);
    }

    value = trim(value);
    if (!value.empty() &&
        (value.front() == '"' || value.front() == '\'')) {
        const char quote = value.front();
        if (value.size() < 2 || value.back() != quote) {
            return std::nullopt;
        }
        title = value.substr(1, value.size() - 2);
    } else if (!value.empty()) {
        return std::nullopt;
    }

    return std::pair{std::move(destination), std::move(title)};
}

std::vector<std::unique_ptr<Node>> parseInline(
    std::string_view source,
    const SourcePosition& base,
    const MarkdownExtensions& extensions);

std::unique_ptr<Node> makeInlineContainer(
    NodeType type,
    std::string_view content,
    std::size_t markerWidth,
    std::size_t sourceStart,
    const SourcePosition& base,
    const MarkdownExtensions& extensions) {
    auto node = std::make_unique<Node>();
    node->type = type;
    node->range = makeRange(
        base.offset + sourceStart,
        base.line,
        base.column + static_cast<std::uint32_t>(sourceStart),
        base.offset + sourceStart + content.size() + markerWidth * 2,
        base.line,
        base.column + static_cast<std::uint32_t>(
                          sourceStart + content.size() + markerWidth * 2));
    SourcePosition contentBase{
        base.offset + sourceStart + markerWidth,
        base.line,
        base.column + static_cast<std::uint32_t>(sourceStart + markerWidth),
    };
    node->children = parseInline(content, contentBase, extensions);
    return node;
}

void trimTrailingSpaces(std::vector<std::unique_ptr<Node>>& children) {
    if (children.empty() || children.back()->type != NodeType::Text) {
        return;
    }
    auto& literal = children.back()->literal;
    while (!literal.empty() && literal.back() == ' ') {
        literal.pop_back();
    }
    if (literal.empty()) {
        children.pop_back();
    }
}

std::vector<std::unique_ptr<Node>> parseInline(
    std::string_view source,
    const SourcePosition& base,
    const MarkdownExtensions& extensions) {
    std::vector<std::unique_ptr<Node>> children;
    std::size_t index = 0;

    const auto rangeFor = [&](std::size_t begin, std::size_t end) {
        return makeRange(
            base.offset + begin,
            base.line,
            base.column + static_cast<std::uint32_t>(begin),
            base.offset + end,
            base.line,
            base.column + static_cast<std::uint32_t>(end));
    };

    while (index < source.size()) {
        if (source[index] == '\\' && index + 1 < source.size()) {
            const char escaped = source[index + 1];
            if (std::ispunct(static_cast<unsigned char>(escaped)) != 0) {
                appendText(
                    children,
                    source.substr(index + 1, 1),
                    rangeFor(index, index + 2));
                index += 2;
                continue;
            }
        }

        if (source[index] == '`') {
            std::size_t markerLength = 1;
            while (index + markerLength < source.size() &&
                   source[index + markerLength] == '`') {
                ++markerLength;
            }
            const std::string marker(markerLength, '`');
            const auto close = source.find(marker, index + markerLength);
            if (close != std::string_view::npos) {
                auto node = std::make_unique<Node>();
                node->type = NodeType::InlineCode;
                node->range =
                    rangeFor(index, close + markerLength);
                node->literal =
                    source.substr(index + markerLength, close - index - markerLength);
                std::replace(node->literal.begin(), node->literal.end(), '\n', ' ');
                node->literal = trim(node->literal);
                children.push_back(std::move(node));
                index = close + markerLength;
                continue;
            }
        }

        const bool isImage =
            source[index] == '!' &&
            index + 1 < source.size() &&
            source[index + 1] == '[';
        if (isImage || source[index] == '[') {
            const std::size_t labelStart = index + (isImage ? 2 : 1);
            const auto labelEnd = source.find(']', labelStart);
            if (labelEnd != std::string_view::npos &&
                labelEnd + 1 < source.size() &&
                source[labelEnd + 1] == '(') {
                const auto destinationEnd = source.find(')', labelEnd + 2);
                if (destinationEnd != std::string_view::npos) {
                    const auto parsed = parseDestination(
                        source.substr(
                            labelEnd + 2,
                            destinationEnd - labelEnd - 2));
                    if (parsed) {
                        auto node = std::make_unique<Node>();
                        node->type = isImage ? NodeType::Image : NodeType::Link;
                        node->range = rangeFor(index, destinationEnd + 1);
                        const auto label =
                            source.substr(labelStart, labelEnd - labelStart);
                        if (isImage) {
                            node->payload =
                                ImageData{parsed->first, parsed->second};
                            node->literal = label;
                        } else {
                            node->payload =
                                LinkData{parsed->first, parsed->second};
                            SourcePosition labelBase{
                                base.offset + labelStart,
                                base.line,
                                base.column +
                                    static_cast<std::uint32_t>(labelStart),
                            };
                            node->children =
                                parseInline(label, labelBase, extensions);
                        }
                        children.push_back(std::move(node));
                        index = destinationEnd + 1;
                        continue;
                    }
                }
            }
        }

        if (extensions.strikethrough &&
            source[index] == '~' &&
            index + 1 < source.size() &&
            source[index + 1] == '~') {
            const auto close = source.find("~~", index + 2);
            if (close != std::string_view::npos && close > index + 2) {
                children.push_back(makeInlineContainer(
                    NodeType::Strikethrough,
                    source.substr(index + 2, close - index - 2),
                    2,
                    index,
                    base,
                    extensions));
                index = close + 2;
                continue;
            }
        }

        if ((source[index] == '*' || source[index] == '_')) {
            const char marker = source[index];
            std::size_t count = 1;
            while (index + count < source.size() && source[index + count] == marker) {
                count++;
            }
            if (count > 3) count = 3;
            
            const std::string delimiter(count, marker);
            const auto close = source.find(delimiter, index + count);
            if (close != std::string_view::npos && close > index + count) {
                if (count == 3) {
                    auto strongNode = std::make_unique<Node>();
                    strongNode->type = NodeType::Strong;
                    strongNode->range = rangeFor(index, close + 3);
                    
                    auto emNode = std::make_unique<Node>();
                    emNode->type = NodeType::Emphasis;
                    emNode->range = rangeFor(index + 1, close + 2);
                    
                    SourcePosition innerBase = base;
                    innerBase.offset += index + 3;
                    innerBase.column += static_cast<std::uint32_t>(index + 3);
                    
                    emNode->children = parseInline(source.substr(index + 3, close - index - 3), innerBase, extensions);
                    strongNode->children.push_back(std::move(emNode));
                    children.push_back(std::move(strongNode));
                    index = close + 3;
                    continue;
                } else {
                    children.push_back(makeInlineContainer(
                        count == 2 ? NodeType::Strong : NodeType::Emphasis,
                        source.substr(index + count, close - index - count),
                        count,
                        index,
                        base,
                        extensions));
                    index = close + count;
                    continue;
                }
            }
        }

        if (source[index] == '<') {
            const auto close = source.find('>', index + 1);
            if (close != std::string_view::npos) {
                const auto candidate =
                    source.substr(index, close - index + 1);
                const auto inside = candidate.substr(1, candidate.size() - 2);
                if (extensions.autoLinks &&
                    (inside.starts_with("http://") ||
                     inside.starts_with("https://") ||
                     inside.starts_with("mailto:"))) {
                    auto node = std::make_unique<Node>();
                    node->type = NodeType::AutoLink;
                    node->range = rangeFor(index, close + 1);
                    node->payload =
                        LinkData{std::string(inside), std::string{}};
                    node->literal = inside.starts_with("mailto:")
                        ? std::string(inside.substr(7))
                        : std::string(inside);
                    children.push_back(std::move(node));
                    index = close + 1;
                    continue;
                }
                const bool tagLike =
                    !inside.empty() &&
                    (std::isalpha(static_cast<unsigned char>(inside.front())) != 0 ||
                     inside.front() == '/' ||
                     inside.front() == '!' ||
                     inside.front() == '?');
                if (tagLike) {
                    auto node = std::make_unique<Node>();
                    node->type = NodeType::HtmlInline;
                    node->range = rangeFor(index, close + 1);
                    node->payload = HtmlData{std::string(candidate)};
                    children.push_back(std::move(node));
                    index = close + 1;
                    continue;
                }
            }
        }

        if (source[index] == '\n' || source[index] == '\r') {
            const bool hardBreak =
                index >= 2 && source[index - 1] == ' ' && source[index - 2] == ' ';
            if (hardBreak) {
                trimTrailingSpaces(children);
            }
            auto node = std::make_unique<Node>();
            node->type = hardBreak ? NodeType::HardBreak : NodeType::SoftBreak;
            std::size_t end = index + 1;
            if (source[index] == '\r' &&
                end < source.size() &&
                source[end] == '\n') {
                ++end;
            }
            node->range = rangeFor(index, end);
            children.push_back(std::move(node));
            index = end;
            continue;
        }

        std::size_t end = index + 1;
        while (end < source.size() &&
               source[end] != '\\' &&
               source[end] != '`' &&
               source[end] != '!' &&
               source[end] != '[' &&
               source[end] != '*' &&
               source[end] != '_' &&
               source[end] != '~' &&
               source[end] != '<' &&
               source[end] != '\n' &&
               source[end] != '\r') {
            ++end;
        }
        appendText(children, source.substr(index, end - index), rangeFor(index, end));
        index = end;
    }

    return children;
}

struct HeadingMatch {
    std::uint8_t level = 0;
    std::size_t contentOffset = 0;
    std::string_view content;
};

std::optional<HeadingMatch> matchHeading(std::string_view line) {
    std::size_t indent = 0;
    while (indent < line.size() && line[indent] == ' ' && indent < 4) {
        ++indent;
    }
    if (indent > 3) {
        return std::nullopt;
    }

    std::size_t level = 0;
    while (indent + level < line.size() &&
           line[indent + level] == '#' &&
           level < 6) {
        ++level;
    }
    if (level == 0 ||
        (indent + level < line.size() &&
         !isAsciiSpace(line[indent + level]))) {
        return std::nullopt;
    }

    std::size_t contentOffset = indent + level;
    while (contentOffset < line.size() && isAsciiSpace(line[contentOffset])) {
        ++contentOffset;
    }
    auto content = trimRight(line.substr(contentOffset));
    while (!content.empty() && content.back() == '#') {
        content.remove_suffix(1);
    }
    content = trimRight(content);
    return HeadingMatch{
        static_cast<std::uint8_t>(level),
        contentOffset,
        content,
    };
}

struct FenceMatch {
    char marker = 0;
    std::size_t length = 0;
    std::size_t indent = 0;
    std::string_view info;
};

std::optional<FenceMatch> matchFence(std::string_view line) {
    std::size_t indent = 0;
    while (indent < line.size() && line[indent] == ' ' && indent < 4) {
        ++indent;
    }
    if (indent > 3 || indent >= line.size()) {
        return std::nullopt;
    }
    const char marker = line[indent];
    if (marker != '`' && marker != '~') {
        return std::nullopt;
    }
    std::size_t length = 0;
    while (indent + length < line.size() &&
           line[indent + length] == marker) {
        ++length;
    }
    if (length < 3) {
        return std::nullopt;
    }
    auto info = trim(line.substr(indent + length));
    if (marker == '`' && info.find('`') != std::string_view::npos) {
        return std::nullopt;
    }
    return FenceMatch{marker, length, indent, info};
}

bool isClosingFence(std::string_view line, const FenceMatch& opening) {
    line = trimLeft(line);
    std::size_t length = 0;
    while (length < line.size() && line[length] == opening.marker) {
        ++length;
    }
    return length >= opening.length && trim(line.substr(length)).empty();
}

bool isThematicBreak(std::string_view line) {
    line = trim(line);
    if (line.empty()) {
        return false;
    }
    const char marker = line.front();
    if (marker != '*' && marker != '-' && marker != '_') {
        return false;
    }
    std::size_t count = 0;
    for (const char value : line) {
        if (value == marker) {
            ++count;
        } else if (!isAsciiSpace(value)) {
            return false;
        }
    }
    return count >= 3;
}

struct ListMarker {
    bool ordered = false;
    std::uint64_t start = 1;
    std::size_t contentOffset = 0;
};

std::optional<ListMarker> matchListMarker(std::string_view line) {
    std::size_t indent = 0;
    while (indent < line.size() && line[indent] == ' ' && indent < 4) {
        ++indent;
    }
    if (indent > 3 || indent >= line.size()) {
        return std::nullopt;
    }

    if ((line[indent] == '-' || line[indent] == '+' || line[indent] == '*') &&
        indent + 1 < line.size() &&
        isAsciiSpace(line[indent + 1])) {
        std::size_t content = indent + 2;
        while (content < line.size() && isAsciiSpace(line[content])) {
            ++content;
        }
        return ListMarker{false, 1, content};
    }

    std::size_t digitEnd = indent;
    while (digitEnd < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[digitEnd])) != 0 &&
           digitEnd - indent < 9) {
        ++digitEnd;
    }
    if (digitEnd == indent ||
        digitEnd >= line.size() ||
        (line[digitEnd] != '.' && line[digitEnd] != ')') ||
        digitEnd + 1 >= line.size() ||
        !isAsciiSpace(line[digitEnd + 1])) {
        return std::nullopt;
    }

    std::uint64_t start = 0;
    for (std::size_t index = indent; index < digitEnd; ++index) {
        start = start * 10 + static_cast<std::uint64_t>(line[index] - '0');
    }
    std::size_t content = digitEnd + 2;
    while (content < line.size() && isAsciiSpace(line[content])) {
        ++content;
    }
    return ListMarker{true, start, content};
}

std::vector<std::string> splitTableRow(std::string_view line) {
    line = trim(line);
    if (!line.empty() && line.front() == '|') {
        line.remove_prefix(1);
    }
    if (!line.empty() && line.back() == '|') {
        line.remove_suffix(1);
    }

    std::vector<std::string> cells;
    std::string current;
    bool escaped = false;
    for (const char character : line) {
        if (escaped) {
            current += character;
            escaped = false;
        } else if (character == '\\') {
            escaped = true;
            current += character;
        } else if (character == '|') {
            cells.emplace_back(trim(current));
            current.clear();
        } else {
            current += character;
        }
    }
    if (escaped) {
        current += '\\';
    }
    cells.emplace_back(trim(current));
    return cells;
}

std::optional<std::vector<std::string>> parseTableDelimiter(
    std::string_view line) {
    if (line.find('|') == std::string_view::npos) {
        return std::nullopt;
    }
    const auto cells = splitTableRow(line);
    if (cells.empty()) {
        return std::nullopt;
    }

    std::vector<std::string> alignments;
    alignments.reserve(cells.size());
    for (auto cell : cells) {
        auto value = trim(cell);
        const bool left = !value.empty() && value.front() == ':';
        const bool right = !value.empty() && value.back() == ':';
        if (left) {
            value.remove_prefix(1);
        }
        if (right && !value.empty()) {
            value.remove_suffix(1);
        }
        value = trim(value);
        if (value.empty() ||
            !std::all_of(value.begin(), value.end(), [](char character) {
                return character == '-';
            })) {
            return std::nullopt;
        }
        if (left && right) {
            alignments.emplace_back("center");
        } else if (right) {
            alignments.emplace_back("right");
        } else if (left) {
            alignments.emplace_back("left");
        } else {
            alignments.emplace_back();
        }
    }
    return alignments;
}

bool startsBlock(std::string_view line) {
    const auto stripped = trimLeft(line);
    return matchFence(line).has_value() ||
        matchHeading(line).has_value() ||
        isThematicBreak(line) ||
        matchListMarker(line).has_value() ||
        (!stripped.empty() && stripped.front() == '>') ||
        (!stripped.empty() && stripped.front() == '<');
}

std::unique_ptr<Node> makeParagraph(
    std::string_view source,
    const Line& first,
    const Line& last,
    std::size_t contentStart,
    std::size_t contentEnd,
    const MarkdownExtensions& extensions) {
    auto paragraph = std::make_unique<Node>();
    paragraph->type = NodeType::Paragraph;
    paragraph->range = makeRange(
        contentStart,
        first.number,
        static_cast<std::uint32_t>(contentStart - first.startOffset + 1),
        contentEnd,
        last.number,
        static_cast<std::uint32_t>(contentEnd - last.startOffset + 1));
    paragraph->literal = source.substr(contentStart, contentEnd - contentStart);
    paragraph->children = parseInline(
        paragraph->literal,
        paragraph->range.begin,
        extensions);
    return paragraph;
}

std::unique_ptr<Node> makeSyntheticParagraph(
    std::string text,
    const SourceRange& range,
    const MarkdownExtensions& extensions) {
    auto paragraph = std::make_unique<Node>();
    paragraph->type = NodeType::Paragraph;
    paragraph->range = range;
    paragraph->literal = std::move(text);
    paragraph->children =
        parseInline(paragraph->literal, range.begin, extensions);
    return paragraph;
}

std::unique_ptr<Node> makeDocument(
    std::string_view source,
    const std::vector<Line>& lines,
    std::vector<Diagnostic>& diagnostics,
    DocumentMetadata& metadata,
    const MarkdownExtensions& extensions,
    bool isRoot = true) {
    auto document = std::make_unique<Node>();
    document->type = NodeType::Document;
    if (!lines.empty()) {
        document->range = makeRange(
            0,
            1,
            1,
            source.size(),
            lines.back().number,
            static_cast<std::uint32_t>(lines.back().text.size() + 1));
    } else {
        document->range = makeRange(0, 1, 1, 0, 1, 1);
    }

    std::size_t lineIndex = 0;
    if (isRoot && !lines.empty() && trim(lines.front().text) == "---") {
        std::size_t closing = 1;
        while (closing < lines.size() &&
               trim(lines[closing].text) != "---") {
            ++closing;
        }
        if (closing < lines.size()) {
            auto frontMatter = std::make_unique<Node>();
            frontMatter->type = NodeType::FrontMatter;
            frontMatter->range = makeRange(
                lines.front().startOffset,
                lines.front().number,
                1,
                lines[closing].endOffset,
                lines[closing].number,
                static_cast<std::uint32_t>(lines[closing].text.size() + 1));
            frontMatter->literal = source.substr(
                lines.front().startOffset,
                lines[closing].endOffset - lines.front().startOffset);
            document->children.push_back(std::move(frontMatter));

            bool readingCssList = false;
            for (std::size_t metadataLine = 1;
                 metadataLine < closing;
                 ++metadataLine) {
                auto value = trim(lines[metadataLine].text);
                if (value.empty() || value.front() == '#') {
                    continue;
                }
                if (readingCssList && value.starts_with("- ")) {
                    metadata.css.emplace_back(trim(value.substr(2)));
                    continue;
                }
                readingCssList = false;
                const auto colon = value.find(':');
                if (colon == std::string_view::npos) {
                    diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW1101",
                        "Unsupported front matter line was ignored.",
                        makeRange(
                            lines[metadataLine].startOffset,
                            lines[metadataLine].number,
                            1,
                            lines[metadataLine].endOffset,
                            lines[metadataLine].number,
                            static_cast<std::uint32_t>(
                                lines[metadataLine].text.size() + 1)),
                    });
                    continue;
                }
                const auto key = trim(value.substr(0, colon));
                auto scalar = trim(value.substr(colon + 1));
                if (scalar.size() >= 2 &&
                    ((scalar.front() == '"' && scalar.back() == '"') ||
                     (scalar.front() == '\'' && scalar.back() == '\''))) {
                    scalar = scalar.substr(1, scalar.size() - 2);
                }
                if (key == "title") {
                    metadata.title = scalar;
                } else if (key == "theme") {
                    metadata.theme = scalar;
                } else if (key == "css") {
                    if (scalar.empty()) {
                        readingCssList = true;
                    } else {
                        metadata.css.emplace_back(scalar);
                    }
                } else {
                    diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW1102",
                        "Unknown front matter key '" + std::string(key) +
                            "' was ignored.",
                        std::nullopt,
                    });
                }
            }
            lineIndex = closing + 1;
        } else {
            diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW1103",
                "Front matter opening delimiter has no closing delimiter.",
                std::nullopt,
            });
        }
    }

    while (lineIndex < lines.size()) {
        const Line& line = lines[lineIndex];
        if (isBlank(line.text)) {
            ++lineIndex;
            continue;
        }

        if (extensions.tables &&
            lineIndex + 1 < lines.size() &&
            line.text.find('|') != std::string_view::npos) {
            const auto alignments =
                parseTableDelimiter(lines[lineIndex + 1].text);
            if (alignments) {
                const auto headerCells = splitTableRow(line.text);
                if (headerCells.size() == alignments->size()) {
                    const std::size_t tableStart = lineIndex;
                    auto table = std::make_unique<Node>();
                    table->type = NodeType::Table;

                    auto head = std::make_unique<Node>();
                    head->type = NodeType::TableHead;
                    auto headRow = std::make_unique<Node>();
                    headRow->type = NodeType::TableRow;
                    for (std::size_t cellIndex = 0;
                         cellIndex < headerCells.size();
                         ++cellIndex) {
                        auto cell = std::make_unique<Node>();
                        cell->type = NodeType::TableCell;
                        cell->payload =
                            TableCellData{(*alignments)[cellIndex], true};
                        cell->literal = headerCells[cellIndex];
                        cell->range = makeRange(
                            line.startOffset,
                            line.number,
                            1,
                            line.endOffset,
                            line.number,
                            static_cast<std::uint32_t>(line.text.size() + 1));
                        cell->children = parseInline(
                            cell->literal,
                            cell->range.begin,
                            extensions);
                        headRow->children.push_back(std::move(cell));
                    }
                    head->children.push_back(std::move(headRow));
                    table->children.push_back(std::move(head));

                    lineIndex += 2;
                    auto body = std::make_unique<Node>();
                    body->type = NodeType::TableBody;
                    while (lineIndex < lines.size() &&
                           !isBlank(lines[lineIndex].text) &&
                           lines[lineIndex].text.find('|') !=
                               std::string_view::npos) {
                        const Line& rowLine = lines[lineIndex];
                        auto rowCells = splitTableRow(rowLine.text);
                        rowCells.resize(alignments->size());
                        auto row = std::make_unique<Node>();
                        row->type = NodeType::TableRow;
                        for (std::size_t cellIndex = 0;
                             cellIndex < alignments->size();
                             ++cellIndex) {
                            auto cell = std::make_unique<Node>();
                            cell->type = NodeType::TableCell;
                            cell->payload =
                                TableCellData{(*alignments)[cellIndex], false};
                            cell->literal = rowCells[cellIndex];
                            cell->range = makeRange(
                                rowLine.startOffset,
                                rowLine.number,
                                1,
                                rowLine.endOffset,
                                rowLine.number,
                                static_cast<std::uint32_t>(
                                    rowLine.text.size() + 1));
                            cell->children = parseInline(
                                cell->literal,
                                cell->range.begin,
                                extensions);
                            row->children.push_back(std::move(cell));
                        }
                        body->children.push_back(std::move(row));
                        ++lineIndex;
                    }
                    table->children.push_back(std::move(body));
                    const Line& tableEnd = lines[lineIndex - 1];
                    table->range = makeRange(
                        lines[tableStart].startOffset,
                        lines[tableStart].number,
                        1,
                        tableEnd.endOffset,
                        tableEnd.number,
                        static_cast<std::uint32_t>(
                            tableEnd.text.size() + 1));
                    document->children.push_back(std::move(table));
                    continue;
                }
            }
        }

        if (const auto fence = matchFence(line.text)) {
            std::size_t endIndex = lineIndex + 1;
            std::string literal;
            bool closed = false;
            while (endIndex < lines.size()) {
                if (isClosingFence(lines[endIndex].text, *fence)) {
                    closed = true;
                    break;
                }
                literal.append(lines[endIndex].text);
                if (lines[endIndex].nextOffset > lines[endIndex].endOffset) {
                    literal.push_back('\n');
                }
                ++endIndex;
            }

            const Line& endLine =
                closed ? lines[endIndex] : lines[std::max(lineIndex, endIndex - 1)];
            auto node = std::make_unique<Node>();
            node->type = NodeType::CodeBlock;
            node->range = makeRange(
                line.startOffset,
                line.number,
                1,
                closed ? endLine.endOffset : source.size(),
                endLine.number,
                static_cast<std::uint32_t>(endLine.text.size() + 1));
            auto info = trim(fence->info);
            const auto languageEnd = info.find_first_of(" \t");
            node->payload = CodeBlockData{
                std::string(info),
                std::string(info.substr(0, languageEnd)),
            };
            node->literal = std::move(literal);
            document->children.push_back(std::move(node));

            if (!closed) {
                diagnostics.push_back({
                    DiagnosticSeverity::Warning,
                    "MW1001",
                    "Fenced code block is not closed before end of document.",
                    document->children.back()->range,
                });
                lineIndex = lines.size();
            } else {
                lineIndex = endIndex + 1;
            }
            continue;
        }

        if (const auto heading = matchHeading(line.text)) {
            auto node = std::make_unique<Node>();
            node->type = NodeType::Heading;
            node->range = makeRange(
                line.startOffset,
                line.number,
                1,
                line.endOffset,
                line.number,
                static_cast<std::uint32_t>(line.text.size() + 1));
            node->payload = HeadingData{heading->level};
            node->literal = heading->content;
            node->children = parseInline(
                heading->content,
                {
                    line.startOffset + heading->contentOffset,
                    line.number,
                    static_cast<std::uint32_t>(heading->contentOffset + 1),
                },
                extensions);
            document->children.push_back(std::move(node));
            ++lineIndex;
            continue;
        }

        if (isThematicBreak(line.text)) {
            auto node = std::make_unique<Node>();
            node->type = NodeType::ThematicBreak;
            node->range = makeRange(
                line.startOffset,
                line.number,
                1,
                line.endOffset,
                line.number,
                static_cast<std::uint32_t>(line.text.size() + 1));
            document->children.push_back(std::move(node));
            ++lineIndex;
            continue;
        }

        const auto stripped = trimLeft(line.text);
        if (!stripped.empty() && stripped.front() == '>') {
            const std::size_t startIndex = lineIndex;
            std::string content;
            while (lineIndex < lines.size()) {
                auto quoteLine = trimLeft(lines[lineIndex].text);
                if (quoteLine.empty() || quoteLine.front() != '>') {
                    break;
                }
                quoteLine.remove_prefix(1);
                if (!quoteLine.empty() && quoteLine.front() == ' ') {
                    quoteLine.remove_prefix(1);
                }
                content.append(quoteLine);
                if (lineIndex + 1 < lines.size()) {
                    content.push_back('\n');
                }
                ++lineIndex;
            }
            const Line& last = lines[lineIndex - 1];
            auto node = std::make_unique<Node>();
            node->type = NodeType::BlockQuote;
            node->range = makeRange(
                lines[startIndex].startOffset,
                lines[startIndex].number,
                1,
                last.endOffset,
                last.number,
                static_cast<std::uint32_t>(last.text.size() + 1));
            node->literal = std::string(trimRight(content));
            auto innerLines = scanLines(node->literal);
            auto innerDoc = makeDocument(node->literal, innerLines, diagnostics, metadata, extensions, false);
            for (auto& child : innerDoc->children) {
                node->children.push_back(std::move(child));
            }
            document->children.push_back(std::move(node));
            continue;
        }

        if (const auto firstMarker = matchListMarker(line.text)) {
            auto list = std::make_unique<Node>();
            list->type = NodeType::List;
            list->payload =
                ListData{firstMarker->ordered, firstMarker->start, true};
            const std::size_t startIndex = lineIndex;

            while (lineIndex < lines.size()) {
                const auto marker = matchListMarker(lines[lineIndex].text);
                if (!marker || marker->ordered != firstMarker->ordered) {
                    break;
                }
                const Line& itemLine = lines[lineIndex];
                auto item = std::make_unique<Node>();
                item->type = NodeType::ListItem;
                item->range = makeRange(
                    itemLine.startOffset,
                    itemLine.number,
                    1,
                    itemLine.endOffset,
                    itemLine.number,
                    static_cast<std::uint32_t>(itemLine.text.size() + 1));
                auto itemContent =
                    itemLine.text.substr(marker->contentOffset);
                ListItemData itemData;
                if (extensions.taskLists &&
                    itemContent.size() >= 3 &&
                    itemContent[0] == '[' &&
                    itemContent[2] == ']' &&
                    (itemContent[1] == ' ' ||
                     itemContent[1] == 'x' ||
                     itemContent[1] == 'X')) {
                    itemData.task = true;
                    itemData.checked =
                        itemContent[1] == 'x' || itemContent[1] == 'X';
                    itemContent.remove_prefix(3);
                    if (!itemContent.empty() && itemContent.front() == ' ') {
                        itemContent.remove_prefix(1);
                    }
                    item->payload = itemData;
                }
                item->children.push_back(makeSyntheticParagraph(
                    std::string(itemContent),
                    makeRange(
                        itemLine.startOffset + marker->contentOffset,
                        itemLine.number,
                        static_cast<std::uint32_t>(marker->contentOffset + 1),
                        itemLine.endOffset,
                        itemLine.number,
                        static_cast<std::uint32_t>(itemLine.text.size() + 1)),
                    extensions));
                list->children.push_back(std::move(item));
                ++lineIndex;
            }

            const Line& last = lines[lineIndex - 1];
            list->range = makeRange(
                lines[startIndex].startOffset,
                lines[startIndex].number,
                1,
                last.endOffset,
                last.number,
                static_cast<std::uint32_t>(last.text.size() + 1));
            document->children.push_back(std::move(list));
            continue;
        }

        if (!stripped.empty() && stripped.front() == '<' &&
            (stripped.size() > 1) &&
            (std::isalpha(static_cast<unsigned char>(stripped[1])) != 0 ||
             stripped[1] == '/' ||
             stripped[1] == '!' ||
             stripped[1] == '?')) {

            // GFM autolinks like <https://...> must NOT be treated as HTML blocks;
            // let them fall through to paragraph parsing so the inline parser
            // can recognise them.
            const bool isAutoLink =
                stripped.starts_with("<http://") ||
                stripped.starts_with("<https://") ||
                stripped.starts_with("<mailto:");

            if (!isAutoLink) {
                const std::size_t startIndex = lineIndex;
                while (lineIndex < lines.size() && !isBlank(lines[lineIndex].text)) {
                    ++lineIndex;
                }
                const Line& last = lines[lineIndex - 1];
                auto node = std::make_unique<Node>();
                node->type = NodeType::HtmlBlock;
                node->range = makeRange(
                    lines[startIndex].startOffset,
                    lines[startIndex].number,
                    1,
                    last.endOffset,
                    last.number,
                    static_cast<std::uint32_t>(last.text.size() + 1));
                node->payload = HtmlData{
                    std::string(source.substr(
                        lines[startIndex].startOffset,
                        last.endOffset - lines[startIndex].startOffset)),
                };
                document->children.push_back(std::move(node));
                continue;
            }
        }


        const std::size_t startIndex = lineIndex;
        ++lineIndex;
        while (lineIndex < lines.size() &&
               !isBlank(lines[lineIndex].text) &&
               !startsBlock(lines[lineIndex].text)) {
            ++lineIndex;
        }
        const Line& last = lines[lineIndex - 1];
        document->children.push_back(makeParagraph(
            source,
            lines[startIndex],
            last,
            lines[startIndex].startOffset,
            last.endOffset,
            extensions));
    }

    return document;
}

} // namespace

Parser::Parser(EngineOptions options)
    : options_(std::move(options)) {}

ParseResult Parser::parse(
    std::string_view markdown,
    const ParseOptions& parseOptions) const {
    ParseResult result;

    if (markdown.size() > options_.maxInputBytes) {
        result.diagnostics.push_back({
            DiagnosticSeverity::Error,
            "MW0001",
            "Input exceeds the configured maximum size.",
            std::nullopt,
        });
        return result;
    }

    if (markdown.starts_with("\xEF\xBB\xBF")) {
        markdown.remove_prefix(3);
    }

    std::string normalized;
    if (!isValidUtf8(markdown)) {
        if (options_.invalidUtf8Policy == InvalidUtf8Policy::Reject) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Error,
                "MW0002",
                "Input is not valid UTF-8.",
                std::nullopt,
            });
            return result;
        }
        normalized = replaceInvalidUtf8(markdown);
        markdown = normalized;
        result.diagnostics.push_back({
            DiagnosticSeverity::Warning,
            "MW0003",
            "Invalid UTF-8 sequences were replaced.",
            std::nullopt,
        });
    }

    const auto lines = scanLines(markdown);
    result.document = makeDocument(
        markdown,
        lines,
        result.diagnostics,
        result.metadata,
        parseOptions.extensions);
    result.ok = true;
    return result;
}

} // namespace mwrender
