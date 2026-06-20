#include <mwrender/engine.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include <mwrender/parser.hpp>
#include <mwrender/query.hpp>
#include <unordered_map>

#include "analysis/document_analysis.hpp"
#include "render/html_renderer.hpp"
#include "theme/theme_manager.hpp"

namespace mwrender {
namespace {

bool containsRemoteCssResource(std::string_view css) {
    std::string normalized;
    normalized.reserve(css.size());
    bool inComment = false;
    for (size_t i = 0; i < css.size(); ++i) {
        if (!inComment && i + 1 < css.size() && css[i] == '/' && css[i+1] == '*') {
            inComment = true;
            ++i;
            continue;
        }
        if (inComment && i + 1 < css.size() && css[i] == '*' && css[i+1] == '/') {
            inComment = false;
            ++i;
            continue;
        }
        if (inComment) continue;

        if (css[i] == '\\' && i + 1 < css.size()) {
            size_t start = i + 1;
            size_t end = start;
            while (end < css.size() && end - start < 6 && std::isxdigit(static_cast<unsigned char>(css[end]))) {
                ++end;
            }
            if (end > start) {
                std::string hexStr(css.substr(start, end - start));
                try {
                    int codePoint = std::stoi(hexStr, nullptr, 16);
                    if (codePoint > 0 && codePoint < 128 && std::isspace(codePoint) == 0) {
                        normalized += static_cast<char>(std::tolower(codePoint));
                    }
                } catch (...) {}
                i = end - 1;
                if (i + 1 < css.size() && (css[i + 1] == ' ' || css[i + 1] == '\t' || css[i + 1] == '\n' || css[i + 1] == '\r' || css[i + 1] == '\f')) {
                    ++i;
                }
            } else {
                ++i;
                const auto byte = static_cast<unsigned char>(css[i]);
                if (std::isspace(byte) == 0) {
                    normalized += static_cast<char>(std::tolower(byte));
                }
            }
        } else {
            const auto byte = static_cast<unsigned char>(css[i]);
            if (std::isspace(byte) == 0) {
                normalized += static_cast<char>(std::tolower(byte));
            }
        }
    }
    return normalized.find("@import") != std::string::npos ||
        normalized.find("url(http:") != std::string::npos ||
        normalized.find("url(https:") != std::string::npos ||
        normalized.find("url(\"http:") != std::string::npos ||
        normalized.find("url(\"https:") != std::string::npos ||
        normalized.find("url('http:") != std::string::npos ||
        normalized.find("url('https:") != std::string::npos ||
        normalized.find("url(//") != std::string::npos ||
        normalized.find("url(\"//") != std::string::npos ||
        normalized.find("url('//") != std::string::npos;
}

bool containsStyleEndTag(std::string_view css) {
    constexpr std::string_view endTag = "</style";
    if (css.size() < endTag.size()) {
        return false;
    }
    for (std::size_t index = 0; index + endTag.size() <= css.size(); ++index) {
        bool matches = true;
        for (std::size_t offset = 0; offset < endTag.size(); ++offset) {
            const auto byte = static_cast<unsigned char>(css[index + offset]);
            if (static_cast<char>(std::tolower(byte)) != endTag[offset]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

bool isWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart) {
        if (candidatePart == candidate.end() || *rootPart != *candidatePart) {
            return false;
        }
    }
    return true;
}

} // namespace

static std::unique_ptr<Node> cloneSubtree(const Node& node);

class Engine::Impl {
public:
    explicit Impl(EngineOptions engineOptions)
        : options(std::move(engineOptions)),
          parser(options),
          themeManager(options.maxThemeFileBytes),
          sanitizer(std::make_shared<SafeHtmlSanitizer>()) {}

    EngineOptions options;
    Parser parser;
    detail::HtmlRenderer htmlRenderer;
    detail::ThemeManager themeManager;
    std::shared_ptr<const HtmlSanitizer> sanitizer;
    mutable std::mutex resourcesMutex;
};

Engine::Engine()
    : Engine(EngineOptions{}) {}

Engine::Engine(EngineOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

void Engine::addThemeRoot(std::filesystem::path path, ThemeOrigin origin) {
    std::scoped_lock lock(impl_->resourcesMutex);
    impl_->themeManager.addRoot(std::move(path), origin);
}

void Engine::setHtmlSanitizer(
    std::shared_ptr<const HtmlSanitizer> sanitizer) {
    std::scoped_lock lock(impl_->resourcesMutex);
    impl_->sanitizer = std::move(sanitizer);
}

ParseResult Engine::parse(
    std::string_view markdown,
    const ParseOptions& options) const {
    return impl_->parser.parse(markdown, options);
}

IncrementalParseResult Engine::update(
    const Node& oldDocument,
    const TextChange& change) const {
    return update(oldDocument, oldDocument.literal, change);
}

IncrementalParseResult Engine::update(
    const Node& oldDocument,
    std::string_view oldMarkdown,
    const TextChange& change) const {
    IncrementalParseResult result;

    if (change.from > change.to || change.to > oldMarkdown.size()) {
        return result;
    }

    std::string newMarkdown(oldMarkdown);
    newMarkdown.replace(change.from, change.to - change.from,
                        change.insertedText);

    auto parseResult = impl_->parser.parse(
        newMarkdown, ParseOptions{});
    if (!parseResult.ok || !parseResult.document) {
        return result;
    }

    std::vector<std::string> oldIds;
    auto collectIds = [](const Node& node, std::vector<std::string>& ids,
                          auto& ref) -> void {
        if (!node.id.empty()) ids.push_back(node.id);
        for (const auto& child : node.children) {
            if (child) ref(*child, ids, ref);
        }
    };
    collectIds(oldDocument, oldIds, collectIds);

    std::vector<std::string> newIds;
    collectIds(*parseResult.document, newIds, collectIds);

    auto sortedOld = oldIds;
    auto sortedNew = newIds;
    std::sort(sortedOld.begin(), sortedOld.end());
    std::sort(sortedNew.begin(), sortedNew.end());

    std::set_difference(sortedNew.begin(), sortedNew.end(),
                        sortedOld.begin(), sortedOld.end(),
                        std::back_inserter(result.insertedNodeIds));

    std::set_difference(sortedOld.begin(), sortedOld.end(),
                        sortedNew.begin(), sortedNew.end(),
                        std::back_inserter(result.removedNodeIds));

    std::unordered_map<std::string, std::string> oldHashes;
    auto collectHashes = [](const Node& node, std::unordered_map<std::string, std::string>& map, auto& ref) -> void {
        if (!node.id.empty()) map[node.id] = node.contentHash;
        for (const auto& child : node.children) {
            if (child) ref(*child, map, ref);
        }
    };
    collectHashes(oldDocument, oldHashes, collectHashes);

    std::unordered_map<std::string, std::string> newHashes;
    collectHashes(*parseResult.document, newHashes, collectHashes);

    for (const auto& oldId : oldIds) {
        if (std::find(newIds.begin(), newIds.end(), oldId) != newIds.end()) {
            if (oldHashes[oldId] != newHashes[oldId]) {
                result.changedNodeIds.push_back(oldId);
            }
        }
    }

    result.ok = parseResult.ok;
    result.document = std::move(parseResult.document);
    result.diagnostics = std::move(parseResult.diagnostics);
    return result;
}

ParseResult Engine::parseFragment(
    std::string_view fragment,
    NodeType expectedType,
    const ParseOptions& options) const {
    // For simple block types, the fragment can be parsed directly.
    // Wrap with a trailing newline to ensure proper block termination.
    std::string wrapped(fragment);
    if (wrapped.empty() || wrapped.back() != '\n') {
        wrapped += '\n';
    }
    // Ensure blank line separation for blocks that need it
    if (expectedType == NodeType::Paragraph ||
        expectedType == NodeType::Heading ||
        expectedType == NodeType::ThematicBreak) {
        if (wrapped.size() < 2 || wrapped.substr(wrapped.size() - 2) != "\n\n") {
            wrapped += '\n';
        }
    }
    return impl_->parser.parse(wrapped, options);
}

RenderResult Engine::renderNode(const NodeRenderRequest& request) const {
    RenderResult result;
    if (!request.document) {
        result.diagnostics.push_back({
            DiagnosticSeverity::Error, "MW3001",
            "renderNode: null document pointer.", std::nullopt});
        return result;
    }

    const Node* target = findNodeById(*request.document, request.nodeId);
    if (!target) {
        result.diagnostics.push_back({
            DiagnosticSeverity::Error, "MW3002",
            "renderNode: node '" + request.nodeId + "' not found.",
            std::nullopt});
        return result;
    }

    auto sanitizer = [this]() -> std::shared_ptr<const HtmlSanitizer> {
        std::scoped_lock lock(impl_->resourcesMutex);
        return impl_->sanitizer;
    }();

    Node wrapper;
    wrapper.type = NodeType::Document;
    wrapper.range = target->range;
    wrapper.contentRange = target->contentRange;
    wrapper.children.push_back(cloneSubtree(*target));

    auto renderResult = impl_->htmlRenderer.render(
        wrapper, request.options, sanitizer.get());
    result.diagnostics = std::move(renderResult.diagnostics);
    if (!renderResult.ok) {
        return result;
    }

    result.fragment = std::move(renderResult.fragment);
    result.ok = true;
    return result;
}

RenderResult Engine::render(const RenderRequest& request) const {
    RenderResult result;

    auto parseResult = impl_->parser.parse(
        request.markdown,
        ParseOptions{request.options.extensions});
    result.diagnostics = std::move(parseResult.diagnostics);
    if (!parseResult.ok || !parseResult.document) {
        return result;
    }
    result.outline = detail::buildOutline(*parseResult.document);
    result.stats = detail::buildWordStats(*parseResult.document);

    RenderOptions effectiveOptions = request.options;
    if (effectiveOptions.allowDocumentTheme &&
        parseResult.metadata.theme &&
        effectiveOptions.themeId == "github-light") {
        effectiveOptions.themeId = *parseResult.metadata.theme;
    }
    if (parseResult.metadata.title &&
        effectiveOptions.title == "Markdown Document") {
        effectiveOptions.title = *parseResult.metadata.title;
    }

    std::shared_ptr<const HtmlSanitizer> sanitizer;
    detail::ThemeManager themeManagerSnapshot(
        impl_->options.maxThemeFileBytes);
    {
        std::scoped_lock lock(impl_->resourcesMutex);
        sanitizer = impl_->sanitizer;
        themeManagerSnapshot = impl_->themeManager;
    }
    auto theme = themeManagerSnapshot.resolve(
        effectiveOptions.themeId,
        effectiveOptions.strictTheme);
    result.diagnostics.insert(
        result.diagnostics.end(),
        std::make_move_iterator(theme.diagnostics.begin()),
        std::make_move_iterator(theme.diagnostics.end()));
    if (!theme.ok) {
        return result;
    }
    if (!effectiveOptions.allowRemoteCssResources &&
        containsRemoteCssResource(theme.css)) {
        result.diagnostics.push_back({
            effectiveOptions.strictTheme
                ? DiagnosticSeverity::Error
                : DiagnosticSeverity::Warning,
            "MW2005",
            "Theme '" + theme.id +
                "' contains remote CSS resources." +
                (effectiveOptions.strictTheme
                     ? std::string{}
                     : " Falling back to github-light."),
            std::nullopt,
        });
        if (effectiveOptions.strictTheme) {
            return result;
        }
        theme = themeManagerSnapshot.resolve("github-light", true);
        if (!theme.ok) {
            return result;
        }
    }
    if (containsStyleEndTag(theme.css)) {
        result.diagnostics.push_back({
            effectiveOptions.strictTheme
                ? DiagnosticSeverity::Error
                : DiagnosticSeverity::Warning,
            "MW2006",
            "Theme '" + theme.id +
                "' contains an HTML style end tag." +
                (effectiveOptions.strictTheme
                     ? std::string{}
                     : " Falling back to github-light."),
            std::nullopt,
        });
        if (effectiveOptions.strictTheme) {
            return result;
        }
        theme = themeManagerSnapshot.resolve("github-light", true);
        if (!theme.ok) {
            return result;
        }
    }

    auto renderResult = impl_->htmlRenderer.render(
        *parseResult.document,
        effectiveOptions,
        sanitizer.get());
    result.diagnostics.insert(
        result.diagnostics.end(),
        std::make_move_iterator(renderResult.diagnostics.begin()),
        std::make_move_iterator(renderResult.diagnostics.end()));
    if (!renderResult.ok) {
        return result;
    }

    result.fragment = std::move(renderResult.fragment);
    result.resolvedThemeId = std::move(theme.id);
    result.css = std::move(theme.css);

    const auto appendCss = [&](std::string_view css, std::string_view name) {
        if (containsStyleEndTag(css)) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW4006",
                "CSS source '" + std::string(name) +
                    "' was rejected because it can escape the HTML style element.",
                std::nullopt,
            });
            return;
        }
        if (!effectiveOptions.allowRemoteCssResources &&
            containsRemoteCssResource(css)) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW4002",
                "CSS source '" + std::string(name) +
                    "' was rejected because it contains remote resources.",
                std::nullopt,
            });
            return;
        }
        result.css += "\n";
        result.css += css;
    };

    if (effectiveOptions.allowDocumentCss &&
        request.sourcePath &&
        !parseResult.metadata.css.empty()) {
        std::error_code error;
        const auto sourceRoot = std::filesystem::weakly_canonical(
            request.sourcePath->parent_path(),
            error);
        if (!error) {
            for (const auto& relativeCss : parseResult.metadata.css) {
                const std::filesystem::path relativePath(relativeCss);
                if (relativePath.is_absolute()) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4003",
                        "Absolute document CSS path was rejected: " +
                            relativeCss,
                        std::nullopt,
                    });
                    continue;
                }
                const auto cssPath = std::filesystem::weakly_canonical(
                    sourceRoot / relativePath,
                    error);
                if (error || !isWithin(sourceRoot, cssPath)) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4004",
                        "Document CSS path escapes the document directory: " +
                            relativeCss,
                        std::nullopt,
                    });
                    error.clear();
                    continue;
                }
                std::ifstream input(cssPath, std::ios::binary);
                if (!input) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4001",
                        "Document CSS could not be read: " + cssPath.string(),
                        std::nullopt,
                    });
                    continue;
                }
                const auto cssSize =
                    std::filesystem::file_size(cssPath, error);
                if (error || cssSize > impl_->options.maxThemeFileBytes) {
                    result.diagnostics.push_back({
                        DiagnosticSeverity::Warning,
                        "MW4005",
                        "Document CSS exceeds the configured maximum size: " +
                            cssPath.string(),
                        std::nullopt,
                    });
                    error.clear();
                    continue;
                }
                const std::string css{
                    std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>()};
                appendCss(css, cssPath.string());
            }
        }
    }

    for (const auto& cssPath : request.css.files) {
        std::error_code error;
        const auto cssSize = std::filesystem::file_size(cssPath, error);
        if (error || cssSize > impl_->options.maxThemeFileBytes) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW4005",
                "Custom CSS exceeds the configured maximum size or is unreadable: " +
                    cssPath.string(),
                std::nullopt,
            });
            continue;
        }
        std::ifstream input(cssPath, std::ios::binary);
        if (!input) {
            result.diagnostics.push_back({
                DiagnosticSeverity::Warning,
                "MW4001",
                "Custom CSS could not be read: " + cssPath.string(),
                std::nullopt,
            });
            continue;
        }
        const std::string css{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        appendCss(css, cssPath.string());
    }
    for (const auto& css : request.css.text) {
        appendCss(css, "request");
    }
    result.html = effectiveOptions.outputMode == OutputMode::Fragment
        ? result.fragment
        : detail::assembleDocument(
              result.fragment,
              result.css,
              effectiveOptions);

    if (effectiveOptions.includeAst) {
        result.document = std::move(parseResult.document);
    }

    result.ok = std::none_of(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [](const Diagnostic& diagnostic) {
            return diagnostic.severity == DiagnosticSeverity::Error;
        });
    return result;
}

