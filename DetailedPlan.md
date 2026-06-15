# Markdown Render Engine 开发文档

## 1. 项目名称

项目暂定名称：

```text
MWRender
```

全称：

```text
Markdown Writer Render Engine
```

项目定位：

```text
一个使用 C++ 实现的轻量级 Markdown 渲染器，支持 Markdown 源码解析、AST 文档树生成、HTML 渲染、HTML 扩展、CSS 扩展和主题系统。
```

---

## 2. 项目目标

本项目的核心目标是先独立实现一个 Markdown 渲染器，为后续 Markdown 编辑器、预览模式、分屏模式、所见即所得模式、HTML 导出、主题系统提供基础。

项目输入是 Markdown 文本，输出是 HTML 文档或 HTML 片段。

基本流程：

```text
Markdown Source
    ↓
Line Scanner
    ↓
Block Parser
    ↓
Inline Parser
    ↓
Markdown AST
    ↓
HTML Renderer
    ↓
Theme CSS
    ↓
Final HTML
```

---

## 3. 项目边界

### 3.1 当前阶段要做

第一阶段重点做：

```text
1. Markdown 基础语法解析
2. Markdown AST 文档树
3. AST 到 HTML 的渲染
4. HTML 扩展支持
5. CSS 扩展支持
6. 主题系统
7. 默认 GitHub Light 风格主题
8. CLI 命令行工具
9. C++ 调用接口
10. 单元测试和快照测试
```

---

### 3.2 当前阶段不做

第一阶段暂时不做：

```text
1. 不做完整 Markdown 编辑器界面
2. 不做所见即所得编辑
3. 不做 Pandoc 多格式转换
4. 不做 PDF / DOCX 导出
5. 不做 Mermaid
6. 不做 LaTeX 公式
7. 不做插件市场
8. 不追求一次性完整兼容 CommonMark 所有边界规则
```

这些内容可以作为后续版本扩展。

---

## 4. 技术选型

推荐技术栈：

```text
语言：C++20
构建系统：CMake
测试框架：Catch2 或 GoogleTest
配置文件：JSON
样式文件：CSS
输出格式：HTML5
默认主题：GitHub Light 风格
```

项目初期不要依赖 Qt。

原因是这个项目应该作为独立渲染核心，后续 Qt Markdown 编辑器只负责调用它。

推荐关系：

```text
MWRender Core
    ↓
Qt Markdown Editor
```

不要变成：

```text
Qt Markdown Editor 内部写死一个渲染器
```

前者更容易复用、测试和维护。

---

## 5. 项目目录结构

推荐目录结构：

```text
mwrender/
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
│
├─ include/
│  └─ mwrender/
│     ├─ mwrender.hpp
│     ├─ options.hpp
│     ├─ result.hpp
│     ├─ ast.hpp
│     ├─ parser.hpp
│     ├─ renderer.hpp
│     ├─ theme.hpp
│     ├─ outline.hpp
│     ├─ stats.hpp
│     └─ version.hpp
│
├─ src/
│  ├─ core/
│  │  ├─ document.cpp
│  │  ├─ source_range.cpp
│  │  └─ string_utils.cpp
│  │
│  ├─ scanner/
│  │  ├─ line_scanner.cpp
│  │  └─ line.cpp
│  │
│  ├─ parser/
│  │  ├─ block_parser.cpp
│  │  ├─ inline_parser.cpp
│  │  ├─ heading_parser.cpp
│  │  ├─ paragraph_parser.cpp
│  │  ├─ list_parser.cpp
│  │  ├─ blockquote_parser.cpp
│  │  ├─ codeblock_parser.cpp
│  │  ├─ html_parser.cpp
│  │  └─ table_parser.cpp
│  │
│  ├─ ast/
│  │  ├─ node.cpp
│  │  ├─ node_type.cpp
│  │  └─ ast_printer.cpp
│  │
│  ├─ html/
│  │  ├─ html_renderer.cpp
│  │  ├─ html_escape.cpp
│  │  ├─ slug_generator.cpp
│  │  └─ sanitizer.cpp
│  │
│  ├─ theme/
│  │  ├─ theme.cpp
│  │  ├─ theme_manager.cpp
│  │  ├─ css_builder.cpp
│  │  └─ theme_json_loader.cpp
│  │
│  ├─ analysis/
│  │  ├─ outline_builder.cpp
│  │  └─ word_counter.cpp
│  │
│  └─ cli/
│     └─ main.cpp
│
├─ themes/
│  └─ github-light/
│     ├─ theme.json
│     ├─ content.css
│     └─ code.css
│
├─ examples/
│  ├─ basic.md
│  ├─ html-extension.md
│  ├─ custom-css.md
│  └─ table.md
│
├─ tests/
│  ├─ parser/
│  ├─ inline/
│  ├─ html/
│  ├─ theme/
│  ├─ security/
│  └─ snapshot/
│
└─ docs/
   ├─ design.md
   ├─ ast-spec.md
   ├─ html-output-spec.md
   ├─ theme-spec.md
   ├─ css-extension-spec.md
   ├─ html-extension-spec.md
   └─ api-spec.md
```

