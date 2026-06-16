结论先说清楚：**你的 MarkdownRenderEngine 现在如果只是 `Markdown -> HTML`，它还只能算“渲染器”；要支撑 Typora 式所见即所得，它至少要升级成“Markdown 解析 + 结构建模 + 源码映射 + 可逆序列化”的内核。**

但也要注意：**不建议让 MarkdownRenderEngine 直接承担完整编辑器职责**。它应该提供“Markdown 结构能力”，而光标、输入法、撤销、选择区、DOM 交互，最好交给 Electron 前端里的编辑器层处理。

---

# 1. 你的 MarkdownRenderEngine 现在大概处于什么阶段

你现在的能力大概是：

```cpp
Markdown -> HTML
Markdown -> Fragment HTML
Markdown + Theme -> Styled HTML
```

这适合：

```text
预览
导出 HTML
导出 PDF
主题渲染
块级渲染
```

但 Typora 式编辑需要的是：

```text
Markdown 源码
↓
解析成结构化文档
↓
在结构化文档上编辑
↓
再稳定地保存回 Markdown
```

所以目标要从：

```text
MarkdownRenderEngine = Markdown 渲染器
```

升级为：

```text
MarkdownRenderEngine = Markdown 文档结构引擎
```

---

# 2. 最核心要补的能力

你至少需要补这几大模块：

```text
1. Lossless AST / CST 解析
2. SourceMap 源码位置映射
3. Block 增量解析
4. Inline 级语法解析
5. Markdown 语义节点模型
6. Markdown 反向序列化
7. 节点稳定 ID
8. 局部更新与 Diff
9. 编辑器友好的诊断与容错
10. 特殊节点交互模型
```

下面详细讲。

---

# 3. 第一项：不能只输出 HTML，要输出 AST / CST

现在你可能是：

```cpp
RenderResult result = engine.render(request);
std::cout << result.html;
```

这只能用于预览。

所见即所得需要：

```cpp
ParseResult result = engine.parse(markdown);
```

然后返回结构：

```cpp
Document
├── Heading(level=1)
├── Paragraph
├── BulletList
├── TaskList
├── CodeBlock
├── Blockquote
└── Table
```

例如 Markdown：

```markdown
# 标题

这是 **加粗** 文本。

- [ ] 写渲染器
```

应该解析为：

```text
Document
├── Heading
│   ├── level: 1
│   └── text: 标题
├── Paragraph
│   ├── Text: 这是 
│   ├── Strong: 加粗
│   └── Text:  文本。
└── TaskList
    └── TaskItem
        ├── checked: false
        └── text: 写渲染器
```

这一步非常关键。

因为 Typora 的编辑不是在 HTML 字符串上编辑，而是在这些语义节点上编辑。

---

# 4. 第二项：需要 Lossless AST / CST，而不是普通 AST

普通 AST 只关心语义。

例如：

```markdown
## Hello
```

普通 AST 可能只保存：

```text
Heading(level=2, text="Hello")
```

但所见即所得还需要知道它原来长什么样：

```markdown
## Hello
```

所以还要保存：

```text
## 的位置
空格的位置
Hello 的位置
整行的源码范围
```

也就是：

```cpp
struct Node {
    NodeType type;

    size_t sourceStart;
    size_t sourceEnd;

    size_t contentStart;
    size_t contentEnd;

    std::vector<Range> markerRanges;
    std::vector<Node> children;
};
```

对于：

```markdown
## Hello
```

应该得到：

```text
type: Heading
level: 2
sourceRange: [0, 8]
markerRange: [0, 2]      // ##
contentRange: [3, 8]     // Hello
```

对于：

```markdown
这是 **bold** 文本
```

应该得到：

```text
Paragraph
├── Text
│   └── sourceRange: 0..3
├── Strong
│   ├── sourceRange: 3..11
│   ├── openingMarker: 3..5
│   ├── contentRange: 5..9
│   └── closingMarker: 9..11
└── Text
    └── sourceRange: 11..14
```

