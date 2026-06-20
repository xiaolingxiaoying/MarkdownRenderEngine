# MWRender C++ API

## Rendering

```cpp
#include <mwrender/mwrender.hpp>

mwrender::Engine engine;
mwrender::RenderRequest request;
request.markdown = "# Hello\n\nRendered by **MWRender**.";
request.options.outputMode = mwrender::OutputMode::Fragment;

const auto result = engine.render(request);
if (result.ok) {
    std::cout << result.html;
}
```

`RenderResult` contains HTML, the article fragment, composed CSS, optional
AST, diagnostics, resolved theme ID, outline, and word statistics.

## Parser Access

Applications that only need the AST can use `mwrender::Parser` directly:

```cpp
mwrender::Parser parser;
const auto parsed = parser.parse("# Title");
```

## Themes

```cpp
engine.addThemeRoot("./themes");
request.options.themeId = "custom-light";
```

`Engine::listThemes()` returns built-in and discovered external themes.

## HTML Policies

```cpp
request.options.htmlPolicy = mwrender::HtmlPolicy::Sanitized;
```

`Disabled` escapes raw HTML and remains the default. `Sanitized` uses the
built-in `SafeHtmlSanitizer`. `Trusted` emits raw HTML unchanged.

Applications can replace the built-in sanitizer:

```cpp
engine.setHtmlSanitizer(std::make_shared<MySanitizer>());
```

## Installed CMake Package

```cmake
find_package(MWRender CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE MWRender::Core)
```
