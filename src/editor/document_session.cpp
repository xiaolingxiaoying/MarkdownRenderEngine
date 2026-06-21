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
    NodeType phase2NodeType = NodeType::Document;
    if (options_.enableIncrementalParsing && document_) {
        const BlockEntry* oldBlock = blockIndex_.blockAtOffset(change.from);
        if (oldBlock) {
            phase2NodeId = oldBlock->nodeId;
            phase2NodeType = oldBlock->type;
        }
    }

    // Determine affected range before editing
    SourceRange affected;
    if (document_) {
        affected = blockIndex_.affectedRangeForChange(change);
        result.affectedRange = affected;
    }

    // Check complex block fallback reasons
    std::string fallbackReason;
    bool forceFullReparse = false;
    if (options_.enableIncrementalParsing && document_ && !phase2NodeId.empty()) {
        if (phase2NodeType == NodeType::List || phase2NodeType == NodeType::ListItem) {
            forceFullReparse = true;
            fallbackReason = "list incremental parsing not supported yet";
        } else if (phase2NodeType == NodeType::BlockQuote) {
            forceFullReparse = true;
            fallbackReason = "blockquote incremental parsing not supported yet";
        } else if (phase2NodeType == NodeType::Table ||
                   phase2NodeType == NodeType::TableHead ||
                   phase2NodeType == NodeType::TableBody ||
                   phase2NodeType == NodeType::TableRow ||
                   phase2NodeType == NodeType::TableCell) {
            forceFullReparse = true;
            fallbackReason = "table incremental parsing not supported yet";
        } else if (phase2NodeType == NodeType::HtmlBlock) {
            forceFullReparse = true;
            fallbackReason = "htmlblock incremental parsing not supported yet";
        } else if (phase2NodeType == NodeType::FootnoteDef) {
            forceFullReparse = true;
            fallbackReason = "footnotedef incremental parsing not supported yet";
        } else if (phase2NodeType == NodeType::FrontMatter) {
            forceFullReparse = true;
            fallbackReason = "frontmatter incremental parsing not supported yet";
        } else if (phase2NodeType == NodeType::Toc) {
            forceFullReparse = true;
            fallbackReason = "toc incremental parsing not supported yet";
        }

        // Also check if any ancestor block is a complex container
        if (!forceFullReparse) {
            std::vector<NodeType> path;
            auto dfs = [&](const Node& n, auto& ref) -> bool {
                if (n.id == phase2NodeId) {
                    for (auto t : path) {
                        if (t == NodeType::List || t == NodeType::ListItem) {
                            fallbackReason = "list incremental parsing not supported yet";
                            return true;
                        }
                        if (t == NodeType::BlockQuote) {
                            fallbackReason = "blockquote incremental parsing not supported yet";
                            return true;
                        }
                        if (t == NodeType::Table || t == NodeType::TableRow || t == NodeType::TableCell) {
                            fallbackReason = "table incremental parsing not supported yet";
                            return true;
                        }
                        if (t == NodeType::FootnoteDef) {
                            fallbackReason = "footnotedef incremental parsing not supported yet";
                            return true;
                        }
                    }
                    return false;
                }
                path.push_back(n.type);
                for (const auto& child : n.children) {
                    if (child && ref(*child, ref)) return true;
                }
                path.pop_back();
                return false;
            };
            if (dfs(*document_, dfs)) {
                forceFullReparse = true;
            }
        }
    }

    // Apply the text change
    markdown_.replace(change.from, change.to - change.from, change.insertedText);
    revision_++;
    result.revision = revision_;

    bool walkedFastPath = false;

    // Fast path: parse only the affected block
    if (options_.enableIncrementalParsing && document_ && !phase2NodeId.empty() && !forceFullReparse &&
        (phase2NodeType == NodeType::Paragraph || phase2NodeType == NodeType::Heading ||
         phase2NodeType == NodeType::ThematicBreak || phase2NodeType == NodeType::CodeBlock ||
         phase2NodeType == NodeType::MathBlock)) {
        
        const auto& allBlocks = blockIndex_.blocks();
        bool isFullDoc = (!allBlocks.empty() &&
                          affected.begin.offset == allBlocks[0].range.begin.offset &&
                          affected.end.offset == allBlocks[0].range.end.offset);

        if (!isFullDoc) {
            std::size_t delta = change.insertedText.size() - (change.to - change.from);
            std::string fragment = markdown_.substr(
                affected.begin.offset,
                (affected.end.offset + delta) - affected.begin.offset);

            auto fragResult = engine_.parseFragment(fragment, phase2NodeType,
                                                     options_.parseOptions);
            if (fragResult.ok && fragResult.document &&
                !fragResult.document->children.empty()) {

                auto newBlock = std::move(fragResult.document->children[0]);
                if (phase2NodeType == NodeType::MathBlock && newBlock->type == NodeType::Paragraph) {
                    if (!newBlock->children.empty() && newBlock->children[0]->type == NodeType::MathBlock) {
                        newBlock = std::move(newBlock->children[0]);
                    }
                }
                if (newBlock->type == phase2NodeType) {
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
                                // Subtree diffing before replacement
                                std::unordered_map<std::string, std::string> oldSubtreeHashes;
                                auto collect = [](const Node& node, std::unordered_map<std::string, std::string>& m, auto& ref) -> void {
                                    if (!node.id.empty()) m[node.id] = node.contentHash;
                                    for (const auto& child : node.children) {
                                        if (child) ref(*child, m, ref);
                                    }
                                };
                                collect(*c, oldSubtreeHashes, collect);

                                NodeIdRegistry registry;
                                registry.inheritIds(*c, *newBlock);
                                computeContentHashes(*newBlock, markdown_);

                                std::unordered_map<std::string, std::string> newSubtreeHashes;
                                collect(*newBlock, newSubtreeHashes, collect);

                                // Compute differences inside the subtree
                                for (const auto& pair : newSubtreeHashes) {
                                    auto it = oldSubtreeHashes.find(pair.first);
                                    if (it == oldSubtreeHashes.end()) {
                                        result.insertedNodeIds.push_back(pair.first);
                                    } else {
                                        if (it->second != pair.second) {
                                            result.changedNodeIds.push_back(pair.first);
                                        }
                                    }
                                }
                                for (const auto& pair : oldSubtreeHashes) {
                                    if (newSubtreeHashes.find(pair.first) == newSubtreeHashes.end()) {
                                        result.removedNodeIds.push_back(pair.first);
                                    }
                                }

                                for (const auto& pair : oldSubtreeHashes) {
                                    nodeMap_.erase(pair.first);
                                }
                                c = std::move(newBlock);
                                std::function<void(const Node&)> updateNodeMapForSubtree = [&](const Node& node) {
                                    if (!node.id.empty()) {
                                        nodeMap_[node.id] = &node;
                                    }
                                    for (const auto& child : node.children) {
                                        if (child) updateNodeMapForSubtree(*child);
                                    }
                                };
                                updateNodeMapForSubtree(*c);
                                blockIndex_.rebuild(*document_);

                                result.revision = revision_;
                                result.fullReparse = false;
                                result.partialReparse = true;
                                result.ok = true;
                                walkedFastPath = true;
                                return true;
                            }
                            if (c && replacer(*c)) return true;
                        }
                        return false;
                    };

                    if (replacer(*document_)) {
                        return result;
                    }
                } else {
                    fallbackReason = "parsed node type changed during incremental edit";
                }
            } else {
                fallbackReason = "failed to parse fragment incrementally";
            }
        } else {
            fallbackReason = "affected range covers entire document";
        }
    }

    // Fallback: full re-parse
    result.fullReparse = true;
    result.fallbackReason = fallbackReason.empty() ? "unsupported or complex edit" : fallbackReason;

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
        result.ok = false;
        result.fallbackReason = std::move(cmdResult.fallbackReason);
        result.diagnostics = std::move(cmdResult.diagnostics);
        result.fullReparse = cmdResult.fullReparse;
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
