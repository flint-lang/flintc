#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrCallOfExternFunctionAtComptime : public BaseError {
  public:
    ErrCallOfExternFunctionAtComptime(   //
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
        oss << BaseError::to_string() << "├─ Extern function '" << YELLOW << function_name << DEFAULT << "' cannot be called at comptime\n";
        oss << "└─ Extern functions are implemented outside of Flint and have no body the compiler could evaluate. Remove the '" << BLUE
            << "@" << DEFAULT << "' to call it at runtime";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Comptime call of extern function '" + function_name + "'";
        return d;
    }

  private:
    std::string function_name;
};
