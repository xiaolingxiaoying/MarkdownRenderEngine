下面这份可以作为你这个独立项目的 **设计报告 / README 初稿**。这个项目建议先叫：

```text id="38d0xt"
Markdown Render Engine
```

或者更具体一点：

```text id="73uzk6"
MWRender
```

定位是：**一个自研 Markdown → HTML 渲染器，支持 HTML 扩展、CSS 扩展、主题系统，默认提供 GitHub Light 风格主题，并为后续 Qt Markdown 编辑器复用。**

---

# Markdown Render Engine 设计报告

## 1. 项目背景

Markdown 编辑器的核心能力不是界面，而是文档渲染能力。一个稳定的 Markdown 软件应该先具备独立的渲染引擎：能够把 Markdown 源文本解析成结构化文档模型，再渲染为 HTML，并通过 CSS 控制最终样式。

本项目的目标是先实现一个独立的 Markdown 渲染器，为后续 Markdown 编辑器、预览模式、导出 HTML、主题系统、所见即所得编辑器提供基础。

CommonMark 的目标是为 Markdown 提供明确、可测试的规范，减少不同解析器之间的歧义；GitHub Flavored Markdown 在 CommonMark 基础上增加了表格、任务列表、删除线、自动链接等常用扩展。因此本项目建议第一阶段以 CommonMark 子集为基础，第二阶段逐步扩展 GFM 能力。([CommonMark][1])

---

## 2. 项目定位

本项目不是完整 Markdown 编辑器，而是一个 **Markdown 渲染核心库**。

它负责：

```text id="432rfl"
Markdown 源码
  ↓
词法 / 行扫描
  ↓
块级解析
  ↓
行内解析
  ↓
AST 文档树
  ↓
HTML 渲染
  ↓
主题 CSS 注入
  ↓
完整 HTML 文档输出
```

后续 Qt 软件只需要调用这个渲染器，就可以得到可显示的 HTML。

---

## 3. 项目目标

### 3.1 核心目标

```text id="h6knyu"
1. 自研 Markdown 解析器。
2. 支持 Markdown → AST。
3. 支持 AST → HTML。
4. 支持 HTML 扩展。
5. 支持 CSS 扩展。
6. 支持主题系统。
7. 默认提供 GitHub Light 风格主题。
8. 提供清晰的 C++ API 和 CLI 接口。
9. 为后续 Qt 预览、导出、WYSIWYG 提供基础。
```

---

### 3.2 非目标

第一阶段不要做这些：

```text id="miwtol"
1. 不做完整 Markdown 编辑器。
2. 不做所见即所得编辑。
3. 不做 Pandoc 多格式转换。
4. 不做 PDF / DOCX 导出。
5. 不做插件市场。
6. 不做完整 LaTeX 引擎。
7. 不追求 100% CommonMark 兼容。
```

这几个可以作为后续版本继续做。

---

# 4. 技术路线

推荐技术栈：

```text id="4o9m1t"
语言：C++20
构建系统：CMake
测试框架：Catch2 / GoogleTest
配置格式：JSON
样式格式：CSS
输出格式：HTML5
默认主题：GitHub Light 风格
```

项目初期不要依赖 Qt。
原因是这个项目应该是一个纯 C++ 渲染库，后面 Qt 编辑器只是调用它。

也就是说：

```text id="dzxnbq"
MWRender Core：不依赖 Qt
Qt Markdown App：依赖 MWRender Core
```

这样架构更干净。

---

# 5. 整体架构

```text id="9zl5ez"
mwrender/
├─ include/
│  └─ mwrender/
│     ├─ renderer.hpp
│     ├─ parser.hpp
│     ├─ ast.hpp
│     ├─ theme.hpp
│     ├─ options.hpp
│     └─ version.hpp
│
├─ src/
│  ├─ lexer/
│  ├─ parser/
│  ├─ ast/
│  ├─ html/
│  ├─ theme/
│  ├─ utils/
│  └─ cli/
│
├─ themes/
│  └─ github-light/
│     ├─ theme.json
│     ├─ content.css
│     └─ preview.css
│
├─ examples/
│  ├─ basic.md
│  ├─ html-extension.md
│  └─ custom-theme.md
│
├─ tests/
│  ├─ parser/
│  ├─ html/
│  ├─ theme/
│  └─ snapshot/
│
├─ docs/
│  ├─ theme-spec.md
│  ├─ html-output-spec.md
│  └─ api.md
│
├─ CMakeLists.txt
└─ README.md
```

