# MWRender Compatibility

## Status

MWRender currently provides a usable Markdown-to-HTML MVP. It follows the
project subset defined by `DevelopmentGuide.md`; it does not claim complete
CommonMark conformance.

## Supported Block Syntax

| Syntax | Status |
| --- | --- |
| ATX headings | Supported |
| Paragraphs | Supported |
| Fenced code blocks | Supported |
| Block quotes | Supported for common single-level cases |
| Ordered lists | Supported for common single-line items |
| Unordered lists | Supported for common single-line items |
| Thematic breaks | Supported |
| Raw HTML blocks | Supported under `HtmlPolicy` |
| GFM tables | Supported (delimiter requires ≥ 1 dash per GFM spec) |
| Front Matter | Restricted `title`, `theme`, `css` subset |

## Supported Inline Syntax

| Syntax | Status |
| --- | --- |
| Text and escapes | Supported |
| Emphasis and strong | Supported for common delimiter cases |
| Inline code | Supported |
| Links and images | Supported |
| Raw inline HTML | Supported under `HtmlPolicy` |
| Soft/hard breaks | Supported |
| GFM strikethrough | Supported |
| GFM angle autolinks | Supported |

## Rendering and Integration

| Capability | Status |
| --- | --- |
| Fragment output | Supported |
| Full HTML5 document | Supported |
| GitHub Light theme | Embedded and supported |
| External theme packages | Supported with schema validation |
| Request CSS | Supported |
| Front Matter CSS | Opt-in |
| Remote CSS resources | Disabled by default |
| Source attributes | None, line, or full |
| Heading slug de-duplication | Supported |
| Outline | Supported |
| WordStats | Supported |
| CLI and C++ API | Supported |
| Installed CMake package | Supported |

## Security

- Raw HTML defaults to escaped output.
- `Trusted` HTML requires an explicit option.
- `Sanitized` HTML uses an injected `HtmlSanitizer`.
- Without a sanitizer, non-strict mode escapes HTML and emits `MW3001`.
- Unsafe link and image protocols are removed (`javascript:`, `vbscript:`, `data:`).
- Case-insensitive scheme normalisation prevents obfuscation attacks.
- External theme entry paths cannot escape their theme directory.
- Front Matter CSS paths cannot escape the document directory.
- Absolute document CSS paths are rejected (MW4003).
- CSS `@import` and remote `url()` resources are rejected by default (MW4002).
- Theme CSS containing remote resources falls back to the default theme (MW2005).

## Phase 8 Conformance Summary

Based on the `mwrender_conformance_tests` suite (CommonMark 0.31.2 / GFM):

| Category | Pass | Expected-Fail | Not-Supported | Unexpected-Fail |
| --- | --- | --- | --- | --- |
| ATX Headings | 8 | 0 | 0 | 0 |
| Paragraphs | 3 | 0 | 0 | 0 |
| Fenced Code Blocks | 7 | 0 | 0 | 0 |
| Blockquotes | 3 | 0 | 0 | 0 |
| Lists | 7 | 0 | 0 | 0 |
| Thematic Breaks | 5 | 0 | 0 | 0 |
| Emphasis & Strong | 6 | 0 | 0 | 0 |
| Inline Code | 4 | 0 | 0 | 0 |
| Links | 6 | 0 | 1 | 0 |
| Images | 4 | 0 | 0 | 0 |
| Soft/Hard Breaks | 2 | 1 | 0 | 0 |
| Raw HTML | 3 | 0 | 0 | 0 |
| Heading Slugs | 4 | 0 | 0 | 0 |
| Backslash Escaping | 3 | 0 | 0 | 0 |
| GFM Tables | 5 | 0 | 0 | 0 |
| GFM Task Lists | 3 | 0 | 0 | 0 |
| GFM Strikethrough | 3 | 0 | 0 | 0 |
| GFM Autolinks | 3 | 0 | 0 | 0 |
| Setext Headings | 0 | 0 | 2 | 0 |
| Indented Code Blocks | 0 | 0 | 1 | 0 |
| Link References | 0 | 0 | 1 | 0 |
| HTML Entities | 1 | 0 | 1 | 0 |

Run `mwrender_conformance_tests` to reproduce these results.

## Known Deviations (Expected-Fail)

| Feature | Description |
| --- | --- |
| Backslash hard break (`\` + newline) | v0.1 only supports trailing-two-spaces hard break; `\` before newline degrades to soft break |

## Unsupported Features (Out of v0.1 Scope)

| Feature | Spec Reference | Notes |
| --- | --- | --- |
| Setext headings (`===`, `---`) | CM §4.3 | `---` is parsed as thematic break |
| Indented code blocks (4 spaces) | CM §4.4 | Not supported; 4 spaces treated as paragraph indent |
| Link reference definitions | CM §4.7 | Planned for a future version |
| HTML entity decoding (`&amp;`, `&#42;`) | CM §2.5 | Entities treated as plain text; decoder not implemented |
| Footnotes | – | Planned for v0.3 |
| Custom containers / Callout | – | Planned for v0.3 |
| Bare URL autolinking | GFM §6.9 | Angle bracket form only |
| Math / MathJax / KaTeX | – | Out of scope |
| Mermaid diagrams | – | Out of scope |
| Syntax highlighting | – | Language class emitted; host integration required |

## Performance Baseline (v0.1, Release Build)

From `mwrender_benchmark` on the development machine (exact values vary):

| Workload | Typical Time |
| --- | --- |
| 1 MB plain text | < 1 second |
| 1 MB GFM mixed | < 1 second |
| 1000 short documents | < 1 second |
| Hard timeout (any workload) | 30 seconds max |

## Known Limitations

- Nested block quotes and nested/multi-paragraph lists are not fully
  CommonMark-compatible.
- Emphasis parsing handles common cases but not the complete CommonMark
  delimiter algorithm.
- GFM bare URL autolinking without angle brackets is not yet implemented.
- Front Matter is a deliberately restricted YAML-like subset.
- No built-in sanitizer backend is bundled; applications inject one.
- Footnotes, definition lists, math, Mermaid and custom containers are not yet
  implemented.
- There is no syntax-highlighting backend; language classes are emitted for
  host integration.

These limitations are explicit compatibility boundaries, not silent claims of
full CommonMark/GFM compliance.

## Diagnostic Code Reference

| Code | Severity | Condition |
| --- | --- | --- |
| MW0001 | Error | Input exceeds maximum byte limit |
| MW0002 | Error | Input is not valid UTF-8 (Reject policy) |
| MW0003 | Warning | Invalid UTF-8 sequences replaced (Replace policy) |
| MW1001 | Warning | Fenced code block not closed before EOF |
| MW1101 | Warning | Front Matter line format not supported |
| MW1102 | Warning | Front Matter unknown key |
| MW1103 | Warning | Front Matter missing closing delimiter |
| MW2001 | Warning | Theme not found; fell back to default |
| MW2005 | Warning/Error | Theme CSS contains remote resources |
| MW3001 | Warning/Error | Sanitized mode requested but no sanitizer configured |
| MW3002 | Warning | Unsafe link URL removed |
| MW3003 | Warning | Unsafe image URL removed |
| MW4001 | Warning | CSS file could not be read |
| MW4002 | Warning | CSS source rejected (remote resources) |
| MW4003 | Warning | Absolute document CSS path rejected |
| MW4004 | Warning | Document CSS path escapes document directory |
| MW4005 | Warning | CSS file exceeds maximum size |
| MW5001 | Error | Renderer received non-Document root node |
