#pragma once

#include "expressions/expression_node.hpp"
#include "parser/ast/call_node_base.hpp"
#include "parser/type/type.hpp"

#include <memory>
#include <vector>

/// @class `InstanceCallNodeBase`
/// @brief The base class for instance calls, both statements and expressions
class InstanceCallNodeBase : public CallNodeBase {
  public:
    InstanceCallNodeBase(                                                         //
        FunctionNode *function,                                                   //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type,                                        //
        std::unique_ptr<ExpressionNode> &instance_variable                        //
        ) :
        CallNodeBase(function, arguments, error_types, type),
        instance_variable(std::move(instance_variable)) {}

    // deconstructor
    ~InstanceCallNodeBase() = default;
    // copy operations - disabled due to unique_ptr member
    InstanceCallNodeBase(const InstanceCallNodeBase &) = delete;
    InstanceCallNodeBase &operator=(const InstanceCallNodeBase &) = delete;
    // move operations
    InstanceCallNodeBase(InstanceCallNodeBase &&) = default;
    InstanceCallNodeBase &operator=(InstanceCallNodeBase &&) = delete;

    /// @var `instance_variable`
    /// @brief The instance variable on which this instance call is executed at
    std::unique_ptr<ExpressionNode> instance_variable;

  protected:
    InstanceCallNodeBase() = default;
};
