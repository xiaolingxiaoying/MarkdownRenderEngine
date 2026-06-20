# MWRender — Development Plan

Based on analysis of `UPDATE.md` and current codebase state.

---

## 0. Current State Overview

### Architecture

```
MarkdownRenderEngine = 文档结构与编辑器内核 (this project)
MWRenderQtDemo       = Qt 桌面壳 + WebView 前端 + QWebChannel 适配层 (separate)
```

### Implemented EditorCore Modules

All headers, implementations, and test files exist. The degree of completeness varies:

| Module | Header | Impl | Tests | Completeness |
|--------|--------|------|-------|-------------|
| DocumentSession | ✅ | ✅ | ✅ | Basic: load/applyChange/findNodeById/applyCommand |
| BlockIndex | ✅ | ✅ | ✅ | Basic: rebuild/blockAtOffset/blockById/affectedRangeForChange |
| NodeIdRegistry | ✅ | ✅ | ✅ | Basic: inheritIds/allocate (3-pass matching) |
| EditorProjection | ✅ | ✅ | ✅ | Basic: projectNode/projectDocument via Engine::renderNode |
| RenderPatch | ✅ | ✅ | ✅ | Basic: RenderPatchGenerator |
| SelectionMap | ✅ | ✅ | ✅ | Rough: visualToSource/sourceToVisual (no hidden marker handling) |
| EditCommand | ✅ | ✅ | ✅ | Complete: 9 command types, all implemented |
| Serializer | ✅ | ✅ | ✅ | Good: preserveOriginalMarkers support |

### Test Status

22 tests total, all passing:
- 8 editor tests (document_session, block_index, node_id_registry, incremental_parse, editor_projection, render_patch, selection_map, edit_command)
- 7 core tests (smoke, edge, conformance, stress, security, snapshot, new_features)
- 1 serializer test
- 2 benchmarks
- 1 comprehensive test
- 3 CLI tests

---

## P0 (Completed)

### Goal: Core editing commands + DocumentSession applyCommand

### Changes

| File | Change |
|------|--------|
| `src/editor/edit_command.cpp` | Added 5 missing commands: DeleteForward, MergeBlock, ToggleEmphasis, ToggleTask, SetHeadingLevel |
| `include/mwrender/editor/document_session.hpp` | Added `applyCommand(EditCommand)` declaration; fixed circular include |
| `src/editor/document_session.cpp` | Added `applyCommand` implementation (diff-based TextChange + applyChange) |
| `include/mwrender/editor/edit_command.hpp` | Forward-declared `DocumentSession` instead of include; fixed cycle |
| `include/mwrender/editor/selection_map.hpp` | Forward-declared `DocumentSession` instead of include; fixed cycle |
| `src/editor/selection_map.cpp` | Added missing `#include <mwrender/editor/document_session.hpp>` |
| `tests/editor/edit_command_test.cpp` | 15 test cases covering all commands |
| `tests/editor/document_session_test.cpp` | Added `applyCommand` test |

### Commands Added

| Command | Behavior |
|---------|----------|
| **DeleteForward** | Deletes char after cursor (or selection); fails at end of text |
| **MergeBlock** | Finds preceding `\n\n` separator and removes it, merging paragraphs |
| **ToggleEmphasis** | Wraps selection with single `*` |
| **ToggleTask** | Toggles `[ ]` ↔ `[x]` on current line; inserts `- [ ] ` if none exists |
| **SetHeadingLevel** | Modifies heading marker level on current line; inserts `# ` if not a heading |

### Verification

All 22 tests pass.

---

## P1 (Completed)

### Goal: Fix existing infrastructure gaps + incremental parsing Phase 1

### Task 1: Engine::update fix

**Problem:**
- `engine.cpp:210-214`: Dead code iterating `sortedNew` with empty body
- `engine.cpp:170`: Uses `oldDocument.literal` to reconstruct markdown, but Document.literal is typically empty
- Method not used by DocumentSession (which maintains `markdown_` directly)

**Solution:**
- Added overload: `update(const Node&, std::string_view oldMarkdown, const TextChange&)`
- Existing overload delegates to new one
- Removed dead code; cleaned up changedNodeIds computation

**Changes:**
- `include/mwrender/engine.hpp` — new overload declaration
- `src/engine.cpp` — rewrite implementation; remove dead code

---

### Task 2: SelectionMap hidden marker handling

**Problem:** `visualToSource` / `sourceToVisual` had no fallback when `findNodeById` fails (patch recreates nodes with new IDs → cursor loss). Inconsistent base offset calculations between visualToSource and sourceToVisual.

**Solution:**
- Unified offset calculation via `visualBase()` helper (contentRange for containers, range for text/code)
- Added fallback: when nodeId not found, pass offset through instead of returning 0
- Comprehensive round-trip tests for all inline formatting types

**Changes:**
- `src/editor/selection_map.cpp` — rewrite with unified visualBase + fallback
- `tests/editor/selection_map_test.cpp` — 8 test functions covering all node types

---

### Task 3: BlockIndex::affectedRangeForChange enhancement

**Problem:** Previously only returned precise range for Paragraph/Heading/CodeBlock/MathBlock/ThematicBreak. For List/BlockQuote fell back to full document range.

**Solution:**
- ListItem → returns parent List range (findParentRange helper)
- BlockQuote → returns BlockQuote range
- Cross-block changes → merged range or common container
- Added helpers: `findParentRange()`, `commonContainerType()`

