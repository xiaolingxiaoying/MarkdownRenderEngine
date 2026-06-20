# MarkdownRenderEngine 修改完善开发计划与目标

> 目标：把 MarkdownRenderEngine 从“Markdown → HTML 渲染器”逐步升级为“可支撑 Typora-like / Obsidian Live Preview 编辑器的 Markdown 文档结构引擎”。

生成日期：2026-06-20  
建议适用分支：`feature/expansion`

---

## 1. 当前项目定位

MarkdownRenderEngine 现在已经不只是普通渲染器。它已经具备这些重要基础：

- Markdown → AST → HTML 管线；
- GFM 表格、任务列表、删除线、自动链接、脚注、TOC；
- 数学公式、Mermaid、代码高亮、GitHub Alert；
- `Node.id`、`SourceRange`、`contentRange`、`markerRanges`；
- `Engine::parse()`；
- `Engine::render()`；
- `Engine::renderNode()`；
- `Engine::update()`；
- `MarkdownSerializer`；
- `Engine::applyCommand()`；
- `SourceMap`；
- `RenderMode::EditorView`。

这些能力说明项目已经开始从“渲染器”进入“编辑器内核”的方向。

但是它目前还不等于 Typora。Typora 的关键不是简单渲染 Markdown，而是：

```text
Markdown 源码
  ↓
AST 结构
  ↓
可编辑投影视图
  ↓
用户输入
  ↓
反向修改 Markdown
  ↓
局部更新界面
```

所以 MarkdownRenderEngine 后续的核心方向应该是：

```text
不要只做 Markdown -> HTML
而要做 Markdown -> AST -> Editor Projection -> Render Patch -> Markdown 回写
```

---

## 2. 总体目标

### 2.1 核心目标

将 MarkdownRenderEngine 升级为一个纯 C++ 的 Markdown 编辑器内核基础库。

它不负责 Qt、不负责 Electron、不负责具体 UI，但要给 UI 提供这些能力：

1. 维护 Markdown 源码；
2. 维护 AST；
3. 根据光标位置找到当前 Markdown 块；
4. 把 AST 节点转换成可编辑投影；
5. 把用户编辑操作转换成 Markdown 修改；
6. 只重新解析受影响块；
7. 只返回变化节点的 HTML patch；
8. 保持光标位置和源码位置映射；
9. 尽量保留用户原始 Markdown 风格；
10. 为 Qt / Electron / WebView 编辑器提供稳定协议。

### 2.2 不做什么

MarkdownRenderEngine 不应该直接做这些：

- 不内置 Qt 窗口；
- 不依赖 QWebEngine；
- 不直接操作浏览器 DOM；
- 不保存专有富文本格式；
- 不让 HTML 成为文档源数据；
- 不把所有编辑器 UI 逻辑塞进 `Engine`。

正确边界是：

```text
MarkdownRenderEngine:
  负责 Markdown 结构、解析、映射、局部更新、命令、序列化。

Qt / Electron App:
  负责窗口、菜单、键盘事件、DOM 显示、文件管理。
```

---

## 3. 新架构目标

建议目标架构如下：

```text
MarkdownRenderEngine
├── Core
│   ├── Parser
│   ├── AST
│   ├── HtmlRenderer
│   ├── ThemeManager
│   ├── Sanitizer
│   └── Serializer
│
├── EditorCore
│   ├── DocumentSession
│   ├── BlockIndex
│   ├── NodeIdRegistry
│   ├── NodeSourceMap
│   ├── EditorProjection
│   ├── RenderPatch
│   ├── EditCommand
│   ├── SelectionMap
│   └── IncrementalParser
│
└── Public API
    ├── Engine
    ├── Parser
    ├── DocumentSession
    ├── EditCommand API
    └── Patch API
```

其中最重要的是新增 `EditorCore`。

---

## 4. 设计原则

### 4.1 Markdown 是唯一真实数据

必须坚持：

