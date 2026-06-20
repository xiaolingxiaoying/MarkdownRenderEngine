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

std::string makeLargeMarkdown(std::size_t targetBytes) {
    std::string md = "# Large Document\n\n";
    int i = 0;
    while (md.size() < targetBytes) {
        md += "This is paragraph " + std::to_string(i++) +
              " with some **bold** and *emphasis* text. "
              "It also has inline `code` and a [link](https://example.com).\n\n";
    }
    return md;
}

// ===== Initial parse of ~1MB document =====
void testLargeDocumentInitialParse() {
    std::string largeMd = makeLargeMarkdown(1'000'000);
    require(largeMd.size() >= 900'000, "Generated markdown is large enough");

    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);

    session.load(largeMd);
    require(session.revision() == 1, "Revision incremented after load");

    const auto& doc = session.document();
    require(doc.children.size() > 10, "Large document has many children");
    std::cout << "  Large doc: " << doc.children.size() << " children, "
              << largeMd.size() << " bytes\n";
}

// ===== Single paragraph edit in large document =====
void testLargeDocumentSingleEdit() {
    std::string largeMd = makeLargeMarkdown(1'000'000);
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load(largeMd);

    // Edit a character in the middle paragraph
    // Offset of ~500KB
    std::size_t editOffset = 500'000;
    if (editOffset >= largeMd.size()) {
        editOffset = largeMd.size() / 2;
    }

    mwrender::TextChange change;
    change.from = editOffset;
    change.to = editOffset + 1;
    change.insertedText = "X";

    auto result = session.applyChange(change);
    require(result.ok, "Large doc edit should succeed");

    // Phase 1: single-block edit should return only 1 changed node
    require(!result.changedNodeIds.empty(), "Should have changed nodes");
    require(result.insertedNodeIds.empty(), "Should have no inserted nodes");
    require(result.removedNodeIds.empty(), "Should have no removed nodes");

    std::cout << "  Large doc edit: changedNodeIds=" << result.changedNodeIds.size() << "\n";
}

// ===== Append to large document =====
void testLargeDocumentAppend() {
    std::string largeMd = makeLargeMarkdown(1'000'000);
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load(largeMd);

    mwrender::TextChange change;
    change.from = largeMd.size();
    change.to = largeMd.size();
    change.insertedText = "Appended paragraph at the end.\n\n";

    auto result = session.applyChange(change);
    require(result.ok, "Large doc append should succeed");
    require(session.markdown().find("Appended paragraph") != std::string::npos,
            "Markdown should contain appended text");

    std::cout << "  Large doc append: changed=" << result.changedNodeIds.size()
              << " inserted=" << result.insertedNodeIds.size() << "\n";
}

} // namespace

int main() {
    std::cout << "Running large_document_test...\n";

    testLargeDocumentInitialParse();
    testLargeDocumentSingleEdit();
    testLargeDocumentAppend();

    std::cout << "All large document tests passed.\n";
    return 0;
}
