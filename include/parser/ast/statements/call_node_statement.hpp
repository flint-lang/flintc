#pragma once

#include "parser/ast/call_node_base.hpp"
#include "statement_node.hpp"

/// @class `CallNodeStatement`
/// @brief Represents function or method call statements
class CallNodeStatement : public StatementNode, public CallNodeBase {
  public:
    CallNodeStatement(                                                            //
        const Hash &hash,                                                         //
        const token_slice &tokens,                                                //
        FunctionNode *function,                                                   //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type                                         //
        ) :
        StatementNode(hash, tokens),
        CallNodeBase(function, arguments, error_types, type) {}

    Variation get_variation() const override {
        return Variation::CALL;
    }

    // deconstructor
    ~CallNodeStatement() override = default;
    // copy operations - deleted because of unique_ptr member
    CallNodeStatement(const CallNodeStatement &) = delete;
    CallNodeStatement &operator=(const CallNodeStatement &) = delete;
    // move operations
    CallNodeStatement(CallNodeStatement &&) = default;
    CallNodeStatement &operator=(CallNodeStatement &&) = delete;
};
