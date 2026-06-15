# MWRender 主题开发与 CSS 规范 (Theme Spec)

MWRender 支持极其强大的样式自定义，允许开发者/设计师编写自己的 CSS 主题包。
由于我们手写了 C++ AST 解析器，渲染生成的 HTML 拥有极其清晰、无污染的 CSS 命名规范，方便精准抓取元素。

## 1. 主题结构

一个合法的 MWRender 主题位于 `themes/主题名/` 目录下，并且至少包含一个 `theme.json` 文件（用于提供基础信息）和相关的 `.css` 文件。
例如，内置的 `github-light` 主题结构如下：
```text
themes/github-light/
├── theme.json
├── content.css
└── code.css
```

### `theme.json` 规范
引擎会读取这个文件，按照 `css` 数组的顺序去合并注入样式。
```json
{
    "name": "github-light",
    "version": "1.0.0",
    "css": [
        "content.css",
        "code.css"
    ]
}
```

## 2. HTML 结构与 Class 命名树

所有 MWRender 生成的正文内容都会被包裹在 `<article class="mw-document markdown-body">` 容器内。
我们使用统一的前缀 `.mw-` 以防止与您的其他全局 CSS 发生冲突。

### 常用区块选择器
* `.mw-document`: 最外层根容器。
* `.mw-heading` / `h1`~`h6`: 所有等级的标题。
* `.mw-paragraph`: 普通段落。
* `.mw-list`: 无序或有序列表 (ul/ol)。
* `.mw-list-item`: 列表的子项目 (li)。
* `.mw-table-wrapper` -> `.mw-table`: 表格外壳与原生表格。
* `.mw-blockquote`: 传统的引用块。

### 扩展组件选择器
* `.markdown-alert` 及修饰符 `.markdown-alert-note` / `.markdown-alert-warning` 等：用于美化 GitHub Alert 提示框，您可以自由定义颜色与边框。
* `.mw-toc`: `[TOC]` 生成的无序目录列表。其子 `li` 会携带 `.toc-level-1` 等类名用于控制层级缩进。
* `.footnotes`: `[^1]` 学术脚注生成的底部区域，包含一个 `<hr class="footnotes-sep">` 以及脚注列表。
* `.task-list-item`: 带有复选框的任务列表项目。

## 3. 开发建议

**使用 CSS Variables (自定义属性)**：
我们在开发主题时强烈推荐在 `:root` 或 `.mw-document` 中声明调色板：
```css
.mw-document {
  --color-fg-default: #1F2328;
  --color-canvas-default: #ffffff;
  --color-border-default: #d0d7de;
  --color-accent-fg: #0969da;
}
.mw-document {
  color: var(--color-fg-default);
  background-color: var(--color-canvas-default);
}
```
这样不仅能统一管理，如果您的网站需要做深色模式切换 (Dark Mode) 也会变得异常简单！
