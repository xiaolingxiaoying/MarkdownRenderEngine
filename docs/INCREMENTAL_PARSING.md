# Incremental Parsing Strategy

## 1. Problem
Full re-parsing of a large Markdown document on every keystroke is inefficient. We need to parse only the affected parts.

## 2. Approach
When a text change occurs, the `DocumentSession` uses the `BlockIndex` to find the affected `SourceRange`. 
We define safe re-parse boundaries for different block types. For instance:
- **Paragraph / Heading**: Can be re-parsed locally.
- **List / Table / BlockQuote**: Often require falling back to the parent block or full document parsing initially.

## 3. Workflow
1. Apply text change to the Markdown string.
2. Identify affected blocks.
3. Extract the local Markdown string.
4. Parse the extracted string into a temporary AST.
5. Replace the old nodes with the new nodes in the main AST.
6. Inherit `NodeId` from the registry.
7. Update `BlockIndex` offsets.
