# MWRender Security Model

## Raw HTML

`HtmlPolicy::Disabled` is the default and escapes raw HTML.

`HtmlPolicy::Sanitized` uses `SafeHtmlSanitizer`, which:

- Allows a conservative set of document-formatting tags.
- Removes event attributes and inline `style`.
- Restricts attributes by element.
- Rejects dangerous and entity-obfuscated URL schemes.
- Removes dangerous containers such as `script`, `style`, `iframe`, `svg`,
  `math`, `object`, and `template`, including their contents.

`HtmlPolicy::Trusted` must only be used for trusted local content.

## Links and Images

Markdown link and image destinations independently reject dangerous schemes
such as `javascript:`, `vbscript:`, and `data:`.

## CSS

Remote CSS resources are disabled by default. Theme, document, and request CSS
cannot contain an HTML style end tag. Theme and document paths are confined to
their permitted root directories.

## Resource Limits

`EngineOptions` limits Markdown input size, theme file size, and parser nesting
depth. Invalid UTF-8 can be rejected or replaced.

## Diagnostics

Security filtering produces stable diagnostic codes documented in
`Compatibility.md`. Sanitizer modifications emit `MW3004`.
