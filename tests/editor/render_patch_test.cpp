#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/editor_projection.hpp>
#include <mwrender/editor/render_patch.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testGeneratePatch() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph 1\n");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);
    mwrender::editor::RenderPatchGenerator generator(projection);

    mwrender::TextChange change;
    change.from = 19;
    change.to = 20;
    change.insertedText = "2";

    auto result = session.applyChange(change);
    require(result.ok, "Update should succeed");

    mwrender::editor::Selection sel;
    sel.anchor.offset = 5;
    sel.focus.offset = 5;
    mwrender::editor::RenderPatch patch = generator.generatePatch(session.document(), result, sel);
    
    // Verify revision and selection are passed through
    require(patch.revision == result.revision, "Patch carries revision from UpdateResult");
    require(patch.selection.anchor.offset == 5, "Patch carries selection anchor");
    require(patch.selection.focus.offset == 5, "Patch carries selection focus");
    
    // We expect exactly 1 changed node: the Paragraph
    require(!patch.changedNodes.empty(), "Should have changed nodes");
    require(patch.insertedNodes.empty(), "Should have no inserted nodes");
    require(patch.removedNodeIds.empty(), "Should have no removed nodes");
    
    auto& snippet = patch.changedNodes[0];
    require(!snippet.html.empty(), "Snippet HTML should not be empty");
    require(snippet.html.find("Paragraph 2") != std::string::npos, "Should contain the new text");
}

} // namespace

int main() {
    std::cout << "Running render_patch_test...\n";
    testGeneratePatch();
    std::cout << "All render_patch tests passed.\n";
    return 0;
}