---

# 6. 渲染流程设计

## 6.1 输入

输入是 Markdown 源文本：

````markdown id="r2sjn9"
# Hello

This is **Markdown**.

```cpp
int main() {
    return 0;
}
````

````

---

## 6.2 解析阶段

解析阶段分成两步：

```text id="7xv6i4"
1. Block Parser：解析块级结构
2. Inline Parser：解析行内结构
````

块级结构包括：

```text id="99mf2n"
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
```

行内结构包括：

```text id="7yozio"
Text
Emphasis
Strong
InlineCode
Link
Image
HtmlInline
LineBreak
SoftBreak
```

---

## 6.3 AST 文档模型

AST 是整个项目的核心。

建议设计：

```cpp id="eatqmd"
enum class NodeType {
    Document,

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

`SourceRange` 非常重要。
后续你做：

```text id="zsf1sd"
源码跳转
同步滚动
Outline
错误定位
所见即所得块级映射
```

都会依赖它。

---

## 6.4 HTML 渲染阶段

HTML Renderer 负责把 AST 转换成 HTML。

例如：

```markdown id="5ra35u"
# Title

Hello **world**.
```

输出：

```html id="xkt9t2"
<article class="mw-document markdown-body">
  <h1 data-source-line="1" id="title">Title</h1>
  <p data-source-line="3">Hello <strong>world</strong>.</p>
</article>
```

注意这里有几个固定规则：

```text id="nqhrl2"
1. 最外层统一使用 <article class="mw-document markdown-body">
2. 每个块级节点尽量带 data-source-line
3. 标题节点生成 id，方便 Outline 和锚点跳转
4. 代码块带 data-lang
5. HTML 扩展可以按策略原样输出
```

GitHub Markdown CSS 这类样式通常要求把渲染后的 Markdown 内容放在 `.markdown-body` 容器中，所以本项目也建议保留 `markdown-body` 这个 class，同时额外增加自己的 `mw-document` class。([GitHub][2])

---

# 7. HTML 输出规范

## 7.1 完整 HTML 文档

渲染器应该支持两种输出：

```text id="y1rqu2"
1. Fragment 模式：只输出 <article>...</article>
2. Full Document 模式：输出完整 HTML 文档
```

### Fragment 模式

```html id="xewof7"
<article class="mw-document markdown-body">
  ...
</article>
```

适合嵌入 Qt `QWebEngineView`。

---

### Full Document 模式

```html id="qwgl4c"
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Markdown Document</title>
  <style>
    /* theme css */
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

## 7.2 固定 class 命名规范

统一使用 `mw-` 前缀，避免和用户 CSS 冲突。

```text id="8beusf"
mw-document
mw-heading
mw-paragraph
mw-code-block
mw-code-lang
mw-blockquote
mw-table
mw-table-wrapper
mw-task-list
mw-task-list-item
mw-html-block
mw-inline-code
mw-link
mw-image
```

示例：

```html id="z8q1h8"
<pre class="mw-code-block" data-lang="cpp" data-source-line="10">
  <code class="language-cpp">int main() { return 0; }</code>
</pre>
```

---

# 8. Markdown 语法支持范围

## 8.1 v0.1 必须支持

