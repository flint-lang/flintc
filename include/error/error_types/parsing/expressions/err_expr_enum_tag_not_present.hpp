#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExprEnumTagNotPresent : public BaseError {
  public:
    ErrExprEnumTagNotPresent(             //
        const ErrorType error_type,       //
        const Hash &file_hash,            //
        const token_slice &tokens,        //
        const std::string &tag,           //
        const std::shared_ptr<Type> &type //
        ) :
        BaseError(error_type, file_hash, tokens),
        tag(tag),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Enum tag '" << YELLOW << tag << DEFAULT << "' not present in type '" << YELLOW
            << type->to_string() << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Enum tag '" + tag + "' not present in type '" + type->to_string() + "'";
        return d;
    }

  private:
    std::string tag;
    std::shared_ptr<Type> type;
};
