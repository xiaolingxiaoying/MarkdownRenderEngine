#include "render/html_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <fstream>
#include <sstream>

#include <mwrender/embed_highlight_css.hpp>
#include <mwrender/embed_highlight_js.hpp>
#include <mwrender/embed_mathjax_js.hpp>
#include <mwrender/embed_mermaid_js.hpp>
#include <mwrender/editor/editor_projection.hpp>
#include "support/document_text.hpp"

namespace mwrender::detail {
namespace {

std::string escapeHtmlText(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char value : text) {
        switch (value) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        default:
            escaped += value;
            break;
        }
    }
    return escaped;
}

std::string escapeHtmlAttribute(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char value : text) {
        switch (value) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped += value;
            break;
        }
    }
    return escaped;
}

std::string_view trimAscii(std::string_view value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

bool isSafeUrl(std::string_view value) {
    value = trimAscii(value);
    if (value.empty() || value.front() == '#' ||
        value.starts_with("./") || value.starts_with("../") ||
        value.starts_with('/')) {
        return true;
    }

    const auto colon = value.find(':');
    const auto pathSeparator = value.find_first_of("/?#");
    if (colon == std::string_view::npos ||
        (pathSeparator != std::string_view::npos && colon > pathSeparator)) {
        return true;
    }

    std::string scheme;
    scheme.reserve(colon);
    for (std::size_t index = 0; index < colon; ++index) {
        const auto character = static_cast<unsigned char>(value[index]);
        if (std::iscntrl(character) != 0 ||
            std::isspace(character) != 0) {
            continue;
        }
        scheme += static_cast<char>(std::tolower(character));
    }
    return scheme == "http" || scheme == "https" || scheme == "mailto";
}

std::string languageClass(std::string_view language) {
    std::string normalized;
    normalized.reserve(language.size());
    for (const auto character : language) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) != 0 ||
            character == '-' ||
            character == '_' ||
            character == '+') {
            normalized += character;
        }
    }
    return normalized;
}

std::string nodeTypeName(NodeType type) {
    switch (type) {
    case NodeType::Heading:
        return "heading";
    case NodeType::Paragraph:
        return "paragraph";
    case NodeType::BlockQuote:
        return "blockquote";
    case NodeType::List:
        return "list";
    case NodeType::ListItem:
        return "list-item";
    case NodeType::CodeBlock:
        return "code-block";
    case NodeType::ThematicBreak:
        return "thematic-break";
    case NodeType::HtmlBlock:
        return "html-block";
    default:
        return "node";
    }
}

class RendererState {
public:
    RendererState(
        const RenderOptions& renderOptions,
        const HtmlSanitizer* htmlSanitizer)
        : options(renderOptions),
          sanitizer(htmlSanitizer) {}

    void renderChildren(const Node& node, std::string& output) {
        for (const auto& child : node.children) {
            renderNode(*child, output, false);
        }
    }

