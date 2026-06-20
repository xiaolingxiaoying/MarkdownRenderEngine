# MarkdownRenderEngine 未完成任务

根据 `UPDATE.md` 检查，以下项目尚未完成：

---

## 1. EditorCore 架构文档

- **位置**: `docs/EditorCore.md`
- **内容**: DocumentSession、BlockIndex、NodeIdRegistry、SelectionMap、EditorProjection、RenderPatch、EditCommand 的架构说明、数据流、使用示例
- **对应**: UPDATE.md Section 8 - 第 1 项

## 2. Node id / contentHash 语义文档

- **位置**: 可与架构文档合并
- **内容**: 说明 `id`（编辑会话内稳定 ID）与 `contentHash`（判断内容变化）的区别，以及 `NodeIdRegistry::inheritIds` 的 3-pass 匹配策略
- **对应**: UPDATE.md Section 8 - 第 2 项

## 3. RenderPatch 补全 revision + selection 字段

- **文件**: `include/mwrender/editor/render_patch.hpp`
- **变更**:
  - `RenderPatch` 新增 `std::size_t revision = 0;`
  - `RenderPatch` 新增 `Selection selection;`（需要 include `edit_command.hpp` 或前向声明）
- **影响**: `RenderPatchGenerator::generatePatch` 需要从 `UpdateResult` 中填充 revision 和 selection
- **对应**: UPDATE.md Section 6 - 问题 4（RenderPatch 协议）

## 4. SelectionMap UTF-16 offset 转换

- **文件**: `src/editor/selection_map.cpp`
- **变更**: 添加 `utf8ToUtf16Offset` / `utf16ToUtf16Offset` 辅助函数，在 visualToSource 和 sourceToVisual 中处理前端 UTF-16 → 后端 UTF-8 的转换
- **优先级**: 低（当前前端使用 byte offset，仅在浏览器原生 selection API 返回 UTF-16 码元偏移时需要）
- **对应**: UPDATE.md Section 6 - 问题 6（SelectionMap）

## 5. 真正的增量解析 Phase 2

- **目标**: 跳过全文 re-parse，只重新解析受影响的块
- **前置条件**: Parser 需要支持 fragment 解析，或新增块级解析接口
- **当前状态**: Phase 1 已完成（仍做全文 parse，但通过 BlockIndex 过滤结果）
- **对应**: UPDATE.md Section 6 - 问题 8（增量解析）

## 6. 扩展 EditCommand（后续扩展）

- **文件**: `src/editor/edit_command.cpp`
- **待实现命令**:
  - `ToggleInlineCode`
  - `ToggleStrikethrough`
  - `InsertLink`
  - `InsertImage`
  - `ToggleList`
  - `IndentListItem`
  - `OutdentListItem`
- **对应**: UPDATE.md Section 6 - 问题 7（EditCommand 扩展）

---

## 测试缺口

| 测试 | 状态 | 说明 |
|------|------|------|
| Selection restore after patch | ❌ 缺失 | 编辑后 selection 能否正确恢复 |
| UTF-16 offset round-trip | ❌ 缺失 | 前端 UTF-16 → 后端 UTF-8 的映射 |
| IME/composition 场景 | ❌ 缺失 | 中文输入法组合输入时的 offset 行为 |
| Undo/Redo 快照 | ❌ 缺失 | 快照型 undo 的单元测试 |
| `RenderPatch` revision 验证 | ❌ 缺失 | patch 携带 revision 后的校验测试 |
