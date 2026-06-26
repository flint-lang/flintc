#pragma once

#include "parser/ast/instance_call_node_base.hpp"
#include "statement_node.hpp"

/// @class `InstanceCallNodeStatement`
/// @brief Represents instance function or method call statements
class InstanceCallNodeStatement : public StatementNode, public InstanceCallNodeBase {
  public:
    InstanceCallNodeStatement(                                                    //
        const Hash &hash,                                                         //
        const token_slice &tokens,                                                //
        FunctionNode *function,                                                   //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type,                                        //
        std::unique_ptr<ExpressionNode> &instance_variable                        //
        ) :
        StatementNode(hash, tokens),
        InstanceCallNodeBase(function, arguments, error_types, type, instance_variable) {}

    Variation get_variation() const override {
        return Variation::INSTANCE_CALL;
    }

    // deconstructor
    ~InstanceCallNodeStatement() override = default;
    // copy operations - deleted because of unique_ptr member
    InstanceCallNodeStatement(const InstanceCallNodeStatement &) = delete;
    InstanceCallNodeStatement &operator=(const InstanceCallNodeStatement &) = delete;
    // move operations
    InstanceCallNodeStatement(InstanceCallNodeStatement &&) = default;
    InstanceCallNodeStatement &operator=(InstanceCallNodeStatement &&) = delete;
};
