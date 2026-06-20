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

void verifyAtomicBlock(const std::string& markdown, mwrender::NodeType expectedType, std::string_view label) {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load(markdown);

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);

    const auto& doc = session.document();
    require(!doc.id.empty(), "Document should have ID");
    
    // Find the node of the expected type
    const mwrender::Node* targetNode = nullptr;
    auto dfs = [&](const mwrender::Node& n, auto& ref) -> void {
        if (n.type == expectedType) {
            targetNode = &n;
            return;
        }
        for (const auto& child : n.children) {
            if (child) ref(*child, ref);
        }
    };
    dfs(doc, dfs);

    require(targetNode != nullptr, (std::string(label) + ": target node not found").c_str());
    require(!targetNode->id.empty(), (std::string(label) + ": node should have ID").c_str());
    require(!targetNode->contentHash.empty(), (std::string(label) + ": node should have contentHash").c_str());

    std::string html = projection.projectNode(doc, targetNode->id);
    
    require(html.find("data-node-id=\"" + targetNode->id + "\"") != std::string::npos,
            (std::string(label) + ": Should contain data-node-id").c_str());
    require(html.find("data-projection-mode=\"atomic\"") != std::string::npos,
            (std::string(label) + ": Should contain data-projection-mode=\"atomic\"").c_str());
    require(html.find("data-content-hash=\"" + targetNode->contentHash + "\"") != std::string::npos,
            (std::string(label) + ": Should contain data-content-hash").c_str());
    require(html.find("data-source-start=") != std::string::npos,
            (std::string(label) + ": Should contain data-source-start").c_str());
    require(html.find("data-source-end=") != std::string::npos,
            (std::string(label) + ": Should contain data-source-end").c_str());
}

void testCodeBlock() {
    verifyAtomicBlock("```cpp\nint x = 1;\n```\n", mwrender::NodeType::CodeBlock, "CodeBlock");
}

void testMathBlock() {
    verifyAtomicBlock("$$\ne = mc^2\n$$\n", mwrender::NodeType::MathBlock, "MathBlock");
}

void testMermaid() {
    verifyAtomicBlock("```mermaid\ngraph TD\n  A --> B\n```\n", mwrender::NodeType::CodeBlock, "Mermaid");
}

void testTable() {
    verifyAtomicBlock("| Col 1 | Col 2 |\n|---|---|\n| Val 1 | Val 2 |\n", mwrender::NodeType::Table, "Table");
}

void testHtmlBlock() {
    verifyAtomicBlock("<div>\n  <p>Hello</p>\n</div>\n", mwrender::NodeType::HtmlBlock, "HtmlBlock");
}

void testFootnoteDef() {
    // FootnoteDef is rendered within the footnote section, so we need to project the Document
    // and verify its list item has the required attributes.
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Text[^1]\n\n[^1]: Footnote definition\n");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);

    const auto& doc = session.document();
    
    const mwrender::Node* fnDef = nullptr;
    auto dfs = [&](const mwrender::Node& n, auto& ref) -> void {
        if (n.type == mwrender::NodeType::FootnoteDef) {
            fnDef = &n;
            return;
        }
        for (const auto& child : n.children) {
            if (child) ref(*child, ref);
        }
    };
    dfs(doc, dfs);

    require(fnDef != nullptr, "FootnoteDef node not found");
    require(!fnDef->id.empty(), "FootnoteDef node should have ID");
    require(!fnDef->contentHash.empty(), "FootnoteDef node should have contentHash");

    // Project the whole document
    std::string html = projection.projectDocument(doc);
    
    // Check that the footnote <li> has the attributes
    require(html.find("id=\"fn-1\"") != std::string::npos, "Footnotes list item should have id=\"fn-1\"");
    require(html.find("data-node-id=\"" + fnDef->id + "\"") != std::string::npos, "Footnotes list item should have data-node-id");
    require(html.find("data-projection-mode=\"atomic\"") != std::string::npos, "Footnotes list item should have data-projection-mode=\"atomic\"");
    require(html.find("data-content-hash=\"" + fnDef->contentHash + "\"") != std::string::npos, "Footnotes list item should have data-content-hash");
}

} // namespace

int main() {
    std::cout << "Running atomic_blocks_test...\n";

    testCodeBlock();
    testMathBlock();
    testMermaid();
    testTable();
    testHtmlBlock();
    testFootnoteDef();

    std::cout << "All atomic_blocks tests passed.\n";
    return 0;
}
