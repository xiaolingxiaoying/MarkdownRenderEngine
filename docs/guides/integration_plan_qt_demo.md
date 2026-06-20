# MWRenderQtDemo 接入新版 MarkdownRenderEngine 的开发计划与规范

> 目标：把 `MWRenderQtDemo/tree/1` 从“自己实现一套 WYSIWYG 内核的 Qt Demo”，改造成“基于新版 MarkdownRenderEngine EditorCore 的可用 Markdown 编辑器 Demo”。
>
> 核心原则：**MarkdownRenderEngine 负责文档结构与编辑内核，MWRenderQtDemo 只负责 Qt UI、WebView、QWebChannel、文件读写与前端交互。**

适用仓库：

- MarkdownRenderEngine：已按 `MarkdownRenderEngine_EditorCore_Development_Plan.md` 完成 EditorCore 改造。
- MWRenderQtDemo：`https://github.com/xiaolingxiaoying/MWRenderQtDemo/tree/1`

---

## 1. 总体判断

当前 `MWRenderQtDemo` 已经有 WYSIWYG 编辑器雏形：`DocumentSession`、`BlockIndex`、`OffsetMapper`、`EditorProjection`、`RenderScheduler`、`EditorBridge`、`WysiwygEditorWidget`、`editor.html/css/js`、`QWebChannel`、`QUndoStack` 等。

这些方向是对的，但现在最大的问题是：

```text
MWRenderQtDemo 自己实现了太多本该属于 MarkdownRenderEngine 的编辑器内核逻辑。
```

既然新版 MarkdownRenderEngine 已经补上：

```text
DocumentSession
BlockIndex
EditorProjection
SelectionMap
RenderPatch
EditCommand
增量解析
Markdown 回写
```

那么 MWRenderQtDemo 下一步应该做的是：

```text
删除或弱化 Qt Demo 中重复的编辑器内核代码，
改成调用 MarkdownRenderEngine 提供的 EditorCore API。
```

最终形成：

```text
MarkdownRenderEngine = 文档结构与编辑内核
MWRenderQtDemo       = Qt 桌面壳 + WebView 前端 + QWebChannel 适配层
```

---

## 2. 总目标

### 2.1 产品目标

完成后，MWRenderQtDemo 至少应该可以做到：

1. 打开 `.md` 文件。
2. 显示 GitHub Light 风格的 Markdown 渲染界面。
3. 支持基础 Live Preview 编辑。
4. 普通段落和标题可以编辑。
5. 加粗、斜体、链接、列表可以通过源码块方式编辑。
6. 代码块、数学公式、Mermaid、表格可以 atomic 显示。
7. 支持保存 Markdown。
8. 普通输入不整页刷新。
9. 普通输入不全文解析。
10. 普通输入只更新当前 block。
11. 支持基础撤销/重做。
12. 支持中文输入法。
13. 支持大文档首屏快速显示。
14. 支持滚动时按需渲染 MathJax / Mermaid / Highlight。

### 2.2 架构目标

最终结构建议如下：

```text
MWRenderQtDemo
├── src/ui/
│   └── MainWindow.*
├── src/wysiwyg/view/
│   ├── WysiwygEditorWidget.*
│   └── LocalSchemeHandler.*
├── src/wysiwyg/bridge/
│   └── EditorBridge.*
├── src/wysiwyg/document/
│   └── QtDocumentAdapter.*
├── src/wysiwyg/edit/
│   └── QtUndoCommand.*
├── resources/wysiwyg/
│   ├── editor.html
│   ├── editor.css
│   ├── editor.js
│   ├── github-light.css
│   ├── highlight.css
│   ├── mathjax-tex-svg.js
│   ├── mermaid.min.js
│   └── highlight.min.js
└── test/wysiwyg/
    ├── qt_adapter_test.cpp
    ├── bridge_patch_test.cpp
    ├── editor_protocol_test.cpp
    ├── live_preview_test.cpp
    ├── file_save_test.cpp
    └── performance_smoke_test.cpp
```

逐步删除或弃用：