    void renderNode(
        const Node& node,
        std::string& output,
        bool tightListParagraph) {
        switch (node.type) {
        case NodeType::Document:
            renderChildren(node, output);
            break;
        case NodeType::Heading:
            renderHeading(node, output);
            break;
        case NodeType::Paragraph:
            if (!tightListParagraph && node.type != NodeType::FootnoteDef) {
                output += "<p class=\"mw-paragraph\"";
                appendSourceAttributes(node, output);
                output += ">";
            }
            renderChildren(node, output);
            if (!tightListParagraph && node.type != NodeType::FootnoteDef) {
                output += "</p>\n";
            }
            break;
        case NodeType::Toc:
            if (options.extensions.toc && !tocHtml_.empty()) {
                output += "<ul class=\"mw-toc\">\n";
                output += tocHtml_;
                output += "</ul>\n";
            }
            break;
        case NodeType::FootnoteRef: {
            const auto* data = std::get_if<FootnoteData>(&node.payload);
            std::string id = data ? data->id : "";
            const auto reference = footnoteRefIds_.find(&node);
            const std::string referenceId = reference != footnoteRefIds_.end()
                ? reference->second
                : "fnref-" + id;
            output += "<sup id=\"" + escapeHtmlAttribute(referenceId) +
                "\"><a href=\"#fn-" + escapeHtmlAttribute(id) + "\">" +
                escapeHtmlText(id) + "</a></sup>";
            break;
        }
        case NodeType::FootnoteDef:
            break;
        case NodeType::BlockQuote: {
            std::string alertType;
            std::string alertTitle;
            const Node* firstTextNode = nullptr;
            std::size_t alertTagLength = 0;

            if (!node.children.empty() && node.children.front()->type == NodeType::Paragraph) {
                const auto& p = node.children.front();
                if (!p->children.empty() && p->children.front()->type == NodeType::Text) {
                    const auto& textNode = p->children.front();
                    std::string_view text = textNode->literal;
                    if (text.starts_with("[!NOTE]")) { alertType = "note"; alertTitle = "Note"; alertTagLength = 7; }
                    else if (text.starts_with("[!TIP]")) { alertType = "tip"; alertTitle = "Tip"; alertTagLength = 6; }
                    else if (text.starts_with("[!IMPORTANT]")) { alertType = "important"; alertTitle = "Important"; alertTagLength = 12; }
                    else if (text.starts_with("[!WARNING]")) { alertType = "warning"; alertTitle = "Warning"; alertTagLength = 10; }
                    else if (text.starts_with("[!CAUTION]")) { alertType = "caution"; alertTitle = "Caution"; alertTagLength = 10; }
                    
                    if (!alertType.empty()) {
                        firstTextNode = textNode.get();
                    }
                }
            }

            if (!alertType.empty()) {
                output += "<div class=\"markdown-alert markdown-alert-" + alertType + "\"";
                appendSourceAttributes(node, output);
                output += ">\n";
                output += "<p class=\"markdown-alert-title\">";
                
                if (alertType == "note") output += "<svg class=\"octicon octicon-info\" viewBox=\"0 0 16 16\" version=\"1.1\" width=\"16\" height=\"16\" aria-hidden=\"true\"><path d=\"M0 8a8 8 0 1 1 16 0A8 8 0 0 1 0 8Zm8-6.5a6.5 6.5 0 1 0 0 13 6.5 6.5 0 0 0 0-13ZM6.5 7.75A.75.75 0 0 1 7.25 7h1a.75.75 0 0 1 .75.75v2.75h.25a.75.75 0 0 1 0 1.5h-2a.75.75 0 0 1 0-1.5h.25v-2h-.25a.75.75 0 0 1-.75-.75ZM8 6a1 1 0 1 1 0-2 1 1 0 0 1 0 2Z\"></path></svg>";
                else if (alertType == "tip") output += "<svg class=\"octicon octicon-light-bulb\" viewBox=\"0 0 16 16\" version=\"1.1\" width=\"16\" height=\"16\" aria-hidden=\"true\"><path d=\"M8 1.5c-2.363 0-4 1.69-4 3.75 0 .984.424 1.629.984 2.304l.214.253c.223.264.47.556.673.848.284.411.537.896.621 1.49a.75.75 0 0 1-1.484.211c-.04-.282-.163-.547-.37-.847a8.456 8.456 0 0 0-.542-.68c-.084-.1-.173-.205-.268-.32C3.201 7.75 2.5 6.766 2.5 5.25 2.5 2.31 4.863 0 8 0s5.5 2.31 5.5 5.25c0 1.516-.701 2.5-1.328 3.259-.095.115-.184.22-.268.319-.207.245-.383.453-.541.681-.208.3-.33.565-.37.847a.751.751 0 0 1-1.485-.212c.084-.593.337-1.078.621-1.489.203-.292.45-.584.673-.848.075-.088.147-.173.213-.253.561-.675.985-1.32.985-2.304 0-2.06-1.637-3.75-4-3.75ZM5.75 12h4.5a.75.75 0 0 1 0 1.5h-4.5a.75.75 0 0 1 0-1.5ZM6 15.25a.75.75 0 0 1 .75-.75h2.5a.75.75 0 0 1 0 1.5h-2.5a.75.75 0 0 1-.75-.75Z\"></path></svg>";
                else if (alertType == "important") output += "<svg class=\"octicon octicon-report\" viewBox=\"0 0 16 16\" version=\"1.1\" width=\"16\" height=\"16\" aria-hidden=\"true\"><path d=\"M0 1.75C0 .784.784 0 1.75 0h12.5C15.216 0 16 .784 16 1.75v9.5A1.75 1.75 0 0 1 14.25 13H8.06l-2.573 2.573A1.458 1.458 0 0 1 3 14.543V13H1.75A1.75 1.75 0 0 1 0 11.25Zm1.75-.25a.25.25 0 0 0-.25.25v9.5c0 .138.112.25.25.25h2a.75.75 0 0 1 .75.75v2.19l2.72-2.72a.749.749 0 0 1 .53-.22h6.5a.25.25 0 0 0 .25-.25v-9.5a.25.25 0 0 0-.25-.25Zm7 2.25v2.5a.75.75 0 0 1-1.5 0v-2.5a.75.75 0 0 1 1.5 0ZM9 9a1 1 0 1 1-2 0 1 1 0 0 1 2 0Z\"></path></svg>";
                else if (alertType == "warning") output += "<svg class=\"octicon octicon-alert\" viewBox=\"0 0 16 16\" version=\"1.1\" width=\"16\" height=\"16\" aria-hidden=\"true\"><path d=\"M6.457 1.047c.659-1.234 2.427-1.234 3.086 0l6.082 11.378A1.75 1.75 0 0 1 14.082 15H1.918a1.75 1.75 0 0 1-1.543-2.575Zm1.763.707a.25.25 0 0 0-.44 0L1.698 13.132a.25.25 0 0 0 .22.368h12.164a.25.25 0 0 0 .22-.368Zm.53 3.996v2.5a.75.75 0 0 1-1.5 0v-2.5a.75.75 0 0 1 1.5 0ZM9 11a1 1 0 1 1-2 0 1 1 0 0 1 2 0Z\"></path></svg>";
                else if (alertType == "caution") output += "<svg class=\"octicon octicon-stop\" viewBox=\"0 0 16 16\" version=\"1.1\" width=\"16\" height=\"16\" aria-hidden=\"true\"><path d=\"M4.47.22A.749.749 0 0 1 5 0h6c.199 0 .389.079.53.22l4.25 4.25c.141.14.22.331.22.53v6a.749.749 0 0 1-.22.53l-4.25 4.25A.749.749 0 0 1 11 16H5a.749.749 0 0 1-.53-.22L.22 11.53A.749.749 0 0 1 0 11V5c0-.199.079-.389.22-.53Zm.84 1.28L1.5 5.31v5.38l3.81 3.81h5.38l3.81-3.81V5.31L10.69 1.5ZM8 4a.75.75 0 0 1 .75.75v3.5a.75.75 0 0 1-1.5 0v-3.5A.75.75 0 0 1 8 4Zm0 8a1 1 0 1 1 0-2 1 1 0 0 1 0 2Z\"></path></svg>";
                
                output += alertTitle + "</p>\n";
                
                for (std::size_t i = 0; i < node.children.size(); ++i) {
                    if (i == 0) {
                        const auto& p = node.children[0];
                        bool hasContent = false;
                        std::string pOutput = "<p class=\"mw-paragraph\"";
                        appendSourceAttributes(*p, pOutput);
                        pOutput += ">";
                        for (std::size_t j = 0; j < p->children.size(); ++j) {
                            if (p->children[j].get() == firstTextNode) {
                                std::string_view remainder = firstTextNode->literal;
                                remainder.remove_prefix(alertTagLength);
                                while (!remainder.empty() && (remainder.front() == ' ' || remainder.front() == '\t' || remainder.front() == '\n' || remainder.front() == '\r')) {
                                    remainder.remove_prefix(1);
                                }
                                if (!remainder.empty()) {
                                    pOutput += escapeHtmlText(remainder);
                                    hasContent = true;
                                }
                            } else {
                                renderNode(*p->children[j], pOutput, false);
                                hasContent = true;
                            }
                        }
                        pOutput += "</p>\n";
                        if (hasContent) {
                            output += pOutput;
                        }
                    } else {
                        renderNode(*node.children[i], output, false);
                    }
                }
                output += "</div>\n";
            } else {
                output += "<blockquote class=\"mw-blockquote\"";
                appendSourceAttributes(node, output);
                output += ">\n";
                renderChildren(node, output);
                output += "</blockquote>\n";
            }
            break;
        }
        case NodeType::List:
            renderList(node, output);
            break;
        case NodeType::ListItem:
            renderListItem(node, output);
            break;
        case NodeType::Table:
            output += "<div class=\"mw-table-wrapper\"";
            appendSourceAttributes(node, output);
            output += "><table class=\"mw-table\">\n";
            renderChildren(node, output);
            output += "</table></div>\n";
            break;
        case NodeType::TableHead:
            output += "<thead>\n";
            renderChildren(node, output);
            output += "</thead>\n";
            break;
        case NodeType::TableBody:
            output += "<tbody>\n";
            renderChildren(node, output);
            output += "</tbody>\n";
            break;
        case NodeType::TableRow:
            output += "<tr>\n";
            renderChildren(node, output);
            output += "</tr>\n";
            break;
        case NodeType::TableCell:
            renderTableCell(node, output);
            break;
        case NodeType::MathBlock:
            output += "<div class=\"math-block\"";
            appendSourceAttributes(node, output);
            output += ">$$";
            output += escapeHtmlText(node.literal);
            output += "$$</div>\n";
            break;
        case NodeType::MathInline:
            output += "<span class=\"math-inline\">\\(";
            output += escapeHtmlText(node.literal);
            output += "\\)</span>";
            break;
        case NodeType::CodeBlock:
            renderCodeBlock(node, output);
            break;
        case NodeType::ThematicBreak:
            output += "<hr class=\"mw-thematic-break\"";
            appendSourceAttributes(node, output);
            output += ">\n";
            break;
        case NodeType::HtmlBlock:
        case NodeType::HtmlInline:
            renderRawHtml(node, output);
            if (node.type == NodeType::HtmlBlock) {
                output += '\n';
            }
            break;
        case NodeType::Text:
            output += escapeHtmlText(node.literal);
            break;
        case NodeType::Emphasis:
            output += "<em>";
            renderChildren(node, output);
            output += "</em>";
            break;
        case NodeType::Strong:
            output += "<strong>";
            renderChildren(node, output);
            output += "</strong>";
            break;
        case NodeType::Strikethrough:
            output += "<del>";
            renderChildren(node, output);
            output += "</del>";
            break;
        case NodeType::InlineCode:
            output += "<code class=\"mw-inline-code\">";
            output += escapeHtmlText(node.literal);
            output += "</code>";
            break;
        case NodeType::Link:
            renderLink(node, output);
            break;
        case NodeType::Image:
            renderImage(node, output);
            break;
        case NodeType::AutoLink:
            renderAutoLink(node, output);
            break;
        case NodeType::SoftBreak:
            output += '\n';
            break;
        case NodeType::HardBreak:
            output += "<br>\n";
            break;
        case NodeType::FrontMatter:
            break;
        }
    }

