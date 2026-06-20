#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/editor_projection.hpp>
#include <mwrender/editor/render_patch.hpp>
#include "../../src/support/json.hpp" // include custom json parser

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testGeneratePatch() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph 1\n");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);
    mwrender::editor::RenderPatchGenerator generator(projection);

    mwrender::TextChange change;
    change.from = 19;
    change.to = 20;
    change.insertedText = "2";

    auto result = session.applyChange(change);
    require(result.ok, "Update should succeed");

    mwrender::editor::Selection sel;
    sel.anchor.offset = 5;
    sel.focus.offset = 5;
    mwrender::editor::RenderPatch patch = generator.generatePatch(session.document(), result, sel);
    
    // Verify revision and selection are passed through
    require(patch.revision == result.revision, "Patch carries revision from UpdateResult");
    require(patch.selection.has_value(), "Patch carries selection");
    require(patch.selection->anchor.offset == 5, "Patch carries selection anchor");
    require(patch.selection->focus.offset == 5, "Patch carries selection focus");
    require(!patch.fullReload, "Should not be fullReload");
    require(patch.diagnostics.empty(), "Should have no diagnostics");
    
    // We expect exactly 1 changed node: the Paragraph
    require(!patch.changedNodes.empty(), "Should have changed nodes");
    require(patch.insertedNodes.empty(), "Should have no inserted nodes");
    require(patch.removedNodeIds.empty(), "Should have no removed nodes");
    
    auto& snippet = patch.changedNodes[0];
    require(!snippet.nodeId.empty(), "Snippet nodeId should not be empty");
    require(!snippet.html.empty(), "Snippet HTML should not be empty");
    require(snippet.html.find("Paragraph 2") != std::string::npos, "Should contain the new text");
    require(snippet.mode == mwrender::editor::ProjectionMode::Editable, "Paragraph should be Editable");
    require(snippet.sourceRange.begin.offset > 0, "sourceRange begin should be set");
    require(snippet.contentRange.begin.offset > 0, "contentRange begin should be set");
}

void testJsonSerialization() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph 1\n");

    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);
    mwrender::editor::RenderPatchGenerator generator(projection);

    mwrender::TextChange change;
    change.from = 19;
    change.to = 20;
    change.insertedText = "2";

    auto result = session.applyChange(change);
    mwrender::editor::Selection sel;
    sel.anchor.offset = 5;
    sel.anchor.affinity = mwrender::editor::Affinity::After;
    sel.anchor.contextNodeId = "ctx1";
    sel.focus.offset = 5;
    sel.focus.affinity = mwrender::editor::Affinity::After;
    sel.focus.contextNodeId = "ctx1";

    mwrender::editor::RenderPatch patch = generator.generatePatch(session.document(), result, sel);
    
    std::string jsonStr = patch.toJson();
    auto parseRes = mwrender::detail::parseJson(jsonStr);
    require(parseRes.value.has_value(), "JSON should be parseable");
    
    const auto* rootObj = parseRes.value->asObject();
    require(rootObj != nullptr, "JSON root should be an object");
    
    // Verify top-level fields
    const auto* revVal = rootObj->at("revision").asNumber();
    require(revVal != nullptr && *revVal == static_cast<double>(result.revision), "revision matches");
    
    const auto* fullReloadVal = rootObj->at("fullReload").asBool();
    require(fullReloadVal != nullptr && !*fullReloadVal, "fullReload matches");

    const auto* changedArr = rootObj->at("changedNodes").asArray();
    require(changedArr != nullptr && changedArr->size() == 1, "changedNodes should contain 1 element");

    const auto* snippetObj = (*changedArr)[0].asObject();
    require(snippetObj != nullptr, "snippet should be an object");
    
    const auto* nodeIdVal = snippetObj->at("nodeId").asString();
    require(nodeIdVal != nullptr && !nodeIdVal->empty(), "nodeId matches");

    const auto* modeVal = snippetObj->at("mode").asString();
    require(modeVal != nullptr && *modeVal == "editable", "mode matches");

    const auto* sourceStartVal = snippetObj->at("sourceStart").asNumber();
    require(sourceStartVal != nullptr, "sourceStart is number");

    const auto* selectionObj = rootObj->at("selection").asObject();
    require(selectionObj != nullptr, "selection is object");
    
    const auto* anchorObj = selectionObj->at("anchor").asObject();
    require(anchorObj != nullptr, "anchor is object");
    
    const auto* anchorOffset = anchorObj->at("offset").asNumber();
    require(anchorOffset != nullptr && *anchorOffset == 5.0, "anchor offset is 5");

    const auto* anchorAff = anchorObj->at("affinity").asString();
    require(anchorAff != nullptr && *anchorAff == "after", "anchor affinity is after");

    const auto* anchorCtx = anchorObj->at("contextNodeId").asString();
    require(anchorCtx != nullptr && *anchorCtx == "ctx1", "anchor contextNodeId matches");
}

} // namespace

int main() {
    std::cout << "Running render_patch_test...\n";
    testGeneratePatch();
    testJsonSerialization();
    std::cout << "All render_patch tests passed.\n";
    return 0;
}
