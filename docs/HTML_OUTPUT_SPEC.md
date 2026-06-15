# MWRender HTML Output Specification

## Output Modes

`OutputMode::Fragment` returns the article fragment. `FullDocument` wraps the
fragment in an HTML5 document and embeds the composed CSS and optional offline
resources.

Every fragment uses this root:

```html
<article class="mw-document markdown-body">
  ...
</article>
```

## Stable Elements

- Headings use `.mw-heading` and deterministic, de-duplicated IDs.
- Paragraphs use `.mw-paragraph`.
- Block quotes use `.mw-blockquote`.
- Lists use `.mw-list`; items use `.mw-list-item`.
- Code blocks use `.mw-code-block` and optional `data-lang`.
- Tables use `.mw-table-wrapper` and `.mw-table`.
- Task items include a disabled checkbox.
- Footnote references and backlinks use unique IDs.

Text and attribute values are escaped independently.

## Source Mapping

`SourceMapMode::None` emits no source attributes.

`SourceMapMode::Line` emits:

```html
data-source-line="12"
```

`SourceMapMode::Full` additionally emits:

```html
data-source-start="120"
data-source-end="146"
data-node-type="heading"
```

## Compatibility Contract

The semantic element and class structure is part of the public output
contract. Whitespace and attribute ordering are protected by snapshot tests
but may change in a documented breaking release.
