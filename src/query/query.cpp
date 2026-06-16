#include <mwrender/query.hpp>

namespace mwrender {
namespace {

bool rangeContainsOffset(const SourceRange& range, std::size_t offset) {
    return offset >= range.begin.offset && offset <= range.end.offset;
}

bool rangesOverlap(const SourceRange& a, const SourceRange& b) {
    return a.begin.offset < b.end.offset && b.begin.offset < a.end.offset;
}

Node* findNodeByOffsetImpl(Node& node, std::size_t offset) {
    if (!rangeContainsOffset(node.range, offset)) {
        return nullptr;
    }
    for (auto& child : node.children) {
        if (auto* found = findNodeByOffsetImpl(*child, offset)) {
            return found;
        }
    }
    return &node;
}

const Node* findNodeByOffsetImpl(const Node& node, std::size_t offset) {
    if (!rangeContainsOffset(node.range, offset)) {
        return nullptr;
    }
    for (const auto& child : node.children) {
        if (auto* found = findNodeByOffsetImpl(*child, offset)) {
            return found;
        }
    }
    return &node;
}

Node* findNodeByIdImpl(Node& node, std::string_view id) {
    if (node.id == id) {
        return &node;
    }
    for (auto& child : node.children) {
        if (auto* found = findNodeByIdImpl(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

const Node* findNodeByIdImpl(const Node& node, std::string_view id) {
    if (node.id == id) {
        return &node;
    }
    for (const auto& child : node.children) {
        if (auto* found = findNodeByIdImpl(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

void collectNodesByRangeImpl(
    Node& node,
    const SourceRange& range,
    std::vector<Node*>& result) {
    if (!rangesOverlap(node.range, range)) {
        return;
    }
    result.push_back(&node);
    for (auto& child : node.children) {
        collectNodesByRangeImpl(*child, range, result);
    }
}

void collectNodesByRangeImpl(
    const Node& node,
    const SourceRange& range,
    std::vector<const Node*>& result) {
    if (!rangesOverlap(node.range, range)) {
        return;
    }
    result.push_back(&node);
    for (const auto& child : node.children) {
        collectNodesByRangeImpl(*child, range, result);
    }
}

} // namespace

Node* findNodeBySourceOffset(Node& root, std::size_t offset) {
    return findNodeByOffsetImpl(root, offset);
}

const Node* findNodeBySourceOffset(const Node& root, std::size_t offset) {
    return findNodeByOffsetImpl(root, offset);
}

Node* findNodeById(Node& root, std::string_view id) {
    return findNodeByIdImpl(root, id);
}

const Node* findNodeById(const Node& root, std::string_view id) {
    return findNodeByIdImpl(root, id);
}

std::vector<Node*> findNodesByRange(Node& root, const SourceRange& range) {
    std::vector<Node*> result;
    collectNodesByRangeImpl(root, range, result);
    return result;
}

std::vector<const Node*> findNodesByRange(
    const Node& root,
    const SourceRange& range) {
    std::vector<const Node*> result;
    collectNodesByRangeImpl(root, range, result);
    return result;
}

} // namespace mwrender
