# MWRender 使用指南

本文介绍如何构建、运行和集成 MWRender。内容适用于：

- 使用命令行把 Markdown 转换为 HTML；
- 在 C++20 项目中使用 `MWRender::Core`；
- 获取 AST、大纲、字数统计和诊断信息；
- 配置主题、自定义 CSS、HTML 安全策略；
- 使用节点查询、序列化和结构化编辑能力。

## 1. 环境要求

- CMake 3.20 或更高版本；
- 支持 C++20 的编译器；
- Windows 推荐使用 MinGW-w64/GCC；
- 构建核心库不需要额外运行时依赖。

本项目预设的 MinGW 编译器路径是：

```text
C:/Program Files/mingw64/bin/g++.exe
```

如果你的编译器安装位置不同，需要修改 `CMakePresets.json` 中的
`CMAKE_CXX_COMPILER`。

## 2. 构建项目

在项目根目录运行以下命令。

### 2.1 Debug 构建

Debug preset 同时生成 clangd 使用的 `compile_commands.json`：

```powershell
cmake --preset clangd
cmake --build --preset clangd
```

生成的主要文件位于：

```text
build/clangd/mwrender.exe
build/clangd/libmwrender_core.a
build/clangd/compile_commands.json
```

### 2.2 Release 构建

```powershell
cmake --preset release
cmake --build --preset release
```

### 2.3 运行测试

```powershell
ctest --preset clangd
```

显示失败测试的详细输出：

```powershell
ctest --test-dir build/clangd --output-on-failure
```

运行单个测试：

```powershell
ctest --test-dir build/clangd -R mwrender.smoke --output-on-failure
```

## 3. 命令行快速开始

以下示例使用 Debug 构建产物。在 PowerShell 中执行：

```powershell
build/clangd/mwrender.exe examples/basic.md -o output.html
```

使用浏览器打开 `output.html` 即可查看完整渲染结果。

### 3.1 基本语法

```text
mwrender [input.md] [options]
```

如果不指定输入文件，或输入文件为 `-`，程序从标准输入读取 Markdown。
如果不使用 `-o`，生成结果写入标准输出。

```powershell
"# Hello" | build/clangd/mwrender.exe
Get-Content README.md | build/clangd/mwrender.exe - --fragment
```

### 3.2 常用示例

生成完整 HTML5 文档：

```powershell
build/clangd/mwrender.exe document.md -o document.html
```

只生成 `<article>` 内容片段：

```powershell
build/clangd/mwrender.exe document.md --fragment -o fragment.html
```

使用暗色主题：

```powershell
build/clangd/mwrender.exe document.md --theme github-dark -o document.html
```

允许经过清理的原始 HTML：

```powershell
build/clangd/mwrender.exe document.md --html-policy sanitized -o document.html
```

查看 AST：

```powershell
build/clangd/mwrender.exe document.md --dump-ast
```

查看可用主题：

```powershell
build/clangd/mwrender.exe --list-themes
```

### 3.3 CLI 参数

| 参数 | 说明 |
| --- | --- |
| `-o, --output <file>` | 输出文件；省略或使用 `-` 时写入 stdout |
| `--fragment` | 只输出文章片段，不生成完整 HTML5 外壳 |
| `--theme <id>` | 选择主题，默认 `github-light` |
| `--config <file>` | 加载 JSON 配置文件 |
| `--theme-path <dir>` | 注册外部主题根目录 |
| `--list-themes` | 列出可用主题 |
| `--css <file>` | 追加自定义 CSS 文件 |
| `--html-policy <policy>` | `disabled`、`sanitized` 或 `trusted` |
| `--title <title>` | 设置完整 HTML 文档的标题 |
| `--lang <language>` | 设置 `<html lang="...">` |
| `--strict` | 主题或 sanitizer 不可用时按错误处理 |
| `--no-tables` | 禁用 GFM 表格 |
| `--no-task-lists` | 禁用任务列表 |
| `--no-strikethrough` | 禁用删除线 |
| `--no-autolinks` | 禁用自动链接 |
| `--allow-document-css` | 允许 Front Matter 引用 CSS 文件 |
| `--allow-remote-css` | 允许 CSS 使用远程资源 |
| `--dump-ast` | 输出 AST，而不是 HTML |
| `--version` | 显示版本 |
| `-h, --help` | 显示帮助 |

命令行参数会覆盖配置文件中的同名设置。

### 3.4 退出码

