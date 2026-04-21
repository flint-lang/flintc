#pragma once

#include "expressions/expression_node.hpp"
#include "parser/ast/call_node_base.hpp"
#include "parser/type/type.hpp"

#include <memory>
#include <vector>

/// @class `CallableCallNodeBase`
/// @brief The base class for callable calls, both statements and expressions
class CallableCallNodeBase : public CallNodeBase {
  public:
    CallableCallNodeBase(                                                         //
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
        const std::vector<std::shared_ptr<Type>> &error_types,                    //
        const std::shared_ptr<Type> &type,                                        //
        const std::string &callable_variable                                      //
        ) :
        CallNodeBase(nullptr, arguments, error_types, type),
        callable_variable(callable_variable) {}

    // deconstructor
    ~CallableCallNodeBase() = default;
    // copy operations - disabled due to unique_ptr member
    CallableCallNodeBase(const CallableCallNodeBase &) = delete;
    CallableCallNodeBase &operator=(const CallableCallNodeBase &) = delete;
    // move operations
    CallableCallNodeBase(CallableCallNodeBase &&) = default;
    CallableCallNodeBase &operator=(CallableCallNodeBase &&) = delete;

    /// @var `callable_variable`
    /// @brief The callable variable on which this callable call is executed at
    std::string callable_variable;

  protected:
    CallableCallNodeBase() = default;
};