```text
src/wysiwyg/document/DocumentSession.*
src/wysiwyg/document/BlockIndex.*
src/wysiwyg/document/OffsetMapper.*
src/wysiwyg/document/EditorProjection.*
src/wysiwyg/document/BlockParser.*
src/wysiwyg/render/RenderScheduler.*
```

这些能力应该由 MarkdownRenderEngine 提供。

---

## 3. 核心设计原则

### 3.1 Markdown 是唯一真实数据

必须坚持：

```text
Markdown 源码是真实文档。
AST 是 Markdown 的结构化表示。
HTML / DOM 只是显示结果。
```

禁止把 DOM 或 HTML 当作文档源数据。

错误方向：

```text
用户编辑 DOM
  ↓
读取 innerHTML
  ↓
从 HTML 反推 Markdown
```

正确方向：

```text
用户输入
  ↓
JS 生成 EditIntent
  ↓
QWebChannel 发送给 C++
  ↓
MarkdownRenderEngine 修改 Markdown
  ↓
MarkdownRenderEngine 返回 RenderPatch
  ↓
JS 局部替换 DOM
```

### 3.2 Qt Demo 不实现编辑内核

MWRenderQtDemo 只做：

```text
窗口
菜单
快捷键
文件读写
QWebEngineView
QWebChannel
DOM patch 应用
前端输入捕获
```

MarkdownRenderEngine 做：

```text
Markdown 文本维护
AST
BlockIndex
NodeId
EditorProjection
SelectionMap
RenderPatch
EditCommand
增量解析
序列化
源码保存
```

### 3.3 普通输入必须走 patch

普通输入、删除、回车、Ctrl+B、任务列表点击，都应该走：

```text
patchStateReady
```

只有这些情况允许走：

```text
fullStateReady
```

- 文件首次加载；
- 外部文件整篇替换；
- 严重错误恢复；
- 用户主动刷新；
- 版本严重不一致且无法 patch。

### 3.4 先做 Live Preview，不直接做完整 Typora

第一目标不是完整 Typora，而是稳定的 Live Preview：

```text
当前块：可以编辑
其他块：渲染显示
复杂块：atomic 显示
```

等这个稳定后，再逐步做更接近 Typora 的隐藏标记编辑。

---

## 4. 新增核心模块：QtDocumentAdapter

### 4.1 作用

`QtDocumentAdapter` 是 MWRenderQtDemo 和 MarkdownRenderEngine 的桥梁。

它负责：

```text
QString <-> std::string
QJsonObject <-> EditCommand
QJsonObject <-> RenderPatch
QJsonObject <-> Selection
QJsonArray  <-> changedNodes / insertedNodes / removedNodeIds
```

它不应该自己解析 Markdown，也不应该自己维护 AST。

### 4.2 文件位置

新增：

```text
src/wysiwyg/document/QtDocumentAdapter.hpp
src/wysiwyg/document/QtDocumentAdapter.cpp
```

### 4.3 建议接口

```cpp
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <memory>

#include <mwrender/editor/document_session.hpp>

class QtDocumentAdapter {
public:
    QtDocumentAdapter();
    ~QtDocumentAdapter();

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

### 4.4 设计要求

`QtDocumentAdapter` 必须满足：

1. 不直接操作 DOM；
2. 不直接生成 Qt UI；
3. 不保存 HTML 为真实文档；
4. 不自己解析 Markdown；
5. 所有文档操作都转交给 `mwrender::editor::DocumentSession`；
6. 所有返回结果都转成前端可用 JSON。

---

## 5. EditorBridge 改造规范

### 5.1 当前问题

当前 `EditorBridge` 中的 `applyTransaction()` 会在编辑后调用：

```text
session_.renderAll()
```

这会导致输入一个字也可能触发大量 block 的重新渲染。

改造后，`EditorBridge` 不应该调用 `renderAll()`。

### 5.2 新职责

`EditorBridge` 只负责：

```text
接收 JS intent
检查 revision
调用 QtDocumentAdapter
发送 patchStateReady / fullStateReady
转发保存、撤销、重做请求
```

### 5.3 建议结构

```cpp
class WysiwygBridge : public QObject {
    Q_OBJECT

public:
    explicit WysiwygBridge(QObject* parent = nullptr);

