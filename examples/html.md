# HTML Policies

Raw HTML is escaped by default:

<mark>highlighted text</mark>

Use `--html-policy sanitized` to preserve safe formatting tags while removing
scripts, event attributes, dangerous URLs, and unsafe containers.