std::string Engine::serialize(
    const Node& document,
    const MarkdownStyle& style) const {
    return serializeMarkdown(document, style);
}

static std::unique_ptr<Node> cloneSubtree(const Node& node) {
    auto n = std::make_unique<Node>();
    n->id = node.id;
    n->type = node.type;
    n->range = node.range;
    n->contentRange = node.contentRange;
    n->markerRanges = node.markerRanges;
    n->payload = node.payload;
    n->literal = node.literal;
    n->children.reserve(node.children.size());
    for (const auto& child : node.children) {
        n->children.push_back(cloneSubtree(*child));
    }
    return n;
}

std::unique_ptr<Node> Engine::applyCommand(
    const Node& document,
    const Command& command) const {
    auto doc = cloneSubtree(document);

    auto target = findNodeById(*doc, command.nodeId);
    if (!target) {
        return doc;
    }

    switch (command.type) {
    case Command::Type::ToggleTask: {
        auto* data = std::get_if<ListItemData>(&target->payload);
        if (data) {
            data->task = true;
            data->checked = !data->checked;
        }
        break;
    }
    case Command::Type::SetHeadingLevel: {
        auto* data = std::get_if<HeadingData>(&target->payload);
        if (data && !command.arg1.empty()) {
            int level = std::stoi(command.arg1);
            if (level >= 1 && level <= 6) {
                data->level = static_cast<std::uint8_t>(level);
            }
        }
        break;
    }
    case Command::Type::WrapStrong: {
        auto wrapper = std::make_unique<Node>();
        wrapper->type = NodeType::Strong;
        wrapper->children = std::move(target->children);
        target->children.clear();
        target->children.push_back(std::move(wrapper));
        break;
    }
    case Command::Type::WrapEmphasis: {
        auto wrapper = std::make_unique<Node>();
        wrapper->type = NodeType::Emphasis;
        wrapper->children = std::move(target->children);
        target->children.clear();
        target->children.push_back(std::move(wrapper));
        break;
    }
    case Command::Type::InsertLink: {
        auto wrapper = std::make_unique<Node>();
        wrapper->type = NodeType::Link;
        wrapper->payload = LinkData{command.arg1, command.arg2};
        auto textNode = std::make_unique<Node>();
        textNode->type = NodeType::Text;
        textNode->literal = command.arg1;
        wrapper->children.push_back(std::move(textNode));
        target->children.push_back(std::move(wrapper));
        break;
    }
    case Command::Type::ToggleStrikethrough: {
        auto wrapper = std::make_unique<Node>();
        wrapper->type = NodeType::Strikethrough;
        wrapper->children = std::move(target->children);
        target->children.clear();
        target->children.push_back(std::move(wrapper));
        break;
    }
    case Command::Type::IndentListItem: {
        auto* parent = findNodeById(*doc, command.arg1);
        if (parent && parent->type == NodeType::List) {
            for (std::size_t i = 1; i < parent->children.size(); ++i) {
                if (parent->children[i]->id == command.nodeId && i > 0) {
                    auto nestedList = std::make_unique<Node>();
                    nestedList->type = NodeType::List;
                    nestedList->payload = parent->payload;
                    nestedList->children.push_back(
                        std::move(parent->children[i]));
                    parent->children[i - 1]->children.push_back(
                        std::move(nestedList));
                    parent->children.erase(
                        parent->children.begin() +
                        static_cast<std::ptrdiff_t>(i));
                    break;
                }
            }
        }
        break;
    }
    case Command::Type::ToggleList: {
        auto* data = std::get_if<ListItemData>(&target->payload);
        if (data) {
            data->task = false;
        }
        break;
    }
    case Command::Type::InsertImage: {
        auto wrapper = std::make_unique<Node>();
        wrapper->type = NodeType::Image;
        wrapper->payload = ImageData{command.arg1, command.arg2};
        wrapper->literal = command.arg2.empty() ? "image" : command.arg2;
        target->children.push_back(std::move(wrapper));
        break;
    }
    case Command::Type::OutdentListItem:
        break;
    }

    return doc;
}

std::vector<ThemeInfo> Engine::listThemes() const {
    detail::ThemeManager snapshot(impl_->options.maxThemeFileBytes);
    {
        std::scoped_lock lock(impl_->resourcesMutex);
        snapshot = impl_->themeManager;
    }
    return snapshot.listThemes();
}

} // namespace mwrender