```text
Markdown 字符串是唯一真实数据。
AST 是 Markdown 的结构化表示。
HTML / DOM 只是显示结果。
```

用户在界面上输入内容后，流程应该是：

```text
用户输入
  ↓
UI 生成 TextChange / EditCommand
  ↓
DocumentSession 修改 Markdown
  ↓
重新解析受影响范围
  ↓
更新 AST
  ↓
生成 RenderPatch
  ↓
UI 替换局部 DOM
```

不要这样做：

```text
用户编辑 DOM
  ↓
读取 DOM.innerHTML
  ↓
试图从 HTML 反推 Markdown
```

这会导致 Markdown 源码不可控。

### 4.2 先做块编辑，再做完整 Typora

不要一上来追求完整 Typora。第一阶段应该做：

```text
光标不在块内：显示渲染结果
光标进入块内：显示 Markdown 源码或者简单可编辑投影
光标离开块：重新渲染该块
```

这类似 Obsidian Live Preview，比完整 Typora 简单得多，也更容易做稳。

### 4.3 可回退设计

增量解析一开始不用支持所有语法。

可以这样：

```text
简单块：局部解析
复杂块：回退到父块解析
特别复杂结构：回退全文解析
```

但是每次回退都要有标记，方便后续优化。

---

## 5. 阶段 0：整理现有 feature/expansion 分支

### 5.1 目标

先把当前已有能力整理清楚，避免后续越改越乱。

### 5.2 任务

1. 整理 `FEATURES.md`；
2. 新增正式设计文档；
3. 拆分过大的测试文件；
4. 修正 `Engine::update()` 的语义；
5. 明确哪些 API 是稳定的，哪些是 experimental；
6. 补齐 `contentRange` / `markerRanges` / `Node.id` 的文档。

### 5.3 建议新增文档

```text
docs/EDITOR_CORE_DESIGN.md
docs/INCREMENTAL_PARSING.md
docs/EDITOR_PROJECTION.md
docs/SELECTION_MAPPING.md
docs/RENDER_PATCH_PROTOCOL.md
docs/COMMAND_EDITING.md
```

### 5.4 验收标准

- 文档能解释清楚“Markdown 是唯一真实数据”；
- 能解释清楚 `range`、`contentRange`、`markerRanges` 的区别；
- 能解释清楚 `RenderMode::Preview`、`EditorView`、`Export` 的区别；
- `Engine::update()` 不再把所有仍存在节点都算成 changed。

---

## 6. 阶段 1：新增 DocumentSession

### 6.1 为什么需要 DocumentSession

当前 `Engine::render()` 更适合一次性渲染：

```text
输入完整 Markdown -> 输出完整 HTML
```

编辑器需要长期维护状态：

```text
当前 Markdown
当前 AST
当前 block 索引
当前 nodeId 映射
当前 revision
当前 SourceMap
```

所以需要 `DocumentSession`。

### 6.2 建议 API

新增：

```text
include/mwrender/editor/document_session.hpp
src/editor/document_session.cpp
```

初步 API：

```cpp
namespace mwrender::editor {

struct DocumentSessionOptions {
    ParseOptions parseOptions;
    RenderOptions renderOptions;
    bool enableIncrementalParsing = true;
    bool fallbackFullReparse = true;
};

struct UpdateResult {
    bool ok = false;
    bool fullReparse = false;
    std::size_t revision = 0;
    std::vector<Diagnostic> diagnostics;
    std::vector<std::string> changedNodeIds;
    std::vector<std::string> insertedNodeIds;
    std::vector<std::string> removedNodeIds;
};

class DocumentSession {
public:
    explicit DocumentSession(DocumentSessionOptions options = {});

    void load(std::string markdown);

    const std::string& markdown() const;
    const Node& document() const;
    std::size_t revision() const;

    UpdateResult applyChange(const TextChange& change);
    UpdateResult applyCommand(const EditCommand& command);

    const Node* findNodeById(std::string_view nodeId) const;
    const Node* findNodeAtOffset(std::size_t sourceOffset) const;
};

}
```

