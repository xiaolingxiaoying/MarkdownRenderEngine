# MWRender 开发规范

## 1. 范围

本文是 MWRender 的实现基线，规定核心架构、公开 API、AST、HTML 输出、主题包、
CSS 扩展和安全策略。

规范关键词：

- “必须”表示实现和兼容性必须满足。
- “应该”表示默认选择，偏离时需要记录理由。
- “可以”表示可选能力。

## 2. 核心设计决策

### 2.1 模块边界

```text
mwrender::Parser
    Markdown -> ParseResult(AST + diagnostics + metadata)

mwrender::HtmlRenderer
    AST + RenderOptions -> HTML fragment

mwrender::ThemeManager
    theme ID + search roots -> resolved Theme

mwrender::CssComposer
    core + theme + extensions -> CSS

mwrender::Engine
    orchestrates parse, render, theme and document assembly
```

Parser 不读取主题，ThemeManager 不解析 Markdown，HtmlRenderer 不读取文件。
文件系统访问集中在 Engine 的输入层和 ThemeManager/CSS loader。

### 2.2 无共享可变请求状态

主题路径等应用级资源可以注册到 `Engine`，但每次渲染的主题、HTML 策略和 CSS
扩展必须放在 `RenderRequest` 中。

不要使用以下形式保存本次请求状态：

```cpp
renderer.setTheme("dark");
renderer.addCustomCss("...");
renderer.render(markdown);
```

推荐：

```cpp
RenderRequest request;
request.markdown = markdown;
request.options.themeId = "github-light";
request.css.text.push_back(customCss);

RenderResult result = engine.render(request);
```

这样更容易实现线程安全，也避免一次请求污染下一次请求。

### 2.3 输出分层

渲染必须先产生 fragment，再决定是否包装 full document：

```text
AST -> HTML fragment -> CSS -> optional document shell
```

Fragment 模式不得偷偷嵌入全局 `<style>`。调用方通过 `RenderResult::css` 获取样式。

## 3. 编码与源码位置

### 3.1 输入编码

- C++ API 接收 UTF-8 `std::string_view`。
- CLI 读取文件时默认按 UTF-8。
- 可以识别并去除 UTF-8 BOM。
- v0.1 不负责自动探测 GBK、UTF-16 等编码。
- 非法 UTF-8 的策略由 `InvalidUtf8Policy` 控制。

```cpp
enum class InvalidUtf8Policy {
    Reject,
    Replace
};
```

### 3.2 SourceRange

内部以字节 offset 作为唯一精确定位基准：

```cpp
struct SourcePosition {
    std::size_t offset = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

struct SourceRange {
    SourcePosition begin;
    SourcePosition end; // half-open: [begin, end)
};
```

规则：

- offset 是 UTF-8 字节偏移。
- line/column 从 1 开始。
- range 使用左闭右开区间。
- column 的 v0.1 定义为字节列，不承诺 grapheme/display column。
- AST 节点范围必须覆盖产生该节点的 Markdown 标记。

## 4. AST 规范

### 4.1 NodeType

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
    Strikethrough,
    InlineCode,
    Link,
    Image,
    HtmlInline,
    AutoLink,
    SoftBreak,
    HardBreak
};
```

### 4.2 类型化 payload

不要把所有节点属性都放进 `map<string, string>`。公开 AST 使用类型化 payload：

```cpp
struct HeadingData {
    std::uint8_t level = 1;
};

struct ListData {
    bool ordered = false;
    std::uint64_t start = 1;
    bool tight = true;
};

struct CodeBlockData {
    std::string info;
    std::string language;
};

struct LinkData {
    std::string destination;
    std::string title;
};

struct ImageData {
    std::string destination;
    std::string title;
};

struct HtmlData {
    std::string raw;
};

using NodePayload = std::variant<
    std::monostate,
    HeadingData,
    ListData,
    CodeBlockData,
    LinkData,
    ImageData,
    HtmlData
>;

