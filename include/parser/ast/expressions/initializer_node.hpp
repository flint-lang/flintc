#pragma once

#include "expression_node.hpp"

#include <memory>
#include <vector>

/// @class `InitializerNode`
/// @brief Represents all initializer expressions
class InitializerNode : public ExpressionNode {
  public:
    InitializerNode(const std::shared_ptr<Type> &type, const bool is_data, std::vector<std::unique_ptr<ExpressionNode>> &args) :
        is_data(is_data),
        args(std::move(args)) {
        this->type = type;
    }

    /// @var `is_data`
    /// @brief Determines whether the initialized type is a data or entity type
    bool is_data;

    /// @var `args`
    /// @brief The arguments with which the initializer will be initialized
    std::vector<std::unique_ptr<ExpressionNode>> args;
};
