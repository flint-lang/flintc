#include "evaluator/evaluator.hpp"

#include "evaluator/value/array_value.hpp"
#include "evaluator/value/bool_value.hpp"
#include "evaluator/value/char_value.hpp"
#include "evaluator/value/float_value.hpp"
#include "evaluator/value/int_value.hpp"
#include "evaluator/value/str_value.hpp"
#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/comptime_node.hpp"
#include "parser/ast/expressions/variable_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"

#include "types.hpp"
#include <cstddef>
#include <memory>
#include <variant>

std::pair<std::optional<std::shared_ptr<Value>>, bool> Evaluator::eval_function( //
    Parser &parser,                                                              //
    FunctionNode *const function,                                                //
    const std::vector<std::shared_ptr<Value>> &args                              //
) {
    if (!function->scope.has_value() || function->visibility == FunctionNode::Visibility::EXTERN) {
        return {std::nullopt, false};
    }
    if (args.size() != function->parameters.size()) {
        return {std::nullopt, false};
    }
    Env env;
    for (size_t i = 0; i < args.size(); i++) {
        env.declare(function->parameters.at(i).name, args.at(i));
    }
    if (!eval_scope(parser, env, function->scope.value())) {
        return {std::nullopt, false};
    }
    return {env.result, true};
}

bool Evaluator::eval_scope(Parser &parser, Env &env, std::shared_ptr<Scope> &scope) {
    Parser::Context ctx = Parser::Context{
        .env = env,
        .level = ContextLevel::COMPTIME,
        .scope = scope,
        .tokens = {},
    };
    return parser.parse_scope(ctx);
}

bool Evaluator::eval_stmt(Parser &parser, Env &env, StatementNode *const stmt) {
    switch (stmt->get_variation()) {
        case StatementNode::Variation::ARRAY_ASSIGNMENT: {
            auto *const node = stmt->as<ArrayAssignmentNode>();
            std::vector<size_t> indices;
            for (const auto &index_expr : node->indexing_expressions) {
                if (!eval_expr(parser, env, index_expr.get(), Mode::RVALUE)) {
                    return false;
                }
                if (!env.result.has_value()) {
                    return false;
                }
                ASSERT(env.result.value()->get_variation() == Value::Variation::INT);
                const PosTriple expr_pos = PosTriple{
                    .line = index_expr->line,
                    .column = index_expr->column,
                    .length = index_expr->length,
                };
                indices.emplace_back(env.result.value()->as<IntValue>()->value.to_uN<size_t>(index_expr->file_hash, expr_pos).value());
            }
            if (!eval_expr(parser, env, node->expression.get(), Evaluator::Mode::RVALUE)) {
                return false;
            }
            if (!env.result.has_value()) {
                return false;
            }
            const std::shared_ptr<Value> rhs_value = env.result.value();
            env.result = std::nullopt;
            if (!eval_expr(parser, env, node->base_expr.get(), Evaluator::Mode::RVALUE)) {
                return false;
            }
            if (!env.result.has_value()) {
                return false;
            }
            ArrayValue *base = env.result.value()->as<ArrayValue>();
            std::optional<std::shared_ptr<Value> *> target = base->get_value_at(indices);
            if (!target.has_value()) {
                return false;
            }
            *(target.value()) = rhs_value;
            break;
        }
        case StatementNode::Variation::ASSIGNMENT: {
            auto *const node = stmt->as<AssignmentNode>();
            if (!eval_expr(parser, env, node->expression.get(), Mode::RVALUE) || !env.result.has_value()) {
                return false;
            }
            env.assign(node->name, env.result.value());
            break;
        }
        case StatementNode::Variation::BREAK:
            break;
        case StatementNode::Variation::CALL:
            UNREACHABLE();
            break;
        case StatementNode::Variation::CALLABLE_CALL:
            break;
        case StatementNode::Variation::CATCH:
            break;
        case StatementNode::Variation::CONTINUE:
            break;
        case StatementNode::Variation::DATA_FIELD_ASSIGNMENT:
            break;
        case StatementNode::Variation::DECLARATION: {
            auto *const node = stmt->as<DeclarationNode>();
            if (!eval_expr(parser, env, node->initializer.get(), Mode::RVALUE) || !env.result.has_value()) {
                return false;
            }
            env.declare(node->name, env.result.value());
            break;
        }
        case StatementNode::Variation::DO_WHILE:
            break;
        case StatementNode::Variation::ENHANCED_FOR_LOOP:
            break;
        case StatementNode::Variation::FOR_LOOP:
            break;
        case StatementNode::Variation::GROUP_ASSIGNMENT:
            break;
        case StatementNode::Variation::GROUP_DECLARATION:
            break;
        case StatementNode::Variation::GROUPED_ARRAY_ASSIGNMENT:
            break;
        case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT:
            break;
        case StatementNode::Variation::IF: {
            auto *const node = stmt->as<IfNode>();
            if (!eval_expr(parser, env, node->condition.get(), Mode::RVALUE) || !env.result.has_value()) {
                return false;
            }
            ASSERT(env.result.value()->get_variation() == Value::Variation::BOOL);
            if (env.result.value()->as<BoolValue>()->value) {
                env.result = std::nullopt;
                return eval_scope(parser, env, node->then_scope);
            }

            std::optional<std::variant<std::unique_ptr<IfNode>, std::shared_ptr<Scope>>> *else_scope = &node->else_scope;
            while (else_scope->has_value()) {
                env.result = std::nullopt;
                if (std::holds_alternative<std::shared_ptr<Scope>>(else_scope->value())) {
                    return eval_scope(parser, env, std::get<std::shared_ptr<Scope>>(else_scope->value()));
                }

                auto &else_if = std::get<std::unique_ptr<IfNode>>(else_scope->value());
                if (!eval_expr(parser, env, else_if->condition.get(), Mode::RVALUE) || !env.result.has_value()) {
                    return false;
                }
                ASSERT(env.result.value()->get_variation() == Value::Variation::BOOL);
                if (env.result.value()->as<BoolValue>()->value) {
                    env.result = std::nullopt;
                    return eval_scope(parser, env, else_if->then_scope);
                }
                else_scope = &else_if->else_scope;
            }
            break;
        } break;
        case StatementNode::Variation::INSTANCE_CALL:
            break;
        case StatementNode::Variation::RETURN: {
            const auto *node = stmt->as<ReturnNode>();
            env.done = true;
            env.result = std::nullopt;
            if (node->return_value.has_value()) {
                return eval_expr(parser, env, node->return_value.value().get(), Mode::RVALUE);
            }
            break;
        }
        case StatementNode::Variation::SWITCH:
            break;
        case StatementNode::Variation::THROW:
            break;
        case StatementNode::Variation::UNARY_OP:
            break;
        case StatementNode::Variation::WHILE:
            break;
    }
    return true;
}

