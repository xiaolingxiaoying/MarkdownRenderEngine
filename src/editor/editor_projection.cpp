#include <mwrender/editor/editor_projection.hpp>
#include <mwrender/options.hpp>

namespace mwrender::editor {

EditorProjection::EditorProjection(const Engine& engine)
    : engine_(engine) {}

std::string EditorProjection::projectNode(const Node& document, const std::string& nodeId) const {
    RenderOptions options;
    options.renderMode = RenderMode::EditorView;
    options.sourceMap = SourceMapMode::Full;
    options.outputMode = OutputMode::Fragment;

    NodeRenderRequest request;
    request.document = &document;
    request.nodeId = nodeId;
    request.options = options;

    auto result = engine_.renderNode(request);
    if (!result.ok) {
        return "";
    }
    return result.fragment;
}

std::string EditorProjection::projectDocument(const Node& document) const {
    RenderOptions options;
    options.renderMode = RenderMode::EditorView;
    options.sourceMap = SourceMapMode::Full;
    options.outputMode = OutputMode::Fragment;

    // We need to render the whole document. Since Engine::render takes a markdown string,
    // and Engine::renderNode takes a node id, we can just use renderNode on the document ID.
    // Assuming the Document node has an ID. If it's empty, we might need a workaround.
    NodeRenderRequest request;
    request.document = &document;
    request.nodeId = document.id;
    request.options = options;

    // If document ID is empty, we can just wrap it in a dummy request or use a known renderer
    // But since Stage 3, Document has an ID from NodeIdRegistry!
    if (document.id.empty()) {
        // Fallback if no ID
        // Currently Engine has no public method to render an AST node directly without an ID string or source markdown.
        // Actually, Engine::renderNode wraps the target in a Document node anyway.
    }

    auto result = engine_.renderNode(request);
    if (!result.ok) {
        return "";
    }
    return result.fragment;
}

} // namespace mwrender::editor