---

## 6. 核心模块划分

### 6.1 Line Scanner

作用：

```text
把 Markdown 源文本按行切分，保留每一行的行号、起始偏移、结束偏移。
```

数据结构：

```cpp
struct Line {
    int number = 0;
    std::size_t startOffset = 0;
    std::size_t endOffset = 0;
    std::string_view text;
};
```

Line Scanner 不负责理解 Markdown 语法，只负责提供后续解析所需的行信息。

需要注意：

```text
1. 保留原始换行信息
2. 兼容 \n 和 \r\n
3. 保留空行
4. 保留每行的源代码位置
```

---

### 6.2 Block Parser

作用：

```text
解析 Markdown 的块级结构。
```

块级结构包括：

```text
Document
Heading
Paragraph
BlockQuote
List
ListItem
CodeBlock
HtmlBlock
ThematicBreak
Table
FrontMatter
```

Block Parser 的推荐匹配优先级：

```text
1. Front Matter
2. Fenced Code Block
3. HTML Block
4. ATX Heading
5. Thematic Break
6. BlockQuote
7. List
8. Table
9. Paragraph
```

原因：

```text
代码块、HTML 块这类多行结构需要优先处理，否则容易被误解析成段落。
```

---

### 6.3 Inline Parser

作用：

```text
解析段落、标题、列表项等块内部的行内 Markdown 语法。
```

行内结构包括：

```text
Text
Emphasis
Strong
InlineCode
Link
Image
HtmlInline
SoftBreak
LineBreak
Strikethrough
AutoLink
```

第一阶段必须支持：

```text
1. 普通文本
2. 加粗
3. 斜体
4. 行内代码
5. 链接
6. 图片
7. HTML inline
8. 软换行
9. 硬换行
```

第二阶段再支持：

```text
1. 删除线
2. 自动链接
3. 脚注引用
4. 行内数学公式
```

---

### 6.4 AST 模块

AST 是项目核心。

所有解析结果都应该先进入 AST，而不是直接渲染 HTML。

原因：

```text
1. AST 可以复用到 HTML 渲染
2. AST 可以生成 Outline
3. AST 可以做字数统计
4. AST 可以支持同步滚动
5. AST 可以为后续所见即所得做源码位置映射
6. AST 可以支持其他格式导出
```

核心数据结构：

```cpp
enum class NodeType {
    Document,

    FrontMatter,

    Heading,
    Paragraph,
    BlockQuote,
    List,
    ListItem,
    CodeBlock,
    ThematicBreak,
    HtmlBlock,

    Table,
    TableHead,
    TableBody,
    TableRow,
    TableCell,

    Text,
    Emphasis,
    Strong,
    InlineCode,
    Link,
    Image,
    HtmlInline,
    SoftBreak,
    LineBreak
};

struct SourceRange {
    std::size_t startOffset = 0;
    std::size_t endOffset = 0;

    int startLine = 0;
    int endLine = 0;

    int startColumn = 0;
    int endColumn = 0;
};

struct Node {
    NodeType type;
    SourceRange range;

    std::string literal;
    std::map<std::string, std::string> attrs;

    std::vector<std::unique_ptr<Node>> children;
};
```

---

### 6.5 HTML Renderer

作用：

```text
把 AST 渲染成 HTML。
```

HTML Renderer 不应该重新解析 Markdown，只接受 AST。

接口示例：

```cpp
class HtmlRenderer {
public:
    std::string render(const Node& document, const RenderOptions& options);
};
```

渲染规则示例：

