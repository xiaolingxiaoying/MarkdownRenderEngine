#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/edit_command.hpp>
#include <mwrender/editor/selection_map.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

// ===== helpers =====
mwrender::editor::EditCommandExecutor makeExecutor(mwrender::editor::DocumentSession& session) {
    mwrender::editor::SelectionMap selMap(session);
    return mwrender::editor::EditCommandExecutor(session, selMap);
}

mwrender::editor::EditCommand selCmd(std::size_t anchor, std::size_t focus) {
    mwrender::editor::EditCommand cmd;
    cmd.selection.anchor.offset = anchor;
    cmd.selection.focus.offset = focus;
    return cmd;
}

// ===== ReplaceSelection =====
void testReplaceSelection() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph 1\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(19, 20);
    cmd.type = mwrender::editor::EditCommandType::ReplaceSelection;
    cmd.text = "2";

    auto result = executor.execute(cmd);
    require(result.ok, "ReplaceSelection should succeed");
    require(result.markdown == "# Title\n\nParagraph 2\n", "Markdown should be updated");
    require(result.newSelection.anchor.offset == 20, "Selection should move forward");
}

// ===== DeleteBackward =====
void testDeleteBackwardChar() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("abc");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(1, 1);
    cmd.type = mwrender::editor::EditCommandType::DeleteBackward;

    auto result = executor.execute(cmd);
    require(result.ok, "DeleteBackward should succeed");
    require(result.markdown == "bc", "DeleteBackward should remove char before cursor");
    require(result.newSelection.anchor.offset == 0, "Cursor should be at 0");
}

void testDeleteBackwardSelection() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("abcd");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(1, 3);
    cmd.type = mwrender::editor::EditCommandType::DeleteBackward;

    auto result = executor.execute(cmd);
    require(result.ok, "DeleteBackward with selection should succeed");
    require(result.markdown == "ad", "DeleteBackward should remove selected text");
    require(result.newSelection.anchor.offset == 1, "Cursor after deletion");
}

// ===== DeleteForward =====
void testDeleteForwardChar() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("abcd");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(1, 1);
    cmd.type = mwrender::editor::EditCommandType::DeleteForward;

    auto result = executor.execute(cmd);
    require(result.ok, "DeleteForward should succeed");
    require(result.markdown == "acd", "DeleteForward should remove char at cursor");
    require(result.newSelection.anchor.offset == 1, "Cursor stays at 1");
}

void testDeleteForwardSelection() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("abcdef");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(2, 5);
    cmd.type = mwrender::editor::EditCommandType::DeleteForward;

    auto result = executor.execute(cmd);
    require(result.ok, "DeleteForward with selection should succeed");
    require(result.markdown == "abf", "DeleteForward should remove selected text");
    require(result.newSelection.anchor.offset == 2, "Cursor after deletion");
}

void testDeleteForwardAtEnd() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("abc");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(3, 3);
    cmd.type = mwrender::editor::EditCommandType::DeleteForward;

    auto result = executor.execute(cmd);
    require(!result.ok, "DeleteForward at end should fail");
    require(result.fallbackReason == "Cannot delete forward at the end of the document", "Should set fallback reason");
}

// ===== SplitBlock =====
void testSplitBlock() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Helloworld");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(5, 5);
    cmd.type = mwrender::editor::EditCommandType::SplitBlock;

    auto result = executor.execute(cmd);
    require(result.ok, "SplitBlock should succeed");
    require(result.markdown == "Hello\n\nworld", "SplitBlock should insert \\n\\n");
    require(result.newSelection.anchor.offset == 7, "Cursor should be after \\n\\n");
}

// ===== MergeBlock =====
void testMergeBlock() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("First paragraph\n\nSecond paragraph\n");

    // Cursor at position 17 = start of "Second paragraph" (after the \n\n separator)
    auto executor = makeExecutor(session);
    auto cmd = selCmd(17, 17);
    cmd.type = mwrender::editor::EditCommandType::MergeBlock;

    auto result = executor.execute(cmd);
    require(result.ok, "MergeBlock should succeed");
    require(result.markdown == "First paragraphSecond paragraph\n", "MergeBlock should remove \\n\\n");
    require(result.newSelection.anchor.offset == 15, "Cursor should be at merge point");
}

