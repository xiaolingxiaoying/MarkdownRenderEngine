#include <mwrender/editor/selection_map.hpp>
#include <mwrender/editor/document_session.hpp>
#include <mwrender/query.hpp>

namespace mwrender::editor {

SelectionMap::SelectionMap(const DocumentSession& session)
    : session_(session) {}

namespace {

// 获取节点的"可视文本起始偏移"：排除所有标记后，可见文本在 source 中的起始位置
std::size_t visualBase(const Node& node) {
    // Text 和 CodeBlock 节点使用 range.begin（它们本身就是可视文本）
    if (node.type == NodeType::Text || node.type == NodeType::CodeBlock) {
        return node.range.begin.offset;
    }
    // 容器节点使用 contentRange.begin（已排除开头标记）
    return node.contentRange.begin.offset;
}

// 获取节点的"可视文本结束偏移"
std::size_t visualEnd(const Node& node) {
    if (node.type == NodeType::Text || node.type == NodeType::CodeBlock) {
        return node.range.end.offset;
    }
    return node.contentRange.end.offset;
}

}

SourcePositionEx SelectionMap::visualToSource(const VisualPosition& visual) const {
    SourcePositionEx source;
    source.contextNodeId = visual.nodeId;
    source.affinity = Affinity::After;

    const Node* node = session_.findNodeById(visual.nodeId);

    // Fallback: 如果 nodeId 找不到（patch 后节点重建），按 offset 搜索文档
    if (!node) {
        source.offset = visual.textOffset;
        return source;
    }

    std::size_t base = visualBase(*node);
    source.offset = base + visual.textOffset;

    if (source.offset > visualEnd(*node)) {
        source.offset = visualEnd(*node);
    }
    if (source.offset < node->range.begin.offset) {
        source.offset = node->range.begin.offset;
    }

    return source;
}

VisualPosition SelectionMap::sourceToVisual(const SourcePositionEx& source) const {
    VisualPosition visual;
    visual.nodeId = source.contextNodeId;

    const Node* node = session_.findNodeById(source.contextNodeId);
    if (!node) {
        visual.textOffset = source.offset;
        return visual;
    }

    std::size_t base = visualBase(*node);
    if (source.offset >= base) {
        visual.textOffset = source.offset - base;
    }
    if (visual.textOffset > visualEnd(*node) - base) {
        visual.textOffset = visualEnd(*node) - base;
    }

    return visual;
}

} // namespace mwrender::editor