bool Evaluator::eval_expr(Parser &parser, Env &env, ExpressionNode *const expr, const Mode mode) {
    switch (expr->get_variation()) {
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            auto *const node = expr->as<ArrayAccessNode>();
            std::vector<size_t> indices;
            for (const auto &index_expr : node->indexing_expressions) {
                if (!eval_expr(parser, env, index_expr.get(), Mode::RVALUE)) {
                    return false;
                }
                if (!env.result.has_value()) {
                    return false;
                }
                ASSERT(env.result.value()->get_variation() == Value::Variation::INT);
                const PosTriple expr_pos = PosTriple{
                    .line = index_expr->line,
                    .column = index_expr->column,
                    .length = index_expr->length,
                };
                indices.emplace_back(env.result.value()->as<IntValue>()->value.to_uN<size_t>(index_expr->file_hash, expr_pos).value());
            }
            env.result = std::nullopt;
            if (!eval_expr(parser, env, node->base_expr.get(), Evaluator::Mode::RVALUE)) {
                return false;
            }
            if (!env.result.has_value()) {
                return false;
            }
            ArrayValue *base = env.result.value()->as<ArrayValue>();
            std::optional<std::shared_ptr<Value> *> target = base->get_value_at(indices);
            if (!target.has_value()) {
                return false;
            }
            env.result = *target.value();
            break;
        }
        case ExpressionNode::Variation::ARRAY_INITIALIZER: {
            ASSERT(mode == Mode::RVALUE);
            auto *const node = expr->as<ArrayInitializerNode>();
            std::vector<size_t> sizes;
            for (const auto &length_expr : node->length_expressions) {
                if (!eval_expr(parser, env, length_expr.get(), Mode::RVALUE)) {
                    return false;
                }
                if (!env.result.has_value()) {
                    return false;
                }
                ASSERT(env.result.value()->get_variation() == Value::Variation::INT);
                const PosTriple expr_pos = PosTriple{
                    .line = length_expr->line,
                    .column = length_expr->column,
                    .length = length_expr->length,
                };
                sizes.emplace_back(env.result.value()->as<IntValue>()->value.to_uN<size_t>(length_expr->file_hash, expr_pos).value());
            }
            if (!eval_expr(parser, env, node->initializer_value.get(), Mode::RVALUE)) {
                return false;
            }
            if (!env.result.has_value()) {
                return false;
            }
            std::shared_ptr<Type> array_type = std::make_shared<ArrayType>(sizes.size(), node->element_type, sizes);
            if (!parser.file_node_ptr->file_namespace->add_type(array_type)) {
                array_type = parser.file_node_ptr->file_namespace->get_type_from_str(array_type->to_string()).value();
            }
            env.result = make_shared<ArrayValue>(env.result.value(), array_type);
            break;
        }
        case ExpressionNode::Variation::BINARY_OP:
            ASSERT(mode == Mode::RVALUE);
            return eval_binop(parser, env, expr->as<BinaryOpNode>());
        case ExpressionNode::Variation::CALL: {
            ASSERT(mode == Mode::RVALUE);
            // TODO: Once complex comptime values (like data) exist the arguments need to be RLVALUEs
            auto *const node = expr->as<CallNodeExpression>();
            std::vector<std::shared_ptr<Value>> args;
            for (size_t i = 0; i < node->arguments.size(); i++) {
                if (!eval_expr(parser, env, node->arguments.at(i).first.get(), Mode::RVALUE)) {
                    return false;
                }
                if (!env.result.has_value()) {
                    return false;
                }
                args.emplace_back(env.result.value());
            }
            const auto &[result, evaluated] = eval_function(parser, node->function, args);
            if (!evaluated) {
                return false;
            }
            if (!result.has_value()) {
                return false;
            }
            env.result = result;
            break;
        }
        case ExpressionNode::Variation::CALLABLE_CALL:
            break;
        case ExpressionNode::Variation::COMPTIME: {
            ASSERT(mode == Mode::RVALUE);
            env.result = expr->as<ComptimeNode>()->value;
            return true;
        }
        case ExpressionNode::Variation::DATA_ACCESS:
            break;
        case ExpressionNode::Variation::GROUP_EXPRESSION:
            break;
        case ExpressionNode::Variation::GROUPED_ARRAY_ACCESS:
            break;
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS:
            break;
        case ExpressionNode::Variation::FUNCTION_REFERENCE:
            break;
        case ExpressionNode::Variation::INITIALIZER:
            break;
        case ExpressionNode::Variation::INLINE_ARRAY_INITIALIZER: {
            UNREACHABLE();
            break;
        }
        case ExpressionNode::Variation::INSTANCE_CALL:
            break;
        case ExpressionNode::Variation::LITERAL:
            ASSERT(mode == Mode::RVALUE);
            return eval_literal(env, expr->as<LiteralNode>());
        case ExpressionNode::Variation::OPTIONAL_CHAIN:
            break;
        case ExpressionNode::Variation::OPTIONAL_UNWRAP:
            break;
        case ExpressionNode::Variation::RANGE_EXPRESSION:
            break;
        case ExpressionNode::Variation::STRING_INTERPOLATION:
            break;
        case ExpressionNode::Variation::SWITCH_EXPRESSION:
            break;
        case ExpressionNode::Variation::SWITCH_DEFAULT:
            break;
        case ExpressionNode::Variation::SWITCH_MATCH:
            break;
        case ExpressionNode::Variation::TYPE_CAST:
            return eval_expr(parser, env, expr->as<TypeCastNode>()->expr.get(), mode);
        case ExpressionNode::Variation::TYPE:
            break;
        case ExpressionNode::Variation::UNARY_OP:
            break;
        case ExpressionNode::Variation::VARIABLE: {
            std::optional<std::shared_ptr<Value> *> addr = env.lookup(expr->as<VariableNode>()->name);
            if (!addr.has_value()) {
                return false;
            }
            switch (mode) {
                case Mode::RVALUE:
                    env.result = *addr.value();
                    break;
                case Mode::LVALUE:
                    env.lvalue_stack.emplace(addr.value());
                    break;
                case Mode::RLVALUE:
                    env.result = *addr.value();
                    env.lvalue_stack.emplace(addr.value());
                    break;
            }
            return true;
        }
        case ExpressionNode::Variation::VARIANT_EXTRACTION:
            break;
        case ExpressionNode::Variation::VARIANT_UNWRAP:
            break;
    }
    return true;
}