    Q_INVOKABLE void loadMarkdown(const QString& markdown, const QString& filePath = {});
    Q_INVOKABLE QString markdown() const;
    Q_INVOKABLE QJsonObject getInitialState();

    Q_INVOKABLE void applyEditIntent(const QJsonObject& intent);
    Q_INVOKABLE void updateSelection(const QString& selectionJson);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void initialStateReady(const QJsonObject& state);
    void fullStateReady(const QJsonObject& state);
    void patchStateReady(const QJsonObject& patch);
    void baseUrlChanged(const QString& url);

private:
    QtDocumentAdapter adapter_;
    QUndoStack undoStack_;
    SelectionAnchor lastSelection_;
    QString lastFilePath_;
};
```

### 5.4 applyEditIntent 流程

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

### 5.5 fullStateReady 使用规范

禁止在普通输入时使用：

```cpp
emit fullStateReady(...)
```

除非：

```text
1. revision 严重不一致；
2. adapter 返回 patch 失败；
3. 文档结构无法恢复；
4. 用户主动刷新。
```

---

## 6. CMake 改造计划

### 6.1 当前问题

当前 `CMakeLists.txt` 把 Qt Demo 自己的文档内核文件全部编译进 `WysiwygCore`。

后续要逐步移除：

```cmake
src/wysiwyg/document/MarkdownBuffer.cpp
src/wysiwyg/document/BlockParser.cpp
src/wysiwyg/document/DocumentSession.cpp
src/wysiwyg/document/BlockIndex.cpp
src/wysiwyg/document/OffsetMapper.cpp
src/wysiwyg/document/EditorProjection.cpp
src/wysiwyg/render/RenderScheduler.cpp
```

### 6.2 第一阶段 CMake

为了安全迁移，第一阶段先并行接入：

```cmake
set(WYSIWYG_CORE_SOURCES
    src/wysiwyg/view/WysiwygEditorWidget.cpp
    src/wysiwyg/view/LocalSchemeHandler.cpp
    src/wysiwyg/bridge/EditorBridge.cpp
    src/wysiwyg/document/QtDocumentAdapter.cpp
    src/wysiwyg/edit/EditCommand.cpp
    resources/wysiwyg.qrc
)
```

### 6.3 链接要求

确保：

```cmake
target_link_libraries(WysiwygCore PUBLIC
    MWRender::Core
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)
```

如果 MarkdownRenderEngine 的 EditorCore 被拆成独立 target，则使用：

```cmake
target_link_libraries(WysiwygCore PUBLIC
    MWRender::Core
    MWRender::Editor
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)
```

### 6.4 本地引擎依赖

继续保持：

```cmake
option(USE_LOCAL_MWRENDER "Use local MarkdownRenderEngine directory" ON)
add_subdirectory("../MarkdownRenderEngine" "${CMAKE_BINARY_DIR}/mwrender_build")
```

这样方便两个项目同时开发。

---

## 7. 前端 editor.js 改造规范

### 7.1 当前可保留内容

当前 `editor.js` 中这些方向是正确的，可以保留：

```text
QWebChannel 初始化
applyFullState
applyPatch
beforeinput
compositionstart
compositionend
selection capture
selection restore
MathJax 队列
Mermaid 缓存
IntersectionObserver
滚动空闲节流
```

### 7.2 需要重构的地方

当前 `applyPatch()` 主要处理单个 block patch。

新版 RenderPatch 应该支持：

```json
{
  "revision": 12,
  "removedNodeIds": [],
  "changedNodes": [],
  "insertedNodes": [],
  "selection": {}
}
```

所以前端应新增：

```js
function applyPatchBatch(patch) {
    if (typeof patch.revision === 'number' && patch.revision < currentRevision) return

    removeNodes(patch.removedNodeIds || [])
    replaceChangedNodes(patch.changedNodes || [])
    insertNewNodes(patch.insertedNodes || [])

    currentRevision = Math.max(currentRevision, Number(patch.revision || 0))

    const selection = pendingSelection || patch.selection
    if (selection) restoreSelection(selection)
    pendingSelection = null
}
```

### 7.3 DOM patch 顺序

必须按这个顺序：

```text
1. 保存当前 selection
2. 删除 removedNodeIds
3. 替换 changedNodes
4. 插入 insertedNodes
5. 重新注册 IntersectionObserver
6. 恢复 selection
```

### 7.4 applyFullState 使用限制

`applyFullState()` 只用于：

```text
首次加载
强制刷新
严重错误恢复
```

普通输入时如果出现 `applyFullState()`，说明后端 patch 失败，要记录日志。

### 7.5 beforeinput 规范

`beforeinput` 中不要直接修改 DOM 后保存 DOM。

正确流程：

```text
event.preventDefault()
captureSelection()
sendIntent(...)
等待后端 patch
restoreSelection()
```

### 7.6 composition 规范

中文输入时必须：

```text
compositionstart:
    暂停 patch 替换当前输入块

