#include <mwrender/serializer.hpp>

#include <sstream>
#include <stdexcept>

namespace mwrender {
namespace {

void serializeBlock(const Node& node, std::ostringstream& out,
                     const MarkdownStyle& style, int indent);
void serializeChildren(const Node& node, std::ostringstream& out,
                        const MarkdownStyle& style, int indent);

void serializeChildren(
    const Node& node,
    std::ostringstream& out,
    const MarkdownStyle& style,
    int indent);

void serializeInlineContent(
    const Node& node,
    std::ostringstream& out,
    const MarkdownStyle& style) {
    for (const auto& child : node.children) {
        serializeBlock(*child, out, style, 0);
    }
}

void serializeText(const Node& node, std::ostringstream& out) {
    if (!node.literal.empty()) {
        out << node.literal;
    }
}

void serializeEmphasis(const Node& node, std::ostringstream& out,
                        const MarkdownStyle& style) {
    const auto* data = std::get_if<EmphasisData>(&node.payload);
    std::string marker = style.emphasisMarker;
    if (style.preserveOriginalMarkers && data && !data->marker.empty()) {
        marker = data->marker;
    }
    out << marker;
    serializeInlineContent(node, out, style);
    out << marker;
}

void serializeStrong(const Node& node, std::ostringstream& out,
                      const MarkdownStyle& style) {
    const auto* data = std::get_if<StrongData>(&node.payload);
    std::string marker = style.strongMarker;
    if (style.preserveOriginalMarkers && data && !data->marker.empty()) {
        marker = data->marker;
    }
    out << marker;
    serializeInlineContent(node, out, style);
    out << marker;
}

void serializeStrikethrough(const Node& node, std::ostringstream& out,
                             const MarkdownStyle& style) {
    out << "~~";
    serializeInlineContent(node, out, style);
    out << "~~";
}

void serializeInlineCode(const Node& node, std::ostringstream& out) {
    out << '`' << node.literal << '`';
}

void serializeLink(const Node& node, std::ostringstream& out,
                    const MarkdownStyle& style) {
    const auto* data = std::get_if<LinkData>(&node.payload);
    out << '[';
    serializeInlineContent(node, out, style);
    out << "](";
    if (data) {
        out << data->destination;
        if (!data->title.empty()) {
            out << " \"" << data->title << '"';
        }
    }
    out << ')';
}

void serializeImage(const Node& node, std::ostringstream& out) {
    const auto* data = std::get_if<ImageData>(&node.payload);
    out << "![";
    out << node.literal;
    out << "](";
    if (data) {
        out << data->destination;
        if (!data->title.empty()) {
            out << " \"" << data->title << '"';
        }
    }
    out << ')';
}

void serializeAutoLink(const Node& node, std::ostringstream& out) {
    out << '<' << node.literal << '>';
}

void serializeHtmlInline(const Node& node, std::ostringstream& out) {
    const auto* data = std::get_if<HtmlData>(&node.payload);
    if (data) {
        out << data->raw;
    }
}

void serializeMath(const Node& node, std::ostringstream& out) {
    if (node.type == NodeType::MathBlock) {
        out << "$$\n" << node.literal << "\n$$";
    } else {
        out << '$' << node.literal << '$';
    }
}

std::string repeatString(const std::string& s, int count) {
    std::string result;
    for (int i = 0; i < count; ++i) result += s;
    return result;
}

void serializeBlock(const Node& node, std::ostringstream& out,
                     const MarkdownStyle& style, int indent) {
    switch (node.type) {
    case NodeType::Document:
        serializeChildren(node, out, style, indent);
        break;

    case NodeType::Paragraph:
        serializeInlineContent(node, out, style);
        out << style.lineEnding << style.lineEnding;
        break;

    case NodeType::Heading: {
        const auto* data = std::get_if<HeadingData>(&node.payload);
        int level = data ? data->level : 1;
        out << repeatString("#", level) << ' ';
        serializeInlineContent(node, out, style);
        out << style.lineEnding << style.lineEnding;
        break;
    }

    case NodeType::BlockQuote:
        out << "> ";
        serializeChildren(node, out, style, indent);
        out << style.lineEnding;
        break;

    case NodeType::List: {
        const auto* data = std::get_if<ListData>(&node.payload);
        int itemIndex = 0;
        for (const auto& item : node.children) {
            std::string marker;
            if (data && data->ordered) {
                uint64_t startNum = data->start;
                if (!style.preserveListStartNumber) startNum = 1;
                marker = std::to_string(startNum + itemIndex) + ".";
                if (style.preserveOriginalMarkers && !data->marker.empty()) {
                    marker = std::to_string(startNum + itemIndex) + data->marker.back();
                }
            } else {
                marker = style.bulletMarker;
                if (style.preserveOriginalMarkers && data && !data->marker.empty()) {
                    marker = data->marker;
                }
            }
            out << marker << " ";

            const auto* itemData = std::get_if<ListItemData>(&item->payload);
            if (itemData && itemData->task) {
                std::string taskMarker = itemData->checked ? "[x]" : "[ ]";
                if (style.preserveTaskMarkerCase && !itemData->taskMarker.empty()) {
                    taskMarker = itemData->taskMarker;
                }
                out << taskMarker << " ";
            }

            for (const auto& child : item->children) {
                if (child->type == NodeType::Paragraph) {
                    for (const auto& inlineChild : child->children) {
                        serializeBlock(*inlineChild, out, style, indent + 1);
                    }
                } else {
                    serializeBlock(*child, out, style, indent + 1);
                }
            }
            out << style.lineEnding;
            itemIndex++;
        }
        out << style.lineEnding;
        break;
    }

    case NodeType::ListItem:
        break;

    case NodeType::CodeBlock: {
        const auto* data = std::get_if<CodeBlockData>(&node.payload);
        std::string marker = style.codeFenceMarker;
        if (style.preserveOriginalMarkers && data && !data->fenceMarker.empty()) {
            marker = data->fenceMarker;
        }
        out << marker;
        if (data && !data->language.empty()) {
            out << data->language;
        }
        out << style.lineEnding;
        out << node.literal;
        if (!node.literal.empty() && node.literal.back() != '\n') {
            out << style.lineEnding;
        }
        out << marker << style.lineEnding << style.lineEnding;
        break;
    }

    case NodeType::HtmlBlock: {
        const auto* data = std::get_if<HtmlData>(&node.payload);
        if (data) {
            out << data->raw << style.lineEnding << style.lineEnding;
        }
        break;
    }

    case NodeType::ThematicBreak:
        out << "---" << style.lineEnding << style.lineEnding;
        break;

    case NodeType::Table: {
        const Node* head = nullptr;
        const Node* body = nullptr;
        for (const auto& child : node.children) {
            if (child->type == NodeType::TableHead) head = child.get();
            if (child->type == NodeType::TableBody) body = child.get();
        }
        if (head && !head->children.empty()) {
            const auto& headerRow = head->children[0];
            out << '|';
            for (const auto& cell : headerRow->children) {
                out << ' ' << cell->literal << " |";
            }
            out << style.lineEnding << '|';
            for (const auto& cell : headerRow->children) {
                const auto* cellData = std::get_if<TableCellData>(&cell->payload);
                std::string delim = "---";
                if (cellData) {
                    if (cellData->alignment == "center") delim = ":---:";
                    else if (cellData->alignment == "left") delim = ":---";
                    else if (cellData->alignment == "right") delim = "---:";
                }
                out << delim << '|';
            }
            out << style.lineEnding;
            if (body) {
                for (const auto& row : body->children) {
                    out << '|';
                    for (const auto& cell : row->children) {
                        out << ' ' << cell->literal << " |";
                    }
                    out << style.lineEnding;
                }
            }
        }
        out << style.lineEnding;
        break;
    }

    case NodeType::TableHead:
    case NodeType::TableBody:
    case NodeType::TableRow:
        serializeChildren(node, out, style, indent);
        break;

    case NodeType::TableCell:
        out << node.literal;
        break;

    case NodeType::Toc:
        out << "[TOC]" << style.lineEnding << style.lineEnding;
        break;

    case NodeType::FootnoteDef: {
        const auto* data = std::get_if<FootnoteData>(&node.payload);
        if (data) {
            out << "[^" << data->id << "]: ";
            serializeInlineContent(node, out, style);
            out << style.lineEnding << style.lineEnding;
        }
        break;
    }

    case NodeType::FootnoteRef: {
        const auto* data = std::get_if<FootnoteData>(&node.payload);
        if (data) {
            out << "[^" << data->id << ']';
        }
        break;
    }

    case NodeType::FrontMatter:
        out << node.literal << style.lineEnding;
        break;

    case NodeType::Text:
        serializeText(node, out);
        break;
    case NodeType::Emphasis:
        serializeEmphasis(node, out, style);
        break;
    case NodeType::Strong:
        serializeStrong(node, out, style);
        break;
    case NodeType::Strikethrough:
        serializeStrikethrough(node, out, style);
        break;
    case NodeType::InlineCode:
        serializeInlineCode(node, out);
        break;
    case NodeType::Link:
        serializeLink(node, out, style);
        break;
    case NodeType::Image:
        serializeImage(node, out);
        break;
    case NodeType::HtmlInline:
        serializeHtmlInline(node, out);
        break;
    case NodeType::AutoLink:
        serializeAutoLink(node, out);
        break;
    case NodeType::MathBlock:
    case NodeType::MathInline:
        serializeMath(node, out);
        break;
    case NodeType::SoftBreak:
        out << '\n';
        break;
    case NodeType::HardBreak:
        out << "  \n";
        break;
    default:
        break;
    }
}

void serializeChildren(
    const Node& node,
    std::ostringstream& out,
    const MarkdownStyle& style,
    int indent) {
    for (const auto& child : node.children) {
        serializeBlock(*child, out, style, indent);
    }
}

} // namespace

std::string serializeMarkdown(
    const Node& document,
    const MarkdownStyle& style) {
    std::ostringstream out;
    serializeBlock(document, out, style, 0);
    return out.str();
}

std::string serializeNode(
    const Node& node,
    const MarkdownStyle& style,
    int indent) {
    std::ostringstream out;
    serializeBlock(node, out, style, indent);
    return out.str();
}

} // namespace mwrender
