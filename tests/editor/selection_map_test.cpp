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

void testSelectionMap() {
    mwrender::editor::DocumentSessionOptions options;
    mwrender::editor::DocumentSession session(options);
    session.load("# Title\n\nParagraph 1\n");

    mwrender::editor::SelectionMap selMap(session);

    const auto& doc = session.document();
    require(doc.children.size() == 2, "Document should have 2 children");

    auto titleId = doc.children[0]->id;

    mwrender::editor::VisualPosition visual;
    visual.nodeId = titleId;
    visual.textOffset = 2;

    mwrender::editor::SourcePositionEx source = selMap.visualToSource(visual);
    require(source.contextNodeId == titleId, "Context ID preserved");
    
    // The contentRange of "# Title\n" starts at offset 2 ('T'). 
    // So visual offset 2 relative to contentRange is offset 4 ('t').
    require(source.offset == 4, "Source offset should be 4");

    mwrender::editor::VisualPosition visualBack = selMap.sourceToVisual(source);
    require(visualBack.nodeId == titleId, "Node ID preserved");
    require(visualBack.textOffset == 2, "Visual offset preserved");
}

} // namespace

int main() {
    std::cout << "Running selection_map_test...\n";
    testSelectionMap();
    std::cout << "All selection_map tests passed.\n";
    return 0;
}
