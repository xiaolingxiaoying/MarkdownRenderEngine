#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/editor_projection.hpp>
#include <mwrender/editor/document_session.hpp>
#include <mwrender/parser.hpp>

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
    require(html.find("data-editable=\"editable\"") != std::string::npos, "Heading should be editable");
}

void testClassify() {
    mwrender::Engine engine;

    auto para = engine.parse("hello\n");
    require(mwrender::editor::EditorProjection::classifyNode(*para.document->children[0])
            == mwrender::editor::Editability::Editable, "Paragraph is Editable");

    auto bold = engine.parse("**bold**\n");
    const auto& strong = bold.document->children[0]->children[0];
    require(strong->type == mwrender::NodeType::Strong, "inner node is Strong");
    require(mwrender::editor::EditorProjection::classifyNode(*strong)
            == mwrender::editor::Editability::SourceEditable, "Strong is SourceEditable");

    auto code = engine.parse("```\ncode\n```\n");
    require(mwrender::editor::EditorProjection::classifyNode(*code.document->children[0])
            == mwrender::editor::Editability::Atomic, "CodeBlock is Atomic");
}

void testProjectionOutputEditability() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph\n");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);
    const auto& doc = session.document();

    // Heading is block-level — gets data-editable
    std::string html = projection.projectNode(doc, doc.children[0]->id);
    require(html.find("data-editable=\"editable\"") != std::string::npos,
            "Heading should have data-editable=editable");

    // Paragraph is block-level — gets data-editable
    html = projection.projectNode(doc, doc.children[1]->id);
    require(html.find("data-editable=\"editable\"") != std::string::npos,
            "Paragraph should have data-editable=editable");
}

} // namespace

int main() {
    std::cout << "Running editor_projection_test...\n";

    testProjectNode();
    testClassify();
    testProjectionOutputEditability();

    std::cout << "All editor_projection tests passed.\n";
    return 0;
}
