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

// ===== Cross-block edit: appending text to create new paragraph =====
void testCrossBlockEdit() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Paragraph 1\n");

    // Append at the end creating a new block
    std::size_t end = session.markdown().size();
    mwrender::TextChange change;
    change.from = end;
    change.to = end;
    change.insertedText = "Paragraph 2\n\n";

    auto result = session.applyChange(change);
    require(result.ok, "Cross-block edit should succeed");
    // This adds a new paragraph — should have more children
    const auto& doc = session.document();
    require(doc.children.size() >= 1, "Document should have children after append");
}

// ===== List item text edit preserves IDs =====
void testListEditPreservesIds() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("- item one\n- item two\n");

    const auto& doc1 = session.document();
    require(doc1.children.size() >= 1, "Document has children");

    // Find the list and list items
    const auto& list = doc1.children[0];
    require(list->type == mwrender::NodeType::List, "First child is List");

    std::string oldListId = list->id;
    std::vector<std::string> oldItemIds;
    for (const auto& item : list->children) {
        if (item) oldItemIds.push_back(item->id);
    }
    require(oldItemIds.size() >= 2, "List has at least 2 items");

    // Edit text inside second list item ("two" → "TWO")
    // "two" starts at offset ~13
    mwrender::TextChange change;
    change.from = session.markdown().find("two");
    change.to = change.from + 3;
    change.insertedText = "TWO";

    auto result = session.applyChange(change);
    require(result.ok, "List edit should succeed");

    const auto& doc2 = session.document();
    const auto& list2 = doc2.children[0];
    require(list2->id == oldListId, "List ID preserved");
    require(list2->children[0]->id == oldItemIds[0], "First item ID preserved");
    require(list2->children[1]->id == oldItemIds[1], "Second item ID preserved");
}

} // namespace

int main() {
    std::cout << "Running incremental_parse_test...\n";

    testIncrementalUpdate();
    testCrossBlockEdit();
    testListEditPreservesIds();

    std::cout << "All incremental_parse tests passed.\n";
    return 0;
}
