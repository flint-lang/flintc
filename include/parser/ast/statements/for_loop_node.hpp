#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `ForLoopNode`
/// @brief Represents traditional for loops
class ForLoopNode : public StatementNode {
  public:
    /// @note Takes ownership of all its constructor arguments
    ForLoopNode(                                    //
        std::unique_ptr<ExpressionNode> &condition, //
        std::shared_ptr<Scope> &definition_scope,   //
        std::unique_ptr<StatementNode> &looparound, //
        std::shared_ptr<Scope> &body                //
        ) :
        condition(std::move(condition)),
        definition_scope(std::move(definition_scope)),
        looparound(std::move(looparound)),
        body(std::move(body)) {}

    // constructor
    ForLoopNode() = default;
    // deconstructor
    ~ForLoopNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ForLoopNode(const ForLoopNode &) = delete;
    ForLoopNode &operator=(const ForLoopNode &) = delete;
    // move operations
    ForLoopNode(ForLoopNode &&) = default;
    ForLoopNode &operator=(ForLoopNode &&) = delete;

    /// @var `condition`
    /// @brief The loop condition, for example 'i < 10'
    std::unique_ptr<ExpressionNode> condition;

    /// @var `definition_scope`
    /// @brief The scope of the loops definition, containing the initializer ( 'i32 i = 0' for example )
    std::shared_ptr<Scope> definition_scope;

    /// @var `looparound`
    /// @brief The looparound statement which gets executed after every loop iteration
    std::unique_ptr<StatementNode> looparound;

    /// @var `body`
    /// @brief The scope of the actual loops body. The parent of the `body` scope is the `definition_scope` of the for loop. In this
    /// scope, the actual instantiation of the loop variable, the initializer, takes place. Ad the end of the body is the looparound
    /// statement contained, for example 'i++'
    std::shared_ptr<Scope> body;
};
