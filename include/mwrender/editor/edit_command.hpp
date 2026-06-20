#pragma once

#include <string>
#include <vector>
#include <optional>

#include <mwrender/editor/selection_map.hpp>
#include <mwrender/editor/document_session.hpp>

namespace mwrender::editor {

enum class EditCommandType {
    ReplaceSelection,
    DeleteBackward,
    DeleteForward,
    SplitBlock,
    MergeBlock,
    ToggleStrong,
    ToggleEmphasis,
    ToggleTask,
    SetHeadingLevel
};

struct Selection {
    SourcePositionEx anchor;
    SourcePositionEx focus;
};

struct EditCommand {
    EditCommandType type;
    Selection selection;
    std::string nodeId;
    std::string text;
    std::string arg1;
    std::string arg2;
};

struct EditCommandResult {
    bool ok = false;
    std::string markdown;
    Selection newSelection;
};

class EditCommandExecutor {
public:
    explicit EditCommandExecutor(DocumentSession& session, const SelectionMap& selMap);

    EditCommandResult execute(const EditCommand& command);

private:
    DocumentSession& session_;
    const SelectionMap& selMap_;
};

} // namespace mwrender::editor
