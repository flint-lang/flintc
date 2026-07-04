#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/ast_node.hpp"

class ErrEmptyStoredFixedArray : public BaseError {
  public:
    ErrEmptyStoredFixedArray(const ErrorType error_type, const Hash &file_hash, const ASTNode::PosTriple &pos) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Stored fixed arrays cannot be empty";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Stored fixed arrays cannot be empty";
        return d;
    }
};