| 退出码 | 含义 |
| --- | --- |
| `0` | 成功 |
| `1` | 解析或渲染失败 |
| `2` | 参数或配置文件错误 |
| `3` | 输入读取或输出写入失败 |

警告和错误诊断写入 stderr，HTML 写入文件或 stdout。

## 4. JSON 配置文件

可以把常用选项保存为 JSON：

```json
{
  "theme": "github-light",
  "output": {
    "mode": "full-document",
    "language": "zh-CN",
    "includeCss": true
  },
  "html": {
    "enabled": true,
    "policy": "sanitized"
  },
  "css": {
    "enabled": true,
    "allowRemoteUrl": false,
    "files": ["custom.css"]
  },
  "sourceMap": {
    "enabled": true
  }
}
```

运行：

```powershell
build/clangd/mwrender.exe document.md --config mwrender.json -o document.html
```

`css.files` 中的相对路径以配置文件所在目录为基准。

当前配置文件支持的字段：

| 字段 | 类型 | 可选值或含义 |
| --- | --- | --- |
| `theme` | string | 主题 ID |
| `output.mode` | string | `fragment` 或 `full-document` |
| `output.language` | string | HTML language |
| `output.includeCss` | bool | 是否在完整文档中嵌入 CSS |
| `html.enabled` | bool | 是否允许处理原始 HTML |
| `html.policy` | string | `disabled`、`sanitized`、`trusted` |
| `css.enabled` | bool | 是否读取 `css.files` |
| `css.allowRemoteUrl` | bool | 是否允许远程 CSS 资源 |
| `css.files` | array | 自定义 CSS 文件列表 |
| `sourceMap.enabled` | bool | 是否输出行级源码映射属性 |

## 5. 支持的 Markdown

MWRender 支持常用 Markdown 和部分 GFM 扩展：

- ATX 标题：`#` 到 `######`；
- 段落、引用、分隔线；
- 有序和无序列表；
- 围栏代码块；
- emphasis、strong、行内代码；
- 链接与图片；
- GFM 表格、任务列表、删除线；
- 尖括号自动链接；
- 原始 HTML；
- Front Matter；
- `[TOC]` 目录；
- 脚注；
- 行内和块级数学公式；
- Mermaid 代码块；
- 代码高亮资源。

### 5.1 目录

```markdown
# 第一章

[TOC]

## 第一节
```

### 5.2 脚注

```markdown
这里有一个脚注[^note]。

[^note]: 脚注内容。
```

### 5.3 数学公式

```markdown
行内公式：$E = mc^2$

$$
\int_a^b f(x)\,dx
$$
```

### 5.4 Mermaid

````markdown
```mermaid
flowchart LR
    A --> B
```
````

### 5.5 当前限制

当前版本不完整支持全部 CommonMark/GFM，主要限制包括：

- 不支持 Setext 标题；
- 不支持四空格缩进代码块；
- 不支持引用式链接定义；
- 不解码全部 HTML entity；
- 裸 URL 自动链接尚未实现，只支持尖括号形式；
- 深层引用、复杂嵌套列表和完整 emphasis delimiter 算法仍有限制。

精确兼容情况见 `docs/Compatibility.md`。

## 6. Front Matter

Front Matter 必须位于文档开头，目前只支持有限字段：

```yaml
---
title: 示例文档
theme: github-dark
css: [custom.css]
---
```

字段含义：

- `title`：完整 HTML 文档标题；
- `theme`：文档建议使用的主题；
- `css`：相对于 Markdown 文件目录的 CSS 文件。

Front Matter CSS 默认不会加载，必须显式使用：

```powershell
build/clangd/mwrender.exe document.md --allow-document-css
```

通过 stdin 输入时没有文档目录，因而不适合使用相对 Front Matter CSS。

## 7. 主题与 CSS

### 7.1 内置主题

项目内置以下主题：

```text
github-light
github-dark
github-auto
github-dark-dimmed
github-dark-high-contrast
github-dark-colorblind
github-light-colorblind
```

### 7.2 请求级 CSS

CLI 使用：

```powershell
build/clangd/mwrender.exe document.md --css custom.css -o document.html
```

C++ API 使用：

```cpp
mwrender::RenderRequest request;
request.markdown = "# Styled";
request.css.files.push_back("custom.css");
request.css.text.push_back(".mw-heading { color: #c62828; }");
```

### 7.3 外部主题

主题目录结构：

```text
themes/
└── custom-light/
    ├── theme.json
    ├── content.css
    └── code.css
```

CLI 加载：

