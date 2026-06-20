#include <mwrender/editor/edit_command.hpp>
#include <algorithm>

namespace mwrender::editor {

EditCommandExecutor::EditCommandExecutor(DocumentSession& session, const SelectionMap& selMap)
    : session_(session), selMap_(selMap) {}

EditCommandResult EditCommandExecutor::execute(const EditCommand& command) {
    EditCommandResult result;
    std::string markdown = session_.markdown();
    
    std::size_t start = command.selection.anchor.offset;
    std::size_t end = command.selection.focus.offset;
    if (start > end) {
        std::swap(start, end);
    }
    
    if (command.type == EditCommandType::ReplaceSelection) {
        if (start <= end && end <= markdown.size()) {
            markdown.replace(start, end - start, command.text);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start + command.text.size();
            result.newSelection.focus.offset = result.newSelection.anchor.offset;
        }
    } else if (command.type == EditCommandType::DeleteBackward) {
        if (start == end && start > 0) {
            markdown.erase(start - 1, 1);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start - 1;
            result.newSelection.focus.offset = start - 1;
        } else if (start < end) {
            markdown.erase(start, end - start);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = start;
        }
    } else if (command.type == EditCommandType::ToggleStrong) {
        if (start < end && end <= markdown.size()) {
            // Simplified toggle logic: just wrap
            markdown.insert(end, "**");
            markdown.insert(start, "**");
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = end + 4;
        }
    } else if (command.type == EditCommandType::SplitBlock) {
        // Just insert newline
        markdown.insert(start, "\n\n");
        result.ok = true;
        result.markdown = markdown;
        result.newSelection.anchor.offset = start + 2;
        result.newSelection.focus.offset = start + 2;
    }
    
    return result;
}

} // namespace mwrender::editor