### 6.3 内部状态

```cpp
std::string markdown_;
std::unique_ptr<Node> document_;
std::size_t revision_ = 0;
BlockIndex blockIndex_;
NodeIdRegistry nodeIds_;
NodeSourceMap sourceMap_;
std::unordered_map<std::string, const Node*> nodeMap_;
```

### 6.4 第一版实现策略

第一版可以内部仍然全文 parse，但 API 要先定好。

也就是：

```text
先让 DocumentSession 工作起来
再逐步把内部从全文解析改成块级解析
```

### 6.5 测试

新增：

```text
tests/editor/document_session_test.cpp
```

测试内容：

- load 后能返回 markdown；
- load 后能返回 AST；
- applyChange 后 revision 增加；
- applyChange 后 markdown 正确变化；
- findNodeById 能找到节点；
- findNodeAtOffset 能找到当前块。

---

## 7. 阶段 2：新增 BlockIndex

### 7.1 目标

让引擎能通过光标位置快速知道当前处于哪个 Markdown 块。

### 7.2 为什么重要

如果用户只在一个段落输入一个字，不应该重新解析全文。

应该：

```text
输入 offset
  ↓
找到当前 block
  ↓
只重新解析当前 block 或当前父容器
```

### 7.3 建议 API

新增：

```text
include/mwrender/editor/block_index.hpp
src/editor/block_index.cpp
```

```cpp
namespace mwrender::editor {

struct BlockEntry {
    std::string nodeId;
    NodeType type;
    SourceRange range;
    SourceRange contentRange;
    std::size_t depth = 0;
};

class BlockIndex {
public:
    void rebuild(const Node& document);

    const BlockEntry* blockAtOffset(std::size_t sourceOffset) const;
    const BlockEntry* blockById(std::string_view nodeId) const;

    SourceRange affectedRangeForChange(const TextChange& change) const;

    const std::vector<BlockEntry>& blocks() const;

private:
    std::vector<BlockEntry> blocks_;
};

}
```

### 7.4 安全重解析边界

不同块有不同边界：

| 类型 | 第一版策略 |
|---|---|
| Heading | 当前行 |
| Paragraph | 当前段落 |
| ThematicBreak | 当前行 |
| CodeBlock | 整个 fenced code block |
| MathBlock | 整个 `$$` 块 |
| Table | 整个表格 |
| List | 整个列表 |
| BlockQuote | 整个引用块 |
| HtmlBlock | 整个 HTML block |
| Mermaid | 整个 fenced mermaid block |

第一版可以保守：

```text
普通段落、标题：局部解析
列表、引用、表格：回退父块或全文解析
```

### 7.5 测试

新增：

```text
tests/editor/block_index_test.cpp
```

测试：

- offset 在标题内能找到 heading；
- offset 在段落内能找到 paragraph；
- offset 在 fenced code 内能找到 code block；
- offset 在表格内能找到 table；
- 修改段落只返回段落范围；
- 修改代码块只返回代码块范围。

---

## 8. 阶段 3：NodeIdRegistry 与稳定节点 ID

### 8.1 当前问题

如果节点 ID 完全依赖内容 hash，那么用户改一个字，节点 ID 就可能变化。

这样 UI 会认为：

```text
旧节点删除
新节点插入
```

这不利于保持光标、滚动、折叠状态。

### 8.2 目标

区分两个概念：

```text
node.id：编辑会话内稳定 ID
node.contentHash：判断内容是否变化
```

### 8.3 建议修改 AST

在 `Node` 中增加：

```cpp
std::string contentHash;
```

保留：

```cpp
std::string id;
```

含义：

```text
id：用于 DOM 绑定，尽量稳定
contentHash：用于判断节点内容是否变化
```

### 8.4 NodeIdRegistry

新增：

```text
include/mwrender/editor/node_id_registry.hpp
src/editor/node_id_registry.cpp
```