```powershell
build/clangd/mwrender.exe document.md `
  --theme-path themes `
  --theme custom-light `
  -o document.html
```

C++ API 加载：

```cpp
mwrender::Engine engine;
engine.addThemeRoot("themes");
```

完整主题清单格式见 `docs/THEME_SPEC.md`。

## 8. HTML 安全策略

`HtmlPolicy` 有三种模式：

| 模式 | 行为 | 使用场景 |
| --- | --- | --- |
| `Disabled` | 转义所有原始 HTML | 默认，处理不可信内容 |
| `Sanitized` | 仅保留 allowlist 中的安全标签和属性 | 需要有限 HTML 格式 |
| `Trusted` | 原样输出 HTML | 仅限完全可信的本地内容 |

CLI 示例：

```powershell
build/clangd/mwrender.exe document.md --html-policy sanitized
```

即使使用 `Trusted`，Markdown 链接和图片仍会检查危险 URL scheme。

默认还会拒绝：

- CSS `@import`；
- CSS 中的远程 `url(http...)`；
- 可逃逸 `<style>` 的 `</style>` 内容；
- 越出允许根目录的主题和文档 CSS 路径。

除非内容和资源完全可信，否则不要使用 `trusted` 或
`--allow-remote-css`。

## 9. C++ API 快速开始

### 9.1 最小渲染示例

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
        for (const auto& diagnostic : result.diagnostics) {
            std::cerr << diagnostic.code << ": "
                      << diagnostic.message << '\n';
        }
        return 1;
    }

    std::cout << result.html;
}
```

`RenderRequest::markdown` 是 `std::string_view`，它引用的字符串必须保持有效，直到
`render()` 返回。

### 9.2 常用渲染选项

```cpp
mwrender::RenderRequest request;
request.markdown = markdown;
request.sourcePath = "docs/article.md";

request.options.outputMode = mwrender::OutputMode::FullDocument;
request.options.renderMode = mwrender::RenderMode::Preview;
request.options.sourceMap = mwrender::SourceMapMode::Line;
request.options.htmlPolicy = mwrender::HtmlPolicy::Sanitized;
request.options.themeId = "github-dark";
request.options.language = "zh-CN";
request.options.title = "示例文档";
request.options.includeCss = true;
request.options.includeAst = true;
```

### 9.3 扩展开关

```cpp
request.options.extensions.tables = true;
request.options.extensions.taskLists = true;
request.options.extensions.strikethrough = true;
request.options.extensions.autoLinks = true;
request.options.extensions.latexMath = true;
request.options.extensions.mermaid = true;
request.options.extensions.toc = true;
request.options.extensions.footnotes = true;
request.options.extensions.highlight = true;
```

### 9.4 RenderResult

```cpp
const auto result = engine.render(request);
```

主要字段：

| 字段 | 内容 |
| --- | --- |
| `ok` | 是否成功完成渲染 |
| `html` | 根据 `OutputMode` 选择的最终结果 |
| `fragment` | 文章 HTML 片段 |
| `css` | 合成后的主题和自定义 CSS |
| `document` | AST，受 `includeAst` 控制 |
| `diagnostics` | 警告和错误 |
| `resolvedThemeId` | 实际使用的主题 ID |
| `outline` | 标题大纲树 |
| `stats` | 字符、单词、段落、标题、图片等统计 |

不要只检查 `diagnostics.empty()`。存在 Warning 时渲染仍可能成功，应以 `ok` 判断
操作是否成功。

### 9.5 只解析 AST

```cpp
mwrender::Parser parser;
const auto parsed = parser.parse("# Heading\n\nText");

if (parsed.ok && parsed.document) {
    const mwrender::Node& root = *parsed.document;
    // root.type == mwrender::NodeType::Document
}
```

AST 节点通过 `std::unique_ptr` 持有子节点，因此整棵树可移动、不可复制。

## 10. 查询、序列化与编辑

### 10.1 根据源码位置查节点

```cpp
const mwrender::Node* node =
    mwrender::findNodeBySourceOffset(*result.document, 12);
```

也可以按 ID 或范围查询：

```cpp
const auto* byId = mwrender::findNodeById(*result.document, nodeId);
const auto nodes = mwrender::findNodesByRange(*result.document, range);
```

offset 是原始 UTF-8 输入的字节偏移，不是 Unicode 字符序号。

### 10.2 AST 序列化为 Markdown

```cpp
mwrender::MarkdownStyle style;
style.bulletMarker = "-";
style.strongMarker = "**";
style.lineEnding = "\n";

const std::string markdown = engine.serialize(*result.document, style);
```

