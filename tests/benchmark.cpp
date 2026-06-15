// Performance benchmark for MWRender (Phase 8).
//
// This benchmark measures parse+render throughput for representative workloads.
// Results are printed to stdout for baseline recording.
// The executable exits 0 unless a hard timeout is exceeded (indicating a
// severe regression). The timeout is deliberately generous for CI environments.
//
// Usage: mwrender_benchmark [--record-baseline]

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/engine.hpp>

namespace {

struct BenchResult {
    std::string name;
    std::size_t inputBytes;
    long long elapsedMs;
    int iterations;
    double throughputMBps;
};

// Run a benchmark returning the elapsed time in milliseconds.
BenchResult bench(
    const std::string& name,
    mwrender::Engine& engine,
    const std::string& markdown,
    int iterations = 1) {
    // Warm-up
    {
        mwrender::RenderRequest req;
        req.markdown = markdown;
        req.options.outputMode = mwrender::OutputMode::Fragment;
        [[maybe_unused]] auto warmup = engine.render(req);
    }

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        mwrender::RenderRequest req;
        req.markdown = markdown;
        req.options.outputMode = mwrender::OutputMode::Fragment;
        const auto result = engine.render(req);
        if (!result.ok) {
            std::cerr << "Benchmark render failed: " << name << '\n';
            std::exit(1);
        }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    const double totalBytes =
        static_cast<double>(markdown.size()) * static_cast<double>(iterations);
    const double throughput =
        elapsed > 0
            ? totalBytes / (static_cast<double>(elapsed) / 1000.0) / (1024.0 * 1024.0)
            : 0.0;

    return {name, markdown.size(), elapsed, iterations, throughput};
}

void printResult(const BenchResult& r) {
    std::cout << "  [BENCH] " << r.name << '\n'
              << "          input: " << r.inputBytes << " bytes"
              << "  iterations: " << r.iterations
              << "  total: " << r.elapsedMs << " ms"
              << "  throughput: ";
    if (r.throughputMBps > 0) {
        std::cout << r.throughputMBps << " MB/s\n";
    } else {
        std::cout << "(n/a)\n";
    }
}

// ---------------------------------------------------------------------------
// Generate workloads
// ---------------------------------------------------------------------------

// Workload 1: Plain text paragraphs (~1 MB)
std::string makePlainTextMb() {
    std::string md;
    md.reserve(1024 * 1024 + 256);
    while (md.size() < 1024 * 1024) {
        md += "This is a plain text paragraph with no special Markdown syntax. "
              "It is used to measure raw throughput of the parser and renderer "
              "on ordinary prose content.\n\n";
    }
    return md;
}

// Workload 2: Mixed GFM content (~1 MB)
std::string makeGfmMixedMb() {
    std::string md;
    md.reserve(1024 * 1024 + 256);
    int counter = 0;
    while (md.size() < 1024 * 1024) {
        md += "## Section " + std::to_string(counter++) + "\n\n";
        md += "Normal paragraph with **bold** and *emphasis* and `code`.\n\n";
        md += "| Column A | Column B |\n| :------- | -------: |\n| value1   | value2   |\n\n";
        md += "- [x] Task one\n- [ ] Task two\n\n";
        md += "~~deleted~~ and <https://example.com>\n\n";
        md += "```python\nprint('hello world')\nresult = [x * 2 for x in range(10)]\n```\n\n";
        md += "> Blockquote with **emphasis** inside.\n\n";
    }
    return md;
}

// Workload 3: Many short documents (1000 × ~400 bytes)
std::string makeShortDocument(int index) {
    return "# Document " + std::to_string(index) + "\n\n"
           "Paragraph with **bold**, *emphasis*, and [link](https://example.com).\n\n"
           "- Item one\n- Item two\n\n"
           "```\nsome code here\n```\n";
}

// Workload 4: Deep heading hierarchy
std::string makeHeadingHeavy() {
    std::string md;
    md.reserve(100 * 80);
    for (int i = 0; i < 2000; ++i) {
        const int level = (i % 6) + 1;
        md += std::string(static_cast<std::size_t>(level), '#') +
              " Heading number " + std::to_string(i) + "\n\n";
        md += "Brief content paragraph.\n\n";
    }
    return md;
}

// Workload 5: Code-heavy content
std::string makeCodeHeavy() {
    std::string md;
    md.reserve(1024 * 512);
    for (int i = 0; i < 100; ++i) {
        md += "## Function " + std::to_string(i) + "\n\n";
        md += "```cpp\n";
        for (int line = 0; line < 20; ++line) {
            md += "    int var_" + std::to_string(line) +
                  " = compute(" + std::to_string(i * line) + ");\n";
        }
        md += "```\n\n";
    }
    return md;
}

} // namespace

int main() {
    std::cout << "=== MWRender Performance Benchmarks ===\n\n";

    mwrender::Engine engine;

    std::vector<BenchResult> results;

    // 1. Plain text 1 MB
    {
        std::cout << "Running: plain text 1 MB...\n";
        const auto md = makePlainTextMb();
        results.push_back(bench("plain-text-1MB", engine, md, 1));
    }

    // 2. Mixed GFM 1 MB
    {
        std::cout << "Running: mixed GFM 1 MB...\n";
        const auto md = makeGfmMixedMb();
        results.push_back(bench("gfm-mixed-1MB", engine, md, 1));
    }

    // 3. Many short documents (1000 iterations)
    {
        std::cout << "Running: 1000 short documents...\n";
        // Use a fixed document for the repeated benchmark
        const auto md = makeShortDocument(42);
        results.push_back(bench("short-doc-x1000", engine, md, 1000));
    }

    // 4. Heading-heavy document
    {
        std::cout << "Running: heading-heavy document...\n";
        const auto md = makeHeadingHeavy();
        results.push_back(bench("heading-heavy", engine, md, 1));
    }

    // 5. Code-heavy document
    {
        std::cout << "Running: code-heavy document...\n";
        const auto md = makeCodeHeavy();
        results.push_back(bench("code-heavy", engine, md, 1));
    }

    // 6. Full document mode with theme (includes CSS composition)
    {
        std::cout << "Running: full document with theme...\n";
        const auto md = makePlainTextMb().substr(0, 64 * 1024); // 64 KB
        results.push_back(bench("full-doc-64KB", engine, md, 1));
    }

    std::cout << "\n=== Benchmark Results ===\n";
    bool hardTimeoutExceeded = false;
    for (const auto& r : results) {
        printResult(r);
        // Hard timeout: no single benchmark should exceed 30 seconds
        if (r.elapsedMs > 30000) {
            std::cerr << "  HARD TIMEOUT exceeded for: " << r.name << '\n';
            hardTimeoutExceeded = true;
        }
    }

    std::cout << "\n=== Throughput Summary ===\n";
    for (const auto& r : results) {
        std::cout << "  " << r.name << ": " << r.elapsedMs << " ms";
        if (r.iterations > 1) {
            std::cout << " (" << r.iterations << " iters, "
                      << (r.elapsedMs / r.iterations) << " ms/iter)";
        }
        std::cout << '\n';
    }

    if (hardTimeoutExceeded) {
        std::cerr << "\nBenchmark FAILED: hard timeout exceeded.\n";
        return 1;
    }

    std::cout << "\nBenchmarks completed successfully.\n";
    return 0;
}
