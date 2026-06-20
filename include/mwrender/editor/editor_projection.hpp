#pragma once

#include <string>

#include <mwrender/ast.hpp>
#include <mwrender/engine.hpp>

namespace mwrender::editor {

enum class Editability {
    Editable,
    SourceEditable,
    Atomic
};

class EditorProjection {
public:
    explicit EditorProjection(const Engine& engine);

    static Editability classifyNode(const Node& node);
    static const char* editabilityName(Editability e);

    std::string projectNode(const Node& document, const std::string& nodeId) const;
    std::string projectDocument(const Node& document) const;

private:
    const Engine& engine_;
};

} // namespace mwrender::editor
