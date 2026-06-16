# MWRender 功能总表

## 核心管线

| 功能 | 文件 | 状态 |
|---|---|---|
| **Markdown → AST → HTML** 完整管线 | `src/parser/parser.cpp` → `src/render/html_renderer.cpp` | ✅ |
| **Block 级解析**（Heading, Paragraph, BlockQuote, List, CodeBlock, ThematicBreak, HtmlBlock, Table） | `src/parser/parser.cpp` | ✅ |
| **Inline 级解析**（Strong, Emphasis, Strikethrough, InlineCode, Link, Image, AutoLink, Math, HtmlInline, 转义, SoftBreak, HardBreak） | `src/parser/parser.cpp` | ✅ |
| **GFM 扩展**（Table, TaskList, Strikethrough, AutoLink, TOC, Footnote） | `options.hpp` / `parser.cpp` | ✅ |
| **MathJax 数学公式**（`$x$` / `$$x$$`） | `parser.cpp` + `html_renderer.cpp` | ✅ |
| **Mermaid 图表**（\`\`\`mermaid 代码块） | `html_renderer.cpp` | ✅ |
| **代码高亮**（highlight.js 集成） | `html_renderer.cpp` | ✅ |
| **GitHub Alert**（`[!NOTE]`, `[!TIP]`, `[!IMPORTANT]`, `[!WARNING]`, `[!CAUTION]`） | `html_renderer.cpp` | ✅ |
| **图片对齐后缀**（`#center`, `#left`, `#right`） | `html_renderer.cpp` | ✅ |
| **Fragment / FullDocument 两种输出模式** | `outputMode` in `options.hpp` | ✅ |

## AST / 文档模型

| 功能 | 文件 | 状态 |
|---|---|---|
| **44 种 NodeType** | `ast.hpp` | ✅ |
| **SourceRange 源码位置**（offset + line + column） | `source.hpp` + `ast.hpp` | ✅ |
| **contentRange 内容纯净范围**（排除标记符号） | `ast.hpp` + `parser.cpp` | ✅ |
| **markerRanges 标记符号范围**（`##`, `**`, `- [ ]` 等） | `ast.hpp` + `parser.cpp` | ✅ |
| **稳定节点 ID**（内容哈希基础，同一输入同一 ID） | `ast.hpp` + `parser.cpp` | ✅ |
| **12 种 NodePayload**（HeadingData, ListData, ListItemData, CodeBlockData, LinkData, ImageData, HtmlData, TableCellData, FootnoteData, EmphasisData, StrongData, StrikethroughData） | `ast.hpp` | ✅ |

## 解析 API

| 功能 | 文件 | 状态 |
|---|---|---|
| **Engine::parse()** 独立解析入口 | `engine.hpp` / `engine.cpp` | ✅ |
| **Parser 类** 底层解析器 | `parser.hpp` / `parser.cpp` | ✅ |
| **UTF-8 处理**（BOM 剥离、无效序列检测与替换） | `parser.cpp` | ✅ |
| **错误诊断**（Error / Warning / Info + 范围 + 错误码） | `diagnostics.hpp` + `parser.cpp` | ✅ |
| **增量解析 Engine::update()**（TextChange → IncrementalParseResult） | `engine.hpp` / `engine.cpp` + `incremental.hpp` | ✅ |
| **Front Matter 解析**（title / theme / css） | `parser.cpp` | ✅ |

## HTML 渲染

| 功能 | 文件 | 状态 |
|---|---|---|
| **HtmlRenderer** 递归节点渲染 | `html_renderer.cpp` | ✅ |
| **HTML 转义**（`&`, `<`, `>`, `"`, `'`） | `html_renderer.cpp` | ✅ |
| **三种 HTML 策略**（Disabled / Sanitized / Trusted） | `options.hpp` + `sanitizer.cpp` | ✅ |
| **SafeHtmlSanitizer** 内置白名单净化器 | `sanitizer.cpp` | ✅ |
| **自定义 HtmlSanitizer 注入** | `engine.hpp` / `engine.cpp` | ✅ |
| **RenderMode::EditorView**（输出 `data-node-id` 属性） | `options.hpp` + `html_renderer.cpp` | ✅ |
| **RenderMode::Preview / Export** | `options.hpp` | ✅ |
| **SourceMapMode**（None / Line / Full） | `options.hpp` + `html_renderer.cpp` | ✅ |
| **Heading Slug 自动生成**（中文 + 去重） | `html_renderer.cpp` + `document_text.cpp` | ✅ |
| **renderNode() 局部渲染**（按节点 ID） | `engine.hpp` / `engine.cpp` | ✅ |
| **完整 HTML5 文档组装**（doctype + CSS + JS 嵌入） | `html_renderer.cpp` | ✅ |

