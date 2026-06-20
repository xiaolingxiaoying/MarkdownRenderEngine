# EditorCore Design Document

## 1. Introduction
The `EditorCore` module is responsible for bridging the gap between the static Markdown `Parser`/`Renderer` and an interactive editor interface. It provides stateful management of the Markdown document during an editing session.

## 2. Core Principles
- **Markdown is the Single Source of Truth**: The raw Markdown string is the only true data source. The AST is a structural representation of it, and HTML/DOM is just the visual representation.
- **Stateful Document Session**: Editing requires a long-lived `DocumentSession` that tracks changes, maintains `BlockIndex`, and manages AST node IDs.

## 3. API Stability
To ensure safe integration with frontends (e.g., MWRenderQtDemo), the EditorCore API is divided into Stable and Experimental components:

### Stable APIs (Ready for integration)
- `DocumentSession::load()` and `DocumentSession::applyCommand()`
- `DocumentSession::markdown()` and `DocumentSession::document()`
- `Node.id` based node lookup (`findNodeById()`)
- `EditCommand` execution (base commands like `ReplaceSelection`, `DeleteBackward`, `DeleteForward`, `SplitBlock`, `MergeBlock`)
- `RenderPatchGenerator` output format

### Experimental APIs (Subject to change)
- `EditorProjection` specific projection modes (`SourceEditable`, `Atomic`)
- `IncrementalParser` (currently falls back to full parse for complex structures)
- `SelectionMap` hidden marker mappings (in development)

## 4. Key Concepts & Semantics

