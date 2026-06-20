# MarkdownRenderEngine 剩余任务实施计划

基于 `1.md`（来自 `UPDATE.md` 检查的未完成项）整理。

---

## 优先级排序

| 优先级 | 任务 | 工作量 | 依赖 |
|--------|------|--------|------|
| **P0** | RenderPatch 补全 `revision` + `selection` | 小 | 无 |
| **P1** | EditCommand 扩展（ToggleInlineCode / ToggleStrikethrough / InsertLink / InsertImage / ToggleList / IndentItem / OutdentItem） | 中 | 无 |
| **P2** | 文档补全（EditorCore 架构 + id/contentHash 语义） | 小 | 无 |
| **P3** | SelectionMap UTF-16 offset 转换 | 中 | 无 |
| **P4** | 增量解析 Phase 2（真正的局部解析） | 大 | Parser fragment 接口 |
| **—** | 测试缺口（selection restore / IME / Undo / revision） | 中 | 随任务附带 |

---

## 任务 1：RenderPatch 补全 `revision` + `selection`

**当前状态：** `RenderPatch` 没有 `revision` 和 `selection` 字段，前端无法验证 patch 是否基于最新 revision，也无法直接获取新的光标位置。

**变更：**

```cpp
// render_patch.hpp
struct RenderPatch {
    std::size_t revision = 0;        // 新增
    Selection selection;             // 新增（来自 edit_command.hpp）
    std::vector<std::string> removedNodeIds;
    // ... 其余不变
};
```

```cpp
// render_patch.cpp — generatePatch
RenderPatch RenderPatchGenerator::generatePatch(
    const Node& document,
    const UpdateResult& parseResult,
    const Selection& newSelection   // 新增参数
) const {
    RenderPatch patch;
    patch.revision = parseResult.revision;
    patch.selection = newSelection;
    // ... 其余不变
}
```

**影响链：**
- `RenderPatchGenerator::generatePatch` 签名变更 → 所有调用处需要更新
- `tests/editor/render_patch_test.cpp` 需要更新

**文件：**
- `include/mwrender/editor/render_patch.hpp` — 新增字段 + 前向声明 `Selection`
- `src/editor/render_patch.cpp` — generatePatch 填充 revision + selection
- `tests/editor/render_patch_test.cpp` — 验证 revision 和 selection 是否正确传递

---

## 任务 2：EditCommand 扩展

**当前状态：** 9 个基础命令已实现。需要补充 7 个扩展命令。

每个命令的实现策略（字符串操作，不依赖 AST）：

| 命令 | 实现策略 |
|------|----------|
| **ToggleInlineCode** | 选中文本 → 前后添加 `` ` ``；如果已被 `` ` `` 包裹则去包裹 |
| **ToggleStrikethrough** | 选中文本 → 前后添加 `~~`；如果已被 `~~` 包裹则去包裹 |
| **InsertLink** | 选中文本作为显示文字，`arg1` 为 URL，插入 `[text](url)` |
| **InsertImage** | 选中文本作为 alt，`arg1` 为 URL，插入 `![alt](url)` |
| **ToggleList** | 在当前行行首添加 `- `；如果已是列表项则去除列表标记 |
| **IndentListItem** | 在当前列表项前添加 4 个空格 |
| **OutdentListItem** | 去除当前列表项前的 4 个空格缩进 |

**文件：**
- `include/mwrender/editor/edit_command.hpp` — `EditCommandType` 新增 7 个枚举值
- `src/editor/edit_command.cpp` — `execute()` 新增 7 个 else-if 分支
- `tests/editor/edit_command_test.cpp` — 14+ 个新测试

---

## 任务 3：文档补全

**当前状态：** 以下文档文件已存在：
- `docs/EDITOR_CORE_DESIGN.md`（16 行，骨架描述）
- `docs/RENDER_PATCH_PROTOCOL.md`（14 行，协议描述）
- `docs/INCREMENTAL_PARSING.md`（19 行，增量解析策略）