这就是 **Lossless AST / CST**。

其中：

```text
AST = 语义树
CST = 保留源码细节的语法树
```

Typora 式编辑更需要 CST，因为它必须在隐藏 Markdown 符号的同时，还能保存回 Markdown。

---

# 5. 第三项：SourceMap 源码映射

所见即所得最大的问题是：

```text
用户看到的位置 ≠ Markdown 源码位置
```

例如源码是：

```markdown
这是 **加粗文字**
```

用户看到的是：

```text
这是 加粗文字
```

视觉里没有 `**`，但源码里有。

所以你需要维护映射：

```text
视觉位置 -> Markdown 源码 offset
Markdown 源码 offset -> 视觉位置
```

这叫 SourceMap。

你需要提供类似 API：

```cpp
SourcePosition visualToSource(NodeId nodeId, VisualPosition pos);

VisualPosition sourceToVisual(size_t sourceOffset);
```

例如：

```markdown
这是 **bold**
```

源码位置：

```text
这是空格 ** bold **
0 1 2 3 4 5 6 7 8 9 10
```

视觉位置：

```text
这是空格 bold
0 1 2 3 4 5 6
```

`**` 被隐藏后，光标映射必须重新计算。

没有 SourceMap，就会出现这些问题：

```text
光标跳动
选区错位
删除错误字符
撤销后位置混乱
点击渲染内容无法定位源码
```

---

# 6. 第四项：Block 级增量解析

所见即所得编辑器不能每输入一个字符就重新解析整篇文档。

你需要支持：

```text
只重新解析受影响的块
只重新渲染受影响的节点
保持其他节点不变
```

比如文档有 10000 行，用户只修改第 500 行，那么只应该更新相关 block。

你需要提供：

```cpp
ParseResult parseBlocks(std::string_view markdown);

IncrementalParseResult update(
    Document oldDoc,
    TextChange change
);
```

`TextChange` 可以类似：

```cpp
struct TextChange {
    size_t from;
    size_t to;
    std::string insertedText;
};
```

更新结果：

```cpp
struct IncrementalParseResult {
    Document newDoc;
    std::vector<NodeId> changedNodes;
    std::vector<NodeId> removedNodes;
    std::vector<NodeId> insertedNodes;
};
```

这对大文件性能非常重要。

---

# 7. 第五项：节点稳定 ID

如果每次解析都重新生成所有节点，前端编辑器会很痛苦。

比如用户正在编辑第三段：

```markdown
第一段

第二段

第三段
```

输入一个字符后，如果所有节点 ID 都变了，编辑器就很难保持：

```text
光标位置
选区
折叠状态
滚动位置
块缓存
局部渲染结果
```

所以你需要稳定节点 ID。

例如：

```cpp
struct Node {
    std::string id;
    NodeType type;
    Range sourceRange;
    std::vector<Node> children;
};
```

稳定 ID 可以根据：

```text
节点类型
源码范围
内容哈希
父节点关系
增量解析继承
```

综合生成。

例如：

```text
heading_001
paragraph_002
task_item_003
codeblock_004
```

当用户只修改 paragraph_002 的内容时，其他节点 ID 不应该变化。

---

# 8. 第六项：Inline 级解析能力

你的块渲染模式一开始只需要识别块：

```text
标题块
段落块
列表块
代码块
引用块
```

但 Typora 式编辑必须识别行内结构：

```text
加粗
斜体
删除线
行内代码
链接
图片
自动链接
HTML inline
转义字符
数学公式
```

例如：

```markdown
这是 **加粗** 和 `代码` 以及 [链接](https://example.com)
```

应该解析为：

```text
Paragraph
├── Text("这是 ")
├── Strong("加粗")
├── Text(" 和 ")
├── InlineCode("代码")
├── Text(" 以及 ")
└── Link
    ├── text: 链接
    └── href: https://example.com
```

没有 Inline AST，就无法做到：

