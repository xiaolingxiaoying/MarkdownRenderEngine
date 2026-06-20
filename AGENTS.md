# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Build & Test Commands

This project uses CMake 3.20+ with MinGW Makefiles presets targeting g++. C++20 is required.

```bash
# Configure (debug, generates compile_commands.json for clangd)
cmake --preset clangd

# Configure (release)
cmake --preset release

# Build
cmake --build build/clangd
cmake --build build/release

# Run all tests
ctest --test-dir build/clangd --output-on-failure

# Run a single test by name
ctest --test-dir build/clangd -R mwrender.smoke --output-on-failure

# Test names: mwrender.smoke, mwrender.edge, mwrender.conformance, mwrender.stress,
#   mwrender.security, mwrender.snapshot, mwrender.new_features, mwrender.benchmark,
#   mwrender.cli.version, mwrender.cli.themes, mwrender.cli.config, mwrender.cli.stdin
```

No third-party test framework is used. Tests use custom `require()` assertions that call `std::exit(1)` on failure.

## CMake Options

- `MWRENDER_BUILD_CLI` (ON) — build the CLI executable
- `MWRENDER_BUILD_TESTS` (ON) — build test executables
- `MWRENDER_ENABLE_YAML` (OFF) — YAML front matter support (not yet implemented)
- `MWRENDER_EMBED_TOOL` — host executable for cross-compilation resource embedding

## Architecture

MWRender is a C++20 Markdown document engine: **Markdown → AST → HTML**. The single library target is `mwrender_core` (aliased `MWRender::Core`). The `Engine` class (PIMPL pattern in `engine.hpp`/`src/engine.cpp`) is the public facade.

### Core Pipeline

1. **Parser** (`src/parser/parser.cpp`, ~1740 lines) — hand-written recursive descent, no external deps. Block parser iterates lines; inline parser scans character-by-character. Produces a lossless CST/AST where every node carries `range`, `contentRange`, and `markerRanges` (SourceRange) plus a stable hash-based `id`.

2. **AST** (`include/mwrender/ast.hpp`) — tree of `Node` structs. `NodeType` enum (31 kinds). Type-specific data stored in `NodePayload` variant (HeadingData, ListData, CodeBlockData, LinkData, etc.) rather than node subclasses.

3. **HTML Renderer** (`src/render/html_renderer.cpp`) — switch-based visitor on `NodeType`. Two-phase: pre-pass collects heading slugs, TOC, footnotes; then renders AST to HTML with `mw-` prefixed CSS classes. Optionally assembles full HTML5 document with embedded CSS/JS.

4. **Engine** (`src/engine.cpp`) — orchestrates parsing, rendering, theme resolution, CSS composition. Also exposes `update()` (incremental re-parse with ID diffing), `serialize()` (AST→Markdown), and `applyCommand()` (structural editing ops like ToggleTask, SetHeadingLevel, WrapStrong).

### Editor Support Modules

- **Serializer** (`include/mwrender/serializer.hpp`, `src/serializer/serializer.cpp`) — round-trips AST back to Markdown with configurable style (bullet/strong/emphasis markers, fence char, line ending).
- **Query API** (`include/mwrender/query.hpp`, `src/query/query.cpp`) — `findNodeBySourceOffset()`, `findNodeById()`, `findNodesByRange()` for cursor/selection mapping.
- **SourceMap** (`include/mwrender/source_map.hpp`, `src/query/source_map.cpp`) — bidirectional mapping between source positions and visual positions (markers hidden).
- **Incremental** (`include/mwrender/incremental.hpp`) — `TextChange` + `IncrementalParseResult` for editor-driven re-parsing. Currently full re-parse with ID-based diffing.

### Configuration Layers

- `EngineOptions` — safety limits (maxInputBytes, maxNestingDepth, invalidUtf8Policy)
- `ParseOptions` → `MarkdownExtensions` — feature toggles (tables, taskLists, strikethrough, autoLinks, latexMath, mermaid, toc, footnotes, highlight)
- `RenderOptions` — output mode, render mode, source map, HTML policy, theme, etc.

### Theme System

Seven GitHub-derived themes are embedded at compile time via `tools/embed.cpp` (converts CSS/JS to C++ byte arrays). External themes are discovered from registered root directories. Theme CSS is security-scanned for `@import`, remote `url()`, and `</style>` injection.

### Security Model

Three HTML policies: `Disabled` (escape all HTML), `Sanitized` (allowlist-based `SafeHtmlSanitizer`), `Trusted` (pass-through). URLs validated by `isSafeUrl()`. Diagnostic codes are stable: `MW0001`–`MW5001`.

## Key Conventions

- **Namespace**: `mwrender` for all public types
- **Public API headers**: `include/mwrender/` (umbrella: `mwrender.hpp`)
- **Internal headers**: co-located in `src/` subdirectories (`render/`, `support/`, `analysis/`, `theme/`)
- **CSS classes**: all HTML elements use `mw-` prefix (e.g., `mw-heading`, `mw-paragraph`, `mw-code-block`)
- **Deterministic output**: same input + options must produce identical HTML (enforced by snapshot tests)
- **Thread safety**: Engine resources are snapshot-protected at render start; parser/renderer have no shared mutable state
- **No external runtime dependencies** for the core library — JSON parsing is custom (`src/support/json.cpp`), resources are embedded

## Documentation

Key docs in `docs/`: `DevelopmentGuide.md` (normative implementation spec, Chinese), `Compatibility.md` (supported syntax matrix + conformance results), `AST_SPEC.md`, `HTML_OUTPUT_SPEC.md`, `SECURITY.md`, `THEME_SPEC.md`, `API.md`, `CLI_USAGE.md`.
