# MWRender Roadmap

## Completed Core

- Markdown parser and public AST.
- Original-input source ranges.
- Stable fragment and full-document HTML.
- Theme packages and multi-entry external themes.
- CLI and installable C++ API.
- Disabled, sanitized, and trusted HTML policies.
- Outline and word statistics.
- Unit, conformance, stress, security, CLI, benchmark, and snapshot tests.
- Core examples and specifications.

## Next

1. Build a minimal Qt split-preview demo.
2. Validate source-to-preview synchronization with `data-source-*`.
3. Measure interactive rendering latency on representative documents.
4. Add incremental rendering only if profiling shows it is necessary.

## Later

- Broader CommonMark conformance.
- Reference links and indented code blocks.
- Rich editor integration.
- WYSIWYG editing only after the preview integration is stable.
