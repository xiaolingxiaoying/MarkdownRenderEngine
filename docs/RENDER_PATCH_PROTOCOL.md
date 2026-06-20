# Render Patch Protocol

## 1. Concept
To achieve high performance, the UI should only update the DOM nodes that actually changed. `RenderPatch` calculates the difference between the previous and current state.

## 2. Data Structure

```json
{
  "revision": 5,
  "selection": {
    "anchor": { "offset": 42, "affinity": "after", "contextNodeId": "p_15" },
    "focus": { "offset": 42, "affinity": "after", "contextNodeId": "p_15" }
  },
  "removedNodeIds": ["li_old"],
  "changedNodes": [
    { "id": "p_15", "html": "<p ...>new content</p>", "parentId": "doc_1", "insertIndex": 1 }
  ],
  "insertedNodes": [
    { "id": "p_16", "html": "<p ...>new paragraph</p>", "parentId": "doc_1", "insertIndex": 2 }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `revision` | integer | Monotonically increasing revision counter. Frontend must reject patches with stale revisions. |
| `selection` | object | New cursor/selection position to restore after DOM update. |
| `removedNodeIds` | string[] | Node IDs to remove from DOM. |
| `changedNodes` | HtmlSnippet[] | HTML replacements for existing nodes. |
| `insertedNodes` | HtmlSnippet[] | New HTML snippets to insert. |

### HtmlSnippet
| Field | Type | Description |
|---|---|---|
| `id` | string | Stable node ID |
| `html` | string | Editor-rendered HTML fragment |
| `parentId` | string | ID of the parent node |
| `insertIndex` | integer | Child index within parent |

## 3. C++ Structures

```cpp
struct RenderPatch {
    std::size_t revision;
    Selection selection;           // from edit_command.hpp
    std::vector<std::string> removedNodeIds;
    std::vector<HtmlSnippet> insertedNodes;
    std::vector<HtmlSnippet> changedNodes;
};
```

## 4. UI Integration
The frontend (e.g., Qt QWebEngine or Electron) receives a JSON representation of the `RenderPatch` and applies it using standard DOM APIs:

```
1. Save current selection
2. For each id in removedNodeIds: document.getElementById(id).remove()
3. For each snippet in changedNodes: replace existing node
4. For each snippet in insertedNodes: insert at parentId + insertIndex
5. Re-register IntersectionObserver for lazy rendering
6. Restore selection from patch.selection
```

Only these cases should trigger a full state push (`fullStateReady`):
- Initial file load
- External file replacement
- User-initiated refresh
- Catastrophic error recovery
- Revision mismatch that cannot be patched
