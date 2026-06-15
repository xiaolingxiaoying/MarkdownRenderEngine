#pragma once

#include <string>
#include <vector>

namespace mwrender {

struct OutlineItem {
    int level = 0;
    std::string title;
    std::string id;
    int line = 0;
    std::vector<OutlineItem> children;
};

} // namespace mwrender