struct Node {
    NodeType type;
    SourceRange range;
    NodePayload payload;
    std::string literal;
    std::vector<std::unique_ptr<Node>> children;
};
```

内部实现可以增加私有状态，但不得要求调用者理解 parser 的临时结构。

### 4.3 AST 不变量

- `Document` 是唯一根节点。
- 块级节点不能作为行内节点的子节点。
- `Text`、`InlineCode` 等叶节点不得有 children。
- `HeadingData::level` 必须在 1 到 6。
- `ListItem` 必须位于 `List` 下。
- HTML 原文只存在于 `HtmlData::raw`，渲染策略不写回 AST。
- Parser 产生的 AST 在渲染期间只读。

## 5. 解析器规范

### 5.1 解析阶段

```text
1. 输入检查与 BOM 处理
2. 可选 Front Matter 提取
3. LineScanner
4. Block Parser
5. Inline Parser
6. AST validation
```

### 5.2 Block Parser

Block Parser 应使用容器状态而不是一组相互独立的大正则。推荐优先级：

```text
fenced code
ATX heading
thematic break
blockquote/list containers
HTML block
paragraph continuation
```

实际实现要处理容器延续，因此这不是简单的逐行 `if/else` 列表。

### 5.3 Inline Parser

Inline Parser 至少需要：

- delimiter stack 处理 emphasis/strong。
- code span 优先保护其内部内容。
- link/image bracket stack。
- raw HTML inline 识别。
- 普通文本批量消费，避免逐字符创建 Text 节点。

相邻 Text 节点应该合并。

### 5.4 错误模型

Markdown 大多数“错误”应退化为文本。Diagnostic 分级：

```cpp
enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct Diagnostic {
    DiagnosticSeverity severity;
    std::string code;
    std::string message;
    std::optional<SourceRange> range;
};
```

稳定的 `code` 供 CLI、Qt 和测试使用，例如：

```text
MW1001 unclosed-fence
MW2001 theme-not-found
MW3001 html-sanitizer-unavailable
MW4001 css-file-rejected
```

## 6. HTML 输出规范

### 6.1 Fragment

默认 fragment：

```html
<article class="mw-document markdown-body">
  <h1 class="mw-heading" id="hello" data-source-line="1">Hello</h1>
  <p class="mw-paragraph" data-source-line="3">
    This is <strong>Markdown</strong>.
  </p>
</article>
```

固定约束：

- 根元素为 `article.mw-document.markdown-body`。
- MWRender 自有 class 使用 `mw-` 前缀。
- 不要求每个标准元素都有 class；class 只用于稳定扩展点。
- 块级 source attributes 可通过选项关闭。
- 输出结尾是否带换行必须保持确定。

### 6.2 Full Document

```html
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Markdown Document</title>
  <style>
    /* composed CSS */
  </style>
</head>
<body>
  <article class="mw-document markdown-body">
    ...
  </article>
</body>
</html>
```

文档 title、lang 和 CSS 必须由 RenderRequest 决定，不从正文标题隐式推断。

### 6.3 节点映射

| AST | HTML |
| --- | --- |
| Heading | `h1` ... `h6` |
| Paragraph | `p` |
| BlockQuote | `blockquote` |
| List | `ul` / `ol` |
| ListItem | `li` |
| CodeBlock | `pre > code` |
| ThematicBreak | `hr` |
| Strong | `strong` |
| Emphasis | `em` |
| InlineCode | `code` |
| Link | `a` |
| Image | `img` |
| SoftBreak | `\n` 或空格，按选项 |
| HardBreak | `br` |

代码块示例：

```html
<pre class="mw-code-block" data-lang="cpp"><code class="language-cpp">...</code></pre>
```

`language-*` 中的值必须经过允许字符归一化，原始 info string 不可直接进入 class。

### 6.4 Source attributes

`SourceMapMode::Line`：

```html
data-source-line="3"
```

`SourceMapMode::Full`：

```html
data-source-line="3"
data-source-start="12"
data-source-end="41"
data-node-type="paragraph"
```

offset 与 `SourceRange` 一致，采用 UTF-8 字节偏移和左闭右开区间。

### 6.5 Slug

标题 ID 规则：

1. 提取标题的可见纯文本。
2. Unicode 字母与数字保留。
3. ASCII 转小写。
4. 空白序列转为 `-`。
5. 移除不参与 ID 的标点。
6. 去除首尾 `-`。
7. 空结果使用 `section`。
8. 重复 ID 追加 `-1`、`-2`。

Slug 算法属于输出兼容性的一部分，修改时必须提供迁移说明。

## 7. 转义与 URL 安全

### 7.1 转义上下文

至少区分：

- HTML text
- HTML attribute
- URL attribute
- CSS variable value

不得用同一个“escape everything”函数处理所有上下文。

HTML text 转义：

```text
& -> &amp;
< -> &lt;
> -> &gt;
```

属性额外转义引号：

```text
" -> &quot;
' -> &#39;
```

### 7.2 URL 策略

默认允许：

- 相对 URL
- `#fragment`
- `http`
- `https`
- `mailto`

