#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <mwrender/engine.hpp>
#include <mwrender/version.hpp>

#include "support/json.hpp"

namespace {

struct CliOptions {
    std::optional<std::filesystem::path> input;
    std::optional<std::filesystem::path> output;
    std::vector<std::filesystem::path> themeRoots;
    std::vector<std::filesystem::path> cssFiles;
    mwrender::RenderOptions render;
    bool listThemes = false;
    bool dumpAst = false;
};

void printUsage(std::ostream& output) {
    output
        << "Usage: mwrender [input.md] [options]\n"
        << "\n"
        << "Options:\n"
        << "  -o, --output <file>         Write output to a file; '-' means stdout\n"
        << "  --fragment                 Output only the article fragment\n"
        << "  --theme <id>               Select a theme (default: github-light)\n"
        << "  --config <file>            Load an mwrender JSON configuration\n"
        << "  --theme-path <directory>   Add an external theme root\n"
        << "  --list-themes              List available themes\n"
        << "  --css <file>               Append a custom CSS file\n"
        << "  --html-policy <policy>     disabled, sanitized, or trusted\n"
        << "  --title <title>            Set the full-document title\n"
        << "  --lang <language>          Set the HTML language\n"
        << "  --strict                   Fail on missing themes/sanitizer\n"
        << "  --no-tables                Disable GFM tables\n"
        << "  --no-task-lists            Disable GFM task lists\n"
        << "  --no-strikethrough         Disable GFM strikethrough\n"
        << "  --no-autolinks             Disable GFM automatic links\n"
        << "  --allow-document-css       Allow Front Matter CSS files\n"
        << "  --allow-remote-css         Allow CSS remote resources\n"
        << "  --dump-ast                 Print the parsed AST instead of HTML\n"
        << "  --version                  Print version information\n"
        << "  -h, --help                 Show this help\n"
        << "\n"
        << "Input defaults to stdin when omitted or '-'.\n";
}

bool requireValue(
    int argc,
    char** argv,
    int& index,
    std::string_view option,
    std::string& value) {
    if (index + 1 >= argc) {
        std::cerr << option << " requires a value.\n";
        return false;
    }
    value = argv[++index];
    return true;
}

std::optional<CliOptions> parseArguments(
    int argc,
    char** argv,
    CliOptions options) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        std::string value;

        if (argument == "--version") {
            std::cout << "mwrender " << mwrender::versionString << '\n';
            std::exit(0);
        }
        if (argument == "--help" || argument == "-h") {
            printUsage(std::cout);
            std::exit(0);
        }
        if (argument == "--fragment") {
            options.render.outputMode = mwrender::OutputMode::Fragment;
        } else if (argument == "--list-themes") {
            options.listThemes = true;
        } else if (argument == "--dump-ast") {
            options.dumpAst = true;
        } else if (argument == "--strict") {
            options.render.strictTheme = true;
            options.render.strictHtmlPolicy = true;
        } else if (argument == "--no-tables") {
            options.render.extensions.tables = false;
        } else if (argument == "--no-task-lists") {
            options.render.extensions.taskLists = false;
        } else if (argument == "--no-strikethrough") {
            options.render.extensions.strikethrough = false;
        } else if (argument == "--no-autolinks") {
            options.render.extensions.autoLinks = false;
        } else if (argument == "--allow-document-css") {
            options.render.allowDocumentCss = true;
        } else if (argument == "--allow-remote-css") {
            options.render.allowRemoteCssResources = true;
        } else if (argument == "--output" || argument == "-o") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
            if (value != "-") {
                options.output = value;
            }
        } else if (argument == "--config") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
        } else if (argument == "--theme") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
            options.render.themeId = value;
        } else if (argument == "--theme-path") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
            options.themeRoots.emplace_back(value);
        } else if (argument == "--css") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
            options.cssFiles.emplace_back(value);
        } else if (argument == "--title") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
            options.render.title = value;
        } else if (argument == "--lang") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
            options.render.language = value;
        } else if (argument == "--html-policy") {
            if (!requireValue(argc, argv, index, argument, value)) {
                return std::nullopt;
            }
            if (value == "disabled") {
                options.render.htmlPolicy = mwrender::HtmlPolicy::Disabled;
            } else if (value == "sanitized") {
                options.render.htmlPolicy = mwrender::HtmlPolicy::Sanitized;
            } else if (value == "trusted") {
                options.render.htmlPolicy = mwrender::HtmlPolicy::Trusted;
            } else {
                std::cerr << "Unknown HTML policy: " << value << '\n';
                return std::nullopt;
            }
        } else if (argument == "-") {
            if (options.input) {
                std::cerr << "Only one input file may be specified.\n";
                return std::nullopt;
            }
            options.input = argument;
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "Unknown option: " << argument << '\n';
            return std::nullopt;
        } else if (options.input) {
            std::cerr << "Only one input file may be specified.\n";
            return std::nullopt;
        } else {
            options.input = argument;
        }
    }
    return options;
}

