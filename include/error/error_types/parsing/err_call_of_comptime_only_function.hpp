#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrCallOfComptimeOnlyFunction : public BaseError {
  public:
    ErrCallOfComptimeOnlyFunction(       //
        const ErrorType error_type,      //
        const Hash &file_hash,           //
        const PosTriple &pos,            //
        const std::string &function_name //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        function_name(function_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Comptime-only function '" << YELLOW << function_name << DEFAULT
            << "' cannot be called at runtime\n";
        oss << "└─ Comptime-only functions must be called at comptime by prefixing the call with '" << BLUE << "@" << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Call of comptime-only function '" + function_name + "' at runtime";
        return d;
    }

  private:
    std::string function_name;
};
