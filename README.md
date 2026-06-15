# MWRender - High-Performance Markdown Engine

**MWRender** 是一个采用纯 C++ 编写的、超高性能的零依赖 Markdown 渲染引擎。
它能将 Markdown 文件直接解析为具有精美排版和交互能力的独立 HTML 文件（Zero-Dependency Offline HTML），特别适合用于构建极速博客、文档网站生成器或内嵌于其他 C++ 项目中。

## 🚀 核心特性

- **极致性能**：采用纯 C++ 手写的递归下降抽象语法树 (AST) 解析器。单线程吞吐量高达 **~70MB/s**（解析 1MB 的超长纯文本仅需 ~13 毫秒）。
- **零依赖脱机渲染**：MWRender 的「FullDocument」模式会将所有外部资源（包括主题 CSS、脚本）全部打包进单一的 HTML 文件中，做到**断网秒开**。
- **高级企业级特性**：
  - 🎨 **主题与样式自定义**：原生支持 FrontMatter 和灵活的自定义主题。
  - 📊 **Mermaid 离线渲染**：无需联网，原生支持在代码块中渲染复杂的流程图。
  - 🧮 **MathJax 离线公式**：内置本地化 MathJax SVG 引擎，完美支持 `$` 与 `$$` 的 LaTeX 渲染。
  - 💻 **Highlight.js 代码高亮**：C++, Python, JS 等语言在离线状态下依然拥有 GitHub 风格着色。
  - 📖 **智能辅助排版**：一键 `[TOC]` 自动提取全篇目录树、支持学术级脚注 `[^1]`、独创的 URL 后缀语法实现图片居中与浮动。
  - 🔔 **GitHub 风格提示框**：原生支持 `> [!NOTE]` 等多种精美警告/提示块，内置高质量 SVG 图标。

## 📥 快速开始 (Quick Start)

### 编译项目
使用 CMake 和您喜欢的编译器进行编译：
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 渲染您的第一篇文档
编译完成后，您可以直接在命令行中使用 `mwrender` 渲染任意 markdown 文件。
为了生成带有自适应系统深浅色主题的独立 HTML：
```bash
./build/release/mwrender.exe my_doc.md -o my_doc.html --theme github-auto --allow-document-css
```
双击打开生成的 `my_doc.html`，享受极其顺滑的离线阅读体验！

## 📚 详细文档体系

为了满足不同的使用场景，我们提供了详细的文档拆分：

- 🛠 **用户使用手册**：[CLI 命令行与高级语法指南](docs/CLI_USAGE.md)
  - 介绍如何配置 FrontMatter、使用离线图表公式以及图片的精准排版。
- 🎨 **主题设计规范**：[MWRender 主题开发文档](docs/THEME_SPEC.md)
  - 详细介绍渲染器生成的 HTML 树结构与 CSS 类名，教您如何定制独一无二的专属主题。
- 💻 **C++ 接入指南**：[将 MWRender 集成到您的项目中](docs/INTEGRATION.md)
  - 为 C++ 开发者提供集成说明、API 概览以及架构解析。

## 📊 性能跑分

在常规现代桌面 CPU 上的基准测试表现：
- **普通包含各种组件的混合文档 (1MB)**: ~78 ms (吞吐量 ~12.8 MB/s)
- **1000 篇短文档连发渲染**: ~124 ms (~0.12 ms/篇)
- **常规 65KB 文章 (带代码高亮与主题)**: < 1 ms

*（运行 `./build/release/mwrender_benchmark.exe` 亲自体验您的机器跑分）*

---

*MWRender - 为极致阅读体验与超高编译速度而生。*