// ===== ToggleStrong =====
void testToggleStrong() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello world");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 5);
    cmd.type = mwrender::editor::EditCommandType::ToggleStrong;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleStrong should succeed");
    require(result.markdown == "**Hello** world", "ToggleStrong should wrap with **");
    require(result.newSelection.anchor.offset == 0, "Cursor at start");
}

void testToggleStrongFail() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello world");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 0); // Empty selection should fail
    cmd.type = mwrender::editor::EditCommandType::ToggleStrong;

    auto result = executor.execute(cmd);
    require(!result.ok, "ToggleStrong on empty selection should fail");
    require(result.fallbackReason == "ToggleStrong requires a non-empty selection", "Should set fallback reason");
}

// ===== ToggleEmphasis =====
void testToggleEmphasis() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello world");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 5);
    cmd.type = mwrender::editor::EditCommandType::ToggleEmphasis;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleEmphasis should succeed");
    require(result.markdown == "*Hello* world", "ToggleEmphasis should wrap with *");
    require(result.newSelection.anchor.offset == 0, "Cursor at start");
}

// ===== ToggleTask =====
void testToggleTaskOn() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("- [ ] task\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(6, 6);
    cmd.type = mwrender::editor::EditCommandType::ToggleTask;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleTask should succeed");
    require(result.markdown == "- [x] task\n", "ToggleTask should check the box");
}

void testToggleTaskOff() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("- [x] done\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(6, 6);
    cmd.type = mwrender::editor::EditCommandType::ToggleTask;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleTask should succeed");
    require(result.markdown == "- [ ] done\n", "ToggleTask should uncheck the box");
}

void testToggleTaskInsert() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("plain line\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 0);
    cmd.type = mwrender::editor::EditCommandType::ToggleTask;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleTask should insert marker");
    require(result.markdown == "- [ ] plain line\n", "ToggleTask should insert - [ ] prefix");
}

// ===== SetHeadingLevel =====
void testSetHeadingLevelChange() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("## Title\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(3, 3);
    cmd.type = mwrender::editor::EditCommandType::SetHeadingLevel;
    cmd.arg1 = "1";

    auto result = executor.execute(cmd);
    require(result.ok, "SetHeadingLevel should succeed");
    require(result.markdown == "# Title\n", "SetHeadingLevel should change ## to #");
}

void testSetHeadingLevelInsert() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Plain text\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 0);
    cmd.type = mwrender::editor::EditCommandType::SetHeadingLevel;
    cmd.arg1 = "3";

    auto result = executor.execute(cmd);
    require(result.ok, "SetHeadingLevel should insert marker");
    require(result.markdown == "### Plain text\n", "SetHeadingLevel should add ### prefix");
}

// ===== ToggleInlineCode =====
void testToggleInlineCodeWrap() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello world");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 5);
    cmd.type = mwrender::editor::EditCommandType::ToggleInlineCode;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleInlineCode wrap should succeed");
    require(result.markdown == "`Hello` world", "Should wrap with backticks");
}

void testToggleInlineCodeUnwrap() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("`Hello` world");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(1, 6);
    cmd.type = mwrender::editor::EditCommandType::ToggleInlineCode;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleInlineCode unwrap should succeed");
    require(result.markdown == "Hello world", "Should remove backticks");
}

// ===== ToggleStrikethrough =====
void testToggleStrikethroughWrap() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello world");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 5);
    cmd.type = mwrender::editor::EditCommandType::ToggleStrikethrough;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleStrikethrough wrap should succeed");
    require(result.markdown == "~~Hello~~ world", "Should wrap with ~~");
}

void testToggleStrikethroughUnwrap() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("~~Hello~~ world");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(2, 7);
    cmd.type = mwrender::editor::EditCommandType::ToggleStrikethrough;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleStrikethrough unwrap should succeed");
    require(result.markdown == "Hello world", "Should remove ~~");
}