    const RenderOptions& options;
    const HtmlSanitizer* sanitizer = nullptr;
    std::vector<Diagnostic> diagnostics;
    bool failed = false;

private:
    void appendSourceAttributes(const Node& node, std::string& output) const {
        if (options.sourceMap == SourceMapMode::None) {
            return;
        }
        if (options.renderMode == RenderMode::EditorView && !node.id.empty()) {
            output += " data-node-id=\"";
            output += node.id;
            output += '"';
            using mwrender::editor::EditorProjection;
            auto edit = EditorProjection::classifyNode(node);
            output += " data-editable=\"";
            output += EditorProjection::editabilityName(edit);
            output += '"';
        }
        output += " data-source-line=\"";
        output += std::to_string(node.range.begin.line);
        output += '"';
        if (options.sourceMap == SourceMapMode::Full) {
            output += " data-source-start=\"";
            output += std::to_string(node.range.begin.offset);
            output += "\" data-source-end=\"";
            output += std::to_string(node.range.end.offset);
            output += "\" data-node-type=\"";
            output += nodeTypeName(node.type);
            output += '"';
        }
    }

    void renderHeading(const Node& node, std::string& output) {
        const auto* data = std::get_if<HeadingData>(&node.payload);
        const unsigned int level = data ? data->level : 1;
        std::string slug = headingSlugs_[&node];

        output += "<h";
        output += std::to_string(level);
        output += " class=\"mw-heading\" id=\"";
        output += escapeHtmlAttribute(slug);
        output += '"';
        appendSourceAttributes(node, output);
        output += ">";
        renderChildren(node, output);
        output += "</h";
        output += std::to_string(level);
        output += ">\n";
    }

