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

    Variation get_variation() const override {
        return Variation::INITIALIZER;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::vector<std::unique_ptr<ExpressionNode>> args_clone;
        for (auto &arg : args) {
            args_clone.emplace_back(arg->clone(scope_id));
        }
        return std::make_unique<InitializerNode>(this->type, args_clone);
    }

    /// @var `args`
    /// @brief The arguments with which the initializer will be initialized
    std::vector<std::unique_ptr<ExpressionNode>> args;
};
