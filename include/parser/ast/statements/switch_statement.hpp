#pragma once

#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/switch_base.hpp"

/// @struct `SSwitchBranch`
/// @brief One branch of the switch statement
struct SSwitchBranch {
  public:
    SSwitchBranch(std::unique_ptr<ExpressionNode> &match, std::unique_ptr<Scope> &body) :
        match(std::move(match)),
        body(std::move(body)) {}

    /// @var `match`
    /// @brief The expression to match the single switch branch to
    std::unique_ptr<ExpressionNode> match;

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
