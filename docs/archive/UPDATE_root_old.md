# MWRenderQtDemo 下一阶段修改原则、目标与 MarkdownRenderEngine 问题整理

## 1. 总体判断

MWRenderQtDemo 现在已经不是单纯的 Markdown 预览 Demo，而是正在向 Markdown Live Preview / Typora-like 编辑器 Demo 发展。

但当前最大的问题是：
MWRenderQtDemo 中仍然承担了太多本应属于 MarkdownRenderEngine 的编辑器内核职责，例如 DocumentSession、BlockIndex、OffsetMapper、EditorProjection、RenderScheduler、EditCommand 等。

后续方向应该调整为：

```text
MarkdownRenderEngine = 文档结构与编辑器内核
MWRenderQtDemo       = Qt 桌面壳 + WebView 前端 + QWebChannel 适配层
```

也就是说，MWRenderQtDemo 不应该继续扩大自己的编辑器内核，而应该逐步删除、弱化、迁移这些逻辑，让 MarkdownRenderEngine 提供稳定的 EditorCore API。

---

## 2. MWRenderQtDemo 的核心定位

MWRenderQtDemo 的定位不是重新写一个 Markdown 编辑器内核，而是验证和承载 MarkdownRenderEngine 的编辑器能力。

它应该负责：

```text
窗口
菜单
快捷键
文件打开
文件保存
QWebEngineView
QWebChannel
前端事件捕获
DOM patch 应用
主题加载
资源加载
性能日志
```

它不应该负责：

```text
Markdown 解析
AST 维护
BlockIndex 维护
NodeId 分配
SourceMap
SelectionMap
EditorProjection
RenderPatch 生成
Markdown 回写
增量解析
复杂编辑命令
```

这些能力应该放在 MarkdownRenderEngine 中。

---

## 3. MWRenderQtDemo 的核心原则

### 3.1 Markdown 是唯一真实数据

必须坚持：

```text
Markdown 源码是真实文档
AST 是 Markdown 的结构化表示
HTML / DOM 只是显示结果
```

禁止把 DOM 或 HTML 当成真实文档。

错误路线：

```text
用户编辑 DOM
  ↓
读取 innerHTML
  ↓
从 HTML 反推 Markdown
```

正确路线：

```text
用户输入
  ↓
editor.js 生成 EditIntent
  ↓
QWebChannel 发送给 C++
  ↓
EditorBridge 转交 QtDocumentAdapter
  ↓
QtDocumentAdapter 转成 MarkdownRenderEngine 的 EditCommand
  ↓
DocumentSession 修改 Markdown
  ↓
返回 RenderPatch
  ↓
editor.js 局部更新 DOM
  ↓
恢复光标
```

---

### 3.2 Qt Demo 只做适配，不做内核

MWRenderQtDemo 中的 QtDocumentAdapter 应该只是协议转换层。

它负责：

```text
QString <-> std::string
QJsonObject <-> EditCommand
QJsonObject <-> RenderPatch
QJsonObject <-> Selection
QJsonArray  <-> changedNodes / insertedNodes / removedNodeIds
```

它不应该：

```text
自己解析 Markdown
自己维护 AST
自己生成完整编辑器模型
自己保存 HTML
自己反推 Markdown
```

---

### 3.3 普通输入必须走 patch

普通输入时禁止整页刷新。

这些操作应该走：

```text
patchStateReady
```

包括：

```text
输入文字
删除文字
回车
Backspace
Delete
Ctrl+B
Ctrl+I
任务列表点击
当前块源码编辑
```

只有这些情况才允许走：

```text
fullStateReady
```

包括：

```text
首次加载文件
外部文件整篇替换
用户主动刷新
严重错误恢复
revision 严重不一致且无法 patch
```

如果普通输入触发了 fullStateReady，就应该认为是性能问题或者 patch 协议不完整。

---

### 3.4 先做 Live Preview，不直接追完整 Typora

第一阶段目标不是完整 Typora，而是稳定的 Live Preview。

推荐第一阶段模式：

```text
未聚焦 block：显示渲染结果
聚焦 block：显示 Markdown 源码或简单可编辑投影
失焦 block：重新渲染
复杂 block：atomic 显示
```

复杂块先不要直接编辑，包括：

```text
CodeBlock
MathBlock
Mermaid
Table
HtmlBlock
FootnoteDef
```

这些块可以先整体显示，双击或聚焦后进入源码编辑。

---

## 4. MWRenderQtDemo 当前需要重点修改的内容

### 4.1 MainWindow：修正打开文件性能计时

当前日志：

