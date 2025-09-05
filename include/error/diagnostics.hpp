#pragma once

#include <string>

/// @enum `DiagnosticLevel`
/// @brief LSP diagnostic severity levels
enum class DiagnosticLevel : int {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4,
};

/// @alias `DiagnosticRange`
/// @brief A range is a given LINE, COLUMN, LENGTH
///
/// @note Multi-line diagnostics are not supported yet, so this is plenty
using DiagnosticRange = std::tuple<int, int, int>;

/// @struct `Diagnostic`
/// @brief Represents a diagnostic message (error, warning, etc.)
struct Diagnostic {
    DiagnosticRange range;
    DiagnosticLevel level;
    std::string message;
    std::string source;

    Diagnostic(DiagnosticRange range, DiagnosticLevel level, const std::string &message, const std::string &source) :
        range(range),
        level(level),
        message(std::move(message)),
        source(std::move(source)) {}
};

#ifdef FLINT_LSP

#include <sstream>
#include <vector>

/// @var `diagnostics`
/// @brief A list of all collected diagnostics
///
/// @note Implementation found in the `lsp_server.cpp` file
extern std::vector<Diagnostic> diagnostics;

#endif