```cpp
class NodeIdRegistry {
public:
    void inheritIds(const Node& oldRoot, Node& newRoot);
    std::string allocate(NodeType type);

private:
    std::unordered_map<NodeSignature, std::string> oldIds_;
};
```

### 8.5 ID 继承策略

优先级：

1. 相同类型 + 相同 source range；
2. 相同父节点 + 相似内容；
3. 相同结构路径；
4. 分配新 ID。

### 8.6 测试

新增：

```text
tests/editor/node_id_registry_test.cpp
```

测试：

- 修改段落内容后，段落 ID 尽量保持；
- 新增段落后，旧段落 ID 保持；
- 删除段落后，其他段落 ID 保持；
- 移动复杂结构时，不出现重复 ID。

---

## 9. 阶段 4：真正的增量解析

### 9.1 目标

让 `DocumentSession::applyChange()` 不再默认全文解析。

目标流程：

```text
TextChange
  ↓
BlockIndex 定位受影响范围
  ↓
局部替换 Markdown
  ↓
局部解析
  ↓
AST patch
  ↓
NodeId 继承
  ↓
返回 changed / inserted / removed
```

### 9.2 第一版支持范围

先支持：

- Heading；
- Paragraph；
- ThematicBreak；
- Fenced CodeBlock；
- MathBlock。

复杂结构先回退：

- List；
- BlockQuote；
- Table；
- HtmlBlock；
- Footnote；
- Link Reference。

### 9.3 API

扩展 `UpdateResult`：

```cpp
struct UpdateResult {
    bool ok = false;
    bool fullReparse = false;
    bool partialReparse = false;
    SourceRange affectedRange;

    std::unique_ptr<Node> document; // 可选
    std::vector<Diagnostic> diagnostics;

    std::vector<std::string> changedNodeIds;
    std::vector<std::string> insertedNodeIds;
    std::vector<std::string> removedNodeIds;
};
```

### 9.4 实现步骤

1. 先实现 `affectedRangeForChange()`；
2. 实现局部 Markdown 提取；
3. 给局部 Markdown 包一层临时 Document 解析；
4. 把新节点替换回旧 AST；
5. 更新后续节点 offset；
6. 重新建立 `BlockIndex`；
7. 重新建立 `nodeMap_`；
8. 生成 patch 信息。

### 9.5 测试

新增：

```text
tests/editor/incremental_parse_test.cpp
```

测试：

- 修改标题只影响标题；
- 修改段落只影响段落；
- 修改代码块内部只影响代码块；
- 修改表格时允许 fullReparse；
- 修改列表时允许 fullReparse；
- 修改普通段落不允许 fullReparse；
- 1MB 文档中修改一个段落，更新时间明显低于全文解析。

---

## 10. 阶段 5：EditorProjection

### 10.1 目标

把 AST 节点变成适合编辑器显示的 HTML，而不是普通预览 HTML。

普通渲染：

```html
<h1>标题</h1>
```

编辑器投影：

```html
<h1
  data-node-id="n1"
  data-source-start="0"
  data-source-end="5"
  data-content-start="2"
  data-content-end="5"
  contenteditable="true">
  标题
</h1>
```

### 10.2 新增模块

```text
include/mwrender/editor/editor_projection.hpp
src/editor/editor_projection.cpp
```

### 10.3 核心结构

```cpp
enum class ProjectionMode {
    Editable,
    SourceEditable,
    Atomic,
    Hidden,
    Unsupported
};

enum class ProjectionSegmentKind {
    Content,
    HiddenMarker,
    VisibleMarker,
    Atomic,
    LineBreak
};

struct ProjectionSegment {
    std::string projectionId;
    std::string nodeId;
    SourceRange sourceRange;
    ProjectionSegmentKind kind;
    std::string text;
};

struct BlockProjection {
    std::string nodeId;
    NodeType type;
    ProjectionMode mode;
    SourceRange sourceRange;
    SourceRange contentRange;
    std::vector<ProjectionSegment> segments;
    std::string html;
};
```