```text
隐藏 **
隐藏 `
隐藏 []()
点击链接编辑 href
Ctrl+B 添加加粗
删除加粗但保留文字
```

---

# 9. 第七项：Markdown 反向序列化

Typora 式编辑器必须能把编辑后的文档重新保存为 Markdown。

也就是：

```text
Editor Document -> Markdown
```

你需要实现 serializer：

```cpp
std::string serializeMarkdown(const Document& doc);
```

比如编辑器里的节点：

```text
Heading(level=2, text="Hello")
```

序列化为：

```markdown
## Hello
```

任务节点：

```text
TaskItem(checked=true, text="完成")
```

序列化为：

```markdown
- [x] 完成
```

加粗节点：

```text
Strong("bold")
```

序列化为：

```markdown
**bold**
```

这比 HTML 渲染更重要。

因为编辑器最终保存的是 `.md`，不是 `.html`。

---

# 10. 第八项：保留 Markdown 原始风格

这是很多 Markdown 编辑器很难做好的地方。

例如用户原来写的是：

```markdown
* item
* item
```

你序列化后变成：

```markdown
- item
- item
```

语义没错，但用户可能不喜欢。

再比如用户写的是：

```markdown
__bold__
```

你保存后变成：

```markdown
**bold**
```

也可能让用户觉得格式被破坏。

所以你需要尽量保留原始写法：

````text
列表 marker 是 - / * / +
加粗 marker 是 ** / __
换行风格是 LF / CRLF
代码围栏是 ``` / ~~~
标题是 ATX / Setext
任务列表 x 是 x / X
````

可以在节点里记录：

````cpp
struct MarkdownStyle {
    std::string bulletMarker;     // "-", "*", "+"
    std::string strongMarker;     // "**", "__"
    std::string codeFenceMarker;  // "```", "~~~"
    LineEnding lineEnding;        // LF / CRLF
};
````

这样保存时不会把用户的 Markdown 全部“格式化重写”。

---

# 11. 第九项：编辑命令模型

严格来说，这部分可以放在编辑器层，不一定放在 MarkdownRenderEngine 里。

但你的内核至少要支持这些语义操作：

```text
段落转标题
标题转段落
段落转列表
列表项缩进
列表项取消缩进
任务项切换完成状态
选区加粗
选区斜体
插入链接
插入图片
插入代码块
插入引用
```

可以设计成命令接口：

```cpp
Document applyCommand(
    const Document& doc,
    const Command& command
);
```

例如：

```cpp
Command cmd;
cmd.type = CommandType::ToggleStrong;
cmd.selection = selection;

Document newDoc = engine.applyCommand(doc, cmd);
```

或者这些命令放在前端编辑器实现，MarkdownRenderEngine 只负责 parse / serialize。

我的建议是：

```text
MarkdownRenderEngine 不要直接处理键盘事件
但可以提供结构化修改 API
```

例如：

```cpp
toggleTask(NodeId taskItemId);
setHeadingLevel(NodeId nodeId, int level);
wrapSelectionWithStrong(Selection selection);
```

---

# 12. 第十项：编辑器友好的 HTML 渲染

你现在的 HTML 可能适合展示：

```html
<h1>Hello</h1>
<p>This is <strong>bold</strong></p>
```

但编辑器需要的 HTML 应该带更多信息：

```html
<h1 data-node-id="n1" data-source-start="0" data-source-end="7">
  Hello
</h1>

<p data-node-id="n2" data-source-start="9" data-source-end="30">
  This is <strong data-node-id="n3">bold</strong>
</p>
```

也就是每个 DOM 节点要能反查 Markdown 节点。

你可以提供两种渲染模式：

```cpp
enum class RenderMode {
    Preview,        // 普通预览
    EditorView,     // 编辑器视图，带 node-id/source-map
    Export          // 导出 HTML/PDF
};
```

例如：

```cpp
RenderRequest request;
request.options.mode = RenderMode::EditorView;
request.options.includeNodeId = true;
request.options.includeSourceMap = true;
```

