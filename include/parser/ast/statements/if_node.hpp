#ifndef __IF_NODE_HPP__
#define __IF_NODE_HPP__

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "statement_node.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

/// IfNode
///     Represents if statements
class IfNode : public StatementNode {
  public:
    IfNode(std::unique_ptr<ExpressionNode> &condition, std::unique_ptr<Scope> &then_scope,
        std::optional<std::variant<std::unique_ptr<IfNode>, std::unique_ptr<Scope>>> &else_scope) :
        condition(std::move(condition)),
        then_scope(std::move(then_scope)),
        else_scope(std::move(else_scope)) {}

    // constructor
    IfNode() = default;
    // deconstructor
    ~IfNode() override = default;
    // copy operations - deleted because of unique_ptr member
    IfNode(const IfNode &) = delete;
    IfNode &operator=(const IfNode &) = delete;
    // move operations
    IfNode(IfNode &&) = default;
    IfNode &operator=(IfNode &&) = default;

    /// condition
    ///     The COndition expression
    std::unique_ptr<ExpressionNode> condition;
    /// then_branch
    ///     The statements to execute when the condition evaluates to 'true'
    std::unique_ptr<Scope> then_scope;
    /// else_branch
    ///     The statements to execute when the condition evaluates to 'false'
    std::optional<std::variant<std::unique_ptr<IfNode>, std::unique_ptr<Scope>>> else_scope;
};

#endif