    void renderList(const Node& node, std::string& output) {
        const auto* data = std::get_if<ListData>(&node.payload);
        const bool ordered = data && data->ordered;
        const bool taskList = std::any_of(
            node.children.begin(),
            node.children.end(),
            [](const auto& child) {
                const auto* item =
                    std::get_if<ListItemData>(&child->payload);
                return item && item->task;
            });
        output += ordered ? "<ol class=\"mw-list" : "<ul class=\"mw-list";
        if (taskList) {
            output += " mw-task-list";
        }
        output += '"';
        if (ordered && data->start != 1) {
            output += " start=\"";
            output += std::to_string(data->start);
            output += '"';
        }
        appendSourceAttributes(node, output);
        output += ">\n";
        renderChildren(node, output);
        output += ordered ? "</ol>\n" : "</ul>\n";
    }

    void renderListItem(const Node& node, std::string& output) {
        const auto* data = std::get_if<ListItemData>(&node.payload);
        output += "<li class=\"mw-list-item";
        if (data && data->task) {
            output += " task-list-item";
        }
        output += '"';
        appendSourceAttributes(node, output);
        output += ">";
        if (data && data->task) {
            output += "<input class=\"task-list-item-checkbox\" type=\"checkbox\" disabled";
            if (data->checked) {
                output += " checked";
            }
            output += "> ";
        }
        for (const auto& child : node.children) {
            renderNode(*child, output, true);
        }
        output += "</li>\n";
    }

