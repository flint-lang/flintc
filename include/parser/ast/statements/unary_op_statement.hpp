#pragma once

#include "parser/ast/unary_op_base.hpp"
#include "statement_node.hpp"

/// class `UnaryOpStatement`
/// @brief Represents unary operation statements
class UnaryOpStatement : public StatementNode, public UnaryOpBase {
  public:
    UnaryOpStatement(                             //
        const Hash &hash,                         //
        const token_slice &tokens,                //
        Token operator_token,                     //
        std::unique_ptr<ExpressionNode> &operand, //
        const bool is_left                        //
        ) :
        StatementNode(hash, tokens),
        UnaryOpBase(operator_token, operand, is_left) {}

    Variation get_variation() const override {
        return Variation::UNARY_OP;
    }

    // deconstructor
    ~UnaryOpStatement() override = default;
    // copy operations - disabled due to unique_ptr member
    UnaryOpStatement(const UnaryOpStatement &) = delete;
    UnaryOpStatement &operator=(const UnaryOpStatement &) = delete;
    // move operations
    UnaryOpStatement(UnaryOpStatement &&) = default;
    UnaryOpStatement &operator=(UnaryOpStatement &&) = default;
};
