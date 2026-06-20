# MWRender 实施计划

## 1. 文档目的

本文根据仓库中的 `PLAN.md` 与 `DetailedPlan.md` 归并而成，用于指导
MWRender 从空仓库发展为可复用的轻量级 Markdown 渲染库。

与两份原始文档相比，本计划进一步明确：

1. 第一阶段的功能边界和非目标。
2. Markdown、HTML、CSS 与主题模块之间的依赖关系。
3. 主题发现、配置覆盖和错误回退规则。
4. 每个开发阶段的交付物、测试项和验收标准。
5. HTML/CSS 扩展的安全边界。

详细 API、AST、主题包和输出格式见
[`DevelopmentGuide.md`](DevelopmentGuide.md)。

## 2. 项目结论

MWRender 定位为一个不依赖 Qt 的 C++20 核心库：

```text
Markdown Source
    -> Front Matter
    -> Line Scanner
    -> Block Parser
    -> Inline Parser
    -> AST
    -> HTML Fragment Renderer
    -> Theme/CSS Composer
    -> HTML Fragment or Full Document
```

首个稳定版本应做到：

- 支持约定的 CommonMark 基础子集。
- 生成保留源码范围的 AST。
- 输出 HTML 片段或完整 HTML5 文档。
- 支持受策略控制的原始 HTML 节点。
- 支持有明确优先级的本地 CSS 扩展。
- 提供规范化主题包和主题选择接口。
- 内置 `github-light` 风格的默认浅色主题。
- 同时提供 C++ API 和 CLI。

## 3. 设计基线

### 3.1 规范基线

- 语法参照 CommonMark 0.31.2，但 v0.1 只承诺文档列出的子集。
- GFM 表格、任务列表、删除线和自动链接作为独立扩展启用。
- HTML 输出为 HTML5。
- 主题清单使用 JSON，主题样式使用 CSS。
- 主题 ID 使用小写 kebab-case，例如 `github-light`。

“参考 CommonMark”不等于 v0.1 宣称完整兼容。只有通过对应官方示例测试的
语法，才可以列入兼容性声明。

### 3.2 轻量原则

- 核心库不依赖 Qt、浏览器内核、网络服务或脚本运行时。
- 渲染过程不访问网络。
- 依赖控制在必要范围内，优先使用标准库。
- JSON 解析器、测试框架和可选 HTML sanitizer 可以作为独立依赖。
- 解析、渲染和主题加载之间通过稳定数据结构隔离。
- 第一阶段不引入插件系统、动态脚本或通用 DOM。

### 3.3 安全基线

- 普通文本、代码、属性值必须按上下文转义。
- URL 必须执行协议校验。
- HTML 默认策略为 `Disabled`。
- `Sanitized` 模式必须使用结构化 HTML 解析/清洗后端，不得用正则实现。
- 未配置 sanitizer 时，`Sanitized` 降级为转义并产生 warning；严格模式下报错。
- `Trusted` 仅面向明确可信的本地文档。
- 自定义 CSS 视为可信本地代码，不承诺把任意 CSS 清洗为安全 CSS。
- 默认禁止远程 CSS、`@import` 和远程资源 URL。

## 4. 版本范围

### 4.1 v0.1 MVP

语法：

- ATX 标题
- 段落
- fenced code block
- 引用
- 有序列表和无序列表
- 分隔线
- 强调和加粗
- 行内代码
- 行内链接和图片
- 软换行和硬换行
- 原始 HTML block/inline 节点

能力：

- Markdown 到 AST
- AST 到 HTML fragment
- Full Document 包装
- `Disabled` 和 `Trusted` HTML 策略
- sanitizer 扩展接口
- 默认 `github-light` 主题
- API 选择主题
- 本地自定义 CSS
- CLI
- AST、HTML snapshot、主题和安全测试

### 4.2 v0.2

- 可用的 `Sanitized` HTML 后端
- GFM 表格、任务列表、删除线、自动链接
- Front Matter 的 `title`、`theme`、`css`
- Outline
- 标题 slug 去重
- 完整 SourceRange 和 HTML source attributes
- 主题列表、主题校验和严格主题模式

### 4.3 v0.3

- 字数与文档统计
- 代码高亮器扩展接口
- 脚注
- 自定义容器/Callout
- 性能基准与模糊测试
- 面向编辑器的块级映射接口

### 4.4 当前非目标

- Qt UI 或完整编辑器
- 所见即所得编辑
- JavaScript 插件执行
- PDF、DOCX、Pandoc 导出
- Mermaid、MathJax、KaTeX 的内置运行时
- 远程主题市场
- 无条件兼容所有 CommonMark/GFM 边界行为

## 5. 目标目录

