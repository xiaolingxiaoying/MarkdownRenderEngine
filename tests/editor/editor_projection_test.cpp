#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/editor_projection.hpp>
#include <mwrender/editor/document_session.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testProjectNode() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph with **bold** text.");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);

    const auto& doc = session.document();
    require(!doc.id.empty(), "Document should have ID");
    require(doc.children.size() == 2, "Document should have 2 children");

    std::string html = projection.projectNode(doc, doc.children[0]->id);
    require(html.find("data-node-id=\"" + doc.children[0]->id + "\"") != std::string::npos, "Should contain data-node-id");
    require(html.find("data-node-type=\"heading\"") != std::string::npos, "Should contain data-node-type");
    require(html.find("data-source-start=\"0\"") != std::string::npos, "Should contain data-source-start");
}

} // namespace

int main() {
    std::cout << "Running editor_projection_test...\n";
    testProjectNode();
    std::cout << "All editor_projection tests passed.\n";
    return 0;
}