这样 Electron 前端点击某个 DOM 节点时，可以知道：

```text
这是哪个 Markdown 节点
对应源码范围是多少
应该怎样进入编辑状态
```

---

# 13. 第十一项：特殊 Markdown 节点的交互模型

Typora 式编辑器里，不同节点不能只当 HTML 展示。

## 13.1 任务列表

Markdown：

```markdown
- [ ] 写渲染器
```

内部节点：

```text
TaskItem {
    checked: false
    content: 写渲染器
}
```

点击 checkbox 后：

```text
checked: false -> true
```

保存为：

```markdown
- [x] 写渲染器
```

所以引擎需要识别：

```text
任务列表节点
checked 状态
checkbox marker 范围
任务内容范围
```

---

## 13.2 链接

Markdown：

```markdown
[GitHub](https://github.com)
```

显示：

```text
GitHub
```

但编辑器还需要知道：

```text
链接文本范围
链接地址范围
title 范围
```

节点：

```text
Link {
    text: GitHub
    href: https://github.com
    title: optional
}
```

用户点击链接时，不能直接跳转，应该提供：

```text
打开链接
编辑链接
取消链接
复制链接
```

---

## 13.3 图片

Markdown：

```markdown
![alt](./a.png)
```

节点：

```text
Image {
    alt: alt
    src: ./a.png
}
```

编辑器需要支持：

```text
显示图片
选中图片
编辑 alt
编辑 src
拖拽图片
复制图片
删除图片节点
```

---

## 13.4 代码块

Markdown：

````markdown
```cpp
int main() {}
```
````

节点：

```text
CodeBlock {
    lang: cpp
    content: int main() {}
}
```

代码块要特殊处理：

```text
不要解析内部 Markdown
保留缩进
支持语言标识
支持代码高亮
支持 Tab 输入
```

---

## 13.5 表格

表格最复杂。

Markdown：

```markdown
| A | B |
|---|---|
| 1 | 2 |
```

节点：

```text
Table
├── HeaderRow
│   ├── Cell("A")
│   └── Cell("B")
└── BodyRow
    ├── Cell("1")
    └── Cell("2")
```

表格编辑需要：

```text
插入行
删除行
插入列
删除列
移动列
修改对齐方式
序列化为 Markdown 表格
```

建议表格不要作为第一阶段目标。

---

# 14. 第十二项：错误容忍与诊断

用户正在输入时，Markdown 经常是不完整的。

例如用户刚输入：

```markdown
**加粗
```

这时 Markdown 是未闭合的。

渲染器不能直接报错或崩溃，而应该给出容错结果：

```text
Paragraph
├── Text("**加粗")
└── Diagnostic: Unclosed strong marker
```

再比如：

```markdown
[链接](https://
```

也要能继续编辑。

所以需要：

```cpp
struct Diagnostic {
    DiagnosticLevel level;
    Range range;
    std::string message;
};
```

编辑器需要的是：

```text
尽量解析
不中断编辑
提供错误信息
保留原始文本
```

---

# 15. 第十三项：HTML 扩展处理

你之前提到希望支持 HTML 扩展。

Markdown 里可能有：

```markdown
<div class="note">
hello
</div>
```

Typora 式编辑器处理 HTML 很麻烦。

你可以把 HTML 分成三种：

```text
安全可视化 HTML
不可编辑 HTML 块
危险 HTML
```

建议第一阶段这样设计：

```text
HTML block 作为特殊 RawHtmlBlock
默认以源码块方式编辑
预览模式再渲染
```

节点：

```text
RawHtmlBlock {
    raw: "<div>hello</div>"
}
```

不要一开始就把 HTML 也做成所见即所得，否则复杂度会急剧上升。

---

# 16. 第十四项：主题系统要分离成三类

现在你可能有 GitHub Light theme。

后面要分成：

```text
Preview Theme     // 预览主题
Editor Theme      // 编辑器主题
Export Theme      // 导出主题
```

