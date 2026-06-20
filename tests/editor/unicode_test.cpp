#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/engine.hpp>
#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/selection_map.hpp>
#include <mwrender/editor/edit_command.hpp>
#include <mwrender/utf.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

// ===== Chinese text round-trip through SelectionMap =====
void testChineseTextRoundtrip() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    // Chinese: 你好世界 = 12 UTF-8 bytes (3 bytes per char)
    session.load("你好世界\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();
    require(doc.children.size() == 1, "Document has 1 paragraph");

    // Find the Text node
    const auto& para = doc.children[0];
    const mwrender::Node* textNode = nullptr;
    for (const auto& c : para->children) {
        if (c && c->type == mwrender::NodeType::Text) {
            textNode = c.get();
            break;
        }
    }
    require(textNode != nullptr, "Found Text node");

    // Visual offset 2 (third character: 世) → source offset 6 (3×2)
    mwrender::editor::VisualPosition v;
    v.nodeId = textNode->id;
    v.textOffset = 6; // offset in bytes, "你"=3, "好"=3
    auto s = selMap.visualToSource(v);
    require(s.offset > 0, "Got source offset");

    mwrender::editor::VisualPosition v2 = selMap.sourceToVisual(s);
    require(v2.textOffset == 6, "Round-trip preserves Chinese byte offset");
}

// ===== Emoji offset handling =====
void testEmojiOffset() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    // "a😀b" — 😀 is 4 bytes in UTF-8 (F0 9F 98 80)
    session.load("a😀b\n");

    mwrender::editor::SelectionMap selMap(session);
    const auto& doc = session.document();
    const auto& para = doc.children[0];

    const mwrender::Node* textNode = nullptr;
    for (const auto& c : para->children) {
        if (c && c->type == mwrender::NodeType::Text) {
            textNode = c.get();
            break;
        }
    }
    require(textNode != nullptr, "Found Text node");

    // 'b' is at source offset 5 (a=1 + 😀=4)
    mwrender::editor::VisualPosition v;
    v.nodeId = textNode->id;
    v.textOffset = 5;
    auto s = selMap.visualToSource(v);
    require(s.offset >= 5, "Emoji: visual offset 5 maps to source offset >=5");

    mwrender::editor::VisualPosition v2 = selMap.sourceToVisual(s);
    require(v2.textOffset == 5, "Emoji round-trip preserves offset");
}

// ===== Chinese text edit via EditCommand =====
void testChineseEditCommand() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("你好世界\n");

    mwrender::editor::EditCommand cmd;
    cmd.type = mwrender::editor::EditCommandType::ReplaceSelection;
    // "你"=3, "好"=3, "世"=3, "界"=3
    // Replace "世" (bytes 6-8) with "吗"
    cmd.selection.anchor.offset = 6;
    cmd.selection.focus.offset = 9;
    cmd.text = "吗";

    auto result = session.applyCommand(cmd);
    require(result.ok, "Chinese ReplaceSelection should succeed");
    require(session.markdown().find("你好吗") != std::string::npos,
            "Markdown should contain '你好吗'");
}

// ===== EditCommand with Chinese text insertion =====
void testChineseInsertText() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("Hello \n");

    mwrender::editor::EditCommand cmd;
    cmd.type = mwrender::editor::EditCommandType::ReplaceSelection;
    cmd.selection.anchor.offset = 6;
    cmd.selection.focus.offset = 6;
    cmd.text = "世界";

    auto result = session.applyCommand(cmd);
    require(result.ok, "Chinese Insert should succeed");
    require(session.markdown() == "Hello 世界\n", "Markdown should contain Chinese text");
}

// ===== UTF-8 / UTF-16 offset conversion =====
void testUtf8ToUtf16() {
    // "a😀b" — 'a'=1 byte, 😀=4 bytes, 'b'=1 byte
    std::string_view s = "a😀b";
    require(mwrender::utf8ToUtf16Offset(s, 0) == 0, "UTF-16 offset 0 at byte 0");
    require(mwrender::utf8ToUtf16Offset(s, 1) == 1, "UTF-16 offset 1 at byte 1 ('a')");
    // 😀 is 4 UTF-8 bytes → 2 UTF-16 code units (surrogate pair)
    require(mwrender::utf8ToUtf16Offset(s, 5) == 3, "UTF-16 offset 3 at byte 5 ('b')");
}

void testUtf16ToUtf8() {
    std::string_view s = "a😀b";
    require(mwrender::utf16ToUtf8Offset(s, 0) == 0, "Byte offset 0 at UTF-16 offset 0");
    require(mwrender::utf16ToUtf8Offset(s, 1) == 1, "Byte offset 1 at UTF-16 offset 1");
    // 😀 takes 2 UTF-16 code units → 4 UTF-8 bytes
    require(mwrender::utf16ToUtf8Offset(s, 3) == 5, "Byte offset 5 at UTF-16 offset 3");
}

void testUtf8Utf16Roundtrip() {
    // Mixed ASCII + CJK + emoji
    std::string_view s = "Hello 你好世界😀!";
    // Only test at character boundaries (offsets that start a new character)
    std::size_t boundaries[] = {0, 1, 2, 3, 4, 5, 6, 9, 12, 15, 18, 22, 23};
    for (auto byteOff : boundaries) {
        if (byteOff > s.size()) continue;
        std::size_t utf16Off = mwrender::utf8ToUtf16Offset(s, byteOff);
        std::size_t backToByte = mwrender::utf16ToUtf8Offset(s, utf16Off);
        require(backToByte == byteOff, "UTF-8 ↔ UTF-16 roundtrip preserves offset at boundary");
    }
}

} // namespace

int main() {
    std::cout << "Running unicode_test...\n";

    testChineseTextRoundtrip();
    testEmojiOffset();
    testChineseEditCommand();
    testChineseInsertText();
    testUtf8ToUtf16();
    testUtf16ToUtf8();
    testUtf8Utf16Roundtrip();

    std::cout << "All unicode tests passed.\n";
    return 0;
}