    void renderTableCell(const Node& node, std::string& output) {
        const auto* data = std::get_if<TableCellData>(&node.payload);
        const bool header = data && data->header;
        output += header ? "<th" : "<td";
        if (data && !data->alignment.empty()) {
            output += " align=\"";
            output += data->alignment;
            output += '"';
        }
        output += ">";
        renderChildren(node, output);
        output += header ? "</th>\n" : "</td>\n";
    }

    void renderCodeBlock(const Node& node, std::string& output) {
        const auto* data = std::get_if<CodeBlockData>(&node.payload);
        const auto language = data ? languageClass(data->language) : std::string{};
        if (options.extensions.mermaid && language == "mermaid") {
            output += "<div class=\"mermaid\"";
            appendSourceAttributes(node, output);
            output += ">";
            output += escapeHtmlText(node.literal);
            output += "</div>\n";
            return;
        }
        output += "<pre class=\"mw-code-block\"";
        if (!language.empty()) {
            output += " data-lang=\"";
            output += escapeHtmlAttribute(language);
            output += '"';
        }
        appendSourceAttributes(node, output);
        output += "><code";
        if (!language.empty()) {
            output += " class=\"language-";
            output += escapeHtmlAttribute(language);
            output += '"';
        }
        output += ">";
        output += escapeHtmlText(node.literal);
        output += "</code></pre>\n";
    }

    void renderRawHtml(const Node& node, std::string& output) {
        const auto* data = std::get_if<HtmlData>(&node.payload);
        const std::string_view raw = data ? data->raw : std::string_view{};
        if (options.htmlPolicy == HtmlPolicy::Trusted) {
            output += raw;
            return;
        }
        if (options.htmlPolicy == HtmlPolicy::Sanitized && sanitizer) {
            auto sanitized = sanitizer->sanitize(raw, node.range);
            diagnostics.insert(
                diagnostics.end(),
                std::make_move_iterator(sanitized.diagnostics.begin()),
                std::make_move_iterator(sanitized.diagnostics.end()));
            if (sanitized.accepted) {
                output += sanitized.html;
            } else {
                output += escapeHtmlText(raw);
            }
            return;
        }
        if (options.htmlPolicy == HtmlPolicy::Sanitized && !sanitizer) {
            const auto severity = options.strictHtmlPolicy
                ? DiagnosticSeverity::Error
                : DiagnosticSeverity::Warning;
            diagnostics.push_back({
                severity,
                "MW3001",
                "Sanitized HTML was requested but no sanitizer is configured; "
                "raw HTML was escaped.",
                node.range,
            });
            failed = failed || options.strictHtmlPolicy;
        }
        output += escapeHtmlText(raw);
    }

