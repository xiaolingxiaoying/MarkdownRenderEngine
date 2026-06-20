# Render Patch Protocol

## 1. Concept
To achieve high performance, the UI should only update the DOM nodes that actually changed. `RenderPatch` calculates the difference between the previous and current state.

## 2. Data Structure
A patch contains:
- `removedNodeIds`: List of node IDs to be removed from the DOM.
- `insertedNodes`: List of new HTML snippets to be inserted.
- `changedNodes`: List of HTML snippets that replace existing nodes.
- `selection`: The new cursor position to be restored after the DOM update.

## 3. UI Integration
The frontend (e.g., Qt QWebEngine or Electron) receives a JSON representation of the `RenderPatch` and applies it using standard DOM APIs (`document.getElementById`, `replaceWith`, etc.).
