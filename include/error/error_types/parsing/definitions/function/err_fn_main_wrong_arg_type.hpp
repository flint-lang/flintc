#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrFnMainWrongArgType : public BaseError {
  public:
    ErrFnMainWrongArgType(                    //
        const ErrorType error_type,           //
        const Hash &file_hash,                //
        const token_slice &tokens,            //
        const std::shared_ptr<Type> &arg_type //
        ) :
        BaseError(error_type, file_hash, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column),
        arg_type(arg_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Wrong argument type for the main function: " << YELLOW << arg_type->to_string() << DEFAULT
            << "\n";
        oss << "└─ The main function can only be one of the following variations:\n";
        oss << "    ├─ " << CYAN << "main()" << DEFAULT << "\n";
        oss << "    └─ " << CYAN << "main(str[] args)" << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Wrong argument type '" + arg_type->to_string() + "'for the main function";
        return d;
    }

  private:
    std::shared_ptr<Type> arg_type;
};
