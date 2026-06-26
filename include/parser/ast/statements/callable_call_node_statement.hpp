#pragma once

#include "parser/ast/callable_call_node_base.hpp"
#include "statement_node.hpp"

/// @class `CallableCallNodeStatement`
/// @brief Represents call statements of callable variables
class CallableCallNodeStatement : public StatementNode, public CallableCallNodeBase {
  public:
    CallableCallNodeStatement(                                                    //
        const Hash &hash,                                                         //
        const token_slice &tokens,                                                //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type,                                        //
        const std::string &callable_variable                                      //
        ) :
        StatementNode(hash, tokens),
        CallableCallNodeBase(arguments, error_types, type, callable_variable) {}

    Variation get_variation() const override {
        return Variation::CALLABLE_CALL;
    }

    // deconstructor
    ~CallableCallNodeStatement() override = default;
    // copy operations - deleted because of unique_ptr member
    CallableCallNodeStatement(const CallableCallNodeStatement &) = delete;
    CallableCallNodeStatement &operator=(const CallableCallNodeStatement &) = delete;
    // move operations
    CallableCallNodeStatement(CallableCallNodeStatement &&) = default;
    CallableCallNodeStatement &operator=(CallableCallNodeStatement &&) = delete;
};
