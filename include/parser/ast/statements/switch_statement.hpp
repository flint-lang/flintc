#pragma once

#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/switch_base.hpp"

/// @struct `Branch`
/// @brief One branch of the switch statement
struct SwitchBranch {
  public:
    SwitchBranch(std::unique_ptr<ExpressionNode> &expr, std::unique_ptr<Scope> &body) :
        expr(std::move(expr)),
        body(std::move(body)) {}

    /// @var `expr`
    /// @brief The expression to match the single switch branch to
    std::unique_ptr<ExpressionNode> expr;

    /// @var `body`
    /// @brief The body of this switch branch
    std::unique_ptr<Scope> body;
};

/// @class `SwitchStatement`
/// @brief Represents switch statements
class SwitchStatement : public SwitchBase, public StatementNode {
  public:
    SwitchStatement(std::unique_ptr<ExpressionNode> &switcher, std::vector<SwitchBranch> &branches) :
        SwitchBase(switcher),
        branches(std::move(branches)) {}

    /// @var `branches`
    /// @brief All the possible switch branches
    std::vector<SwitchBranch> branches;
};