默认拒绝：

- `javascript`
- `vbscript`
- 非图片场景的 `data`
- 控制字符或混淆后形成的危险 scheme

`file` 和 `data:image/...` 必须由宿主显式开启。检查前要去除 scheme 周围空白并
进行 ASCII 大小写归一化。

## 8. HTML 扩展规范

### 8.1 策略

```cpp
enum class HtmlPolicy {
    Disabled,
    Sanitized,
    Trusted
};
```

#### Disabled

- raw HTML AST 节点保留原文。
- Renderer 将其作为文本转义。
- 这是默认策略。

#### Sanitized

- raw HTML 传给 `HtmlSanitizer`。
- sanitizer 返回安全 HTML 或拒绝结果。
- 不可用时默认退化为 Disabled 并产生 `MW3001`。
- `strictHtmlPolicy=true` 时不可用即返回错误。

#### Trusted

- raw HTML 原样进入输出。
- 只适用于可信本地输入。
- API 调用必须显式选择，Front Matter 不得单独开启。

### 8.2 Sanitizer 接口

```cpp
struct SanitizeResult {
    bool accepted = false;
    std::string html;
    std::vector<Diagnostic> diagnostics;
};

class HtmlSanitizer {
public:
    virtual ~HtmlSanitizer() = default;
    virtual SanitizeResult sanitize(
        std::string_view html,
        const SourceRange& range) const = 0;
};
```

Sanitizer 必须基于 HTML tokenizer/parser 或成熟库。禁止使用标签正则替换来宣称
Safe/Sanitized。

## 9. CSS 扩展规范

### 9.1 CSS 来源

```cpp
enum class CssOrigin {
    Core,
    Theme,
    User,
    Workspace,
    Document,
    Request
};

struct CssSource {
    CssOrigin origin;
    std::string name;
    std::string css;
};
```

保留来源信息便于诊断、调试和未来 source map。

### 9.2 合并顺序

由低到高：

```text
core
theme variables
theme content.css
theme code.css
user CSS
workspace CSS
document CSS
request CSS
```

同一层按注册顺序拼接。

### 9.3 信任模型

CSS 可以加载网络资源、覆盖 UI、读取本地 URL 或造成可用性问题。MWRender v0.x
采用以下模型：

- 内置主题 CSS 可信。
- 用户、workspace、document 和 request CSS 必须由宿主能力开关允许。
- 默认禁止 `@import`。
- 默认禁止 `http:`、`https:` 和协议相对的 `url()`。
- 本地资源只能位于允许根目录。
- 不承诺把任意不可信 CSS 转化为安全 CSS。
- 在 QWebEngine 等宿主中还应使用网络拦截/CSP 作为第二层保护。

只靠字符串搜索无法完整解析 CSS。严格拒绝远程资源时，应该使用 CSS tokenizer，
或直接禁止不可信 CSS 输入。

## 10. 主题规范

### 10.1 主题目录

