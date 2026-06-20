#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/document_session.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testIncrementalUpdate() {
    mwrender::editor::DocumentSessionOptions options;
    options.enableIncrementalParsing = true;
    mwrender::editor::DocumentSession session(options);

    session.load("# Heading 1\n\nParagraph 1\n\n- item 1\n");
    const auto& doc1 = session.document();
    require(doc1.children.size() == 3, "Should have 3 children");
    
    std::string oldHeadingId = doc1.children[0]->id;
    std::string oldParaId = doc1.children[1]->id;
    std::string oldListId = doc1.children[2]->id;

    // "Paragraph 1" starts at offset 13.
    // Let's modify it to "Paragraph 2".
    mwrender::TextChange change;
    change.from = 23;
    change.to = 24;
    change.insertedText = "2";

    auto result = session.applyChange(change);
    require(result.ok, "Update should succeed");
    
    require(!result.changedNodeIds.empty(), "Should have changed nodes");
    require(result.insertedNodeIds.empty(), "Should have no inserted nodes");
    require(result.removedNodeIds.empty(), "Should have no removed nodes");
    
    // Check if the IDs of Heading and List are preserved
    const auto& doc2 = session.document();
    require(doc2.children[0]->id == oldHeadingId, "Heading ID preserved");
    require(doc2.children[2]->id == oldListId, "List ID preserved");
    require(doc2.children[1]->id == oldParaId, "Paragraph ID preserved (but content changed)");
}

} // namespace

int main() {
    std::cout << "Running incremental_parse_test...\n";
    testIncrementalUpdate();
    std::cout << "All incremental_parse tests passed.\n";
    return 0;
}
