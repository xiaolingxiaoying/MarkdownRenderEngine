#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <mwrender/source.hpp>

namespace mwrender {

class SourceMap {
public:
    void build(const std::string& source);

    SourcePosition visualToSource(std::size_t visualOffset) const;
    std::size_t sourceToVisual(std::size_t sourceOffset) const;

    std::size_t sourceSize() const { return sourceOffsets_.size(); }

private:
    std::vector<std::size_t> sourceOffsets_;
    std::vector<std::size_t> visualOffsets_;
};

} // namespace mwrender