```text
MarkdownRenderEngine/
├─ CMakeLists.txt
├─ cmake/
├─ include/mwrender/
│  ├─ ast.hpp
│  ├─ diagnostics.hpp
│  ├─ engine.hpp
│  ├─ options.hpp
│  ├─ parser.hpp
│  ├─ result.hpp
│  ├─ theme.hpp
│  └─ version.hpp
├─ src/
│  ├─ ast/
│  ├─ parser/
│  ├─ render/
│  ├─ security/
│  ├─ theme/
│  └─ support/
├─ apps/mwrender/
├─ themes/github-light/
│  ├─ theme.json
│  ├─ content.css
│  └─ code.css
├─ tests/
│  ├─ parser/
│  ├─ render/
│  ├─ security/
│  ├─ theme/
│  ├─ integration/
│  └─ snapshots/
├─ examples/
└─ docs/
```

## 6. 开发阶段

### Phase 0: 工程骨架

任务：

- 建立 CMake library、CLI 和 test targets。
- 确定编译器最低版本和 warning 级别。
- 添加格式化、静态检查和测试命令。
- 建立公开头文件与内部头文件边界。
- 添加版本号和 `mwrender --version`。

交付物：

- `mwrender_core` 静态/共享库目标。
- `mwrender` CLI 目标。
- 一个可运行的 smoke test。

验收：

- Windows 主开发环境可配置、编译和测试。
- 公共头文件可被独立示例程序包含。
- 核心库不链接 Qt。

### Phase 1: Source、Scanner 与 AST

任务：

- 统一输入为 UTF-8 字节序列。
- 实现 `SourceRange` 和行索引。
- 实现保留空行及 `LF`/`CRLF` 的 `LineScanner`。
- 定义 NodeType 和类型化 Node payload。
- 实现 AST visitor 和调试打印器。

验收：

- 每个块节点都能定位到源文本字节范围和行列。
- AST 所有权明确，无裸 owning pointer。
- 对空文档、无末尾换行和混合换行有测试。

### Phase 2: Block Parser

顺序建议：

1. fenced code block
2. ATX heading
3. thematic break
4. blockquote
5. list/list item
6. raw HTML block
7. paragraph

任务：

- 采用逐行容器状态机，不在规则间重复复制整段文本。
- 未闭合 fenced code block 解析至 EOF 并产生 warning。
- 段落保存待行内解析的原始内容。
- 记录列表起始编号、紧凑/宽松状态的扩展空间。

验收：

- 所有 v0.1 块级语法有正例、边界例和冲突例。
- 解析错误优先降级为文本或 warning，不崩溃。
- 1 MB 普通文本不会出现明显超线性增长。

### Phase 3: Inline Parser

任务：

- 文本和反斜杠转义。
- code span。
- emphasis/strong delimiter 处理。
- link/image。
- raw HTML inline。
- soft break/hard break。

验收：

- 嵌套 emphasis、code span 中的标记和链接属性转义有测试。
- 行内解析不重复解析代码内容。
- 不完整标记可预测地降级为文本。

### Phase 4: HTML Renderer

任务：

- 实现上下文相关转义。
- 实现所有 v0.1 AST 节点到 HTML 的映射。
- 支持 fragment 和 full-document。
- 支持 source attributes 开关。
- 实现标题 slug 和重复 ID 处理。
- 校验链接与图片 URL 协议。

验收：

- HTML snapshot 稳定且确定。
- 相同 AST 与选项始终产生相同输出。
- HTML 文本、属性和 URL 注入测试通过。
- Fragment 不包含 `<html>`、`<head>` 或全局 `<style>`。

### Phase 5: 主题系统

任务：

- 实现 `theme.json` schemaVersion 1。
- 校验主题 ID、入口文件和路径边界。
- 加载内置 `github-light`。
- 实现确定性的主题注册与覆盖规则。
- 生成 CSS 变量并拼接主题 CSS。
- 实现 `--theme`、`--theme-path` 和 `--list-themes`。

主题覆盖顺序由低到高：

```text
built-in
user
workspace
explicit API/CLI paths, in registration order
```

后注册的同 ID 主题覆盖先注册主题，并产生可查询的 diagnostic。

验收：

- 默认请求无需外部文件即可加载内置主题。
- 主题不存在时按 strict/fallback 规则处理。
- `../`、绝对 entry 路径和符号链接越界被拒绝。
- 主题文件读取有大小限制和清晰错误。

### Phase 6: HTML 与 CSS 扩展

任务：

- 实现 `Disabled`、`Sanitized`、`Trusted` HTML 策略。
- 定义可注入的 `HtmlSanitizer` 接口。
- 为 CSS 来源建模，不把所有 CSS 混成无来源字符串。
- 实现 CSS 合并顺序、远程资源限制和文件路径检查。
- 支持 API CSS text/file 与 CLI `--css`。

CSS 合并顺序由低到高：

```text
core
theme variables
theme content
theme code
user
workspace
document
request
```

验收：