```text
Heading     → h1/h2/h3/h4/h5/h6
Paragraph   → p
Strong      → strong
Emphasis    → em
InlineCode  → code
CodeBlock   → pre > code
BlockQuote  → blockquote
List        → ul/ol
ListItem    → li
Link        → a
Image       → img
HtmlBlock   → 原样输出或过滤后输出
HtmlInline  → 原样输出或过滤后输出
```

---

### 6.6 Theme Manager

作用：

```text
管理主题包、加载主题配置、拼接 CSS、处理主题变量。
```

主题由三部分组成：

```text
1. theme.json
2. content.css
3. code.css
```

主题管理器职责：

```text
1. 查找主题目录
2. 加载 theme.json
3. 加载 content.css
4. 加载 code.css
5. 合并主题变量
6. 生成最终 CSS
7. 支持通过 themeId 选择主题
```

---

### 6.7 CSS Extension Manager

作用：

```text
处理用户自定义 CSS。
```

CSS 来源包括：

```text
1. 内置 core.css
2. 主题 content.css
3. 用户全局 custom.css
4. 工作区 custom.css
5. Markdown 文档 Front Matter 指定的 CSS
6. API 传入的 CSS 字符串
```

CSS 加载优先级：

```text
core.css
    ↓
theme content.css
    ↓
theme code.css
    ↓
global custom.css
    ↓
workspace custom.css
    ↓
document custom.css
    ↓
API custom CSS
```

越后面的优先级越高。

---

### 6.8 HTML Extension Policy

作用：

```text
控制 Markdown 中 HTML 的解析和输出策略。
```

支持三种模式：

```cpp
enum class HtmlPolicy {
    Disabled,
    Safe,
    Trusted
};
```

含义：

```text
Disabled：禁止 HTML，全部转义为普通文本。
Safe：允许安全 HTML，过滤危险标签和危险属性。
Trusted：信任 HTML，基本原样输出。
```

默认使用：

```text
Safe
```

---

### 6.9 Outline Builder

作用：

```text
从 AST 中提取标题结构，生成文档大纲。
```

数据结构：

```cpp
struct OutlineItem {
    int level = 0;
    std::string title;
    std::string id;
    int line = 0;
    std::vector<OutlineItem> children;
};
```

示例 Markdown：

```markdown
# A
## B
### C
## D
```

生成大纲：

```text
A
├─ B
│  └─ C
└─ D
```

---

### 6.10 Word Counter

作用：

```text
基于 AST 提取可见文本，并进行字数统计。
```

统计项：

```cpp
struct WordStats {
    int characters = 0;
    int charactersNoSpaces = 0;
    int cjkCharacters = 0;
    int englishWords = 0;
    int paragraphs = 0;
    int headings = 0;
    int codeLines = 0;
    int images = 0;
    int links = 0;
};
```

默认规则：

```text
1. Markdown 标记符号不计入正文
2. HTML 标签不计入正文
3. HTML 内部可见文本计入正文
4. 中文一个汉字算一个字符
5. 英文按单词统计
6. 代码块默认不计入正文词数，但统计代码行数
7. 图片 alt 文本可配置是否计入
```

---

## 7. Markdown 语法支持计划

### 7.1 v0.1 基础语法

v0.1 必须支持：

````text
1. 标题：# ## ### #### ##### ######
2. 段落
3. 加粗：**text**
4. 斜体：*text*
5. 行内代码：`code`
6. 代码块：```lang
7. 引用：>
8. 无序列表：- item
9. 有序列表：1. item
10. 分割线：--- / *** / ___
11. 链接：[text](url)
12. 图片：![alt](url)
13. HTML inline
14. HTML block
15. 软换行
16. 硬换行
````

---

### 7.2 v0.2 扩展语法

v0.2 支持：

```text
1. GFM 表格
2. 任务列表
3. 删除线
4. 自动链接
5. 标题自动生成 id
6. Front Matter 简单解析
7. 代码块语言标记
```

---

### 7.3 v0.3 高级语法

v0.3 支持：

```text
1. 脚注
2. 定义列表
3. 数学公式占位节点
4. Mermaid 占位节点
5. Callout 块
6. 自定义容器块
```

---

## 8. HTML 输出规范

### 8.1 输出模式

支持两种输出模式：

```text
1. Fragment 模式
2. Full Document 模式
```

---

### 8.2 Fragment 模式

Fragment 模式只输出 Markdown 内容主体。

示例：

```html
<article class="mw-document markdown-body">
  <h1 id="hello" data-source-line="1">Hello</h1>
  <p data-source-line="3">This is <strong>Markdown</strong>.</p>
