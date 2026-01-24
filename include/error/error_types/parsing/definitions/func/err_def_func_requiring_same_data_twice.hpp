#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefFuncRequiringSameDataTwice : public BaseError {
  public:
    ErrDefFuncRequiringSameDataTwice(     //
        const ErrorType error_type,       //
        const Hash &file_file,            //
        const token_slice &tokens,        //
        const std::shared_ptr<Type> &type //
        ) :
        BaseError(error_type, file_file, tokens.first->line, tokens.first->column, tokens.second->column - tokens.first->column),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Requiring the same type twice: " << YELLOW << type->to_string() << DEFAULT << "\n";
        oss << "└─ Each required data type needs to be unique";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Requiring the same type twice: '" + type->to_string() + "'";
        return d;
    }

  private:
    const std::shared_ptr<Type> &type;
};
