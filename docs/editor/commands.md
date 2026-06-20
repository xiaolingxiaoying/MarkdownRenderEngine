# Command Editing

## 1. Introduction
The EditorCore does not manipulate AST nodes or DOM nodes directly when a user types. Instead, the UI sends "intents" which are transformed into `EditCommand` objects. These commands manipulate the **single source of truth: the raw Markdown string**, and then the Core re-parses the affected sections.

## 2. Command Execution Flow

```text
UI Intent (e.g., "Delete", "InsertText")
       ↓
QtDocumentAdapter maps intent to EditCommand
       ↓
DocumentSession::applyCommand(EditCommand)
       ↓
EditCommandExecutor::execute()
   ├── Validates offset and boundaries
   ├── Computes new Markdown string
   └── Computes new Selection
       ↓
DocumentSession::applyChange(TextChange)
   ├── Identifies affected block using BlockIndex
   ├── IncrementalParser parses new block
   └── NodeIdRegistry preserves IDs
       ↓
RenderPatchGenerator generates UI updates
```

## 3. Base Commands

The minimum required commands for a functional editor are:

### `ReplaceSelection`
- **Purpose**: Handles standard character typing and pasting over selections.
- **Action**: Replaces the text between `Selection.anchor` and `Selection.focus` with the provided string. If the selection is collapsed, it inserts the text.
- **Selection Result**: Cursor is moved to the end of the inserted text.

### `DeleteBackward` (Backspace)
- **Purpose**: Deletes characters before the cursor.
- **Action**: Deletes text respecting UTF-8 multi-byte character boundaries (including emojis and surrogate pairs). If invoked at the beginning of a block, it may merge the block with the preceding one.
- **Selection Result**: Cursor rests at the deletion boundary.

### `DeleteForward` (Delete)
- **Purpose**: Deletes characters after the cursor.
- **Action**: Deletes text respecting UTF-8 boundaries. If invoked at the end of a block, it may merge the block with the succeeding one.
- **Selection Result**: Cursor remains at the deletion boundary.

### `SplitBlock` (Enter)
- **Purpose**: Breaks the current block into two.
- **Action**: Usually inserts a newline or paragraph break (`\n\n`) depending on context. In lists, it creates a new list item.
- **Selection Result**: Cursor moves into the newly created block.

### `MergeBlock`
- **Purpose**: Combines two adjacent blocks into one.
- **Action**: Removes the intervening newlines/markers. Often triggered implicitly by `DeleteBackward` at the start of a block.

## 4. Error Handling and Diagnostics
If a command cannot be executed safely (e.g., modifying a complex nested structure that isn't supported for partial edits yet), `applyCommand` will return `UpdateResult::ok = false` along with `diagnostics` detailing the failure. The frontend can then choose to fall back to a full re-parse or reject the user's input.