| 文件 | 补充内容 |
|------|----------|
| `docs/EDITOR_CORE_DESIGN.md` | 补充各模块 API 签名示例、数据流图、与 MWRenderQtDemo 的边界说明 |
| `docs/RENDER_PATCH_PROTOCOL.md` | 补充 `revision` 和 `selection` 字段说明，对齐实际实现 |
| `docs/INCREMENTAL_PARSING.md` | 补充 Phase 1 完成状态、Phase 2 前置条件（Parser fragment 接口） |

---

## 任务 4：SelectionMap UTF-16 offset 转换

**当前状态：** `visualToSource` / `sourceToVisual` 使用 byte offset。浏览器原生 `Selection` API 返回 UTF-16 码元偏移（中文/emoji 可能导致错位）。

**变更：**

```cpp
// support/utf.hpp (新建)
namespace mwrender {
    std::size_t utf8ToUtf16Offset(std::string_view utf8, std::size_t byteOffset);
    std::size_t utf16ToUtf8Offset(std::string_view utf8, std::size_t utf16Offset);
}
```

```cpp
// selection_map.cpp — 在转换入口添加 UTF 转换（可选开关）
SourcePositionEx SelectionMap::visualToSource(const VisualPosition& visual) const {
    // 如果前端使用 UTF-16 offset:
    // source.offset = utf16ToUtf8Offset(markdown, visual.textOffset);
    // 如果前端使用 byte offset（当前假设）:
    // source.offset = base + visual.textOffset; (不变)
}
```

**优先级：低** — 当前前端使用 byte offset，暂时不需要。当前端切换到浏览器原生 `Selection` API 时启用。

**文件：**
- `include/mwrender/support/utf.hpp` (新建)
- `src/support/utf.cpp` (新建)
- `CMakeLists.txt` — 添加 `src/support/utf.cpp`
- `tests/editor/unicode_test.cpp` — 新增 UTF 转换测试

---

## 任务 5：增量解析 Phase 2

**当前状态：** `INCREMENTAL_PARSING.md` 已经描述了策略。Phase 1 完成了结果过滤 + `fullReparse` 开关。

**Phase 2 目标：** 跳过全文 `parse()`，只解析受影响的块。

**前置条件：** Parser 需要支持 fragment 解析。

```cpp
// parser.hpp — 新增方法
[[nodiscard]] ParseResult parseBlock(
    std::string_view markdown,
    NodeType expectedType,
    const ParseOptions& options = {}) const;
```

**实现方案：**
1. 从 markdown 中提取受影响块的文本
2. 用 `parseBlock` 解析该文本（需要 Parser 支持从指定行开始解析）
3. 用新 AST 替换旧 Document 中的对应节点
4. 用 `NodeIdRegistry::inheritIds` 继承 ID
5. 更新 `BlockIndex`

**复杂度：大** — `parseBlock` 需要修改 Parser，使其支持从指定 block 类型和内容开始解析。当前 Parser 总是 `scanLines` 从头开始解析。

**文件：**
- `include/mwrender/parser.hpp` — 新增 `parseBlock` 声明
- `src/parser/parser.cpp` — 实现 block-level 解析入口
- `src/editor/document_session.cpp` — `applyChange` 中 Phase 2 分支
- `tests/editor/incremental_parse_test.cpp` — 验证只解析受影响的块

---

## 测试缺口

| 测试 | 挂钩任务 | 实现方式 |
|------|----------|----------|
| Selection restore after patch | 任务 1 | `render_patch_test.cpp` 中验证 `RenderPatch.selection` 正确传递 |
| UTF-16 offset round-trip | 任务 4 | `unicode_test.cpp` 中验证 utf8↔utf16 转换 |
| IME/composition 场景 | 任务 4 | `unicode_test.cpp` 中测试组合字符 |
| Undo/Redo 快照 | 独立 | 新增 `tests/editor/undo_test.cpp`，测试快照保存和恢复 |
| `RenderPatch` revision 验证 | 任务 1 | `render_patch_test.cpp` 中验证 `patch.revision == session.revision()` |

---

## 推荐执行顺序

```
任务 1 (RenderPatch)   → 独立，影响前端协议，先做
任务 2 (EditCommand)    → 独立
任务 3 (文档补全)       → 独立，可与任务 2 并行
任务 4 (UTF-16)         → 独立，优先级低
任务 5 (增量解析 Phase 2) → 需要充分设计 Parser 接口，最后做
```