**Changes:**
- `include/mwrender/editor/block_index.hpp` — new private helpers
- `src/editor/block_index.cpp` — extended `affectedRangeForChange` + helpers
- `tests/editor/block_index_test.cpp` — 3 new test functions

---

### Task 4: DocumentSession incremental parsing Phase 1

**Problem:** `DocumentSession::applyChange` always did full reparse + `fullReparse = true`. All nodes reported as potentially changed → frontend full DOM replacement on every edit.

**Phase 1 (no true incremental parsing — still full parse, but filtered results):**
1. Integrated `BlockIndex` into `DocumentSession`
2. After full parse + ID inheritance, uses `BlockIndex::affectedRangeForChange` to determine affected range
3. Filters `changedNodeIds` / `insertedNodeIds` / `removedNodeIds` to only include nodes in the affected range
4. Sets `fullReparse = false` when change is within a single simple block

**Changes:**
- `include/mwrender/editor/document_session.hpp` — added `BlockIndex` member
- `src/editor/document_session.cpp` — filter results using BlockIndex in both `load()` and `applyChange()`
- `tests/editor/document_session_test.cpp` — updated expectations for fullReparse

---

### P1 Execution Order

```
Task 1 (Engine::update fix)     → independent (done first)
Task 2 (SelectionMap)           → independent (done second)
Task 3 (BlockIndex enhancement) → prerequisite for Task 4 (done third)
Task 4 (Incremental Phase 1)    → depends on Task 3 (done fourth)
```

### Verification

All 22 tests pass.

---

## P2 (Completed)

### Goal: EditorProjection tiering + test coverage + patch diagnostics

### Task 1: EditorProjection tiering API

**Problem:** EditorProjection only rendered via Engine::renderNode with EditorView mode. No API to classify how each node type should be edited. Frontend had no way to know if a block was editable, source-editable, or atomic.

**Solution:**
- Added `Editability` enum (`Editable`, `SourceEditable`, `Atomic`)
- Added `classifyNode()` static method mapping all 31 NodeType to editability tiers
- Added `editabilityName()` for HTML attribute serialization
- HTML Renderer outputs `data-editable` attribute in EditorView mode

**Changes:**
- `include/mwrender/editor/editor_projection.hpp` — new enum + static methods
- `src/editor/editor_projection.cpp` — classifyNode implementation
- `src/render/html_renderer.cpp` — appendSourceAttributes outputs data-editable
- `tests/editor/editor_projection_test.cpp` — 3 test functions covering classification + output

---

### Task 2: fallback diagnostics + patch optimization

**Problem:** No way to know why a full re-parse was triggered. Debugging performance required manual analysis.

**Solution:**
- Added `fallbackReason` field to `UpdateResult` struct
- Set `"affected range covers entire document"` when change can't be localized
- Set `"cross-block change requires full re-parse"` when change spans multiple blocks
- Unset (empty string) when single-block edit uses patch path

**Changes:**
- `include/mwrender/editor/document_session.hpp` — UpdateResult::fallbackReason
- `src/editor/document_session.cpp` — set reason in both fallback paths

---

### Task 3: Test coverage expansion

**New test files:**

| File | Tests |
|------|-------|
| `tests/editor/unicode_test.cpp` | Chinese text round-trip, Emoji offset handling, Chinese EditCommand, Chinese text insertion |
| `tests/editor/large_document_test.cpp` | 1MB initial parse, single paragraph edit in large doc, append to large doc |

**Enhanced test files:**

| File | New tests |
|------|-----------|
| `tests/editor/incremental_parse_test.cpp` | Cross-block edit (append creates new block), List item edit preserves IDs |
| `tests/editor/edit_command_test.cpp` | Chinese ReplaceSelection, SetHeadingLevel on content line |
| `tests/editor/document_session_test.cpp` | List edit preserves other block IDs |

**CMake targets added:**
- `mwrender.editor.unicode` — unicode_test.cpp
- `mwrender.editor.large` — large_document_test.cpp (TIMEOUT 120)

---

## P3 (Future)

### Goal: Production-ready editor infrastructure

- **True incremental parsing** (Phase 2): local re-parse for simple blocks instead of full re-parse
- **Full SelectionMap** with proper UTF-16 offset handling, IME/composition protection
- **Current block source editing** in the Live Preview
- **Undo/Redo** snapshot system
- **Large document (1MB+) optimization**: initialState only returns first N blocks, renderVisibleRange on demand
- **Patch protocol stabilization**: ensure all normal edits produce patchStateReady, never fullStateReady
- **Editor benchmark** targets: initial parse < 100ms for 10KB, edit < 5ms for single block

### Key areas:
1. **Incremental parsing Phase 2**: Actually skip full re-parse for Paragraph/Heading edits
2. **Live preview mode**: Focused block shows source, others show rendered
3. **Snapshot undo/redo**: beforeMarkdown/afterMarkdown + selection snapshots
4. **Large document**: visible range rendering, MathJax/Mermaid/Highlight scheduling
5. **Selection restore after patch**: robust cursor restoration in editor.js

---

## Reference

- `UPDATE.md` — Full analysis report from MWRenderQtDemo perspective
- `AGENTS.md` — Build system, test commands, architecture conventions
- `docs/API.md` — Public API documentation
- `docs/DevelopmentGuide.md` — Normative implementation spec