</article>
```

适合后续嵌入 Qt 的 QWebEngineView。

---

### 8.3 Full Document 模式

Full Document 模式输出完整 HTML 文档。

示例：

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Markdown Document</title>
  <style>
    /* final css */
  </style>
</head>
<body>
  <article class="mw-document markdown-body">
    ...
  </article>
</body>
</html>
```

适合导出 HTML 文件。

---

### 8.4 HTML class 命名规范

所有自定义 class 使用 `mw-` 前缀。

```text
mw-document
mw-heading
mw-paragraph
mw-blockquote
mw-list
mw-list-item
mw-code-block
mw-inline-code
mw-table
mw-table-wrapper
mw-html-block
mw-image
mw-link
mw-task-list
mw-task-list-item
```

---

### 8.5 data 属性规范

为了后续编辑器同步，块级元素应保留源代码信息。

示例：

```html
<h1 id="title" data-source-line="1" data-source-start="0" data-source-end="7">
  Title
</h1>
```

推荐属性：

```text
data-source-line
data-source-start
data-source-end
data-node-type
```

---

## 9. 主题系统设计

### 9.1 主题包结构

主题包目录：

```text
themes/
└─ github-light/
   ├─ theme.json
   ├─ content.css
   ├─ code.css
   └─ assets/
      ├─ fonts/
      └─ images/
```

---

### 9.2 theme.json 规范

示例：

```json
{
  "id": "github-light",
  "name": "GitHub Light",
  "version": "1.0.0",
  "type": "light",
  "author": "MWRender",
  "description": "A GitHub-like light theme for Markdown documents.",

  "entry": {
    "content": "content.css",
    "code": "code.css"
  },

  "variables": {
    "color.background": "#ffffff",
    "color.foreground": "#24292f",
    "color.muted": "#57606a",
    "color.border": "#d0d7de",
    "color.accent": "#0969da",
    "color.codeBackground": "#f6f8fa",
    "color.blockquoteBorder": "#d0d7de",
    "color.tableBorder": "#d0d7de",

    "font.body": "-apple-system, BlinkMacSystemFont, Segoe UI, Helvetica, Arial, sans-serif",
    "font.code": "SFMono-Regular, Consolas, Liberation Mono, Menlo, monospace",

    "layout.maxWidth": "980px",
    "layout.padding": "45px",
    "layout.lineHeight": "1.6"
  }
}
```

---

### 9.3 CSS 变量规范

渲染器会把 theme.json 中的变量转换成 CSS 变量。

示例：

```css
:root {
  --mw-color-bg: #ffffff;
  --mw-color-fg: #24292f;
  --mw-color-muted: #57606a;
  --mw-color-border: #d0d7de;
  --mw-color-accent: #0969da;
  --mw-color-code-bg: #f6f8fa;

  --mw-font-body: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  --mw-font-code: SFMono-Regular, Consolas, monospace;

  --mw-layout-max-width: 980px;
  --mw-layout-padding: 45px;
  --mw-line-height: 1.6;
}
```

---

### 9.4 默认 GitHub Light 风格主题

默认主题目标：

```text
1. 白色背景
2. 深灰正文
3. 蓝色链接
4. 标题下边框
5. 浅灰代码块背景
6. 表格边框
7. 引用左边框
8. 页面内容居中
9. 最大宽度 980px
10. 代码使用等宽字体
```

---

## 10. HTML 扩展设计

### 10.1 HTML 模式

配置：

```cpp
enum class HtmlPolicy {
    Disabled,
    Safe,
    Trusted
};
```

---

### 10.2 Disabled 模式

Markdown 输入：

```markdown
<div>Hello</div>
```

输出：

```html
<p>&lt;div&gt;Hello&lt;/div&gt;</p>
```

---

### 10.3 Safe 模式

允许常见安全标签：

```text
div
span
p
br
hr
table
thead
tbody
tr
td
th
details
summary
kbd
mark
sup
sub
section
article
aside
header
footer
```

禁止危险标签：

```text
script
iframe
object
embed
link
meta
base
form
input
button
textarea
select
style
```

禁止危险属性：

```text
onclick
onload
onerror
onmouseover
onfocus
onblur
style 中的危险 url
href="javascript:..."
src="javascript:..."
```

