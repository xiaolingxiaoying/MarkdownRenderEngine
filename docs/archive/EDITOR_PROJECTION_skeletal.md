# Editor Projection

## 1. Introduction
Unlike standard HTML rendering which produces static preview content, an editor needs HTML annotated with metadata to map DOM nodes back to the Markdown AST.

## 2. Goal
`EditorProjection` takes an AST node and outputs HTML elements with special attributes such as `data-node-id`, `data-source-start`, and `data-source-end`.

## 3. Projection Modes
- **Editable**: Text can be directly edited (e.g., plain paragraphs).
- **SourceEditable**: Reveals Markdown syntax when the cursor enters (e.g., `**bold**`).
- **Atomic**: Acts as a single block that isn't edited inline directly, or opens a dedicated popup/modal (e.g., Mermaid blocks, Math blocks).
