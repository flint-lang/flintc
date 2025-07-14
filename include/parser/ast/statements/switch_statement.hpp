#pragma once

#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/switch_base.hpp"

/// @struct `SSwitchBranch`
/// @brief One branch of the switch statement
struct SSwitchBranch {
  public:
    SSwitchBranch(std::vector<std::unique_ptr<ExpressionNode>> &matches, std::unique_ptr<Scope> &body) :
        matches(std::move(matches)),
        body(std::move(body)) {}

    /// @var `matches`
    /// @brief The expression(s) to match the single switch branch to
    /// @note If this vector contains multiple values this means that this branch will be executed in more cases
    std::vector<std::unique_ptr<ExpressionNode>> matches;

    /// @var `body`
    /// @brief The body of this switch branch
    std::unique_ptr<Scope> body;
};

/// @class `SwitchStatement`
/// @brief Represents switch statements
class SwitchStatement : public SwitchBase, public StatementNode {
  public:
    SwitchStatement(std::unique_ptr<ExpressionNode> &switcher, std::vector<SSwitchBranch> &branches) :
        SwitchBase(switcher),
        branches(std::move(branches)) {}

    /// @var `branches`
    /// @brief All the possible switch branches
    std::vector<SSwitchBranch> branches;
};