    void renderLink(const Node& node, std::string& output) {
        const auto* data = std::get_if<LinkData>(&node.payload);
        if (!data || !isSafeUrl(data->destination)) {
            diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW3002",
                "An unsafe link destination was removed.",
                node.range,
            });
            renderChildren(node, output);
            return;
        }
        output += "<a class=\"mw-link\" href=\"";
        output += escapeHtmlAttribute(data->destination);
        output += '"';
        if (!data->title.empty()) {
            output += " title=\"";
            output += escapeHtmlAttribute(data->title);
            output += '"';
        }
        output += ">";
        renderChildren(node, output);
        output += "</a>";
    }

    void renderImage(const Node& node, std::string& output) {
        const auto* data = std::get_if<ImageData>(&node.payload);
        if (!data || !isSafeUrl(data->destination)) {
            diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW3003",
                "An unsafe image destination was removed.",
                node.range,
            });
            output += escapeHtmlText(node.literal);
            return;
        }
        std::string src = data->destination;
        std::string style;
        if (src.ends_with("#center")) {
            style = "display: block; margin: 0 auto;";
            src = src.substr(0, src.length() - 7);
        } else if (src.ends_with("#left")) {
            style = "float: left; margin-right: 1em;";
            src = src.substr(0, src.length() - 5);
        } else if (src.ends_with("#right")) {
            style = "float: right; margin-left: 1em;";
            src = src.substr(0, src.length() - 6);
        }

        output += "<img class=\"mw-image\" src=\"";
        output += escapeHtmlAttribute(src);
        output += "\" alt=\"";
        output += escapeHtmlAttribute(node.literal);
        output += '"';
        if (!style.empty()) {
            output += " style=\"";
            output += style;
            output += '"';
        }
        if (!data->title.empty()) {
            output += " title=\"";
            output += escapeHtmlAttribute(data->title);
            output += '"';
        }
        output += ">";
    }

    void renderAutoLink(const Node& node, std::string& output) {
        const auto* data = std::get_if<LinkData>(&node.payload);
        if (!data || !isSafeUrl(data->destination)) {
            output += escapeHtmlText(node.literal);
            return;
        }
        output += "<a class=\"mw-link mw-autolink\" href=\"";
        output += escapeHtmlAttribute(data->destination);
        output += "\">";
        output += escapeHtmlText(node.literal);
        output += "</a>";
    }

public:
    std::string tocHtml_;
    std::vector<const Node*> footnoteDefs_;
    std::unordered_map<const Node*, std::string> headingSlugs_;
    std::unordered_map<const Node*, std::string> footnoteRefIds_;
    std::unordered_map<std::string, std::vector<std::string>> footnoteRefs_;
    std::unordered_map<std::string, std::size_t> slugCounts_;

    void prePass(const Node& node) {
        if (node.type == NodeType::Heading) {
            const auto* data = std::get_if<HeadingData>(&node.payload);
            const unsigned int level = data ? data->level : 1;
            auto slug = slugBase(plainText(node));
            auto& count = slugCounts_[slug];
            if (count > 0) {
                slug += '-' + std::to_string(count);
            }
            ++count;
            headingSlugs_[&node] = slug;
            tocHtml_ += "<li class=\"toc-level-" + std::to_string(level) + "\"><a href=\"#" + escapeHtmlAttribute(slug) + "\">" + escapeHtmlText(plainText(node)) + "</a></li>\n";
        } else if (node.type == NodeType::FootnoteRef) {
            const auto* data = std::get_if<FootnoteData>(&node.payload);
            const std::string id = data ? data->id : "";
            auto& references = footnoteRefs_[id];
            std::string referenceId = "fnref-" + id;
            if (!references.empty()) {
                referenceId += '-' + std::to_string(references.size() + 1);
            }
            references.push_back(referenceId);
            footnoteRefIds_[&node] = std::move(referenceId);
        } else if (node.type == NodeType::FootnoteDef) {
            footnoteDefs_.push_back(&node);
        }
        for (const auto& child : node.children) {
            prePass(*child);
        }
    }
};

} // namespace

