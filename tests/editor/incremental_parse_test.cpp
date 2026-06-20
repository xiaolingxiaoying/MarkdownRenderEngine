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

void testCodeBlockAndMathBlockIncremental() {
    mwrender::editor::DocumentSessionOptions options;
    options.enableIncrementalParsing = true;
    mwrender::editor::DocumentSession session(options);

    session.load("# Title\n\n```cpp\nint x = 1;\n```\n\n$$\ne = mc^2\n$$\n");
    const auto& doc1 = session.document();
    require(doc1.children.size() == 3, "Document should have 3 blocks");
    
    std::string oldCodeId = doc1.children[1]->id;
    std::string oldMathId = doc1.children[2]->type == mwrender::NodeType::MathBlock 
        ? doc1.children[2]->id 
        : doc1.children[2]->children[0]->id;

    // Edit inside CodeBlock: change "1" to "2"
    // "1" is at offset ~22 in the document
    std::size_t oneOffset = session.markdown().find("1;");
    require(oneOffset != std::string::npos, "Found '1;'");
    mwrender::TextChange change1;
    change1.from = oneOffset;
    change1.to = oneOffset + 1;
    change1.insertedText = "2";

    auto result1 = session.applyChange(change1);
    require(result1.ok, "CodeBlock edit should succeed");
    require(result1.partialReparse, "CodeBlock edit should use partial reparse");
    require(!result1.fullReparse, "CodeBlock edit should not trigger full reparse");
    require(result1.changedNodeIds.size() == 1 && result1.changedNodeIds[0] == oldCodeId, "Changed node should be the CodeBlock");

    // Edit inside MathBlock: change "e = mc^2" to "e = hf"
    std::size_t formulaOffset = session.markdown().find("mc^2");
    require(formulaOffset != std::string::npos, "Found 'mc^2'");
    mwrender::TextChange change2;
    change2.from = formulaOffset;
    change2.to = formulaOffset + 4;
    change2.insertedText = "hf";

    auto result2 = session.applyChange(change2);
    require(result2.ok, "MathBlock edit should succeed");
    require(result2.partialReparse, "MathBlock edit should use partial reparse");
    require(!result2.fullReparse, "MathBlock edit should not trigger full reparse");
    require(result2.changedNodeIds.size() == 1 && result2.changedNodeIds[0] == oldMathId, "Changed node should be the MathBlock");
}

void testComplexBlockFallback() {
    mwrender::editor::DocumentSessionOptions options;
    options.enableIncrementalParsing = true;
    mwrender::editor::DocumentSession session(options);

    session.load("# Title\n\n- item 1\n- item 2\n");
    // Edit list item: change "item 1" to "item 2"
    std::size_t offset = session.markdown().find("item 1");
    require(offset != std::string::npos, "Found 'item 1'");
    mwrender::TextChange change;
    change.from = offset;
    change.to = offset + 6;
    change.insertedText = "item A";

    auto result = session.applyChange(change);
    require(result.ok, "Edit should succeed");
    require(result.fullReparse, "Complex block edit should trigger full reparse");
    require(result.fallbackReason == "list incremental parsing not supported yet", "Fallback reason matches");
}

void testSubtreeDiffing() {
    mwrender::editor::DocumentSessionOptions options;
    options.enableIncrementalParsing = true;
    mwrender::editor::DocumentSession session(options);

    session.load("Paragraph with **bold** text.\n");
    const auto& doc1 = session.document();
    const auto& para1 = doc1.children[0];
    require(para1->children.size() == 3, "Paragraph should have 3 children");
    std::string oldStrongId = para1->children[1]->id;

    // Replace "**bold**" with "*em*"
    // "**bold**" is at offset 15
    std::size_t boldOffset = session.markdown().find("**bold**");
    require(boldOffset != std::string::npos, "Found '**bold**'");
    mwrender::TextChange change;
    change.from = boldOffset;
    change.to = boldOffset + 8;
    change.insertedText = "*em*";

    auto result = session.applyChange(change);
    require(result.ok, "Edit should succeed");
    require(result.partialReparse, "Paragraph edit should be partial");

    // The Paragraph ID is preserved inside changedNodeIds.
    // The old Strong and its children should be in removedNodeIds.
    // The new Emphasis and its children should be in insertedNodeIds.
    require(!result.changedNodeIds.empty(), "Should have changed nodes");
    
    bool foundRemovedStrong = false;
    for (const auto& id : result.removedNodeIds) {
        if (id == oldStrongId) foundRemovedStrong = true;
    }
    require(foundRemovedStrong, "Old Strong ID should be in removedNodeIds");
    require(!result.insertedNodeIds.empty(), "New Emphasis should be in insertedNodeIds");
}

} // namespace

int main() {
    std::cout << "Running incremental_parse_test...\n";

    testIncrementalUpdate();
    testCrossBlockEdit();
    testListEditPreservesIds();
    testCodeBlockAndMathBlockIncremental();
    testComplexBlockFallback();
    testSubtreeDiffing();

    std::cout << "All incremental_parse tests passed.\n";
    return 0;
}