compositionupdate:
    允许浏览器临时显示输入法候选文本

compositionend:
    计算 diff
    发送 replaceSelection intent
    等待 patch
```

禁止：

```text
composition 过程中强行 replaceWith 当前 block
```

否则中文输入法会断。

---

## 8. Patch JSON 协议规范

### 8.1 RenderPatch JSON

统一协议：

```json
{
  "revision": 1,
  "fullReload": false,
  "removedNodeIds": [],
  "changedNodes": [
    {
      "nodeId": "n1",
      "html": "<p data-node-id=\"n1\">Hello</p>",
      "mode": "editable",
      "sourceStart": 0,
      "sourceEnd": 5,
      "projection": {}
    }
  ],
  "insertedNodes": [
    {
      "nodeId": "n2",
      "html": "<p data-node-id=\"n2\">New</p>",
      "mode": "editable",
      "sourceStart": 7,
      "sourceEnd": 10,
      "insertAfterNodeId": "n1",
      "projection": {}
    }
  ],
  "selection": {
    "anchor": 5,
    "focus": 5,
    "anchorAffinity": "after",
    "focusAffinity": "after",
    "contextNodeId": "n1"
  },
  "diagnostics": []
}
```

### 8.2 NodePatch 字段

```text
nodeId：DOM 查找 ID
html：新 HTML
mode：editable / sourceEditable / atomic / hidden / unsupported
sourceStart：Markdown UTF-16 起始 offset
sourceEnd：Markdown UTF-16 结束 offset
insertAfterNodeId：插入位置，可选
projection：投影信息，可选
```

### 8.3 Selection 字段

```text
anchor：源码 UTF-16 offset
focus：源码 UTF-16 offset
anchorAffinity：before / after
focusAffinity：before / after
contextNodeId：优先恢复到哪个 block
```

### 8.4 版本字段

每个 patch 必须带：

```text
revision
```

前端必须忽略旧 patch：

```js
if (patch.revision < currentRevision) return
```

---

## 9. 编辑模式设计

### 9.1 第一阶段：块源码编辑模式

先做最稳的模式：

```text
未聚焦 block：显示渲染结果
聚焦 block：显示 Markdown 源码
失焦 block：重新渲染
```

优点：

```text
实现简单
不容易破坏 Markdown
能快速变得可用
兼容复杂语法
```

### 9.2 第二阶段：基础 Live Preview

支持：

```text
Paragraph：直接编辑
Heading：直接编辑
Strong：隐藏 **，但可编辑内容
Emphasis：隐藏 *
InlineCode：隐藏 `
Link：显示文本，源码保留
TaskList：checkbox 可点击
```

### 9.3 第三阶段：复杂块 atomic

保持 atomic：

```text
CodeBlock
MathBlock
Mermaid
Table
HtmlBlock
FootnoteDef
```

这些块双击后进入源码编辑。

### 9.4 支持优先级

第一批：

```text
Paragraph
Heading
EmptyParagraph
SoftBreak
HardBreak
```

第二批：

```text
Strong
Emphasis
InlineCode
Strikethrough
Link
TaskList
```

第三批：

```text
List
BlockQuote
Image
CodeBlock source edit
MathBlock source edit
```

第四批：

```text
Table editor
Mermaid editor
HTML block editor
```

---

## 10. Undo / Redo 规范

### 10.1 短期方案：快照型撤销

第一阶段可以使用快照：

```text
beforeMarkdown
afterMarkdown
beforeSelection
afterSelection
```

