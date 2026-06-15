你现在已经写好了 Markdown 渲染器，接下来不要马上冲 WYSIWYG。应该先把它从“能跑”变成“可测试、可集成、可扩展”的核心库。建议按下面顺序做。

---

# 1. 先确认你的渲染器现在属于哪一层

你现在的渲染器最好已经能完成：

```text
Markdown 输入
  ↓
解析
  ↓
HTML 输出
  ↓
CSS 主题渲染
```

接下来要把它整理成：

```text
Markdown Render Core
├─ Parser
├─ AST
├─ HTML Renderer
├─ Theme System
├─ CLI
├─ C++ API
├─ Test Suite
└─ Docs
```

不要让它只是一个 `main.cpp` 里直接读文件、转 HTML、输出文件的小程序。

---

# 2. 第一件事：补测试

这是最重要的。
Markdown 渲染器很容易“现在看着能用，后面一改全坏”。

你应该先建立测试目录：

```text
tests/
├─ block/
├─ inline/
├─ html/
├─ theme/
├─ security/
└─ snapshot/
```

重点测试这些：

```text
标题
段落
加粗
斜体
行内代码
代码块
引用
列表
链接
图片
HTML inline
HTML block
表格，如果你已经支持
CSS 主题输出
```

例如：

```markdown
# Hello

This is **Markdown**.
```

期望输出：

```html
<h1 id="hello">Hello</h1>
<p>This is <strong>Markdown</strong>.</p>
```

建议你做 **snapshot 测试**：

```text
input.md
expected.html
```

以后每次改解析器，跑测试就知道有没有破坏旧功能。

---

# 3. 第二件事：确认有没有 AST

你现在的渲染器如果是：

```text
Markdown → 直接拼 HTML
```

那后面做编辑器会很难。

最好改成：

```text
Markdown → AST → HTML
```

AST 结构类似：

```cpp
enum class NodeType {
    Document,
    Heading,
    Paragraph,
    Strong,
    Emphasis,
    Text,
    CodeBlock,
    BlockQuote,
    List,
    ListItem,
    Link,
    Image,
    HtmlBlock,
    HtmlInline
};

struct SourceRange {
    int startLine;
    int endLine;
    int startOffset;
    int endOffset;
};

struct Node {
    NodeType type;
    SourceRange range;
    std::string literal;
    std::map<std::string, std::string> attrs;
    std::vector<std::unique_ptr<Node>> children;
};
```

如果你已经有 AST，下一步就是完善 `SourceRange`。

如果你没有 AST，建议你先重构，不要急着做 Qt 软件。

---

# 4. 第三件事：加 SourceRange

这个非常关键。

每个块级节点都应该知道自己来自 Markdown 的哪几行：

```text
# Title     -> line 1
paragraph   -> line 3-5
code block  -> line 7-12
```

HTML 输出时加上：

```html
<h1 data-source-line="1" data-source-start="0" data-source-end="7">
  Title
</h1>
```

这个以后会用于：

```text
源码和预览同步滚动
Outline 点击跳转
WYSIWYG 块级编辑
错误定位
局部重新渲染
```

所以现在就要补，不然后面会返工。

---

# 5. 第四件事：规范 HTML 输出

你的 HTML 输出不要随便生成，要固定结构。

建议统一这样：

```html
<article class="mw-document markdown-body">
  ...
</article>
```

块级元素尽量加：

```html
data-source-line
data-node-type
```

例如：

```html
<h2
  id="install"
  class="mw-heading"
  data-node-type="heading"
  data-source-line="12">
  Install
</h2>
```

代码块：

```html
<pre class="mw-code-block" data-lang="cpp" data-source-line="20">
  <code class="language-cpp">...</code>
</pre>
```

这样后面主题、同步滚动、编辑器集成都方便。

---

# 6. 第五件事：把主题系统整理成规范

不要只是写一个 `github-light.css`。
你应该做成主题包：

```text
themes/
└─ github-light/
   ├─ theme.json
   ├─ content.css
   └─ code.css
```

`theme.json`：

```json
{
  "id": "github-light",
  "name": "GitHub Light",
  "version": "1.0.0",
  "type": "light",
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
    "font.body": "-apple-system, BlinkMacSystemFont, Segoe UI, Helvetica, Arial, sans-serif",
    "font.code": "Consolas, Menlo, monospace",
    "layout.maxWidth": "980px",
    "layout.padding": "45px",
    "layout.lineHeight": "1.6"
  }
}
```

你的渲染器要支持：

```bash
mwrender input.md -o output.html --theme github-light
```

后面可以加：

```bash
mwrender input.md -o output.html --theme-path ./themes --theme my-theme
```

---

# 7. 第六件事：加 CLI 和库接口

你至少要有两种使用方式。

