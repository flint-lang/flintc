#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "statement_node.hpp"

#include <memory>
#include <utility>
#include <variant>

/// @class `EnhForLoopNode`
/// @brief Represents enhanced for loops
class EnhForLoopNode : public StatementNode {
  public:
    /// @note Takes ownership of all its constructor arguments
    EnhForLoopNode(                                                                                                    //
        const std::variant<std::pair<std::optional<std::string>, std::optional<std::string>>, std::string> &iterators, //
        std::unique_ptr<ExpressionNode> &iterable,                                                                     //
        std::shared_ptr<Scope> &definition_scope,                                                                      //
        std::shared_ptr<Scope> &body                                                                                   //
        ) :
        iterators(iterators),
        iterable(std::move(iterable)),
        definition_scope(std::move(definition_scope)),
        body(std::move(body)) {}

    Variation get_variation() const override {
        return Variation::ENHANCED_FOR_LOOP;
    }

    // constructor
    EnhForLoopNode() = default;
    // deconstructor
    ~EnhForLoopNode() override = default;
    // copy operations - deleted because of unique_ptr member
    EnhForLoopNode(const EnhForLoopNode &) = delete;
    EnhForLoopNode &operator=(const EnhForLoopNode &) = delete;
    // move operations
    EnhForLoopNode(EnhForLoopNode &&) = default;
    EnhForLoopNode &operator=(EnhForLoopNode &&) = delete;

    /// @var `iterators`
    /// @brief Either a pair of index and element or a single value, which then is the tuple iteration
    std::variant<std::pair<std::optional<std::string>, std::optional<std::string>>, std::string> iterators;

    /// @var `iterable`
    /// @brief The iterable to iterate through
    std::unique_ptr<ExpressionNode> iterable;

    /// @var `definition_scope`
    /// @brief The scope of the loops definition, containing the initializer ( 'i32 i = 0' for example )
    std::shared_ptr<Scope> definition_scope;

    /// @var `body`
    /// @brief The scope of the actual loops body. The parent of the `body` scope is the `definition_scope` of the for loop. In this
    /// scope, the actual instantiation of the loop variable, the initializer, takes place. Ad the end of the body is the looparound
    /// statement contained, for example 'i++'
    std::shared_ptr<Scope> body;
};
