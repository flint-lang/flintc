#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/type/type.hpp"

class ErrExprCallAmbiguous : public BaseError {
  public:
    ErrExprCallAmbiguous(                                               //
        const ErrorType error_type,                                     //
        const Hash &file_hash,                                          //
        const token_slice &tokens,                                      //
        const std::vector<std::pair<FunctionNode *, size_t>> &functions //
        ) :
        BaseError(error_type, file_hash, tokens),
        functions(functions) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Ambiguous function call, could be any of these functions:\n";
        for (size_t i = 0; i < functions.size(); i++) {
            const auto &[function, implicit_param_count] = functions.at(i);
            const bool is_last = i + 1 == functions.size();
            if (is_last) {
                oss << "    └─ ";
            } else {
                oss << "    ├─ ";
            }
            oss << function->get_signature_string(implicit_param_count);
            if (!is_last) {
                oss << "\n";
            }
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Instance calls are only allowed on variables of type 'entity' or 'func'";
        return d;
    }

  private:
    std::vector<std::pair<FunctionNode *, size_t>> functions;
};
