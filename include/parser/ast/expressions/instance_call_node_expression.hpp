#pragma once

#include "expression_node.hpp"
#include "parser/ast/instance_call_node_base.hpp"

#include <memory>
#include <vector>

/// @class `InstanceCallNodeExpression`
/// @brief Represents function or method calls
class InstanceCallNodeExpression : public InstanceCallNodeBase, public ExpressionNode {
  public:
    explicit InstanceCallNodeExpression(                                          //
        FunctionNode *function,                                                   //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type,                                        //
        std::unique_ptr<ExpressionNode> &instance_variable                        //
        ) :
        InstanceCallNodeBase(function, arguments, error_types, type, instance_variable) {
        ExpressionNode::type = type;
    }

    Variation get_variation() const override {
        return Variation::INSTANCE_CALL;
    }

    // deconstructor
    ~InstanceCallNodeExpression() override = default;
    // copy operations - deleted because of unique_ptr member
    InstanceCallNodeExpression(const InstanceCallNodeExpression &) = delete;
    InstanceCallNodeExpression &operator=(const InstanceCallNodeExpression &) = delete;
    // move operations
    InstanceCallNodeExpression(InstanceCallNodeExpression &&) = default;
    InstanceCallNodeExpression &operator=(InstanceCallNodeExpression &&) = delete;
};
