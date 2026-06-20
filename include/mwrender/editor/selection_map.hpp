#pragma once

#include <string>
#include <cstddef>

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

class SelectionMap {
public:
    explicit SelectionMap(const DocumentSession& session);

    SourcePositionEx visualToSource(const VisualPosition& visual) const;
    VisualPosition sourceToVisual(const SourcePositionEx& source) const;

private:
    const DocumentSession& session_;
};

} // namespace mwrender::editor