### 10.4 分级策略

#### 第一批：Editable

- Paragraph；
- Heading；
- Text；
- SoftBreak；
- HardBreak。

#### 第二批：SourceEditable

光标进入后显示源码：

- Strong；
- Emphasis；
- InlineCode；
- Strikethrough；
- Link；
- Image；
- List；
- BlockQuote；
- TaskList。

#### 第三批：Atomic

先整体显示，不直接编辑：

- CodeBlock；
- MathBlock；
- Mermaid；
- Table；
- HtmlBlock；
- FootnoteDef。

### 10.5 测试

新增：

```text
tests/editor/editor_projection_test.cpp
```

测试：

- heading 生成 data-node-id；
- paragraph 生成可编辑投影；
- `**bold**` 能生成 hidden marker；
- link 能保留 source range；
- code block 是 atomic；
- table 是 atomic；
- sourceStart/sourceEnd 与原 Markdown 对应。

---

## 11. 阶段 6：Node-aware SourceMap / SelectionMap

### 11.1 目标

解决光标映射问题。

需要支持：

```text
屏幕位置 -> Markdown offset
Markdown offset -> 屏幕位置
```

更准确地说：

```text
nodeId + visual offset -> source offset
source offset -> nodeId + visual offset
```

### 11.2 新增模块

```text
include/mwrender/editor/selection_map.hpp
src/editor/selection_map.cpp
```

### 11.3 API

```cpp
struct VisualPosition {
    std::string nodeId;
    std::size_t visualOffset = 0;
    Affinity affinity = Affinity::After;
};

struct SourcePositionEx {
    std::size_t offset = 0;
    Affinity affinity = Affinity::After;
    std::string contextNodeId;
};

struct Selection {
    SourcePositionEx anchor;
    SourcePositionEx focus;
};

class SelectionMap {
public:
    void build(const BlockProjection& projection);

    SourcePositionEx visualToSource(const VisualPosition& visual) const;
    VisualPosition sourceToVisual(const SourcePositionEx& source) const;
};
```

### 11.4 必须支持的映射

- `# 标题` 隐藏 `# `；
- `**bold**` 隐藏 `**`；
- `*em*` 隐藏 `*`；
- `` `code` `` 隐藏反引号；
- `[text](url)` 显示 text，隐藏语法；
- `![alt](url)` 显示图片或 alt，隐藏语法；
- `- [ ] task` checkbox 与源码对应；
- 空段落占位符；
- 中文输入法 composition 不打断。

### 11.5 测试

新增：

```text
tests/editor/selection_map_test.cpp
```

测试：

- `**bold**` 中 visual 0 对应 source 2；
- heading 内容开头映射正确；
- link 文本映射正确；
- hidden marker 边界映射正确；
- 删除时 affinity 正确；
- 中文字符 UTF-8/UTF-16 映射正确。

---

## 12. 阶段 7：RenderPatch 协议

### 12.1 目标

不要每次返回整篇 HTML。

应该返回：

```text
哪个节点变了
这个节点的新 HTML 是什么
哪些节点删除了
哪些节点新增了
光标应该恢复到哪里
```

### 12.2 新增模块

```text
include/mwrender/editor/render_patch.hpp
src/editor/render_patch.cpp
```

### 12.3 API

```cpp
struct NodeHtmlPatch {
    std::string nodeId;
    std::string html;
    ProjectionMode mode;
    SourceRange sourceRange;
};

struct RenderPatch {
    std::size_t revision = 0;
    std::vector<std::string> removedNodeIds;
    std::vector<NodeHtmlPatch> insertedNodes;
    std::vector<NodeHtmlPatch> changedNodes;
    std::optional<Selection> selection;
};
```

### 12.4 UI 协议

UI 只需要做：

```text
removedNodeIds -> 删除 DOM
changedNodes -> 按 nodeId 替换 DOM
insertedNodes -> 插入 DOM
selection -> 恢复光标
```

