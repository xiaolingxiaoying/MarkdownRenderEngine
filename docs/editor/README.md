# EditorCore Documentation

Welcome to the `EditorCore` documentation index. The `EditorCore` module is a high-performance, stateful editing layer built on top of the stateless `mwrender_core` Markdown document engine. It is designed to bridge the C++ Markdown engine with rich web-based editor frontends (e.g. using Qt WebEngine, QWebChannel, or Electron).

Below is the list of documents describing the design, protocols, and mechanisms of the `EditorCore`:

## 📚 Document Index

1. **[EditorCore Design & Architecture](design.md)**
   - Core design principles, API stability classification, data flows, and testing strategies.
   
2. **[Render Patch Protocol](protocol.md)**
   - The JSON protocol and algorithms used to patch the DOM incrementally (insert/change/remove elements) based on AST node IDs.

3. **[Intent-Based Command Editing](commands.md)**
   - Describes the 16 structured editing commands (e.g., `ReplaceSelection`, `SplitBlock`, `DeleteBackward`, `ToggleStrong`, `ToggleTask`, etc.) and how they modify the Markdown string.

4. **[Selection & Cursor Mapping](selection.md)**
   - Bidirectional mapping between visual cursor positions (UTF-16 code units) and source Markdown text offsets (UTF-8 bytes), including cursor affinity and hidden marker handling.

5. **[Block-Level Incremental Parsing](incremental.md)**
   - Fast-path parsing logic for local edits, subtree diffing, and container-based safety checks for full-parse fallback.

---

## 🛠️ Module Structure

The EditorCore header files are located in `include/mwrender/editor/` and source files in `src/editor/`:

```text
include/mwrender/editor/
├── block_index.hpp          # Flat index of block-level AST nodes
├── document_session.hpp     # Long-lived stateful editing session
├── edit_command.hpp         # Command definitions and executor
├── editor_projection.hpp    # HTML rendering for editor projection modes
├── node_id_registry.hpp     # Stable ID inheritance registry
├── render_patch.hpp         # UI patching data structures
└── selection_map.hpp        # Visual ↔ Source cursor mapping
```

---

## 🏁 How the Editor Works (The Edit Loop)

```text
User Input in WebView
         │
         ▼
[JS] editor.js sends EditIntent via QWebChannel
         │
         ▼
[C++] QtDocumentAdapter maps to EditCommand
         │
         ▼
[C++] DocumentSession::applyCommand()
 ├── 1. EditCommandExecutor applies text change
 ├── 2. IncrementalParser parses only changed block
 ├── 3. NodeIdRegistry inherits stable IDs
 └── 4. Compute changed/inserted/removed node IDs
         │
         ▼
[C++] RenderPatchGenerator compiles RenderPatch (JSON)
         │
         ▼
[JS] editor.js applies DOM patch batch
         │
         ▼
WebView updates only changed DOM elements (Fast Path: < 5ms)
```
