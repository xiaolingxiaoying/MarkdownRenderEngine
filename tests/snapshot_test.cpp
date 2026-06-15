#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <mwrender/engine.hpp>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "Could not read snapshot fixture: " << path.string() << '\n';
        std::exit(1);
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
}

std::string normalizeNewlines(std::string text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\r') {
            if (index + 1 < text.size() && text[index + 1] == '\n') {
                ++index;
            }
            normalized += '\n';
        } else {
            normalized += text[index];
        }
    }
    return normalized;
}

void runSnapshot(std::string_view name) {
    const std::filesystem::path root = MWRENDER_SNAPSHOT_DIR;
    const auto markdown =
        normalizeNewlines(readFile(root / (std::string(name) + ".md")));
    const auto expected =
        normalizeNewlines(readFile(root / (std::string(name) + ".html")));

    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment;
    request.options.sourceMap = mwrender::SourceMapMode::None;
    request.options.includeAst = false;
    const auto result = engine.render(request);

    if (!result.ok || result.html != expected) {
        std::cerr << "Snapshot failed: " << name << "\n\nExpected:\n"
                  << expected << "\nActual:\n" << result.html << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    for (const std::string_view snapshot : {
             "blocks",
             "inline",
             "gfm",
             "document",
         }) {
        runSnapshot(snapshot);
    }
    return 0;
}
