#include <mwrender/editor/document_session.hpp>
#include <mwrender/editor/edit_command.hpp>
#include <mwrender/editor/node_id_registry.hpp>
#include <mwrender/editor/selection_map.hpp>
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
        blockIndex_.rebuild(*document_);
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

    // Save old node info before text change for Phase 2 matching
    std::string phase2NodeId;
    if (options_.enableIncrementalParsing && document_) {
        const Node* oldNode = findNodeAtOffset(change.from);
        if (oldNode) phase2NodeId = oldNode->id;
    }

    // Apply the text change
    markdown_.replace(change.from, change.to - change.from, change.insertedText);
    revision_++;
    result.revision = revision_;

    // Phase 2: Fast path for simple block edits — parse only the affected block
    if (options_.enableIncrementalParsing && document_ && !phase2NodeId.empty()) {
        blockIndex_.rebuild(*document_);
        SourceRange affected = blockIndex_.affectedRangeForChange(change);
        const auto& allBlocks = blockIndex_.blocks();
        bool isFullDoc = (!allBlocks.empty() &&
                          affected.begin.offset == allBlocks[0].range.begin.offset &&
                          affected.end.offset == allBlocks[0].range.end.offset);

        if (!isFullDoc) {
            std::string fragment = markdown_.substr(
                affected.begin.offset,
                affected.end.offset - affected.begin.offset);

            auto fragResult = engine_.parseFragment(fragment, NodeType::Paragraph,
                                                     options_.parseOptions);
            if (fragResult.ok && fragResult.document &&
                !fragResult.document->children.empty()) {

                auto newBlock = std::move(fragResult.document->children[0]);
                // Adjust fragment-relative offsets to document-absolute offsets
                auto shiftRanges = [&](Node& n, auto& self) -> void {
                    n.range.begin.offset = affected.begin.offset + n.range.begin.offset;
                    n.range.end.offset = affected.begin.offset + n.range.end.offset;
                    if (n.contentRange.begin.offset != 0 || n.contentRange.end.offset != 0) {
                        n.contentRange.begin.offset = affected.begin.offset + n.contentRange.begin.offset;
                        n.contentRange.end.offset = affected.begin.offset + n.contentRange.end.offset;
                    }
                    for (auto& mr : n.markerRanges) {
                        mr.begin.offset = affected.begin.offset + mr.begin.offset;
                        mr.end.offset = affected.begin.offset + mr.end.offset;
                    }
                    for (auto& c : n.children) {
                        if (c) self(*c, self);
                    }
                };
                shiftRanges(*newBlock, shiftRanges);

                // Find the old node by ID in the mutable tree and replace
                std::function<bool(Node&)> replacer = [&](Node& n) -> bool {
                    for (auto& c : n.children) {
                        if (c && c->id == phase2NodeId) {
                            NodeIdRegistry registry;
                            registry.inheritIds(*c, *newBlock);
                            computeContentHashes(*newBlock, markdown_);
                            std::string inheritedId = newBlock->id;
                            c = std::move(newBlock);
                            nodeMap_.clear();
                            buildNodeMap(*document_);
                            blockIndex_.rebuild(*document_);

                            result.changedNodeIds.push_back(inheritedId);
                            result.revision = revision_;
                            result.fullReparse = false;
                            result.ok = true;
                            return true;
                        }
                        if (c && replacer(*c)) return true;
                    }
                    return false;
                };

                if (replacer(*document_)) {
                    return result;
                }
            }
        }
    }

    // Fallback: full re-parse (existing Phase 1 logic)
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
        blockIndex_.rebuild(*document_);

        // Phase 1 incremental: filter results to only affected range
        SourceRange affected = blockIndex_.affectedRangeForChange(change);
        const auto& allBlocks = blockIndex_.blocks();
        bool isFullDoc = (!allBlocks.empty() &&
                          affected.begin.offset == allBlocks[0].range.begin.offset &&
                          affected.end.offset == allBlocks[0].range.end.offset);

        if (!isFullDoc) {
            auto filterInRange = [&](const std::vector<std::string>& ids) {
                std::vector<std::string> filtered;
                for (const auto& id : ids) {
                    const BlockEntry* be = blockIndex_.blockById(id);
                    if (be && be->range.begin.offset >= affected.begin.offset
                          && be->range.end.offset <= affected.end.offset) {
                        filtered.push_back(id);
                    }
                }
                return filtered;
            };

            result.changedNodeIds = filterInRange(result.changedNodeIds);
            result.insertedNodeIds = filterInRange(result.insertedNodeIds);
            result.removedNodeIds = filterInRange(result.removedNodeIds);

            if (result.changedNodeIds.size() <= 1 &&
                result.insertedNodeIds.empty() &&
                result.removedNodeIds.empty()) {
                result.fullReparse = false;
            } else {
                result.fallbackReason = "cross-block change requires full re-parse";
            }
        } else {
            result.fallbackReason = "affected range covers entire document";
        }
    }

    result.ok = (document_ != nullptr);
    return result;
}

UpdateResult DocumentSession::applyCommand(const EditCommand& command) {
    UpdateResult result;
    result.revision = revision_;

    SelectionMap selMap(*this);
    EditCommandExecutor executor(*this, selMap);
    auto cmdResult = executor.execute(command);
    if (!cmdResult.ok) {
        return result;
    }

    const auto& oldMd = markdown_;
    const auto& newMd = cmdResult.markdown;

    std::size_t diffStart = 0;
    while (diffStart < oldMd.size() && diffStart < newMd.size() &&
           oldMd[diffStart] == newMd[diffStart]) {
        diffStart++;
    }

    std::size_t oldEnd = oldMd.size();
    std::size_t newEnd = newMd.size();
    while (oldEnd > diffStart && newEnd > diffStart &&
           oldMd[oldEnd - 1] == newMd[newEnd - 1]) {
        oldEnd--;
        newEnd--;
    }

    TextChange change;
    change.from = diffStart;
    change.to = oldEnd;
    change.insertedText = newMd.substr(diffStart, newEnd - diffStart);

    return applyChange(change);
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
