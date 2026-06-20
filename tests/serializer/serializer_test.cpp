#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <mwrender/engine.hpp>
#include <mwrender/serializer.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testSerializerPreserve() {
    mwrender::Engine engine;
    auto ast = engine.parse("___bold___\n\n* item 1\n* item 2\n\n~~~cpp\nint main(){}\n~~~\n");
    
    mwrender::MarkdownStyle style;
    style.preserveOriginalMarkers = true;
    
    std::string result = mwrender::serializeMarkdown(*ast.document, style);
    std::cout << "Serialized:\n" << result << "\n";
    
    require(result.find("___bold___") != std::string::npos, "Should preserve ___ bold marker");
    require(result.find("* item 1") != std::string::npos, "Should preserve * list marker");
    require(result.find("~~~cpp") != std::string::npos, "Should preserve ~~~ code fence marker");
}

} // namespace

int main() {
    std::cout << "Running serializer_test...\n";
    testSerializerPreserve();
    std::cout << "All serializer tests passed.\n";
    return 0;
}