const mwrender::detail::JsonValue* member(
    const mwrender::detail::JsonValue* object,
    std::string_view key) {
    return object ? object->get(key) : nullptr;
}

bool applyConfig(
    const std::filesystem::path& configPath,
    CliOptions& options) {
    std::ifstream input(configPath, std::ios::binary);
    if (!input) {
        std::cerr << "Could not read config file: " << configPath.string() << '\n';
        return false;
    }
    const std::string source{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    const auto parsed = mwrender::detail::parseJson(source);
    if (!parsed.value || !parsed.value->asObject()) {
        std::cerr << "Invalid config JSON: " << parsed.error << '\n';
        return false;
    }

    if (const auto* theme = member(&*parsed.value, "theme")) {
        if (const auto* value = theme->asString()) {
            options.render.themeId = *value;
        }
    }

    if (const auto* output = member(&*parsed.value, "output")) {
        if (const auto* mode = member(output, "mode")) {
            if (const auto* value = mode->asString()) {
                if (*value == "fragment") {
                    options.render.outputMode =
                        mwrender::OutputMode::Fragment;
                } else if (*value == "full-document") {
                    options.render.outputMode =
                        mwrender::OutputMode::FullDocument;
                }
            }
        }
        if (const auto* language = member(output, "language")) {
            if (const auto* value = language->asString()) {
                options.render.language = *value;
            }
        }
        if (const auto* includeCss = member(output, "includeCss")) {
            if (const auto* value = includeCss->asBool()) {
                options.render.includeCss = *value;
            }
        }
    }

    if (const auto* html = member(&*parsed.value, "html")) {
        bool enabled = true;
        if (const auto* enabledValue = member(html, "enabled")) {
            if (const auto* value = enabledValue->asBool()) {
                enabled = *value;
            }
        }
        options.render.htmlPolicy = mwrender::HtmlPolicy::Disabled;
        if (enabled) {
            if (const auto* policy = member(html, "policy")) {
                if (const auto* value = policy->asString()) {
                    if (*value == "sanitized") {
                        options.render.htmlPolicy =
                            mwrender::HtmlPolicy::Sanitized;
                    } else if (*value == "trusted") {
                        options.render.htmlPolicy =
                            mwrender::HtmlPolicy::Trusted;
                    }
                }
            }
        }
    }

    if (const auto* css = member(&*parsed.value, "css")) {
        bool enabled = true;
        if (const auto* enabledValue = member(css, "enabled")) {
            if (const auto* value = enabledValue->asBool()) {
                enabled = *value;
            }
        }
        if (const auto* remote = member(css, "allowRemoteUrl")) {
            if (const auto* value = remote->asBool()) {
                options.render.allowRemoteCssResources = *value;
            }
        }
        if (enabled) {
            if (const auto* filesValue = member(css, "files")) {
                if (const auto* files = filesValue->asArray()) {
                    for (const auto& file : *files) {
                        if (const auto* path = file.asString()) {
                            options.cssFiles.push_back(
                                configPath.parent_path() / *path);
                        }
                    }
                }
            }
        }
    }

    if (const auto* sourceMap = member(&*parsed.value, "sourceMap")) {
        if (const auto* enabledValue = member(sourceMap, "enabled")) {
            if (const auto* value = enabledValue->asBool()) {
                options.render.sourceMap = *value
                    ? mwrender::SourceMapMode::Line
                    : mwrender::SourceMapMode::None;
            }
        }
    }
    return true;
}

std::optional<std::filesystem::path> findConfigPath(
    int argc,
    char** argv) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--config") {
            if (index + 1 >= argc) {
                std::cerr << "--config requires a value.\n";
                return std::filesystem::path{};
            }
            return std::filesystem::path(argv[index + 1]);
        }
    }
    return std::nullopt;
}

