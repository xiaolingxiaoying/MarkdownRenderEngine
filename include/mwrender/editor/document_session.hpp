#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <mwrender/ast.hpp>
#include <mwrender/diagnostics.hpp>
#include <mwrender/engine.hpp>
#include <mwrender/incremental.hpp>
#include <mwrender/options.hpp>

namespace mwrender::editor {

struct DocumentSessionOptions {
    ParseOptions parseOptions;
    RenderOptions renderOptions;
    bool enableIncrementalParsing = true;
    bool fallbackFullReparse = true;
};

struct UpdateResult {
    bool ok = false;
    bool fullReparse = false;
    std::size_t revision = 0;
    std::vector<Diagnostic> diagnostics;
    std::vector<std::string> changedNodeIds;
    std::vector<std::string> insertedNodeIds;
    std::vector<std::string> removedNodeIds;
};

class DocumentSession {
public:
    explicit DocumentSession(DocumentSessionOptions options = {});
    ~DocumentSession();

    DocumentSession(const DocumentSession&) = delete;
    DocumentSession& operator=(const DocumentSession&) = delete;

    void load(std::string markdown);

    [[nodiscard]] const std::string& markdown() const;
    [[nodiscard]] const Node& document() const;
    [[nodiscard]] std::size_t revision() const;

    UpdateResult applyChange(const TextChange& change);

    [[nodiscard]] const Node* findNodeById(std::string_view nodeId) const;
    [[nodiscard]] const Node* findNodeAtOffset(std::size_t sourceOffset) const;

private:
    void buildNodeMap(const Node& node);
    const Node* findNodeAtOffsetImpl(const Node& node, std::size_t offset) const;

    DocumentSessionOptions options_;
    Engine engine_;

    std::string markdown_;
    std::unique_ptr<Node> document_;
    std::size_t revision_ = 0;

    std::unordered_map<std::string, const Node*> nodeMap_;
};

} // namespace mwrender::editor
