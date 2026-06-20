#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/ast.hpp>
#include <mwrender/incremental.hpp>

namespace mwrender::editor {

struct BlockEntry {
    std::string nodeId;
    NodeType type;
    SourceRange range;
    SourceRange contentRange;
    std::size_t depth = 0;
};

class BlockIndex {
public:
    BlockIndex() = default;

    void rebuild(const Node& document);

    [[nodiscard]] const BlockEntry* blockAtOffset(std::size_t sourceOffset) const;
    [[nodiscard]] const BlockEntry* blockById(std::string_view nodeId) const;

    [[nodiscard]] SourceRange affectedRangeForChange(const TextChange& change) const;

    [[nodiscard]] const std::vector<BlockEntry>& blocks() const;

private:
    void traverse(const Node& node, std::size_t currentDepth);
    bool isBlockNode(NodeType type) const;
    [[nodiscard]] SourceRange findParentRange(const BlockEntry* child, NodeType parentType) const;
    [[nodiscard]] NodeType commonContainerType(const BlockEntry* a, const BlockEntry* b) const;

    std::vector<BlockEntry> blocks_;
};

} // namespace mwrender::editor
