#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>

/// @class `AssignmentNode`
/// @brief Represents assignment statements
class AssignmentNode : public StatementNode {
  public:
    AssignmentNode(                                  //
        const std::shared_ptr<Type> &type,           //
        const std::string &name,                     //
        std::unique_ptr<ExpressionNode> &expression, //
        const bool is_shorthand = false              //
        ) :
        type(type),
        name(name),
        expression(std::move(expression)),
        is_shorthand(is_shorthand) {}

    Variation get_variation() const override {
        return Variation::ASSIGNMENT;
    }

    // constructor
    AssignmentNode() = default;
    // deconstructor
    ~AssignmentNode() override = default;
    // copy operations - deleted because of unique_ptr member
    AssignmentNode(const AssignmentNode &) = delete;
    AssignmentNode &operator=(const AssignmentNode &) = delete;
    // move operations
    AssignmentNode(AssignmentNode &&) = default;
    AssignmentNode &operator=(AssignmentNode &&) = default;

    /// @var `type`
    /// @brief The type of the variable
    std::shared_ptr<Type> type;

    /// @var `name`
    /// @brief The name of the variable being assigned to
    std::string name;

    /// @var `expression`
    /// @brief The expression to assign
    std::unique_ptr<ExpressionNode> expression;

    /// @var `is_shorthand`
    /// @brief Whether this is a shorthand assignment
    bool is_shorthand;
};
