#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/edit_command.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testLoad(mwrender::editor::DocumentSession& session) {
    const std::string markdown = "# Hello\n\nThis is a paragraph.";
    session.load(markdown);

    require(session.markdown() == markdown, "markdown() should return the loaded string");
    require(session.revision() == 1, "revision should be 1 after initial load");

    const mwrender::Node& doc = session.document();
    require(doc.type == mwrender::NodeType::Document, "root node must be Document");
    require(doc.children.size() == 2, "should have 2 children (Heading, Paragraph)");
}

void testApplyChange(mwrender::editor::DocumentSession& session) {
    session.load("# Hello\n\nThis is a paragraph.");
    
    // Change "Hello" to "Hello World"
    // offset 7 is after "Hello"
    mwrender::TextChange change;
    change.from = 7;
    change.to = 7;
    change.insertedText = " World";

    auto result = session.applyChange(change);

    require(result.ok, "applyChange should succeed");
    require(result.fullReparse == false, "Single-block edit should not trigger full reparse");
    require(result.revision == 2, "revision should increment");
    require(session.revision() == 2, "session revision should match");
    require(session.markdown() == "# Hello World\n\nThis is a paragraph.", "markdown should be updated");

    const mwrender::Node& doc = session.document();
    require(doc.children.size() == 2, "still 2 children");
}

void testFindNodeAtOffset(mwrender::editor::DocumentSession& session) {
    session.load("# Hello\n\nPara 2");

    // offset 0 is '#'
    const auto* node1 = session.findNodeAtOffset(0);
    require(node1 != nullptr, "should find node at offset 0");
    require(node1->type == mwrender::NodeType::Heading, "node at 0 is heading");

    // offset 10 is 'P'
    const auto* node2 = session.findNodeAtOffset(10);
    require(node2 != nullptr, "should find node at offset 10");
    require(node2->type == mwrender::NodeType::Text, "node at 10 is text inside paragraph");
}

void testFindNodeById(mwrender::editor::DocumentSession& session) {
    session.load("# Hello\n\nPara 2");
    
    // Engine generates IDs based on content by default in current stage.
    const mwrender::Node& doc = session.document();
    require(!doc.children.empty(), "document has children");
    
    const std::string& headingId = doc.children[0]->id;
    require(!headingId.empty(), "heading should have an ID");
    
    const mwrender::Node* found = session.findNodeById(headingId);
    require(found == doc.children[0].get(), "findNodeById should return the correct node pointer");
}

void testApplyCommandReplaceSelection(mwrender::editor::DocumentSession& session) {
    session.load("# Title\n\nParagraph 1\n");

    mwrender::editor::EditCommand cmd;
    cmd.type = mwrender::editor::EditCommandType::ReplaceSelection;
    cmd.selection.anchor.offset = 19;
    cmd.selection.focus.offset = 20;
    cmd.text = "2";

    auto result = session.applyCommand(cmd);
    require(result.ok, "applyCommand should succeed");
    require(result.fullReparse == false, "Single-block edit should not trigger full reparse");
    require(result.revision == 2, "revision should increment");
    require(session.markdown() == "# Title\n\nParagraph 2\n", "markdown should be updated");
}

void testListEditPreservesOtherIds(mwrender::editor::DocumentSession& session) {
    session.load("- item one\n- item two\n");

    const auto& doc1 = session.document();
    const auto& list = doc1.children[0];
    std::string oldListId = list->id;
    std::string oldItem0Id = list->children[0]->id;
    std::string oldItem1Id = list->children[1]->id;

    // Edit "one" → "ONE"
    std::size_t pos = session.markdown().find("one");
    mwrender::TextChange change;
    change.from = pos;
    change.to = pos + 3;
    change.insertedText = "ONE";

    auto result = session.applyChange(change);
    require(result.ok, "List edit should succeed");

    const auto& doc2 = session.document();
    const auto& list2 = doc2.children[0];
    require(list2->id == oldListId, "List ID preserved");
    require(list2->children[0]->id == oldItem0Id, "First item ID preserved");
    require(list2->children[1]->id == oldItem1Id, "Second item ID preserved");
}

} // namespace

int main() {
    std::cout << "Running document_session_test...\n";

    mwrender::editor::DocumentSessionOptions options;
    
    {
        mwrender::editor::DocumentSession session(options);
        testLoad(session);
    }
    {
        mwrender::editor::DocumentSession session(options);
        testApplyChange(session);
    }
    {
        mwrender::editor::DocumentSession session(options);
        testFindNodeAtOffset(session);
    }
    {
        mwrender::editor::DocumentSession session(options);
        testFindNodeById(session);
    }
    {
        mwrender::editor::DocumentSession session(options);
        testApplyCommandReplaceSelection(session);
    }
    {
        mwrender::editor::DocumentSession session(options);
        testListEditPreservesOtherIds(session);
    }

    std::cout << "All document_session tests passed.\n";
    return 0;
}