```text
github-light/
├─ theme.json
├─ content.css
├─ code.css
└─ assets/
```

`assets/` 可选。v1 主题不得包含脚本。

### 10.2 theme.json schemaVersion 1

```json
{
  "schemaVersion": 1,
  "id": "github-light",
  "name": "GitHub Light",
  "version": "1.0.0",
  "appearance": "light",
  "author": "MWRender",
  "description": "A GitHub-like light theme for Markdown documents.",
  "entry": {
    "content": "content.css",
    "code": "code.css"
  },
  "variables": {
    "color.background": "#ffffff",
    "color.foreground": "#1f2328",
    "color.muted": "#59636e",
    "color.border": "#d1d9e0",
    "color.accent": "#0969da",
    "color.codeBackground": "#f6f8fa",
    "font.body": "-apple-system, BlinkMacSystemFont, \"Segoe UI\", sans-serif",
    "font.code": "SFMono-Regular, Consolas, monospace",
    "layout.maxWidth": "980px",
    "layout.padding": "45px",
    "layout.lineHeight": "1.6"
  }
}
```

### 10.3 字段规则

| 字段 | 规则 |
| --- | --- |
| schemaVersion | 必须为支持的整数版本 |
| id | `[a-z0-9]+(?:-[a-z0-9]+)*` |
| name | 非空展示名称 |
| version | SemVer 字符串 |
| appearance | `light`、`dark` 或 `auto` |
| entry.content | 必须存在且位于主题根目录 |
| entry.code | 可选，必须位于主题根目录 |
| variables | 仅接受已知 key，未知 key warning |

入口路径必须是主题根目录内的相对路径。禁止绝对路径、`..` 越界和解析后的符号
链接越界。

### 10.4 标准变量

| JSON key | CSS variable |
| --- | --- |
| color.background | `--mw-color-bg` |
| color.foreground | `--mw-color-fg` |
| color.muted | `--mw-color-muted` |
| color.border | `--mw-color-border` |
| color.accent | `--mw-color-accent` |
| color.codeBackground | `--mw-color-code-bg` |
| font.body | `--mw-font-body` |
| font.code | `--mw-font-code` |
| layout.maxWidth | `--mw-layout-max-width` |
| layout.padding | `--mw-layout-padding` |
| layout.lineHeight | `--mw-line-height` |

变量输出到 `.mw-document`，避免污染宿主页面全局 `:root`：

```css
.mw-document {
  --mw-color-bg: #ffffff;
  --mw-color-fg: #1f2328;
}
```

### 10.5 发现与覆盖

注册层级由低到高：

```text
1. built-in
2. user
3. workspace
4. explicit API/CLI paths
```

规则：

- 同层按注册顺序，后注册者覆盖。
- 同 ID 覆盖必须产生 diagnostic，并记录来源路径。
- `listThemes()` 返回最终可选主题以及被覆盖来源信息。
- 默认主题固定为 `github-light`。
- 主题不存在时，非严格模式回退默认主题并 warning。
- 严格模式返回错误，不生成带错误主题的文档。

### 10.6 GitHub Light 命名

`github-light` 表示 GitHub-like 视觉方向，不声明为 GitHub 官方主题。主题 CSS
应由项目维护，并记录借鉴来源和适用许可证，不把外部样式文件当作不可控运行时
依赖。

## 11. 配置与 Front Matter

### 11.1 RenderOptions

```cpp
enum class OutputMode {
    Fragment,
    FullDocument
};

enum class SourceMapMode {
    None,
    Line,
    Full
};

struct RenderOptions {
    OutputMode outputMode = OutputMode::FullDocument;
    SourceMapMode sourceMap = SourceMapMode::Line;
    HtmlPolicy htmlPolicy = HtmlPolicy::Disabled;

    std::string themeId = "github-light";
    std::string language = "zh-CN";
    std::string title = "Markdown Document";

    bool includeCss = true;
    bool strictTheme = false;
    bool strictHtmlPolicy = false;
    bool allowDocumentTheme = true;
    bool allowDocumentCss = false;
    bool allowRemoteCssResources = false;
};
```

