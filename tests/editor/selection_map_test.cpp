#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/selection_map.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void roundtrip(const mwrender::editor::SelectionMap& selMap,
               std::string_view nodeId, std::size_t visualOffset,
               std::string_view label) {
    mwrender::editor::VisualPosition v;
    v.nodeId = std::string(nodeId);
    v.textOffset = visualOffset;

    mwrender::editor::SourcePositionEx s = selMap.visualToSource(v);
    require(s.contextNodeId == nodeId, (std::string(label) + ": contextNodeId preserved").c_str());

    mwrender::editor::VisualPosition v2 = selMap.sourceToVisual(s);
    require(v2.nodeId == nodeId, (std::string(label) + ": roundtrip nodeId preserved").c_str());
    require(v2.textOffset == visualOffset, (std::string(label) + ": roundtrip textOffset preserved").c_str());
}

// ===== Heading =====
void testHeadingMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph 1\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    std::string titleId = doc.children[0]->id;

    // visual offset 0 → source offset 2 (after "# ", at 'T')
    mwrender::editor::VisualPosition v;
    v.nodeId = titleId;
    v.textOffset = 0;
    auto s = selMap.visualToSource(v);
    require(s.offset == 2, "Heading visual offset 0 -> source 2");

    // visual offset 2 → source offset 4 ('t')
    roundtrip(selMap, titleId, 2, "Heading offset 2");
}

// ===== Paragraph Text =====
void testTextMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello world\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    // Paragraph > Text node
    const auto& para = doc.children[0];
    require(para->type == mwrender::NodeType::Paragraph, "first child is Paragraph");
    require(!para->children.empty(), "Paragraph has children");

    // The Text child may be wrapped or direct — find first Text node
    const mwrender::Node* textNode = nullptr;
    for (const auto& c : para->children) {
        if (c && (c->type == mwrender::NodeType::Text ||
                  c->type == mwrender::NodeType::SoftBreak)) {
            textNode = c.get();
            break;
        }
    }
    require(textNode != nullptr, "Found Text node in paragraph");
    require(textNode->type == mwrender::NodeType::Text, "child is Text");

    // visual offset 0 → source offset 0 ('H')
    roundtrip(selMap, textNode->id, 0, "Text offset 0");
    // visual offset 3 → source offset 3 ('l', since "Hello")
    roundtrip(selMap, textNode->id, 3, "Text offset 3");
}

// ===== Task List =====
void testTaskListMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("- [ ] task\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    // Find the Paragraph (or Text) inside the ListItem
    const auto& list = doc.children[0];
    require(list->type == mwrender::NodeType::List, "root child is List");
    const auto& item = list->children[0];
    require(item->type == mwrender::NodeType::ListItem, "first child is ListItem");

    // The visible text "task" starts at item->contentRange.begin.offset = 6
    // (offset 6 is 't', after "- [ ] ")
    mwrender::editor::VisualPosition v;
    v.nodeId = item->id;
    v.textOffset = 0;

    auto s = selMap.visualToSource(v);
    require(s.offset == 6, "Task list visual offset 0 -> source 6 (after '- [ ] ')");

    roundtrip(selMap, item->id, 2, "Task list offset 2");
}

// ===== Bold / Strong =====
void testStrongMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("**bold**\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    // Paragraph > Strong > Text
    const auto& para = doc.children[0];
    require(para->type == mwrender::NodeType::Paragraph, "root child is Paragraph");
    const auto& strong = para->children[0];
    require(strong->type == mwrender::NodeType::Strong, "first child is Strong");
    const auto& text = strong->children[0];
    require(text->type == mwrender::NodeType::Text, "child is Text");

    // The visible text "bold" — UI should target the Text child
    roundtrip(selMap, text->id, 0, "Strong text offset 0");
    roundtrip(selMap, text->id, 2, "Strong text offset 2");
}

