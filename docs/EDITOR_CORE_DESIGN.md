# EditorCore Design Document

## 1. Introduction
The `EditorCore` module is responsible for bridging the gap between the static Markdown `Parser`/`Renderer` and an interactive editor interface. It provides stateful management of the Markdown document during an editing session.

## 2. Core Principles
- **Markdown is the Single Source of Truth**: The raw Markdown string is the only true data source. The AST is a structural representation of it, and HTML/DOM is just the visual representation.
- **Stateful Document Session**: Editing requires a long-lived `DocumentSession` that tracks changes, maintains `BlockIndex`, and manages AST node IDs.

## 3. Architecture
- **DocumentSession**: Entry point for editing operations.
- **BlockIndex**: Fast lookup of markdown blocks based on source offsets.
- **NodeIdRegistry**: Allocates and maintains stable DOM node IDs.
- **EditorProjection**: Transforms AST nodes into editable HTML segments.
- **SelectionMap**: Bidirectional mapping between visual and source offsets.
- **RenderPatch**: Calculates incremental DOM updates.
