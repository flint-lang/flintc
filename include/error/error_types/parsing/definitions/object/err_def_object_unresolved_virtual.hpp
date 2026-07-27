#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"

class ErrDefObjectUnresolvedVirtual : public BaseError {
  public:
    ErrDefObjectUnresolvedVirtual(  //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const int line,             //
        const int column,           //
        const int length,           //
        const FunctionNode *virt_fn //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        virt_fn(virt_fn) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Virtual function '" << YELLOW << virt_fn->name << DEFAULT << "' not implemented in object";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Virtual function '" + virt_fn->name + "' unresolved";
        return d;
    }

  private:
    const FunctionNode *virt_fn;
};