std::optional<std::string> readInput(
    const std::optional<std::filesystem::path>& inputPath) {
    if (!inputPath || inputPath->string() == "-") {
        return std::string(
            std::istreambuf_iterator<char>(std::cin),
            std::istreambuf_iterator<char>());
    }

    std::ifstream input(*inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "Could not read input file: " << inputPath->string() << '\n';
        return std::nullopt;
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

bool writeOutput(
    const std::optional<std::filesystem::path>& outputPath,
    std::string_view content) {
    if (!outputPath) {
        std::cout << content;
        return static_cast<bool>(std::cout);
    }
    std::ofstream output(*outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Could not write output file: " << outputPath->string() << '\n';
        return false;
    }
    output << content;
    return static_cast<bool>(output);
}

std::string_view nodeName(mwrender::NodeType type) {
    using enum mwrender::NodeType;
    switch (type) {
    case Document: return "Document";
    case FrontMatter: return "FrontMatter";
    case Heading: return "Heading";
    case Paragraph: return "Paragraph";
    case BlockQuote: return "BlockQuote";
    case List: return "List";
    case ListItem: return "ListItem";
    case CodeBlock: return "CodeBlock";
    case ThematicBreak: return "ThematicBreak";
    case HtmlBlock: return "HtmlBlock";
    case Table: return "Table";
    case TableHead: return "TableHead";
    case TableBody: return "TableBody";
    case TableRow: return "TableRow";
    case TableCell: return "TableCell";
    case Text: return "Text";
    case Emphasis: return "Emphasis";
    case Strong: return "Strong";
    case Strikethrough: return "Strikethrough";
    case InlineCode: return "InlineCode";
    case Link: return "Link";
    case Image: return "Image";
    case HtmlInline: return "HtmlInline";
    case AutoLink: return "AutoLink";
    case SoftBreak: return "SoftBreak";
    case HardBreak: return "HardBreak";
    }
    return "Unknown";
}

void dumpNode(const mwrender::Node& node, std::ostream& output, int depth = 0) {
    output << std::string(static_cast<std::size_t>(depth * 2), ' ')
           << nodeName(node.type);
    if (!node.literal.empty()) {
        output << "(\"" << node.literal << "\")";
    }
    output << " [" << node.range.begin.line << ':' << node.range.begin.column
           << '-' << node.range.end.line << ':' << node.range.end.column
           << "]\n";
    for (const auto& child : node.children) {
        dumpNode(*child, output, depth + 1);
    }
}

void printDiagnostics(const std::vector<mwrender::Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        const char* severity = "info";
        if (diagnostic.severity == mwrender::DiagnosticSeverity::Warning) {
            severity = "warning";
        } else if (diagnostic.severity == mwrender::DiagnosticSeverity::Error) {
            severity = "error";
        }
        std::cerr << severity << ' ' << diagnostic.code << ": "
                  << diagnostic.message << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    CliOptions defaults;
    const auto configPath = findConfigPath(argc, argv);
    if (configPath && configPath->empty()) {
        return 2;
    }
    if (configPath && !applyConfig(*configPath, defaults)) {
        return 2;
    }

    const auto cli = parseArguments(argc, argv, std::move(defaults));
    if (!cli) {
        printUsage(std::cerr);
        return 2;
    }

    mwrender::Engine engine;
    for (const auto& root : cli->themeRoots) {
        engine.addThemeRoot(root);
    }

    if (cli->listThemes) {
        for (const auto& theme : engine.listThemes()) {
            std::cout << theme.id << '\t' << theme.name << '\t'
                      << theme.appearance << '\n';
        }
        return 0;
    }

    const auto markdown = readInput(cli->input);
    if (!markdown) {
        return 3;
    }

    mwrender::RenderRequest request;
    request.markdown = *markdown;
    if (cli->input && cli->input->string() != "-") {
        request.sourcePath = cli->input;
    }
    request.options = cli->render;
    request.css.files = cli->cssFiles;

    auto result = engine.render(request);
    printDiagnostics(result.diagnostics);
    if (!result.ok) {
        return 1;
    }

    if (cli->dumpAst) {
        if (result.document) {
            dumpNode(*result.document, std::cout);
        }
        return 0;
    }

    return writeOutput(cli->output, result.html) ? 0 : 3;
}
