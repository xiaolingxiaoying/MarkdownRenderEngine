#pragma once

#include <string>

#include <mwrender/ast.hpp>
#include <mwrender/engine.hpp>

namespace mwrender::editor {

enum class ProjectionMode {
    Editable,
    SourceEditable,
    Atomic,
    Hidden,
    Unsupported
};

struct BlockProjection {
    std::string renderedHtml;
    std::string sourceText;
    std::size_t sourceStart = 0;
    std::size_t sourceEnd = 0;
    ProjectionMode mode = ProjectionMode::Atomic;
};

class EditorProjection {
public:
    explicit EditorProjection(const Engine& engine, RenderOptions options = {});

    static ProjectionMode classifyNode(const Node& node);
    static const char* projectionModeName(ProjectionMode m);

    std::string projectNode(const Node& document, const std::string& nodeId) const;
    std::string projectDocument(const Node& document) const;

    BlockProjection projectBlock(const Node& document, const std::string& nodeId, const std::string& fullMarkdown) const;

private:
    const Engine& engine_;
    RenderOptions options_;
};

} // namespace mwrender::editor
