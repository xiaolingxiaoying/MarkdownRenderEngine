# MWRender 用户使用手册 (CLI & 高级语法)

本文档将教您如何作为终端用户使用 MWRender 独立命令行程序，以及如何使用 MWRender 独占的高级 Markdown 扩展语法。

## 1. 命令行接口 (CLI Usage)

编译完成后，您可以在终端调用 `mwrender`。

### 基本命令
```bash
mwrender <input_file> [options]
```
例如，渲染一个文件并指定输出：
```bash
mwrender document.md -o output.html
```

### 常用运行参数

* **`-o, --output <file>`**: 指定生成的输出文件路径。如果不指定，将输出到终端。
* **`--theme <id>`**: 指定要使用的主题。引擎直接免安装内置了以下 7 款 GitHub 主题：
  * `github-light` (默认), `github-dark`, `github-auto` (自适应深浅)
  * `github-dark-dimmed`, `github-dark-high-contrast`, `github-dark-colorblind`, `github-light-colorblind`
* **`--theme-dir <dir>`**: 指定引擎加载主题 CSS 样式的根目录。默认会去寻找 `themes/`。
* **`--html-policy <disabled|trusted|sanitized>`**: 
  * `trusted`: 允许 Markdown 内部直接写入原生 HTML 标签。
  * `disabled`: 将所有 HTML 标签当做纯文本显示 (即 escape)。
  * `sanitized`: 过滤危险的 HTML 标签，仅保留安全标签。
  * `sanitize`: 对标签进行安全过滤（移除 `<script>` 等风险标签）。
* **`--allow-document-css`**: 生成独立的完整 HTML 页面，包含 `<html>` 头部以及内联样式。开启此项后，生成的 HTML 才可以脱网独立使用。
* **`--log-level <info|warning|error>`**: 控制终端报错的冗余程度。

---

## 2. 独家高级扩展语法指南

MWRender 除了完美支持所有的基础 Markdown 外，还内置了强大的排版增强特性：

### 2.1 FrontMatter 配置
在 Markdown 文件的最顶部使用 YAML 设置元数据，可以配置主题：
```yaml
---
theme: github-light
css: ["styles/custom.css"]
---
```

### 2.2 自动生成目录 `[TOC]`
只需在段落中单独敲入 `[TOC]`，引擎会自动提取整篇文章的 `<h1>` 到 `<h6>` 标题，并在该位置生成一棵完美的缩进目录树，带有超链接跳转。
```markdown
# 欢迎阅读
[TOC]
## 第一章
```

### 2.3 离线 LaTeX 数学公式
完美支持 MathJax (SVG 渲染引擎)，支持**无需联网**的本地公式解析。
* **行内公式**：使用单美元符号包围，例如 `$E = mc^2$`。
* **块级公式**：使用双美元符号包围（可换行），例如：
```markdown
$$
\int_a^b f(x) dx
$$
```

### 2.4 Mermaid 图表渲染
使用代码块并指定语言为 `mermaid` 即可：
<pre><code>```mermaid
graph TD;
    A-->B;
    A-->C;
```</code></pre>

### 2.5 纯离线代码高亮
指定代码块的语言，MWRender 导出的 HTML 会自带 Highlight.js 渲染库！
<pre><code>```cpp
#include &lt;iostream&gt;
```</code></pre>

### 2.6 图片完美对齐 (Image Alignment)
通过给图片的 URL 增加**哈希锚点后缀**，即可轻松实现原生 Markdown 无法做到的居中与浮动排版：
* **居中**：`![描述](image.png#center)`
* **左浮动**：`![描述](image.png#left)`
* **右浮动**：`![描述](image.png#right)`

### 2.7 学术级脚注 (Footnotes)
支持带有回跳链接（`↩`）的脚注：
```markdown
这是一段有学术价值的文字[^1]。

[^1]: 这里是脚注的详细解释，它会自动被收集到文章页面的最底端。
```

### 2.8 GitHub Alert 提示框
支持嵌套原生 SVG 提示框，语法与 GitHub 官方保持一致：
```markdown
> [!NOTE]
> 这是一个笔记提示。

> [!WARNING]
> 这是一个危险警告！
```
支持的类型包括：`NOTE`, `TIP`, `IMPORTANT`, `WARNING`, `CAUTION`。
