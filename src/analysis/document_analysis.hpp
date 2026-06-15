#pragma once

#include <vector>

#include <mwrender/ast.hpp>
#include <mwrender/outline.hpp>
#include <mwrender/stats.hpp>

namespace mwrender::detail {

[[nodiscard]] std::vector<OutlineItem> buildOutline(const Node& document);
[[nodiscard]] WordStats buildWordStats(const Node& document);

} // namespace mwrender::detail

