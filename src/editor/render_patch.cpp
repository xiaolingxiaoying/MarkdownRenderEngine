#include <mwrender/editor/render_patch.hpp>
#include <mwrender/query.hpp>
#include <cstdio>

namespace mwrender::editor {

RenderPatchGenerator::RenderPatchGenerator(const EditorProjection& projection)
    : projection_(projection) {}

namespace {
// Helper to find parent and index
bool findParentAndIndex(const Node& current, const std::string& targetId, std::string& parentId, std::size_t& index) {
    for (std::size_t i = 0; i < current.children.size(); ++i) {
        if (current.children[i] && current.children[i]->id == targetId) {
            parentId = current.id;
            index = i;
            return true;
        }
    }
    for (const auto& child : current.children) {
        if (child && findParentAndIndex(*child, targetId, parentId, index)) {
            return true;
        }
    }
    return false;
}



// Helper to escape JSON strings
std::string escapeJsonString(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}
} // namespace

std::string RenderPatch::toJson() const {
    std::string json = "{";
    json += "\"revision\":" + std::to_string(revision) + ",";
    json += "\"fullReload\":" + std::string(fullReload ? "true" : "false") + ",";
    
    // removedNodeIds
    json += "\"removedNodeIds\":[";
    for (std::size_t i = 0; i < removedNodeIds.size(); ++i) {
        if (i > 0) json += ",";
        json += "\"" + escapeJsonString(removedNodeIds[i]) + "\"";
    }
    json += "],";

    // helper for NodeHtmlPatch
    auto serializeNodePatch = [](const NodeHtmlPatch& p) -> std::string {
        std::string modeStr;
        switch (p.mode) {
            case ProjectionMode::Editable: modeStr = "editable"; break;
            case ProjectionMode::SourceEditable: modeStr = "source-editable"; break;
            case ProjectionMode::Atomic: modeStr = "atomic"; break;
            case ProjectionMode::Hidden: modeStr = "hidden"; break;
            case ProjectionMode::Unsupported: modeStr = "unsupported"; break;
        }
        std::string s = "{";
        s += "\"nodeId\":\"" + escapeJsonString(p.nodeId) + "\",";
        s += "\"html\":\"" + escapeJsonString(p.html) + "\",";
        s += "\"mode\":\"" + modeStr + "\",";
        s += "\"sourceStart\":" + std::to_string(p.sourceRange.begin.offset) + ",";
        s += "\"sourceEnd\":" + std::to_string(p.sourceRange.end.offset) + ",";
        s += "\"contentStart\":" + std::to_string(p.contentRange.begin.offset) + ",";
        s += "\"contentEnd\":" + std::to_string(p.contentRange.end.offset) + ",";
        s += "\"parentId\":\"" + escapeJsonString(p.parentId) + "\",";
        s += "\"insertIndex\":" + std::to_string(p.insertIndex);
        s += "}";
        return s;
    };

    // changedNodes
    json += "\"changedNodes\":[";
    for (std::size_t i = 0; i < changedNodes.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeNodePatch(changedNodes[i]);
    }
    json += "],";

    // insertedNodes
    json += "\"insertedNodes\":[";
    for (std::size_t i = 0; i < insertedNodes.size(); ++i) {
        if (i > 0) json += ",";
        json += serializeNodePatch(insertedNodes[i]);
    }
    json += "],";

    // selection
    if (selection.has_value()) {
        auto serializePos = [](const SourcePositionEx& pos) -> std::string {
            std::string aff = (pos.affinity == Affinity::Before) ? "before" : "after";
            return "{\"offset\":" + std::to_string(pos.offset) + 
                   ",\"affinity\":\"" + aff + 
                   "\",\"contextNodeId\":\"" + escapeJsonString(pos.contextNodeId) + "\"}";
        };
        json += "\"selection\":{";
        json += "\"anchor\":" + serializePos(selection->anchor) + ",";
        json += "\"focus\":" + serializePos(selection->focus);
        json += "},";
    } else {
        json += "\"selection\":{},";
    }

    // diagnostics
    json += "\"diagnostics\":[";
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i > 0) json += ",";
        const auto& d = diagnostics[i];
        std::string sev;
        switch (d.severity) {
            case DiagnosticSeverity::Info: sev = "info"; break;
            case DiagnosticSeverity::Warning: sev = "warning"; break;
            case DiagnosticSeverity::Error: sev = "error"; break;
        }
        json += "{";
        json += "\"severity\":\"" + sev + "\",";
        json += "\"code\":\"" + escapeJsonString(d.code) + "\",";
        json += "\"message\":\"" + escapeJsonString(d.message) + "\"";
        if (d.range.has_value()) {
            json += ",\"range\":{";
            json += "\"start\":" + std::to_string(d.range->begin.offset) + ",";
            json += "\"end\":" + std::to_string(d.range->end.offset);
            json += "}";
        }
        json += "}";
    }
    json += "]";

    json += "}";
    return json;
}

RenderPatch RenderPatchGenerator::generatePatch(
    const Node& document,
    const UpdateResult& parseResult,
    const std::optional<Selection>& newSelection
) const {
    RenderPatch patch;
    patch.revision = parseResult.revision;
    patch.fullReload = parseResult.fullReparse;
    patch.selection = newSelection;
    patch.removedNodeIds = parseResult.removedNodeIds;
    patch.diagnostics = parseResult.diagnostics;

    for (const auto& id : parseResult.changedNodeIds) {
        if (const auto* node = mwrender::findNodeById(document, id)) {
            if (node->type == NodeType::Paragraph ||
                node->type == NodeType::Heading ||
                node->type == NodeType::BlockQuote ||
                node->type == NodeType::List ||
                node->type == NodeType::ListItem ||
                node->type == NodeType::CodeBlock ||
                node->type == NodeType::ThematicBreak ||
                node->type == NodeType::HtmlBlock ||
                node->type == NodeType::Table ||
                node->type == NodeType::MathBlock ||
                node->type == NodeType::FootnoteDef) {
                
                NodeHtmlPatch snippet;
                snippet.nodeId = id;
                snippet.html = projection_.projectNode(document, id);
                snippet.mode = EditorProjection::classifyNode(*node);
                snippet.sourceRange = node->range;
                snippet.contentRange = node->contentRange;
                
                findParentAndIndex(document, id, snippet.parentId, snippet.insertIndex);
                
                patch.changedNodes.push_back(std::move(snippet));
            }
        }
    }

    for (const auto& id : parseResult.insertedNodeIds) {
        if (const auto* node = mwrender::findNodeById(document, id)) {
            if (node->type == NodeType::Paragraph ||
                node->type == NodeType::Heading ||
                node->type == NodeType::BlockQuote ||
                node->type == NodeType::List ||
                node->type == NodeType::ListItem ||
                node->type == NodeType::CodeBlock ||
                node->type == NodeType::ThematicBreak ||
                node->type == NodeType::HtmlBlock ||
                node->type == NodeType::Table ||
                node->type == NodeType::MathBlock ||
                node->type == NodeType::FootnoteDef) {
                
                NodeHtmlPatch snippet;
                snippet.nodeId = id;
                snippet.html = projection_.projectNode(document, id);
                snippet.mode = EditorProjection::classifyNode(*node);
                snippet.sourceRange = node->range;
                snippet.contentRange = node->contentRange;
                
                findParentAndIndex(document, id, snippet.parentId, snippet.insertIndex);
                
                patch.insertedNodes.push_back(std::move(snippet));
            }
        }
    }

    return patch;
}

} // namespace mwrender::editor
