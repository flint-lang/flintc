#pragma once

#include "parser/ast/expressions/expression_node.hpp"
#include "statement_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `GroupDeclarationNode`
/// @brief Represents group declarations of variable or data
class GroupDeclarationNode : public StatementNode {
  public:
    GroupDeclarationNode(                                                            //
        const std::vector<std::pair<std::shared_ptr<Type>, std::string>> &variables, //
        std::unique_ptr<ExpressionNode> &initializer                                 //
        ) :
        variables(variables),
        initializer(std::move(initializer)) {}

    Variation get_variation() const override {
        return Variation::GROUP_DECLARATION;
    }

    // empty constructor
    GroupDeclarationNode() = default;
    // destructor
    ~GroupDeclarationNode() override = default;
    // copy operations - disabled due to unique_ptr member
    GroupDeclarationNode(const GroupDeclarationNode &) = delete;
    GroupDeclarationNode &operator=(const GroupDeclarationNode &) = delete;
    // move operations
    GroupDeclarationNode(GroupDeclarationNode &&) = default;
    GroupDeclarationNode &operator=(GroupDeclarationNode &&) = default;

    /// @var `variables`
    /// @brief A list containing the types and names of the variables
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> variables;

    /// @var `initializer`
    /// @brief The expression with which the group will be initialized with
    std::unique_ptr<ExpressionNode> initializer;
};