```text
[PERF] file readAll 0 ms
[PERF] file size == 10672 bytes
[PERF] applyLoadedContent 9 ms
[PERF] openMarkdownFile total 3869 ms
```

这不能直接说明渲染器慢。

因为 `openMarkdownFile total` 很可能把 QFileDialog 选择文件的时间也算进去了。

应该改为：

```cpp
QString filePath = QFileDialog::getOpenFileName(...);
if (filePath.isEmpty()) return;

PerfScope perf("openMarkdownFile after dialog");
```

然后拆分日志：

```text
file dialog
file read
decode
adapter.loadMarkdown
adapter.initialState
initialStateReady signal
editor.js applyFullState
DOM replace
MathJax visible render
Mermaid visible render
Highlight visible render
```

目标是确认真正慢点在：

```text
QFileDialog
WebEngine 初始化
页面资源加载
Markdown 解析
HTML 生成
DOM 替换
MathJax / Mermaid / Highlight
```

---

### 4.2 QtDocumentAdapter：改成真正的适配层

QtDocumentAdapter 应该接入：

```cpp
mwrender::editor::DocumentSession
```

最终结构应该接近：

```cpp
class QtDocumentAdapter {
public:
    void loadMarkdown(const QString& markdown);

    QString markdown() const;
    int revision() const;

    QJsonObject initialState(int firstBlockCount = 80);
    QJsonObject applyEditIntent(const QJsonObject& intent);
    QJsonObject applyCommand(const QJsonObject& command);

    QJsonArray renderVisibleRange(int startBlock, int count);
    QJsonObject forceFullState();

private:
    QJsonObject renderPatchToJson(const mwrender::editor::RenderPatch& patch) const;
    mwrender::editor::EditCommand editIntentFromJson(const QJsonObject& intent) const;
    mwrender::editor::Selection selectionFromJson(const QJsonObject& obj) const;
    QJsonObject selectionToJson(const mwrender::editor::Selection& selection) const;

private:
    std::unique_ptr<mwrender::editor::DocumentSession> session_;
};
```

当前短期可以先使用临时方案：

```text
QtDocumentAdapter 内部保存 markdown_
每次编辑修改 markdown_
暂时全文 parse
但只返回当前 block patch
```

也就是说，内部可以暂时不完美增量解析，但前端不能整页刷新。

---

### 4.3 EditorBridge：只负责转发，不负责渲染

EditorBridge 应该只做：

```text
接收 JS intent
检查 baseRevision
调用 QtDocumentAdapter
发送 patchStateReady
必要时发送 fullStateReady
处理保存
处理撤销重做
保存最近 selection
```

它不应该：

```text
直接 renderAll
直接解析 Markdown
直接维护 BlockIndex
直接生成 HTML
直接修改 DOM
```

普通输入流程应该固定为：

```text
editor.js beforeinput
  ↓
bridge.applyEditIntent(intent)
  ↓
EditorBridge 检查 baseRevision
  ↓
QtDocumentAdapter.applyEditIntent(intent)
  ↓
MarkdownRenderEngine::DocumentSession.applyCommand()
  ↓
返回 RenderPatch
  ↓
emit patchStateReady(patch)
```

---

### 4.4 editor.js：保留事件捕获，强化 patch 协议

editor.js 目前方向是对的，可以继续保留：

```text
QWebChannel 初始化
beforeinput
compositionstart
compositionend
selection capture
selection restore
applyFullState
applyPatch / applyPatchBatch
IntersectionObserver
MathJax 队列
Mermaid 缓存
滚动空闲节流
```

但需要重点强化：

```text
applyPatchBatch
selection restore
composition 输入法保护
stale revision 忽略
patch apply 性能日志
```

DOM patch 顺序必须固定：

```text
1. 保存 selection
2. 删除 removedNodeIds
3. 替换 changedNodes
4. 插入 insertedNodes
5. 重新注册 IntersectionObserver
6. 恢复 selection
```

---

### 4.5 CMake：逐步移除 Demo 内部编辑器内核

MWRenderQtDemo 后续应该逐步删除或弃用：

```text
src/wysiwyg/document/DocumentSession.*
src/wysiwyg/document/BlockIndex.*
src/wysiwyg/document/OffsetMapper.*
src/wysiwyg/document/EditorProjection.*
src/wysiwyg/document/BlockParser.*
src/wysiwyg/render/RenderScheduler.*
```

保留：

```text
src/wysiwyg/view/WysiwygEditorWidget.*
src/wysiwyg/view/LocalSchemeHandler.*
src/wysiwyg/bridge/EditorBridge.*
src/wysiwyg/document/QtDocumentAdapter.*
src/wysiwyg/edit/MarkdownUndoCommand.*
resources/wysiwyg/editor.html
resources/wysiwyg/editor.css
resources/wysiwyg/editor.js
```