---

### 10.4 Trusted 模式

Trusted 模式适合本地可信文档。

处理方式：

```text
1. HTML block 原样输出
2. HTML inline 原样输出
3. 不过滤 style
4. 不过滤 script
```

默认不建议开启 Trusted。

---

## 11. CSS 扩展设计

### 11.1 CSS 来源

支持 CSS 来源：

```text
1. 内置基础 CSS
2. 主题 CSS
3. 用户全局 CSS
4. 工作区 CSS
5. 文档 Front Matter CSS
6. API 传入 CSS
```

---

### 11.2 CSS 加载顺序

```text
core.css
theme/content.css
theme/code.css
global/custom.css
workspace/custom.css
document/custom.css
api/custom.css
```

后加载的 CSS 覆盖前面的 CSS。

---

### 11.3 Front Matter 指定 CSS

Markdown 示例：

```markdown
---
title: Test
theme: github-light
css:
  - ./note.css
  - ./print.css
---

# Hello
```

第一阶段可以只实现简单解析：

```text
1. 检测文档开头是否为 ---
2. 读取到第二个 ---
3. 提取 theme 字段
4. 提取 css 列表
5. 不做复杂 YAML 解析
```

---

### 11.4 CSS 安全选项

配置结构：

```cpp
struct CssPolicy {
    bool allowCustomCss = true;
    bool allowExternalCss = false;
    bool allowRemoteUrl = false;
    bool allowInlineStyle = true;
};
```

默认：

```text
允许本地 CSS
允许主题 CSS
允许 API 注入 CSS
禁止远程 CSS
禁止远程 url()
```

---

## 12. C++ API 设计

### 12.1 RenderOptions

```cpp
namespace mwrender {

enum class OutputMode {
    Fragment,
    FullDocument
};

enum class HtmlPolicy {
    Disabled,
    Safe,
    Trusted
};

struct RenderOptions {
    OutputMode outputMode = OutputMode::FullDocument;

    std::string themeId = "github-light";
    std::string language = "zh-CN";
    std::string title = "Markdown Document";

    bool includeCss = true;
    bool enableHtml = true;
    bool enableCssExtension = true;
    bool enableSourceMap = true;

    HtmlPolicy htmlPolicy = HtmlPolicy::Safe;

    std::vector<std::filesystem::path> customCssFiles;
    std::vector<std::string> customCssText;
};

}
```

---

### 12.2 RenderResult

```cpp
namespace mwrender {

struct RenderWarning {
    int line = 0;
    int column = 0;
    std::string message;
};

struct RenderError {
    int line = 0;
    int column = 0;
    std::string message;
};

struct RenderResult {
    bool ok = false;

    std::string html;
    std::string css;

    std::vector<RenderWarning> warnings;
    std::optional<RenderError> error;
};

}
```

---

### 12.3 Renderer

```cpp
namespace mwrender {

class Renderer {
public:
    Renderer();
    explicit Renderer(RenderOptions options);

    void setOptions(const RenderOptions& options);
    void setTheme(const std::string& themeId);

    void addThemePath(const std::filesystem::path& path);
    void addCustomCssFile(const std::filesystem::path& path);
    void addCustomCssText(const std::string& css);

    RenderResult render(const std::string& markdown);
    RenderResult renderFile(const std::filesystem::path& inputPath);
    bool renderFileToFile(const std::filesystem::path& inputPath,
                          const std::filesystem::path& outputPath);
};

}
```

---

### 12.4 Parser

```cpp
namespace mwrender {

struct ParseResult {
    bool ok = false;
    std::unique_ptr<Node> document;
    std::vector<RenderWarning> warnings;
    std::optional<RenderError> error;
};

class Parser {
public:
    ParseResult parse(const std::string& markdown);
};

}
```

---

### 12.5 ThemeManager

```cpp
namespace mwrender {

struct ThemeInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string type;
    std::string description;
};

class ThemeManager {
public:
    void addThemePath(const std::filesystem::path& path);

    std::vector<ThemeInfo> listThemes() const;
    std::optional<ThemeInfo> findTheme(const std::string& id) const;

    std::string buildThemeCss(const std::string& id) const;
};

}
```

---

## 13. CLI 设计

### 13.1 基础命令

```bash
mwrender input.md -o output.html
```

---

### 13.2 指定主题

```bash
mwrender input.md -o output.html --theme github-light
```

