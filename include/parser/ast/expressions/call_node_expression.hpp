#pragma once

#include "expression_node.hpp"
#include "parser/ast/call_node_base.hpp"

#include <memory>
#include <vector>

/// @class `CallNodeExpression`
/// @brief Represents function or method calls
class CallNodeExpression : public CallNodeBase, public ExpressionNode {
  public:
    explicit CallNodeExpression(                                                  //
        FunctionNode *function,                                                   //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type                                         //
        ) :
        CallNodeBase(function, arguments, error_types, type) {
        ExpressionNode::type = type;
    }

    Variation get_variation() const override {
        return Variation::CALL;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments_clone;
        for (auto &[arg, is_reference] : arguments) {
            arguments_clone.emplace_back(arg->clone(scope_id), is_reference);
        }
        auto fn = std::make_unique<CallNodeExpression>(function, arguments_clone, error_types, ExpressionNode::type);
        fn->scope_id = scope_id;
        fn->has_catch = CallNodeBase::has_catch;
        return fn;
    }

    // deconstructor
    ~CallNodeExpression() override = default;
    // copy operations - deleted because of unique_ptr member
    CallNodeExpression(const CallNodeExpression &) = delete;
    CallNodeExpression &operator=(const CallNodeExpression &) = delete;
    // move operations
    CallNodeExpression(CallNodeExpression &&) = default;
    CallNodeExpression &operator=(CallNodeExpression &&) = delete;
};
