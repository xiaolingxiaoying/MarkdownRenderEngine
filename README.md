# MWRender

MWRender 是一个轻量级、安全、高性能的 C++20 Markdown 渲染引擎。它完全不依赖 Qt 或浏览器内核，只需标准库即可将 Markdown 解析为安全的 HTML5 文档或片段，并提供完善的 CSS 主题扩展与自定义选项。

## ✨ 特性 (Features)

* **纯净内核**：采用纯 C++20 编写，无冗余第三方依赖（不包含 DOM 树引擎或 JS 运行时）。
* **标准兼容**：支持核心的 CommonMark 语法（标题、段落、引用、列表、代码块等），以及扩展的 **GFM** 语法（表格、任务列表、删除线、自动链接）。
* **极速渲染**：1MB 文档解析+渲染耗时不到 20 毫秒（吞吐量 >50MB/s）。
* **极高安全性**：
  * 默认 `Disabled` 的 HTML 策略（拦截所有原生 HTML 节点注入）。
  * 默认拦截危险的 `javascript:`、`vbscript:` 和 `data:` URL（防御混淆和大小写攻击）。
  * 拦截危险的 CSS 路径穿越与远程资源加载（`@import` 与外部 `url()`）。
* **现代主题系统**：自带基于 `github-markdown-css` 的 `github-light` 浅色主题，并支持以外部 JSON 声明挂载自定义的本地主题包。
* **双模访问**：同时提供现代化的 C++ `Engine` API 接口与随时可用的 `mwrender` 命令行（CLI）工具。
* **AST 与元数据**：支持生成完整的源码行号映射（source map）属性，并提供带有 `title`、`theme` 等元数据配置的 `Front Matter` 解析。

---

## 🚀 快速开始 (Quick Start)

### 命令行工具 (CLI)

MWRender 提供了一个好用的命令行工具，编译后可直接在终端使用。

```bash
# 基本用法：将 Markdown 渲染为 HTML (输出完整的 HTML5 文档)
mwrender input.md -o output.html

# 仅输出 HTML 片段（不带 <html> <body> 包装），适合嵌入
mwrender input.md --fragment -o snippet.html

# 切换为其他已加载的主题（自带 github-light）
mwrender input.md --theme github-light

# 附加自定义的本地 CSS 样式
mwrender input.md --css custom.css

# 开启受信任模式（允许解析输入文本中原生的 HTML 标签，请确保输入可信！）
mwrender input.md --html-policy trusted

# 列出目前所有可用的主题
mwrender --list-themes

# 打印生成的 AST 语法树结构供调试用
mwrender input.md --dump-ast
```

你可以通过 `mwrender --help` 查看所有的可用选项与高级配置（例如指定配置文件 `--config` 等）。

### C++ API 调用 (Library)

在 C++ 项目中消费 MWRender 非常简单：

```cpp
#include <iostream>
#include <mwrender/engine.hpp>

int main() {
    const std::string markdown = "# Hello MWRender\n\nThis is a **bold** attempt.";

    // 1. 初始化引擎
    mwrender::Engine engine;
    
    // 2. 构造渲染请求
    mwrender::RenderRequest request;
    request.markdown = markdown;
    request.options.outputMode = mwrender::OutputMode::Fragment; // 仅输出片段
    // request.options.htmlPolicy = mwrender::HtmlPolicy::Trusted; // 根据需要可开启

    // 3. 执行渲染
    auto result = engine.render(request);
    if (!result.ok) {
        std::cerr << "Render failed!\n";
        return 1;
    }

    // 4. 获取输出
    std::cout << result.html << "\n";
    return 0;
}
```

---

## 📦 编译与安装 (Build & Install)

本项目基于 CMake 构建。你可以在任何支持 C++20 的环境下使用 MinGW 或 MSVC 进行编译。

```powershell
# 1. 生成 Release 配置 (推荐)
cmake --preset release

# 2. 编译项目 (包含核心库与 CLI)
cmake --build --preset release

# 3. 运行所有的 CTest 测试 (包含一致性、压力测试与安全测试)
ctest --preset release --output-on-failure

# 4. 安装 (生成 include, lib, bin, share 等供消费)
cmake --install build/release --prefix /your/install/path
```

对于 CMake 消费方项目，在你的 `CMakeLists.txt` 中：
```cmake
find_package(MWRender CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE MWRender::Core)
```

---

## 🎨 主题扩展 (Theme Packages)

MWRender 设计了基于 `theme.json` 的标准主题包格式。你可以自由制作主题包并挂载到渲染器。

目录结构：
```text
my-theme/
├─ theme.json
└─ content.css
```

`theme.json` 的基本清单：
```json
{
  "schemaVersion": 1,
  "id": "my-theme",
  "name": "My Theme",
  "version": "1.0.0",
  "appearance": "light",
  "entry": {
    "content": "content.css"
  }
}
```
通过 C++ 的 `Engine::addThemeRoot()` 或 CLI 的 `--theme-path` 参数注册该主题所在的父目录后，即可使用 `--theme my-theme` 激活该主题。

---

## ⚙️ Front Matter 配置

MWRender 支持解析 Markdown 文件头部的受限 YAML (Front Matter)，你可以直接在文档内部声明渲染元数据：

```yaml
---
title: My Documentation
theme: github-light
css:
  - note.css
---
# Document Content
...
```
*注：出于安全考虑，文档内的 `css` 引入默认是被拦截的。需要在 CLI 中通过 `--allow-document-css` 或在 API 的 `RenderOptions` 中显式开启 `allowDocumentCss`。*

---

## 📖 更多文档

* [COMPATIBILITY.md](docs/COMPATIBILITY.md) - 支持语法说明、安全策略边界及 CommonMark 兼容性测试报告。
* [DevelopmentGuide.md](docs/DevelopmentGuide.md) - 深入了解架构、AST 结构、API 与主题制作详情。
* [ImplementationPlan.md](docs/ImplementationPlan.md) - 项目开发演进与路线规划。

## 第三方声明

内置的 `github-light` 样式表基于 Sindre Sorhus 开发的 [`github-markdown-css`](https://github.com/sindresorhus/github-markdown-css) 5.9.0，并遵循其 MIT 开源协议（详见内置主题目录下的 `LICENSE`）。
