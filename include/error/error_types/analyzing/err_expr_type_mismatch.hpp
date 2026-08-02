#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

/// @class `ErrExprTypeMismatch`
/// @brief Represents type mismatch errors
class ErrExprTypeMismatch : public BaseError {
  public:
    ErrExprTypeMismatch(                       //
        const ErrorType error_type,            //
        const Hash &file_hash,                 //
        const ASTNode::PosTriple &pos,         //
        const std::shared_ptr<Type> &expected, //
        const std::shared_ptr<Type> &type      //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        expected(expected),
        type(type) {}

    ErrExprTypeMismatch(                       //
        const ErrorType error_type,            //
        const Hash &file_hash,                 //
        const unsigned int line,               //
        const unsigned int column,             //
        const unsigned int length,             //
        const std::shared_ptr<Type> &expected, //
        const std::shared_ptr<Type> &type      //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        expected(expected),
        type(type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "└─ Type mismatch of expression\n";
        oss << "    ├─ Expected: " << YELLOW << expected->to_string() << DEFAULT << "\n";
        oss << "    └─ But got:  " << YELLOW << type->to_string() << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Expression type mismatch, expected '" + expected->to_string() + "' but got '" + type->to_string() + "'";
        return d;
    }

  private:
    /// @var `expected`
    /// @brief The expected type
    std::shared_ptr<Type> expected;

    /// @var `type`
    /// @brief The actual present type
    std::shared_ptr<Type> type;
};
