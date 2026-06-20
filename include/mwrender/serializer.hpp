#pragma once

#include <string>
#include <string_view>

#include <mwrender/ast.hpp>

namespace mwrender {

struct MarkdownStyle {
    std::string bulletMarker = "-";
    std::string strongMarker = "**";
    std::string emphasisMarker = "*";
    std::string codeFenceMarker = "```";
    std::string lineEnding = "\n";

    bool preserveOriginalMarkers = true;
    bool preserveBlankLines = true;
    bool preserveListStartNumber = true;
    bool preserveTaskMarkerCase = true;
};

struct Command {
    enum class Type {
        ToggleTask,
        SetHeadingLevel,
        ToggleList,
        WrapStrong,
        WrapEmphasis,
        InsertLink,
        InsertImage,
        ToggleStrikethrough,
        IndentListItem,
        OutdentListItem
    };
    Type type;
    std::string nodeId;
    std::string arg1;
    std::string arg2;
};

[[nodiscard]] std::string serializeMarkdown(
    const Node& document,
    const MarkdownStyle& style = {});

[[nodiscard]] std::string serializeNode(
    const Node& node,
    const MarkdownStyle& style = {},
    int indent = 0);

} // namespace mwrender
