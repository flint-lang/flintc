#pragma once

#include "expression_node.hpp"

#include <memory>
#include <vector>

/// @class `InitializerNode`
/// @brief Represents all initializer expressions
class InitializerNode : public ExpressionNode {
  public:
    InitializerNode(const std::shared_ptr<Type> &type, std::vector<std::unique_ptr<ExpressionNode>> &args) :
        args(std::move(args)) {
        this->type = type;
    }

    /// @var `args`
    /// @brief The arguments with which the initializer will be initialized
    std::vector<std::unique_ptr<ExpressionNode>> args;
};
