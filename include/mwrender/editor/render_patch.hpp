#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include <mwrender/ast.hpp>
#include <mwrender/engine.hpp>
#include <mwrender/editor/editor_projection.hpp>

#include <mwrender/editor/document_session.hpp>

namespace mwrender::editor {

struct RenderPatch {
    std::vector<std::string> removedNodeIds;
    
    struct HtmlSnippet {
        std::string id;
        std::string html;
        std::string parentId;
        std::size_t insertIndex = 0;
    };
    
    std::vector<HtmlSnippet> insertedNodes;
    std::vector<HtmlSnippet> changedNodes;
};

class RenderPatchGenerator {
public:
    explicit RenderPatchGenerator(const EditorProjection& projection);

    RenderPatch generatePatch(
        const Node& document,
        const UpdateResult& parseResult
    ) const;

private:
    const EditorProjection& projection_;
};

} // namespace mwrender::editor
