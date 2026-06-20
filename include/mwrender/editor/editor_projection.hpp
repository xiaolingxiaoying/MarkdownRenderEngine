#pragma once

#include <string>

#include <mwrender/ast.hpp>
#include <mwrender/engine.hpp>

namespace mwrender::editor {

class EditorProjection {
public:
    explicit EditorProjection(const Engine& engine);

    std::string projectNode(const Node& document, const std::string& nodeId) const;
    std::string projectDocument(const Node& document) const;

private:
    const Engine& engine_;
};

} // namespace mwrender::editor
