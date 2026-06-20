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

void testEditCommand() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph 1\n");

    mwrender::editor::SelectionMap selMap(session);
    mwrender::editor::EditCommandExecutor executor(session, selMap);

    mwrender::editor::EditCommand cmd;
    cmd.type = mwrender::editor::EditCommandType::ReplaceSelection;
    cmd.selection.anchor.offset = 19;
    cmd.selection.focus.offset = 20;
    cmd.text = "2";

    auto result = executor.execute(cmd);
    require(result.ok, "Command should succeed");
    require(result.markdown == "# Title\n\nParagraph 2\n", "Markdown should be updated");
    require(result.newSelection.anchor.offset == 20, "Selection should move forward");
}

} // namespace

int main() {
    std::cout << "Running edit_command_test...\n";
    testEditCommand();
    std::cout << "All edit_command tests passed.\n";
    return 0;
}
