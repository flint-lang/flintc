#pragma once

#include "parser/ast/switch_base.hpp"

/// @struct `ESwitchBranch`
/// @brief One branch of the switch expression
struct ESwitchBranch {
  public:
    ESwitchBranch(std::unique_ptr<ExpressionNode> &match, std::unique_ptr<ExpressionNode> &expr) :
        match(std::move(match)),
        expr(std::move(expr)) {}

    /// @var `expr`
    /// @brief The expression to match the single switch branch to
    std::unique_ptr<ExpressionNode> match;

    /// @var `expr`
    /// @brief The expression to execute in this branch
    std::unique_ptr<ExpressionNode> expr;
};

/// @class `SwitchExpression`
/// @brief Represents switch expressions
class SwitchExpression : public SwitchBase, public ExpressionNode {
  public:
    SwitchExpression(std::unique_ptr<ExpressionNode> &switcher, std::vector<ESwitchBranch> &branches) :
        SwitchBase(switcher),
        branches(std::move(branches)) {
        this->type = this->branches.front().expr->type;
    }

    /// @var `branches`
    /// @brief All the possible switch branches
    std::vector<ESwitchBranch> branches;
};
