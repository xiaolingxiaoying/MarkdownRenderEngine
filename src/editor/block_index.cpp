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

    if (!startBlock) {
        if (!blocks_.empty()) return blocks_[0].range;
        return SourceRange{};
    }

    if (startBlock && endBlock && startBlock->nodeId == endBlock->nodeId) {
        NodeType t = startBlock->type;
        // Simple blocks: return precise range
        if (t == NodeType::Paragraph || t == NodeType::Heading ||
            t == NodeType::CodeBlock || t == NodeType::MathBlock ||
            t == NodeType::ThematicBreak || t == NodeType::HtmlBlock) {
            return startBlock->range;
        }
        // ListItem: find parent List for safe re-parse
        if (t == NodeType::ListItem) {
            return findParentRange(startBlock, NodeType::List);
        }
        // BlockQuote: return BlockQuote range
        if (t == NodeType::BlockQuote) {
            return startBlock->range;
        }
        // List: return List range
        if (t == NodeType::List) {
            return startBlock->range;
        }
    }

    // Cross-block change: merge start and end ranges if possible
    if (startBlock && endBlock) {
        // Find a common ancestor container
        NodeType container = commonContainerType(startBlock, endBlock);
        if (container == NodeType::List || container == NodeType::BlockQuote) {
            // Find the container block that covers both start and end
            for (const auto& b : blocks_) {
                if (b.type == container &&
                    b.range.begin.offset <= startBlock->range.begin.offset &&
                    b.range.end.offset >= endBlock->range.end.offset) {
                    return b.range;
                }
            }
        }
        // Start block to end block combined range
        SourceRange merged;
        merged.begin = startBlock->range.begin;
        merged.end = endBlock->range.end;
        return merged;
    }

    // Fallback: entire document range
    if (!blocks_.empty()) {
        return blocks_[0].range;
    }

    return SourceRange{};
}

SourceRange BlockIndex::findParentRange(const BlockEntry* child, NodeType parentType) const {
    if (!child || blocks_.empty()) {
        return SourceRange{};
    }
    // Scan backwards from child to find the nearest ancestor of the given type
    // The child is at some depth; we look for a block at a lower depth.
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) {
        if (it->nodeId == child->nodeId) continue;
        if (it->type == parentType && it->depth < child->depth &&
            it->range.begin.offset <= child->range.begin.offset &&
            it->range.end.offset >= child->range.end.offset) {
            return it->range;
        }
    }
    // Fallback to document range
    return blocks_[0].range;
}

NodeType BlockIndex::commonContainerType(const BlockEntry* a, const BlockEntry* b) const {
    if (!a || !b) return NodeType::Document;
    // If both are at the same depth and same type, return that type
    if (a->depth == b->depth && a->type == b->type) {
        return a->type;
    }
    // Check if one is a container of the other
    if (a->range.begin.offset <= b->range.begin.offset &&
        a->range.end.offset >= b->range.end.offset) {
        if (a->type == NodeType::List || a->type == NodeType::BlockQuote) return a->type;
    }
    if (b->range.begin.offset <= a->range.begin.offset &&
        b->range.end.offset >= a->range.end.offset) {
        if (b->type == NodeType::List || b->type == NodeType::BlockQuote) return b->type;
    }
    return NodeType::Document;
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
    bool isBlock = isBlockNode(node.type);
    if (isBlock) {
        blocks_.push_back(BlockEntry{
            node.id,
            node.type,
            node.range,
            node.contentRange,
            currentDepth
        });
    }

    if (isBlock) {
        for (const auto& child : node.children) {
            if (child) {
                traverse(*child, currentDepth + 1);
            }
        }
    }
}

} // namespace mwrender::editor