## 序列化

| 功能 | 文件 | 状态 |
|---|---|---|
| **MarkdownSerializer** AST → Markdown 字符串 | `serializer.hpp` / `serializer.cpp` | ✅ |
| **serializeNode() 单节点序列化** | `serializer.hpp` / `serializer.cpp` | ✅ |
| **MarkdownStyle 风格保留**（bulletMarker, strongMarker, emphasisMarker, codeFenceMarker, lineEnding） | `serializer.hpp` / `serializer.cpp` | ✅ |
| **Engine::serialize()** 便利 API | `engine.hpp` / `engine.cpp` | ✅ |

## 结构化编辑命令

| 功能 | 文件 | 状态 |
|---|---|---|
| **Command::Type 命令枚举**（ToggleTask, SetHeadingLevel, ToggleList, WrapStrong, WrapEmphasis, InsertLink, InsertImage, ToggleStrikethrough, IndentListItem, OutdentListItem） | `serializer.hpp` | ✅ |
| **Engine::applyCommand()** 文档结构化修改 | `engine.hpp` / `engine.cpp` | ✅ |

## 节点查询

| 功能 | 文件 | 状态 |
|---|---|---|
| **findNodeBySourceOffset()**（mutable + const 重载） | `query.hpp` / `query.cpp` | ✅ |
| **findNodeById()**（mutable + const 重载） | `query.hpp` / `query.cpp` | ✅ |
| **findNodesByRange()**（mutable + const 重载） | `query.hpp` / `query.cpp` | ✅ |

## SourceMap 源码映射