## 命令行方式

```bash
mwrender input.md -o output.html
mwrender input.md -o output.html --theme github-light
mwrender input.md --fragment
mwrender input.md --dump-ast
```

## C++ 库方式

```cpp
mwrender::RenderOptions options;
options.themeId = "github-light";
options.outputMode = mwrender::OutputMode::FullDocument;

mwrender::Renderer renderer(options);
auto result = renderer.render(markdownText);

if (result.ok) {
    std::cout << result.html;
}
```

后面 Qt 软件就调用这个库，而不是重新写一套渲染逻辑。

---

# 8. 第七件事：加 HTML 安全策略

因为你想支持 HTML 扩展，所以要明确策略。

建议有三种模式：

```cpp
enum class HtmlPolicy {
    Disabled,
    Safe,
    Trusted
};
```

含义：

```text
Disabled：HTML 全部转义
Safe：允许 div、span、table、mark 等安全标签，过滤 script、onclick、javascript:
Trusted：本地可信文件，原样输出
```

默认建议：

```text
Safe
```

这样你的渲染器会显得更专业，也方便以后接入编辑器。

---

# 9. 第八件事：加 Outline 和字数统计

这两个非常适合作为下一阶段功能，因为它们都基于 AST。

## Outline

从 AST 里提取标题：

```cpp
struct OutlineItem {
    int level;
    std::string title;
    std::string id;
    int line;
};
```

用于后续 Qt 编辑器左侧大纲面板。

## 字数统计

从 AST 提取可见文本：

```cpp
struct WordStats {
    int characters;
    int charactersNoSpaces;
    int cjkCharacters;
    int englishWords;
    int paragraphs;
    int codeLines;
};
```

这样你的渲染器不只是“转 HTML”，而是成为文档引擎。

---

# 10. 第九件事：加示例和文档

你的仓库里应该有：

```text
examples/
├─ basic.md
├─ html.md
├─ table.md
├─ theme.md
└─ full-demo.md
```

文档：

```text
docs/
├─ design.md
├─ ast-spec.md
├─ html-output-spec.md
├─ theme-spec.md
├─ api.md
└─ roadmap.md
```

README 里写清楚：

```text
项目定位
支持语法
构建方式
CLI 使用
主题规范
API 示例
后续计划
```

---

# 11. 第十件事：做一个最小 Qt 预览 Demo

等上面的核心库整理好后，再做一个很小的 Qt demo。

不要一上来做完整编辑器。
先做：

```text
左边 QPlainTextEdit
右边 QWebEngineView
输入 Markdown
实时调用你的渲染器生成 HTML
右边显示预览
```

结构：

```text
MarkdownPreviewDemo
├─ MainWindow
├─ SourceEditor
├─ PreviewView
└─ RendererAdapter
```

这一步的目标只是验证：

```text
你的渲染器能不能被 Qt 调用
主题 CSS 能不能正确显示
HTML 输出是否适合 QWebEngineView
性能是否够用
```

---

# 12. 你现在的推荐路线

按优先级排：

```text
1. 补测试
2. 确认 AST
3. 补 SourceRange
4. 规范 HTML 输出
5. 规范主题系统
6. 做 CLI
7. 做 C++ API
8. 加 HTML 安全策略
9. 加 Outline
10. 加字数统计
11. 写文档
12. 做 Qt 预览 Demo
13. 再考虑分屏编辑器
14. 最后考虑 WYSIWYG
```

---

# 13. 你不要急着做的东西

暂时不要急着做：

```text
完整 Qt 编辑器
所见即所得
Mermaid
LaTeX
Pandoc
工作区
代码编辑器
Git
终端
插件系统
```

这些都可以做，但不是现在。

你现在最重要的是把渲染器变成一个真正的核心库。

---

# 14. 最小下一步任务清单

你可以直接照这个做：

```text
任务 1：整理项目目录
任务 2：添加 tests/snapshot
任务 3：添加 --dump-ast
任务 4：添加 RenderOptions
任务 5：添加 Renderer 类
任务 6：添加 theme.json + content.css
任务 7：添加 --theme 参数
任务 8：HTML 输出统一包裹 article.mw-document
任务 9：标题生成 id
任务 10：块级 HTML 输出 data-source-line
任务 11：添加 OutlineBuilder
任务 12：添加 WordCounter
任务 13：写 docs/theme-spec.md
任务 14：写 docs/html-output-spec.md
任务 15：做 Qt Preview Demo
```

---

你的下一阶段目标可以定成：

```text
把 Markdown 渲染器从“能转换 HTML 的程序”
升级为
“可测试、可扩展、可被 Qt 编辑器调用的 Markdown 文档渲染核心库”。
```

做到这一步后，再继续做 Markdown 编辑器就会稳很多。
