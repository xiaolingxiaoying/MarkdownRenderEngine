# Incremental Parsing Strategy

## 1. Problem
Full re-parsing of a large Markdown document on every keystroke is inefficient. We need to parse only the affected parts.

## 2. Approach
When a text change occurs, the `DocumentSession` uses the `BlockIndex` to find the affected `SourceRange`. 
We define safe re-parse boundaries for different block types. For instance:
- **Paragraph / Heading**: Can be re-parsed locally.
- **List / Table / BlockQuote**: Often require falling back to the parent block or full document parsing initially.

## 3. Implementation Status

### Phase 1 (Completed)
- **Strategy**: Still do full `Engine::parse()`, but use `BlockIndex::affectedRangeForChange()` to filter `UpdateResult`
- **Behavior**:
  - Single simple block edit (Paragraph, Heading, ThematicBreak): `fullReparse = false`, only that block's ID in `changedNodeIds`
  - ListItem edit: returns parent List range, all items within range reported
  - Cross-block edit: `fullReparse = true`, with `fallbackReason` diagnostic
- **Key changes**:
  - `DocumentSession` integrates `BlockIndex` — rebuilds on `load()` and `applyChange()`
  - Results filtered to only include nodes in the affected source range
  - `UpdateResult::fallbackReason` records why full re-parse was needed
  - `RenderPatch::revision` and `RenderPatch::selection` carry edit metadata

### Phase 2 (Not Implemented — requires Parser changes)
- **Goal**: Skip full `Engine::parse()`, only parse the affected block(s)
- **Workflow**:
  1. Apply text change to the Markdown string.
  2. Identify affected blocks via `BlockIndex::affectedRangeForChange()`.
  3. Extract the local Markdown string for the affected block(s).
  4. Parse the extracted string using a new `Parser::parseBlock()` fragment interface.
  5. Replace the old nodes with the new nodes in the main AST.
  6. Inherit `NodeId` from the registry.
  7. Update `BlockIndex` offsets.
- **Prerequisite**: `Parser` needs a `parseBlock(std::string_view, NodeType)` method that can parse a markdown fragment without scanning from the start of the full document.
- **Fallback**: For complex structures (Table, HtmlBlock, nested BlockQuote), fall back to full parse with `fallbackReason` diagnostic.

## 4. Block Types and Re-parse Strategy

| Block Type | Phase 1 | Phase 2 (future) |
|---|---|---|
| Paragraph | Result filtering | Local re-parse |
| Heading | Result filtering | Local re-parse |
| ThematicBreak | Result filtering | Local re-parse |
| CodeBlock | Result filtering | Local re-parse |
| ListItem | Parent List range | Parent List re-parse |
| BlockQuote | BlockQuote range | BlockQuote re-parse |
| List | List range | List re-parse |
| Table | Full document | Full document (complex) |
| HtmlBlock | Full document | Full document (complex) |