### 12.5 测试

新增：

```text
tests/editor/render_patch_test.cpp
```

测试：

- 修改段落只返回一个 changed node；
- 删除段落返回 removed node；
- 新增段落返回 inserted node；
- 修改标题不刷新全文；
- patch 中带 revision；
- stale patch 可以被 UI 忽略。

---

## 13. 阶段 8：EditCommand / 结构化编辑

### 13.1 目标

用户操作不要直接改 DOM，而是变成命令。

例如：

```text
Ctrl+B -> WrapStrong
点击 checkbox -> ToggleTask
Enter -> SplitBlock
Backspace at block start -> MergeBlock
```

### 13.2 新增或扩展

```text
include/mwrender/editor/edit_command.hpp
src/editor/edit_command.cpp
```

### 13.3 命令类型

```cpp
enum class EditCommandType {
    ReplaceSelection,
    DeleteBackward,
    DeleteForward,
    SplitBlock,
    MergeBlock,
    ToggleStrong,
    ToggleEmphasis,
    ToggleInlineCode,
    ToggleStrikethrough,
    InsertLink,
    InsertImage,
    ToggleTask,
    SetHeadingLevel,
    ToggleList,
    IndentListItem,
    OutdentListItem
};
```

### 13.4 命令输入

```cpp
struct EditCommand {
    EditCommandType type;
    Selection selection;
    std::string nodeId;
    std::string text;
    std::string arg1;
    std::string arg2;
};
```

### 13.5 执行结果

```cpp
struct EditCommandResult {
    bool ok = false;
    std::string markdown;
    Selection newSelection;
    std::vector<Diagnostic> diagnostics;
};
```

### 13.6 第一批必须实现

- ReplaceSelection；
- DeleteBackward；
- DeleteForward；
- SplitBlock；
- MergeBlock；
- ToggleStrong；
- ToggleEmphasis；
- ToggleTask；
- SetHeadingLevel。

### 13.7 测试

新增：

```text
tests/editor/edit_command_test.cpp
```

测试：

- 输入文字；
- 删除文字；
- 选中文本加粗；
- 取消加粗；
- Enter 拆分段落；
- Backspace 合并段落；
- task list 勾选；
- heading level 改变；
- 中文输入不破坏 offset。

---

## 14. 阶段 9：Serializer 风格保留

### 14.1 目标

编辑后保存 Markdown 时，不要无意义改变用户原始风格。

例如：

```markdown
* item
* item
```

不要变成：

```markdown
- item
- item
```

例如：

```markdown
__bold__
```

不要变成：

```markdown
**bold**
```

除非用户明确触发格式化。

### 14.2 扩展 MarkdownStyle

```cpp
struct MarkdownStyle {
    std::string bulletMarker = "-";
    std::string strongMarker = "**";
    std::string emphasisMarker = "*";
    std::string codeFenceMarker = "```";
    std::string lineEnding = "\n";

    bool preserveOriginalMarkers = true;
    bool preserveBlankLines = true;
    bool preserveListStartNumber = true;
    bool preserveTaskMarkerCase = true;
};
```

### 14.3 AST 记录原始 marker

扩展 payload：

```cpp
struct ListData {
    bool ordered = false;
    std::uint64_t start = 1;
    bool tight = true;
    std::string marker;
};

