#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/ast_node.hpp"

class ErrExprArrayComplexNeedsInitializer : public BaseError {
  public:
    ErrExprArrayComplexNeedsInitializer( //
        const ErrorType error_type,      //
        const Hash &file_hash,           //
        const ASTNode::PosTriple &pos    //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string()
            << "└─ Array initializer for complex values needs explicit initializer value, default value is not sufficient";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Array initializer for complex values needs explicit initializer value, default value is not sufficient";
        return d;
    }
};