### Node Identity: `Node.id` vs `Node.contentHash`
- **`Node.id`**: A stable string identifier bound to the DOM `data-node-id`. It survives edits (e.g., modifying a paragraph's text keeps the same `Node.id`). Used by `RenderPatch` to update the UI.
- **`Node.contentHash`**: A hash based strictly on the node's type and content. Used internally to determine if a node has actually changed or if it can be skipped during patch generation.

### Ranges: `SourceRange`, `contentRange`, `markerRanges`
- **`SourceRange`**: The absolute full span of the node in the original Markdown string, including all syntax markers (e.g., `**bold**` -> the entire string).
- **`contentRange`**: The span of the actual textual content, excluding the syntax markers (e.g., `**bold**` -> only `bold`).
- **`markerRanges`**: The specific spans of the syntax markers themselves (e.g., the two sets of `**`).

### Render Modes
- **`RenderMode::Preview`**: Generates standard HTML output for reading. No editor-specific metadata.
- **`RenderMode::EditorView`**: Generates HTML injected with `data-node-id`, `data-source-start`, `data-projection-mode` and other metadata needed by the editor frontend to map DOM back to the AST.
- **`RenderMode::Export`**: Generates clean HTML optimized for exporting to external formats (e.g., PDF, static websites), often fully self-contained.

## 5. Architecture

```text
┌─────────────────────────────────────────────────────────────┐
│                    DocumentSession                          │
│  ┌─────────────┐  ┌──────────┐  ┌────────────────┐        │
│  │  BlockIndex │  │NodeIdReg.│  │  SelectionMap  │        │
│  └─────────────┘  └──────────┘  └────────────────┘        │
│  ┌──────────────────┐  ┌──────────────────────────┐       │
│  │ EditorProjection │  │  RenderPatchGenerator    │       │
│  └──────────────────┘  └──────────────────────────┘       │
│  ┌──────────────────────────────────────────────────┐      │
│  │            Engine (parse / render)               │      │
│  └──────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### DocumentSession
- **Header**: `include/mwrender/editor/document_session.hpp`
- **Methods**:
  - `load(std::string markdown)` — Load markdown, parse, assign initial IDs
  - `markdown() const` — Return current markdown string
  - `document() const` — Return current AST root
  - `revision() const` — Return current revision counter
  - `applyChange(const TextChange&)` — Apply raw text change, re-parse, return UpdateResult
  - `applyCommand(const EditCommand&)` — Apply structured edit command, re-parse, return UpdateResult
  - `findNodeById(string_view)` — Find AST node by stable node ID
  - `findNodeAtOffset(size_t)` — Find deepest node containing a source offset

### BlockIndex
- **Header**: `include/mwrender/editor/block_index.hpp`
- **Responsibilities**: Maintain flat list of markdown blocks with depth, support offset-to-block lookup

### NodeIdRegistry
- **Header**: `include/mwrender/editor/node_id_registry.hpp`
- **Purpose**: Assign stable IDs that survive edits.

### EditorProjection
- **Header**: `include/mwrender/editor/editor_projection.hpp`
- **Purpose**: Render AST nodes as HTML fragments suitable for editor display

### SelectionMap
- **Header**: `include/mwrender/editor/selection_map.hpp`
- **Purpose**: Bidirectional mapping between visual cursor positions and source markdown offsets.

### RenderPatch
- **Header**: `include/mwrender/editor/render_patch.hpp`
- **Purpose**: Describe incremental DOM changes for the frontend to apply.

### EditCommand
- **Header**: `include/mwrender/editor/edit_command.hpp`
- **Implemented commands** (16 total):
  - Text editing: ReplaceSelection, DeleteBackward, DeleteForward
  - Block editing: SplitBlock, MergeBlock
  - Inline formatting: ToggleStrong, ToggleEmphasis, ToggleInlineCode, ToggleStrikethrough
  - Structural: SetHeadingLevel, ToggleTask, ToggleList, IndentListItem, OutdentListItem
  - Insertion: InsertLink, InsertImage

## 6. Data Flow (Edit Cycle)

```text
User input (beforeinput / compositionend)
  ↓
editor.js → EditIntent (JSON)
  ↓
QWebChannel → EditorBridge
  ↓
QtDocumentAdapter → EditCommand
  ↓
DocumentSession::applyCommand()
  ├── EditCommandExecutor computes new markdown
  ├── DocumentSession::applyChange() (re-parse + ID inheritance)
  ├── BlockIndex::affectedRangeForChange() (determine affected range)
  ├── Filter changed/inserted/removed node IDs
  └── Return UpdateResult
  ↓
RenderPatchGenerator::generatePatch()
  ├── projectNode() for each changed/inserted node
  ├── Fill revision, selection from UpdateResult
  └── Return RenderPatch
  ↓
EditorBridge → patchStateReady (JSON)
  ↓
QWebChannel → editor.js applyPatchBatch()
  ├── Save selection
  ├── Remove removedNodeIds
  ├── Replace changedNodes
  ├── Insert insertedNodes
  └── Restore selection
```

## 7. Testing Strategy
The EditorCore logic is heavily tested to ensure stability during typing. Tests are located in `tests/editor/`:
- `document_session_test.cpp`: Tests loading, markdown retrieval, and basic change application.
- `node_id_registry_test.cpp`: Tests ID stability across edits (e.g., ID reuse, avoiding duplicates).
- `edit_command_test.cpp`: Exhaustively tests command behaviors (e.g., backspace, enter) and boundary conditions.
- `unicode_test.cpp`: Ensures UTF-8 character boundaries (CJK, emojis) are respected during deletions.
- `render_patch_test.cpp`: Verifies the generation of DOM patch instructions.
- `selection_map_test.cpp`: Tests bidirectional cursor mappings.
- `incremental_parse_test.cpp`: Tests block-level incremental re-parsing without full AST rebuilds.
- `editor_benchmark.cpp`: Performance tests for 1MB+ documents.

## 8. Boundary with MWRenderQtDemo

| MarkdownRenderEngine | MWRenderQtDemo |
|---|---|
| Markdown text maintenance | MainWindow, menus, shortcuts |
| AST, BlockIndex, NodeIdRegistry | File open/save, dirty state |
| EditorProjection, SelectionMap | QWebEngineView, QWebChannel |
| RenderPatch generation | editor.js DOM patch application |
| EditCommand execution | EditorBridge protocol conversion |
| Incremental parsing | QtDocumentAdapter (QString ↔ std::string) |
