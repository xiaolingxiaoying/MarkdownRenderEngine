#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include <mwrender/ast.hpp>

namespace mwrender::editor {

class NodeIdRegistry {
public:
    NodeIdRegistry() = default;

    void inheritIds(const Node& oldRoot, Node& newRoot);
    std::string allocate(NodeType type);

private:
    void collectUsedIds(const Node& node);
    void matchSubtrees(const Node& oldNode, Node& newNode);
    
    std::size_t counter_ = 1;
    std::unordered_set<std::string> usedIds_;
};

} // namespace mwrender::editor
