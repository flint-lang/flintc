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
        BaseError(error_type, wrong_fn->file_hash, wrong_fn->line, wrong_fn->column, wrong_fn->length),
        wrong_fn(wrong_fn),
        first_defined(first_defined) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Defined extern function '" << YELLOW << wrong_fn->name << DEFAULT << "' twice\n";
        const auto &file_path = first_defined->file_hash.path;
        oss << "└─ It was first defined at " << GREEN << file_path.filename().string() << ":" << first_defined->line << ":"
            << first_defined->column << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        const auto &file_path = first_defined->file_hash.path;
        d.message = "Defined extern function '" + wrong_fn->name + "' twice, first defined at " + file_path.filename().string() + ":" +
            std::to_string(first_defined->line) + ":" + std::to_string(first_defined->column);
        return d;
    }

  private:
    const FunctionNode *wrong_fn;
    const FunctionNode *first_defined;
};
