# MWRender Theme Specification

## Package Layout

An external theme is a directory whose name matches its theme ID:

```text
themes/
└─ custom-light/
   ├─ theme.json
   ├─ content.css
   └─ code.css
```

`content.css` is required. `code.css` is optional.

## Manifest

```json
{
  "schemaVersion": 1,
  "id": "custom-light",
  "name": "Custom Light",
  "version": "1.0.0",
  "appearance": "light",
  "entry": {
    "content": "content.css",
    "code": "code.css"
  },
  "variables": {
    "color.background": "#ffffff",
    "color.foreground": "#24292f",
    "color.accent": "#0969da",
    "font.code": "Consolas, monospace"
  }
}
```

Required fields are `schemaVersion`, `id`, `name`, `version`,
`appearance`, and `entry.content`. `appearance` must be `light`, `dark`,
or `auto`.

CSS is composed in this order:

1. Generated CSS variables.
2. `entry.content`.
3. `entry.code`, when present.

All entry paths must be relative files contained by the theme directory.
Each file is subject to `EngineOptions::maxThemeFileBytes`.

## Variables

Supported manifest variables:

```text
color.background
color.foreground
color.muted
color.border
color.accent
color.codeBackground
font.body
font.code
layout.maxWidth
layout.padding
layout.lineHeight
```

They are emitted as `--mw-*` custom properties on `.mw-document`.

## Loading

```bash
mwrender document.md --theme-path ./themes --theme custom-light
```

```cpp
mwrender::Engine engine;
engine.addThemeRoot("./themes");
```

External roots override built-in themes according to registration origin and
order. Invalid themes produce diagnostics instead of loading partial CSS.

## Security

By default, remote `@import` and `url(http...)` resources are rejected.
CSS containing an HTML `</style` end tag is also rejected. Applications can
explicitly allow remote resources with
`RenderOptions::allowRemoteCssResources`.

## Main Selectors

All fragment output is wrapped by:

```html
<article class="mw-document markdown-body">
```

Common selectors include `.mw-heading`, `.mw-paragraph`, `.mw-blockquote`,
`.mw-list`, `.mw-list-item`, `.mw-code-block`, `.mw-table`,
`.mw-task-list`, `.mw-toc`, and `.footnotes`.
