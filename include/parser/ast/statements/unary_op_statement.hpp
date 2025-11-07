#pragma once

#include "parser/ast/unary_op_base.hpp"
#include "statement_node.hpp"

/// class `UnaryOpStatement`
/// @brief Represents unary operation statements
class UnaryOpStatement : public UnaryOpBase, public StatementNode {
  public:
    using UnaryOpBase::UnaryOpBase;

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
