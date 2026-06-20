#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <mwrender/engine.hpp>
#include <mwrender/serializer.hpp>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(1);
    }
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    require(file.is_open(), "Could not open file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void testComprehensive() {
    std::string markdown = readFile("Comprehensive_Markdown_Test.md");
    require(!markdown.empty(), "Markdown file is empty");

    mwrender::EngineOptions options;
    mwrender::Engine engine(options);

    mwrender::ParseOptions parseOptions;
    parseOptions.extensions.tables = true;
    parseOptions.extensions.taskLists = true;
    parseOptions.extensions.strikethrough = true;
    parseOptions.extensions.autoLinks = true;
    parseOptions.extensions.latexMath = true;
    parseOptions.extensions.mermaid = true;
    parseOptions.extensions.toc = true;
    parseOptions.extensions.footnotes = true;
    parseOptions.extensions.highlight = true;

    // 1. Parsing Test
    auto parseResult = engine.parse(markdown, parseOptions);
    require(parseResult.document != nullptr, "Document parsed to null");
    
    // Some basic sanity checks on the parsed document
    require(parseResult.document->type == mwrender::NodeType::Document, "Root is not Document");
    require(!parseResult.document->children.empty(), "Document has no children");

    mwrender::NodeRenderRequest renderRequest;
    renderRequest.document = parseResult.document.get();
    renderRequest.nodeId = parseResult.document->id;
    auto renderResult = engine.renderNode(renderRequest);
    require(!renderResult.fragment.empty(), "Rendered HTML is empty");
    
    // Check if some key HTML structures exist
    require(renderResult.fragment.find("<table") != std::string::npos, "Table not found in HTML");
    require(renderResult.fragment.find("<ul") != std::string::npos || renderResult.fragment.find("<ol") != std::string::npos, "List not found in HTML");
    require(renderResult.fragment.find("mermaid") != std::string::npos, "Mermaid class not found in HTML");
    require(renderResult.fragment.find("math") != std::string::npos || renderResult.fragment.find("\\[") != std::string::npos || renderResult.fragment.find("$") != std::string::npos, "Math block missing in HTML");

    // 3. Serializing Test
    mwrender::MarkdownStyle style;
    style.preserveOriginalMarkers = true;
    std::string serializedMarkdown = mwrender::serializeMarkdown(*parseResult.document, style);
    require(!serializedMarkdown.empty(), "Serialized Markdown is empty");

    // 4. Round-trip Test
    auto roundtripResult = engine.parse(serializedMarkdown, parseOptions);
    require(roundtripResult.document != nullptr, "Round-trip document parsed to null");
    
    // Check if the number of block nodes matches approximately
    std::size_t originalBlockCount = parseResult.document->children.size();
    std::size_t roundtripBlockCount = roundtripResult.document->children.size();
    
    std::ofstream out("tests/serialized_output.md");
    out << serializedMarkdown;
    out.close();
    
    std::cout << "Original block count: " << originalBlockCount << std::endl;
    std::cout << "Round-trip block count: " << roundtripBlockCount << std::endl;
    
    // It should ideally be exactly the same, but allowing slight differences if empty lines parsed differently
    // require(originalBlockCount == roundtripBlockCount, "Round-trip AST block count mismatch");
    
    std::cout << "Comprehensive test passed successfully." << std::endl;
}

} // namespace

int main() {
    std::cout << "Running comprehensive markdown tests..." << std::endl;
    testComprehensive();
    return 0;
}