优点：简单、稳定、容易测试。

缺点：大文档内存占用高。

### 10.2 中期方案：命令型撤销

后续改成：

```text
EditCommand
InverseEditCommand
Selection before/after
```

### 10.3 Qt UndoCommand

建议：

```cpp
class MarkdownUndoCommand : public QUndoCommand {
public:
    MarkdownUndoCommand(QtDocumentAdapter* adapter,
                        QJsonObject forward,
                        QJsonObject backward,
                        QString label);

    void redo() override;
    void undo() override;

private:
    QtDocumentAdapter* adapter_;
    QJsonObject forward_;
    QJsonObject backward_;
};
```

### 10.4 合并输入

连续输入普通文字应该合并为一个 undo step：

```text
用户连续输入 hello
按 Ctrl+Z 一次撤销 hello
不要撤销一个字符
```

合并规则：

```text
同一 block
同一类型 insertText
间隔 < 800ms
selection 连续
```

---

## 11. 文件打开与保存

### 11.1 打开流程

```text
MainWindow::openFile
  ↓
FileCodec 读取 UTF-8 / UTF-8 BOM / CRLF
  ↓
EditorBridge::loadMarkdown
  ↓
QtDocumentAdapter::loadMarkdown
  ↓
DocumentSession::load
  ↓
initialStateReady
  ↓
editor.js applyFullState
```

### 11.2 保存流程

```text
Ctrl+S
  ↓
EditorBridge::markdown()
  ↓
QtDocumentAdapter::markdown()
  ↓
DocumentSession::markdown()
  ↓
写回文件
```

### 11.3 保存原则

保存时只保存：

```text
Markdown 源码
```

不要保存：

```text
HTML
DOM
AST JSON
编辑器投影 JSON
```

---

## 12. 性能规范

### 12.1 禁止慢路径

普通输入时禁止：

```text
engine.render(整篇文档)
engine.parse(整篇文档)
session.renderAll()
applyFullState()
editor.replaceChildren()
递归扫描整棵 AST 查 nodeId
MathJax 全文重排
Mermaid 全文重绘
Highlight 全文重跑
```

### 12.2 推荐路径

普通输入：

```text
beforeinput
  ↓
EditCommand
  ↓
DocumentSession.applyCommand
  ↓
局部解析
  ↓
RenderPatch
  ↓
applyPatchBatch
  ↓
恢复光标
```

### 12.3 目标指标

| 场景 | 目标 |
|---|---|
| 小文档首次打开 | < 100ms |
| 1MB 文档首屏显示 | < 200ms |
| 1MB 文档完整后台渲染 | 可分批完成 |
| 普通段落输入一个字 | < 16ms |
| 当前 block patch | < 8ms |
| 光标恢复 | < 4ms |
| 中文输入 composition | 不中断 |
| MathJax 渲染 | 只处理可见区域 |
| Mermaid 渲染 | 缓存 SVG |

### 12.4 性能日志

增加开关：

```text
MWRENDER_QT_PERF=1
```

打开后输出：

```text
loadMarkdown time
initialState time
applyEditIntent time
patch size
patch apply time
selection restore time
MathJax time
Mermaid time
```

关闭时不要刷屏。

---

## 13. 测试计划

### 13.1 C++ 单元测试

新增：

```text
test/wysiwyg/qt_document_adapter_test.cpp
test/wysiwyg/editor_bridge_test.cpp
test/wysiwyg/patch_protocol_test.cpp
test/wysiwyg/undo_redo_test.cpp
test/wysiwyg/file_save_test.cpp
```

测试内容：

1. load markdown；
2. initialState 有 HTML；
3. applyEditIntent 返回 patch；
4. 输入文字后 markdown 正确；
5. 删除文字后 markdown 正确；
6. Enter 拆分段落；
7. Backspace 合并段落；
8. Ctrl+B 生成 `**`；
9. ToggleTask 修改 `[ ]` / `[x]`；
10. 保存结果等于 markdown。

### 13.2 WebEngine UI 测试

新增或完善：

```text
test/wysiwyg/webengine_ui_test.cpp
```

测试：

