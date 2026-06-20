#include <mwrender/editor/render_patch.hpp>
#include <mwrender/query.hpp>

namespace mwrender::editor {

RenderPatchGenerator::RenderPatchGenerator(const EditorProjection& projection)
    : projection_(projection) {}

namespace {
// Helper to find parent and index
bool findParentAndIndex(const Node& current, const std::string& targetId, std::string& parentId, std::size_t& index) {
    for (std::size_t i = 0; i < current.children.size(); ++i) {
        if (current.children[i] && current.children[i]->id == targetId) {
            parentId = current.id;
            index = i;
            return true;
        }
    }
    for (const auto& child : current.children) {
        if (child && findParentAndIndex(*child, targetId, parentId, index)) {
            return true;
        }
    }
    return false;
}
}

RenderPatch RenderPatchGenerator::generatePatch(
    const Node& document,
    const UpdateResult& parseResult
) const {
    RenderPatch patch;
    patch.removedNodeIds = parseResult.removedNodeIds;

    for (const auto& id : parseResult.changedNodeIds) {
        RenderPatch::HtmlSnippet snippet;
        snippet.id = id;
        snippet.html = projection_.projectNode(document, id);
        
        // We only care about parent/index if it's inserted, but could populate for changed too
        findParentAndIndex(document, id, snippet.parentId, snippet.insertIndex);
        
        patch.changedNodes.push_back(std::move(snippet));
    }

    for (const auto& id : parseResult.insertedNodeIds) {
        RenderPatch::HtmlSnippet snippet;
        snippet.id = id;
        snippet.html = projection_.projectNode(document, id);
        
        findParentAndIndex(document, id, snippet.parentId, snippet.insertIndex);
        
        patch.insertedNodes.push_back(std::move(snippet));
    }

    return patch;
}

} // namespace mwrender::editor
