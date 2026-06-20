#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/block_index.hpp>
#include <mwrender/engine.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testBlockIndexBuilding(mwrender::Engine& engine) {
    const std::string markdown = "# Heading\n\nPara 1\n\n```\nCode\n```";
    auto parseResult = engine.parse(markdown);
    require(parseResult.ok && parseResult.document != nullptr, "Parsing should succeed");

    mwrender::editor::BlockIndex index;
    index.rebuild(*parseResult.document);

    const auto& blocks = index.blocks();
    // Document, Heading, Paragraph, CodeBlock
    require(blocks.size() == 4, "Should index 4 blocks");

    require(blocks[0].type == mwrender::NodeType::Document, "Block 0 is Document");
    require(blocks[1].type == mwrender::NodeType::Heading, "Block 1 is Heading");
    require(blocks[2].type == mwrender::NodeType::Paragraph, "Block 2 is Paragraph");
    require(blocks[3].type == mwrender::NodeType::CodeBlock, "Block 3 is CodeBlock");
}

void testBlockAtOffset(mwrender::Engine& engine) {
    const std::string markdown = "# Heading\n\nPara 1\n\n```\nCode\n```";
    auto parseResult = engine.parse(markdown);

    mwrender::editor::BlockIndex index;
    index.rebuild(*parseResult.document);

    // offset 0 is in heading
    const auto* b1 = index.blockAtOffset(0);
    require(b1 != nullptr && b1->type == mwrender::NodeType::Heading, "Offset 0 is Heading");

    // offset 12 is in paragraph
    const auto* b2 = index.blockAtOffset(12);
    require(b2 != nullptr && b2->type == mwrender::NodeType::Paragraph, "Offset 12 is Paragraph");

    // offset 25 is in code block
    const auto* b3 = index.blockAtOffset(25);
    require(b3 != nullptr && b3->type == mwrender::NodeType::CodeBlock, "Offset 25 is CodeBlock");
}

void testAffectedRangeForChange(mwrender::Engine& engine) {
    const std::string markdown = "# Heading\n\nPara 1\n\n- list item\n";
    auto parseResult = engine.parse(markdown);

    mwrender::editor::BlockIndex index;
    index.rebuild(*parseResult.document);

    // Change inside Paragraph
    mwrender::TextChange changePara;
    changePara.from = 13;
    changePara.to = 14;
    auto rangePara = index.affectedRangeForChange(changePara);
    const auto* paraBlock = index.blockAtOffset(13);
    require(paraBlock != nullptr, "paraBlock found");
    require(rangePara == paraBlock->range, "Change in paragraph affects only paragraph");

    // Change inside ListItem — returns parent List range
    mwrender::TextChange changeList;
    changeList.from = 22;
    changeList.to = 22;
    auto rangeList = index.affectedRangeForChange(changeList);
    // Should no longer fall back to document range: ListItem returns parent List range
    require(rangeList.begin.offset > 0, "Change in list item returns parent List range, not document");
}

void testAffectedRangeForChangeInList(mwrender::Engine& engine) {
    const std::string markdown = "- item one\n- item two\n";
    auto parseResult = engine.parse(markdown);

    mwrender::editor::BlockIndex index;
    index.rebuild(*parseResult.document);

    // Find the List block (not ListItem) by scanning types
    const mwrender::editor::BlockEntry* listBlock = nullptr;
    for (const auto& b : index.blocks()) {
        if (b.type == mwrender::NodeType::List) {
            listBlock = &b;
            break;
        }
    }
    require(listBlock != nullptr, "List block found");

    // Verify ListItem exists
    const auto* item2 = index.blockAtOffset(12);
    require(item2 != nullptr, "Block found at offset 12");
    require(item2->type == mwrender::NodeType::ListItem, "Block at offset 12 is ListItem");

    // Change inside second item
    mwrender::TextChange change;
    change.from = 12;
    change.to = 12;
    auto range = index.affectedRangeForChange(change);
    // ListItem returns parent List range
    require(range.begin.offset == listBlock->range.begin.offset,
            "ListItem change returns parent List range start");
    require(range.end.offset == listBlock->range.end.offset,
            "ListItem change returns parent List range end");
}

void testBlockQuoteAffectedRange(mwrender::Engine& engine) {
    const std::string markdown = "> quoted text\n\n> second quote\n";
    auto parseResult = engine.parse(markdown);

    mwrender::editor::BlockIndex index;
    index.rebuild(*parseResult.document);

    // Find the BlockQuote block
    const mwrender::editor::BlockEntry* quoteBlock = nullptr;
    for (const auto& b : index.blocks()) {
        if (b.type == mwrender::NodeType::BlockQuote) {
            quoteBlock = &b;
            break;
        }
    }
    require(quoteBlock != nullptr, "BlockQuote found");
    require(quoteBlock->range.begin.offset < quoteBlock->range.end.offset,
            "BlockQuote has valid range");
}

} // namespace

int main() {
    std::cout << "Running block_index_test...\n";

    mwrender::EngineOptions options;
    mwrender::Engine engine(options);

    testBlockIndexBuilding(engine);
    testBlockAtOffset(engine);
    testAffectedRangeForChange(engine);
    testAffectedRangeForChangeInList(engine);
    testBlockQuoteAffectedRange(engine);

    std::cout << "All block_index tests passed.\n";
    return 0;
}
