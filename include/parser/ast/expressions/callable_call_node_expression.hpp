#pragma once

#include "expression_node.hpp"
#include "parser/ast/callable_call_node_base.hpp"
#include <memory>
#include <vector>

/// @class `CallableCallNodeExpression`
/// @brief Represents calls of callable variables
class CallableCallNodeExpression : public ExpressionNode, public CallableCallNodeBase {
  public:
    explicit CallableCallNodeExpression(                                          //
        const Hash &hash,                                                         //
        const PosTriple &pos,                                                     //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type,                                        //
        const std::string &callable_variable                                      //
        ) :
        ExpressionNode(hash, pos, true),
        CallableCallNodeBase(arguments, error_types, type, callable_variable) {
        ExpressionNode::type = type;
    }

    Variation get_variation() const override {
        return Variation::CALLABLE_CALL;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments_clone;
        for (auto &[arg, is_reference] : arguments) {
            arguments_clone.emplace_back(arg->clone(scope_id), is_reference);
        }
        return std::make_unique<CallableCallNodeExpression>(                      //
            file_hash, PosTriple{line, column, length},                           //
            arguments_clone, error_types, ExpressionNode::type, callable_variable //
        );
    }

    // deconstructor
    ~CallableCallNodeExpression() override = default;
    // copy operations - deleted because of unique_ptr member
    CallableCallNodeExpression(const CallableCallNodeExpression &) = delete;
    CallableCallNodeExpression &operator=(const CallableCallNodeExpression &) = delete;
    // move operations
    CallableCallNodeExpression(CallableCallNodeExpression &&) = default;
    CallableCallNodeExpression &operator=(CallableCallNodeExpression &&) = delete;
};
