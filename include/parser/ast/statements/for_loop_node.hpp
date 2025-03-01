#ifndef __FOR_LOOP_NODE_HPP__
#define __FOR_LOOP_NODE_HPP__

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>

/// @class `ForLoopNode`
/// @brief Represents traditional for loops
///
/// @note Takes ownership of all its constructor arguments
class ForLoopNode : public StatementNode {
  public:
    ForLoopNode(std::unique_ptr<StatementNode> &initializer, //
        std::unique_ptr<ExpressionNode> &condition,          //
        std::unique_ptr<StatementNode> &looparound,          //
        std::unique_ptr<Scope> &definition_scope             //
        ) :
        initializer(std::move(initializer)),
        condition(std::move(condition)),
        looparound(std::move(looparound)),
        definition_scope(std::move(definition_scope)) {}

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

    /// @var `initializer`
    /// @brief The initializer of the loop, most often 'i32 i = 0'
    std::unique_ptr<StatementNode> initializer;

    /// @var `condition`
    /// @brief The loop condition, for example 'i < 10'
    std::unique_ptr<ExpressionNode> condition;

    /// @var `looparound`
    /// @brief The looparound expression, for example 'i++'
    std::unique_ptr<StatementNode> looparound;

    /// @var `definition_scope`
    /// @brief The Scope of the definition, this is necessary to encapsulate the initializer within the for loop.
    ///        The definition_scope's body scope contains the body of the for loop
    std::unique_ptr<Scope> definition_scope;
};

#endif