### 11.2 Front Matter

v0.2 允许字段：

```yaml
---
title: Example
theme: github-light
css:
  - ./note.css
---
```

规则：

- 必须位于文档开头。
- 使用正式 YAML 解析器，或明确实现受限子集并拒绝子集外语法。
- Front Matter 只提供候选配置。
- 宿主能力开关决定其是否生效。
- `theme` 不能绕过允许的主题根目录。
- `css` 路径相对 Markdown 文件目录解析，并受允许根目录限制。

### 11.3 覆盖顺序

```text
defaults
application config
workspace config
front matter
RenderRequest/CLI
```

安全能力使用交集：

```text
effective capability = host allowance AND request allowance
```

Front Matter 永远不能把 `Disabled` HTML 提升为 `Trusted`。

## 12. C++ API

### 12.1 请求与结果

```cpp
struct CssRequest {
    std::vector<std::filesystem::path> files;
    std::vector<std::string> text;
};

struct RenderRequest {
    std::string_view markdown;
    std::optional<std::filesystem::path> sourcePath;
    RenderOptions options;
    CssRequest css;
};

struct RenderResult {
    bool ok = false;
    std::string html;
    std::string fragment;
    std::string css;
    std::unique_ptr<Node> document;
    std::vector<Diagnostic> diagnostics;
    std::string resolvedThemeId;
};
```

`html` 规则：

- FullDocument 时是完整文档。
- Fragment 时与 `fragment` 相同。

返回 AST 可以通过选项关闭，以减少不需要 AST 的调用方内存占用。

### 12.2 Engine

```cpp
class Engine {
public:
    Engine();
    explicit Engine(EngineOptions options);

    void addThemeRoot(
        std::filesystem::path path,
        ThemeOrigin origin = ThemeOrigin::Explicit);

    void setHtmlSanitizer(std::shared_ptr<const HtmlSanitizer> sanitizer);

    [[nodiscard]]
    RenderResult render(const RenderRequest& request) const;

    [[nodiscard]]
    std::vector<ThemeInfo> listThemes() const;
};
```

线程安全约定：

- 完成资源注册后，并发调用 `render()` 应该安全。
- 渲染期间不得修改 theme registry。
- 如需动态刷新主题，使用新 Engine 或显式同步的 reload API。

### 12.3 使用示例

```cpp
#include <mwrender/engine.hpp>

int main() {
    mwrender::Engine engine;

    const std::string markdown = "# Hello\n\nThis is **Markdown**.";

    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.themeId = "github-light";
    request.options.outputMode = mwrender::OutputMode::FullDocument;

    const mwrender::RenderResult result = engine.render(request);
    if (!result.ok) {
        return 1;
    }

    std::cout << result.html;
}
```

## 13. CLI 规范

```text
mwrender [input] [options]
```

基础命令：

```bash
mwrender README.md -o README.html
mwrender README.md --fragment
mwrender README.md --theme github-light
mwrender README.md --css custom.css
mwrender README.md --html-policy disabled
mwrender README.md --html-policy trusted
mwrender --list-themes
mwrender README.md --dump-ast
```

约定：

- input 省略或为 `-` 时读取 stdin。
- `-o -` 或未指定输出文件时写 stdout。
- diagnostics 写 stderr。
- CLI 参数覆盖配置文件与 Front Matter。
- `--html-policy trusted` 必须显式给出。
- `--strict` 开启主题、sanitizer 和 CSS 文件的严格失败。

退出码：

```text
0 success, warnings allowed
1 render or validation failure
2 invalid CLI usage
3 input/output failure
```

## 14. 默认主题 CSS 约束

默认主题至少覆盖：

- 文档容器和响应式 padding
- h1-h6
- p、a、strong、em
- inline code 和 fenced code
- blockquote
- ul、ol、li、task list
- table
- hr
- img
- kbd、mark、details/summary

