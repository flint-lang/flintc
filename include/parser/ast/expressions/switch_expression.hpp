#pragma once

#include "parser/ast/scope.hpp"
#include "parser/ast/switch_base.hpp"

/// @struct `ESwitchBranch`
/// @brief One branch of the switch expression
struct ESwitchBranch {
  public:
    ESwitchBranch(                                             //
        const std::shared_ptr<Scope> &scope,                   //
        std::vector<std::unique_ptr<ExpressionNode>> &matches, //
        std::unique_ptr<ExpressionNode> &expr                  //
        ) :
        scope(scope),
        matches(std::move(matches)),
        expr(std::move(expr)) {}

    /// @var `scope`
    /// @brief The scope in which the whole branch is contained in. This is needed for optionals / variants in branches to be able to
    /// "extract" the values
    std::shared_ptr<Scope> scope;

    /// @var `matches`
    /// @brief The expression(s) to match the single switch branch to
    /// @note If this vector contains multiple values this means that this branch will be executed in more cases
    std::vector<std::unique_ptr<ExpressionNode>> matches;

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
