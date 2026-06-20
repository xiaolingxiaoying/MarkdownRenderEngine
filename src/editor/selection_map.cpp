#include <mwrender/editor/selection_map.hpp>
#include <mwrender/query.hpp>

namespace mwrender::editor {

SelectionMap::SelectionMap(const DocumentSession& session)
    : session_(session) {}

SourcePositionEx SelectionMap::visualToSource(const VisualPosition& visual) const {
    SourcePositionEx source;
    source.contextNodeId = visual.nodeId;
    source.affinity = Affinity::After;
    
    const Node* node = session_.findNodeById(visual.nodeId);
    if (!node) {
        return source;
    }

    // For a simple map, textOffset inside the DOM node usually corresponds to the length of the string without markers.
    // If we assume EditorProjection maps cleanly, we can approximate source offset by adding textOffset to node's contentRange start.
    // In a fully robust implementation, we would subtract marker lengths.
    // For now, we will provide a basic offset calculation.
    
    // In many nodes, literal is populated or contentRange is tight.
    // If visual textOffset is 0, it's at the start of the content range.
    if (node->type == NodeType::Text || node->type == NodeType::CodeBlock) {
        source.offset = node->range.begin.offset + visual.textOffset;
    } else {
        // For container nodes (like Paragraph), if it has text nodes, the UI usually targets the text nodes.
        // If the UI targets the Paragraph node with an offset, it might be the Nth child.
        // As a fallback, we use contentRange or range start.
        source.offset = node->contentRange.begin.offset + visual.textOffset;
    }
    
    // Cap to end
    if (source.offset > node->contentRange.end.offset) {
        source.offset = node->contentRange.end.offset;
    }
    
    return source;
}

VisualPosition SelectionMap::sourceToVisual(const SourcePositionEx& source) const {
    VisualPosition visual;
    visual.nodeId = source.contextNodeId;
    
    const Node* node = session_.findNodeById(source.contextNodeId);
    if (!node) {
        return visual;
    }
    
    // Inverse of visualToSource
    if (node->type == NodeType::Text || node->type == NodeType::CodeBlock) {
        if (source.offset >= node->range.begin.offset) {
            visual.textOffset = source.offset - node->range.begin.offset;
        }
    } else {
        if (source.offset >= node->contentRange.begin.offset) {
            visual.textOffset = source.offset - node->contentRange.begin.offset;
        }
    }
    
    return visual;
}

} // namespace mwrender::editor
