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

/// @function `diagnostic_to_json`
/// @brief Converts a diagnostic to JSON format
///
/// @param `diagnostic` The diagnostic to convert
/// @return `std::string` The converte diagnostic JSON string representation
static std::string diagnostic_to_json(const Diagnostic &diagnostic) {
    const int line = std::get<0>(diagnostic.range);
    const int column = std::get<1>(diagnostic.range);
    const int length = std::get<2>(diagnostic.range);
    std::stringstream json;
    json << "{\n";
    json << "        \"range\": {\n";
    json << "          \"start\": {\"line\": " << line << ", \"character\": " << column << "},\n";
    json << "          \"end\": {\"line\": " << line << ", \"character\": " << column + length << "}\n";
    json << "        },\n";
    json << "        \"severity\": " << static_cast<int>(diagnostic.level) << ",\n";
    json << "        \"message\": \"" << diagnostic.message << "\",\n";
    json << "        \"source\": \"" << diagnostic.source << "\"\n";
    json << "      }";
    return json.str();
}

/// @function `diagnostics_to_json_array`
/// @brief Converts a vector of diagnostics to a JSON array
///
/// @return `std::string` The converted diagnostics JSON array
static std::string diagnostics_to_json_array() {
    std::stringstream json;
    json << "[\n";

    for (size_t i = 0; i < diagnostics.size(); ++i) {
        json << "      " << diagnostic_to_json(diagnostics[i]);
        if (i < diagnostics.size() - 1) {
            json << ",";
        }
        json << "\n";
    }

    json << "    ]";
    return json.str();
}

#endif