### 10.3 应用结构化编辑命令

```cpp
mwrender::Command command{
    mwrender::Command::Type::SetHeadingLevel,
    headingNodeId,
    "2",
    ""
};

auto changedDocument = engine.applyCommand(*result.document, command);
const std::string changedMarkdown = engine.serialize(*changedDocument);
```

支持的命令包括任务状态切换、标题等级、列表切换、strong/emphasis 包裹、插入链接或
图片、删除线以及列表缩进调整。

### 10.4 更新文档

```cpp
mwrender::TextChange change;
change.from = 0;
change.to = 0;
change.insertedText = "# New title\n\n";

auto updated = engine.update(*result.document, change);
```

结果包含新 AST，以及 changed/inserted/removed 节点 ID。当前实现会重新解析全文，再
通过稳定 ID 比较节点变化。

### 10.5 渲染单个节点

```cpp
mwrender::NodeRenderRequest nodeRequest;
nodeRequest.document = result.document.get();
nodeRequest.nodeId = nodeId;
nodeRequest.options = request.options;

const auto nodeResult = engine.renderNode(nodeRequest);
```

这适合编辑器只更新某个预览区域的场景。

## 11. 在其他 CMake 项目中使用

先安装 MWRender：

```powershell
cmake --install build/release --prefix D:/libs/mwrender
```

调用方的 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyMarkdownApp LANGUAGES CXX)

find_package(MWRender CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_compile_features(my_app PRIVATE cxx_std_20)
target_link_libraries(my_app PRIVATE MWRender::Core)
```

配置调用方项目：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:/libs/mwrender
cmake --build build
```

也可以在同一个大型 CMake 工程中使用 `add_subdirectory()`：

```cmake
add_subdirectory(external/MarkdownRenderEngine)
target_link_libraries(my_app PRIVATE MWRender::Core)
```

## 12. 资源限制与线程使用

可以在创建 Engine 时设置限制：

```cpp
mwrender::EngineOptions options;
options.maxInputBytes = 8ULL * 1024ULL * 1024ULL;
options.maxThemeFileBytes = 1ULL * 1024ULL * 1024ULL;
options.maxNestingDepth = 128;
options.invalidUtf8Policy = mwrender::InvalidUtf8Policy::Reject;

mwrender::Engine engine(options);
```

`Engine` 内部保护主题和 sanitizer 配置。渲染开始时会取得资源快照，因此可复用一个
Engine 执行多次渲染。不要在渲染结束前销毁 `RenderRequest::markdown` 引用的字符串。

## 13. 常见问题

### clangd 提示头文件找不到

先生成编译数据库：

```powershell
cmake --preset clangd
```

然后在 VS Code 中执行 `clangd: Restart language server`。

### 生成的 Fragment 没有样式

Fragment 模式只输出文章结构，不会把 `<style>` 标签塞进片段。请从
`RenderResult::css` 获取 CSS，或由宿主页面自行加载主题样式。

### Markdown 中的 HTML 被显示成文本

默认策略是 `HtmlPolicy::Disabled`。对不可信内容建议改用 `Sanitized`，不要直接使用
`Trusted`。

### 自定义 CSS 没有生效

检查：

1. CSS 文件路径是否正确；
2. Front Matter CSS 是否启用了 `--allow-document-css`；
3. CSS 是否包含默认禁止的远程 URL 或 `@import`；
4. 是否查看了 stderr 中的 `MW4001` 到 `MW4006` 诊断。

### 主题不存在

使用 `--list-themes` 查看主题 ID。外部主题需要同时传入 `--theme-path` 和
`--theme`。非严格模式下通常回退到 `github-light`，严格模式下渲染失败。

### 中文位置为什么按字节计算

`SourcePosition::offset` 是 UTF-8 字节偏移，用于稳定、精确地映射原始输入。不能直接
把它当作中文字符下标；编辑器集成时应在自身字符模型与 UTF-8 字节位置之间转换。

## 14. 相关文档

- `docs/CLI_USAGE.md`：CLI 与扩展语法速查；
- `docs/API.md`：公共 API 简介；
- `docs/INTEGRATION.md`：CMake 集成；
- `docs/Compatibility.md`：语法兼容矩阵和诊断码；
- `docs/THEME_SPEC.md`：外部主题规范；
- `docs/SECURITY.md`：安全模型；
- `docs/SOURCE_READING_GUIDE.md`：架构与源码阅读路线。