因为所见即所得编辑时，样式不完全等于最终 HTML 预览。

例如：

```text
编辑器里需要显示光标
需要显示选区
需要显示当前块高亮
需要显示不可见元素
需要显示图片选中框
需要显示表格控制按钮
```

所以建议：

```text
github-light-preview.css
github-light-editor.css
github-light-export.css
```

不要只用一套 CSS 解决所有场景。

---

# 17. MarkdownRenderEngine 应该提供的 API

你的 API 可以逐步扩展成这样。

## 17.1 渲染 API

```cpp
RenderResult render(const RenderRequest& request);
```

用于：

```text
Preview
Export
Fragment Render
```

---

## 17.2 解析 API

```cpp
ParseResult parse(const ParseRequest& request);
```

例如：

```cpp
struct ParseRequest {
    std::string markdown;
    ParseOptions options;
};

struct ParseOptions {
    bool enableSourceMap = true;
    bool enableLosslessAst = true;
    bool enableDiagnostics = true;
};
```

---

## 17.3 增量解析 API

```cpp
IncrementalParseResult update(
    const Document& oldDocument,
    const TextChange& change
);
```

用于大文件实时编辑。

---

## 17.4 序列化 API

```cpp
std::string serializeMarkdown(const Document& document);
```

用于保存 `.md` 文件。

---

## 17.5 节点查询 API

```cpp
Node* findNodeBySourceOffset(const Document& doc, size_t offset);

Node* findNodeById(const Document& doc, NodeId id);

std::vector<Node*> findNodesByRange(
    const Document& doc,
    Range range
);
```

用于：

```text
点击定位
光标定位
选区转换
局部渲染
```

---

## 17.6 Fragment 渲染 API

```cpp
RenderResult renderNode(
    const Document& doc,
    NodeId nodeId,
    RenderOptions options
);
```

用于局部更新。

---

# 18. 推荐的数据结构

你可以设计一个统一节点模型。

```cpp
enum class NodeType {
    Document,

    Paragraph,
    Heading,
    Blockquote,
    BulletList,
    OrderedList,
    ListItem,
    TaskItem,
    CodeBlock,
    HtmlBlock,
    ThematicBreak,
    Table,
    TableRow,
    TableCell,

    Text,
    Strong,
    Emphasis,
    Delete,
    InlineCode,
    Link,
    Image,
    HtmlInline,
    SoftBreak,
    HardBreak
};
```

节点结构：

```cpp
struct Node {
    NodeId id;
    NodeType type;

    Range sourceRange;
    Range contentRange;

    std::vector<Range> markerRanges;
    std::vector<Node> children;

    std::unordered_map<std::string, std::string> attrs;
};
```

例如 Heading：

```text
type: Heading
attrs:
  level: 2
```

TaskItem：

```text
type: TaskItem
attrs:
  checked: false
```

Link：

```text
type: Link
attrs:
  href: https://example.com
  title: optional
```

Image：

```text
type: Image
attrs:
  src: ./a.png
  alt: text
```

CodeBlock：

```text
type: CodeBlock
attrs:
  lang: cpp
```

---

# 19. 哪些功能属于 MarkdownRenderEngine，哪些不属于

这个边界很重要。

## 应该属于 MarkdownRenderEngine

```text
Markdown 解析
AST / CST 生成
HTML 渲染
主题渲染
局部渲染
源码位置映射
Markdown 序列化
增量解析
诊断信息
节点查询
```

## 不建议属于 MarkdownRenderEngine

```text
键盘输入处理
鼠标选择
光标绘制
中文输入法 IME
撤销/重做栈
剪贴板
拖拽文件
Electron 窗口管理
具体 DOM 操作
```

这些应该交给前端编辑器层。

最终分工应该是：

```text
MarkdownRenderEngine：理解 Markdown
Editor Core：编辑 Markdown 文档
Electron App：管理桌面软件
```

---

# 20. 最小可行版本应该怎么做