HtmlRenderResult HtmlRenderer::render(
    const Node& document,
    const RenderOptions& options,
    const HtmlSanitizer* sanitizer) const {
    HtmlRenderResult result;
    if (document.type != NodeType::Document) {
        result.diagnostics.push_back({
            DiagnosticSeverity::Error,
            "MW5001",
            "HTML rendering requires a Document root node.",
            document.range,
        });
        return result;
    }

    RendererState state(options, sanitizer);
    state.prePass(document);
    result.fragment =
        "<article class=\"mw-document markdown-body\">\n";
    state.renderChildren(document, result.fragment);

    if (options.extensions.footnotes && !state.footnoteDefs_.empty()) {
        result.fragment += "<hr class=\"footnotes-sep\">\n<section class=\"footnotes\">\n<ol>\n";
        for (const auto* defNode : state.footnoteDefs_) {
            const auto* data = std::get_if<FootnoteData>(&defNode->payload);
            std::string id = data ? data->id : "";
            result.fragment += "<li id=\"fn-" + escapeHtmlAttribute(id) + "\">\n";
            state.renderChildren(*defNode, result.fragment);
            const auto references = state.footnoteRefs_.find(id);
            if (references != state.footnoteRefs_.end()) {
                for (std::size_t index = 0;
                     index < references->second.size();
                     ++index) {
                    result.fragment += " <a href=\"#" +
                        escapeHtmlAttribute(references->second[index]) +
                        "\" aria-label=\"Back to reference " +
                        std::to_string(index + 1) + "\">↩</a>";
                }
            }
            result.fragment += '\n';
            result.fragment += "</li>\n";
        }
        result.fragment += "</ol>\n</section>\n";
    }

    result.fragment += "</article>\n";
    result.diagnostics = std::move(state.diagnostics);
    result.ok = !state.failed;
    return result;
}

std::string assembleDocument(
    std::string_view fragment,
    std::string_view css,
    const RenderOptions& options) {
    std::string html;
    html += "<!doctype html>\n<html lang=\"";
    html += escapeHtmlAttribute(options.language);
    html += "\">\n<head>\n";
    html += "  <meta charset=\"utf-8\">\n";
    html += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    html += "  <title>";
    html += escapeHtmlText(options.title);
    html += "</title>\n";
    if (options.includeCss && !css.empty()) {
        html += "  <style>\n";
        html += css;
        if (css.back() != '\n') {
            html += '\n';
        }
        html += "    .markdown-body {\n";
        html += "      box-sizing: border-box;\n";
        html += "      min-width: 200px;\n";
        html += "      max-width: 980px;\n";
        html += "      margin: 0 auto;\n";
        html += "      padding: 45px;\n";
        html += "    }\n";
        html += "    @media (max-width: 767px) {\n";
        html += "      .markdown-body {\n";
        html += "        padding: 15px;\n";
        html += "      }\n";
        html += "    }\n";
        html += "  </style>\n";
    }
    html += "</head>\n<body style=\"background-color: var(--color-canvas-default, #fff);\">\n";
    html += fragment;

    if (options.extensions.latexMath) {
        if (!mathjaxJs.empty()) {
            html += "<script>\n";
            html += "window.MathJax = {\n";
            html += "  tex: {\n";
            html += "    inlineMath: [['$', '$'], ['\\\\(', '\\\\)']],\n";
            html += "    displayMath: [['$$', '$$'], ['\\\\[', '\\\\]']]\n";
            html += "  },\n";
            html += "  svg: { fontCache: 'global' }\n";
            html += "};\n";
            html += "</script>\n";
            html += "<script>\n" + std::string(mathjaxJs) + "\n</script>\n";
        }
    }

    if (options.extensions.mermaid) {
        if (!mermaidJs.empty()) {
            html += "<script>\n" + std::string(mermaidJs) + "\n</script>\n";
            html += "<script>mermaid.initialize({startOnLoad:true});</script>\n";
        }
    }

    if (options.extensions.highlight) {
        if (!highlightJs.empty() && !highlightCss.empty()) {
            html += "<style>\n" + std::string(highlightCss) + "\n</style>\n";
            html += "<script>\n" + std::string(highlightJs) + "\n</script>\n";
            html += "<script>hljs.highlightAll();</script>\n";
        }
    }

    html += "</body>\n</html>\n";
    return html;
}

} // namespace mwrender::detail
