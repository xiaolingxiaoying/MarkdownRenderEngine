#include <chrono>
#include <iostream>
#include <string>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/edit_command.hpp>
#include <mwrender/editor/render_patch.hpp>

namespace {

void runBenchmark() {
    std::string largeMarkdown = "# Large Document\n\n";
    for (int i = 0; i < 10000; ++i) {
        largeMarkdown += "This is paragraph " + std::to_string(i) + " with some **bold** and *emphasis* text.\n\n";
    }

    mwrender::editor::DocumentSession session;
    
    // 1. Initial Parse Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    session.load(largeMarkdown);
    auto end = std::chrono::high_resolution_clock::now();
    auto initialParseMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Initial Parse (10000 paragraphs): " << initialParseMs << " ms\n";
    
    // 2. Incremental Edit Benchmark (Append)
    std::string appendChange = "Appended paragraph at the end.\n\n";
    start = std::chrono::high_resolution_clock::now();
    mwrender::TextChange change;
    change.insertedText = appendChange;
    change.from = largeMarkdown.size();
    change.to = largeMarkdown.size();
    auto parseResult = session.applyChange(change);
    end = std::chrono::high_resolution_clock::now();
    auto incrementalAppendMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Incremental Parse (Append): " << incrementalAppendMs << " ms\n";
    
    // 3. Render Patch Benchmark
    mwrender::Engine engine;
    mwrender::editor::EditorProjection projection(engine);
    mwrender::editor::RenderPatchGenerator patchGen(projection);
    
    start = std::chrono::high_resolution_clock::now();
    auto patch = patchGen.generatePatch(session.document(), parseResult);
    end = std::chrono::high_resolution_clock::now();
    auto renderPatchMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Render Patch Generation: " << renderPatchMs << " ms\n";
    std::cout << "Patch Ops: inserted=" << patch.insertedNodes.size() << " changed=" << patch.changedNodes.size() << " removed=" << patch.removedNodeIds.size() << "\n";
}

} // namespace

int main() {
    std::cout << "Running Editor Benchmarks...\n";
    runBenchmark();
    std::cout << "Benchmarks completed.\n";
    return 0;
}
