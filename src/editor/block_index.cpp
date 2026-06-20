#include <mwrender/editor/block_index.hpp>
#include <algorithm>

namespace mwrender::editor {

void BlockIndex::rebuild(const Node& document) {
    blocks_.clear();
    traverse(document, 0);
}

const BlockEntry* BlockIndex::blockAtOffset(std::size_t sourceOffset) const {
    const BlockEntry* bestMatch = nullptr;

    for (const auto& block : blocks_) {
        if (sourceOffset >= block.range.begin.offset && sourceOffset <= block.range.end.offset) {
            // Find the deepest matching block
            if (!bestMatch || block.depth > bestMatch->depth) {
                bestMatch = &block;
            }
        }
    }

    return bestMatch;
}

const BlockEntry* BlockIndex::blockById(std::string_view nodeId) const {
    for (const auto& block : blocks_) {
        if (block.nodeId == nodeId) {
            return &block;
        }
    }
    return nullptr;
}

SourceRange BlockIndex::affectedRangeForChange(const TextChange& change) const {
    const BlockEntry* startBlock = blockAtOffset(change.from);
    const BlockEntry* endBlock = blockAtOffset(change.to);

    if (startBlock && endBlock && startBlock->nodeId == endBlock->nodeId) {
        // Simple case: change is completely within one block
        NodeType t = startBlock->type;
        if (t == NodeType::Paragraph || t == NodeType::Heading || t == NodeType::CodeBlock || t == NodeType::MathBlock || t == NodeType::ThematicBreak) {
            return startBlock->range;
        }
    }

    // Fallback: entire document range. 
    // If blocks_ is not empty, block 0 is usually the Document node itself.
    if (!blocks_.empty()) {
        return blocks_[0].range;
    }
    
    return SourceRange{};
}

const std::vector<BlockEntry>& BlockIndex::blocks() const {
    return blocks_;
}

bool BlockIndex::isBlockNode(NodeType type) const {
    switch (type) {
        case NodeType::Document:
        case NodeType::Heading:
        case NodeType::Paragraph:
        case NodeType::BlockQuote:
        case NodeType::List:
        case NodeType::ListItem:
        case NodeType::CodeBlock:
        case NodeType::ThematicBreak:
        case NodeType::HtmlBlock:
        case NodeType::Table:
        case NodeType::MathBlock:
        case NodeType::FootnoteDef:
            return true;
        default:
            return false;
    }
}

void BlockIndex::traverse(const Node& node, std::size_t currentDepth) {
    if (isBlockNode(node.type)) {
        blocks_.push_back(BlockEntry{
            node.id,
            node.type,
            node.range,
            node.contentRange,
            currentDepth
        });
    }

    for (const auto& child : node.children) {
        if (child) {
            traverse(*child, currentDepth + 1);
        }
    }
}

} // namespace mwrender::editor
