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
    std::string marker;
};

struct ListItemData {
    bool task = false;
    bool checked = false;
    std::string taskMarker;
};

struct CodeBlockData {
    std::string info;
    std::string language;
    std::string fenceMarker;
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

struct EmphasisData {
    char delimiter = '*';
    std::string marker;
};

struct StrongData {
    char delimiter = '*';
    std::string marker;
};

struct StrikethroughData {
    char delimiter = '~';
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
    FootnoteData,
    EmphasisData,
    StrongData,
    StrikethroughData>;

struct Node {
    std::string id;
    std::string contentHash;
    NodeType type = NodeType::Document;
    SourceRange range;
    SourceRange contentRange;
    std::vector<SourceRange> markerRanges;
    NodePayload payload;
    std::string literal;
    std::vector<std::unique_ptr<Node>> children;
};

} // namespace mwrender