// ===== InsertLink =====
void testInsertLink() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("click here");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 10);
    cmd.type = mwrender::editor::EditCommandType::InsertLink;
    cmd.arg1 = "https://example.com";

    auto result = executor.execute(cmd);
    require(result.ok, "InsertLink should succeed");
    require(result.markdown == "[click here](https://example.com)",
            "Should insert markdown link");
}

// ===== InsertImage =====
void testInsertImage() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("alt text");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 8);
    cmd.type = mwrender::editor::EditCommandType::InsertImage;
    cmd.arg1 = "https://example.com/img.png";

    auto result = executor.execute(cmd);
    require(result.ok, "InsertImage should succeed");
    require(result.markdown == "![alt text](https://example.com/img.png)",
            "Should insert markdown image");
}

// ===== ToggleList =====
void testToggleListAdd() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("item\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 0);
    cmd.type = mwrender::editor::EditCommandType::ToggleList;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleList add should succeed");
    require(result.markdown == "- item\n", "Should add list marker");
}

void testToggleListRemove() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("- item\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(2, 2);
    cmd.type = mwrender::editor::EditCommandType::ToggleList;

    auto result = executor.execute(cmd);
    require(result.ok, "ToggleList remove should succeed");
    require(result.markdown == "item\n", "Should remove list marker");
}

// ===== IndentListItem / OutdentListItem =====
void testIndentListItem() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("- item\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(2, 2);
    cmd.type = mwrender::editor::EditCommandType::IndentListItem;

    auto result = executor.execute(cmd);
    require(result.ok, "IndentListItem should succeed");
    require(result.markdown == "    - item\n", "Should add 4 spaces indent");
}

void testOutdentListItem() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("    - item\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(6, 6);
    cmd.type = mwrender::editor::EditCommandType::OutdentListItem;

    auto result = executor.execute(cmd);
    require(result.ok, "OutdentListItem should succeed");
    require(result.markdown == "- item\n", "Should remove 4 spaces indent");
}

// ===== Chinese text edit =====
void testChineseReplaceSelection() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello \n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(6, 6);
    cmd.type = mwrender::editor::EditCommandType::ReplaceSelection;
    cmd.text = "世界";

    auto result = executor.execute(cmd);
    require(result.ok, "Chinese ReplaceSelection should succeed");
    require(result.markdown == "Hello 世界\n", "Markdown should contain Chinese text");
    require(result.newSelection.anchor.offset == 12, "Cursor after Chinese text");
}

void testSetHeadingLevelWithContent() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello world\n");

    auto executor = makeExecutor(session);
    auto cmd = selCmd(0, 0);
    cmd.type = mwrender::editor::EditCommandType::SetHeadingLevel;
    cmd.arg1 = "2";

    auto result = executor.execute(cmd);
    require(result.ok, "SetHeadingLevel on content should succeed");
    require(result.markdown == "## Hello world\n",
            "Should prepend heading marker to content");
}

} // namespace

int main() {
    std::cout << "Running edit_command_test...\n";

    testReplaceSelection();
    testDeleteBackwardChar();
    testDeleteBackwardSelection();
    testDeleteForwardChar();
    testDeleteForwardSelection();
    testDeleteForwardAtEnd();
    testSplitBlock();
    testMergeBlock();
    testToggleStrong();
    testToggleStrongFail();
    testToggleEmphasis();
    testToggleTaskOn();
    testToggleTaskOff();
    testToggleTaskInsert();
    testSetHeadingLevelChange();
    testSetHeadingLevelInsert();
    testChineseReplaceSelection();
    testSetHeadingLevelWithContent();
    testToggleInlineCodeWrap();
    testToggleInlineCodeUnwrap();
    testToggleStrikethroughWrap();
    testToggleStrikethroughUnwrap();
    testInsertLink();
    testInsertImage();
    testToggleListAdd();
    testToggleListRemove();
    testIndentListItem();
    testOutdentListItem();

    std::cout << "All edit_command tests passed.\n";
    return 0;
}