如果 MarkdownRenderEngine 后续拆出 EditorCore target，则链接方式应该变为：

```cmake
target_link_libraries(WysiwygCore PUBLIC
    MWRender::Core
    MWRender::Editor
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)
```

---

## 5. MWRenderQtDemo 的阶段目标

### Phase 1：修正性能日志与打开流程

目标：

```text
确认 3869ms 的真实来源
排除 QFileDialog 和 WebEngine 初始化干扰
```

任务：

```text
把 openMarkdownFile total 移到 QFileDialog 之后
拆分 adapter.loadMarkdown
拆分 initialState
拆分 applyFullState
拆分 DOM replace
拆分 MathJax / Mermaid / Highlight
```

验收：

```text
10KB 文件打开后处理耗时可解释
不再用包含文件选择时间的 total 判断性能
```

---

### Phase 2：QtDocumentAdapter 保存 Markdown 源码

目标：

```text
保存时保存 Markdown，不保存 HTML，不从 AST 反序列化覆盖原文风格
```

任务：

```text
adapter 内部保存 markdown_
loadMarkdown 写入 markdown_
applyEditIntent 修改 markdown_
markdown() 直接返回 markdown_
```

验收：

```text
Ctrl+S 保存的是原始 Markdown 修改后的结果
不会无意义改变列表符号、加粗符号、空行风格
```

---

### Phase 3：打通最小编辑命令

目标：

```text
普通段落可以真正编辑
```

必须实现：

```text
replaceSelection
deleteBackward
deleteForward
splitBlock
mergeBlock
```

验收：

```text
可以输入英文
可以输入中文
可以 Backspace
可以 Delete
可以 Enter 新段落
可以保存
```

---

### Phase 4：当前块源码编辑

目标：

```text
先做稳定 Live Preview
```

行为：

```text
点击 block 进入源码编辑
当前 block 显示 Markdown 源码
其他 block 保持渲染显示
失焦后当前 block 重新渲染
复杂块 atomic 显示，双击源码编辑
```

验收：

```text
段落可编辑
标题可编辑
列表可源码编辑
加粗/斜体/链接可源码编辑
代码块/公式/Mermaid 不被破坏
```

---

### Phase 5：普通输入走 RenderPatch

目标：

```text
输入一个字只更新当前 block
```

任务：

```text
adapter 返回 changedNodes / insertedNodes / removedNodeIds
EditorBridge emit patchStateReady
editor.js applyPatchBatch
普通输入禁止 fullStateReady
```

验收：

```text
输入一个字不 applyFullState
不 replaceChildren 整篇文档
不重新跑全文 MathJax
不重新跑全文 Mermaid
不重新跑全文 Highlight
```

---

### Phase 6：Undo / Redo 与保存

目标：

```text
变成真正可用编辑器 Demo
```

短期使用快照型 undo：

```text
beforeMarkdown
afterMarkdown
beforeSelection
afterSelection
```

验收：

```text
Ctrl+Z 可撤销
Ctrl+Y / Ctrl+Shift+Z 可重做
连续输入可合并 undo
Ctrl+S 保存正确
关闭前提示未保存
```

---

### Phase 7：大文档优化

目标：

```text
1MB 文档可用
```

任务：

```text
initialState 只返回首屏 block
renderVisibleRange 按需渲染
MathJax 只处理可见区域
Mermaid 缓存 SVG
Highlight 只处理可见代码块
滚动时空闲渲染
```

验收：

```text
1MB 文档首屏快速显示
普通输入不卡
滚动不卡
复杂块延迟渲染
```

---

## 6. MarkdownRenderEngine 当前暴露的问题

### 问题 1：还没有真正成为长期文档会话

普通渲染器模式是：

```text
输入完整 Markdown
  ↓
输出完整 HTML
```

编辑器内核需要的是：

```text
维护当前 Markdown
维护当前 AST
维护 BlockIndex
维护 nodeId 映射
维护 revision
维护 SourceMap / SelectionMap
支持 applyCommand
返回 RenderPatch
```

因此 MarkdownRenderEngine 需要 DocumentSession。

DocumentSession 应该负责：

```text
load markdown
返回 markdown
返回 AST
维护 revision
applyChange
applyCommand
findNodeById
findNodeAtOffset
生成 RenderPatch
```

---

### 问题 2：BlockIndex 能力必须下沉到引擎

用户输入一个字时，必须能快速知道当前光标在哪个 Markdown block。

