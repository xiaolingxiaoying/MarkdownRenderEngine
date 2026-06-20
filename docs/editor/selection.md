# Selection Mapping

## 1. Introduction
The `SelectionMap` module is responsible for the bidirectional mapping between the visual cursor positions in the editor UI (DOM) and the textual source offsets in the raw Markdown string. Accurate mapping is critical for ensuring that user inputs and edits occur at the exact correct locations in the original Markdown text without offset drift or data corruption.

## 2. Offset Semantics & Boundaries
Handling offsets across different technology stacks introduces complications due to text encodings.

### `MarkdownRenderEngine` Core (UTF-8 Byte Offsets)
The EditorCore, Parser, and AST internally use **UTF-8 byte offsets** exclusively. This applies to `SourceRange`, `contentRange`, and `markerRanges`.

### UI / WebView Frontend (UTF-16 Code Unit Offsets)
Browser DOM APIs and JavaScript (e.g., `Selection.anchorOffset`, `String.length`) represent string indices in **UTF-16 code units**.

### `QtDocumentAdapter` (Conversion Layer)
The `QtDocumentAdapter` bridges the EditorCore and the frontend. It is strictly responsible for bidirectional conversion:
- Converting UTF-16 code unit offsets from the UI into UTF-8 byte offsets before passing them to `EditCommand` and `DocumentSession`.
- Converting UTF-8 byte offsets from the `RenderPatch` back into UTF-16 code unit offsets for the UI to restore the cursor.

## 3. Data Structures

```cpp
enum class Affinity {
    Before,
    After
};

struct SourcePosition {
    std::size_t offset = 0;             // UTF-8 byte offset in the Markdown source
    Affinity affinity = Affinity::After; // Hint for ambiguous boundaries
    std::string contextNodeId;          // The nodeId of the block containing the cursor
};

struct Selection {
    SourcePosition anchor;
    SourcePosition focus;
};
```

### Affinity
When a cursor sits on a boundary between two nodes (e.g., between the end of an emphasis block and the start of a strong block), `Affinity` helps determine which context the cursor belongs to:
- `Affinity::Before`: Cursor prefers the preceding context.
- `Affinity::After`: Cursor prefers the succeeding context.

## 4. Visual vs. Source Mapping

### Visual to Source (Input processing)
When the user clicks or types:
1. UI calculates visual offset relative to a `data-node-id`.
2. UI converts to UTF-16 index.
3. Bridge converts to UTF-8 `SourcePosition`.
4. Command is executed against the Markdown using `SourcePosition.offset`.

### Source to Visual (Patch restoration)
After `EditCommand` executes, it returns a new `Selection` (in UTF-8) reflecting the updated cursor position.
1. Core packages this `Selection` into the `RenderPatch`.
2. Bridge converts `Selection` offsets back to UTF-16.
3. UI maps the `contextNodeId` and UTF-16 offset back to DOM nodes and sets the browser selection.

## 5. Hidden Markers (Experimental)
Future capabilities (e.g., Live Preview) will hide Markdown syntax markers visually (e.g., hiding `**` in `**bold**`). `SelectionMap` handles projecting visual offsets over hidden markers by skipping the source offsets over the `markerRanges` associated with the node.