````text id="34g49i"
1. ATX 标题：# ## ###
2. 段落
3. 加粗：**text**
4. 斜体：*text*
5. 行内代码：`code`
6. fenced code block：```lang
7. 引用：>
8. 无序列表：- / * / +
9. 有序列表：1.
10. 分割线：---
11. 链接：[text](url)
12. 图片：![alt](url)
13. 软换行和硬换行
14. HTML inline
15. HTML block
````

---

## 8.2 v0.2 支持

```text id="2gyk1e"
1. GFM 表格
2. GFM 任务列表
3. 删除线
4. 自动链接
5. 脚注
6. 代码块基础高亮接口
```

GFM 在 CommonMark 基础上增加了表格、任务列表、删除线、自动链接等扩展，因此这些适合作为第二阶段扩展，而不是第一版就强行全部完成。([GitHub][3])

---

# 9. HTML 扩展设计

你希望支持 HTML 扩展，这是正确的。
但是要分成三种策略。

## 9.1 HTML 支持级别

```cpp id="vggt88"
enum class HtmlPolicy {
    Disabled,       // 不允许 HTML，全部转义
    Safe,           // 允许安全 HTML，过滤 script/style/event handler
    Trusted         // 完全信任，原样输出
};
```

---

## 9.2 Disabled 模式

Markdown：

```markdown id="5dum6m"
<div>Hello</div>
```

输出：

```html id="klcrux"
<p>&lt;div&gt;Hello&lt;/div&gt;</p>
```

适合安全模式。

---

## 9.3 Safe 模式

允许：

```text id="s8j468"
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
```

禁止：

```text id="u65kl5"
script
iframe
object
embed
link
meta
style
onload
onclick
onerror
javascript:
```

---

## 9.4 Trusted 模式

直接原样输出 HTML。

适合本地 Markdown 编辑器，但导出或打开陌生文件时要谨慎。

---

# 10. CSS 扩展设计

CSS 扩展要有明确的加载顺序。

建议顺序：

```text id="snloqk"
1. core.css
2. theme content.css
3. user global custom.css
4. workspace custom.css
5. document custom.css
```

后面的优先级更高。

---

## 10.1 CSS 扩展来源

支持这些来源：

```text id="h80fjq"
1. 渲染器默认 CSS
2. 主题 CSS
3. 用户指定 CSS 文件
4. Markdown Front Matter 指定 CSS
5. API 直接传入 CSS 字符串
```

---

## 10.2 Front Matter 示例

```markdown id="322kv8"
---
title: Test
theme: github-light
css:
  - ./custom.css
---

# Hello
```

第一阶段可以先不完整支持 YAML，只做简单提取：

```text id="73m31j"
如果文档开头是 --- 到 ---，先作为 metadata block 保存
```

后期再接入 YAML 解析库。

---

## 10.3 CSS 安全策略

CSS 本身也有风险，例如远程资源、字体、背景图。

建议提供：

```cpp id="m8j4pw"
struct CssPolicy {
    bool allowExternalCss = false;
    bool allowRemoteUrl = false;
    bool allowInlineStyle = true;
    bool allowDocumentCss = true;
};
```

默认策略：

```text id="84mbhr"
允许主题 CSS
允许本地 CSS
允许 inline style
禁止远程 CSS
禁止远程 url()
```

---

# 11. 主题系统设计

主题系统是这个项目的重点之一。

主题不是简单 CSS 文件，而是一个主题包。

## 11.1 主题目录结构

```text id="nmq958"
themes/
└─ github-light/
   ├─ theme.json
   ├─ content.css
   ├─ code.css
   └─ assets/
      └─ fonts/
