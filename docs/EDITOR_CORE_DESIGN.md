# EditorCore Design Document

## 1. Introduction
The `EditorCore` module is responsible for bridging the gap between the static Markdown `Parser`/`Renderer` and an interactive editor interface. It provides stateful management of the Markdown document during an editing session.

## 2. Core Principles
- **Markdown is the Single Source of Truth**: The raw Markdown string is the only true data source. The AST is a structural representation of it, and HTML/DOM is just the visual representation.
- **Stateful Document Session**: Editing requires a long-lived `DocumentSession` that tracks changes, maintains `BlockIndex`, and manages AST node IDs.

## 3. Architecture

```
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
- **Key methods**: `rebuild()`, `blockAtOffset()`, `blockById()`, `affectedRangeForChange()`
- **Supported blocks**: Document, Heading, Paragraph, BlockQuote, List, ListItem, CodeBlock, ThematicBreak, HtmlBlock, Table, MathBlock, FootnoteDef

### NodeIdRegistry
- **Header**: `include/mwrender/editor/node_id_registry.hpp`
- **Purpose**: Assign stable IDs that survive edits (unlike content-hash-based IDs)
- **Strategy**: 3-pass matching during `inheritIds()`:
  1. Exact match by type + contentHash (unchanged nodes keep old ID)
  2. Type match (modified nodes inherit old ID by position)
  3. Allocate new ID for inserted nodes

### EditorProjection
- **Header**: `include/mwrender/editor/editor_projection.hpp`
- **Purpose**: Render AST nodes as HTML fragments suitable for editor display
- **Editability tiers** (via `classifyNode()` and `data-editable` HTML attribute):
  - `Editable`: Paragraph, Heading, Text, SoftBreak, HardBreak
  - `SourceEditable`: Strong, Emphasis, Strikethrough, InlineCode, Link, Image, AutoLink, MathInline, FootnoteRef
  - `Atomic`: CodeBlock, MathBlock, HtmlBlock, Table, FootnoteDef, FrontMatter, Toc

### SelectionMap
- **Header**: `include/mwrender/editor/selection_map.hpp`
- **Purpose**: Bidirectional mapping between visual cursor positions (nodeId + textOffset) and source markdown offsets
- **Key methods**: `visualToSource()`, `sourceToVisual()`
- **Offset convention**: Uses UTF-8 byte offsets (frontend must convert from UTF-16 if using native Selection API)

### RenderPatch
- **Header**: `include/mwrender/editor/render_patch.hpp`
- **Fields**: `revision`, `selection`, `removedNodeIds`, `insertedNodes`, `changedNodes`
- **Purpose**: Describe incremental DOM changes for the frontend to apply

### EditCommand
- **Header**: `include/mwrender/editor/edit_command.hpp`
- **Implemented commands** (16 total):
  - Text editing: ReplaceSelection, DeleteBackward, DeleteForward
  - Block editing: SplitBlock, MergeBlock
  - Inline formatting: ToggleStrong, ToggleEmphasis, ToggleInlineCode, ToggleStrikethrough
  - Structural: SetHeadingLevel, ToggleTask, ToggleList, IndentListItem, OutdentListItem
  - Insertion: InsertLink, InsertImage

## 4. Data Flow (Edit Cycle)

```
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
  ├── Re-register IntersectionObserver
  └── Restore selection
```

## 5. Boundary with MWRenderQtDemo

| MarkdownRenderEngine | MWRenderQtDemo |
|---|---|
| Markdown text maintenance | MainWindow, menus, shortcuts |
| AST, BlockIndex, NodeIdRegistry | File open/save, dirty state |
| EditorProjection, SelectionMap | QWebEngineView, QWebChannel |
| RenderPatch generation | editor.js DOM patch application |
| EditCommand execution | EditorBridge protocol conversion |
| Incremental parsing | QtDocumentAdapter (QString ↔ std::string) |
