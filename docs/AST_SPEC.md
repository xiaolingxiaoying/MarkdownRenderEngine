# MWRender AST Specification

The parser produces a tree rooted at `NodeType::Document`.

## Node Model

Each `Node` contains:

- `type`: semantic node kind.
- `range`: original Markdown source range.
- `payload`: type-specific data such as heading level or link destination.
- `literal`: text or code owned by the node.
- `children`: ordered child nodes.

The public definitions are in `include/mwrender/ast.hpp`.

## Source Ranges

`SourceRange` uses a half-open byte range:

```text
[begin.offset, end.offset)
```

Positions contain a one-based line and column. Offsets refer to the original
input bytes, including a UTF-8 BOM and bytes replaced under
`InvalidUtf8Policy::Replace`.

## Block Nodes

Block nodes include `Heading`, `Paragraph`, `BlockQuote`, `List`,
`ListItem`, `CodeBlock`, `ThematicBreak`, `HtmlBlock`, `Table`,
`MathBlock`, `Toc`, and `FootnoteDef`.

## Inline Nodes

Inline nodes include `Text`, `Emphasis`, `Strong`, `Strikethrough`,
`InlineCode`, `Link`, `Image`, `HtmlInline`, `AutoLink`, `MathInline`,
`FootnoteRef`, `SoftBreak`, and `HardBreak`.

## Ownership

Nodes own their children through `std::unique_ptr<Node>`. A returned document
therefore has a single owner and can be moved but not copied.
