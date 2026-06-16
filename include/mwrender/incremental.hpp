#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <mwrender/ast.hpp>
#include <mwrender/diagnostics.hpp>
#include <mwrender/options.hpp>
#include <mwrender/source.hpp>

namespace mwrender {

struct TextChange {
    std::size_t from = 0;
    std::size_t to = 0;
    std::string insertedText;
};

struct IncrementalParseResult {
    bool ok = false;
    std::unique_ptr<Node> document;
    std::vector<Diagnostic> diagnostics;
    std::vector<std::string> changedNodeIds;
    std::vector<std::string> removedNodeIds;
    std::vector<std::string> insertedNodeIds;
};

struct NodeRenderRequest {
    const Node* document = nullptr;
    std::string nodeId;
    RenderOptions options;
};

} // namespace mwrender
