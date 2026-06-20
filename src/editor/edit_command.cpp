#include <mwrender/editor/edit_command.hpp>
#include <mwrender/editor/document_session.hpp>
#include <mwrender/utf.hpp>
#include <algorithm>
#include <cstdlib>

namespace mwrender::editor {

EditCommandExecutor::EditCommandExecutor(DocumentSession& session, const SelectionMap& selMap)
    : session_(session), selMap_(selMap) {}

namespace {

std::size_t lineStart(std::string_view s, std::size_t pos) {
    if (pos == 0) return 0;
    std::size_t ls = s.rfind('\n', pos - 1);
    return ls == std::string::npos ? 0 : ls + 1;
}

std::size_t lineEnd(std::string_view s, std::size_t pos) {
    std::size_t le = s.find('\n', pos);
    return le == std::string::npos ? s.size() : le;
}

bool isHeadingLine(std::string_view s, std::size_t ls) {
    if (ls >= s.size()) return false;
    char c = s[ls];
    return c == '#';
}

int headingLevel(std::string_view s, std::size_t ls) {
    int level = 0;
    while (ls + level < s.size() && s[ls + level] == '#') ++level;
    return level;
}

} // namespace

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
        } else {
            result.fallbackReason = "Selection boundaries are out of range for ReplaceSelection";
        }
    } else if (command.type == EditCommandType::DeleteBackward) {
        if (start == end && start > 0) {
            std::size_t bound = previousUtf8Boundary(markdown, start);
            markdown.erase(bound, start - bound);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = bound;
            result.newSelection.focus.offset = bound;
        } else if (start < end) {
            markdown.erase(start, end - start);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = start;
        } else {
            result.fallbackReason = "Cannot delete backward at the beginning of the document";
        }
    } else if (command.type == EditCommandType::DeleteForward) {
        if (start < end) {
            markdown.erase(start, end - start);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = start;
        } else if (start < markdown.size()) {
            std::size_t bound = nextUtf8Boundary(markdown, start);
            markdown.erase(start, bound - start);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = start;
        } else {
            result.fallbackReason = "Cannot delete forward at the end of the document";
        }
    } else if (command.type == EditCommandType::SplitBlock) {
        if (start <= markdown.size()) {
            markdown.insert(start, "\n\n");
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start + 2;
            result.newSelection.focus.offset = start + 2;
        } else {
            result.fallbackReason = "Cursor offset is out of range for SplitBlock";
        }
    } else if (command.type == EditCommandType::MergeBlock) {
        if (start > 0) {
            std::size_t sep = markdown.rfind("\n\n", start - 1);
            if (sep != std::string::npos && sep + 2 <= start) {
                markdown.erase(sep, start - sep);
                result.ok = true;
                result.markdown = markdown;
                result.newSelection.anchor.offset = sep;
                result.newSelection.focus.offset = sep;
            } else if (start < markdown.size() && markdown[start] == '\n') {
                markdown.erase(start, 1);
                result.ok = true;
                result.markdown = markdown;
                result.newSelection.anchor.offset = start;
                result.newSelection.focus.offset = start;
            } else {
                result.fallbackReason = "No block to merge with";
            }
        } else {
            result.fallbackReason = "Cannot merge block at the beginning of the document";
        }
    } else if (command.type == EditCommandType::ToggleStrong) {
        if (start < end && end <= markdown.size()) {
            markdown.insert(end, "**");
            markdown.insert(start, "**");
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = end + 4;
        } else {
            result.fallbackReason = "ToggleStrong requires a non-empty selection";
        }
    } else if (command.type == EditCommandType::ToggleEmphasis) {
        if (start < end && end <= markdown.size()) {
            markdown.insert(end, "*");
            markdown.insert(start, "*");
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = end + 2;
        } else {
            result.fallbackReason = "ToggleEmphasis requires a non-empty selection";
        }
    } else if (command.type == EditCommandType::ToggleTask) {
        if (start > markdown.size()) {
            result.fallbackReason = "Cursor offset is out of range for ToggleTask";
            return result;
        }
        std::size_t ls = lineStart(markdown, start);
        std::size_t le = lineEnd(markdown, ls);

        // Search for existing task marker [ ] or [x]
        bool found = false;
        for (std::size_t i = ls; i + 2 < le; ++i) {
            if (markdown[i] == '[' && (markdown[i+1] == ' ' || markdown[i+1] == 'x') && markdown[i+2] == ']') {
                markdown[i+1] = (markdown[i+1] == ' ') ? 'x' : ' ';
                result.ok = true;
                result.markdown = markdown;
                result.newSelection.anchor.offset = start;
                result.newSelection.focus.offset = start;
                found = true;
                break;
            }
        }
        if (!found) {
            markdown.insert(ls, "- [ ] ");
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start + 6;
            result.newSelection.focus.offset = start + 6;
        }
    } else if (command.type == EditCommandType::SetHeadingLevel) {
        if (start > markdown.size()) {
            result.fallbackReason = "Cursor offset is out of range for SetHeadingLevel";
            return result;
        }
        int level = 1;
        if (!command.arg1.empty()) {
            level = std::stoi(command.arg1);
            if (level < 1) level = 1;
            if (level > 6) level = 6;
        }
        std::string marker(static_cast<std::size_t>(level), '#');

        std::size_t ls = lineStart(markdown, start);
        if (isHeadingLine(markdown, ls)) {
            int oldLevel = headingLevel(markdown, ls);
            markdown.replace(ls, static_cast<std::size_t>(oldLevel), marker);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start;
            result.newSelection.focus.offset = start;
        } else {
            markdown.insert(ls, marker + " ");
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start + level + 1;
            result.newSelection.focus.offset = start + level + 1;
        }
    } else if (command.type == EditCommandType::ToggleInlineCode) {
        if (start < end && end <= markdown.size()) {
            bool wrapped = (start >= 1 && end + 1 <= markdown.size() &&
                            markdown[start - 1] == '`' && markdown[end] == '`');
            if (wrapped) {
                markdown.erase(end, 1);
                markdown.erase(start - 1, 1);
                result.ok = true;
                result.markdown = markdown;
                result.newSelection.anchor.offset = start - 1;
                result.newSelection.focus.offset = start - 1;
            } else {
                markdown.insert(end, "`");
                markdown.insert(start, "`");
                result.ok = true;
                result.markdown = markdown;
                result.newSelection.anchor.offset = start;
                result.newSelection.focus.offset = end + 2;
            }
        } else {
            result.fallbackReason = "ToggleInlineCode requires a non-empty selection";
        }
    } else if (command.type == EditCommandType::ToggleStrikethrough) {
        if (start < end && end <= markdown.size()) {
            bool wrapped = (start >= 2 && end + 2 <= markdown.size() &&
                            markdown.substr(start - 2, 2) == "~~" &&
                            markdown.substr(end, 2) == "~~");
            if (wrapped) {
                markdown.erase(end, 2);
                markdown.erase(start - 2, 2);
                result.ok = true;
                result.markdown = markdown;
                result.newSelection.anchor.offset = start - 2;
                result.newSelection.focus.offset = start - 2;
            } else {
                markdown.insert(end, "~~");
                markdown.insert(start, "~~");
                result.ok = true;
                result.markdown = markdown;
                result.newSelection.anchor.offset = start;
                result.newSelection.focus.offset = end + 4;
            }
        } else {
            result.fallbackReason = "ToggleStrikethrough requires a non-empty selection";
        }
    } else if (command.type == EditCommandType::InsertLink) {
        if (start <= end && end <= markdown.size()) {
            std::string url = command.arg1.empty() ? "url" : command.arg1;
            std::string text = command.text.empty()
                ? markdown.substr(start, end - start)
                : command.text;
            std::string link = "[" + text + "](" + url + ")";
            markdown.replace(start, end - start, link);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start + link.size();
            result.newSelection.focus.offset = start + link.size();
        } else {
            result.fallbackReason = "Selection boundaries are out of range for InsertLink";
        }
    } else if (command.type == EditCommandType::InsertImage) {
        if (start <= end && end <= markdown.size()) {
            std::string url = command.arg1.empty() ? "url" : command.arg1;
            std::string alt = command.text.empty()
                ? markdown.substr(start, end - start)
                : command.text;
            std::string image = "![" + alt + "](" + url + ")";
            markdown.replace(start, end - start, image);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start + image.size();
            result.newSelection.focus.offset = start + image.size();
        } else {
            result.fallbackReason = "Selection boundaries are out of range for InsertImage";
        }
    } else if (command.type == EditCommandType::ToggleList) {
        if (start > markdown.size()) {
            result.fallbackReason = "Cursor offset is out of range for ToggleList";
            return result;
        }
        std::size_t ls = lineStart(markdown, start);
        // Check if line already starts with a list marker
        bool hasListMarker = (markdown[ls] == '-' || markdown[ls] == '*' || markdown[ls] == '+');
        if (hasListMarker && ls + 1 < markdown.size() && markdown[ls + 1] == ' ') {
            // Remove list marker
            markdown.erase(ls, 2);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start > ls + 1 ? start - 2 : ls;
            result.newSelection.focus.offset = result.newSelection.anchor.offset;
        } else {
            // Insert list marker
            markdown.insert(ls, "- ");
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start + 2;
            result.newSelection.focus.offset = start + 2;
        }
    } else if (command.type == EditCommandType::IndentListItem) {
        if (start > markdown.size()) {
            result.fallbackReason = "Cursor offset is out of range for IndentListItem";
            return result;
        }
        std::size_t ls = lineStart(markdown, start);
        markdown.insert(ls, "    ");
        result.ok = true;
        result.markdown = markdown;
        result.newSelection.anchor.offset = start + 4;
        result.newSelection.focus.offset = start + 4;
    } else if (command.type == EditCommandType::OutdentListItem) {
        if (start > markdown.size()) {
            result.fallbackReason = "Cursor offset is out of range for OutdentListItem";
            return result;
        }
        std::size_t ls = lineStart(markdown, start);
        if (ls + 4 <= markdown.size() && markdown.substr(ls, 4) == "    ") {
            markdown.erase(ls, 4);
            result.ok = true;
            result.markdown = markdown;
            result.newSelection.anchor.offset = start > ls + 3 ? start - 4 : ls;
            result.newSelection.focus.offset = result.newSelection.anchor.offset;
        } else {
            result.fallbackReason = "No indentation to remove";
        }
    } else {
        result.fallbackReason = "Unknown EditCommandType";
    }
    
    return result;
}

} // namespace mwrender::editor
