#include <mwrender/editor/node_id_registry.hpp>
#include <algorithm>

namespace mwrender::editor {

void NodeIdRegistry::inheritIds(const Node& oldRoot, Node& newRoot) {
    newRoot.id = oldRoot.id.empty() ? allocate(newRoot.type) : oldRoot.id;
    matchSubtrees(oldRoot, newRoot);
}

std::string NodeIdRegistry::allocate(NodeType) {
    return "n" + std::to_string(counter_++);
}

void NodeIdRegistry::matchSubtrees(const Node& oldNode, Node& newNode) {
    // Simple greedy match
    std::vector<bool> oldMatched(oldNode.children.size(), false);
    std::vector<bool> newMatched(newNode.children.size(), false);

    // Pass 1: exact match by type and contentHash
    for (std::size_t i = 0; i < newNode.children.size(); ++i) {
        if (!newNode.children[i]) continue;
        for (std::size_t j = 0; j < oldNode.children.size(); ++j) {
            if (!oldNode.children[j] || oldMatched[j]) continue;
            if (newNode.children[i]->type == oldNode.children[j]->type &&
                newNode.children[i]->contentHash == oldNode.children[j]->contentHash) {
                newNode.children[i]->id = oldNode.children[j]->id;
                oldMatched[j] = true;
                newMatched[i] = true;
                matchSubtrees(*oldNode.children[j], *newNode.children[i]);
                break;
            }
        }
    }

    // Pass 2: match by type (for modified nodes)
    for (std::size_t i = 0; i < newNode.children.size(); ++i) {
        if (!newNode.children[i] || newMatched[i]) continue;
        for (std::size_t j = 0; j < oldNode.children.size(); ++j) {
            if (!oldNode.children[j] || oldMatched[j]) continue;
            if (newNode.children[i]->type == oldNode.children[j]->type) {
                newNode.children[i]->id = oldNode.children[j]->id;
                oldMatched[j] = true;
                newMatched[i] = true;
                matchSubtrees(*oldNode.children[j], *newNode.children[i]);
                break;
            }
        }
    }

    // Pass 3: allocate new IDs for unmatched new nodes
    for (std::size_t i = 0; i < newNode.children.size(); ++i) {
        if (!newNode.children[i] || newMatched[i]) continue;
        newNode.children[i]->id = allocate(newNode.children[i]->type);
        matchSubtrees(*newNode.children[i], *newNode.children[i]); // will recursively allocate for its children
    }
}

} // namespace mwrender::editor
