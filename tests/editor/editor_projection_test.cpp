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
    require(html.find("data-projection-mode=\"editable\"") != std::string::npos, "Heading should be editable");
}

void testClassify() {
    mwrender::Engine engine;

    auto para = engine.parse("hello\n");
    require(mwrender::editor::EditorProjection::classifyNode(*para.document->children[0])
            == mwrender::editor::ProjectionMode::Editable, "Paragraph is Editable");

    auto bold = engine.parse("**bold**\n");
    const auto& strong = bold.document->children[0]->children[0];
    require(strong->type == mwrender::NodeType::Strong, "inner node is Strong");
    require(mwrender::editor::EditorProjection::classifyNode(*strong)
            == mwrender::editor::ProjectionMode::SourceEditable, "Strong is SourceEditable");

    auto code = engine.parse("```\ncode\n```\n");
    require(mwrender::editor::EditorProjection::classifyNode(*code.document->children[0])
            == mwrender::editor::ProjectionMode::Atomic, "CodeBlock is Atomic");
}

void testProjectionOutputEditability() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph\n");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);
    const auto& doc = session.document();

    // Heading is block-level — gets data-projection-mode
    std::string html = projection.projectNode(doc, doc.children[0]->id);
    require(html.find("data-projection-mode=\"editable\"") != std::string::npos,
            "Heading should have data-projection-mode=editable");

    // Paragraph is block-level — gets data-projection-mode
    html = projection.projectNode(doc, doc.children[1]->id);
    require(html.find("data-projection-mode=\"editable\"") != std::string::npos,
            "Paragraph should have data-projection-mode=editable");
}

void testProjectBlock() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    std::string markdown = "# Title\n\nParagraph with **bold** text.";
    session.load(markdown);

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);

    const auto& doc = session.document();
    require(doc.children.size() == 2, "Document should have 2 children");

    std::string targetId = doc.children[1]->id;
    mwrender::editor::BlockProjection proj = projection.projectBlock(doc, targetId, markdown);

    require(!proj.renderedHtml.empty(), "renderedHtml should not be empty");
    require(proj.renderedHtml.find("Paragraph with") != std::string::npos, "renderedHtml should contain text");
    require(proj.sourceText == "Paragraph with **bold** text.", "sourceText should match the markdown segment exactly");
    require(proj.sourceStart == 9, "sourceStart should match the beginning offset of Paragraph");
    require(proj.sourceEnd == 38, "sourceEnd should match the end offset of Paragraph");
    require(proj.mode == mwrender::editor::ProjectionMode::Editable, "Paragraph mode should be Editable");
}

void testCheckboxInteractiveInEditorView() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("- [ ] task\n");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);

    const auto& doc = session.document();
    require(doc.children.size() == 1, "Document has 1 child");
    const auto& list = doc.children[0];
    require(list->children.size() == 1, "List has 1 child");
    const auto& item = list->children[0];

    std::string html = projection.projectNode(doc, item->id);
    require(html.find("type=\"checkbox\"") != std::string::npos, "Should contain checkbox input");
    require(html.find("disabled") == std::string::npos, "Checkbox should NOT be disabled in EditorView mode");
}

} // namespace

int main() {
    std::cout << "Running editor_projection_test...\n";

    testProjectNode();
    testClassify();
    testProjectionOutputEditability();
    testProjectBlock();
    testCheckboxInteractiveInEditorView();

    std::cout << "All editor_projection tests passed.\n";
    return 0;
}