- `Disabled` 对 HTML 原文进行转义。
- `Trusted` 仅在显式请求时透传。
- sanitizer 缺失行为符合宽松/严格模式定义。
- 默认配置不读取远程 CSS。
- CSS 文件无法越过允许根目录。

### Phase 7: GFM 与文档分析

任务：

- 增加 extension flags。
- 实现表格、任务列表、删除线和自动链接。
- 增加 Outline 与 WordStats。
- 支持 Front Matter 中受允许列表约束的配置。

验收：

- 每种扩展可独立启停。
- 禁用扩展时输入按基础 Markdown 规则降级。
- Front Matter 不可开启宿主应用明确禁用的能力。

### Phase 8: 稳定化

任务：

- 导入选定的 CommonMark/GFM conformance cases。
- 增加随机输入、深层嵌套和超长行测试。
- 建立 benchmark 基线。
- 检查 ABI/API 暴露面。
- 完善安装、包导出和使用示例。

验收：

- 所有公开功能有 API 或 CLI 集成测试。
- 解析器面对任意输入不崩溃、不死循环。
- 文档、示例和实际 API 一致。
- 发布清单列出已支持和未支持语法。

## 7. 配置优先级

最终值由低到高覆盖：

```text
library defaults
application config
workspace config
document front matter
per-render request or CLI
```

约束：

- 安全能力采用“上限”模型，不是简单覆盖。
- 例如宿主禁用 document CSS 后，Front Matter 不能重新开启。
- CLI/API 可以进一步收紧策略。
- `Trusted` HTML 必须由宿主或本次请求显式允许。

## 8. 测试矩阵

| 领域 | 主要测试 |
| --- | --- |
| Scanner | UTF-8、空行、LF/CRLF、无末尾换行、offset |
| Block parser | 规则优先级、嵌套、未闭合结构、退化文本 |
| Inline parser | delimiter、转义、嵌套、链接、代码 |
| Renderer | HTML/属性转义、fragment/full、slug、source map |
| Theme | schema、覆盖、fallback、路径越界、CSS 顺序 |
| Security | script、事件属性、危险 URL、远程 CSS、路径穿越 |
| CLI | 参数优先级、stdin/stdout、错误码、文件编码 |
| Performance | 大文件、长行、深嵌套、重复 delimiter |

Snapshot 只覆盖稳定输出；行为判断应优先使用结构化断言，避免所有测试都依赖
大段字符串。

## 9. 性能与资源目标

v0.1 不承诺跨机器固定毫秒数，先建立可重复 benchmark。必须满足：

- 扫描和常规解析的预期复杂度为 O(n)。
- 不因每个节点复制完整源文本。
- 主题在内容未变化时可缓存。
- 单次渲染不保留文档 AST 或输出的隐藏全局副本。
- 1 MB 文档可以完成解析和渲染。
- 对最大输入、最大嵌套深度和最大主题/CSS 文件设置可配置上限。

## 10. 发布里程碑

### Milestone A: 可解析

- Phase 0-3 完成。
- CLI 可输出 AST。
- 基础语法测试通过。

### Milestone B: 可渲染

- Phase 4 完成。
- Markdown 可稳定输出 fragment/full HTML。

### Milestone C: 可换肤

- Phase 5 完成。
- 内置默认主题与外部主题选择可用。

### Milestone D: 可扩展

- Phase 6-7 完成。
- HTML/CSS 策略和 GFM flags 可用。

### Milestone E: 可发布

- Phase 8 完成。
- 文档、安装、测试、基准和兼容性清单齐备。

## 11. 主要风险

| 风险 | 控制措施 |
| --- | --- |
| 自研解析器边界规则膨胀 | 明确子集，按官方示例逐项扩展 |
| emphasis/list 嵌套复杂 | 使用状态机和 delimiter/container 模型 |
| HTML sanitizer 被低估 | 独立接口，禁止正则清洗，默认 Disabled |
| CSS 被误认为可安全清洗 | 仅允许可信本地 CSS，默认禁远程资源 |
| 主题路径穿越 | canonical path + root containment 检查 |
| API 过早固化 | v0.x 使用 request/result 对象集中演进 |
| 快照测试过度耦合格式 | 结构断言与 snapshot 分工 |
| 为 Qt 提前加入 UI 依赖 | 核心保持纯 C++，Qt 只做上层适配 |

## 12. 完成定义

一个阶段只有同时满足以下条件才算完成：

1. 代码与公开接口已实现。
2. 正常、边界和失败路径已有测试。
3. 文档示例可编译或可运行。
4. CLI 与 API 行为一致。
5. 新增安全选项有默认值和降级规则。
6. 没有把尚未实现的能力写入兼容性声明。

## 13. 参考规范

- [CommonMark Specification 0.31.2](https://spec.commonmark.org/0.31.2/)
- [GitHub Flavored Markdown Specification](https://github.github.com/gfm/)
- [OWASP Cross Site Scripting Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cross_Site_Scripting_Prevention_Cheat_Sheet.html)

