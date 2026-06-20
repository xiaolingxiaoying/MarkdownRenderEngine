#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/node_id_registry.hpp>
#include <functional>
#include <string_view>

namespace mwrender::editor {

namespace {
void computeContentHashes(Node& node, std::string_view markdown) {
    if (node.range.begin.offset <= node.range.end.offset && node.range.end.offset <= markdown.size()) {
        std::string_view content = markdown.substr(node.range.begin.offset, node.range.end.offset - node.range.begin.offset);
        node.contentHash = std::to_string(std::hash<std::string_view>{}(content));
    }
    for (auto& child : node.children) {
        if (child) {
            computeContentHashes(*child, markdown);
        }
    }
}
}

DocumentSession::DocumentSession(DocumentSessionOptions options)
    : options_(std::move(options)), engine_() {}

DocumentSession::~DocumentSession() = default;

void DocumentSession::load(std::string markdown) {
    markdown_ = std::move(markdown);
    revision_++;

    ParseResult result = engine_.parse(markdown_, options_.parseOptions);
    document_ = std::move(result.document);
    
    if (document_) {
        computeContentHashes(*document_, markdown_);
        NodeIdRegistry registry;
        Node emptyDoc;
        emptyDoc.type = NodeType::Document;
        registry.inheritIds(emptyDoc, *document_);
    }

    nodeMap_.clear();
    if (document_) {
        buildNodeMap(*document_);
    }
}

const std::string& DocumentSession::markdown() const {
    return markdown_;
}

const Node& DocumentSession::document() const {
    return *document_;
}

std::size_t DocumentSession::revision() const {
    return revision_;
}

UpdateResult DocumentSession::applyChange(const TextChange& change) {
    UpdateResult result;
    result.revision = revision_;

    if (change.from > change.to || change.to > markdown_.size()) {
        return result; // Invalid change
    }

    // Apply the text change
    markdown_.replace(change.from, change.to - change.from, change.insertedText);
    revision_++;
    result.revision = revision_;

    result.fullReparse = true;

    ParseResult parseResult = engine_.parse(markdown_, options_.parseOptions);
    
    if (document_ && parseResult.document) {
        computeContentHashes(*parseResult.document, markdown_);
        NodeIdRegistry registry;
        registry.inheritIds(*document_, *parseResult.document);

        std::unordered_map<std::string, const Node*> newMap;
        auto collect = [](const Node& n, std::unordered_map<std::string, const Node*>& m, auto& ref) -> void {
            if (!n.id.empty()) m[n.id] = &n;
            for (const auto& c : n.children) {
                if (c) ref(*c, m, ref);
            }
        };
        collect(*parseResult.document, newMap, collect);
        
        for (const auto& pair : newMap) {
            auto it = nodeMap_.find(pair.first);
            if (it == nodeMap_.end()) {
                result.insertedNodeIds.push_back(pair.first);
            } else {
                if (it->second->contentHash != pair.second->contentHash) {
                    result.changedNodeIds.push_back(pair.first);
                }
            }
        }
        for (const auto& pair : nodeMap_) {
            if (newMap.find(pair.first) == newMap.end()) {
                result.removedNodeIds.push_back(pair.first);
            }
        }
    }

    document_ = std::move(parseResult.document);
    result.diagnostics = std::move(parseResult.diagnostics);
    
    // Clear and rebuild map
    nodeMap_.clear();
    if (document_) {
        buildNodeMap(*document_);
    }

    result.ok = (document_ != nullptr);
    return result;
}

const Node* DocumentSession::findNodeById(std::string_view nodeId) const {
    auto it = nodeMap_.find(std::string(nodeId));
    if (it != nodeMap_.end()) {
        return it->second;
    }
    return nullptr;
}

const Node* DocumentSession::findNodeAtOffset(std::size_t sourceOffset) const {
    if (!document_) {
        return nullptr;
    }
    return findNodeAtOffsetImpl(*document_, sourceOffset);
}

void DocumentSession::buildNodeMap(const Node& node) {
    if (!node.id.empty()) {
        nodeMap_[node.id] = &node;
    }
    for (const auto& child : node.children) {
        if (child) {
            buildNodeMap(*child);
        }
    }
}

const Node* DocumentSession::findNodeAtOffsetImpl(const Node& node, std::size_t offset) const {
    // If the offset is outside this node's range, it's not here
    if (offset < node.range.begin.offset || offset > node.range.end.offset) {
        return nullptr;
    }

    // Try children first (they are more specific)
    for (const auto& child : node.children) {
        if (child) {
            if (const Node* found = findNodeAtOffsetImpl(*child, offset)) {
                return found;
            }
        }
    }

    // If no child contains it, but this node does, return this node
    return &node;
}

} // namespace mwrender::editor