---

### 13.3 输出片段

```bash
mwrender input.md --fragment
```

---

### 13.4 禁用 HTML

```bash
mwrender input.md -o output.html --no-html
```

---

### 13.5 设置 HTML 策略

```bash
mwrender input.md -o output.html --html-policy safe
mwrender input.md -o output.html --html-policy trusted
mwrender input.md -o output.html --html-policy disabled
```

---

### 13.6 添加 CSS

```bash
mwrender input.md -o output.html --css custom.css
```

---

### 13.7 指定主题目录

```bash
mwrender input.md -o output.html --theme-path ./themes
```

---

### 13.8 列出主题

```bash
mwrender --list-themes
```

---

### 13.9 输出 AST 调试信息

```bash
mwrender input.md --dump-ast
```

示例输出：

```text
Document
├─ Heading(level=1)
│  └─ Text("Hello")
└─ Paragraph
   ├─ Text("This is ")
   ├─ Strong
   │  └─ Text("Markdown")
   └─ Text(".")
```

---

## 14. 配置文件设计

支持 `mwrender.json`。

示例：

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
    "policy": "safe"
  },

  "css": {
    "enabled": true,
    "allowRemoteUrl": false,
    "files": [
      "./custom.css"
    ]
  },

  "sourceMap": {
    "enabled": true
  }
}
```

命令行使用：

```bash
mwrender input.md -o output.html --config mwrender.json
```

命令行参数优先级高于配置文件。

优先级：

```text
默认配置
    ↓
mwrender.json
    ↓
Front Matter
    ↓
