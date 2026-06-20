#include <mwrender/editor/selection_map.hpp>
#include <mwrender/editor/document_session.hpp>
#include <mwrender/query.hpp>

namespace mwrender::editor {

SelectionMap::SelectionMap(const DocumentSession& session)
    : session_(session) {}

namespace {

void collectSegments(const Node& node, std::vector<ProjectionSegment>& segments, std::string& visibleText) {
    if (!node.children.empty()) {
        std::size_t currentOffset = node.range.begin.offset;
        for (const auto& child : node.children) {
            if (!child) continue;
            if (child->range.begin.offset > currentOffset) {
                ProjectionSegment markerSeg;
                markerSeg.projectionId = node.id + "_marker_" + std::to_string(currentOffset);
                markerSeg.nodeId = node.id;
                markerSeg.sourceRange.begin.offset = currentOffset;
                markerSeg.sourceRange.end.offset = child->range.begin.offset;
                markerSeg.kind = ProjectionSegmentKind::Hidden;
                segments.push_back(markerSeg);
            }
            collectSegments(*child, segments, visibleText);
            currentOffset = child->range.end.offset;
        }
        if (node.range.end.offset > currentOffset) {
            ProjectionSegment markerSeg;
            markerSeg.projectionId = node.id + "_marker_" + std::to_string(currentOffset);
            markerSeg.nodeId = node.id;
            markerSeg.sourceRange.begin.offset = currentOffset;
            markerSeg.sourceRange.end.offset = node.range.end.offset;
            markerSeg.kind = ProjectionSegmentKind::Hidden;
            segments.push_back(markerSeg);
        }
        return;
    }

    if (node.contentRange.begin.offset > node.range.begin.offset || node.contentRange.end.offset < node.range.end.offset) {
        if (node.contentRange.begin.offset > node.range.begin.offset) {
            ProjectionSegment markerSeg;
            markerSeg.projectionId = node.id + "_marker_start";
            markerSeg.nodeId = node.id;
            markerSeg.sourceRange.begin.offset = node.range.begin.offset;
            markerSeg.sourceRange.end.offset = node.contentRange.begin.offset;
            markerSeg.kind = ProjectionSegmentKind::Hidden;
            segments.push_back(markerSeg);
        }
        
        ProjectionSegment textSeg;
        textSeg.projectionId = node.id + "_text";
        textSeg.nodeId = node.id;
        textSeg.sourceRange = node.contentRange;
        textSeg.kind = ProjectionSegmentKind::Text;
        textSeg.text = node.literal;
        segments.push_back(textSeg);
        visibleText += textSeg.text;

        if (node.range.end.offset > node.contentRange.end.offset) {
            ProjectionSegment markerSeg;
            markerSeg.projectionId = node.id + "_marker_end";
            markerSeg.nodeId = node.id;
            markerSeg.sourceRange.begin.offset = node.contentRange.end.offset;
            markerSeg.sourceRange.end.offset = node.range.end.offset;
            markerSeg.kind = ProjectionSegmentKind::Hidden;
            segments.push_back(markerSeg);
        }
        return;
    }

    ProjectionSegment seg;
    seg.projectionId = node.id + "_text";
    seg.nodeId = node.id;
    seg.sourceRange = node.range;
    seg.kind = ProjectionSegmentKind::Text;
    seg.text = node.literal;
    segments.push_back(seg);
    visibleText += seg.text;
}

} // namespace

SourcePositionEx SelectionMap::visualToSource(const VisualPosition& visual) const {
    SourcePositionEx source;
    source.contextNodeId = visual.nodeId;
    source.affinity = Affinity::After;

    const Node* node = session_.findNodeById(visual.nodeId);
    if (!node) {
        source.offset = visual.textOffset;
        return source;
    }

    std::vector<ProjectionSegment> segments;
    std::string visibleText;
    collectSegments(*node, segments, visibleText);

    std::size_t visibleByteOffset = visual.textOffset;

    if (visibleByteOffset == 0) {
        for (const auto& seg : segments) {
            if (seg.kind == ProjectionSegmentKind::Text) {
                source.offset = seg.sourceRange.begin.offset;
                return source;
            }
        }
        source.offset = node->contentRange.begin.offset;
        return source;
    }

    std::size_t currentVisualOffset = 0;
    const ProjectionSegment* lastTextSeg = nullptr;

    for (const auto& seg : segments) {
        if (seg.kind == ProjectionSegmentKind::Text) {
            std::size_t len = seg.text.size();
            if (visibleByteOffset >= currentVisualOffset && visibleByteOffset <= currentVisualOffset + len) {
                std::size_t rel = visibleByteOffset - currentVisualOffset;
                source.offset = seg.sourceRange.begin.offset + rel;
                return source;
            }
            currentVisualOffset += len;
            lastTextSeg = &seg;
        }
    }

    if (lastTextSeg) {
        source.offset = lastTextSeg->sourceRange.end.offset;
    } else {
        source.offset = node->contentRange.end.offset;
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

    std::vector<ProjectionSegment> segments;
    std::string visibleText;
    collectSegments(*node, segments, visibleText);

    std::size_t visibleByteOffset = 0;
    bool found = false;

    for (const auto& seg : segments) {
        if (seg.kind == ProjectionSegmentKind::Text) {
            if (source.offset >= seg.sourceRange.begin.offset && source.offset <= seg.sourceRange.end.offset) {
                std::size_t rel = source.offset - seg.sourceRange.begin.offset;
                visibleByteOffset = visibleByteOffset + rel;
                found = true;
                break;
            }
            visibleByteOffset += seg.text.size();
        } else {
            // Hidden segment
            if (source.offset >= seg.sourceRange.begin.offset && source.offset <= seg.sourceRange.end.offset) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        if (source.offset < node->contentRange.begin.offset) {
            visibleByteOffset = 0;
        } else {
            visibleByteOffset = visibleText.size();
        }
    }

    visual.textOffset = visibleByteOffset;
    return visual;
}

} // namespace mwrender::editor
