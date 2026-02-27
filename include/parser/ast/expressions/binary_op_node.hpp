#pragma once

#include "expression_node.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <utility>

/// @class `BinaryOpNode`
/// @brief Represents binary operations.
class BinaryOpNode : public ExpressionNode {
  public:
    explicit BinaryOpNode(                      //
        const Token operator_token,             //
        std::unique_ptr<ExpressionNode> &left,  //
        std::unique_ptr<ExpressionNode> &right, //
        const std::shared_ptr<Type> &type,      //
        bool is_shorthand = false               //
        ) :
        operator_token(operator_token),
        left(std::move(left)),
        right(std::move(right)),
        is_shorthand(is_shorthand) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::BINARY_OP;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::unique_ptr<ExpressionNode> left_clone = left->clone(scope_id);
        std::unique_ptr<ExpressionNode> right_clone = right->clone(scope_id);
        return std::make_unique<BinaryOpNode>(operator_token, left_clone, right_clone, this->type, is_shorthand);
    }

    // empty constructor
    BinaryOpNode() = default;
    // deconstructor
    ~BinaryOpNode() override = default;
    // copy operations - disabled due to unique_ptr memeber
    BinaryOpNode(const BinaryOpNode &) = delete;
    BinaryOpNode &operator=(const BinaryOpNode &) = delete;
    // move operations
    BinaryOpNode(BinaryOpNode &&) = default;
    BinaryOpNode &operator=(BinaryOpNode &&) = default;

    /// @var `operator_token`
    /// @brief The operator token of the binary operation
    Token operator_token{};

    /// @var `left`
    /// @brief The lhs of the binary operation
    std::unique_ptr<ExpressionNode> left;

    /// @var `right`
    /// @brief The rhs of the binary operation
    std::unique_ptr<ExpressionNode> right;

    /// @var `is_shorthand`
    /// @brief Whether this binary operation is a shorthand operation on the lhs (for += operations for example)
    bool is_shorthand;
};