命令行参数
```

---

## 15. 渲染流程详细设计

### 15.1 完整流程

```text
1. 读取 Markdown 文本
2. 读取 RenderOptions
3. 处理 Front Matter
4. Line Scanner 切分文本
5. Block Parser 生成块级 AST
6. Inline Parser 解析行内 AST
7. 生成 Outline
8. 生成 WordStats
9. HTML Renderer 渲染 HTML 片段
10. ThemeManager 加载主题 CSS
11. CSS Extension Manager 合并自定义 CSS
12. 根据 OutputMode 输出 Fragment 或 Full Document
```

---

### 15.2 错误处理

渲染器不应该因为小语法错误直接失败。

大多数 Markdown 语法错误应该降级处理。

例如：

````markdown
```cpp
int main() {
````

缺少结束代码围栏。

处理方式：

```text
1. 生成 warning
2. 把后续内容按代码块处理到文档末尾
3. 不直接崩溃
```

错误分为：

```text
Fatal Error：无法读取文件、主题不存在且无 fallback、内存错误
Warning：Markdown 语法不完整、HTML 被过滤、CSS 文件不存在
```

---

## 16. HTML 转义规则

所有普通文本必须转义：

```text
&  → &amp;
<  → &lt;
>  → &gt;
"  → &quot;
'  → &#39;
```

行内代码和代码块也需要转义 HTML 特殊字符。

示例：

Markdown：

```markdown
`<div>Hello</div>`
```

HTML：

```html
<code>&lt;div&gt;Hello&lt;/div&gt;</code>
```

---

## 17. 链接和图片安全

链接需要检查协议。

允许：

```text
http:
https:
mailto:
file: 可配置
相对路径
锚点 #
```

禁止：

```text
javascript:
vbscript:
data: 默认禁止，图片 data:image 可配置
```

图片 `src` 同样需要检查。

默认策略：

```text
允许相对路径
允许 http/https
禁止 javascript
禁止不安全 data
```

---

## 18. Slug 生成规则

标题自动生成 id。

示例：

```markdown
# Hello World
```

输出：

```html
<h1 id="hello-world">Hello World</h1>
```

规则：

```text
1. 转小写
2. 去除首尾空白
3. 空格替换为 -
4. 去除大部分标点符号
5. 中文保留
6. 重复标题追加 -1、-2、-3
```

示例：

```markdown
# 安装
# 安装
# 安装
```

输出：

```html
<h1 id="安装">安装</h1>
<h1 id="安装-1">安装</h1>
<h1 id="安装-2">安装</h1>
```

---

## 19. 测试方案

### 19.1 单元测试

测试模块：

```text
1. LineScanner
2. BlockParser
3. InlineParser
4. HtmlRenderer
5. ThemeManager
6. CssBuilder
7. HtmlSanitizer
8. OutlineBuilder
9. WordCounter
```

---

### 19.2 Snapshot 测试

输入 Markdown，比较输出 HTML。

示例：

输入：

```markdown
# Hello

This is **Markdown**.
```

期望：

```html
<article class="mw-document markdown-body">
<h1 id="hello" data-source-line="1">Hello</h1>
<p data-source-line="3">This is <strong>Markdown</strong>.</p>
</article>
```

---

### 19.3 安全测试

输入：

```html
<script>alert(1)</script>
```

Safe 模式下不应该输出 script。

输入：

```html
<a href="javascript:alert(1)">click</a>
```

Safe 模式下应该移除危险 href 或转义整个标签。

---

### 19.4 主题测试

测试：

```text
1. 默认主题是否能加载
2. 指定主题是否能加载
3. 主题不存在是否回退
4. content.css 是否合并成功
5. 变量是否成功转换成 CSS 变量
6. 自定义 CSS 是否覆盖主题 CSS
```

---

### 19.5 性能测试

测试文档：

```text
1. 10KB Markdown
2. 100KB Markdown
3. 1MB Markdown
4. 5MB Markdown
```

指标：

```text
1. 解析耗时
2. 渲染耗时
3. 内存占用
4. 输出 HTML 大小
```

初期目标：

```text
100KB 文档解析和渲染应保持明显流畅。
1MB 文档应可以接受。
```

---

## 20. 开发阶段规划

### Phase 0：项目初始化

任务：

```text
1. 创建 Git 仓库
2. 配置 CMake
3. 配置 include/src/tests 目录
4. 添加基础 CLI
5. 添加 README
6. 添加测试框架
```

验收标准：

```text
1. 项目可以成功编译
2. CLI 可以输出版本号
3. 测试框架可以运行
```

---

### Phase 1：基础 AST 和 Parser

任务：

```text
1. 实现 LineScanner
2. 实现 Node / AST
3. 实现 Heading Parser
4. 实现 Paragraph Parser
5. 实现 ThematicBreak Parser
6. 实现 CodeBlock Parser
7. 实现基础 AST 打印
```

验收标准：

```text
输入 Markdown 能生成基础 AST。
```

---

### Phase 2：Inline Parser

任务：

```text
1. Text
2. Strong
3. Emphasis
4. InlineCode
5. Link
6. Image
7. HtmlInline
8. SoftBreak
9. LineBreak
```

验收标准：

```text
段落和标题内部的行内语法可以正确解析。
```

---

### Phase 3：HTML Renderer

任务：

```text
1. 实现 HtmlEscape
2. 实现 Heading 渲染
3. 实现 Paragraph 渲染
4. 实现 Strong / Emphasis 渲染
5. 实现 CodeBlock 渲染
6. 实现 Link / Image 渲染
7. 实现 Fragment 输出
8. 实现 Full Document 输出
```

验收标准：

```text
Markdown 可以转换成完整 HTML。
```

---

### Phase 4：主题系统

任务：

```text
1. 设计 theme.json
2. 实现 ThemeManager
3. 实现 CssBuilder
4. 添加 github-light 默认主题
5. 支持 --theme 参数
6. 支持 --list-themes
```

验收标准：

```text
可以使用不同主题渲染 HTML。
```

---

### Phase 5：HTML 和 CSS 扩展

任务：

```text
1. 实现 HtmlPolicy
2. 实现 Safe HTML 过滤
3. 实现 Trusted HTML 输出
4. 实现 Disabled HTML 转义
5. 支持 --css 参数
6. 支持 Front Matter 指定 CSS
```

验收标准：

```text
HTML 和 CSS 扩展可控、安全、可配置。
```

---

### Phase 6：GFM 基础扩展

任务：

```text
1. 表格
2. 任务列表
3. 删除线
4. 自动链接
5. 代码块语言 class
```

验收标准：

```text
常见 GitHub 风格 Markdown 能较好渲染。
```

---

### Phase 7：分析能力

任务：

```text
1. OutlineBuilder
2. WordCounter
3. Heading slug
4. SourceRange 完善
5. data-source-line 输出
```

验收标准：

```text
渲染结果可以支持后续编辑器的 Outline、字数统计和同步滚动。
```

---

## 21. 最小可行版本

v0.1 最小可行版本功能：

```text
1. Markdown 文件输入
2. HTML 文件输出
3. 支持标题、段落、加粗、斜体、链接、图片、代码块、引用、列表
4. 支持 HTML block / inline
5. 支持默认 github-light 主题
6. 支持 CLI
7. 支持 Full Document 输出
8. 支持 Fragment 输出
```

v0.1 命令示例：

```bash
mwrender README.md -o README.html --theme github-light
```

---

## 22. 代码风格规范

### 22.1 命名规范

```text
类名：PascalCase
函数名：camelCase
变量名：camelCase
枚举名：PascalCase
枚举值：PascalCase
文件名：snake_case
命名空间：mwrender
```

示例：

```cpp
namespace mwrender {

class HtmlRenderer {
public:
    std::string renderDocument(const Node& node);
};

}
```

---

### 22.2 头文件规范

每个公开头文件放在：

```text
include/mwrender/
```

内部实现头文件放在：

```text
src/
```

公开 API 尽量保持稳定。

内部 Parser 可以频繁修改。

---

### 22.3 错误处理规范

不建议在普通解析错误中大量使用异常。

推荐：

```text
1. 文件读取失败：返回 RenderError
2. 主题不存在：warning + fallback
3. Markdown 语法不完整：warning
4. 内部严重错误：异常或 fatal error
```

---

## 23. 和后续 Qt 编辑器的关系

MWRender 后续会被 Qt Markdown 编辑器调用。

Qt 编辑器中：

```text
QPlainTextEdit 输入 Markdown
    ↓
调用 MWRender::Renderer
    ↓
得到 HTML
    ↓
QWebEngineView 显示
```

后续所见即所得模式需要：

```text
1. AST
2. SourceRange
3. data-source-line
4. Outline
5. WordStats
6. 局部渲染接口
```

所以当前项目必须从一开始保留源码位置信息。

---

## 24. 后续扩展方向

### 24.1 Mermaid

后续可增加：

````text
```mermaid
graph TD
A --> B
````

````

解析成：

```text
MermaidBlock
````

HTML 输出：

```html
<div class="mw-mermaid">graph TD...</div>
```

---

### 24.2 LaTeX

后续可增加：

```text
MathInline
MathBlock
```

HTML 输出：

```html
<span class="mw-math-inline">...</span>
<div class="mw-math-block">...</div>
```

渲染可交给 KaTeX 或 MathJax。

---

### 24.3 Pandoc 集成

后续 Markdown 编辑器可以把 MWRender 的主题系统扩展到 Pandoc 导出。

思路：

```text
主题包
├─ content.css
├─ reference.docx
├─ template.tex
└─ export.yaml
```

MWRender 当前只负责 HTML 渲染，不负责 Pandoc。

---

### 24.4 WYSIWYG 编辑器

后续所见即所得模式可基于 AST 和 SourceRange 实现：

```text
当前块源码编辑
其他块渲染显示
光标移动时切换块状态
```

当前项目只需要为这个目标预留数据结构。

---

## 25. 项目验收标准

项目初期完成后，应满足：

```text
1. 可以作为命令行工具使用
2. 可以作为 C++ 库使用
3. 可以把 Markdown 渲染成 HTML
4. 可以选择默认主题
5. 可以加载自定义 CSS
6. 可以控制 HTML 是否允许
7. 可以输出完整 HTML
8. 可以输出 HTML 片段
9. 有基础单元测试
10. 有主题规范文档
11. 有 API 文档
12. 有示例 Markdown 文件
```

---

## 26. 开发优先级总结

最优先：

```text
1. AST
2. Block Parser
3. Inline Parser
4. HTML Renderer
5. 默认主题
6. CLI
```

第二优先：

```text
1. HTML 安全策略
2. CSS 扩展
3. 主题选择接口
4. Snapshot 测试
```

第三优先：

```text
1. 表格
2. 任务列表
3. Outline
4. 字数统计
5. SourceRange 完善
```

最后再考虑：

```text
1. Mermaid
2. LaTeX
3. Pandoc
4. 所见即所得
5. Qt 编辑器集成
```

---

## 27. 一句话总结

MWRender 的第一阶段目标不是做一个完整 Markdown 编辑器，而是做一个可靠、可测试、可扩展的 Markdown 渲染核心。

它应该完成：

```text
Markdown → AST → HTML → Theme CSS → Final HTML
```

并且为后续 Qt 编辑器、主题系统、预览模式、所见即所得模式和文档导出系统提供基础。
