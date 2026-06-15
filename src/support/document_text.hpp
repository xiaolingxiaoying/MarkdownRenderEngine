#pragma once

#include <string>
#include <string_view>

#include <mwrender/ast.hpp>

namespace mwrender::detail {

[[nodiscard]] std::string plainText(const Node& node);
[[nodiscard]] std::string slugBase(std::string_view text);

} // namespace mwrender::detail

