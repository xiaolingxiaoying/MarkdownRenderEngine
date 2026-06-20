# MWRender

MWRender is a C++20 Markdown document engine that parses Markdown into a public
AST and renders deterministic HTML for CLI and application integration.

## Features

- Markdown to AST to HTML architecture.
- Source line, column, and original byte ranges.
- Fragment and standalone HTML5 output.
- GFM tables, task lists, strikethrough, autolinks, TOC, and footnotes.
- Offline themes, highlighting, MathJax, and Mermaid resources.
- Built-in safe HTML sanitizer with explicit trusted mode.
- Outline and word statistics.
- Installable CMake package and command-line application.
- Unit, snapshot, conformance, stress, and security tests.

See [Compatibility](docs/specs/compatibility.md) for the exact supported syntax.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## CLI

```bash
mwrender document.md -o document.html
mwrender document.md --fragment
mwrender document.md --theme github-dark
mwrender document.md --html-policy sanitized
mwrender document.md --dump-ast
```

See [CLI Usage](docs/guides/cli.md) for all options.

## C++ API

```cpp
#include <iostream>
#include <mwrender/mwrender.hpp>

int main() {
    mwrender::Engine engine;
    mwrender::RenderRequest request;
    request.markdown = "# Hello\n\nRendered by **MWRender**.";
    request.options.outputMode = mwrender::OutputMode::Fragment;

    const auto result = engine.render(request);
    if (!result.ok) {
        return 1;
    }
    std::cout << result.html;
}
```

See [API](docs/core/api.md) and [Integration](docs/guides/integration.md).

## Themes

Seven GitHub-derived themes are embedded. External theme packages support
`content.css`, optional `code.css`, manifest variables, and path validation.

See [Theme Specification](docs/specs/theme.md).

## Documentation

- [EditorCore Documentation Index](docs/editor/README.md)
- [Architecture](docs/core/architecture.md)
- [AST Specification](docs/specs/ast.md)
- [HTML Output Specification](docs/specs/html.md)
- [Security](docs/core/security.md)
- [Roadmap (Legacy)](docs/archive/ROADMAP_v0.1.md)
