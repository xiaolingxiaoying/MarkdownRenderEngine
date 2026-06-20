### 项目现状总览

你已经完成了 **绝大部分 EditorCore 基础设施的骨架搭建**。对比 `UPDATE.md` 第 6 节列出的 10 个问题：

| 问题 | 状态 | 详述 |
| :--- | :--- | :--- |
| 1. DocumentSession | ✅ 基本完成 | `load`/`applyChange`/`findNodeById`/`findNodeAtOffset` 都已实现，但 `applyChange` 永远做全文重解析 |
| 2. BlockIndex | ✅ 基本完成 | `rebuild`/`blockAtOffset`/`blockById`/`affectedRangeForChange` 已实现，但复杂结构处理较简单 |
| 3. NodeId 稳定 | ✅ 基本完成 | `NodeIdRegistry` 已有 3-pass 匹配 (精确 hash -> 类型 -> 分配新 ID) |
| 4. RenderPatch 协议 | ✅ 基本完成 | `RenderPatchGenerator` 已实现 |
| 5. EditorProjection | ✅ 基本完成 | 依赖 `Engine::renderNode` + `EditorView` 模式 |
| 6. SelectionMap | ⚠️ 粗糙实现 | `visualToSource`/`sourceToVisual` 没有处理隐藏 marker (`#`, `**`, `*`, \` 等) |
| 7. EditCommand 不完整 | ⚠️ 部分完成 | 只实现了 `ReplaceSelection`/`DeleteBackward`/`ToggleStrong`/`SplitBlock`，缺少 `DeleteForward`/`MergeBlock`/`ToggleEmphasis`/`ToggleTask`/`SetHeadingLevel` |
| 8. 增量解析 | ❌ 未实现 | 永远全文重解析 |
| 9. Markdown 风格保留 | ✅ 已完成 | `Serializer` 已支持 `preserveOriginalMarkers` |
| 10. 测试体系 | ✅ 已存在 | 所有 editor 测试文件都已创建 |

### 需要做的工作（按优先级）

**P0: 完善 EditCommand (核心编辑能力)**

`EditCommandExecutor::execute` 当前只处理了 4 种命令。需要补充：

- `DeleteForward` - 删除光标后的字符
- `MergeBlock` - 合并段落（在空段落开头 Backspace）
- `ToggleEmphasis` - 切换斜体
- `ToggleTask` - 切换任务列表
- `SetHeadingLevel` - 设置标题级别

**P0: DocumentSession 支持 applyCommand**

当前 `DocumentSession` 只有 `applyChange(TextChange)`，应该增加 `applyCommand(EditCommand)`，让命令编辑走正确的编辑 + 重解析 + ID 继承路径。

**P1: 增量解析 (Incremental Parsing)**

`DocumentSession::applyChange` 当前总是做 `fallbackFullReparse`。需要实现块级增量解析：

- 简单块 (`Paragraph`/`Heading`/`ThematicBreak`) -> 局部解析
- 复杂块 (`List`/`BlockQuote`) -> 回退父块解析
- 复杂结构 (`Table`/`HtmlBlock`) -> 全文回退，但记录 `diagnostics`

**P1: Engine::update 修复**

`engine.cpp:210-214` 有一段死代码—遍历 `sortedNew` 但不做任何事（本应计算 `changedNodeIds` 但实际上 hash 比较在下面单独做了）。需要清理。

**P1: SelectionMap 完善**

当前实现没有处理隐藏标记。需要正确计算：

- `#` 标题标记
- `**`/`__` 加粗标记
- `*`/`_` 斜体标记
- \` 行内代码标记
- `[text](url)` 链接显示文本
- `- [ ]` 任务列表 checkbox
- UTF-8/UTF-16 offset 转换

**P2: EditorProjection 分级**

当前 `EditorView` 只加了 `data-node-id`。需要实现三个分级：

- **Editable** (`Paragraph`/`Heading`/`Text`/`SoftBreak`/`HardBreak`) - 可直接编辑
- **SourceEditable** (`Strong`/`Emphasis`/`InlineCode`/`Link`) - 显示隐藏标记或源码编辑
- **Atomic** (`CodeBlock`/`MathBlock`/Mermaid/`Table`) - 整体显示，双击编辑

**P2: 在普通输入中避免 fullStateReady**

当前 `DocumentSession::applyChange` 总是做全文重解析，会导致前端收到全量更新。需要配合增量解析，让简单编辑只返回 `patch`。

**P2: 编辑器测试完善**

现有测试文件 (`tests/editor/*.cpp`) 骨架已创建，但需要确认：

- 测试能否通过编译
- 测试覆盖度是否足够
- 是否覆盖了中文 offset、selection restore、1MB 文档等场景

---

### 推荐提交顺序

```text
1.  fix: clean up dead code in Engine::update changedNodeIds calculation
2.  feat: complete EditCommandExecutor (add DeleteForward, MergeBlock,
          ToggleEmphasis, ToggleTask, SetHeadingLevel)
3.  feat: add DocumentSession::applyCommand(EditCommand)
4.  perf: implement block-level incremental parsing for Heading/Paragraph
5.  feat: improve SelectionMap with hidden marker handling
6.  feat: improve EditorProjection with Editable/SourceEditable/Atomic tiers
7.  test: add comprehensive Chinese input and UTF offset tests
8.  test: add 1MB document editing benchmark test
9.  docs: add EditorCore architecture documentation
```