```

---

## 11.2 theme.json 规范

```json id="62zqep"
{
  "id": "github-light",
  "name": "GitHub Light",
  "version": "1.0.0",
  "type": "light",
  "author": "Markdown Render Engine",
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

GitHub Markdown CSS 的常见用法是给 Markdown 容器设置 `.markdown-body`，并建议类似 GitHub 的 980px 最大宽度和页面 padding。本项目默认主题可以参考这种布局思想，但不应该直接把外部 CSS 当成自己的不可控核心。([GitHub][2])

---

## 11.3 主题变量规范

建议统一使用这些变量：

```css id="kkbxfr"
:root {
  --mw-color-bg: #ffffff;
  --mw-color-fg: #24292f;
  --mw-color-muted: #57606a;
  --mw-color-border: #d0d7de;
  --mw-color-accent: #0969da;
  --mw-color-code-bg: #f6f8fa;

  --mw-font-body: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  --mw-font-code: "SFMono-Regular", Consolas, monospace;

  --mw-layout-max-width: 980px;
  --mw-layout-padding: 45px;
  --mw-line-height: 1.6;
}
```

然后所有主题 CSS 都围绕这些变量写。

---

## 11.4 content.css 示例

```css id="f8yrzb"
.mw-document {
  box-sizing: border-box;
  max-width: var(--mw-layout-max-width);
  margin: 0 auto;
  padding: var(--mw-layout-padding);
  color: var(--mw-color-fg);
  background: var(--mw-color-bg);
  font-family: var(--mw-font-body);
  line-height: var(--mw-line-height);
}

.mw-document h1,
.mw-document h2,
.mw-document h3,
.mw-document h4,
.mw-document h5,
.mw-document h6 {
  margin-top: 24px;
  margin-bottom: 16px;
  font-weight: 600;
  line-height: 1.25;
}

.mw-document h1,
.mw-document h2 {
  padding-bottom: 0.3em;
  border-bottom: 1px solid var(--mw-color-border);
}

.mw-document a {
  color: var(--mw-color-accent);
  text-decoration: none;
}

.mw-document a:hover {
  text-decoration: underline;
}

.mw-document code {
  padding: 0.2em 0.4em;
  background: var(--mw-color-code-bg);
  border-radius: 6px;
  font-family: var(--mw-font-code);
  font-size: 85%;
}

.mw-document pre {
  padding: 16px;
  overflow: auto;
  background: var(--mw-color-code-bg);
  border-radius: 6px;
}

.mw-document pre code {
  padding: 0;
  background: transparent;
  font-size: 100%;
}

.mw-document blockquote {
  margin: 0;
  padding: 0 1em;
  color: var(--mw-color-muted);
  border-left: 0.25em solid var(--mw-color-border);
}

.mw-document table {
  border-spacing: 0;
  border-collapse: collapse;
}

.mw-document table th,
.mw-document table td {
  padding: 6px 13px;
  border: 1px solid var(--mw-color-border);
}
```

---

# 12. 主题选择接口

主题必须可以通过接口选择。

## 12.1 C++ API 设计

```cpp id="ic0piq"
namespace mwrender {

struct RenderOptions {
    std::string themeId = "github-light";
    bool fullDocument = true;
    bool includeCss = true;
    bool enableHtml = true;
    bool enableCssExtension = true;
    HtmlPolicy htmlPolicy = HtmlPolicy::Safe;
};

class Renderer {
public:
    explicit Renderer(const RenderOptions& options = {});

    RenderResult renderToHtml(const std::string& markdown);
    RenderResult renderToHtmlFile(const std::string& markdown,
                                  const std::filesystem::path& outputPath);

    void setTheme(const std::string& themeId);
    void addThemePath(const std::filesystem::path& path);
    void addCustomCss(const std::string& css);
};

}
```

---

## 12.2 使用示例

```cpp id="v0pm82"
#include <mwrender/renderer.hpp>

int main() {
    mwrender::RenderOptions options;
    options.themeId = "github-light";
    options.fullDocument = true;
    options.enableHtml = true;

    mwrender::Renderer renderer(options);
    renderer.addThemePath("./themes");

    auto result = renderer.renderToHtml("# Hello\n\nThis is **Markdown**.");

    if (result.ok) {
        std::cout << result.html;
    } else {
        std::cerr << result.error.message;
    }
}
```

---

## 12.3 CLI 接口

```bash id="0fwli2"
mwrender input.md -o output.html
```

指定主题：

```bash id="k7svua"
mwrender input.md -o output.html --theme github-light
```

指定外部 CSS：

```bash id="jp5v9m"
mwrender input.md -o output.html --css custom.css
```

只输出 HTML 片段：

```bash id="dfo2v0"
mwrender input.md --fragment
```

禁用 HTML：

```bash id="t5nedc"
mwrender input.md --no-html
```

使用信任模式：

```bash id="t4246p"
mwrender input.md --html-policy trusted
```

---

# 13. 渲染配置规范

可以支持一个配置文件：

```json id="zn1n5z"
{
  "theme": "github-light",
  "html": {
    "policy": "safe",
    "allowInlineHtml": true,
    "allowHtmlBlock": true
  },
  "css": {
    "allowCustomCss": true,
    "allowRemoteUrl": false,
    "files": [
      "./custom.css"
    ]
  },
  "output": {
    "fullDocument": true,
    "includeCss": true,
    "lang": "zh-CN"
  }
}
```

CLI 使用：

```bash id="whchvy"
mwrender input.md -o output.html --config mwrender.json
```

---

# 14. 渲染结果对象

```cpp id="6qqdss"
struct RenderError {
    int line = 0;
    int column = 0;
    std::string message;
};

struct RenderWarning {
    int line = 0;
    int column = 0;
    std::string message;
};

struct RenderResult {
    bool ok = false;
    std::string html;
    std::vector<RenderWarning> warnings;
    std::optional<RenderError> error;
};
```

这样后续 Qt 编辑器可以把错误显示在状态栏或问题面板。

---

# 15. 默认主题：GitHub Light 风格

默认主题不要直接叫“完全复刻 GitHub”。
建议叫：

```text id="opy0om"
github-light
```

描述：

```text id="l8f6xp"
A GitHub-like light theme for Markdown documents.
```

因为你做的是类似 GitHub 风格，而不是 GitHub 官方主题。

默认视觉目标：

```text id="7nhrse"
白色背景
深灰正文
蓝色链接
标题下边框
浅灰代码块背景
表格边框
引用左边框
最大宽度 980px
正文居中显示
```

---

# 16. HTML 扩展输出示例

输入：

```markdown id="pxmh9m"
# HTML Test

<div class="note">
  This is a custom HTML block.
</div>

This is <mark>highlighted</mark>.
```

输出：

```html id="7m05fw"
<article class="mw-document markdown-body">
  <h1 id="html-test" data-source-line="1">HTML Test</h1>

  <div class="mw-html-block" data-source-line="3">
    <div class="note">
      This is a custom HTML block.
    </div>
  </div>

  <p data-source-line="7">
    This is <mark>highlighted</mark>.
  </p>
</article>
```

这里是否包一层 `mw-html-block` 可以配置。

建议默认：

```text id="4m02by"
Safe 模式：包一层，方便控制
Trusted 模式：可以原样输出
```

---

# 17. CSS 扩展示例

Markdown：

```markdown id="w66hzr"
---
css:
  - note.css
---

# Test

<div class="note">
自定义提示块
</div>
```

`note.css`：

```css id="paf75j"
.note {
  padding: 12px 16px;
  border-left: 4px solid #0969da;
  background: #f6f8fa;
  border-radius: 6px;
}
```

输出 HTML 时：

```html id="et4ibg"
<style>
/* core.css */
/* theme content.css */
/* note.css */
</style>
```

或者：

```html id="r6y7mb"
<link rel="stylesheet" href="note.css">
```

建议默认内联 CSS，方便单文件导出。

---

# 18. 解析器设计细节

## 18.1 Line Scanner

先按行扫描：

```cpp id="az5cni"
struct Line {
    int number;
    std::size_t startOffset;
    std::size_t endOffset;
    std::string_view text;
};
```

---

## 18.2 Block Parser

Block Parser 按优先级匹配：

```text id="8cau79"
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

注意 fenced code block 要优先于 paragraph，否则代码块会被误解析。

---

## 18.3 Inline Parser

Inline Parser 处理段落内部：

```text id="9yr5n6"
1. Escape
2. Inline Code
3. Strong / Emphasis
4. Link
5. Image
6. HtmlInline
7. Text
```

第一版可以用相对简单的状态机，不要一上来追求完整 CommonMark 所有边界规则。

---

# 19. 主题管理器设计

```cpp id="k9kzvg"
class Theme {
public:
    std::string id;
    std::string name;
    std::string version;
    std::string type;
    std::filesystem::path rootPath;
    std::string contentCss;
    std::unordered_map<std::string, std::string> variables;
};

class ThemeManager {
public:
    void addThemePath(const std::filesystem::path& path);
    std::vector<ThemeInfo> listThemes() const;
    std::optional<Theme> loadTheme(const std::string& id) const;
    std::string buildCss(const Theme& theme) const;
};
```

主题加载顺序：

```text id="nmfsrs"
1. 内置 themes/
2. 用户指定 theme path
3. 当前工作区 themes/
```

如果主题 ID 重复：

```text id="sfcl6t"
用户主题 > 工作区主题 > 内置主题
```

---

# 20. 测试方案

## 20.1 Parser 测试

输入 Markdown，检查 AST。

```text id="4iwyi2"
# Title
```

期望：

```text id="bj1zzh"
Document
└─ Heading(level=1)
   └─ Text("Title")
```

---

## 20.2 HTML Snapshot 测试

输入 Markdown：

```markdown id="mpkn44"
Hello **world**.
```

期望 HTML：

```html id="yvpa3d"
<p>Hello <strong>world</strong>.</p>
```

---

## 20.3 Theme 测试

检查：

```text id="3at8v5"
1. 能否加载 theme.json
2. content.css 是否存在
3. 变量是否正确注入
4. 主题不存在时是否回退 github-light
```

---

## 20.4 安全测试

检查这些是否被过滤：

```html id="4n95u5"
<script>alert(1)</script>
<img src=x onerror=alert(1)>
<a href="javascript:alert(1)">click</a>
```

---

# 21. 版本规划

## v0.1：基础渲染器

```text id="z4pt77"
1. Markdown → AST
2. AST → HTML
3. 标题、段落、加粗、斜体、链接、图片、代码块
4. HTML inline / block
5. 默认 github-light 主题
6. CLI 工具
7. Fragment / Full Document 输出
```

---

## v0.2：主题与 CSS 扩展完善

```text id="kfocd0"
1. theme.json 规范
2. 多主题加载
3. 自定义 CSS
4. Front Matter 读取 theme/css
5. CSS 加载优先级
6. 安全策略
```

---

## v0.3：GFM 扩展

```text id="nny2d4"
1. 表格
2. 任务列表
3. 删除线
4. 自动链接
5. 基础代码高亮接口
```

---

## v0.4：编辑器集成准备

```text id="kyit14"
1. SourceRange 完善
2. Outline 提取
3. 字数统计
4. 标题 slug 生成
5. 同步滚动支持
6. 局部渲染接口预留
```

---

# 22. 后续给 Qt 编辑器的接口预留

后续 Qt 软件需要：

```text id="mqqr0j"
1. 渲染完整 HTML
2. 渲染 HTML 片段
3. 获取 Outline
4. 获取字数统计
5. 获取源码行号到 HTML block 的映射
6. 获取标题 id
7. 选择主题
8. 加载自定义 CSS
```

所以后期 API 可以扩展为：

```cpp id="bim9oz"
struct OutlineItem {
    int level;
    std::string title;
    std::string id;
    int line;
};

struct WordStats {
    int characters;
    int charactersNoSpaces;
    int cjkCharacters;
    int englishWords;
    int paragraphs;
    int codeLines;
};

struct DocumentResult {
    std::unique_ptr<Node> ast;
    std::string html;
    std::vector<OutlineItem> outline;
    WordStats stats;
};
```

---

# 23. 项目最终边界总结

这个项目应该聚焦一句话：

```text id="w44u2p"
实现一个支持主题系统、HTML 扩展和 CSS 扩展的自研 Markdown → HTML 渲染器。
```

不要一开始做编辑器。
先把这个渲染器做稳定，再把它接入 Qt 软件。

推荐最终项目结构：

```text id="cwjp8n"
MWRender
├─ Markdown Parser
├─ Markdown AST
├─ HTML Renderer
├─ Theme System
├─ CSS Extension System
├─ HTML Extension Policy
├─ CLI Tool
└─ C++ API
```

默认主题：

```text id="q1lpif"
github-light
```

主题接口：

```text id="ngkw56"
theme.json + content.css + variables
```

渲染接口：

```text id="sjqzc2"
Renderer::renderToHtml(markdown, options)
```

CLI 接口：

```text id="dgpabb"
mwrender input.md -o output.html --theme github-light --css custom.css
```

这就是一个非常适合先独立开发的小项目。后面你的 Markdown 编辑器、预览模式、导出 HTML、所见即所得模式、工作区系统，都可以复用它。

[1]: https://commonmark.org/?utm_source=chatgpt.com "CommonMark"
[2]: https://github.com/sindresorhus/github-markdown-css?utm_source=chatgpt.com "sindresorhus/github-markdown-css"
[3]: https://github.github.com/gfm/?utm_source=chatgpt.com "GitHub Flavored Markdown Spec"