| 功能 | 文件 | 状态 |
|---|---|---|
| **SourceMap::build()** 构建映射表 | `source_map.hpp` / `source_map.cpp` | ✅ |
| **visualToSource()** 视觉位置 → 源码 offset | `source_map.hpp` / `source_map.cpp` | ✅ |
| **sourceToVisual()** 源码 offset → 视觉位置 | `source_map.hpp` / `source_map.cpp` | ✅ |
| **标记隐藏后的偏移修正**（`**`, `` ` ``, `$`, `[]()`, `~~`, `![alt]()` 等） | `source_map.cpp` | ✅ |

## 主题系统

| 功能 | 文件 | 状态 |
|---|---|---|
| **ThemeManager 主题管理** | `theme_manager.hpp` / `theme_manager.cpp` | ✅ |
| **7 个内嵌 GitHub 主题**（Light, Light CB, Dark, Dark Dimmed, Dark CB, Dark HC, Auto） | `CMakeLists.txt`（embed） | ✅ |
| **外部主题加载**（`theme.json` + `content.css` 目录结构） | `theme_manager.cpp` | ✅ |
| **主题安全扫描**（`@import` 检测、远程 `url()` 拦截、`</style>` 逃逸检测） | `engine.cpp` | ✅ |
| **CSS 变量解析** | `theme_manager.cpp` | ✅ |
| **路径遍历防护** | `theme_manager.cpp` | ✅ |
| **主题注册优先级**（内置 < 显式 < 文档主题） | `theme_manager.cpp` | ✅ |

## 文档分析

| 功能 | 文件 | 状态 |
|---|---|---|
| **buildOutline()** 文档大纲树（层级标题） | `document_analysis.cpp` | ✅ |
| **buildWordStats()** 字数统计（字符、CJK、单词、段落、标题、代码行、图片、链接） | `document_analysis.cpp` | ✅ |

## 辅助工具

| 功能 | 文件 | 状态 |
|---|---|---|
| **JSON 解析器**（递归下降，无外部依赖） | `json.hpp` / `json.cpp` | ✅ |
| **plainText()** AST 子树纯文本提取 | `document_text.cpp` | ✅ |
| **slugBase()** URL 安全 slug 生成 | `document_text.cpp` | ✅ |
| **资源嵌入工具**（embed.cpp，将 CSS/JS 编译为 C++ 字节数组） | `tools/embed.cpp` | ✅ |

## CLI 工具

| 功能 | 文件 | 状态 |
|---|---|---|
| `mwrender` 命令行程序 | `apps/mwrender/main.cpp` | ✅ |
| 输入文件 / stdin 读取 | `main.cpp` | ✅ |
| 输出文件 / stdout 写入 | `main.cpp` | ✅ |
| `--fragment` / `--theme` / `--html-policy` 等选项 | `main.cpp` | ✅ |
| `--dump-ast` AST 树转储 | `main.cpp` | ✅ |
| `--list-themes` 主题列表 | `main.cpp` | ✅ |
| `--config` 配置文件支持（JSON） | `main.cpp` | ✅ |
| `--version` 版本显示 | `main.cpp` | ✅ |

## 测试套件（12 个测试 / 200+ 用例）

| 测试 | 文件 | 覆盖范围 |
|---|---|---|
| `mwrender.smoke` | `smoke_test.cpp` | 空文档、核心 Markdown、HTML 安全、主题、诊断、GFM 扩展、数学、脚注 |
| `mwrender.edge` | `edge_test.cpp` | 转义、CRLF、UTF-8 策略、自定义净化器、大纲、并发 8×50 轮 |
| `mwrender.conformance` | `conformance_test.cpp` | 60+ CommonMark/GFM 规范用例（22 组） |
| `mwrender.stress` | `stress_test.cpp` | 1MB 输入、10K 行、200 级嵌套、5K heading、2000 个未匹配 `*` |
| `mwrender.security` | `security_test.cpp` | 29 个 XSS/注入向量、URL 策略、CSS 逃逸、路径遍历 |
| `mwrender.snapshot` | `snapshot_test.cpp` | 4 组 HTML 快照比对 |
| `mwrender.new_features` | `new_features_test.cpp` | AST 新字段 14 项、RenderMode、Query API、增量解析、SourceMap、Serializer、命令、renderNode、大文档 |
| `mwrender.benchmark` | `benchmark.cpp` | 6 组性能基准（纯文本/GFM/短文档/标题/代码/完整文档） |
| `mwrender.cli.*` | CMake 脚本 | CLI 版本/主题列表/配置文件/stdin 管道 |

## 安全边界

| 功能 | 文件 | 状态 |
|---|---|---|
| 输入大小限制（16MB） | `options.hpp` | ✅ |
| 嵌套深度限制（256 级） | `options.hpp` | ✅ |
| 主题文件大小限制（2MB） | `options.hpp` | ✅ |
| 远程 CSS 资源拦截 | `engine.cpp` | ✅ |
| URL 安全校验（`isSafeUrl()`） | `html_renderer.cpp` | ✅ |
| 严格模式（`strictTheme`, `strictHtmlPolicy`） | `options.hpp` | ✅ |
| 诊断错误码（MW0001–MW5001） | 各文件 | ✅ |

## 公开 API 一览（34 个）

```
Engine::Engine()
Engine::parse()            // 解析入口
Engine::update()           // 增量重解析
Engine::render()           // 完整渲染
Engine::renderNode()       // 按 ID 局部渲染
Engine::serialize()        // AST → Markdown
Engine::applyCommand()     // 结构化编辑
Engine::addThemeRoot()
Engine::setHtmlSanitizer()
Engine::listThemes()

Parser::Parser()
Parser::parse()

findNodeBySourceOffset()   // 按 offset 查节点
findNodeById()             // 按 ID 查节点
findNodesByRange()         // 按范围查节点

serializeMarkdown()        // 自由函数序列化
serializeNode()            // 单节点序列化

SourceMap::build()         // 构建映射
SourceMap::visualToSource()
SourceMap::sourceToVisual()

SafeHtmlSanitizer          // 白名单净化器
ThemeManager               // 主题管理器
HtmlRenderer               // HTML 渲染器
```
