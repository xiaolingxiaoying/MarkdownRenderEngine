#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <mwrender/source.hpp>

namespace mwrender {

enum class NodeType : std::uint8_t {
    Document,
    FrontMatter,
    Heading,
    Paragraph,
    BlockQuote,
    List,
    ListItem,
    CodeBlock,
    ThematicBreak,
    HtmlBlock,
    Table,
    TableHead,
    TableBody,
    TableRow,
    TableCell,
    Text,
    Emphasis,
    Strong,
    Strikethrough,
    InlineCode,
    Link,
    Image,
    HtmlInline,
    AutoLink,
    MathBlock,
    MathInline,
    Toc,
    FootnoteRef,
    FootnoteDef,
    SoftBreak,
    HardBreak
};

struct HeadingData {
    std::uint8_t level = 1;
};

struct ListData {
    bool ordered = false;
    std::uint64_t start = 1;
    bool tight = true;
};

struct ListItemData {
    bool task = false;
    bool checked = false;
};

struct CodeBlockData {
    std::string info;
    std::string language;
};

struct LinkData {
    std::string destination;
    std::string title;
};

struct ImageData {
    std::string destination;
    std::string title;
};

struct HtmlData {
    std::string raw;
};

struct TableCellData {
    std::string alignment;
    bool header = false;
};

struct FootnoteData {
    std::string id;
};

using NodePayload = std::variant<
    std::monostate,
    HeadingData,
    ListData,
    ListItemData,
    CodeBlockData,
    LinkData,
    ImageData,
    HtmlData,
    TableCellData,
    FootnoteData>;

struct Node {
    NodeType type = NodeType::Document;
    SourceRange range;
    NodePayload payload;
    std::string literal;
    std::vector<std::unique_ptr<Node>> children;
};

} // namespace mwrender