需要支持：

```text
offset -> block
nodeId -> block
change -> affectedRange
```

第一版支持：

```text
Heading
Paragraph
ThematicBreak
Fenced CodeBlock
MathBlock
```

复杂结构可以先回退：

```text
List
BlockQuote
Table
HtmlBlock
Footnote
Link Reference
```

---

### 问题 3：NodeId 需要稳定，不能完全依赖内容

如果 nodeId 根据内容 hash 生成，那么用户修改一个字，nodeId 就会变化。

这样 UI 会认为：

```text
旧节点删除
新节点插入
```

会造成：

```text
光标丢失
滚动位置丢失
折叠状态丢失
DOM patch 变大
```

所以应该区分：

```text
node.id          = 编辑会话内稳定 ID
node.contentHash = 判断内容是否变化
```

需要 NodeIdRegistry 负责 ID 继承。

---

### 问题 4：RenderPatch 协议需要稳定

MarkdownRenderEngine 不应该只返回完整 HTML。

它应该返回：

```json
{
  "revision": 1,
  "fullReload": false,
  "removedNodeIds": [],
  "changedNodes": [],
  "insertedNodes": [],
  "selection": {},
  "diagnostics": []
}
```

UI 只负责：

```text
removedNodeIds -> 删除 DOM
changedNodes -> 替换 DOM
insertedNodes -> 插入 DOM
selection -> 恢复光标
```

---

### 问题 5：EditorProjection 还需要明确分级

MarkdownRenderEngine 需要输出编辑器视图，而不是普通预览 HTML。

第一阶段：

```text
Paragraph -> Editable
Heading -> Editable
Text -> Editable
SoftBreak -> Editable
HardBreak -> Editable
```

第二阶段：

```text
Strong -> SourceEditable 或 HiddenMarker
Emphasis -> SourceEditable 或 HiddenMarker
InlineCode -> SourceEditable 或 HiddenMarker
Link -> SourceEditable
Image -> SourceEditable
TaskList -> 可点击
```

第三阶段：

```text
CodeBlock -> Atomic
MathBlock -> Atomic
Mermaid -> Atomic
Table -> Atomic
HtmlBlock -> Atomic
FootnoteDef -> Atomic
```

---

### 问题 6：SelectionMap 是所见即所得的核心难点

编辑器必须解决：

```text
屏幕位置 -> Markdown offset
Markdown offset -> 屏幕位置
```

尤其是这些情况：