1. 打开编辑器；
2. 输入文本；
3. 光标不丢；
4. 中文输入法模拟；
5. patch 不触发 full reload；
6. 点击 checkbox；
7. Ctrl+B；
8. 代码块不进入非法编辑状态。

### 13.3 性能 Smoke 测试

新增：

```text
test/wysiwyg/performance_smoke_test.cpp
```

场景：

```text
1000 段普通段落
100 个代码块
100 个数学块
100 个 Mermaid
1MB 混合文档
```

验收：

```text
普通输入不 full reload
普通输入 patch 数量 <= 3
普通输入耗时 < 16ms
```

---

## 14. 开发阶段安排

### Phase 1：接入新版 MarkdownRenderEngine

目标：

```text
Qt Demo 可以使用 MarkdownRenderEngine EditorCore。
```

任务：

1. 确认 `../MarkdownRenderEngine` 是最新版本；
2. 确认 CMake 能链接 EditorCore；
3. 新增 `QtDocumentAdapter`；
4. 保留旧实现，不立即删除；
5. 写最小 adapter 测试。

验收：

```text
项目可编译
能 load markdown
能获得 initialState
能显示基础 HTML
```

### Phase 2：EditorBridge 改用 QtDocumentAdapter

目标：

```text
EditorBridge 不再直接使用旧 DocumentSession。
```

任务：

1. 替换 `session_` 为 `adapter_`；
2. `loadMarkdown()` 调用 adapter；
3. `markdown()` 调用 adapter；
4. `applyEditIntent()` 调用 adapter；
5. `initialStateReady` 数据来自 adapter；
6. 暂时保留 fullStateReady 作为 fallback。

验收：

```text
打开文件正常
显示正常
输入普通文字能修改 markdown
```

### Phase 3：Patch 协议升级

目标：

```text
普通输入只走 RenderPatch，不走 fullStateReady。
```

任务：

1. 后端返回 `changedNodes / insertedNodes / removedNodeIds`；
2. editor.js 新增 `applyPatchBatch()`；
3. 旧 `applyPatch()` 兼容单节点 patch；
4. 删除普通输入中的 fullStateReady；
5. 加 patch 日志。

验收：

```text
输入一个字只替换当前 block
不触发 editor.replaceChildren
不触发 applyFullState
```

### Phase 4：当前块源码编辑

目标：

```text
先做到稳定可编辑。
```

任务：

1. 点击 block 进入 source-edit；
2. 当前 block 显示 Markdown 源码；
3. 输入发 `replaceSelection`；
4. 停止输入 200ms 后 patch 当前 block；
5. 失焦后恢复渲染视图；
6. 复杂块默认 source-edit 或 atomic。

验收：

```text
段落可编辑
标题可编辑
加粗源码可编辑
列表源码可编辑
保存正确
```

### Phase 5：基础 Live Preview

目标：

```text
普通写作体验接近 Obsidian Live Preview。
```

任务：

1. Paragraph 直接编辑；
2. Heading 直接编辑；
3. Strong / Emphasis hidden marker；
4. InlineCode hidden marker；
5. Link 文本可编辑；
6. TaskList checkbox 可点击；
7. CodeBlock / Math / Mermaid 保持 atomic。

验收：

```text
写普通文章流畅
Ctrl+B 可用
任务列表可点击
复杂块不破坏 Markdown
```

### Phase 6：撤销重做与保存

目标：

```text
编辑器具备基本可用性。
```

任务：

1. Ctrl+S 保存；
2. Ctrl+Z 撤销；
3. Ctrl+Y / Ctrl+Shift+Z 重做；
4. 连续输入合并 undo；
5. 保存前强制同步 composition；
6. 关闭文件前提示未保存。

验收：

```text
编辑、撤销、重做、保存流程完整
保存文件是合法 Markdown
```

### Phase 7：性能优化

目标：

```text
大文档可用。
```

任务：

1. 删除旧 `ensureEngineCache` 全文渲染；
2. 删除普通输入 `renderAll`；
3. 首屏只渲染可见块；
4. 滚动时按需渲染；
5. MathJax / Mermaid 可见区延迟处理；
6. SVG 缓存；
7. 性能日志开关；
8. 1MB 文档测试。

验收：

