#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <random>

#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/edit_command.hpp>
#include <mwrender/editor/render_patch.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

// Generate a mixed 1MB document
std::string generate1MBMixedDocument() {
    std::string md;
    md.reserve(1'050'000);
    
    // Elements to mix
    std::vector<std::string> paragraphs = {
        "This is a standard paragraph containing some **bold text** and *italic text* to test inline nodes.\n\n",
        "Another paragraph with inline `code` and a [link to example](https://example.com) for selection mapping.\n\n",
        "Short paragraph for fast-path incremental updates.\n\n",
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.\n\n"
    };

    std::vector<std::string> headings = {
        "# Heading Level 1\n\n",
        "## Heading Level 2\n\n",
        "### Heading Level 3\n\n",
        "#### Heading Level 4\n\n"
    };

    std::vector<std::string> codeBlocks = {
        "```cpp\nint main() {\n    std::cout << \"Hello World!\" << std::endl;\n    return 0;\n}\n```\n\n",
        "```javascript\nfunction greet(name) {\n    console.log(`Hello, ${name}!`);\n}\n```\n\n"
    };

    std::vector<std::string> mathBlocks = {
        "$$\ne = mc^2\n$$\n\n",
        "$$\n\\sum_{i=1}^{n} i = \\frac{n(n+1)}{2}\n$$\n\n"
    };

    std::vector<std::string> tables = {
        "| Name | Age | Job |\n|---|---|---|\n| Alice | 30 | Engineer |\n| Bob | 25 | Designer |\n\n"
    };

    std::mt19937 rng(42); // deterministic seed
    
    int paragraphsCount = 0;
    int headingsCount = 0;
    int codeBlocksCount = 0;
    int mathBlocksCount = 0;
    int tablesCount = 0;

    while (md.size() < 1'000'000) {
        int r = rng() % 100;
        if (r < 60) {
            md += paragraphs[rng() % paragraphs.size()];
            paragraphsCount++;
        } else if (r < 75) {
            md += headings[rng() % headings.size()];
            headingsCount++;
        } else if (r < 85) {
            md += codeBlocks[rng() % codeBlocks.size()];
            codeBlocksCount++;
        } else if (r < 92) {
            md += mathBlocks[rng() % mathBlocks.size()];
            mathBlocksCount++;
        } else {
            md += tables[rng() % tables.size()];
            tablesCount++;
        }
    }

    std::cout << "Generated mixed document: size = " << md.size() << " bytes\n"
              << "  - Paragraphs: " << paragraphsCount << "\n"
              << "  - Headings: " << headingsCount << "\n"
              << "  - CodeBlocks: " << codeBlocksCount << "\n"
              << "  - MathBlocks: " << mathBlocksCount << "\n"
              << "  - Tables: " << tablesCount << "\n";
              
    return md;
}

void checkNodeIdUniqueness(const mwrender::Node& root) {
    std::unordered_set<std::string> ids;
    auto dfs = [&](const mwrender::Node& n, auto& ref) -> void {
        if (!n.id.empty()) {
            require(ids.insert(n.id).second, ("Duplicate node ID found: " + n.id).c_str());
        }
        for (const auto& child : n.children) {
            if (child) ref(*child, ref);
        }
    };
    dfs(root, dfs);
}

void runStressTest() {
    std::string md = generate1MBMixedDocument();

    mwrender::editor::DocumentSessionOptions options;
    options.enableIncrementalParsing = true;
    mwrender::editor::DocumentSession session(options);

    // 1. Initial Parse Time
    auto start = std::chrono::high_resolution_clock::now();
    session.load(md);
    auto end = std::chrono::high_resolution_clock::now();
    auto initialParseMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Initial parse time: " << initialParseMs << " ms (Target: < 1000 ms)\n";
    require(initialParseMs < 1000, "Initial parse should be under 1000ms");

    checkNodeIdUniqueness(session.document());

    // 2. Perform 100 randomized edits
    std::mt19937 rng(1337);
    std::size_t totalEdits = 100;
    std::size_t fastPathCount = 0;
    std::size_t fallbackCount = 0;
    
    double totalFastPathMs = 0.0;
    double totalFallbackMs = 0.0;

    for (std::size_t editIdx = 0; editIdx < totalEdits; ++editIdx) {
        std::size_t docSize = session.markdown().size();
        std::size_t offset = rng() % (docSize - 100);

        // Find a safe spot inside the text (avoiding block fences or special markers to keep AST sane)
        // Scan forward to find a letter or space
        while (offset < docSize && !std::isalnum(static_cast<unsigned char>(session.markdown()[offset]))) {
            offset++;
        }
        if (offset >= docSize - 10) {
            offset = docSize / 2;
        }

        mwrender::TextChange change;
        change.from = offset;
        change.to = offset + 1;
        change.insertedText = "Y";

        auto editStart = std::chrono::high_resolution_clock::now();
        auto result = session.applyChange(change);
        auto editEnd = std::chrono::high_resolution_clock::now();

        double elapsedMs = std::chrono::duration<double, std::milli>(editEnd - editStart).count();
        require(result.ok, "Edit should succeed");

        checkNodeIdUniqueness(session.document());

        if (result.partialReparse) {
            fastPathCount++;
            totalFastPathMs += elapsedMs;
        } else {
            fallbackCount++;
            totalFallbackMs += elapsedMs;
            std::cout << "  [Fallback #" << fallbackCount << "] Reason: \"" 
                      << result.fallbackReason << "\" at offset " << offset 
                      << " took " << elapsedMs << " ms\n";
        }
    }

    double avgFastPathMs = fastPathCount > 0 ? (totalFastPathMs / fastPathCount) : 0.0;
    double avgFallbackMs = fallbackCount > 0 ? (totalFallbackMs / fallbackCount) : 0.0;

    std::cout << "Stress Test Completed:\n"
              << "  - Total Edits: " << totalEdits << "\n"
              << "  - Fast-path Updates: " << fastPathCount << " (Average: " << avgFastPathMs << " ms)\n"
              << "  - Fallback Updates: " << fallbackCount << " (Average: " << avgFallbackMs << " ms)\n";

#ifdef NDEBUG
    const double targetMs = 16.0;
#else
    const double targetMs = 100.0;
#endif

    if (fastPathCount > 0) {
        std::cout << "Average fast-path update time: " << avgFastPathMs << " ms (Target: < " << targetMs << " ms)\n";
        require(avgFastPathMs < targetMs, ("Average fast-path update must be under " + std::to_string(targetMs) + "ms").c_str());
    }
}

} // namespace

int main() {
    std::cout << "Running Editor Stress and Stability Tests...\n";
    runStressTest();
    std::cout << "All Editor Stress Tests passed.\n";
    return 0;
}
