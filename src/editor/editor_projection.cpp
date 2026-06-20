#include <mwrender/editor/editor_projection.hpp>
#include <mwrender/options.hpp>
#include <mwrender/query.hpp>

namespace mwrender::editor {

ProjectionMode EditorProjection::classifyNode(const Node& node) {
    switch (node.type) {
        case NodeType::Paragraph:
        case NodeType::Heading:
        case NodeType::Text:
        case NodeType::SoftBreak:
        case NodeType::HardBreak:
            return ProjectionMode::Editable;

        case NodeType::Strong:
        case NodeType::Emphasis:
        case NodeType::Strikethrough:
        case NodeType::InlineCode:
        case NodeType::Link:
        case NodeType::Image:
        case NodeType::AutoLink:
        case NodeType::MathInline:
        case NodeType::FootnoteRef:
            return ProjectionMode::SourceEditable;

        case NodeType::CodeBlock:
        case NodeType::MathBlock:
        case NodeType::HtmlBlock:
        case NodeType::HtmlInline:
        case NodeType::Table:
        case NodeType::TableHead:
        case NodeType::TableBody:
        case NodeType::TableRow:
        case NodeType::TableCell:
        case NodeType::FootnoteDef:
        case NodeType::FrontMatter:
        case NodeType::Toc:
            return ProjectionMode::Atomic;

        default:
            return ProjectionMode::Atomic;
    }
}

const char* EditorProjection::projectionModeName(ProjectionMode m) {
    switch (m) {
        case ProjectionMode::Editable:       return "editable";
        case ProjectionMode::SourceEditable: return "source-editable";
        case ProjectionMode::Atomic:         return "atomic";
        case ProjectionMode::Hidden:         return "hidden";
        case ProjectionMode::Unsupported:    return "unsupported";
    }
    return "atomic";
}

EditorProjection::EditorProjection(const Engine& engine, RenderOptions options)
    : engine_(engine), options_(std::move(options)) {}

std::string EditorProjection::projectNode(const Node& document, const std::string& nodeId) const {
    RenderOptions options = options_;
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
    RenderOptions options = options_;
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

BlockProjection EditorProjection::projectBlock(const Node& document, const std::string& nodeId, const std::string& fullMarkdown) const {
    BlockProjection proj;
    proj.renderedHtml = projectNode(document, nodeId);
    
    if (const auto* node = findNodeById(document, nodeId)) {
        proj.sourceStart = node->range.begin.offset;
        proj.sourceEnd = node->range.end.offset;
        proj.mode = classifyNode(*node);
        
        if (proj.sourceStart <= fullMarkdown.size() && proj.sourceEnd <= fullMarkdown.size() && proj.sourceStart <= proj.sourceEnd) {
            proj.sourceText = fullMarkdown.substr(proj.sourceStart, proj.sourceEnd - proj.sourceStart);
        }
    }
    return proj;
}

} // namespace mwrender::editor