struct StrongData {
    char delimiter = '*';
    std::string marker;
};
```

### 14.4 测试

新增：

```text
tests/editor/serializer_preserve_test.cpp
```

测试：

- `* item` 保持 `*`；
- `+ item` 保持 `+`；
- `__bold__` 保持 `__`；
- CRLF 文档保持 CRLF；
- 修改一个段落不影响其他段落空行风格。

---

## 15. 阶段 10：补 CommonMark/GFM 兼容

### 15.1 为什么要补

如果语法解析不稳定，编辑器会出现：

- 光标定位错误；
- 块范围错误；
- 局部更新错误；
- 序列化破坏原文；
- 列表和引用编辑错乱。

### 15.2 优先级

第一优先级：

1. nested list；
2. multi-paragraph list item；
3. nested blockquote；
4. complete delimiter stack；
5. link reference definitions。

第二优先级：

1. HTML entity decoding；
2. Setext heading；
3. indented code block；
4. bare URL autolink；
5. definition list；
6. custom container。

### 15.3 测试

继续扩展：

```text
tests/conformance_test.cpp
```

并为编辑器新增对应测试：

```text
tests/editor/complex_list_edit_test.cpp
tests/editor/blockquote_edit_test.cpp
tests/editor/delimiter_stack_edit_test.cpp
```

---

## 16. 阶段 11：性能目标

### 16.1 性能原则

编辑器的关键不是“整篇渲染很快”，而是：

```text
用户输入一个字时，当前帧不卡。
```

### 16.2 目标指标

| 场景 | 目标 |
|---|---|
| 1MB 文档首次解析 | < 1000ms |
| 1MB 文档首次可见内容 | < 200ms |
| 普通段落输入一个字 | < 16ms |
| 普通段落局部 render patch | < 8ms |
| 光标恢复 | < 4ms |
| 不含复杂块的局部编辑 | 不触发 full reparse |
| 大文档滚动 | 不因 MathJax/Mermaid 阻塞 |

### 16.3 Benchmark

新增：

```text
tests/editor/editor_benchmark.cpp
```

Benchmark 场景：

- 1000 段落；
- 5000 标题；
- 100 个代码块；
- 100 个数学块；
- 100 个 Mermaid；
- 100 个表格；
- 1MB 混合文档；
- 随机修改 100 次。

---

## 17. 阶段 12：与 MWRenderQtDemo 集成

### 17.1 当前 Qt Demo 问题

Qt Demo 现在已经有自己的 `DocumentSession`、`BlockIndex`、`EditorProjection`、`RenderScheduler` 等模块。

但是这些能力更应该沉淀到 MarkdownRenderEngine 中。Qt Demo 只保留 UI 适配层。

### 17.2 改造目标

Qt Demo 变成：

```text
QWebEngine
  ↓
JS 捕获输入
  ↓
QWebChannel 发送 EditCommand
  ↓
MarkdownRenderEngine::DocumentSession
  ↓
返回 RenderPatch
  ↓
JS 替换局部 DOM
```

### 17.3 Qt Demo 应该删除或弱化的逻辑

从 Qt Demo 中逐步移出：

- MarkdownBuffer；
- BlockParser；
- BlockIndex；
- OffsetMapper；
- EditorProjection；
- 部分 EditCommand 逻辑。

这些应该进入 MarkdownRenderEngine。

Qt Demo 保留：

- MainWindow；
- WysiwygEditorWidget；
- QWebChannel bridge；
- editor.html/css/js；
- 文件打开保存；
- 菜单；
- 快捷键；
- UI theme。

---

## 18. 推荐代码目录

建议在 MarkdownRenderEngine 中加入：

```text
include/mwrender/editor/
    document_session.hpp
    block_index.hpp
    node_id_registry.hpp
    editor_projection.hpp
    selection_map.hpp
    render_patch.hpp
    edit_command.hpp
    editor_options.hpp

src/editor/
    document_session.cpp
    block_index.cpp
    node_id_registry.cpp
    editor_projection.cpp
    selection_map.cpp
    render_patch.cpp
    edit_command.cpp

tests/editor/
    document_session_test.cpp
    block_index_test.cpp
    node_id_registry_test.cpp
    incremental_parse_test.cpp
    editor_projection_test.cpp
    selection_map_test.cpp
    render_patch_test.cpp
    edit_command_test.cpp
    serializer_preserve_test.cpp