你不要一开始就做完整 Typora 内核。

建议按这个顺序完善 MarkdownRenderEngine。

---

## V1：渲染器增强版

目标：

```text
稳定 Markdown -> HTML
支持 Fragment
支持 GitHub Light Theme
支持 HTML 扩展
支持代码高亮
支持基本导出
```

这就是你现在正在做的方向。

---

## V2：结构解析版

新增：

```text
Block AST
Inline AST
sourceRange
contentRange
markerRanges
diagnostics
```

此时可以支持 Obsidian-like 块渲染。

---

## V3：Live Preview 支撑版

新增：

```text
SourceMap
节点稳定 ID
局部渲染
增量解析
findNodeByOffset
renderNode
```

此时可以支持：

```text
当前块渲染
行内语法隐藏
checkbox 点击
图片 widget
链接 widget
```

这一版就已经很有价值。

---

## V4：Typora 支撑版

新增：

```text
Markdown serializer
Lossless CST
原始风格保留
结构化编辑命令
Markdown Document Model
```

此时才具备 Typora-like 编辑器内核基础。

---

## V5：完整所见即所得协作版

可选新增：

```text
Transaction
Undo/Redo
Selection Model
Collaborative editing
CRDT / OT
Plugin system
```

这一阶段不建议你现在考虑。

---

# 21. 最推荐的实现路线

你的 MarkdownRenderEngine 最应该优先补这 6 个功能：

```text
1. Block AST
2. Inline AST
3. Source Range
4. Marker Range
5. Fragment Render
6. Markdown Serializer
```

其中优先级最高的是：

```text
AST + SourceMap + Serializer
```

没有 AST，编辑器不知道文档结构。

没有 SourceMap，视觉编辑和 Markdown 源码无法对应。

没有 Serializer，编辑后的内容无法稳定保存为 Markdown。

---

# 22. 一个清晰的目标架构

最终可以变成：

```text
MarkdownRenderEngine
├── Parser
│   ├── BlockParser
│   ├── InlineParser
│   ├── HtmlParser
│   └── DiagnosticCollector
│
├── Document Model
│   ├── Node
│   ├── Mark
│   ├── Range
│   ├── SourceMap
│   └── StableId
│
├── Renderer
│   ├── HtmlRenderer
│   ├── FragmentRenderer
│   ├── EditorViewRenderer
│   └── ExportRenderer
│
├── Serializer
│   ├── MarkdownSerializer
│   ├── StylePreserver
│   └── Formatter
│
└── Incremental Engine
    ├── BlockDiff
    ├── DirtyRange
    ├── PartialParse
    └── NodeCache
```

这个结构就可以真正支撑：

```text
Source Mode
Split Mode
Preview Mode
Obsidian Live Preview
Typora WYSIWYG Mode
```

---

# 23. 最后给你一个判断标准

你的 MarkdownRenderEngine 什么时候算是可以支撑所见即所得？

看它能不能回答这几个问题：

```text
1. 用户点击某个加粗文字，我能知道它来自 Markdown 的哪几个字符吗？
2. 用户隐藏了 **，我还能正确保存回 **bold** 吗？
3. 用户点击任务 checkbox，我能把 - [ ] 改成 - [x] 吗？
4. 用户编辑标题内容，我能保持 ## 不丢失吗？
5. 用户只改一段话，我能只重新解析这一段吗？
6. 用户原来用 * 做列表，我保存时能继续用 * 吗？
7. 用户 Markdown 写到一半不完整，我不会崩溃吗？
8. 用户打开大文件，我不会每次全量重渲染吗？
```

这些都能做到，你的 MarkdownRenderEngine 就不只是渲染器了，而是一个真正可以服务于 Typora-like 编辑器的 Markdown 文档内核。

最务实的下一步是：**先把 `render()` 旁边补一个 `parse()`，让它返回带 source range 的 Block AST 和 Inline AST。** 这是从渲染器走向所见即所得编辑器内核的第一道门。
