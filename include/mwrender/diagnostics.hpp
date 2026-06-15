#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <mwrender/source.hpp>

namespace mwrender {

enum class DiagnosticSeverity : std::uint8_t {
    Info,
    Warning,
    Error
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    std::string code;
    std::string message;
    std::optional<SourceRange> range;
};

} // namespace mwrender

