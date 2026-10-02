#pragma once

#include "evaluator/env.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/parser.hpp"
#include "value/value.hpp"

/// @class `Evaluator`
/// @brief The class which is responsible for evaluating all compile-time expressions
/// @note This class cannot be initialized and all functions within this class are static
/// @note None of the functions inside this class throw any errors, to make it also act as a check on comptime evaluation. It evaluates as
///       much as it can and if it cannot evaluate the whole thing the functions return a nullopt. For example when evaluating `foo(10 + 20,
///       30)` the call `foo` is not compile-time evaluatable as its not a comptime call `@foo`, but the parameter `10 + 20` is evaluatable.
///       So, eve, if the result of the eval functions is nullopt the eval functions *may* evaluated deeper nested expressions if they were
///       evaluatable
class Evaluator {
  public:
    Evaluator() = delete;

    /// @enum `Mode`
    /// @brief The evaluation mode, needed for expression evaluation
    enum class Mode {
        // When evaluating the expression the result is an rvalue, e.g. an explicit value. In this mode the result of the expression is
        // *stored* in the `env.result` field
        RVALUE,
        // When evaluating the expression the result is an lvalue, e.g. an *address*. When this mode is active the result *address* (not
        // loading the result) is pushed onto the `env.lvalue_stack` field
        LVALUE,
        // When evaluating the expression the result is both an rvalue and an lvalue, so the value is both loaded and stored in `env.result`
        // and its address is stored in the `env.lvalue_stack` as well
        RLVALUE,
    };

    /// @function `eval_function`
    /// @brief Evaluates the given function and returns the compile-time value resulting from it
    ///
    /// @param `parser` The parser instance in which to evaluate the given functions scope. Needed for on-demand parsing
    /// @param `function` The function to evaluate
    /// @param `args` The arguments with which to call the given function at comptime
    /// @return `std::pair<std::optional<std::shared_ptr<Value>>, bool>` A pair holding:
    ///   - The result of evaluating the function, if there even is a result at all
    ///   - Whether the function was fully evaluated (true) or failed (false)
    [[nodiscard]] static std::pair<std::optional<std::shared_ptr<Value>>, bool> eval_function( //
        Parser &parser,                                                                        //
        FunctionNode *const function,                                                          //
        const std::vector<std::shared_ptr<Value>> &args                                        //
    );

    /// @function `eval_scope`
    /// @brief Evaluates the given scope and returns the compile-time value resulting from it. A scope not returning a value is *not* an
    /// error, as a scope could strictly be non-returning.
    ///
    /// @param `parser` The parser instance in which to evaluate the given scope. Needed for on-demand parsing
    /// @param `env` The environment of the evaluation containing variables etc
    /// @param `scope` The scope to evaluate
    /// @return `bool` Whether the scope was fully evaluated or failed
    [[nodiscard]] static bool eval_scope(Parser &parser, Env &env, std::shared_ptr<Scope> &scope);

    /// @function `eval_stmt`
    /// @brief Evaluates the given statement and returns the compile-time value resulting from it
    ///
    /// @param `parser` The parser instance in which to evaluate the given statement. Needed for on-demand parsing
    /// @param `env` The environment of the evaluation containing variables etc
    /// @param `stmt` The statement to evaluate
    /// @return `bool` Whether the statement was fully evaluated or failed
    [[nodiscard]] static bool eval_stmt(Parser &parser, Env &env, StatementNode *const stmt);

    /// @function `eval_expr`
    /// @brief Evaluates the given expression and returns the compile-time value resulting from it
    ///
    /// @param `parser` The parser instance in which to evaluate the given expression. Needed for on-demand parsing
    /// @param `env` The environment of the evaluation containing variables etc
    /// @param `expr` The expression to evaluate
    /// @param `mode` The mode in which to evaluate the expression
    /// @return `bool` Whether the expression was fully evaluated or failed
    [[nodiscard]] static bool eval_expr(Parser &parser, Env &env, ExpressionNode *const expr, const Mode mode);

    /// @function `eval_binop`
    /// @brief Evaluates the given binary operator expression and returns the compile-time value resulting from it
    ///
    /// @param `parser` The parser instance in which to evaluate the given binary operation. Needed for on-demand parsing
    /// @param `env` The environment of the evaluation containing variables etc
    /// @param `binop` The binary operator expression to evaluate
    /// @return `bool` Whether the binary operation was fully evaluated or failed
    [[nodiscard]] static bool eval_binop(Parser &parser, Env &env, BinaryOpNode *const binop);

    /// @function `eval_literal`
    /// @brief Evaluates the given literal expression and returns the compile-time value resulting from it
    ///
    /// @param `env` The environment of the evaluation containing variables etc
    /// @param `literal` The literal expression to evaluate
    /// @return `bool` Whether the literal was fully evaluated or failed
    [[nodiscard]] static bool eval_literal(Env &env, LiteralNode *const literal);
};
