#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/builtins.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "matcher/matcher.hpp"
#include "parser/ap_float.hpp"
#include "parser/parser.hpp"

#include "error/error_types/parsing/unexpected/err_pars_unexpected_token.hpp"
#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/callable_call_node_expression.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/function_reference_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/inline_array_initializer_node.hpp"
#include "parser/ast/expressions/instance_call_node_expression.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/optional_unwrap_node.hpp"
#include "parser/ast/expressions/range_expression_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/type_node.hpp"
#include "parser/ast/expressions/variant_unwrap_node.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/entity_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/variant_type.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <variant>

bool Parser::check_castability(std::unique_ptr<ExpressionNode> &lhs, std::unique_ptr<ExpressionNode> &rhs) {
    PROFILE_CUMULATIVE("Parser::check_castability_expr");
    if (lhs->type->equals(rhs->type)) {
        return true;
    }
    const CastDirection castability = check_castability(lhs->type, rhs->type);
    switch (castability.kind) {
        case CastDirection::Kind::NOT_CASTABLE: {
            return false;
        }
        case CastDirection::Kind::SAME_TYPE: {
            return true;
        }
        case CastDirection::Kind::CAST_LHS_TO_RHS: {
            return check_castability(rhs->type, lhs);
        }
        case CastDirection::Kind::CAST_BIDIRECTIONAL:
        case CastDirection::Kind::CAST_RHS_TO_LHS: {
            return check_castability(lhs->type, rhs);
        }
        case CastDirection::Kind::CAST_BOTH_TO_COMMON: {
            if (!check_castability(castability.common_type, lhs, false)) {
                return false;
            }
            if (!check_castability(castability.common_type, rhs, false)) {
                return false;
            }
            return true;
        }
    }
    // Should never reach here
    UNREACHABLE();
    return false;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::check_const_folding( //
    std::unique_ptr<ExpressionNode> &lhs,                                   //
    const Token operation,                                                  //
    std::unique_ptr<ExpressionNode> &rhs                                    //
) {
    PROFILE_CUMULATIVE("Parser::check_const_folding");
    // Currently, only literals can be const folded
    const bool is_lhs_not_lit = lhs->get_variation() != ExpressionNode::Variation::LITERAL;
    const bool is_rhs_not_lit = rhs->get_variation() != ExpressionNode::Variation::LITERAL;
    if (is_lhs_not_lit || is_rhs_not_lit) {
        return std::nullopt;
    }

    // Const folding can only be applied if the binary operator is an arithmetic operation
    if (!Matcher::token_match(operation, Matcher::operational_binop) && !Matcher::token_match(operation, Matcher::boolean_binop)) {
        return std::nullopt;
    }

    // Add the two literals together
    const auto *lhs_lit = lhs->as<LiteralNode>();
    const auto *rhs_lit = rhs->as<LiteralNode>();
    std::optional<std::unique_ptr<LiteralNode>> result = add_literals(lhs_lit, operation, rhs_lit);
    if (!result.has_value()) {
        return std::nullopt;
    }

    // Make sure to actually erase the lhs and rhs pointers to prevent memory leaks
    lhs.reset();
    rhs.reset();
    return result;
}

std::optional<std::unique_ptr<LiteralNode>> Parser::add_literals( //
    const LiteralNode *lhs,                                       //
    const Token operation,                                        //
    const LiteralNode *rhs                                        //
) {
    PROFILE_CUMULATIVE("Parser::add_literals");
    const auto pos_triple = ASTNode::PosTriple{
        .line = lhs->line,
        .column = lhs->column,
        .length = (rhs->column - lhs->column) + rhs->length,
    };
    switch (operation) {
        default:
            // It should never come here, if it did something went wrong
            UNREACHABLE();
            break;
        case TOK_PLUS:
            if (std::holds_alternative<LitInt>(lhs->value)) {
                const APInt lhs_int = std::get<LitInt>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    rhs_float += lhs_int;
                    LitValue lit_value = LitFloat{.value = rhs_float};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    rhs_int += lhs_int;
                    LitValue lit_value = LitInt{.value = rhs_int};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            } else if (std::holds_alternative<LitFloat>(lhs->value)) {
                APFloat lhs_float = std::get<LitFloat>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    const APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    lhs_float += rhs_float;
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    const APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    lhs_float += rhs_int;
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                LitValue lit_value = LitFloat{.value = lhs_float};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            } else if (std::holds_alternative<LitStr>(lhs->value)) {
                const std::string new_lit = std::get<LitStr>(lhs->value).value + std::get<LitStr>(rhs->value).value;
                LitValue lit_value = LitStr{.value = new_lit};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value + std::get<LitU8>(rhs->value).value;
                LitValue lit_value = LitU8{.value = new_lit};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            }
            break;
        case TOK_MINUS:
            if (std::holds_alternative<LitInt>(lhs->value)) {
                const APInt lhs_int = std::get<LitInt>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    rhs_float -= lhs_int;
                    LitValue lit_value = LitFloat{.value = rhs_float};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    rhs_int -= lhs_int;
                    LitValue lit_value = LitInt{.value = rhs_int};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            } else if (std::holds_alternative<LitFloat>(lhs->value)) {
                APFloat lhs_float = std::get<LitFloat>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    const APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    lhs_float -= rhs_float;
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    const APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    lhs_float -= rhs_int;
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                LitValue lit_value = LitFloat{.value = lhs_float};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value - std::get<LitU8>(rhs->value).value;
                LitValue lit_value = LitU8{.value = new_lit};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            }
            break;
        case TOK_MULT:
            if (std::holds_alternative<LitInt>(lhs->value)) {
                const APInt lhs_int = std::get<LitInt>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    rhs_float *= lhs_int;
                    LitValue lit_value = LitFloat{.value = rhs_float};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    rhs_int *= lhs_int;
                    LitValue lit_value = LitInt{.value = rhs_int};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            } else if (std::holds_alternative<LitFloat>(lhs->value)) {
                APFloat lhs_float = std::get<LitFloat>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    const APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    lhs_float *= rhs_float;
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    const APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    lhs_float *= rhs_int;
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                LitValue lit_value = LitFloat{.value = lhs_float};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value * std::get<LitU8>(rhs->value).value;
                LitValue lit_value = LitU8{.value = new_lit};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            }
            break;
        case TOK_DIV:
            if (std::holds_alternative<LitInt>(lhs->value)) {
                APInt lhs_int = std::get<LitInt>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    const APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    APFloat lhs_float = APFloat(lhs_int);
                    lhs_float /= rhs_float;
                    LitValue lit_value = LitFloat{.value = lhs_float};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    const APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    lhs_int /= rhs_int;
                    LitValue lit_value = LitInt{.value = lhs_int};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            } else if (std::holds_alternative<LitFloat>(lhs->value)) {
                APFloat lhs_float = std::get<LitFloat>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    const APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    lhs_float /= rhs_float;
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    const APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    lhs_float /= rhs_int;
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                LitValue lit_value = LitFloat{.value = lhs_float};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value / std::get<LitU8>(rhs->value).value;
                LitValue lit_value = LitU8{.value = new_lit};
                return std::make_unique<LiteralNode>( //
                    lhs->file_hash, pos_triple,       //
                    lit_value, rhs->type, true        //
                );
            }
            break;
        case TOK_POW:
            if (std::holds_alternative<LitInt>(lhs->value)) {
                APInt lhs_int = std::get<LitInt>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    const APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    APFloat lhs_float = APFloat(lhs_int);
                    lhs_float ^= rhs_float;
                    LitValue lit_value = LitFloat{.value = lhs_float};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    const APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    lhs_int ^= rhs_int;
                    LitValue lit_value = LitInt{.value = lhs_int};
                    return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            } else if (std::holds_alternative<LitFloat>(lhs->value)) {
                APFloat lhs_float = std::get<LitFloat>(lhs->value).value;
                if (std::holds_alternative<LitFloat>(rhs->value)) {
                    const APFloat rhs_float = std::get<LitFloat>(rhs->value).value;
                    lhs_float ^= rhs_float;
                } else if (std::holds_alternative<LitInt>(rhs->value)) {
                    const APInt rhs_int = std::get<LitInt>(rhs->value).value;
                    lhs_float ^= rhs_int;
                } else {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                LitValue lit_value = LitFloat{.value = lhs_float};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = static_cast<char>(std::pow(std::get<LitU8>(lhs->value).value, std::get<LitU8>(rhs->value).value));
                LitValue lit_value = LitU8{.value = new_lit};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            }
            break;
        case TOK_AND:
            if (std::holds_alternative<LitBool>(lhs->value)) {
                const bool new_lit = std::get<LitBool>(lhs->value).value && std::get<LitBool>(rhs->value).value;
                LitValue lit_value = LitBool{.value = new_lit};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            }
            break;
        case TOK_OR:
            if (std::holds_alternative<LitBool>(lhs->value)) {
                const bool new_lit = std::get<LitBool>(lhs->value).value || std::get<LitBool>(rhs->value).value;
                LitValue lit_value = LitBool{.value = new_lit};
                return std::make_unique<LiteralNode>(lhs->file_hash, pos_triple, lit_value, rhs->type, true);
            }
            break;
    }
    return std::nullopt;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_variable(std::shared_ptr<Scope> &scope, const token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::create_variable");
    for (auto tok = tokens.first; tok != tokens.second; tok++) {
        if (tok->token == TOK_IDENTIFIER) {
            const std::string name(tok->lexme);
            if (scope->variables.find(name) != scope->variables.end()) {
                return std::make_unique<VariableNode>(                                                                             //
                    file_hash, get_pos_triple(tokens), name, scope->variables.at(name).type, !scope->variables.at(name).is_mutable //
                );
            }
            if (scope->captured_entity_identifiers.find(name) == scope->captured_entity_identifiers.end()) {
                THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_hash, tok->line, tok->column, name);
                return std::nullopt;
            }
            const auto &captured_type = scope->captured_entity_identifiers.at(name);
            switch (captured_type->get_variation()) {
                default:
                    UNREACHABLE();
                    return std::nullopt;
                case Type::Variation::DATA: {
                    ASSERT(scope->variables.find("self") != scope->variables.end());
                    const auto &self = scope->variables.at("self");
                    ASSERT(self.type->get_variation() == Type::Variation::ENTITY);
                    const EntityNode *entity_node = self.type->as<EntityType>()->entity_node;
                    const DataNode *required_data_node = captured_type->as<DataType>()->data_node;
                    size_t idx = 0;
                    for (const auto &[data_node, accessor] : entity_node->data_modules) {
                        if (data_node == required_data_node) {
                            break;
                        }
                        idx++;
                    }
                    if (idx == entity_node->data_modules.size()) {
                        // The data node is not present in the entity type
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    std::unique_ptr<ExpressionNode> base_expr = std::make_unique<VariableNode>( //
                        file_hash, get_pos_triple(tokens), "self", self.type, false             //
                    );
                    std::unique_ptr<ExpressionNode> access = std::make_unique<DataAccessNode>( //
                        file_hash, get_pos_triple(tokens),                                     //
                        base_expr,                                                             //
                        std::nullopt,                                                          // Entity fields have no name
                        idx,                                                                   // The index of the data in the entity struct
                        captured_type                                                          //
                    );
                    return std::move(access);
                }
                case Type::Variation::ENTITY:
                    ASSERT(scope->variables.find("self") != scope->variables.end());
                    const auto &self = scope->variables.at("self");
                    ASSERT(self.type->get_variation() == Type::Variation::ENTITY);
                    // Store the name of the parent accessor in the variable, it will be changed to `self` later in the
                    // `create_field_access_base` function. We do this in order to be able to tell which parent was accessed in the
                    // `create_field_access_base` function.
                    return std::make_unique<VariableNode>(file_hash, get_pos_triple(tokens), name, self.type, false);
            }
        }
    }
    return std::nullopt;
}

std::optional<UnaryOpExpression> Parser::create_unary_op_expression( //
    const Context &ctx,                                              //
    std::shared_ptr<Scope> &scope,                                   //
    const token_slice &tokens                                        //
) {
    PROFILE_CUMULATIVE("Parser::create_unary_op_expression");
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    auto unary_op_values = create_unary_op_base(ctx, scope, tokens_mut);
    if (!unary_op_values.has_value()) {
        return std::nullopt;
    }
    Token operation = unary_op_values.value().operation;
    std::unique_ptr<ExpressionNode> &base_expr = unary_op_values.value().base_expr;
    bool is_left = unary_op_values.value().is_left;
    UnaryOpExpression un_op(file_hash, get_pos_triple(tokens), operation, base_expr, is_left);
    if (operation == TOK_EXCLAMATION) {
        if (is_left) {
            // The ! operator is only allowed on the right of the expression
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (un_op.type->get_variation() != Type::Variation::OPTIONAL) {
            // The post ! operator is only allowed on optional values
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const auto *optional_type = un_op.type->as<OptionalType>();
        if (un_op.operand->get_variation() != ExpressionNode::Variation::VARIABLE) {
            // Optional unwrapping is only allowed on variables for now
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return std::nullopt;
        }
        // Set the type of the unary op to the base type of the optional, as the unwrap will return the base type of it
        un_op.type = optional_type->base_type;
    } else if (operation == TOK_BIT_AND) {
        // Reference of operator, the result will be a pointer type of the expression's type
        if (!is_left) {
            // The & operator is only allowed on the left of the expression
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // Becaues any type could be / become a pointer type we don't really need to check the expression itself every type is able to be
        // pointed to. The only thing we need to check for is to prevent double pointers, they don't make any sense in the context of Flint
        // at least.
        if (un_op.type->get_variation() == Type::Variation::POINTER) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // & on a dynamic array T[] produces T* (pointer to the element data section), not T[]*
        std::shared_ptr<Type> ptr_type;
        if (un_op.type->get_variation() == Type::Variation::ARRAY && !un_op.type->as<ArrayType>()->sizes.has_value()) {
            ptr_type = std::make_shared<PointerType>(un_op.type->as<ArrayType>()->type);
        } else {
            ptr_type = std::make_shared<PointerType>(un_op.type);
        }
        if (!file_node_ptr->file_namespace->add_type(ptr_type)) {
            ptr_type = file_node_ptr->file_namespace->get_type_from_str(ptr_type->to_string()).value();
        }
        un_op.type = ptr_type;
    }
    return un_op;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_anonymous_error( //
    const Context &ctx,                                                        //
    std::shared_ptr<Scope> &scope,                                             //
    const token_slice &tokens                                                  //
) {
    PROFILE_CUMULATIVE("Parser::create_anonymous_error");
    ASSERT(tokens.first->token == TOK_ERROR);
    ASSERT(std::next(tokens.first)->token == TOK_DOT);
    ASSERT((tokens.first + 2)->token == TOK_IDENTIFIER);
    std::string err_type_name = "error.";
    if (std::holds_alternative<FunctionNode *>(scope->function)) {
        err_type_name += std::to_string(std::get<FunctionNode *>(scope->function)->get_id());
    } else {
        err_type_name += "test." + std::to_string(std::get<TestNode *>(scope->function)->test_id);
    }
    const auto error_type = file_node_ptr->file_namespace->get_type_from_str(err_type_name).value();
    const std::string err_value((tokens.first + 2)->lexme);
    std::optional<std::unique_ptr<ExpressionNode>> message;
    if (tokens.first + 3 != tokens.second) {
        ASSERT(std::prev(tokens.second)->token == TOK_RIGHT_PAREN);
        ASSERT((tokens.first + 3)->token == TOK_LEFT_PAREN);
        ASSERT(std::prev(tokens.second)->token == TOK_RIGHT_PAREN);
        if (tokens.first + 4 == tokens.second - 1) {
            // Empty expression between parenthesis of throwing the error
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const token_slice message_tokens = {tokens.first + 4, tokens.second - 1};
        message = create_expression(ctx, scope, message_tokens, Type::get_primitive_type("str"));
        if (!message.has_value()) {
            return std::nullopt;
        }
    }
    LitValue literal = LitError{
        .error_type = error_type,
        .value = err_value,
        .message = std::move(message),
    };
    return std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), literal, error_type);
}

std::optional<LiteralNode> Parser::create_literal(const token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::create_literal");
    // Literals can have a size of at most 2 tokens
    if (get_slice_size(tokens) > 2) {
        return std::nullopt;
    }
    // If the tokens are 2 long we have a literal expression
    Token front_token = TOK_EOF;
    token_list::iterator tok = tokens.first;
    if (get_slice_size(tokens) == 2) {
        // Currently the only literal experssion is a minus sign in front of the literal, or a $ sign in front of the string
        if (tokens.first->token == TOK_MINUS) {
            front_token = TOK_MINUS;
        } else if (tokens.first->token == TOK_DOLLAR) {
            front_token = TOK_DOLLAR;
        } else if (tokens.first->token == TOK_NOT) {
            front_token = TOK_NOT;
        } else {
            THROW_ERR(                                                                                 //
                ErrParsUnexpectedToken, ERR_PARSING, file_hash,                                        //
                tok->line, tok->column, std::vector<Token>{TOK_MINUS, TOK_DOLLAR, TOK_NOT}, tok->token //
            );
            return std::nullopt;
        }
        tok++;
    }

    std::string lexme(tok->lexme);
    if (tok->token == TOK_FLOAT_VALUE || tok->token == TOK_INT_VALUE) {
        // Erase all '_' characters from the literal
        lexme.erase(std::remove(lexme.begin(), lexme.end(), '_'), lexme.end());
    }
    if (Matcher::tokens_match({tok, tok + 1}, Matcher::literal)) {
        switch (tok->token) {
            default:
                // As long as the pattern of the literal is added in the Matcher::literal pattern this branch actually is unreachable
                // because if it would be reached it would mean that something about the Matcher went wrong, which is not a user error but a
                // dev error
                UNREACHABLE();
                break;
            case TOK_NONE: {
                std::shared_ptr<Type> void_type = Type::get_primitive_type("void");
                std::optional<std::shared_ptr<Type>> opt_type = file_node_ptr->file_namespace->get_type_from_str("void?");
                ASSERT(opt_type.has_value());
                LitValue lit_val = LitOptional{};
                return LiteralNode(file_hash, get_pos_triple(tokens), lit_val, opt_type.value());
            }
            case TOK_NULL: {
                std::shared_ptr<Type> void_type = Type::get_primitive_type("void");
                std::optional<std::shared_ptr<Type>> ptr_type = file_node_ptr->file_namespace->get_type_from_str("void*");
                ASSERT(ptr_type.has_value());
                LitValue lit_val = LitPtr{};
                return LiteralNode(file_hash, get_pos_triple(tokens), lit_val, ptr_type.value());
            }
            case TOK_INT_VALUE: {
                APInt lit_int = APInt(lexme);
                lit_int.is_negative = front_token == TOK_MINUS;
                LitValue lit_val = LitInt{.value = lit_int};
                return LiteralNode(file_hash, get_pos_triple(tokens), lit_val,
                    file_node_ptr->file_namespace->get_type_from_str("int").value());
            }
            case TOK_FLOAT_VALUE: {
                APFloat lit_float = APFloat(lexme);
                lit_float.is_negative = front_token == TOK_MINUS;
                LitValue lit_val = LitFloat{.value = lit_float};
                return LiteralNode(file_hash, get_pos_triple(tokens), lit_val,
                    file_node_ptr->file_namespace->get_type_from_str("float").value());
            }
            case TOK_STR_VALUE: {
                size_t pos = 0;
                while ((pos = lexme.find("\\\"", pos)) != std::string::npos) {
                    lexme.replace(pos, 2, "\"");
                }
                if (front_token == TOK_DOLLAR) {
                    LitValue lit_value = LitStr{.value = lexme};
                    return LiteralNode(file_hash, get_pos_triple(tokens), lit_value, Type::get_primitive_type("str"));
                } else {
                    const std::string &str = lexme;
                    std::stringstream processed_str;
                    for (unsigned int i = 0; i < str.length(); i++) {
                        if (str[i] == '\\' && i + 1 < str.length()) {
                            switch (str[i + 1]) {
                                case 'n':
                                    processed_str << '\n';
                                    break;
                                case 't':
                                    processed_str << '\t';
                                    break;
                                case 'r':
                                    processed_str << '\r';
                                    break;
                                case '\\':
                                    processed_str << '\\';
                                    break;
                                case '{':
                                    processed_str << '{';
                                    break;
                                case '}':
                                    processed_str << '}';
                                    break;
                                case '0':
                                    processed_str << '\0';
                                    break;
                                case 'x': {
                                    // Hex value follows
                                    if ((i + 3) >= str.length()) {
                                        THROW_BASIC_ERR(ERR_PARSING);
                                        return std::nullopt;
                                    }
                                    std::string hex_digits = str.substr(i + 2, 2);
                                    int hex_value = std::stoi(hex_digits, nullptr, 16);
                                    processed_str << static_cast<char>(hex_value);
                                    i += 2; // Skip the two hex digits
                                    break;
                                }
                                default:
                                    THROW_BASIC_ERR(ERR_PARSING);
                                    return std::nullopt;
                            }
                            i++; // Skip the next character
                        } else {
                            processed_str << str[i];
                        }
                    }
                    LitValue lit_value = LitStr{.value = processed_str.str()};
                    return LiteralNode(file_hash, get_pos_triple(tokens), lit_value, Type::get_primitive_type("type.flint.str.lit"));
                }
            }
            case TOK_TRUE: {
                LitValue lit_value = LitBool{.value = front_token != TOK_NOT};
                return LiteralNode(file_hash, get_pos_triple(tokens), lit_value, Type::get_primitive_type("bool"));
            }
            case TOK_FALSE: {
                LitValue lit_value = LitBool{.value = front_token == TOK_NOT};
                return LiteralNode(file_hash, get_pos_triple(tokens), lit_value, Type::get_primitive_type("bool"));
            }
            case TOK_CHAR_VALUE: {
                char char_value = lexme[0];
                // Handle special cases
                if (lexme == "\\n") {
                    char_value = '\n';
                } else if (lexme == "\\t") {
                    char_value = '\t';
                } else if (lexme == "\\r") {
                    char_value = '\r';
                } else if (lexme == "\\\\") {
                    char_value = '\\';
                } else if (lexme == "\\0") {
                    char_value = '\0';
                } else if (lexme == "\\'") {
                    char_value = '\'';
                } else if (lexme.substr(0, 2) == "\\x") {
                    ASSERT(lexme.size() == 4);
                    const std::string hex_digits = lexme.substr(2, 2);
                    unsigned int hex_value = std::stoi(hex_digits, nullptr, 16);
                    char_value = static_cast<char>(hex_value);
                }
                LitValue lit_value = LitU8{.value = char_value};
                return LiteralNode(file_hash, get_pos_triple(tokens), lit_value, Type::get_primitive_type("u8"));
            }
        }
    }
    return std::nullopt;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_string_interpolation( //
    const Context &ctx,                                                             //
    std::shared_ptr<Scope> &scope,                                                  //
    const std::string &interpol_string,                                             //
    const token_slice &tokens                                                       //
) {
    PROFILE_CUMULATIVE("Parser::create_string_interpolation");
    // First, get all balanced ranges of { } symbols which are not leaded by a \\ symbol
    const auto &tok = std::prev(tokens.second);
    std::vector<uint2> ranges = Matcher::balanced_ranges_vec(interpol_string, "([^\\\\]|^)\\{", "[^\\\\]\\}");
    std::vector<std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<LiteralNode>>> interpol_content;
    // If the ranges are empty, the interpolation does not contain any groups
    if (ranges.empty()) {
        LitValue lit_value = LitStr{.value = interpol_string};
        const auto pos_triple = get_pos_triple(tokens);
        interpol_content.emplace_back(std::make_unique<LiteralNode>(file_hash, pos_triple, lit_value, Type::get_primitive_type("str")));
        return std::make_unique<StringInterpolationNode>(file_hash, pos_triple, interpol_content);
    }
    // First, add all the strings from the begin to the first ranges begin to the interpolation content
    for (auto it = ranges.begin(); it != ranges.end(); ++it) {
        // Check for empty expression: { and } are adjacent
        if (it->second - it->first <= 1) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }

        // Add string before first { or between } and {
        if (it == ranges.begin() && it->first > 0) {
            // Add string that's present before the first { symbol
            token_list lit_toks = {
                {TOK_STR_VALUE, tok->line, tok->column, tok->file_id, std::string_view(interpol_string.data(), it->first)} //
            };
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        } else if (it != ranges.begin() && it->first - std::prev(it)->second > 1) {
            // Add string in between } and { symbols
            size_t start_pos = std::prev(it)->second + 1; // Position after previous }
            size_t length = it->first - start_pos;        // Length until current {
            token_list lit_toks = {
                {TOK_STR_VALUE, tok->line, tok->column, tok->file_id, std::string_view(interpol_string.data() + start_pos, length)} //
            };
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        }

        // Extract the expression between { and }
        size_t expr_start = it->first + 1;               // Position after {
        size_t expr_length = it->second - it->first - 1; // Length between { and }
        const std::string expr_str = interpol_string.substr(expr_start, expr_length);
        Lexer lexer("", expr_str);
        token_list expr_tokens = lexer.scan();
        if (expr_tokens.empty()) {
            return std::nullopt;
        }
        for (auto &token : expr_tokens) {
            token.line = tok->line;
            token.column += tok->column + it->first + 1;
        }
        token_slice expr_slice = {expr_tokens.begin(), expr_tokens.end()};
        collapse_types_in_slice(expr_slice, expr_tokens);
        if (expr_tokens.back().token == TOK_EOF) {
            expr_slice.second--;
        }
        std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(ctx, scope, expr_slice);
        if (!expr.has_value()) {
            return std::nullopt;
        }
        // Cast every expression inside to a str type (if it isn't already)
        const std::shared_ptr<Type> str_type = Type::get_primitive_type("str");
        if (!check_castability(str_type, expr.value(), true)) {
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, expr_slice, str_type, expr.value()->type);
            return std::nullopt;
        }
        interpol_content.emplace_back(std::move(expr.value()));

        // Add string after last } symbol
        if (std::next(it) == ranges.end() && it->second + 1 < interpol_string.length()) {
            size_t start_pos = it->second + 1; // Position after }
            token_list lit_toks = {
                {TOK_STR_VALUE, tok->line, tok->column, tok->file_id, std::string_view(interpol_string.data() + start_pos)} //
            };
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        }
    }

    // Optimization: Collapse adjacent string literals and simplify if possible
    std::vector<std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<LiteralNode>>> optimized_content;
    std::string accumulated_string;
    bool has_accumulated = false;

    for (auto &elem : interpol_content) {
        bool is_str_literal = false;
        std::string literal_value;

        // Check if element is ExpressionNode or LiteralNode variant
        if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(elem)) {
            auto &expr = std::get<std::unique_ptr<ExpressionNode>>(elem);

            // Check if it's a type.flint.str.lit literal (after our int/float->str conversion)
            if (expr->type->to_string() == "type.flint.str.lit" && expr->get_variation() == ExpressionNode::Variation::LITERAL) {
                auto *lit = expr->as<LiteralNode>();
                const auto &lit_str = std::get<LitStr>(lit->value);
                literal_value = lit_str.value;
                is_str_literal = true;
            }
            // Check if it's a TypeCast wrapping a type.flint.str.lit
            else if (expr->get_variation() == ExpressionNode::Variation::TYPE_CAST) {
                auto *cast = expr->as<TypeCastNode>();
                if (cast->expr->type->to_string() == "type.flint.str.lit" &&
                    cast->expr->get_variation() == ExpressionNode::Variation::LITERAL) {
                    auto *lit = cast->expr->as<LiteralNode>();
                    const auto &lit_str = std::get<LitStr>(lit->value);
                    literal_value = lit_str.value;
                    is_str_literal = true;
                }
            }
        } else { // std::unique_ptr<LiteralNode>
            auto &lit_ptr = std::get<std::unique_ptr<LiteralNode>>(elem);
            if (lit_ptr->type->to_string() == "type.flint.str.lit" || lit_ptr->type->to_string() == "str") {
                const auto &lit_str = std::get<LitStr>(lit_ptr->value);
                literal_value = lit_str.value;
                is_str_literal = true;
            }
        }

        if (is_str_literal) {
            // Accumulate string literals
            accumulated_string += literal_value;
            has_accumulated = true;
        } else {
            // Non-literal expression: flush accumulated strings first
            if (has_accumulated) {
                LitValue str_val = LitStr{accumulated_string};
                auto lit_node = std::make_unique<LiteralNode>(                                                 //
                    file_hash, get_pos_triple(tokens), str_val, Type::get_primitive_type("type.flint.str.lit") //
                );
                optimized_content.emplace_back(std::move(lit_node));
                accumulated_string.clear();
                has_accumulated = false;
            }
            // Add the non-literal expression
            optimized_content.emplace_back(std::move(elem));
        }
    }

    // Flush any remaining accumulated strings
    if (has_accumulated) {
        LitValue str_val = LitStr{accumulated_string};
        auto lit_node = std::make_unique<LiteralNode>(                                                 //
            file_hash, get_pos_triple(tokens), str_val, Type::get_primitive_type("type.flint.str.lit") //
        );
        optimized_content.emplace_back(std::move(lit_node));
    }

    // If the result is a single string literal, cast to str and return directly
    if (optimized_content.size() == 1) {
        const std::shared_ptr<Type> str_type = Type::get_primitive_type("str");

        if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(optimized_content[0])) {
            auto expr = std::move(std::get<std::unique_ptr<ExpressionNode>>(optimized_content[0]));
            if (expr->type->to_string() == "type.flint.str.lit") {
                check_castability(str_type, expr, true);
                return expr;
            } else {
                // Interpolating only a single expression like `$"{val}"` is not allowed, you should use `str(val)` instead
                THROW_ERR(ErrExprInterpolationOnlyOneExpr, ERR_PARSING, file_hash, tokens);
                return std::nullopt;
            }
        } else {
            auto lit = std::move(std::get<std::unique_ptr<LiteralNode>>(optimized_content[0]));
            if (lit->type->to_string() == "type.flint.str.lit") {
                std::unique_ptr<ExpressionNode> expr = std::make_unique<LiteralNode>(std::move(*lit));
                check_castability(str_type, expr, true);
                return expr;
            }
        }
    }

    // Otherwise, return string interpolation with optimized content
    return std::make_unique<StringInterpolationNode>(file_hash, get_pos_triple(tokens), optimized_content);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_call_expression( //
    const Context &ctx,                                                        //
    std::shared_ptr<Scope> &scope,                                             //
    const token_slice &tokens,                                                 //
    const std::optional<Namespace *> &alias,                                   //
    const bool is_func_module_call                                             //
) {
    PROFILE_CUMULATIVE("Parser::create_call_expression");
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    std::optional<CreateCallOrInitializerBaseRet> ret = std::nullopt;
    if (alias.has_value()) {
        ret = create_call_or_initializer_base(ctx, scope, tokens_mut, alias.value(), is_func_module_call);
    } else {
        ret = create_call_or_initializer_base(ctx, scope, tokens_mut, file_node_ptr->file_namespace.get(), is_func_module_call);
    }
    if (!ret.has_value()) {
        return std::nullopt;
    }
    ASSERT(!ret->is_initializer);
    if (ret->instance_variable.has_value()) {
        ASSERT(ret->instance_variable.value()->get_variation() == ExpressionNode::Variation::VARIABLE);
        const VariableNode *instance_var = ret->instance_variable.value()->as<VariableNode>();
        if (scope->variables.find(instance_var->name) == scope->variables.end()) {
            // Instance call on nonexistent instance variable
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (!scope->variables.at(instance_var->name).is_mutable && !ret->function->is_const) {
            // Instance calls on constant instance variables are not allowed
            THROW_ERR(ErrExprCallOnConstInstance, ERR_PARSING, file_hash, tokens.first->line, tokens.first->column, instance_var->name);
            return std::nullopt;
        }
        std::unique_ptr<InstanceCallNodeExpression> instance_call_node = std::make_unique<InstanceCallNodeExpression>( //
            file_hash,                                                                                                 //
            get_pos_triple(tokens),                                                                                    //
            ret->function,                                                                                             //
            ret->args,                                                                                                 //
            ret->function->error_types,                                                                                //
            ret->type,                                                                                                 //
            ret->instance_variable.value()                                                                             //
        );
        instance_call_node->scope_id = scope->scope_id;
        last_parsed_call = instance_call_node.get();
        return std::move(instance_call_node);
    } else if (ret->callable.has_value()) {
        const auto &error_types = scope->variables.at(ret->callable.value()).type->as<FnType>()->error_types;
        std::unique_ptr<CallableCallNodeExpression> callable_call_node = std::make_unique<CallableCallNodeExpression>( //
            file_hash,                                                                                                 //
            get_pos_triple(tokens),                                                                                    //
            ret->args,                                                                                                 //
            error_types,                                                                                               //
            ret->type,                                                                                                 //
            ret->callable.value()                                                                                      //
        );
        callable_call_node->scope_id = scope->scope_id;
        last_parsed_call = callable_call_node.get();
        return std::move(callable_call_node);
    } else {
        std::unique_ptr<CallNodeExpression> simple_call_node = std::make_unique<CallNodeExpression>( //
            file_hash,                                                                               //
            get_pos_triple(tokens),                                                                  //
            ret->function,                                                                           //
            ret->args,                                                                               //
            ret->function->error_types,                                                              //
            ret->type                                                                                //
        );
        simple_call_node->scope_id = scope->scope_id;
        last_parsed_call = simple_call_node.get();
        return std::move(simple_call_node);
    }
}

std::optional<std::unique_ptr<FunctionReferenceNode>> Parser::create_function_reference(const token_slice &tokens) {
    // If the first token is a type then it's a func module's or entities' function reference, so we need to search for the referenced
    // function within that func module / entity type
    token_slice tokens_mut = tokens;
    std::string referenced_fn_name = "";
    if (tokens.first->token == TOK_TYPE) {
        switch (tokens.first->type->get_variation()) {
            default:
                // Referencing functions is only allowed when referencing functions of func modules or entities (yet)
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            case Type::Variation::ENTITY:
                [[fallthrough]];
            case Type::Variation::FUNC:
                referenced_fn_name = tokens.first->type->to_string() + ".";
                tokens_mut.first++;
                break;
        }
    }

    // TODO: Support aliased func-module types before the reference operator (`alias.FuncType::function`)

    // TODO: If the first token is an identifier, then that identifier is an alias of another file, so we need to search for the function
    // within that aliased imported file

    // If the first token is the function reference itself we need to search in the current file for a "regular" function to reference
    ASSERT(tokens_mut.first->token == TOK_REFERENCE);
    ASSERT((tokens_mut.first + 1)->token == TOK_IDENTIFIER);
    referenced_fn_name += std::string((tokens_mut.first + 1)->lexme);
    std::vector<const FunctionNode *> potential_functions = file_node_ptr->file_namespace->get_functions_with_name( //
        referenced_fn_name, false, true                                                                             //
    );

    if (potential_functions.empty()) {
        // Try to get all potential functions again and include all functions from all Core modules this time. If we now found a function it
        // means that a Core module function has been referenced, which is not allowed. If still no functions have been found then the
        // referenced function simply does not exist
        potential_functions = file_node_ptr->file_namespace->get_functions_with_name(referenced_fn_name, false, false);
        if (potential_functions.empty()) {
            // No function found with this name in this namespace
            THROW_ERR(ErrExprFnRefNonexistent, ERR_PARSING, file_hash, tokens, referenced_fn_name);
            return std::nullopt;
        }
        // The reference targets a Core module function, this is not allowed
        THROW_ERR(ErrExprFnRefCore, ERR_PARSING, file_hash, tokens, referenced_fn_name);
        return std::nullopt;
    }

    // Check if the function's name exists more than once.
    if (potential_functions.size() > 1) {
        // TODO: How to support function overloading here? I think overloading could be supported when the result-type of the fn variable is
        // known, so the `create_function_reference` function would need an additional parameter, being the expected type of the reference,
        // and if more overloads exist then we choose the one with the correct type. Yes, this could work nicely.
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    const FunctionNode *referenced_function = potential_functions.front();
    if (referenced_function->is_extern) {
        // It is not allowed to reference extern functions
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return std::make_unique<FunctionReferenceNode>(file_hash, get_pos_triple(tokens), referenced_function);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_initializer( //
    const Context &ctx,                                                    //
    std::shared_ptr<Scope> &scope,                                         //
    const token_slice &tokens                                              //
) {
    PROFILE_CUMULATIVE("Parser::create_initializer");
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    auto ret = create_call_or_initializer_base(ctx, scope, tokens_mut, file_node_ptr->file_namespace.get());
    if (!ret.has_value()) {
        return std::nullopt;
    }
    ASSERT(ret->is_initializer);
    std::vector<std::unique_ptr<ExpressionNode>> args;
    for (auto &arg : ret->args) {
        args.emplace_back(std::move(arg.first));
    }
    return std::make_unique<InitializerNode>(file_hash, get_pos_triple(tokens), ret->type, args);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_type_cast( //
    const Context &ctx,                                                  //
    std::shared_ptr<Scope> &scope,                                       //
    const token_slice &tokens                                            //
) {
    PROFILE_CUMULATIVE("Parser::create_type_cast");
    ASSERT(tokens.first->token == TOK_TYPE);
    ASSERT(std::next(tokens.first)->token == TOK_LEFT_PAREN);
    token_slice tokens_mut = tokens;
    std::optional<uint2> expr_range = Matcher::balanced_range_extraction(           //
        tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
    );
    if (!expr_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Remove the parenthesis from the expression token ranges
    expr_range.value().first++;
    expr_range.value().second--;
    ASSERT(expr_range.value().second > expr_range.value().first);

    // Get the type the expression needs to be converted to
    const std::shared_ptr<Type> to_type = tokens.first->type;
    const std::string to_type_string = to_type->to_string();

    // Create the expression
    token_slice expr_tokens = {tokens_mut.first + expr_range.value().first, tokens_mut.first + expr_range.value().second};
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(ctx, scope, expr_tokens);
    if (!expression.has_value()) {
        return std::nullopt;
    }

    // Check if the expression already is the desired type, in that case just return the expression directly
    if (expression.value()->type == to_type) {
        return expression;
    }

    // Enums are allowed to be cast to strings and to integers
    if (expression.value()->type->get_variation() == Type::Variation::ENUM) {
        if (to_type_string == "str") {
            return std::make_unique<TypeCastNode>(file_hash, get_pos_triple(tokens), to_type, expression.value());
        } else if (to_type_string == "u8" || to_type_string == "u16" || to_type_string == "u32" || to_type_string == "u64" //
            || to_type_string == "i8" || to_type_string == "i16" || to_type_string == "i32" || to_type_string == "i64"     //
        ) {
            return std::make_unique<TypeCastNode>(file_hash, get_pos_triple(tokens), to_type, expression.value());
        }
    }

    // Check if the type of the expression is castable at all
    const std::string expr_type_str = expression.value()->type->to_string();
    if (primitive_casting_table.find(expr_type_str) == primitive_casting_table.end()) {
        THROW_ERR(                                                                                                              //
            ErrExprCastInvalid, ERR_PARSING, file_hash,                                                                         //
            expression.value()->line, expression.value()->column, expression.value()->length, to_type, expression.value()->type //
        );
        return std::nullopt;
    }
    const std::vector<std::string_view> &to_types = primitive_casting_table.at(expr_type_str);
    if (std::find(to_types.begin(), to_types.end(), to_type_string) == to_types.end()) {
        THROW_ERR(                                                                                                              //
            ErrExprCastInvalid, ERR_PARSING, file_hash,                                                                         //
            expression.value()->line, expression.value()->column, expression.value()->length, to_type, expression.value()->type //
        );
        return std::nullopt;
    }
    if (expr_type_str == "int" || expr_type_str == "float") {
        expression.value()->type = to_type;
        return expression;
    }

    if (!check_castability(to_type, expression.value(), false)) {
        THROW_ERR(                                                                                                              //
            ErrExprCastInvalid, ERR_PARSING, file_hash,                                                                         //
            expression.value()->line, expression.value()->column, expression.value()->length, to_type, expression.value()->type //
        );
        return std::nullopt;
    }
    // Set source location on the resulting expression
    expression.value()->line = tokens.first->line;
    expression.value()->column = tokens.first->column;
    expression.value()->length = (tokens.first + expr_range.value().second + 1)->column - tokens.first->column;
    return expression;
}

std::optional<GroupExpressionNode> Parser::create_group_expression( //
    const Context &ctx,                                             //
    std::shared_ptr<Scope> &scope,                                  //
    const token_slice &tokens                                       //
) {
    PROFILE_CUMULATIVE("Parser::create_group_expression");
    token_slice tokens_mut = tokens;
    // First, remove all trailing garbage from the expression tokens
    remove_trailing_garbage(tokens_mut);
    // Now, the first and the last token must be open and closing parenthesis respectively
    ASSERT(tokens_mut.first->token == TOK_LEFT_PAREN);
    ASSERT(std::prev(tokens_mut.second)->token == TOK_RIGHT_PAREN);
    // Remove the open and closing parenthesis
    tokens_mut.first++;
    tokens_mut.second--;

    // Get all balanced match ranges of commas in the group expression
    std::vector<uint2> match_ranges = Matcher::get_match_ranges_in_range_outside_group( //
        tokens_mut,                                                                     //
        Matcher::until_comma,                                                           //
        {0, std::distance(tokens_mut.first, tokens_mut.second)},                        //
        Matcher::token(TOK_LEFT_PAREN),                                                 //
        Matcher::token(TOK_RIGHT_PAREN)                                                 //
    );
    // Its not a group expression if there is only one expression inside the parenthesis, this should never happen
    ASSERT(!match_ranges.empty());
    // Remove all duplicates, because when the fourth token is a comma we get the ranges 0-3, 1-3 and 2-3, and we only care about the
    // first one, not all later ones
    unsigned int last_second = UINT32_MAX;
    for (auto it = match_ranges.begin(); it != match_ranges.end();) {
        if (it->second == last_second) {
            match_ranges.erase(it);
        } else {
            last_second = it->second;
            ++it;
        }
    }
    // All tokens from the end of the second range up to the end are the last expression of the group
    ASSERT(tokens_mut.first + match_ranges.back().second < tokens_mut.second);
    match_ranges.emplace_back(match_ranges.back().second, std::distance(tokens_mut.first, tokens_mut.second));

    // Decrement all second matches ranges to exclude all commas from the expression (except for the last match range, it has no comma
    // at its last position
    for (auto it = match_ranges.begin(); it != match_ranges.end() - 1; ++it) {
        it->second--;
    }

    // Parse all expressions in the group
    std::vector<std::unique_ptr<ExpressionNode>> expressions;
    for (const uint2 &match_range : match_ranges) {
        token_slice expression_tokens = {tokens_mut.first + match_range.first, tokens_mut.first + match_range.second};
        auto expr = create_expression(ctx, scope, expression_tokens);
        if (!expr.has_value()) {
            return std::nullopt;
        }
        expressions.emplace_back(std::move(expr.value()));
    }

    // Check if the types in the group are correct
    for (unsigned int i = 0; i < expressions.size(); i++) {
        const std::string type_str = expressions[i]->type->to_string();
        if (type_str == "type.flint.str.lit") {
            expressions[i] = std::make_unique<TypeCastNode>(                                       //
                file_hash, get_pos_triple(tokens), Type::get_primitive_type("str"), expressions[i] //
            );
        } else if (expressions[i]->type->get_variation() == Type::Variation::GROUP) {
            // Nested groups are not allowed
            const auto match_range = match_ranges[i];
            token_slice expression_tokens = {tokens_mut.first + match_range.first, tokens_mut.first + match_range.second};
            THROW_ERR(ErrExprNestedGroup, ERR_PARSING, file_hash, expression_tokens);
            return std::nullopt;
        }
    }
    return GroupExpressionNode(file_hash, get_pos_triple(tokens), expressions);
}

std::optional<std::vector<std::unique_ptr<ExpressionNode>>> Parser::create_group_expressions( //
    const Context &ctx,                                                                       //
    std::shared_ptr<Scope> &scope,                                                            //
    const token_slice &tokens                                                                 //
) {
    PROFILE_CUMULATIVE("Parser::create_group_expressions");
    token_slice tokens_mut = tokens;
    std::vector<std::unique_ptr<ExpressionNode>> expressions;
    while (tokens_mut.first != tokens_mut.second) {
        std::optional<uint2> next_expr_range = Matcher::get_next_match_range(tokens_mut, Matcher::until_comma);
        if (!next_expr_range.has_value()) {
            // The last expression
            std::optional<std::unique_ptr<ExpressionNode>> indexing_expression = create_expression(ctx, scope, tokens_mut);
            tokens_mut.first = tokens_mut.second;
            if (!indexing_expression.has_value()) {
                return std::nullopt;
            }
            expressions.emplace_back(std::move(indexing_expression.value()));
        } else {
            // Not the last expression
            std::optional<std::unique_ptr<ExpressionNode>> indexing_expression = create_expression(   //
                ctx, scope, {tokens_mut.first, tokens_mut.first + next_expr_range.value().second - 1} //
            );
            tokens_mut.first += next_expr_range.value().second;
            if (!indexing_expression.has_value()) {
                return std::nullopt;
            }
            expressions.emplace_back(std::move(indexing_expression.value()));
        }
    }
    return expressions;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_range_expression( //
    const Context &ctx,                                                         //
    std::shared_ptr<Scope> &scope,                                              //
    const token_slice &tokens                                                   //
) {
    PROFILE_CUMULATIVE("Parser::create_range_expression");
    // A range expression consists of an lhs and an rhs, for now the lhs and rhs "expressions" consist of one token each, being a literal
    // token, but range expressions will be able to consist of any expression as the lsh and rhs in the future, but this day is not today
    const std::vector<uint2> &ranges = Matcher::get_match_ranges_in_range_outside_group( //
        tokens,                                                                          //
        Matcher::token(TOK_RANGE),                                                       //
        {0, std::distance(tokens.first, tokens.second)},                                 //
        Matcher::token(TOK_LEFT_PAREN),                                                  //
        Matcher::token(TOK_RIGHT_PAREN)                                                  //
    );
    ASSERT(ranges.size() == 1);
    const uint2 &range = ranges.front();
    const token_slice lhs_tokens = {tokens.first, tokens.first + range.first};
    const bool is_open_low = lhs_tokens.first == lhs_tokens.second;
    std::optional<std::unique_ptr<ExpressionNode>> lhs_expr;
    if (!is_open_low) {
        lhs_expr = create_expression(ctx, scope, lhs_tokens);
        if (!lhs_expr.has_value()) {
            return std::nullopt;
        }
    }
    const token_slice rhs_tokens = {tokens.first + range.second, tokens.second};
    const bool is_open_up = rhs_tokens.first == rhs_tokens.second;
    std::optional<std::unique_ptr<ExpressionNode>> rhs_expr;
    if (!is_open_up) {
        rhs_expr = create_expression(ctx, scope, rhs_tokens);
        if (!rhs_expr.has_value()) {
            return std::nullopt;
        }
    }
    const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
    if (is_open_low && is_open_up) {
        // It's an open-begin and open-ended range, e.g. it's just '..' meaning "from begin to end"
        ASSERT(!lhs_expr.has_value());
        ASSERT(!rhs_expr.has_value());
        LitValue lhs_zero = LitInt{.value = APInt("0")};
        lhs_expr = std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lhs_zero, u64_ty);
        LitValue rhs_max = LitInt{.value = APInt(std::to_string(UINT64_MAX))};
        rhs_expr = std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), rhs_max, u64_ty);
        return std::make_unique<RangeExpressionNode>(file_hash, get_pos_triple(tokens), lhs_expr.value(), rhs_expr.value());
    } else if (is_open_low) {
        // Its a range expression which begins at 0, because '0..5' and '..5' are the same
        ASSERT(!lhs_expr.has_value());
        ASSERT(rhs_expr.has_value());
        LitValue lhs_zero = LitInt{.value = APInt("0")};
        lhs_expr = std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lhs_zero, u64_ty);
    } else if (is_open_up) {
        // Its an open ended range expression
        ASSERT(lhs_expr.has_value());
        ASSERT(!rhs_expr.has_value());
        LitValue rhs_max = LitInt{.value = APInt(std::to_string(UINT64_MAX))};
        rhs_expr = std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), rhs_max, u64_ty);
    }
    if (!check_castability(u64_ty, lhs_expr.value())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, lhs_tokens, u64_ty, lhs_expr.value()->type);
        return std::nullopt;
    }
    if (!check_castability(u64_ty, rhs_expr.value())) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, rhs_tokens, u64_ty, rhs_expr.value()->type);
        return std::nullopt;
    }
    const bool is_lhs_lit = lhs_expr.value()->get_variation() == ExpressionNode::Variation::LITERAL;
    const bool is_rhs_lit = rhs_expr.value()->get_variation() == ExpressionNode::Variation::LITERAL;
    if (is_lhs_lit && is_rhs_lit) {
        const auto *lhs_lit = lhs_expr.value()->as<LiteralNode>();
        const auto *rhs_lit = rhs_expr.value()->as<LiteralNode>();
        // Ensure that the range is correct (a range like '5..1' is not correct, it should be '1..5'
        if (!std::holds_alternative<LitInt>(lhs_lit->value) || !std::holds_alternative<LitInt>(rhs_lit->value)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // Make sure that no value of the range is negative, that's not allowed
        const APInt lhs_val = std::get<LitInt>(lhs_lit->value).value;
        if (lhs_val.is_negative) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const APInt rhs_val = std::get<LitInt>(rhs_lit->value).value;
        if (rhs_val.is_negative) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // Check if the rhs val is smaller than the lhs val, that's an error case since the range is the wrong way around
        if (lhs_val > rhs_val) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }
    return std::make_unique<RangeExpressionNode>(file_hash, get_pos_triple(tokens), lhs_expr.value(), rhs_expr.value());
}

std::optional<DataAccessNode> Parser::create_data_access( //
    const Context &ctx,                                   //
    std::shared_ptr<Scope> &scope,                        //
    const token_slice &tokens                             //
) {
    PROFILE_CUMULATIVE("Parser::create_data_access");
    token_slice tokens_mut = tokens;
    auto field_access_base = create_field_access_base(ctx, scope, tokens_mut);
    if (!field_access_base.has_value()) {
        return std::nullopt;
    }
    return DataAccessNode(                    //
        file_hash,                            //
        get_pos_triple(tokens),               //
        field_access_base.value().base_expr,  //
        field_access_base.value().field_name, //
        field_access_base.value().field_id,   //
        field_access_base.value().field_type  //
    );
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_grouped_data_access( //
    const Context &ctx,                                                            //
    std::shared_ptr<Scope> &scope,                                                 //
    const token_slice &tokens                                                      //
) {
    PROFILE_CUMULATIVE("Parser::create_grouped_data_access");
    token_slice tokens_mut = tokens;
    auto grouped_field_access_base = create_grouped_access_base(ctx, scope, tokens_mut);
    if (!grouped_field_access_base.has_value()) {
        return std::nullopt;
    }
    if (grouped_field_access_base->alternative_expression.has_value()) {
        return std::move(grouped_field_access_base->alternative_expression);
    }
    return std::make_unique<GroupedDataAccessNode>(    //
        file_hash,                                     //
        get_pos_triple(tokens),                        //
        grouped_field_access_base.value().base_expr,   //
        grouped_field_access_base.value().field_names, //
        grouped_field_access_base.value().field_ids,   //
        grouped_field_access_base.value().field_types  //
    );
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_array_initializer( //
    const Context &ctx,                                                          //
    std::shared_ptr<Scope> &scope,                                               //
    const token_slice &tokens                                                    //
) {
    PROFILE_CUMULATIVE("Parser::create_array_initializer");
    token_list toks = clone_from_slice(tokens);
    std::optional<uint2> length_expression_range = Matcher::balanced_range_extraction( //
        tokens, Matcher::token(TOK_LEFT_BRACKET), Matcher::token(TOK_RIGHT_BRACKET)    //
    );
    if (!length_expression_range.has_value()) {
        // If there are no length expressions, check if the first token is an array type token in which the sizes are comptime-known
        if (tokens.first->token != TOK_TYPE || tokens.first->type->get_variation() != Type::Variation::ARRAY) {
            return std::nullopt;
        }
        const std::shared_ptr<Type> &array_type = tokens.first->type;
        const ArrayType *arr_type = array_type->as<ArrayType>();
        if (!arr_type->sizes.has_value()) {
            return std::nullopt;
        }

        // Create all the literal expressions for all the const sizes
        std::vector<std::unique_ptr<ExpressionNode>> length_expressions;
        const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
        for (const size_t len : arr_type->sizes.value()) {
            LitValue value = LitInt{.value = APInt(std::to_string(len))};
            length_expressions.emplace_back(std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), value, u64_ty));
        }

        // Get the initializer values inside the tokens {...} to inline-initialize the array or (...) for a single initializer
        token_slice initializer_tokens = {tokens.first + 1, tokens.second};
        remove_surrounding_paren(initializer_tokens);
        if (initializer_tokens.first->token == TOK_LEFT_BRACE) {
            // Skip the {
            initializer_tokens.first++;
            // Skip the } and check if it was a }
            initializer_tokens.second--;
            if (initializer_tokens.second->token != TOK_RIGHT_BRACE) {
                THROW_ERR(                                                                                                   //
                    ErrParsUnexpectedToken, ERR_PARSING, file_hash, initializer_tokens.second->line,                         //
                    initializer_tokens.second->column, std::vector<Token>{TOK_RIGHT_BRACE}, initializer_tokens.second->token //
                );
                return std::nullopt;
            }
            // Check if there is a trailing comma, skip it
            if (std::prev(initializer_tokens.second)->token == TOK_COMMA) {
                initializer_tokens.second--;
            }
            // Create the initializer values
            auto exprs = create_group_expressions(ctx, scope, initializer_tokens);
            if (!exprs.has_value()) {
                return std::nullopt;
            }
            // The number of expressions must match the expected number of elements in the array
            // TODO: Handle inferred sizes through `_` for the size
            size_t sizes_sum = 1;
            for (const size_t size : arr_type->sizes.value()) {
                sizes_sum *= size;
            }
            if (exprs.value().size() != sizes_sum) {
                // Expected `sizes_sum` values in inline-initializer, but only got `expr.value().size()`.
                // TODO: Add a way to auto-fill all missing values, for example creating an array of size 10 and only filling in the first 3
                // values in the initializer. Maybe by manually writing the last expressions as a default expression, so for example
                // `i32[10]{10, 20, 30, _}`?
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }

            // Counts match up, check if all expressions are castable to the base type of the array
            for (auto &expr : exprs.value()) {
                if (!check_castability(arr_type->type, expr, true)) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, initializer_tokens, arr_type->type, expr->type);
                    return std::nullopt;
                }
            }
            return std::make_unique<InlineArrayInitializerNode>(                                 //
                file_hash, get_pos_triple(tokens), array_type, length_expressions, exprs.value() //
            );
        }

        // Create the initializer expression
        std::optional<std::unique_ptr<ExpressionNode>> initializer;
        if (std::next(initializer_tokens.first) == initializer_tokens.second && initializer_tokens.first->token == TOK_UNDERSCORE) {
            initializer = std::make_unique<DefaultNode>(file_hash, get_pos_triple(tokens), arr_type->type);
        } else {
            initializer = create_expression(ctx, scope, initializer_tokens);
        }
        if (!initializer.has_value()) {
            return std::nullopt;
        }
        if (!check_castability(arr_type->type, initializer.value(), true)) {
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, initializer_tokens, arr_type->type, initializer.value()->type);
            return std::nullopt;
        }

        return std::make_unique<ArrayInitializerNode>(                                             //
            file_hash, get_pos_triple(tokens), array_type, length_expressions, initializer.value() //
        );
    }

    // Get the element type of the array
    token_slice tokens_mut = tokens;
    token_slice type_tokens = {tokens_mut.first, tokens_mut.first + length_expression_range.value().first};
    tokens_mut.first += length_expression_range.value().first;
    std::optional<std::shared_ptr<Type>> element_type = file_node_ptr->file_namespace->get_type(type_tokens);
    if (!element_type.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Get the initializer tokens (...) and remove the surrounding parenthesis
    token_slice initializer_tokens = {tokens.first + length_expression_range.value().second, tokens_mut.second};
    tokens_mut.second = tokens.first + length_expression_range.value().second;
    remove_surrounding_paren(initializer_tokens);
    if (initializer_tokens.first->token == TOK_LEFT_BRACE) {
        // Brace initializer { expr, expr, ... } with inline array initialization
        initializer_tokens.first++;
        initializer_tokens.second--;
        if (initializer_tokens.second->token != TOK_RIGHT_BRACE) {
            THROW_ERR(                                                                                                   //
                ErrParsUnexpectedToken, ERR_PARSING, file_hash, initializer_tokens.second->line,                         //
                initializer_tokens.second->column, std::vector<Token>{TOK_RIGHT_BRACE}, initializer_tokens.second->token //
            );
            return std::nullopt;
        }

        // Check if there is a trailing comma, skip it
        if (std::prev(initializer_tokens.second)->token == TOK_COMMA) {
            initializer_tokens.second--;
        }
        auto exprs = create_group_expressions(ctx, scope, initializer_tokens);
        if (!exprs.has_value()) {
            return std::nullopt;
        }
        const size_t total_element_count = exprs.value().size();

        // Strip brackets from tokens_mut to reveal the length expression content
        ASSERT(tokens_mut.first->token == TOK_LEFT_BRACKET);
        tokens_mut.first++;
        tokens_mut.second--;
        ASSERT(tokens_mut.second->token == TOK_RIGHT_BRACKET);

        // Parse length expressions from tokens_mut, handling inferred size using default expression _
        const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
        std::vector<std::unique_ptr<ExpressionNode>> length_expressions;
        std::vector<size_t> known_sizes;
        size_t default_idx = UINT64_MAX;
        token_slice len_tokens = tokens_mut;
        while (len_tokens.first != len_tokens.second) {
            const auto next_range = Matcher::get_next_match_range(len_tokens, Matcher::until_comma);
            token_slice dim_tokens;
            if (!next_range.has_value()) {
                dim_tokens = len_tokens;
                len_tokens.first = len_tokens.second;
            } else {
                dim_tokens = {len_tokens.first, len_tokens.first + next_range.value().second - 1};
                len_tokens.first += next_range.value().second;
            }
            if (dim_tokens.first->token == TOK_UNDERSCORE && std::next(dim_tokens.first) == dim_tokens.second) {
                // default _ : at most one allowed
                if (default_idx != SIZE_MAX) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                default_idx = length_expressions.size();
                // Placeholder literal, value will be filled in after inference
                LitValue temp = LitInt{.value = APInt("0")};
                length_expressions.emplace_back(std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), temp, u64_ty));
                continue;
            }

            auto expr = create_expression(ctx, scope, dim_tokens);
            if (!expr.has_value()) {
                return std::nullopt;
            }
            if (!check_castability(u64_ty, expr.value(), true)) {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, dim_tokens, u64_ty, expr.value()->type);
                return std::nullopt;
            }
            std::optional<size_t> size = get_size_from_expr(expr.value());
            if (!size.has_value()) {
                // Length expressions in inline array initializers must be comptime-known
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            known_sizes.emplace_back(size.value());
            length_expressions.emplace_back(std::move(expr.value()));
        }

        // Compute product of known (non-default) sizes
        size_t product = 1;
        for (size_t size : known_sizes) {
            product *= size;
        }
        if (default_idx != UINT64_MAX) {
            // Infer the default size from the total element count
            if (product == 0 || total_element_count % product != 0) {
                // Size cannot be inferred since it's not an integer value (the dimensionality would need a fractional length)
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            const size_t inferred = total_element_count / product;
            LiteralNode *lit = length_expressions[default_idx]->as<LiteralNode>();
            lit->value = LitInt{.value = APInt(std::to_string(inferred))};
            known_sizes.insert(known_sizes.begin() + default_idx, inferred);
        } else {
            // No default: validate that product matches total element count
            if (total_element_count != product) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
        }

        // Build the array type (all sizes are now known by now)
        std::shared_ptr<Type> array_type = std::make_shared<ArrayType>(length_expressions.size(), element_type.value(), known_sizes);
        if (!file_node_ptr->file_namespace->add_type(array_type)) {
            array_type = file_node_ptr->file_namespace->get_type_from_str(array_type->to_string()).value();
        }

        // Check castability of all brace expressions to the element type
        for (auto &expr : exprs.value()) {
            if (!check_castability(element_type.value(), expr, true)) {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, initializer_tokens, element_type.value(), expr->type);
                return std::nullopt;
            }
        }
        return std::make_unique<InlineArrayInitializerNode>(                                 //
            file_hash, get_pos_triple(tokens), array_type, length_expressions, exprs.value() //
        );
    }

    // Now we can create the initializer expression
    std::optional<std::unique_ptr<ExpressionNode>> initializer;
    if (std::next(initializer_tokens.first) == initializer_tokens.second && initializer_tokens.first->token == TOK_UNDERSCORE) {
        initializer = std::make_unique<DefaultNode>(file_hash, get_pos_triple(tokens), element_type.value());
    } else {
        initializer = create_expression(ctx, scope, initializer_tokens);
    }
    if (!initializer.has_value()) {
        return std::nullopt;
    }
    if (!check_castability(element_type.value(), initializer.value(), true)) {
        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, initializer_tokens, element_type.value(), initializer.value()->type);
        return std::nullopt;
    }

    // The first token in the tokens list should be a left bracket
    ASSERT(tokens_mut.first->token == TOK_LEFT_BRACKET);
    tokens_mut.first++;
    // The last token in the tokens list should be a right bracket
    ASSERT(std::prev(tokens_mut.second)->token == TOK_RIGHT_BRACKET);
    tokens_mut.second--;
    // Now, everything left in the `tokens_mut` vector should be the length expressions [...]
    auto length_expressions = create_group_expressions(ctx, scope, tokens_mut);
    if (!length_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Every expression in the indexing expressions needs to be castable a `u64` type, if it's not of that type already we need to cast it
    const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
    if (!ensure_castability_multiple(u64_ty, length_expressions.value(), tokens_mut)) {
        return std::nullopt;
    }

    std::vector<size_t> known_sizes;
    for (const auto &expr : length_expressions.value()) {
        std::optional<size_t> size = get_size_from_expr(expr);
        if (size.has_value()) {
            known_sizes.emplace_back(size.value());
        }
    }
    std::string actual_type_str = element_type.value()->to_string() + "[";
    const bool is_const_array = known_sizes.size() == length_expressions.value().size();
    if (is_const_array) {
        for (size_t i = 0; i < known_sizes.size(); i++) {
            if (i > 0) {
                actual_type_str += ", ";
            }
            actual_type_str += std::to_string(known_sizes.at(i));
        }
        actual_type_str += "]";
    } else {
        actual_type_str += std::string(length_expressions.value().size() - 1, ',') + "]";
    }
    std::optional<std::shared_ptr<Type>> actual_array_type = file_node_ptr->file_namespace->get_type_from_str(actual_type_str);
    if (!actual_array_type.has_value()) {
        // This type does not yet exist, so we need to create it
        if (is_const_array) {
            actual_array_type = std::make_shared<ArrayType>(length_expressions.value().size(), element_type.value(), known_sizes);
        } else {
            actual_array_type = std::make_shared<ArrayType>(length_expressions.value().size(), element_type.value(), std::nullopt);
        }
        file_node_ptr->file_namespace->add_type(actual_array_type.value());
    }
    return std::make_unique<ArrayInitializerNode>(                                                                    //
        file_hash, get_pos_triple(tokens), actual_array_type.value(), length_expressions.value(), initializer.value() //
    );
}

