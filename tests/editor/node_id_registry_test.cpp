#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <memory>

#include <mwrender/editor/node_id_registry.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testAllocate() {
    mwrender::editor::NodeIdRegistry registry;
    std::string id1 = registry.allocate(mwrender::NodeType::Paragraph);
    std::string id2 = registry.allocate(mwrender::NodeType::Heading);
    require(!id1.empty() && !id2.empty() && id1 != id2, "allocate generates unique IDs");
}

void testInheritIds() {
    mwrender::editor::NodeIdRegistry registry;
    
    mwrender::Node oldDoc;
    oldDoc.id = "doc1";
    oldDoc.type = mwrender::NodeType::Document;
    
    auto oldPara1 = std::make_unique<mwrender::Node>();
    oldPara1->id = "p1";
    oldPara1->type = mwrender::NodeType::Paragraph;
    oldPara1->contentHash = "hash1";
    
    auto oldPara2 = std::make_unique<mwrender::Node>();
    oldPara2->id = "p2";
    oldPara2->type = mwrender::NodeType::Paragraph;
    oldPara2->contentHash = "hash2";

    oldDoc.children.push_back(std::move(oldPara1));
    oldDoc.children.push_back(std::move(oldPara2));
    
    mwrender::Node newDoc;
    newDoc.type = mwrender::NodeType::Document;
    
    auto newPara1 = std::make_unique<mwrender::Node>();
    newPara1->type = mwrender::NodeType::Paragraph;
    newPara1->contentHash = "hash1"; // Exact match
    
    auto newPara2 = std::make_unique<mwrender::Node>();
    newPara2->type = mwrender::NodeType::Paragraph;
    newPara2->contentHash = "hash3"; // Modified (same type, different hash)
    
    auto newPara3 = std::make_unique<mwrender::Node>();
    newPara3->type = mwrender::NodeType::Paragraph;
    newPara3->contentHash = "hash4"; // Inserted
    
    newDoc.children.push_back(std::move(newPara1));
    newDoc.children.push_back(std::move(newPara2));
    newDoc.children.push_back(std::move(newPara3));
    
    registry.inheritIds(oldDoc, newDoc);
    
    require(newDoc.id == "doc1", "Root inherits ID");
    require(newDoc.children[0]->id == "p1", "Exact match inherits ID p1");
    require(newDoc.children[1]->id == "p2", "Modified node inherits ID p2 by type sequence");
    require(!newDoc.children[2]->id.empty() && newDoc.children[2]->id != "p1" && newDoc.children[2]->id != "p2", "Inserted node gets new ID");
}

} // namespace

int main() {
    std::cout << "Running node_id_registry_test...\n";
    testAllocate();
    testInheritIds();
    std::cout << "All node_id_registry tests passed.\n";
    return 0;
}