主题规则必须限定在 `.mw-document` 下：

```css
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

.mw-document *,
.mw-document *::before,
.mw-document *::after {
  box-sizing: inherit;
}
```

不得使用影响宿主页面的无前缀全局选择器，如裸 `body`、`a`、`table` 或 `*`。

## 15. 测试规范

### 15.1 单元测试

- Scanner、slug、escape、URL policy、theme validation 使用精确断言。
- Parser 检查 AST 类型、payload、层级和 SourceRange。
- Renderer 检查结构和关键属性。

### 15.2 Snapshot

- Snapshot 文件必须稳定、可读。
- 格式化空白属于输出契约时才进入 snapshot。
- 修改 snapshot 必须说明行为变化，不允许盲目批量接受。

### 15.3 Conformance

- 记录每个 CommonMark/GFM example 的 pass、expected-fail 或 not-supported。
- 不把未支持用例静默排除。
- 发布说明提供兼容性汇总。

### 15.4 安全回归

至少覆盖：

```html
<script>alert(1)</script>
<img src=x onerror=alert(1)>
<a href="javascript:alert(1)">click</a>
<svg><script>alert(1)</script></svg>
```

以及：

- 大小写和空白混淆的危险 scheme。
- HTML entity/编码混淆。
- CSS `@import` 和远程 `url()`。
- 主题 entry 路径穿越。
- symlink 越过允许根目录。

## 16. 代码规范

- C++20。
- 命名空间 `mwrender`。
- 类型名 PascalCase。
- 函数和变量 camelCase。
- 文件名 snake_case。
- 公开 API 使用 `[[nodiscard]]` 标注重要结果。
- owning pointer 使用 `std::unique_ptr` 或有明确共享语义的 `std::shared_ptr`。
- parser 热路径避免 `std::regex` 和不必要 substring 复制。
- 公开头文件不暴露内部 parser 状态。
- 注释解释规则与原因，不复述代码。

## 17. 依赖策略

允许的依赖类别：

- JSON parser：解析 `theme.json` 和配置。
- 测试框架。
- 可选 YAML parser。
- 可选 HTML sanitizer backend。

要求：

- 每个依赖必须说明用途、许可证、链接方式和可选性。
- 核心解析与 HTML 渲染不能依赖 Qt。
- 可选功能通过 CMake option 隔离。
- 不为一个简单工具函数引入大型依赖。

示例：

```cmake
option(MWRENDER_BUILD_CLI "Build the CLI" ON)
option(MWRENDER_BUILD_TESTS "Build tests" ON)
option(MWRENDER_ENABLE_YAML "Enable YAML front matter" OFF)
option(MWRENDER_ENABLE_SANITIZER "Enable sanitized HTML backend" OFF)
```

## 18. Qt 集成边界

未来 Qt 层负责：

- 编辑器输入与 debounce。
- 文件编码提示。
- 把 `RenderResult::html` 交给 QWebEngineView。
- 配置网络拦截、CSP 和本地资源访问。
- 根据 `data-source-*` 实现同步滚动。
- 展示 diagnostics、outline 和 stats。

MWRender 核心不包含 QObject、QString、QFile 或 QWebEngine 类型。单独适配库可以
提供 UTF-8/string 与 Qt 类型转换。

## 19. API 演进规则

- v0.x 可以调整 API，但每次调整同步修改示例和文档。
- `theme.json.schemaVersion` 独立于库版本。
- HTML class、slug 和 source attributes 视为输出兼容性接口。
- 新语法默认关闭还是开启必须在版本说明中明确。
- 安全默认值不得在次版本中静默放宽。

## 20. 参考资料

- [CommonMark Specification 0.31.2](https://spec.commonmark.org/0.31.2/)
- [GitHub Flavored Markdown Specification](https://github.github.com/gfm/)
- [GitHub Markdown documentation](https://docs.github.com/en/get-started/writing-on-github)
- [OWASP XSS Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cross_Site_Scripting_Prevention_Cheat_Sheet.html)

