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
    require(result.fullReparse == true, "Stage 1 expects full reparse");
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

    std::cout << "All document_session tests passed.\n";
    return 0;
}
