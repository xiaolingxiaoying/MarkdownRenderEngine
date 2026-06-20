#pragma once

#include <string>
#include <cstddef>
#include <vector>
#include <mwrender/source.hpp>

namespace mwrender::editor {

class DocumentSession;

enum class Affinity {
    Before,
    After
};

struct VisualPosition {
    std::string nodeId;
    std::size_t textOffset = 0;
};

struct SourcePositionEx {
    std::size_t offset = 0;
    Affinity affinity = Affinity::After;
    std::string contextNodeId;
};

enum class ProjectionSegmentKind {
    Text,
    Marker,
    Hidden
};

struct ProjectionSegment {
    std::string projectionId;
    std::string nodeId;
    SourceRange sourceRange;
    ProjectionSegmentKind kind = ProjectionSegmentKind::Text;
    std::string text;
};

class SelectionMap {
public:
    explicit SelectionMap(const DocumentSession& session);

    SourcePositionEx visualToSource(const VisualPosition& visual) const;
    VisualPosition sourceToVisual(const SourcePositionEx& source) const;

private:
    const DocumentSession& session_;
};

} // namespace mwrender::editor
