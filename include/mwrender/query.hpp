#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/ast.hpp>
#include <mwrender/source.hpp>

namespace mwrender {

[[nodiscard]] Node* findNodeBySourceOffset(
    Node& root,
    std::size_t offset);

[[nodiscard]] const Node* findNodeBySourceOffset(
    const Node& root,
    std::size_t offset);

[[nodiscard]] Node* findNodeById(
    Node& root,
    std::string_view id);

[[nodiscard]] const Node* findNodeById(
    const Node& root,
    std::string_view id);

[[nodiscard]] std::vector<Node*> findNodesByRange(
    Node& root,
    const SourceRange& range);

[[nodiscard]] std::vector<const Node*> findNodesByRange(
    const Node& root,
    const SourceRange& range);

} // namespace mwrender
