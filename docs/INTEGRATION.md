# MWRender C++ 接入与二次开发指南

MWRender 的核心模块是一个独立、静态无依赖的 C++ 库 (`mwrender_core`)。您可以非常方便地将它作为一个模块链接到您的任意 C++ 应用程序中（比如 Qt 桌面应用、高性能服务端生成器或游戏引擎）。

## 1. CMake 引入

如果您使用的是 CMake，可以通过 `add_subdirectory` 将本引擎引入：
```cmake
# 在您的 CMakeLists.txt 中：
add_subdirectory(path/to/MarkdownRenderEngine)

# 链接核心库 mwrender_core
target_link_libraries(YourApp PRIVATE mwrender_core)
```

## 2. 核心架构：两步渲染

MWRender 引擎采用现代编译器架构，严格区分 **AST 解析阶段 (Parser)** 和 **HTML 渲染阶段 (HtmlRenderer)**。
这使得您可以轻松地在中间步骤进行拦截，甚至编写您自己的 `PdfRenderer` 或 `WordRenderer`！

### 步骤 A：将字符串解析为 AST 抽象语法树
引入头文件：
```cpp
#include "mwrender/parser.hpp"
#include "mwrender/ast.hpp"
```

调用解析：
```cpp
using namespace mwrender;

// 配置引擎选项 (可以开启 MathJax, TOC 等高级语法扩展)
EngineOptions options;
options.extensions.toc = true;
options.extensions.footnotes = true;
options.extensions.latexMath = true;

// 初始化 Parser
Parser parser(options);

// 解析 Markdown 字符串
std::string markdown_source = "# Hello\nThis is **MWRender**.";
ParseResult result = parser.parse(markdown_source);

// 获取解析出来的根节点 (NodeType::Document)
std::unique_ptr<Node> document = std::move(result.document);
```

### 步骤 B：将 AST 渲染为最终 HTML
引入头文件：
```cpp
#include "mwrender/html_renderer.hpp"
```

执行渲染：
```cpp
// 准备渲染配置
RenderOptions renderOptions;
renderOptions.allowDocumentCss = true;
renderOptions.themeDir = "themes"; 

HtmlRenderer renderer;
// 可以选择传入一个 htmlSanitizer 防止 XSS 攻击
HtmlRenderResult htmlResult = renderer.render(*document, renderOptions, nullptr);

if (htmlResult.ok) {
    std::cout << htmlResult.fragment << std::endl;
} else {
    std::cerr << "Render failed!" << std::endl;
}
```

## 3. 拦截与扩展 (Advanced)

由于 `result.document` 是由各种类型（`HeadingData`, `ParagraphData` 等）构成的子节点树。
您可以在 `HtmlRenderer::render` 被调用之前，写一个递归函数遍历这棵树，进行您想要的操作。例如：
* 敏感词替换。
* 自动提取所有的图片链接。
* 修改指定层级标题的内容。

```cpp
void walkAst(Node* node) {
    if (node->type == NodeType::ImageData) {
        // ... 提取图片路径
    }
    for (auto& child : node->children) {
        walkAst(child.get());
    }
}
```
