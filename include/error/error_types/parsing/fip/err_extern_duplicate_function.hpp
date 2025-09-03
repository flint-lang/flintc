#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

class ErrExternDuplicateFunction : public BaseError {
  public:
    ErrExternDuplicateFunction(           //
        const ErrorType error_type,       //
        const FunctionNode *wrong_fn,     //
        const FunctionNode *first_defined //
        ) :
        BaseError(error_type, wrong_fn->file_name, wrong_fn->line, wrong_fn->column, wrong_fn->length),
        wrong_fn(wrong_fn),
        first_defined(first_defined) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Defined extern function '" << YELLOW << wrong_fn->name << DEFAULT << "' twice\n";
        oss << "└─ It was first defined at " << GREEN << first_defined->file_name << ":" << first_defined->line << ":"
            << first_defined->column << DEFAULT;
        return oss.str();
    }

  private:
    const FunctionNode *wrong_fn;
    const FunctionNode *first_defined;
};