bool Evaluator::eval_binop(Parser &parser, Env &env, BinaryOpNode *const binop) {
    const bool lhs_evaluated = eval_expr(parser, env, binop->left.get(), Mode::RVALUE);
    const auto lhs = env.result;
    env.result = std::nullopt;

    const bool rhs_evaluated = eval_expr(parser, env, binop->right.get(), Mode::RVALUE);
    const auto rhs = env.result;
    env.result = std::nullopt;

    if (!lhs_evaluated || !lhs.has_value() || !rhs_evaluated || !rhs.has_value()) {
        return false;
    }
    switch (binop->operator_token) {
        default:
            // Unsupported folding operation of literals
            break;
        case TOK_PLUS:
            switch (lhs.value()->get_variation()) {
                case Value::Variation::CHAR: {
                    const CharValue *lhs_char = lhs.value()->as<CharValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::CHAR: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = APInt(std::to_string(static_cast<uint32_t>(rhs.value()->as<CharValue>()->value)));
                            const APInt result_int = lhs_int + rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        case Value::Variation::INT: {
                            APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            lhs_int += rhs.value()->as<IntValue>()->value;
                            const auto result = lhs_int.to_uN<uint8_t>(                                //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        case Value::Variation::STR:
                            env.result = std::make_shared<StrValue>(std::string(1, lhs_char->value) + rhs.value()->as<StrValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::FLOAT: {
                    const FloatValue *lhs_float = lhs.value()->as<FloatValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value + rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value + rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::INT: {
                    const IntValue *lhs_int = lhs.value()->as<IntValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(APFloat(lhs_int->value) + rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<IntValue>(lhs_int->value + rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::STR: {
                    const StrValue *lhs_str = lhs.value()->as<StrValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::CHAR:
                            env.result = std::make_shared<StrValue>(lhs_str->value + std::string(1, rhs.value()->as<CharValue>()->value));
                            return true;
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<StrValue>(lhs_str->value + rhs.value()->as<FloatValue>()->value.to_string());
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<StrValue>(lhs_str->value + rhs.value()->as<IntValue>()->value.to_string());
                            return true;
                        case Value::Variation::STR:
                            env.result = std::make_shared<StrValue>(lhs_str->value + rhs.value()->as<StrValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case TOK_MINUS:
            switch (lhs.value()->get_variation()) {
                case Value::Variation::CHAR: {
                    const CharValue *lhs_char = lhs.value()->as<CharValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::CHAR: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = APInt(std::to_string(static_cast<uint32_t>(rhs.value()->as<CharValue>()->value)));
                            const APInt result_int = lhs_int - rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        case Value::Variation::INT: {
                            APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            lhs_int -= rhs.value()->as<IntValue>()->value;
                            const auto result = lhs_int.to_uN<uint8_t>(                                //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::FLOAT: {
                    const FloatValue *lhs_float = lhs.value()->as<FloatValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value - rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value - rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::INT: {
                    const IntValue *lhs_int = lhs.value()->as<IntValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(APFloat(lhs_int->value) - rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<IntValue>(lhs_int->value - rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case TOK_MULT:
            switch (lhs.value()->get_variation()) {
                case Value::Variation::CHAR: {
                    const CharValue *lhs_char = lhs.value()->as<CharValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::CHAR: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = APInt(std::to_string(static_cast<uint32_t>(rhs.value()->as<CharValue>()->value)));
                            const APInt result_int = lhs_int * rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        case Value::Variation::INT: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = rhs.value()->as<IntValue>()->value;
                            const APInt result_int = lhs_int * rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::FLOAT: {
                    const FloatValue *lhs_float = lhs.value()->as<FloatValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value * rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value * rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::INT: {
                    const IntValue *lhs_int = lhs.value()->as<IntValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(APFloat(lhs_int->value) * rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<IntValue>(lhs_int->value * rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case TOK_DIV:
            switch (lhs.value()->get_variation()) {
                case Value::Variation::CHAR: {
                    const CharValue *lhs_char = lhs.value()->as<CharValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::CHAR: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = APInt(std::to_string(static_cast<uint32_t>(rhs.value()->as<CharValue>()->value)));
                            const APInt result_int = lhs_int / rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        case Value::Variation::INT: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = rhs.value()->as<IntValue>()->value;
                            const APInt result_int = lhs_int / rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::FLOAT: {
                    const FloatValue *lhs_float = lhs.value()->as<FloatValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value / rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value / rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::INT: {
                    const IntValue *lhs_int = lhs.value()->as<IntValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT: {
                            env.result = std::make_shared<FloatValue>(APFloat(lhs_int->value) / rhs.value()->as<FloatValue>()->value);
                            return true;
                        }
                        case Value::Variation::INT:
                            env.result = std::make_shared<IntValue>(lhs_int->value / rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case TOK_POW:
            switch (lhs.value()->get_variation()) {
                case Value::Variation::CHAR: {
                    const CharValue *lhs_char = lhs.value()->as<CharValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::CHAR: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = APInt(std::to_string(static_cast<uint32_t>(rhs.value()->as<CharValue>()->value)));
                            const APInt result_int = lhs_int ^ rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        case Value::Variation::INT: {
                            const APInt lhs_int = APInt(std::to_string(static_cast<uint32_t>(lhs_char->value)));
                            const APInt rhs_int = rhs.value()->as<IntValue>()->value;
                            const APInt result_int = lhs_int ^ rhs_int;
                            const auto result = result_int.to_uN<uint8_t>(                             //
                                binop->file_hash, PosTriple{binop->line, binop->column, binop->length} //
                            );
                            if (!result.has_value()) {
                                env.result = std::nullopt;
                                return false;
                            }
                            env.result = std::make_shared<CharValue>(result.value());
                            return true;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::FLOAT: {
                    const FloatValue *lhs_float = lhs.value()->as<FloatValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value ^ rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<FloatValue>(lhs_float->value ^ rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case Value::Variation::INT: {
                    const IntValue *lhs_int = lhs.value()->as<IntValue>();
                    switch (rhs.value()->get_variation()) {
                        case Value::Variation::FLOAT:
                            env.result = std::make_shared<FloatValue>(APFloat(lhs_int->value) ^ rhs.value()->as<FloatValue>()->value);
                            return true;
                        case Value::Variation::INT:
                            env.result = std::make_shared<IntValue>(lhs_int->value ^ rhs.value()->as<IntValue>()->value);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case TOK_AND:
            if (lhs.value()->get_variation() != Value::Variation::BOOL) {
                env.result = std::nullopt;
                return false;
            }
            if (rhs.value()->get_variation() != Value::Variation::BOOL) {
                env.result = std::nullopt;
                return false;
            }
            env.result = std::make_shared<BoolValue>(lhs.value()->as<BoolValue>()->value && rhs.value()->as<BoolValue>()->value);
            return true;
        case TOK_OR:
            if (lhs.value()->get_variation() != Value::Variation::BOOL) {
                env.result = std::nullopt;
                return false;
            }
            if (rhs.value()->get_variation() != Value::Variation::BOOL) {
                env.result = std::nullopt;
                return false;
            }
            env.result = std::make_shared<BoolValue>(lhs.value()->as<BoolValue>()->value || rhs.value()->as<BoolValue>()->value);
            return true;
        case TOK_EQUAL_EQUAL:
        case TOK_NOT_EQUAL:
        case TOK_LESS:
        case TOK_LESS_EQUAL:
        case TOK_GREATER:
        case TOK_GREATER_EQUAL: {
            const Value::Variation lhs_var = lhs.value()->get_variation();
            const Value::Variation rhs_var = rhs.value()->get_variation();
            if (lhs_var == Value::Variation::STR && rhs_var == Value::Variation::STR) {
                const std::string &lhs_str = lhs.value()->as<StrValue>()->value;
                const std::string &rhs_str = rhs.value()->as<StrValue>()->value;
                switch (binop->operator_token) {
                    case TOK_EQUAL_EQUAL:
                        env.result = std::make_shared<BoolValue>(lhs_str == rhs_str);
                        return true;
                    case TOK_NOT_EQUAL:
                        env.result = std::make_shared<BoolValue>(lhs_str != rhs_str);
                        return true;
                    case TOK_LESS:
                        env.result = std::make_shared<BoolValue>(lhs_str < rhs_str);
                        return true;
                    case TOK_LESS_EQUAL:
                        env.result = std::make_shared<BoolValue>(lhs_str <= rhs_str);
                        return true;
                    case TOK_GREATER:
                        env.result = std::make_shared<BoolValue>(lhs_str > rhs_str);
                        return true;
                    case TOK_GREATER_EQUAL:
                        env.result = std::make_shared<BoolValue>(lhs_str >= rhs_str);
                        return true;
                    default:
                        break;
                }
                UNREACHABLE();
            }

            if (lhs_var == Value::Variation::BOOL && rhs_var == Value::Variation::BOOL) {
                const bool lhs_bool = lhs.value()->as<BoolValue>()->value;
                const bool rhs_bool = rhs.value()->as<BoolValue>()->value;
                switch (binop->operator_token) {
                    case TOK_EQUAL_EQUAL:
                        env.result = std::make_shared<BoolValue>(lhs_bool == rhs_bool);
                        return true;
                    case TOK_NOT_EQUAL:
                        env.result = std::make_shared<BoolValue>(lhs_bool != rhs_bool);
                        return true;
                    default:
                        break;
                }
                UNREACHABLE();
            }

            APFloat lhs_float = APFloat("0");
            switch (lhs_var) {
                case Value::Variation::CHAR:
                    lhs_float = APFloat(std::to_string(static_cast<uint32_t>(lhs.value()->as<CharValue>()->value)));
                    break;
                case Value::Variation::FLOAT:
                    lhs_float = lhs.value()->as<FloatValue>()->value;
                    break;
                case Value::Variation::INT:
                    lhs_float = APFloat(lhs.value()->as<IntValue>()->value);
                    break;
                default:
                    UNREACHABLE();
            }
            APFloat rhs_float = APFloat("0");
            switch (rhs_var) {
                case Value::Variation::CHAR:
                    rhs_float = APFloat(std::to_string(static_cast<uint32_t>(rhs.value()->as<CharValue>()->value)));
                    break;
                case Value::Variation::FLOAT:
                    rhs_float = rhs.value()->as<FloatValue>()->value;
                    break;
                case Value::Variation::INT:
                    rhs_float = APFloat(rhs.value()->as<IntValue>()->value);
                    break;
                default:
                    UNREACHABLE();
            }
            const APFloat diff = lhs_float - rhs_float;
            const bool is_zero = diff.digits.size() == 1 && diff.digits[0] == 0;
            const bool is_less = !is_zero && diff.is_negative;
            switch (binop->operator_token) {
                case TOK_EQUAL_EQUAL:
                    env.result = std::make_shared<BoolValue>(is_zero);
                    return true;
                case TOK_NOT_EQUAL:
                    env.result = std::make_shared<BoolValue>(!is_zero);
                    return true;
                case TOK_LESS:
                    env.result = std::make_shared<BoolValue>(is_less);
                    return true;
                case TOK_LESS_EQUAL:
                    env.result = std::make_shared<BoolValue>(is_less || is_zero);
                    return true;
                case TOK_GREATER:
                    env.result = std::make_shared<BoolValue>(!is_zero && !is_less);
                    return true;
                case TOK_GREATER_EQUAL:
                    env.result = std::make_shared<BoolValue>(is_zero || !is_less);
                    return true;
                default:
                    break;
            }
            UNREACHABLE();
        }
    }
    env.result = std::nullopt;
    return true;
}

bool Evaluator::eval_literal([[maybe_unused]] Env &env, LiteralNode *const literal) {
    if (std::holds_alternative<LitEnum>(literal->value)) {
        env.result = std::nullopt;
        return false;
    } else if (std::holds_alternative<LitError>(literal->value)) {
        env.result = std::nullopt;
        return false;
    } else if (std::holds_alternative<LitVariantTag>(literal->value)) {
        env.result = std::nullopt;
        return false;
    } else if (std::holds_alternative<LitVariant>(literal->value)) {
        env.result = std::nullopt;
        return false;
    } else if (std::holds_alternative<LitOptional>(literal->value)) {
        env.result = std::nullopt;
        return false;
    } else if (std::holds_alternative<LitPtr>(literal->value)) {
        env.result = std::nullopt;
        return false;
    } else if (std::holds_alternative<LitInt>(literal->value)) {
        const auto &lit = std::get<LitInt>(literal->value);
        env.result = std::make_shared<IntValue>(lit.value);
        return true;
    } else if (std::holds_alternative<LitFloat>(literal->value)) {
        const auto &lit = std::get<LitFloat>(literal->value);
        env.result = std::make_shared<FloatValue>(lit.value);
        return true;
    } else if (std::holds_alternative<LitU8>(literal->value)) {
        const auto &lit = std::get<LitU8>(literal->value);
        env.result = std::make_shared<CharValue>(lit.value);
        return true;
    } else if (std::holds_alternative<LitBool>(literal->value)) {
        const auto &lit = std::get<LitBool>(literal->value);
        env.result = std::make_shared<BoolValue>(lit.value);
        return true;
    } else if (std::holds_alternative<LitStr>(literal->value)) {
        const auto &lit = std::get<LitStr>(literal->value);
        env.result = std::make_shared<StrValue>(lit.value);
        return true;
    } else {
        UNREACHABLE();
    }
}
