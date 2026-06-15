# MWRender Integration Guide

MWRender exposes an installable C++20 library target named `MWRender::Core`.

## Installed Package

Install the project:

```bash
cmake --install build --config Release --prefix ./install
```

Use it from another CMake project:

```cmake
find_package(MWRender CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE MWRender::Core)
```

Configure the consumer with:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/install
```

## Engine API

```cpp
#include <iostream>
#include <mwrender/mwrender.hpp>

int main() {
    mwrender::Engine engine;
    engine.addThemeRoot("./themes");

    mwrender::RenderRequest request;
    request.markdown = "# Integrated\n\nHello **C++**.";
    request.options.outputMode = mwrender::OutputMode::FullDocument;
    request.options.themeId = "github-light";
    request.options.htmlPolicy = mwrender::HtmlPolicy::Sanitized;

    const auto result = engine.render(request);
    if (!result.ok) {
        for (const auto& diagnostic : result.diagnostics) {
            std::cerr << diagnostic.code << ": "
                      << diagnostic.message << '\n';
        }
        return 1;
    }

    std::cout << result.html;
}
```

The engine owns its parser, theme registry, renderer, and sanitizer. Reuse an
`Engine` instance across renders to avoid repeatedly rebuilding that state.

## AST-Only Parsing

```cpp
mwrender::Parser parser;
const auto parsed = parser.parse("# Heading");
if (parsed.ok && parsed.document) {
    const auto& root = *parsed.document;
}
```

The returned tree uses `std::unique_ptr` ownership. Every node contains a
`SourceRange` pointing to the original Markdown input.

## Result Data

`RenderResult` provides:

- `html`: selected final output.
- `fragment`: article-only HTML.
- `css`: composed theme and custom CSS.
- `document`: AST when `includeAst` is enabled.
- `outline`: heading hierarchy.
- `stats`: document statistics.
- `diagnostics`: warnings and errors.

## Threading

Theme and sanitizer configuration is protected internally. A render takes a
snapshot of those resources before processing. Applications should still keep
the input referenced by `RenderRequest::markdown` alive until `render`
returns.