std::optional<ArrayAccessNode> Parser::create_array_access( //
    const Context &ctx,                                     //
    std::shared_ptr<Scope> &scope,                          //
    const token_slice &tokens                               //
) {
    PROFILE_CUMULATIVE("Parser::create_array_access");
    // The array access must end with a closing bracket token. Then, everything from that closing bracket to the left until an opening
    // bracket is considered the indexing expressions. Everything that comes before that initial opening bracket is considered the base
    // expression.
    token_list toks = clone_from_slice(tokens);
    ASSERT(std::prev(tokens.second)->token == TOK_RIGHT_BRACKET);
    token_slice indexing_tokens = {tokens.second - 1, tokens.second - 1};
    token_slice base_expr_tokens = {tokens.first, tokens.second - 1};
    unsigned int depth = 0;
    for (; base_expr_tokens.second != tokens.first;) {
        // We can decrement the 'indexing_tokens' begin as well as the 'base_expr_tokens' end at the same time, needing only one loop to
        // get both ranges
        if (base_expr_tokens.second->token == TOK_RIGHT_BRACKET) {
            depth++;
        } else if (base_expr_tokens.second->token == TOK_LEFT_BRACKET) {
            depth--;
            if (depth == 0) {
                // Let the indexing tokens start right after the bracket
                indexing_tokens.first++;
                break;
            }
        }
        indexing_tokens.first--;
        base_expr_tokens.second--;
    }
    // First we parse the base expression, it's type must be an array type (or string type)
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(ctx, scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const bool is_array_type = base_expr.value()->type->get_variation() == Type::Variation::ARRAY;
    const bool is_str_type = base_expr.value()->type->to_string() == "str";
    if (!is_array_type && !is_str_type) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Now we can parse the indexing expression(s)
    std::optional<std::vector<std::unique_ptr<ExpressionNode>>> indexing_expressions = create_group_expressions( //
        ctx, scope, indexing_tokens                                                                              //
    );
    if (!indexing_expressions.has_value()) {
        return std::nullopt;
    }
    const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
    if (!ensure_castability_multiple(u64_ty, indexing_expressions.value(), indexing_tokens)) {
        return std::nullopt;
    }
    if (is_str_type) {
        if (indexing_expressions.value().size() > 1) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (indexing_expressions.value().front()->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
            return ArrayAccessNode(                                                                                                 //
                file_hash, get_pos_triple(tokens), base_expr.value(), Type::get_primitive_type("str"), indexing_expressions.value() //
            );
        } else {
            return ArrayAccessNode(                                                                                                //
                file_hash, get_pos_triple(tokens), base_expr.value(), Type::get_primitive_type("u8"), indexing_expressions.value() //
            );
        }
    }
    // The indexing expression size must match the array dimensionality
    ASSERT(is_array_type);
    const auto *array_type = base_expr.value()->type->as<ArrayType>();
    if (indexing_expressions.value().size() != array_type->dimensionality) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Check how many of the indexing expressions are range expressions. The dimensionality of the array access only get's decreased if the
    // indexing expression is not a range expression. For range expressions the dimensionality actually stays the same
    unsigned int dimensionality = array_type->dimensionality;
    for (const auto &indexing_expression : indexing_expressions.value()) {
        if (indexing_expression->get_variation() != ExpressionNode::Variation::RANGE_EXPRESSION) {
            dimensionality--;
        }
    }
    if (dimensionality == 0) {
        return ArrayAccessNode(file_hash, get_pos_triple(tokens), base_expr.value(), array_type->type, indexing_expressions.value());
    } else {
        // TODO: Known Sizes
        std::shared_ptr<Type> new_arr_type = std::make_shared<ArrayType>(dimensionality, array_type->type, std::nullopt);
        if (!file_node_ptr->file_namespace->add_type(new_arr_type)) {
            new_arr_type = file_node_ptr->file_namespace->get_type_from_str(new_arr_type->to_string()).value();
        }
        return ArrayAccessNode(file_hash, get_pos_triple(tokens), base_expr.value(), new_arr_type, indexing_expressions.value());
    }
}

std::optional<GroupedArrayAccessNode> Parser::create_grouped_array_access( //
    const Context &ctx,                                                    //
    std::shared_ptr<Scope> &scope,                                         //
    const token_slice &tokens                                              //
) {
    PROFILE_CUMULATIVE("Parser::create_grouped_array_access");
    // The grouped array access must end with a closing bracket token. Then, everything from that closing bracket to the left until an
    // opening bracket is considered the indexing expressions. Everything that comes before that initial opening bracket is considered the
    // base expression.
    ASSERT(std::prev(tokens.second)->token == TOK_RIGHT_BRACKET);
    token_slice indexing_tokens = {tokens.second - 1, tokens.second - 1};
    token_slice base_expr_tokens = {tokens.first, tokens.second - 1};
    unsigned int depth = 0;
    for (; base_expr_tokens.second != tokens.first;) {
        // We can decrement the 'indexing_tokens' begin as well as the 'base_expr_tokens' end at the same time, needing only one loop to
        // get both ranges
        if (base_expr_tokens.second->token == TOK_RIGHT_BRACKET) {
            depth++;
        } else if (base_expr_tokens.second->token == TOK_LEFT_BRACKET) {
            depth--;
            if (depth == 0) {
                // Let the indexing tokens start right after the bracket
                indexing_tokens.first++;
                break;
            }
        }
        indexing_tokens.first--;
        base_expr_tokens.second--;
    }
    ASSERT(base_expr_tokens.second->token == TOK_LEFT_BRACKET);
    base_expr_tokens.second--;
    ASSERT(base_expr_tokens.second->token == TOK_DOT);
    // First we parse the base expression, it's type must be an array type (or string type)
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(ctx, scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const bool is_array_type = base_expr.value()->type->get_variation() == Type::Variation::ARRAY;
    const bool is_str_type = base_expr.value()->type->to_string() == "str";
    if (!is_array_type && !is_str_type) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Now we can parse the indexing expression(s)
    std::optional<std::vector<std::unique_ptr<ExpressionNode>>> indexing_expressions = create_group_expressions( //
        ctx, scope, indexing_tokens                                                                              //
    );
    if (!indexing_expressions.has_value()) {
        return std::nullopt;
    }
    // The indexing expressions must all have the type of a group with N values where N is the dimensionality of the accessed array
    const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
    std::shared_ptr<Type> indexing_ty = u64_ty;
    std::shared_ptr<Type> base_ty{nullptr};
    if (is_array_type) {
        const auto *array_ty = base_expr.value()->type->as<ArrayType>();
        if (array_ty->dimensionality > 1) {
            std::vector<std::shared_ptr<Type>> indexing_group_types;
            for (size_t i = 0; i < array_ty->dimensionality; i++) {
                indexing_group_types.emplace_back(u64_ty);
            }
            indexing_ty = std::make_shared<GroupType>(indexing_group_types);
            if (!Type::add_type(indexing_ty)) {
                indexing_ty = Type::get_type_from_str(indexing_ty->to_string()).value();
            }
        }
        base_ty = array_ty->type;
    } else {
        base_ty = Type::get_primitive_type("u8");
    }
    if (!ensure_castability_multiple(indexing_ty, indexing_expressions.value(), indexing_tokens)) {
        return std::nullopt;
    }
    // The retuned type is the base type of the array as a group of N where N is the number of indexing expression
    std::vector<std::shared_ptr<Type>> return_types;
    for (size_t i = 0; i < indexing_expressions.value().size(); i++) {
        return_types.emplace_back(base_ty);
    }
    std::shared_ptr<Type> ret_ty = std::make_shared<GroupType>(return_types);
    if (!Type::add_type(ret_ty)) {
        ret_ty = Type::get_type_from_str(ret_ty->to_string()).value();
    }
    return GroupedArrayAccessNode(file_hash, get_pos_triple(tokens), base_expr.value(), ret_ty, indexing_expressions.value());
}

std::optional<OptionalChainNode> Parser::create_optional_chain( //
    const Context &ctx,                                         //
    std::shared_ptr<Scope> &scope,                              //
    const token_slice &tokens                                   //
) {
    PROFILE_CUMULATIVE("Parser::create_optional_chain");
    // First, we need to find the `?` token, everything left to that token is our base expression
    auto iterator = tokens.second - 1;
    while (iterator != tokens.first) {
        if (iterator->token == TOK_QUESTION) {
            break;
        }
        --iterator;
    }
    // If the iterator is the beginning this means that no `?` token is present in the list of tokens, this means something in the
    // matcher went wrong, not here in the parser
    ASSERT(iterator != tokens.first);
    // Everything to the left of the iterator is the base expression and can be parsed as such
    const token_slice base_expr_tokens = {tokens.first, iterator};

    // Move past the `?` token
    iterator++;
    ChainOperation operation;
    std::shared_ptr<Type> result_type;
    // Now we need to check what the rhs of the optional chain is
    // TODO: Change the 'is_toplevel_chain_node' to something else, to detect whether it actually *is* the top level
    if (iterator->token == TOK_LEFT_BRACKET) {
        auto array_access_base = create_array_access_base(ctx, scope, tokens);
        if (!array_access_base.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        operation = ChainArrayAccess{std::move(array_access_base.value().indexing_exprs)};
        return OptionalChainNode(                                                   //
            file_hash, get_pos_triple(tokens), array_access_base.value().base_expr, //
            true, operation, array_access_base.value().result_type                  //
        );
    } else if (iterator->token == TOK_DOT) {
        auto field_access_base = create_field_access_base(ctx, scope, tokens, true);
        if (!field_access_base.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        operation = ChainFieldAccess{field_access_base.value().field_name, field_access_base.value().field_id};
        return OptionalChainNode(                                                                                                         //
            file_hash, get_pos_triple(tokens), field_access_base.value().base_expr, true, operation, field_access_base.value().field_type //
        );
    }
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_optional_unwrap( //
    const Context &ctx,                                                        //
    std::shared_ptr<Scope> &scope,                                             //
    const token_slice &tokens                                                  //
) {
    PROFILE_CUMULATIVE("Parser::create_optional_unwrap");
    // We first need to get the last exclamation operator as our separator for the base expression
    auto iterator = tokens.second - 1;
    while (iterator != tokens.first) {
        if (iterator->token == TOK_EXCLAMATION) {
            break;
        }
        --iterator;
    }
    ASSERT(iterator != tokens.first);
    ASSERT(iterator->token == TOK_EXCLAMATION);
    const token_slice base_expr_tokens = {tokens.first, iterator};
    // If nothing follows after the optional unwrap node we can return it directly
    if (iterator == tokens.second - 1) {
        auto base_expr = create_expression(ctx, scope, base_expr_tokens);
        if (!base_expr.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (base_expr.value()->type->get_variation() != Type::Variation::OPTIONAL) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<OptionalUnwrapNode>(file_hash, get_pos_triple(tokens), base_expr.value());
    }
    // Skip the `!`
    ++iterator;

    if (iterator->token == TOK_LEFT_BRACKET) {
        // It's an array access. First we need to make sure that the base expression is an array or string type
        auto base_expr = create_expression(ctx, scope, base_expr_tokens);
        if (!base_expr.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (base_expr.value()->type->get_variation() != Type::Variation::OPTIONAL) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const auto *optional_type = base_expr.value()->type->as<OptionalType>();
        unsigned int dimensionality = 1;
        std::shared_ptr<Type> result_type;
        if (optional_type->base_type->get_variation() == Type::Variation::ARRAY) {
            const auto *base_array_type = optional_type->base_type->as<ArrayType>();
            result_type = base_array_type->type;
            dimensionality = base_array_type->dimensionality;
        } else if (optional_type->base_type->to_string() != "str") {
            result_type = Type::get_primitive_type("u8");
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }

        // The last token should be a right bracket and everything in between are the indexing expressions
        if (std::prev(tokens.second)->token != TOK_RIGHT_BRACKET) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        token_slice indexing_tokens = {std::next(iterator), std::prev(tokens.second)};
        std::optional<std::vector<std::unique_ptr<ExpressionNode>>> indexing_expressions = create_group_expressions( //
            ctx, scope, indexing_tokens                                                                              //
        );
        if (!indexing_expressions.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (indexing_expressions.value().size() != dimensionality) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
        if (!ensure_castability_multiple(u64_ty, indexing_expressions.value(), indexing_tokens)) {
            return std::nullopt;
        }
        std::unique_ptr<ExpressionNode> opt_unwrap = std::make_unique<OptionalUnwrapNode>( //
            file_hash, get_pos_triple(tokens), base_expr.value()                           //
        );
        return std::make_unique<ArrayAccessNode>(                                                    //
            file_hash, get_pos_triple(tokens), opt_unwrap, result_type, indexing_expressions.value() //
        );
    } else if (iterator->token == TOK_DOT && (iterator + 1)->token == TOK_LEFT_PAREN) {
        // It's a grouped field access
        auto grouped_access_base = create_grouped_access_base(ctx, scope, tokens, true);
        if (!grouped_access_base.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        auto &base_expr = grouped_access_base.value().base_expr;
        std::unique_ptr<ExpressionNode> opt_unwrap = std::make_unique<OptionalUnwrapNode>( //
            file_hash, get_pos_triple(tokens), base_expr                                   //
        );
        const auto &field_names = grouped_access_base.value().field_names;
        const auto &field_ids = grouped_access_base.value().field_ids;
        const auto &field_types = grouped_access_base.value().field_types;
        return std::make_unique<GroupedDataAccessNode>(file_hash, get_pos_triple(tokens), opt_unwrap, field_names, field_ids, field_types);
    } else if (iterator->token == TOK_DOT) {
        // It's a field access
        auto field_access_base = create_field_access_base(ctx, scope, tokens, true);
        if (!field_access_base.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        auto &base_expr = field_access_base.value().base_expr;
        std::unique_ptr<ExpressionNode> opt_unwrap = std::make_unique<OptionalUnwrapNode>(file_hash, get_pos_triple(tokens), base_expr);
        const auto &field_name = field_access_base.value().field_name;
        const auto &field_id = field_access_base.value().field_id;
        const auto &field_type = field_access_base.value().field_type;
        return std::make_unique<DataAccessNode>(file_hash, get_pos_triple(tokens), opt_unwrap, field_name, field_id, field_type);
    }
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<VariantExtractionNode> Parser::create_variant_extraction( //
    const Context &ctx,                                                 //
    std::shared_ptr<Scope> &scope,                                      //
    const token_slice &tokens                                           //
) {
    PROFILE_CUMULATIVE("Parser::create_variant_extraction");
    token_list toks = clone_from_slice(tokens);
    // We first need to get the last question operator as our separator for the base expression
    auto iterator = tokens.second - 1;
    while (iterator != tokens.first) {
        if (iterator->token == TOK_QUESTION) {
            break;
        }
        --iterator;
    }
    ASSERT(iterator != tokens.first);
    ASSERT(iterator->token == TOK_QUESTION);
    const token_slice base_expr_tokens = {tokens.first, iterator};
    // Next should follow an open paren containing a type token or a tag literal followed by a closing paren
    ++iterator;
    ASSERT(iterator->token == TOK_LEFT_PAREN);
    auto end_it = ++iterator;
    while (end_it->token != TOK_RIGHT_PAREN) {
        end_it++;
    }
    ASSERT(end_it->token == TOK_RIGHT_PAREN);
    const token_slice type_tokens = {iterator, end_it};
    auto type_expr = create_expression(ctx, scope, type_tokens);
    if (!type_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    iterator = end_it;
    std::pair<std::optional<std::string>, std::shared_ptr<Type>> unwrap_tag_and_type;
    const auto type_expr_variation = type_expr.value()->get_variation();
    if (type_expr_variation == ExpressionNode::Variation::TYPE) {
        unwrap_tag_and_type = {std::nullopt, type_expr.value()->type};
    } else if (type_expr_variation == ExpressionNode::Variation::LITERAL) {
        const auto *literal_node = type_expr.value()->as<LiteralNode>();
        if (!std::holds_alternative<LitVariantTag>(literal_node->value)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const LitVariantTag &lit_variant = std::get<LitVariantTag>(literal_node->value);
        VariantNode *const variant_node = std::get<VariantNode *const>(lit_variant.variant_type->as<VariantType>()->var_or_list);
        const auto &tag_type = variant_node->get_type_of_tag(lit_variant.variation_tag);
        if (!tag_type.has_value()) {
            // Tag not present in variant type
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        unwrap_tag_and_type = {lit_variant.variation_tag, tag_type.value()};
    }
    type_expr.value().reset();

    // If nothing follows after the variant extraction node we can return its result wrapped in an optional directly
    if (iterator == tokens.second - 1) {
        auto base_expr = create_expression(ctx, scope, base_expr_tokens);
        if (!base_expr.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (base_expr.value()->type->get_variation() != Type::Variation::VARIANT) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const auto *variant_type = base_expr.value()->type->as<VariantType>();
        std::optional<unsigned char> type_id;
        if (unwrap_tag_and_type.first.has_value()) {
            type_id = variant_type->get_idx_of_tag(unwrap_tag_and_type.first.value());
        } else {
            type_id = variant_type->get_idx_of_type(unwrap_tag_and_type.second);
        }
        if (!type_id.has_value()) {
            // Type not part of the variant
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (base_expr.value()->get_variation() != ExpressionNode::Variation::VARIABLE) {
            // Extracting from non-variable expressions is not supported yet
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return VariantExtractionNode(file_hash, get_pos_triple(tokens), base_expr.value(), unwrap_tag_and_type.second, type_id.value());
    }
    // Skip the `)`
    ++iterator;

    if (iterator->token == TOK_LEFT_BRACKET) {
        // TODO: It's an array access. First we need to make sure that the extracted type is an array or string type
    } else if (iterator->token == TOK_DOT && (iterator + 1)->token == TOK_LEFT_PAREN) {
        // TODO: It's a grouped field access
    } else if (iterator->token == TOK_DOT) {
        // TODO: It's a field access
    }
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_variant_unwrap( //
    const Context &ctx,                                                       //
    std::shared_ptr<Scope> &scope,                                            //
    const token_slice &tokens                                                 //
) {
    PROFILE_CUMULATIVE("Parser::create_variant_unwrap");
    // We first need to get the last exclamation operator as our separator for the base expression
    auto iterator = tokens.second - 1;
    while (iterator != tokens.first) {
        if (iterator->token == TOK_EXCLAMATION) {
            break;
        }
        --iterator;
    }
    ASSERT(iterator != tokens.first);
    ASSERT(iterator->token == TOK_EXCLAMATION);
    const token_slice base_expr_tokens = {tokens.first, iterator};
    // Next should follow an open paren containing a type token or a tag literal followed by a closing paren
    ++iterator;
    ASSERT(iterator->token == TOK_LEFT_PAREN);
    auto end_it = ++iterator;
    while (end_it->token != TOK_RIGHT_PAREN) {
        end_it++;
    }
    ASSERT(end_it->token == TOK_RIGHT_PAREN);
    const token_slice type_tokens = {iterator, end_it};
    auto type_expr = create_expression(ctx, scope, type_tokens);
    if (!type_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    iterator = end_it;
    std::pair<std::optional<std::string>, std::shared_ptr<Type>> unwrap_tag_and_type;
    const auto type_expr_variation = type_expr.value()->get_variation();
    if (type_expr_variation == ExpressionNode::Variation::TYPE) {
        unwrap_tag_and_type = {std::nullopt, type_expr.value()->type};
    } else if (type_expr_variation == ExpressionNode::Variation::LITERAL) {
        const auto *literal_node = type_expr.value()->as<LiteralNode>();
        if (!std::holds_alternative<LitVariantTag>(literal_node->value)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const LitVariantTag &lit_variant = std::get<LitVariantTag>(literal_node->value);
        VariantNode *const variant_node = std::get<VariantNode *const>(lit_variant.variant_type->as<VariantType>()->var_or_list);
        const auto &tag_type = variant_node->get_type_of_tag(lit_variant.variation_tag);
        if (!tag_type.has_value()) {
            // Tag not present in variant type
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        unwrap_tag_and_type = {lit_variant.variation_tag, tag_type.value()};
    }
    type_expr.value().reset();

    // If nothing follows after the variant unwrap node we can return it directly
    if (iterator == tokens.second - 1) {
        auto base_expr = create_expression(ctx, scope, base_expr_tokens);
        if (!base_expr.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (base_expr.value()->type->get_variation() != Type::Variation::VARIANT) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const auto *variant_type = base_expr.value()->type->as<VariantType>();
        std::optional<unsigned char> type_id;
        if (unwrap_tag_and_type.first.has_value()) {
            type_id = variant_type->get_idx_of_tag(unwrap_tag_and_type.first.value());
        } else {
            type_id = variant_type->get_idx_of_type(unwrap_tag_and_type.second);
        }
        if (!type_id.has_value()) {
            // Type not part of the variant
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (base_expr.value()->get_variation() != ExpressionNode::Variation::VARIABLE) {
            // Unwrapping non-variable expressions is not supported yet
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<VariantUnwrapNode>(                                                           //
            file_hash, get_pos_triple(tokens), base_expr.value(), unwrap_tag_and_type.second, type_id.value() //
        );
    }
    // Skip the `)`
    ++iterator;

    if (iterator->token == TOK_LEFT_BRACKET) {
        // TODO: It's an array access. First we need to make sure that the unwrapped type is an array or string type
    } else if (iterator->token == TOK_DOT && (iterator + 1)->token == TOK_LEFT_PAREN) {
        // TODO: It's a grouped field access
    } else if (iterator->token == TOK_DOT) {
        // TODO: It's a field access
    }
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_pivot_expression( //
    const Context &ctx,                                                         //
    std::shared_ptr<Scope> &scope,                                              //
    const token_slice &tokens,                                                  //
    const std::optional<std::shared_ptr<Type>> &expected_type                   //
) {
    token_slice tokens_mut = tokens;
    token_list toks;
    if (DEBUG_MODE) {
        toks = clone_from_slice(tokens);
    }
    ASSERT(tokens_mut.first != tokens_mut.second); // Assert that tokens is not empty
    if (!Matcher::tokens_match(tokens_mut, Matcher::group_expression)) {
        remove_surrounding_paren(tokens_mut);
    }

    // Try to parse primary expressions first (literal, variables)
    size_t token_size = get_slice_size(tokens_mut);
    if (token_size == 1) {
        if (Matcher::tokens_match(tokens_mut, Matcher::literal)) {
            std::optional<LiteralNode> lit = create_literal(tokens_mut);
            if (!lit.has_value()) {
                return std::nullopt;
            }
            return std::make_unique<LiteralNode>(std::move(lit.value()));
        } else if (Matcher::tokens_match(tokens_mut, Matcher::variable_expr)) {
            return create_variable(scope, tokens_mut);
        } else if (tokens_mut.first->token == TOK_UNDERSCORE) {
            if (!expected_type.has_value()) {
                // Default node at a place where it's type cannot be inferred. This is fine because when used in initializers, for example,
                // at the time we parse the initializer argument the type cannot be inferred as we do not know *what* we are initializing
                // yet
                return std::make_unique<DefaultNode>(file_hash, get_pos_triple(tokens), Type::get_primitive_type("type.flint.default"));
            }
            return std::make_unique<DefaultNode>(file_hash, get_pos_triple(tokens), expected_type.value());
        } else if (tokens_mut.first->token == TOK_TYPE) {
            return std::make_unique<TypeNode>(file_hash, get_pos_triple(tokens), tokens_mut.first->type);
        } else if (tokens_mut.first->token == TOK_RANGE) {
            std::optional<std::unique_ptr<ExpressionNode>> range = create_range_expression(ctx, scope, tokens_mut);
            if (!range.has_value()) {
                return std::nullopt;
            }
            return std::move(range.value());
        }
    } else if (token_size == 2) {
        if (Matcher::tokens_match(tokens_mut, Matcher::literal_expr)) {
            std::optional<LiteralNode> lit = create_literal(tokens_mut);
            if (!lit.has_value()) {
                return std::nullopt;
            }
            return std::make_unique<LiteralNode>(std::move(lit.value()));
        } else if (Matcher::tokens_match(tokens_mut, Matcher::string_interpolation)) {
            ASSERT(tokens_mut.first->token == TOK_DOLLAR && std::prev(tokens_mut.second)->token == TOK_STR_VALUE);
            std::optional<std::unique_ptr<ExpressionNode>> interpol = create_string_interpolation( //
                ctx, scope, std::string(std::prev(tokens_mut.second)->lexme), tokens_mut           //
            );
            if (!interpol.has_value()) {
                return std::nullopt;
            }
            return std::move(interpol.value());
        }
    }

    if (Matcher::tokens_match(tokens_mut, Matcher::aliased_function_call)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().second == token_size) {
            // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is something
            // located on the right of the call still
            if (tokens_mut.first->token == TOK_TYPE) {
                // It's some form of "alias" on a base type
                switch (tokens_mut.first->type->get_variation()) {
                    default:
                        break;
                    case Type::Variation::ERROR_SET: {
                        const auto *error_type = tokens_mut.first->type->as<ErrorSetType>();
                        // It's an error literal with a message added to it
                        ASSERT((tokens_mut.first + 1)->token == TOK_DOT);
                        ASSERT((tokens_mut.first + 2)->token == TOK_IDENTIFIER);
                        ASSERT((tokens_mut.first + 3)->token == TOK_LEFT_PAREN);
                        const std::string value((tokens_mut.first + 2)->lexme);
                        const auto pair = error_type->error_node->get_id_msg_pair_of_value(value);
                        if (!pair.has_value()) {
                            // Unsupported error value
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        token_slice message_tokens = {tokens_mut.first + 4, tokens_mut.first + range.value().second - 1};
                        auto message =
                            create_expression(ctx, scope, message_tokens, file_node_ptr->file_namespace->get_type_from_str("str"));
                        if (!message.has_value()) {
                            return std::nullopt;
                        }
                        const std::shared_ptr<Type> lit_type = tokens_mut.first->type;
                        LitValue lit_value = LitError{.error_type = lit_type, .value = value, .message = std::move(message.value())};
                        return std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lit_value, lit_type);
                    }
                    case Type::Variation::FUNC: {
                        const auto *func_node = tokens_mut.first->type->as<FuncType>()->func_node;
                        std::optional<std::unique_ptr<ExpressionNode>> call_node = std::nullopt;
                        if (func_node->file_hash.to_string() != file_hash.to_string()) {
                            auto *func_namespace = Resolver::get_namespace_from_hash(func_node->file_hash);
                            call_node = create_call_expression(ctx, scope, tokens_mut, func_namespace, true);
                        } else {
                            call_node = create_call_expression(ctx, scope, tokens_mut, std::nullopt, true);
                        }
                        if (!call_node.has_value()) {
                            return std::nullopt;
                        }
                        return std::move(call_node.value());
                    }
                    case Type::Variation::VARIANT: {
                        const auto &type = tokens_mut.first->type;
                        const auto *variant_type = type->as<VariantType>();
                        if (!variant_type->is_err_variant && std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                            [[maybe_unused]] const auto *variant_node = std::get<VariantNode *const>(variant_type->var_or_list);
                            ASSERT((tokens_mut.first + 1)->token == TOK_DOT);
                            auto tag_it = tokens_mut.first + 2;
                            ASSERT(tag_it->token == TOK_IDENTIFIER || tag_it->token == TOK_TYPE);
                            const std::string tag = (tag_it->token == TOK_IDENTIFIER) //
                                ? std::string(tag_it->lexme)                          //
                                : tag_it->type->to_string();
                            const auto &possible_types = variant_type->get_possible_types();
                            std::optional<std::shared_ptr<Type>> variation_type;
                            for (const auto &[possible_tag, var_type] : possible_types) {
                                if (possible_tag.has_value() && possible_tag.value() == tag) {
                                    variation_type = var_type;
                                    break;
                                }
                            }
                            if (!variation_type.has_value()) {
                                THROW_BASIC_ERR(ERR_PARSING);
                                return std::nullopt;
                            }
                            tag_it++;
                            ASSERT(tag_it->token == TOK_LEFT_PAREN);
                            std::optional<std::unique_ptr<ExpressionNode>> expr = std::nullopt;
                            if (variation_type.value()->to_string() == "void") {
                                if ((tag_it + 1)->token != TOK_RIGHT_PAREN) {
                                    THROW_BASIC_ERR(ERR_PARSING);
                                    return std::nullopt;
                                }
                            } else {
                                if ((tag_it + 1)->token == TOK_RIGHT_PAREN) {
                                    THROW_BASIC_ERR(ERR_PARSING);
                                    return std::nullopt;
                                }
                                const auto &expr_range = Matcher::get_next_match_range(     //
                                    {tag_it, tokens_mut.second}, Matcher::until_right_paren //
                                );
                                if (!expr_range.has_value()) {
                                    THROW_BASIC_ERR(ERR_PARSING);
                                    return std::nullopt;
                                }
                                expr = create_expression(ctx, scope, {tag_it, tag_it + expr_range.value().second}, variation_type.value());
                                if (!expr.has_value()) {
                                    return std::nullopt;
                                }
                            }
                            LitValue lit_value = LitVariant{
                                .variant_type = type,
                                .variation_tag = tag,
                                .expr = std::move(expr),
                            };
                            return std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lit_value, type);
                        }
                        break;
                    }
                }
            }
            // The first element should be the alias token
            ASSERT(tokens_mut.first->token == TOK_ALIAS);
            Namespace *alias_namespace = tokens_mut.first->alias_namespace;
            tokens_mut.first++;
            // Then a dot should follow
            ASSERT(tokens_mut.first->token == TOK_DOT);
            tokens_mut.first++;
            auto call_node = create_call_expression(ctx, scope, tokens_mut, alias_namespace);
            if (!call_node.has_value()) {
                return std::nullopt;
            }
            return std::move(call_node.value());
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::function_call) || Matcher::tokens_match(tokens, Matcher::instance_call)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().second == token_size) {
            // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is something
            // located on the right of the call still
            auto call_node = create_call_expression(ctx, scope, tokens_mut, std::nullopt);
            if (!call_node.has_value()) {
                return std::nullopt;
            }
            return call_node;
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::group_expression)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().first == 0 && range.value().second == token_size) {
            std::optional<GroupExpressionNode> group = create_group_expression(ctx, scope, tokens_mut);
            if (!group.has_value()) {
                return std::nullopt;
            }
            return std::make_unique<GroupExpressionNode>(std::move(group.value()));
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::type_cast)) {
        if (primitives.find(tokens_mut.first->type->to_string()) == primitives.end()) {
            if (tokens_mut.first->type->get_variation() == Type::Variation::ARRAY) {
                // It's an array initializer
                return create_array_initializer(ctx, scope, tokens);
            }
            // It's an initializer
            std::optional<std::unique_ptr<ExpressionNode>> initializer = create_initializer(ctx, scope, tokens_mut);
            if (!initializer.has_value()) {
                return std::nullopt;
            }
            return initializer;
        } else if (tokens_mut.first->type->get_variation() == Type::Variation::MULTI && tokens_mut.first->type->to_string() != "bool8") {
            // It's an explicit initializer of an multi-type
            std::optional<std::unique_ptr<ExpressionNode>> initializer = create_initializer(ctx, scope, tokens_mut);
            if (!initializer.has_value()) {
                return std::nullopt;
            }
            return initializer;
        } else {
            // It's a regular type-cast (only primitive types can be cast and primitive types have no initializer)
            std::optional<std::unique_ptr<ExpressionNode>> type_cast = create_type_cast(ctx, scope, tokens_mut);
            if (!type_cast.has_value()) {
                return std::nullopt;
            }
            return type_cast;
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::anonymous_error)) {
        return create_anonymous_error(ctx, scope, tokens_mut);
    }
    if (Matcher::tokens_start_with_continuous(tokens_mut, Matcher::unary_pre_operator, Matcher::expression_separator)   //
        || Matcher::tokens_end_with_continuous(tokens_mut, Matcher::unary_post_operator, Matcher::expression_separator) //
    ) {
        std::optional<UnaryOpExpression> unary_op = create_unary_op_expression(ctx, scope, tokens_mut);
        if (!unary_op.has_value()) {
            return std::nullopt;
        }
        return std::make_unique<UnaryOpExpression>(std::move(unary_op.value()));
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::type_field_access)) {
        if (token_size == 3 || (token_size == 4 && std::prev(tokens_mut.second)->token == TOK_INT_VALUE)) {
            ASSERT(tokens_mut.first->token == TOK_TYPE);
            const std::shared_ptr<Type> type = tokens_mut.first->type;
            switch (type->get_variation()) {
                default:
                    break;
                case Type::Variation::DATA: {
                    const auto *data_type = type->as<DataType>();
                    if (!data_type->data_node->is_const && !data_type->data_node->is_shared) {
                        // Accessing fields from a type that's not const or shared is not allowed
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    ASSERT((tokens_mut.first + 1)->token == TOK_DOT);
                    ASSERT((tokens_mut.first + 2)->token == TOK_IDENTIFIER);
                    const std::string field_name((tokens_mut.first + 2)->lexme);
                    const auto &fields = data_type->data_node->fields;
                    auto field = fields.begin();
                    for (; field != fields.end(); ++field) {
                        if (field->name == field_name) {
                            break;
                        }
                    }
                    if (field == fields.end()) {
                        // Accessing nonexistent field of global const or shared data
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    // For shared data, return a VariableNode referencing the global variable
                    if (data_type->data_node->is_shared) {
                        const std::string hash_str = data_type->data_node->file_hash.to_string();
                        const std::string mangled_name = hash_str + ".shared." + data_type->data_node->name + "." + field_name;
                        return std::make_unique<VariableNode>(file_hash, get_pos_triple(tokens_mut), mangled_name, field->type, false);
                    }
                    ASSERT(field->initializer.has_value());
                    return field->initializer.value()->clone(scope->scope_id);
                }
                case Type::Variation::ENUM: {
                    const auto *enum_type = type->as<EnumType>();
                    ASSERT((tokens_mut.first + 1)->token == TOK_DOT);
                    ASSERT((tokens_mut.first + 2)->token == TOK_IDENTIFIER);
                    const std::string value((tokens_mut.first + 2)->lexme);
                    const auto &values = enum_type->enum_node->values;
                    bool value_exists = false;
                    for (const auto &[v, i] : values) {
                        if (v == value) {
                            value_exists = true;
                            break;
                        }
                    }
                    if (!value_exists) {
                        // Unsupported enum value
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    LitValue lit_value = LitEnum{.enum_type = type, .values = std::vector<std::string>{value}};
                    return std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lit_value, type);
                }
                case Type::Variation::ERROR_SET: {
                    const auto *error_type = type->as<ErrorSetType>();
                    ASSERT((tokens_mut.first + 1)->token == TOK_DOT);
                    ASSERT((tokens_mut.first + 2)->token == TOK_IDENTIFIER);
                    const std::string value((tokens_mut.first + 2)->lexme);
                    const auto pair = error_type->error_node->get_id_msg_pair_of_value(value);
                    if (!pair.has_value()) {
                        // Unsupported error value
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    LitValue lit_value = LitError{.error_type = type, .value = value, .message = std::nullopt};
                    return std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lit_value, type);
                }
                case Type::Variation::VARIANT: {
                    const auto *variant_type = type->as<VariantType>();
                    ASSERT((tokens_mut.first + 1)->token == TOK_DOT);
                    const auto tag_it = tokens_mut.first + 2;
                    ASSERT(tag_it->token == TOK_IDENTIFIER || tag_it->token == TOK_TYPE);
                    const std::string tag = (tag_it->token == TOK_IDENTIFIER) ? std::string(tag_it->lexme) : tag_it->type->to_string();
                    const auto &possible_types = variant_type->get_possible_types();
                    std::optional<std::shared_ptr<Type>> variation_type;
                    for (const auto &[possible_tag, var_type] : possible_types) {
                        if (possible_tag.has_value() && possible_tag.value() == tag) {
                            variation_type = var_type;
                            break;
                        }
                    }
                    if (!variation_type.has_value()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    LitValue lit_value = LitVariantTag{.variant_type = type, .variation_tag = tag};
                    return std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lit_value, type);
                }
            }
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::function_reference)) {
        if (token_size == 2 || token_size == 3) {
            return create_function_reference(tokens_mut);
        }
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::optional_chain, Matcher::expression_separator)) {
        std::optional<OptionalChainNode> chain = create_optional_chain(ctx, scope, tokens_mut);
        if (!chain.has_value()) {
            return std::nullopt;
        }
        return std::make_unique<OptionalChainNode>(std::move(chain.value()));
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::data_access, Matcher::expression_separator)) {
        std::optional<DataAccessNode> data_access = create_data_access(ctx, scope, tokens_mut);
        if (!data_access.has_value()) {
            return std::nullopt;
        }
        return std::make_unique<DataAccessNode>(std::move(data_access.value()));
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::grouped_data_access, Matcher::expression_separator)) {
        return create_grouped_data_access(ctx, scope, tokens_mut);
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::array_initializer)) {
        const bool is_array = tokens_mut.first->token == TOK_TYPE && tokens_mut.first->type->get_variation() == Type::Variation::ARRAY;
        const bool brackets_follow_type = std::next(tokens_mut.first)->token == TOK_LEFT_BRACKET;
        if (is_array || brackets_follow_type) {
            return create_array_initializer(ctx, scope, tokens_mut);
        }
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::array_access, Matcher::expression_separator)) {
        std::optional<ArrayAccessNode> access = create_array_access(ctx, scope, tokens_mut);
        if (!access.has_value()) {
            return std::nullopt;
        }
        return std::make_unique<ArrayAccessNode>(std::move(access.value()));
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::grouped_array_access, Matcher::expression_separator)) {
        std::optional<GroupedArrayAccessNode> access = create_grouped_array_access(ctx, scope, tokens_mut);
        if (!access.has_value()) {
            return std::nullopt;
        }
        return std::make_unique<GroupedArrayAccessNode>(std::move(access.value()));
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::optional_unwrap, Matcher::expression_separator)) {
        std::optional<std::unique_ptr<ExpressionNode>> unwrap = create_optional_unwrap(ctx, scope, tokens_mut);
        if (!unwrap.has_value()) {
            return std::nullopt;
        }
        return std::move(unwrap.value());
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::variant_extraction, Matcher::expression_separator)) {
        std::optional<VariantExtractionNode> extraction = create_variant_extraction(ctx, scope, tokens_mut);
        if (!extraction.has_value()) {
            return std::nullopt;
        }
        return std::make_unique<VariantExtractionNode>(std::move(extraction.value()));
    }
    if (Matcher::tokens_end_with_continuous(tokens_mut, Matcher::variant_unwrap, Matcher::expression_separator)) {
        std::optional<std::unique_ptr<ExpressionNode>> unwrap = create_variant_unwrap(ctx, scope, tokens_mut);
        if (!unwrap.has_value()) {
            return std::nullopt;
        }
        return std::move(unwrap.value());
    }
    const std::vector<uint2> range_expr_matches = Matcher::get_match_ranges_in_range_outside_group( //
        tokens_mut,                                                                                 //
        Matcher::range_expression,                                                                  //
        {0, std::distance(tokens_mut.first, tokens_mut.second)},                                    //
        Matcher::token(TOK_LEFT_BRACKET),                                                           //
        Matcher::token(TOK_RIGHT_BRACKET)                                                           //
    );
    if (range_expr_matches.size() == 1) {
        std::optional<std::unique_ptr<ExpressionNode>> range = create_range_expression(ctx, scope, tokens_mut);
        if (!range.has_value()) {
            return std::nullopt;
        }
        return std::move(range.value());
    }

    // Find the highest precedence operator
    unsigned int smallest_precedence = 100; //
    size_t pivot_pos = 0;
    Token pivot_token = TOK_EOL;

    // Find all possible binary operators at the root level
    // Start at the first index because the first token is never a unary operator
    for (auto it = std::next(tokens_mut.first); it != tokens_mut.second; ++it) {
        // Skip tokens inside parentheses or function calls
        if (std::prev(it)->token == TOK_LEFT_PAREN) {
            if (it->token == TOK_RIGHT_PAREN) {
                // Skip the call entirely if there is nothing inside the parenthesis
                continue;
            }
            int paren_depth = 1;
            while (++it != tokens_mut.second && paren_depth > 0) {
                if (it->token == TOK_LEFT_PAREN) {
                    paren_depth++;
                } else if (it->token == TOK_RIGHT_PAREN) {
                    paren_depth--;
                }
            }
            if (it == tokens_mut.second) {
                break;
            }
        }

        // Check if this is a operator and if no operator is to the left of this operator. If there is any operator to the left of this
        // one, this means that this operator is an unary operator
        if (token_precedence.find(it->token) != token_precedence.end() &&
            token_precedence.find(std::prev(it)->token) == token_precedence.end()) {
            // Update smallest precedence if needed
            const unsigned int precedence = token_precedence.at(it->token);
            const Associativity associativity = token_associativity.at(it->token);
            if ((precedence <= smallest_precedence && associativity == Associativity::LEFT) ||
                (precedence < smallest_precedence && associativity == Associativity::RIGHT)) {
                smallest_precedence = precedence;
                pivot_pos = std::distance(tokens_mut.first, it);
                pivot_token = it->token;
            }
        }
    }

    // If no binary operators, this is an error
    if (smallest_precedence == 0) {
        return std::nullopt;
    }

    // Extract the left and right parts of the expression
    token_slice lhs_tokens = {tokens_mut.first, tokens_mut.first + pivot_pos};
    token_slice rhs_tokens = {tokens_mut.first + pivot_pos + 1, tokens_mut.second};
    if (lhs_tokens.first == lhs_tokens.second) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (rhs_tokens.first == rhs_tokens.second) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Recursively parse both sides
    auto lhs = create_pivot_expression(ctx, scope, lhs_tokens, expected_type);
    if (!lhs.has_value()) {
        return std::nullopt;
    }

    auto rhs = create_pivot_expression(ctx, scope, rhs_tokens, expected_type);
    if (!rhs.has_value()) {
        return std::nullopt;
    }

    // Check if both sides of the binop match, if they don't then crash
    if (!lhs.value()->type->equals(rhs.value()->type)) {
        // Check if the operator is a optional default, in this case we need to check whether the lhs is an optional and whether the rhs
        // is the base type of the optional, otherwise it is considered an error
        if (pivot_token == TOK_OPT_DEFAULT) {
            if (lhs.value()->type->get_variation() != Type::Variation::OPTIONAL) {
                // ?? operator not possible on non-optional type
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            const auto *lhs_opt = lhs.value()->type->as<OptionalType>();
            if (!check_castability(lhs_opt->base_type, rhs.value(), true)) {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, lhs_opt->base_type, rhs.value()->type);
                return std::nullopt;
            }
            return std::make_unique<BinaryOpNode>(                                                           //
                file_hash, get_pos_triple(tokens), pivot_token, lhs.value(), rhs.value(), lhs_opt->base_type //
            );
        } else {
            // Check if one of the sides is a homogeneous group variation of the other side
            // This only works if the *other side*s type is comparable at all. Only primitive types and enums are comparable
            bool is_castable = true;
            const std::shared_ptr<Type> &lhs_type = lhs.value()->type;
            const std::shared_ptr<Type> &rhs_type = rhs.value()->type;

            const auto lhs_variation = lhs_type->get_variation();
            const auto rhs_variation = rhs_type->get_variation();

            const bool lhs_is_group = lhs_variation == Type::Variation::GROUP;
            const bool rhs_is_group = rhs_variation == Type::Variation::GROUP;
            const bool lhs_is_comparable = lhs_variation == Type::Variation::ENUM || lhs_variation == Type::Variation::PRIMITIVE;
            const bool rhs_is_comparable = rhs_variation == Type::Variation::ENUM || rhs_variation == Type::Variation::PRIMITIVE;

            if (lhs_is_group && rhs_is_comparable) {
                // All elements of the lhs group must match the rhs type, otherwise it's not a homogenous group
                const GroupType *lhs_group_type = lhs_type->as<GroupType>();
                GroupExpressionNode *lhs_group_expr = dynamic_cast<GroupExpressionNode *>(rhs.value().get());
                const bool rhs_is_literal = rhs_type->to_string() == "int" || rhs_type->to_string() == "float";
                const std::shared_ptr<Type> cmp_type = rhs_is_literal ? lhs_group_type->types.front() : rhs_type;
                for (size_t i = 0; i < lhs_group_type->types.size(); i++) {
                    const auto &type = lhs_group_type->types.at(i);
                    if (lhs_group_expr != nullptr) {
                        if (!check_castability(rhs_type, lhs_group_expr->expressions.at(i))) {
                            is_castable = false;
                            break;
                        }
                        continue;
                    }
                    if (!type->equals(cmp_type)) {
                        is_castable = false;
                        break;
                    }
                }
                if (is_castable && rhs_is_literal) {
                    // Set the type of the rhs literal to mark it as "resolved"
                    rhs.value()->type = cmp_type;
                }
            } else if (rhs_is_group && lhs_is_comparable) {
                // All elements of the rhs group must match the lhs type or be castable to it, otherwise it's not a homogenous group
                const GroupType *rhs_group_type = rhs_type->as<GroupType>();
                GroupExpressionNode *rhs_group_expr = dynamic_cast<GroupExpressionNode *>(rhs.value().get());
                const bool lhs_is_literal = lhs_type->to_string() == "int" || lhs_type->to_string() == "float";
                const std::shared_ptr<Type> cmp_type = lhs_is_literal ? rhs_group_type->types.front() : lhs_type;
                for (size_t i = 0; i < rhs_group_type->types.size(); i++) {
                    const auto &type = rhs_group_type->types.at(i);
                    if (rhs_group_expr != nullptr) {
                        if (!check_castability(lhs_type, rhs_group_expr->expressions.at(i))) {
                            is_castable = false;
                            break;
                        }
                        continue;
                    }
                    if (!type->equals(lhs_type)) {
                        const std::string type_str = type->to_string();
                        if (type_str == "int" || type_str == "float")
                            is_castable = false;
                        break;
                    }
                }
                if (is_castable && lhs_is_literal) {
                    // Set the type of the lhs literal to mark it as "resolved"
                    lhs.value()->type = cmp_type;
                }
            } else if (lhs_is_group && rhs_is_group) {
                // Both sides are groups, each element of each side must be castable or equal to the other side
                // For example the groups (int, i32) and (i64, int) should result in both sides being of type (i64, i32)
                //
                // Non-group expressions could also have a group type as their result. Only GroupExpressionNodes can be cast to other group
                // types, for example if we do a function call which returns `(u32, i32)` then we cannot cast it's expressions directly. For
                // this case the whole group needs to be cast. from `(u32, i32) -> (u64, i64)` for example. This means that we have three
                // four distinct possibilities to account for:
                // - both sides are group expressions
                // - left group expression, right other expression returning a group
                // - left some expression returning a group, right group expression
                // - none of the sides are group expressions
                const GroupType *lhs_group_type = lhs_type->as<GroupType>();
                const GroupType *rhs_group_type = rhs_type->as<GroupType>();
                GroupExpressionNode *lhs_group_expr = dynamic_cast<GroupExpressionNode *>(lhs.value().get());
                GroupExpressionNode *rhs_group_expr = dynamic_cast<GroupExpressionNode *>(rhs.value().get());
                if (lhs_group_type->types.size() == rhs_group_type->types.size()) {
                    if (lhs_group_expr != nullptr && rhs_group_expr != nullptr) {
                        // Both sides are group expressions
                        for (size_t i = 0; i < lhs_group_type->types.size(); i++) {
                            if (!check_castability(lhs_group_expr->expressions.at(i), rhs_group_expr->expressions.at(i))) {
                                is_castable = false;
                                break;
                            }
                        }
                    } else if (lhs_group_expr != nullptr && rhs_group_expr == nullptr) {
                        // Rhs is no group expr, lhs is a group expr
                        is_castable = check_castability(rhs_type, lhs.value());
                    } else if (lhs_group_expr == nullptr && rhs_group_expr != nullptr) {
                        // Lhs is no group expr, rhs is a group expr
                        is_castable = check_castability(lhs_type, rhs.value());
                    } else {
                        // TODO: Both sides are non-group expressions
                        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                        is_castable = false;
                    }
                }
            } else {
                is_castable = check_castability(lhs.value(), rhs.value());
            }
            if (!is_castable) {
                THROW_ERR(ErrExprBinopTypeMismatch, ERR_PARSING, file_hash,                           //
                    lhs_tokens, rhs_tokens, pivot_token, lhs_type->to_string(), rhs_type->to_string() //
                );
                return std::nullopt;
            }
        }
    }

    // Check for const folding, and return the folded value if const folding was able to be applied
    std::optional<std::unique_ptr<ExpressionNode>> folded_result = check_const_folding(lhs.value(), pivot_token, rhs.value());
    if (folded_result.has_value()) {
        return std::move(folded_result.value());
    }

    // Finally check if one of the two sides are string literals, if they are they need to become a string variable
    if (lhs.value()->type->to_string() == "type.flint.str.lit") {
        lhs = std::make_unique<TypeCastNode>(file_hash, get_pos_triple(tokens), Type::get_primitive_type("str"), lhs.value());
    }
    if (rhs.value()->type->to_string() == "type.flint.str.lit") {
        rhs = std::make_unique<TypeCastNode>(file_hash, get_pos_triple(tokens), Type::get_primitive_type("str"), rhs.value());
    }

    // Check it the binary operator is a `catch` keyword, if so the lhs should be a function call. We then set it's "has_catch" field to
    // true
    if (pivot_token == TOK_CATCH) {
        switch (lhs.value()->get_variation()) {
            default:
                // Not allowed lhs expression to catch binop
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            case ExpressionNode::Variation::CALL:
                lhs.value()->as<CallNodeExpression>()->has_catch = true;
                break;
            case ExpressionNode::Variation::CALLABLE_CALL:
                lhs.value()->as<CallableCallNodeExpression>()->has_catch = true;
                break;
            case ExpressionNode::Variation::INSTANCE_CALL:
                lhs.value()->as<InstanceCallNodeExpression>()->has_catch = true;
                break;
        }
    }

    // Create the binary operator node
    if (Matcher::token_match(pivot_token, Matcher::relational_binop)) {
        return std::make_unique<BinaryOpNode>(                                                                         //
            file_hash, get_pos_triple(tokens), pivot_token, lhs.value(), rhs.value(), Type::get_primitive_type("bool") //
        );
    }
    return std::make_unique<BinaryOpNode>(                                                          //
        file_hash, get_pos_triple(tokens), pivot_token, lhs.value(), rhs.value(), lhs.value()->type //
    );
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_expression( //
    const Context &ctx,                                                   //
    std::shared_ptr<Scope> &scope,                                        //
    const token_slice &tokens,                                            //
    const std::optional<std::shared_ptr<Type>> &expected_type             //
) {
    token_slice expr_tokens = tokens;
    remove_trailing_garbage(expr_tokens);

    // Parse expression using precedence levels
    auto expression = create_pivot_expression(ctx, scope, expr_tokens, expected_type);

    if (!expression.has_value()) {
        return std::nullopt;
    }

    // Check if the types are implicitely type castable, if they are, wrap the expression in a TypeCastNode
    if (expected_type.has_value() && !expected_type.value()->equals(expression.value()->type)) {
        switch (expected_type.value()->get_variation()) {
            default: {
                if (!check_castability(expected_type.value(), expression.value(), true)) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, expected_type.value(), expression.value()->type);
                    return std::nullopt;
                }
                break;
            }
            case Type::Variation::ERROR_SET: {
                const auto *target_error_type = expected_type.value()->as<ErrorSetType>();
                if (expression.value()->type->get_variation() != Type::Variation::ERROR_SET) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, expected_type.value(), expression.value()->type);
                    return std::nullopt;
                }
                const auto *expr_error_type = expression.value()->type->as<ErrorSetType>();
                // The expr error set type needs to be a superset of the target error type to be castable to it, this means that the
                // expression type "extends" the target type
                std::optional<const ErrorNode *> parent_node = target_error_type->error_node;
                bool is_castable = false;
                while (parent_node.has_value()) {
                    if (parent_node.value() == expr_error_type->error_node) {
                        is_castable = true;
                        break;
                    }
                    parent_node = parent_node.value()->get_parent_node();
                }
                if (!is_castable) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, expected_type.value(), expression.value()->type);
                    return std::nullopt;
                }
                expression = std::make_unique<TypeCastNode>(file_hash, get_pos_triple(tokens), expected_type.value(), expression.value());
                break;
            }
        }
    }

    expression.value()->file_hash = file_hash;
    expression.value()->line = tokens.first->line;
    expression.value()->column = tokens.first->column;
    expression.value()->length = tokens.second->column - tokens.first->column;
    return expression;
}
