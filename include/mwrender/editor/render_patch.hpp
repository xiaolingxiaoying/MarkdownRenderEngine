#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include <mwrender/ast.hpp>
#include <mwrender/engine.hpp>
#include <mwrender/editor/editor_projection.hpp>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/edit_command.hpp>

namespace mwrender::editor {

struct NodeHtmlPatch {
    std::string nodeId;
    std::string html;
    ProjectionMode mode = ProjectionMode::Atomic;
    SourceRange sourceRange;
    SourceRange contentRange;
    std::string parentId;
    std::size_t insertIndex = 0;
};

struct RenderPatch {
    std::size_t revision = 0;
    bool fullReload = false;
    std::vector<std::string> removedNodeIds;
    std::vector<NodeHtmlPatch> changedNodes;
    std::vector<NodeHtmlPatch> insertedNodes;
    std::optional<Selection> selection;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] std::string toJson() const;
};

class RenderPatchGenerator {
public:
    explicit RenderPatchGenerator(const EditorProjection& projection);

    RenderPatch generatePatch(
        const Node& document,
        const UpdateResult& parseResult,
        const std::optional<Selection>& newSelection = std::nullopt
    ) const;

private:
    const EditorProjection& projection_;
};

} // namespace mwrender::editor