```text
1MB 文档首屏快速显示
普通输入不卡
滚动不卡
复杂块延迟渲染
```

---

## 15. 版本里程碑

### v0.1：接入版

完成：

```text
QtDocumentAdapter
EditorBridge 接入
初始显示
基础保存
```

### v0.2：可编辑版

完成：

```text
普通段落编辑
标题编辑
patch 更新
不整页刷新
```

### v0.3：Live Preview 基础版

完成：

```text
Strong
Emphasis
InlineCode
Link
TaskList
当前块编辑
```

### v0.4：可用编辑器 Demo

完成：

```text
打开
编辑
保存
撤销
重做
中文输入
复杂块 atomic
大文档初步优化
```

### v0.5：接近产品原型

完成：

```text
菜单
设置
主题
字体
最近文件
文件状态
导出入口
稳定测试
```

---

## 16. 具体文件修改清单

### 16.1 新增文件

```text
src/wysiwyg/document/QtDocumentAdapter.hpp
src/wysiwyg/document/QtDocumentAdapter.cpp
src/wysiwyg/edit/MarkdownUndoCommand.hpp
src/wysiwyg/edit/MarkdownUndoCommand.cpp
test/wysiwyg/qt_document_adapter_test.cpp
test/wysiwyg/patch_protocol_test.cpp
test/wysiwyg/live_preview_test.cpp
test/wysiwyg/performance_smoke_test.cpp
```

### 16.2 重点修改文件

```text
CMakeLists.txt
src/wysiwyg/bridge/EditorBridge.hpp
src/wysiwyg/bridge/EditorBridge.cpp
src/wysiwyg/view/WysiwygEditorWidget.cpp
resources/wysiwyg/editor.js
resources/wysiwyg/editor.css
src/ui/MainWindow.cpp
src/ui/MainWindow.hpp
```

### 16.3 逐步删除文件

等新架构稳定后删除：

```text
src/wysiwyg/document/DocumentSession.hpp
src/wysiwyg/document/DocumentSession.cpp
src/wysiwyg/document/BlockIndex.hpp
src/wysiwyg/document/BlockIndex.cpp
src/wysiwyg/document/OffsetMapper.hpp
src/wysiwyg/document/OffsetMapper.cpp
src/wysiwyg/document/EditorProjection.hpp
src/wysiwyg/document/EditorProjection.cpp
src/wysiwyg/document/BlockParser.hpp
src/wysiwyg/document/BlockParser.cpp
src/wysiwyg/render/RenderScheduler.hpp
src/wysiwyg/render/RenderScheduler.cpp
```

---

## 17. 最小可用目标

如果只想最快变成能用，按这个最小目标做：

```text
1. QtDocumentAdapter 接入新版 MarkdownRenderEngine
2. EditorBridge 使用 adapter
3. editor.js 支持 patch batch
4. 当前块源码编辑
5. Ctrl+S 保存 markdown
6. Ctrl+Z 撤销
```

做到这里，你就已经有一个可用的 Markdown Live Preview 编辑器 Demo。

---

## 18. 最终总结

MWRenderQtDemo 后续的正确定位是：

```text
不是继续写一个 Markdown 编辑器内核，
而是验证和承载 MarkdownRenderEngine 的编辑器内核。
```

最重要的改造是：

```text
旧结构：
MWRenderQtDemo 自己维护 DocumentSession / BlockIndex / Projection

新结构：
MarkdownRenderEngine 维护 DocumentSession / BlockIndex / Projection
MWRenderQtDemo 只负责 UI 和协议转换
```

最终链路：

```text
用户输入
  ↓
editor.js 捕获 beforeinput
  ↓
QWebChannel 发送 EditIntent
  ↓
EditorBridge 转发
  ↓
QtDocumentAdapter 转换类型
  ↓
MarkdownRenderEngine::DocumentSession 修改 Markdown
  ↓
返回 RenderPatch
  ↓
EditorBridge emit patchStateReady
  ↓
editor.js applyPatchBatch
  ↓
局部更新 DOM
  ↓
恢复光标
```

这条链路打通后，项目就会从“能显示但难编辑、容易慢”的 Demo，变成“真正可用的 Markdown 编辑器原型”。
