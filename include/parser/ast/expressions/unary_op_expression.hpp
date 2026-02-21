#pragma once

#include "expression_node.hpp"
#include "lexer/token.hpp"
#include "parser/ast/unary_op_base.hpp"

#include <memory>

/// @class `UnaryOpExpression`
/// @brief Represents unary operation expressions
class UnaryOpExpression : public UnaryOpBase, public ExpressionNode {
  public:
    explicit UnaryOpExpression(Token operator_token, std::unique_ptr<ExpressionNode> &operand, const bool is_left) :
        UnaryOpBase(operator_token, operand, is_left) {
        ExpressionNode::type = UnaryOpBase::operand->type;
    }

    Variation get_variation() const override {
        return Variation::UNARY_OP;
    }

    std::unique_ptr<ExpressionNode> clone() const override {
        std::unique_ptr<ExpressionNode> operand_clone = operand->clone();
        return std::make_unique<UnaryOpExpression>(operator_token, operand_clone, is_left);
    }

    // deconstructor
    ~UnaryOpExpression() override = default;
    // copy operations - disabled due to unique_ptr member
    UnaryOpExpression(const UnaryOpExpression &) = delete;
    UnaryOpExpression &operator=(const UnaryOpExpression &) = delete;
    // move operations
    UnaryOpExpression(UnaryOpExpression &&) = default;
    UnaryOpExpression &operator=(UnaryOpExpression &&) = default;
};