```text
# 标题 隐藏 # 
**bold** 隐藏 **
*em* 隐藏 *
`code` 隐藏 `
[text](url) 显示 text
- [ ] task 显示 checkbox
中文输入法 composition
emoji / surrogate pair
UTF-16 / UTF-8 offset 转换
```

如果 SelectionMap 不稳定，编辑器会出现：

```text
光标跳动
中文输入中断
删除错字符
加粗位置错误
patch 后光标丢失
```

---

### 问题 7：EditCommand 不完整

MarkdownRenderEngine 需要结构化命令，而不是让 UI 直接改 DOM。

第一批必须实现：

```text
ReplaceSelection
DeleteBackward
DeleteForward
SplitBlock
MergeBlock
ToggleStrong
ToggleEmphasis
ToggleTask
SetHeadingLevel
```

后续扩展：

```text
ToggleInlineCode
ToggleStrikethrough
InsertLink
InsertImage
ToggleList
IndentListItem
OutdentListItem
```

---

### 问题 8：增量解析还不能一开始追求完美

增量解析第一版不要覆盖所有 Markdown 语法。

推荐策略：

```text
简单块：局部解析
复杂块：回退父块解析
特别复杂结构：回退全文解析
```

但每次回退都必须记录 diagnostics，例如：

```text
fallbackFullReparse: true
reason: "table edit not supported yet"
```

这样后续才能逐步优化。

---

### 问题 9：Markdown 风格保留不够

保存时不能无意义改变用户原文风格。

例如：

```markdown
* item
```

不应该自动变成：

```markdown
- item
```

例如：

```markdown
__bold__
```

不应该自动变成：

```markdown
**bold**
```

需要 AST 记录原始 marker：

```text
bulletMarker
strongMarker
emphasisMarker
codeFenceMarker
lineEnding
blankLines
taskMarkerCase
```

Serializer 默认 preserve。

---

### 问题 10：测试体系需要补 EditorCore 测试

MarkdownRenderEngine 应该新增：

```text
tests/editor/document_session_test.cpp
tests/editor/block_index_test.cpp
tests/editor/node_id_registry_test.cpp
tests/editor/incremental_parse_test.cpp
tests/editor/editor_projection_test.cpp
tests/editor/selection_map_test.cpp
tests/editor/render_patch_test.cpp
tests/editor/edit_command_test.cpp
tests/editor/serializer_preserve_test.cpp
tests/editor/editor_benchmark.cpp
```

重点测试：

```text
普通输入只影响当前段落
标题编辑不刷新全文
删除段落返回 removedNodeIds
新增段落返回 insertedNodes
nodeId 稳定
中文 offset 正确
selection 能恢复
1MB 文档编辑不 full reparse
```

---

## 7. 两个项目的职责边界

### MWRenderQtDemo 负责

```text
MainWindow
菜单栏
快捷键
文件读写
未保存状态
QWebEngineView
QWebChannel
EditorBridge
QtDocumentAdapter
editor.html
editor.css
editor.js
DOM patch
selection capture / restore
IME 事件处理
MathJax / Mermaid / Highlight 可见区调度
```

### MarkdownRenderEngine 负责

```text
Markdown 文本维护
AST
BlockIndex
NodeIdRegistry
NodeSourceMap
EditorProjection
SelectionMap
RenderPatch
EditCommand
IncrementalParser
MarkdownSerializer
DocumentSession
```

---

## 8. 推荐提交顺序

### MWRenderQtDemo

```text
1. perf: move openMarkdownFile timer after file dialog
2. perf: split load/render/dom performance logs
3. refactor: make QtDocumentAdapter keep markdown source
4. feat: map JS edit intents to adapter commands
5. feat: implement replaceSelection/deleteBackward/deleteForward/splitBlock
6. feat: support current block source editing
7. feat: apply RenderPatch batch in editor.js
8. fix: prevent fullStateReady during normal text input
9. feat: add Ctrl+S save markdown
10. feat: add snapshot undo/redo
11. test: add text edit/backspace/enter/save tests
12. test: add Chinese input and UTF offset tests
```

### MarkdownRenderEngine

```text
1. docs: add EditorCore architecture documents
2. refactor: clarify Node id and contentHash semantics
3. feat: add DocumentSession
4. feat: add BlockIndex
5. feat: add NodeIdRegistry
6. fix: correct Engine::update changedNodeIds semantics
7. feat: add EditorProjection for Heading/Paragraph
8. feat: add RenderPatch API
9. feat: add SelectionMap
10. feat: add ReplaceSelection/Delete/Split/Merge commands
11. feat: add block-level incremental parsing for Heading/Paragraph
12. feat: add ToggleStrong/ToggleEmphasis/ToggleTask
13. feat: preserve original Markdown style in serializer
14. perf: add editor benchmark
```

---

## 9. 最小可用版本目标

如果只想最快做出可编辑 Demo，最低目标是：

```text
1. QtDocumentAdapter 保存 Markdown 源码
2. EditorBridge 使用 QtDocumentAdapter
3. editor.js 支持 beforeinput -> EditIntent
4. 支持 replaceSelection
5. 支持 deleteBackward / deleteForward
6. 支持 splitBlock
7. 支持当前块源码编辑
8. 支持 patchStateReady 更新当前 block
9. Ctrl+S 保存 Markdown
10. Ctrl+Z 撤销
```

做到这里，就可以称为：

```text
可用的 Markdown Live Preview 编辑器 Demo
```

暂时不需要做到完整 Typora。

---

## 10. 最终目标

最终完整链路应该是：

```text
用户输入
  ↓
editor.js 捕获 beforeinput / composition
  ↓
QWebChannel 发送 EditIntent
  ↓
EditorBridge 检查 revision
  ↓
QtDocumentAdapter 转换协议
  ↓
MarkdownRenderEngine::DocumentSession.applyCommand
  ↓
DocumentSession 修改 Markdown
  ↓
BlockIndex 定位受影响范围
  ↓
IncrementalParser 局部解析
  ↓
NodeIdRegistry 继承稳定 ID
  ↓
EditorProjection 生成编辑器 HTML
  ↓
RenderPatch 返回 changedNodes / insertedNodes / removedNodeIds
  ↓
EditorBridge emit patchStateReady
  ↓
editor.js applyPatchBatch
  ↓
恢复 selection
  ↓
保存时写回 Markdown
```

做到这条链路后，MWRenderQtDemo 就会从“能打开和预览 Markdown 的 Demo”变成“真正能编辑 Markdown 的 Live Preview 编辑器原型”。

而 MarkdownRenderEngine 也会从“Markdown -> HTML 渲染器”升级为“Typora-like / Obsidian-like 编辑器可用的 Markdown 文档结构引擎”。