// ===== Emphasis =====
void testEmphasisMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("*em*\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    const auto& para = doc.children[0];
    require(para->type == mwrender::NodeType::Paragraph, "root child is Paragraph");
    const auto& em = para->children[0];
    require(em->type == mwrender::NodeType::Emphasis, "first child is Emphasis");
    const auto& text = em->children[0];
    require(text->type == mwrender::NodeType::Text, "child is Text");

    roundtrip(selMap, text->id, 0, "Emphasis text offset 0");
    roundtrip(selMap, text->id, 1, "Emphasis text offset 1");
}

// ===== Inline Code =====
void testInlineCodeMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("`code`\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    const auto& para = doc.children[0];
    require(para->type == mwrender::NodeType::Paragraph, "root child is Paragraph");
    const auto& code = para->children[0];
    require(code->type == mwrender::NodeType::InlineCode, "first child is InlineCode");

    // InlineCode has literal directly, no Text child
    // visual offset within contentRange
    roundtrip(selMap, code->id, 0, "Code offset 0");
    roundtrip(selMap, code->id, 2, "Code offset 2");
}

// ===== Link =====
void testLinkMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("[text](url)\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    const auto& para = doc.children[0];
    require(para->type == mwrender::NodeType::Paragraph, "root child is Paragraph");
    const auto& link = para->children[0];
    require(link->type == mwrender::NodeType::Link, "first child is Link");
    const auto& text = link->children[0];
    require(text->type == mwrender::NodeType::Text, "child is Text");

    // UI should target Text child, not Link parent
    roundtrip(selMap, text->id, 0, "Link text offset 0");
    roundtrip(selMap, text->id, 2, "Link text offset 2");
}

// ===== Fallback: unknown nodeId =====
void testFallbackMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello\n");

    mwrender::editor::SelectionMap selMap(session);

    // Use a non-existent nodeId — should fall back gracefully
    mwrender::editor::VisualPosition v;
    v.nodeId = "nonexistent";
    v.textOffset = 3;

    auto s = selMap.visualToSource(v);
    // With fallback, offset should be textOffset (3)
    require(s.offset == 3, "Fallback: offset equals textOffset");
}

// ===== Unicode (CJK and Emojis) =====
void testUnicodeMapping() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("你好**世界**\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();

    // Paragraph > Strong > Text
    const auto& para = doc.children[0];
    require(para->children.size() == 2, "Paragraph should have 2 children");
    const auto& strong = para->children[1];
    require(strong->type == mwrender::NodeType::Strong, "second child is Strong");
    const auto& text = strong->children[0];
    require(text->type == mwrender::NodeType::Text, "child is Text");

    // CJK characters: "世界" in UTF-8 has 6 bytes.
    // roundtrip visual offset 0 of "世界"
    roundtrip(selMap, text->id, 0, "CJK text offset 0");
    // visual offset 3 of "世界" (after "世", 3 UTF-8 bytes)
    roundtrip(selMap, text->id, 3, "CJK text offset 3");
    // visual offset 6 of "世界" (after "世界", 6 UTF-8 bytes)
    roundtrip(selMap, text->id, 6, "CJK text offset 6");

    // Test with Emojis (surrogate pairs)
    session.load("hello 🌟 world\n");
    mwrender::editor::SelectionMap selMap2(session);
    const auto& doc2 = session.document();
    const auto& para2 = doc2.children[0];
    const auto& text2 = para2->children[0];

    // "hello 🌟 world"
    // "hello " has length 6 (6 UTF-8 bytes)
    // "🌟" has length 4 UTF-8 bytes
    // roundtrip visual offset 6 (at star)
    roundtrip(selMap2, text2->id, 6, "Emoji offset 6");
    // roundtrip visual offset 10 (after star, 6 + 4 = 10 UTF-8 bytes)
    roundtrip(selMap2, text2->id, 10, "Emoji offset 10");
}

} // namespace

int main() {
    std::cout << "Running selection_map_test...\n";

    testHeadingMapping();
    testTextMapping();
    testTaskListMapping();
    testStrongMapping();
    testEmphasisMapping();
    testInlineCodeMapping();
    testLinkMapping();
    testFallbackMapping();
    testUnicodeMapping();

    std::cout << "All selection_map tests passed.\n";
    return 0;
}
