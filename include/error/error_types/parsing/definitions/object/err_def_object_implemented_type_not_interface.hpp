#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/ast_node.hpp"

class ErrDefObjectImplementedTypeNotInterface : public BaseError {
  public:
    ErrDefObjectImplementedTypeNotInterface( //
        const ErrorType error_type,          //
        const Hash &file_file,               //
        const ASTNode::PosTriple &pos        //
        ) :
        BaseError(error_type, file_file, pos.line, pos.column, pos.length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Only interface types are allowed to be implemented by objects";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Only interface types are allowed to be implemented by objects";
        return d;
    }
};