```

---

## 19. 推荐开发顺序

最推荐的实际提交顺序：

```text
1. docs: add editor core architecture documents
2. refactor: clarify Node id/contentHash semantics
3. feat: add DocumentSession shell
4. feat: add BlockIndex
5. feat: add nodeId -> Node* map
6. fix: correct Engine::update changedNodeIds semantics
7. feat: add EditorProjection for Heading/Paragraph
8. feat: add RenderPatch API
9. feat: add SelectionMap
10. feat: add ReplaceSelection/Delete/Split/Merge commands
11. feat: add block-level incremental parsing for Heading/Paragraph
12. feat: add ToggleStrong/ToggleEmphasis/ToggleTask
13. perf: add editor benchmark
14. docs: add Qt Demo integration guide
```

---

## 20. 里程碑

### v0.2：编辑器内核基础

完成：

- DocumentSession；
- BlockIndex；
- nodeMap；
- EditorProjection 基础；
- RenderPatch 基础；
- SelectionMap 基础；
- ReplaceSelection / Delete / SplitBlock。

验收：

- 普通段落可以局部编辑；
- 普通标题可以局部编辑；
- 编辑一个段落不刷新全文；
- patch 能被 Qt Demo 使用。

### v0.3：块级增量解析

完成：

- Heading 局部解析；
- Paragraph 局部解析；
- CodeBlock 局部解析；
- MathBlock 局部解析；
- 复杂结构回退；
- NodeId 继承。

验收：

- 1MB 文档中编辑普通段落不 full reparse；
- nodeId 不乱变；
- 光标能恢复。

### v0.4：Live Preview 支撑

完成：

- SourceEditable 模式；
- hidden marker；
- strong/emphasis/link 映射；
- ToggleStrong；
- ToggleEmphasis；
- InsertLink；
- ToggleTask。

验收：

- 类似 Obsidian Live Preview；
- 光标所在块可编辑；
- 非当前块渲染显示；
- Markdown 源码保持正确。

### v0.5：Typora-like 基础

完成：

- 更多行内语法直接编辑；
- nested list；
- nested blockquote；
- delimiter stack；
- table atomic editor；
- IME 稳定策略；
- Undo/Redo 建议协议。

验收：

- 能稳定编辑中等复杂 Markdown；
- 不破坏源码；
- UI 不明显卡顿。

---

## 21. 风险与处理

### 风险 1：过早追求完整 Typora

处理：

```text
先做 Live Preview，再做 Typora-like。
```

### 风险 2：UI 逻辑污染核心库

处理：

```text
核心库只输出结构和 patch，不依赖 Qt/DOM。
```

### 风险 3：增量解析太难

处理：

```text
先实现简单块增量，复杂块回退全文解析。
```

### 风险 4：光标映射复杂

处理：

```text
先支持 Heading/Paragraph/Text，再扩展 Strong/Link/List。
```

### 风险 5：Markdown 风格被破坏

处理：

```text
AST 中保存原始 marker，Serializer 默认 preserve。
```

---

## 22. 最终目标

最终希望 MarkdownRenderEngine 变成：

```text
一个纯 C++ Markdown 文档结构引擎。
```

它不仅能做：

```text
Markdown -> HTML
```

还要能做：

```text
Markdown -> AST
AST -> 可编辑投影
用户操作 -> Markdown 修改
Markdown 修改 -> 局部 AST 更新
AST 更新 -> 局部 HTML patch
源码位置 <-> 光标位置
AST -> Markdown 序列化
```

一句话总结：

> MarkdownRenderEngine 的下一阶段目标，不是继续堆渲染功能，而是成为 Typora-like / Obsidian-like Markdown 编辑器的底层文档引擎。

---

## 23. 最小可用路线

如果只想快速做出能用版本，按这个最小路线：

```text
1. DocumentSession
2. BlockIndex
3. 当前块源码编辑
4. 当前块局部渲染
5. RenderPatch
6. 光标映射
7. ReplaceSelection / Delete / SplitBlock
```

做到这一步，你就能做出：

```text
光标所在块可编辑
其他块渲染显示
编辑不卡
Markdown 保存正确
```

这就是一个实用的 Markdown Live Preview 编辑器内核。
