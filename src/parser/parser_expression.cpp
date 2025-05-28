#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/builtins.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/parser.hpp"

#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/tuple_type.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <variant>

bool Parser::check_castability(std::unique_ptr<ExpressionNode> &lhs, std::unique_ptr<ExpressionNode> &rhs) {
    const GroupType *lhs_group = dynamic_cast<const GroupType *>(lhs->type.get());
    const GroupType *rhs_group = dynamic_cast<const GroupType *>(rhs->type.get());
    if (lhs_group == nullptr && rhs_group == nullptr) {
        // Both single type
        // If one of them is a multi-type, the other one has to be a single value with the same type as the base type of the mutli-type
        const MultiType *lhs_mult = dynamic_cast<const MultiType *>(lhs->type.get());
        const MultiType *rhs_mult = dynamic_cast<const MultiType *>(rhs->type.get());
        if (lhs_mult != nullptr && rhs_mult == nullptr && lhs_mult->base_type == rhs->type) {
            rhs = std::make_unique<TypeCastNode>(lhs->type, rhs);
            return true;
        } else if (lhs_mult == nullptr && rhs_mult != nullptr && lhs->type == rhs_mult->base_type) {
            lhs = std::make_unique<TypeCastNode>(rhs->type, lhs);
            return true;
        }
        if (lhs->type->to_string() == "__flint_type_str_lit" && rhs->type->to_string() == "str") {
            lhs = std::make_unique<TypeCastNode>(rhs->type, lhs);
            return true;
        } else if (lhs->type->to_string() == "str" && rhs->type->to_string() == "__flint_type_str_lit") {
            rhs = std::make_unique<TypeCastNode>(lhs->type, rhs);
            return true;
        }
        if (type_precedence.find(lhs->type->to_string()) == type_precedence.end() ||
            type_precedence.find(rhs->type->to_string()) == type_precedence.end()) {
            // Not castable, wrong arg types
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        const unsigned int lhs_precedence = type_precedence.at(lhs->type->to_string());
        const unsigned int rhs_precedence = type_precedence.at(rhs->type->to_string());
        if (lhs_precedence > rhs_precedence) {
            // The right type needs to be cast to the left type
            rhs = std::make_unique<TypeCastNode>(lhs->type, rhs);
        } else {
            // The left type needs to be cast to the right type
            lhs = std::make_unique<TypeCastNode>(rhs->type, lhs);
        }
    } else if (lhs_group == nullptr && rhs_group != nullptr) {
        // Left is no group, right is group
        // Check if left is a multi-type, then the right is castable to the left
        const MultiType *lhs_mult = dynamic_cast<const MultiType *>(lhs->type.get());
        if (lhs_mult == nullptr) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        // The group must have the same size as the multi-type
        if (lhs_mult->width != rhs_group->types.size()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        // All elements in the group must have the same type as the multi-type
        for (size_t i = 0; i < lhs_mult->width; i++) {
            if (lhs_mult->base_type != rhs_group->types[i]) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
        }
        rhs = std::make_unique<TypeCastNode>(lhs->type, rhs);
    } else if (lhs_group != nullptr && rhs_group == nullptr) {
        // Left is group, right is no group
        // Check if right is a multi-type, then the left is castable to the right
        const MultiType *rhs_mult = dynamic_cast<const MultiType *>(rhs->type.get());
        if (rhs_mult == nullptr) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        // The group must have the same size as the multi-type
        if (rhs_mult->width != lhs_group->types.size()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        // All elements in the group must have the same type as the multi-type
        for (size_t i = 0; i < rhs_mult->width; i++) {
            if (rhs_mult->base_type != lhs_group->types[i]) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
        }
        lhs = std::make_unique<TypeCastNode>(rhs->type, lhs);
    } else {
        // Both group
        // TODO
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return false;
    }
    return true;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::check_const_folding( //
    std::unique_ptr<ExpressionNode> &lhs,                                   //
    const Token operation,                                                  //
    std::unique_ptr<ExpressionNode> &rhs                                    //
) {
    // The lhs and rhs types must be the same to be folded
    if (lhs->type != rhs->type) {
        return std::nullopt;
    }
    // Currently, only literals can be const folded
    const LiteralNode *lhs_ptr = dynamic_cast<const LiteralNode *>(lhs.get());
    const LiteralNode *rhs_ptr = dynamic_cast<const LiteralNode *>(rhs.get());
    if (lhs_ptr == nullptr || rhs_ptr == nullptr) {
        return std::nullopt;
    }
    // Const folding can only be applied if the binary operator is an arithmetic operation
    if (!Matcher::token_match(operation, Matcher::operational_binop) && !Matcher::token_match(operation, Matcher::boolean_binop)) {
        return std::nullopt;
    }

    // Add the two literals together
    std::optional<std::unique_ptr<LiteralNode>> result = add_literals(lhs_ptr, operation, rhs_ptr);
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
    switch (operation) {
        default:
            // It should never come here, if it did something went wrong
            assert(false);
            break;
        case TOK_PLUS:
            if (std::holds_alternative<int>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<int>(lhs->value) + std::get<int>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<float>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<float>(lhs->value) + std::get<float>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<std::string>(lhs->value)) {
                return std::make_unique<LiteralNode>(                                                      //
                    std::get<std::string>(lhs->value) + std::get<std::string>(rhs->value), lhs->type, true //
                );
            } else if (std::holds_alternative<char>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<char>(lhs->value) + std::get<char>(rhs->value), lhs->type, true);
            }
            break;
        case TOK_MINUS:
            if (std::holds_alternative<int>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<int>(lhs->value) - std::get<int>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<float>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<float>(lhs->value) - std::get<float>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<char>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<char>(lhs->value) - std::get<char>(rhs->value), lhs->type, true);
            }
            break;
        case TOK_MULT:
            if (std::holds_alternative<int>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<int>(lhs->value) * std::get<int>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<float>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<float>(lhs->value) * std::get<float>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<char>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<char>(lhs->value) * std::get<char>(rhs->value), lhs->type, true);
            }
            break;
        case TOK_DIV:
            if (std::holds_alternative<int>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<int>(lhs->value) / std::get<int>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<float>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<float>(lhs->value) / std::get<float>(rhs->value), lhs->type, true);
            } else if (std::holds_alternative<char>(lhs->value)) {
                return std::make_unique<LiteralNode>(std::get<char>(lhs->value) / std::get<char>(rhs->value), lhs->type, true);
            }
            break;
        case TOK_POW:
            if (std::holds_alternative<int>(lhs->value)) {
                return std::make_unique<LiteralNode>(static_cast<int>(std::pow(std::get<int>(lhs->value), std::get<int>(rhs->value))),
                    lhs->type, true);
            } else if (std::holds_alternative<float>(lhs->value)) {
                return std::make_unique<LiteralNode>(                                                                       //
                    static_cast<float>(std::pow(std::get<float>(lhs->value), std::get<float>(rhs->value))), lhs->type, true //
                );
            } else if (std::holds_alternative<char>(lhs->value)) {
                return std::make_unique<LiteralNode>(                                                                    //
                    static_cast<char>(std::pow(std::get<char>(lhs->value), std::get<char>(rhs->value))), lhs->type, true //
                );
            }
            break;
        case TOK_AND:
            if (std::holds_alternative<bool>(lhs->value)) {
                return std::make_unique<LiteralNode>(                                         //
                    std::get<bool>(lhs->value) && std::get<bool>(rhs->value), lhs->type, true //
                );
            }
            break;
        case TOK_OR:
            if (std::holds_alternative<bool>(lhs->value)) {
                return std::make_unique<LiteralNode>(                                         //
                    std::get<bool>(lhs->value) || std::get<bool>(rhs->value), lhs->type, true //
                );
            }
            break;
    }
    return std::nullopt;
}

std::optional<VariableNode> Parser::create_variable(Scope *scope, const token_slice &tokens) {
    std::optional<VariableNode> var = std::nullopt;
    for (auto tok = tokens.first; tok != tokens.second; tok++) {
        if (tok->type == TOK_IDENTIFIER) {
            std::string name = tok->lexme;
            if (scope->variables.find(name) == scope->variables.end()) {
                THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, tok->line, tok->column, name);
                return std::nullopt;
            }
            return VariableNode(name, std::get<0>(scope->variables.at(name)));
        }
    }
    return var;
}

std::optional<UnaryOpExpression> Parser::create_unary_op_expression(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    auto unary_op_values = create_unary_op_base(scope, tokens_mut);
    if (!unary_op_values.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return UnaryOpExpression(                 //
        std::get<0>(unary_op_values.value()), //
        std::get<1>(unary_op_values.value()), //
        std::get<2>(unary_op_values.value())  //
    );
}

std::optional<LiteralNode> Parser::create_literal(const token_slice &tokens) {
    // Literals can have a size of at most 2 tokens
    if (get_slice_size(tokens) > 2) {
        return std::nullopt;
    }
    // If the tokens are 2 long we have a literal expression
    Token front_token = TOK_EOF;
    token_list::iterator tok;
    if (get_slice_size(tokens) == 2) {
        // Currently the only literal experssion is a minus sign in front of the literal, or a $ sign in front of the string
        if (tokens.first->type == TOK_MINUS) {
            front_token = TOK_MINUS;
            tok = (tokens.first + 1);
        } else if (tokens.first->type == TOK_DOLLAR) {
            front_token = TOK_DOLLAR;
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
        }
    } else {
        tok = tokens.first;
    }

    if (Matcher::tokens_match({tok, tok + 1}, Matcher::literal)) {
        switch (tok->type) {
            default:
                THROW_ERR(ErrValUnknownLiteral, ERR_PARSING, file_name, tok->line, tok->column, tok->lexme);
                return std::nullopt;
                break;
            case TOK_INT_VALUE: {
                if (front_token == TOK_MINUS) {
                    std::variant<int, float, std::string, bool, char> value = std::stoi(tok->lexme) * -1;
                    return LiteralNode(value, Type::get_primitive_type("i32"));
                } else {
                    std::variant<int, float, std::string, bool, char> value = std::stoi(tok->lexme);
                    return LiteralNode(value, Type::get_primitive_type("i32"));
                }
            }
            case TOK_FLINT_VALUE: {
                if (front_token == TOK_MINUS) {
                    std::variant<int, float, std::string, bool, char> value = std::stof(tok->lexme) * -1;
                    return LiteralNode(value, Type::get_primitive_type("f32"));
                } else {
                    std::variant<int, float, std::string, bool, char> value = std::stof(tok->lexme);
                    return LiteralNode(value, Type::get_primitive_type("f32"));
                }
            }
            case TOK_STR_VALUE: {
                if (front_token == TOK_DOLLAR) {
                    std::variant<int, float, std::string, bool, char> value = tok->lexme;
                    return LiteralNode(value, Type::get_primitive_type("str"));
                } else {
                    const std::string &str = tok->lexme;
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
                                case '0':
                                    processed_str << '\0';
                                    break;
                            }
                            i++; // Skip the next character
                        } else {
                            processed_str << str[i];
                        }
                    }
                    std::variant<int, float, std::string, bool, char> value = processed_str.str();
                    return LiteralNode(value, Type::get_primitive_type("__flint_type_str_lit"));
                }
            }
            case TOK_TRUE: {
                std::variant<int, float, std::string, bool, char> value = true;
                return LiteralNode(value, Type::get_primitive_type("bool"));
            }
            case TOK_FALSE: {
                std::variant<int, float, std::string, bool, char> value = false;
                return LiteralNode(value, Type::get_primitive_type("bool"));
            }
            case TOK_CHAR_VALUE: {
                std::variant<int, float, std::string, bool, char> value = tok->lexme[0];
                return LiteralNode(value, Type::get_primitive_type("u8"));
            }
        }
    }
    return std::nullopt;
}

std::optional<StringInterpolationNode> Parser::create_string_interpolation(Scope *scope, const std::string &interpol_string) {
    // First, get all balanced ranges of { } symbols which are not leaded by a \\ symbol
    std::vector<uint2> ranges = Matcher::balanced_ranges_vec(interpol_string, "([^\\\\]|)\\{", "[^\\\\]\\}");
    std::vector<std::variant<std::unique_ptr<TypeCastNode>, std::unique_ptr<LiteralNode>>> interpol_content;
    // If the ranges are empty, the interpolation does not contain any groups
    if (ranges.empty()) {
        interpol_content.emplace_back(std::make_unique<LiteralNode>(interpol_string, Type::get_primitive_type("str")));
        return StringInterpolationNode(interpol_content);
    }
    // First, add all the strings from the begin to the first ranges begin to the interpolation content
    for (auto it = ranges.begin(); it != ranges.end(); ++it) {
        // If its at the very beginning the conditions differs
        if (it->first > 0 ? (it->second - it->first <= 1) : (it->second == 0)) {
            // Empty expression
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (it == ranges.begin() && it->first > 0) {
            // Add string thats present before the first { symbol
            token_list lit_toks = {{TOK_STR_VALUE, interpol_string.substr(0, it->first + 1), 0, 0}};
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        } else if (it != ranges.begin() && it->first - std::prev(it)->second > 1) {
            // Add string in between } { symbols
            token_list lit_toks = {
                {TOK_STR_VALUE, interpol_string.substr(std::prev(it)->second + 2, it->first - std::prev(it)->second - 1), 0, 0} //
            };
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        }
        Lexer lexer = (it->first == 0) ? Lexer("string_intepolation", interpol_string.substr(1, it->second))
                                       : Lexer("string_interpolation", interpol_string.substr(it->first + 2, (it->second - it->first - 1)));
        token_list expr_tokens = lexer.scan();
        if (expr_tokens.empty()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        token_slice expr_tokens_slice = {expr_tokens.begin(), expr_tokens.end()};
        std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expr_tokens_slice);
        if (!expr.has_value()) {
            THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens_slice);
            return std::nullopt;
        }
        // Cast every expression inside to a str type
        interpol_content.emplace_back(std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), expr.value()));
        if (std::next(it) == ranges.end() && it->second + 2 < interpol_string.length()) {
            // Add string after last } symbol
            token_list lit_toks = {
                {TOK_STR_VALUE, interpol_string.substr(it->second + 2, (interpol_string.length() - it->second - 1)), 0, 0} //
            };
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        }
    }
    return StringInterpolationNode(interpol_content);
}

std::optional<std::variant<std::unique_ptr<CallNodeExpression>, std::unique_ptr<InitializerNode>>> //
Parser::create_call_or_initializer_expression(Scope *scope, const token_slice &tokens, const std::optional<std::string> &alias_base) {
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    auto call_or_init_node_args = create_call_or_initializer_base(scope, tokens_mut, alias_base);
    if (!call_or_init_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }
    // Now, check if its a initializer or a call
    if (std::get<3>(call_or_init_node_args.value()).has_value()) {
        // Its an initializer
        std::vector<std::unique_ptr<ExpressionNode>> args;
        for (auto &arg : std::get<1>(call_or_init_node_args.value())) {
            args.emplace_back(std::move(arg.first));
        }
        std::unique_ptr<InitializerNode> initializer_node = std::make_unique<InitializerNode>( //
            std::get<2>(call_or_init_node_args.value()),                                       // type
            std::get<3>(call_or_init_node_args.value()).value(),                               // is_data
            args                                                                               // args
        );
        return initializer_node;
    } else {
        // Its a call
        std::unique_ptr<CallNodeExpression> call_node = std::make_unique<CallNodeExpression>( //
            std::get<0>(call_or_init_node_args.value()),                                      // name
            std::get<1>(call_or_init_node_args.value()),                                      // args
            std::get<4>(call_or_init_node_args.value()),                                      // can_throw
            std::get<2>(call_or_init_node_args.value())                                       // type
        );
        call_node->scope_id = scope->scope_id;
        set_last_parsed_call(call_node->call_id, call_node.get());
        return call_node;
    }
}

std::optional<TypeCastNode> Parser::create_type_cast(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
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
    assert(expr_range.value().second > expr_range.value().first);

    // Get the type the expression needs to be converted to
    TokenContext type_token = TokenContext{TOK_EOF, "", 0, 0};
    for (auto iterator = tokens_mut.first; iterator != tokens_mut.second; ++iterator) {
        if (Matcher::tokens_match({iterator, iterator + 1}, Matcher::type_prim) && std::next(iterator) != tokens_mut.second &&
            std::next(iterator)->type == TOK_LEFT_PAREN) {
            type_token = *iterator;
            break;
        }
    }
    if (type_token.type == TOK_EOF) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Create the expression
    token_slice expr_tokens = {tokens_mut.first + expr_range.value().first, tokens_mut.first + expr_range.value().second};
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, expr_tokens);
    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
        return std::nullopt;
    }
    // if (!std::holds_alternative<std::shared_ptr<Type>>(expression.value()->type)) {
    //     if (std::get<std::vector<std::shared_ptr<Type>>>(expression.value()->type).size() == 1) {
    //         expression.value()->type = std::get<std::vector<std::shared_ptr<Type>>>(expression.value()->type).at(0);
    //     } else {
    //         THROW_BASIC_ERR(ERR_PARSING);
    //         return std::nullopt;
    //     }
    // }

    // Check if the type of the expression is castable at all
    if (primitive_casting_table.find(expression.value()->type->to_string()) == primitive_casting_table.end()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::vector<std::string_view> &to_types = primitive_casting_table.at(expression.value()->type->to_string());
    if (std::find(to_types.begin(), to_types.end(), type_token.lexme) == to_types.end()) {
        // The given expression type cannot be cast to the wanted type
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return TypeCastNode(Type::get_primitive_type(type_token.lexme), expression.value());
}

std::optional<GroupExpressionNode> Parser::create_group_expression(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    // First, remove all leading and trailing garbage from the expression tokens
    remove_leading_garbage(tokens_mut);
    remove_trailing_garbage(tokens_mut);
    // Now, the first and the last token must be open and closing parenthesis respectively
    assert(tokens_mut.first->type == TOK_LEFT_PAREN);
    assert(std::prev(tokens_mut.second)->type == TOK_RIGHT_PAREN);
    // Remove the open and closing parenthesis
    tokens_mut.first++;
    tokens_mut.second--;
    std::vector<std::unique_ptr<ExpressionNode>> expressions;
    while (tokens_mut.first != tokens_mut.second) {
        // Check if the tokens contain any opening / closing parenthesis. If it doesnt, the group parsing can be simplified a lot
        if (!Matcher::tokens_contain(tokens_mut, Matcher::token(TOK_LEFT_PAREN))) {
            // Extract all tokens until the comma if it contains a comma. If it does not contain a comma, we are at the end of the group
            if (!Matcher::tokens_contain(tokens_mut, Matcher::token(TOK_COMMA))) {
                auto expr = create_expression(scope, tokens_mut);
                if (!expr.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
                    return std::nullopt;
                }
                expressions.emplace_back(std::move(expr.value()));
                break;
            } else {
                std::optional<uint2> expr_range = Matcher::get_next_match_range(tokens_mut, Matcher::until_comma);
                if (!expr_range.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                token_slice expr_tokens = {tokens_mut.first + expr_range.value().first, tokens_mut.first + expr_range.value().second};
                tokens_mut.first += expr_range.value().second;
                // If the last token is a comma, it is removed
                if (std::prev(expr_tokens.second)->type == TOK_COMMA) {
                    expr_tokens.second--;
                }
                auto expr = create_expression(scope, expr_tokens);
                if (!expr.has_value()) {
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
                    return std::nullopt;
                }
                expressions.emplace_back(std::move(expr.value()));
            }
        } else if (Matcher::tokens_contain(tokens_mut, Matcher::token(TOK_COMMA))) {
            std::optional<uint2> expr_range = Matcher::get_next_match_range(tokens_mut, Matcher::until_comma);
            if (!expr_range.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            token_slice expr_tokens = {tokens_mut.first + expr_range.value().first, tokens_mut.first + expr_range.value().second};
            tokens_mut.first += expr_range.value().second;
            auto expr = create_expression(scope, expr_tokens);
            if (!expr.has_value()) {
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
                return std::nullopt;
            }
            expressions.emplace_back(std::move(expr.value()));
        } else {
            // THIS IS NOT A GROUPED EXPRESSION
            return std::nullopt;
        }
    }
    return GroupExpressionNode(expressions);
}

std::optional<std::vector<std::unique_ptr<ExpressionNode>>> Parser::create_group_expressions(Scope *scope, token_slice &tokens) {
    std::vector<std::unique_ptr<ExpressionNode>> expressions;
    while (tokens.first != tokens.second) {
        std::optional<uint2> next_expr_range = Matcher::get_next_match_range(tokens, Matcher::until_comma);
        if (!next_expr_range.has_value()) {
            // The last expression
            std::optional<std::unique_ptr<ExpressionNode>> indexing_expression = create_expression(scope, tokens);
            tokens.first = tokens.second;
            if (!indexing_expression.has_value()) {
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            expressions.emplace_back(std::move(indexing_expression.value()));
        } else {
            // Not the last expression
            std::optional<std::unique_ptr<ExpressionNode>> indexing_expression = create_expression( //
                scope, {tokens.first, tokens.first + next_expr_range.value().second - 1}            //
            );
            tokens.first += next_expr_range.value().second;
            if (!indexing_expression.has_value()) {
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            expressions.emplace_back(std::move(indexing_expression.value()));
        }
    }
    return expressions;
}

std::optional<DataAccessNode> Parser::create_data_access(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    auto field_access_base = create_field_access_base(scope, tokens_mut);
    if (!field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    std::variant<std::string, std::unique_ptr<ExpressionNode>> var_name = std::get<1>(field_access_base.value());
    return DataAccessNode(                      //
        std::get<0>(field_access_base.value()), // data_type
        var_name,                               // var_name
        std::get<2>(field_access_base.value()), // field_name
        std::get<3>(field_access_base.value()), // field_id
        std::get<4>(field_access_base.value())  // field_type
    );
}

std::optional<ArrayInitializerNode> Parser::create_array_initializer(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    std::optional<uint2> length_expression_range = Matcher::balanced_range_extraction(  //
        tokens_mut, Matcher::token(TOK_LEFT_BRACKET), Matcher::token(TOK_RIGHT_BRACKET) //
    );
    if (!length_expression_range.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Get the element type of the array
    token_slice type_tokens = {tokens_mut.first, tokens_mut.first + length_expression_range.value().first};
    tokens_mut.first += length_expression_range.value().first;
    if (!check_type_aliasing(type_tokens)) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::optional<std::shared_ptr<Type>> element_type = Type::get_type(type_tokens);
    if (!element_type.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Get the initializer tokens (...) and remove the surrounding parenthesis
    token_slice initializer_tokens = {tokens.first + length_expression_range.value().second, tokens_mut.second};
    tokens_mut.second = tokens.first + length_expression_range.value().second;
    remove_surrounding_paren(initializer_tokens);
    // Now we can create the initializer expression
    std::optional<std::unique_ptr<ExpressionNode>> initializer;
    if (std::next(initializer_tokens.first) == initializer_tokens.second && initializer_tokens.first->type == TOK_UNDERSCORE) {
        initializer = std::make_unique<DefaultNode>(element_type.value());
    } else {
        initializer = create_expression(scope, initializer_tokens);
    }
    if (!initializer.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, initializer_tokens);
        return std::nullopt;
    }

    // The first token in the tokens list should be a left bracket
    assert(tokens_mut.first->type == TOK_LEFT_BRACKET);
    tokens_mut.first++;
    // The last token in the tokens list should be a right bracket
    assert(std::prev(tokens_mut.second)->type == TOK_RIGHT_BRACKET);
    tokens_mut.second--;
    // Now, everything left in the `tokens_mut` vector should be the length expressions [...]
    auto length_expressions = create_group_expressions(scope, tokens_mut);
    if (!length_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    std::string actual_type_str = element_type.value()->to_string() + "[" + std::string(length_expressions.value().size() - 1, ',') + "]";
    std::optional<std::shared_ptr<Type>> actual_array_type = Type::get_type_from_str(actual_type_str);
    if (!actual_array_type.has_value()) {
        // This type does not yet exist, so we need to create it
        actual_array_type = std::make_shared<ArrayType>(length_expressions.value().size(), element_type.value());
        Type::add_type(actual_array_type.value());
    }
    return ArrayInitializerNode(    //
        actual_array_type.value(),  //
        length_expressions.value(), //
        initializer.value()         //
    );
}

std::optional<ArrayAccessNode> Parser::create_array_access(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    // The tokens should begin with an identifier, otherwise we have done something wrong somewhere
    assert(tokens_mut.first->type == TOK_IDENTIFIER);
    const std::string variable_name = tokens_mut.first->lexme;
    std::optional<std::shared_ptr<Type>> variable_type = scope->get_variable_type(variable_name);
    if (!variable_type.has_value()) {
        THROW_ERR(ErrVarNotDeclared, ERR_PARSING, file_name, tokens_mut.first->line, tokens_mut.first->column, variable_name);
        return std::nullopt;
    }
    const ArrayType *array_variable_type = dynamic_cast<const ArrayType *>(variable_type.value().get());
    if (array_variable_type == nullptr) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::shared_ptr<Type> result_type = array_variable_type->type;
    tokens_mut.first++;

    // Now the tokens should begin with a left bracket and end with a right bracket, otherwise we did something wrong elsewhere
    assert(tokens_mut.first->type == TOK_LEFT_BRACKET);
    tokens_mut.first++;
    assert(std::prev(tokens_mut.second)->type == TOK_RIGHT_BRACKET);
    tokens_mut.second--;
    std::optional<std::vector<std::unique_ptr<ExpressionNode>>> indexing_expressions = create_group_expressions(scope, tokens_mut);
    if (!indexing_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return ArrayAccessNode(result_type, variable_name, variable_type.value(), indexing_expressions.value());
}

std::optional<GroupedDataAccessNode> Parser::create_grouped_data_access(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    auto grouped_field_access_base = create_grouped_access_base(scope, tokens_mut);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return GroupedDataAccessNode(                       //
        std::get<0>(grouped_field_access_base.value()), // data_type
        std::get<1>(grouped_field_access_base.value()), // var_name
        std::get<2>(grouped_field_access_base.value()), // field_names
        std::get<3>(grouped_field_access_base.value()), // field_ids
        std::get<4>(grouped_field_access_base.value())  // field_types
    );
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_stacked_expression(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    auto separator = tokens_mut.second - 1;
    size_t depth = 0;
    for (; separator != tokens_mut.first; --separator) {
        if (separator->type == TOK_RIGHT_PAREN) {
            depth++;
        } else if (separator->type == TOK_LEFT_PAREN) {
            depth--;
        } else if (separator->type == TOK_DOT && depth == 0) {
            break;
        }
    }
    // The separator is now the dot that separates the lhs from the rhs
    // For now, only field accesses are supported, having `.call()` is not supported yet
    // TODO: Add support for the `.call()` expression stacking syntax
    token_slice left_tokens = {tokens_mut.first, separator};
    std::optional<std::unique_ptr<ExpressionNode>> left_expr = create_expression(scope, left_tokens, std::nullopt);
    if (!left_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Okay, so now the rhs is only an identifier
    ++separator;
    if (separator->type != TOK_IDENTIFIER) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::shared_ptr<Type> left_expr_type = left_expr.value()->type;
    const DataType *data_type = dynamic_cast<const DataType *>(left_expr_type.get());
    if (data_type == nullptr) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const DataNode *data_node = data_type->data_node;
    const std::string field_name = separator->lexme;
    size_t field_id = 0;
    for (const auto &field : data_node->fields) {
        if (std::get<0>(field) == field_name) {
            break;
        }
        field_id++;
    }
    if (field_id == data_node->fields.size()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::shared_ptr<Type> field_type = std::get<1>(data_node->fields[field_id]);
    std::variant<std::string, std::unique_ptr<ExpressionNode>> variable = std::move(left_expr.value());
    return std::make_unique<DataAccessNode>(left_expr_type, variable, field_name, field_id, field_type);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_pivot_expression(Scope *scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    token_list toks;
    if (DEBUG_MODE) {
        toks = clone_from_slice(tokens);
    }
    assert(tokens_mut.first != tokens_mut.second); // Assert that tokens is not empty
    if (!Matcher::tokens_match(tokens_mut, Matcher::group_expression)) {
        remove_surrounding_paren(tokens_mut);
    }

    // Try to parse primary expressions first (literal, variables)
    size_t token_size = get_slice_size(tokens_mut);
    if (token_size == 1) {
        if (Matcher::tokens_match(tokens_mut, Matcher::literal)) {
            std::optional<LiteralNode> lit = create_literal(tokens_mut);
            if (!lit.has_value()) {
                THROW_ERR(ErrExprLitCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return std::make_unique<LiteralNode>(std::move(lit.value()));
        } else if (Matcher::tokens_match(tokens_mut, Matcher::variable_expr)) {
            std::optional<VariableNode> variable = create_variable(scope, tokens_mut);
            if (!variable.has_value()) {
                THROW_ERR(ErrExprVariableCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return std::make_unique<VariableNode>(std::move(variable.value()));
        }
    } else if (token_size == 2) {
        if (Matcher::tokens_match(tokens_mut, Matcher::literal_expr)) {
            std::optional<LiteralNode> lit = create_literal(tokens_mut);
            if (!lit.has_value()) {
                THROW_ERR(ErrExprLitCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return std::make_unique<LiteralNode>(std::move(lit.value()));
        } else if (Matcher::tokens_match(tokens_mut, Matcher::string_interpolation)) {
            assert(tokens_mut.first->type == TOK_DOLLAR && std::prev(tokens_mut.second)->type == TOK_STR_VALUE);
            std::optional<StringInterpolationNode> interpol = create_string_interpolation(scope, std::prev(tokens_mut.second)->lexme);
            if (!interpol.has_value()) {
                THROW_ERR(ErrExprLitCreationFailed, ERR_PARSING, file_name, tokens);
                return std::nullopt;
            }
            return std::make_unique<StringInterpolationNode>(std::move(interpol.value()));
        }
    }

    if (Matcher::tokens_match(tokens_mut, Matcher::aliased_function_call)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().second == token_size) {
            // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is something
            // located on the right of the call still
            // The first element should be an initializer for the alias
            assert(tokens_mut.first->type == TOK_IDENTIFIER);
            const std::string alias_base = tokens_mut.first->lexme;
            tokens_mut.first++;
            // Then a dot should follow
            assert(tokens_mut.first->type == TOK_DOT);
            tokens_mut.first++;
            // The rest is the call itself
            auto call_or_initializer_expression = create_call_or_initializer_expression(scope, tokens_mut, alias_base);
            if (!call_or_initializer_expression.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            if (std::holds_alternative<std::unique_ptr<CallNodeExpression>>(call_or_initializer_expression.value())) {
                return std::move(std::get<std::unique_ptr<CallNodeExpression>>(call_or_initializer_expression.value()));
            } else {
                return std::move(std::get<std::unique_ptr<InitializerNode>>(call_or_initializer_expression.value()));
            }
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::function_call)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().second == token_size) {
            // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is something
            // located on the right of the call still
            auto call_or_initializer_expression = create_call_or_initializer_expression(scope, tokens_mut, std::nullopt);
            if (!call_or_initializer_expression.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            if (std::holds_alternative<std::unique_ptr<CallNodeExpression>>(call_or_initializer_expression.value())) {
                return std::move(std::get<std::unique_ptr<CallNodeExpression>>(call_or_initializer_expression.value()));
            } else {
                return std::move(std::get<std::unique_ptr<InitializerNode>>(call_or_initializer_expression.value()));
            }
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::group_expression)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().first == 0 && range.value().second == token_size) {
            std::optional<GroupExpressionNode> group = create_group_expression(scope, tokens_mut);
            if (!group.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<GroupExpressionNode>(std::move(group.value()));
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::type_cast)) {
        std::optional<TypeCastNode> type_cast = create_type_cast(scope, tokens_mut);
        if (!type_cast.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<TypeCastNode>(std::move(type_cast.value()));
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::unary_op_expr)) {
        // For it to be considered an unary operation, either right after the operator needs to come a paren group, or no other binop
        // tokens
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (!Matcher::tokens_contain(tokens_mut, Matcher::binary_operator) || (range.has_value() && range.value().second == token_size)) {
            std::optional<UnaryOpExpression> unary_op = create_unary_op_expression(scope, tokens_mut);
            if (!unary_op.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<UnaryOpExpression>(std::move(unary_op.value()));
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::data_access)) {
        if (token_size == 3 || (token_size == 4 && std::prev(tokens_mut.second)->type == TOK_INT_VALUE)) {
            std::optional<DataAccessNode> data_access = create_data_access(scope, tokens_mut);
            if (!data_access.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<DataAccessNode>(std::move(data_access.value()));
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::grouped_data_access)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().first == 2 && range.value().second == token_size) {
            std::optional<GroupedDataAccessNode> group_access = create_grouped_data_access(scope, tokens_mut);
            if (!group_access.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<GroupedDataAccessNode>(std::move(group_access.value()));
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::array_initializer)) {
        std::optional<ArrayInitializerNode> initializer = create_array_initializer(scope, tokens_mut);
        if (!initializer.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<ArrayInitializerNode>(std::move(initializer.value()));
    } else if (Matcher::tokens_match(tokens_mut, Matcher::array_access)) {
        std::optional<ArrayAccessNode> access = create_array_access(scope, tokens_mut);
        if (!access.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<ArrayAccessNode>(std::move(access.value()));
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::stacked_expression)) {
        return create_stacked_expression(scope, tokens_mut);
    }

    // Find the highest precedence operator
    unsigned int smallest_precedence = 100; //
    size_t pivot_pos = 0;
    Token pivot_token = TOK_EOL;

    // Find all possible binary operators at the root level
    // Start at the first index because the first token is never a unary operator
    for (auto it = std::next(tokens_mut.first); it != tokens_mut.second; ++it) {
        // Skip tokens inside parentheses or function calls
        if (it->type == TOK_LEFT_PAREN) {
            int paren_depth = 1;
            while (++it != tokens_mut.second && paren_depth > 0) {
                if (it->type == TOK_LEFT_PAREN) {
                    paren_depth++;
                } else if (it->type == TOK_RIGHT_PAREN) {
                    paren_depth--;
                }
            }
            if (it == tokens_mut.second) {
                break;
            }
        }

        // Check if this is a operator and if no operator is to the left of this operator. If there is any operator to the left of this
        // one, this means that this operator is an unary operator
        if (token_precedence.find(it->type) != token_precedence.end() &&
            token_precedence.find(std::prev(it)->type) == token_precedence.end()) {
            // Update smallest precedence if needed
            const unsigned int precedence = token_precedence.at(it->type);
            const Associativity associativity = token_associativity.at(it->type);
            if ((precedence <= smallest_precedence && associativity == Associativity::LEFT) ||
                (precedence < smallest_precedence && associativity == Associativity::RIGHT)) {
                smallest_precedence = precedence;
                pivot_pos = std::distance(tokens_mut.first, it);
                pivot_token = it->type;
            }
        }
    }

    // If no binary operators, this is an error
    if (smallest_precedence == 0) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    // Extract the left and right parts of the expression
    token_slice lhs_tokens = {tokens_mut.first, tokens_mut.first + pivot_pos};
    token_slice rhs_tokens = {tokens_mut.first + pivot_pos + 1, tokens_mut.second};

    // Recursively parse both sides
    auto lhs = create_pivot_expression(scope, lhs_tokens);
    if (!lhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, lhs_tokens);
        return std::nullopt;
    }

    auto rhs = create_pivot_expression(scope, rhs_tokens);
    if (!rhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, rhs_tokens);
        return std::nullopt;
    }

    // Check if all parameter types actually match the argument types
    // If we came until here, the arg count definitely matches the parameter count
    if (lhs.value()->type != rhs.value()->type) {
        if (!check_castability(lhs.value(), rhs.value())) {
            return std::nullopt;
        }
    }

    // Check for const folding, and return the folded value if const folding was able to be applied
    std::optional<std::unique_ptr<ExpressionNode>> folded_result = check_const_folding(lhs.value(), pivot_token, rhs.value());
    if (folded_result.has_value()) {
        return std::move(folded_result.value());
    }

    // Finally check if one of the two sides are string literals, if they are they need to become a string variable
    if (lhs.value()->type->to_string() == "__flint_type_str_lit") {
        lhs.value() = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), lhs.value());
    }
    if (rhs.value()->type->to_string() == "__flint_type_str_lit") {
        rhs.value() = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), rhs.value());
    }

    // Create the binary operator node
    if (Matcher::token_match(pivot_token, Matcher::relational_binop)) {
        return std::make_unique<BinaryOpNode>(pivot_token, lhs.value(), rhs.value(), Type::get_primitive_type("bool"));
    }
    return std::make_unique<BinaryOpNode>(pivot_token, lhs.value(), rhs.value(), lhs.value()->type);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_expression( //
    Scope *scope,                                                         //
    const token_slice &tokens,                                            //
    const std::optional<std::shared_ptr<Type>> &expected_type             //
) {
    token_slice expr_tokens = tokens;
    remove_trailing_garbage(expr_tokens);

    // Parse expression using precedence levels
    auto expression = create_pivot_expression(scope, expr_tokens);

    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    // Check if the types are implicitely type castable, if they are, wrap the expression in a TypeCastNode
    if (expected_type.has_value() && expected_type.value() != expression.value()->type) {
        if (primitive_implicit_casting_table.find(expression.value()->type->to_string()) != primitive_implicit_casting_table.end()) {
            const std::vector<std::string_view> &to_types = primitive_implicit_casting_table.at(expression.value()->type->to_string());
            if (std::find(to_types.begin(), to_types.end(), expected_type.value()->to_string()) != to_types.end()) {
                expression = std::make_unique<TypeCastNode>(expected_type.value(), expression.value());
            } else {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                return std::nullopt;
            }
        } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(expected_type.value().get())) {
            const GroupType *group_type = dynamic_cast<const GroupType *>(expression.value()->type.get());
            if (group_type == nullptr) {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                return std::nullopt;
            }
            if (tuple_type->types != group_type->types) {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                return std::nullopt;
            }
        } else {
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
            return std::nullopt;
        }
    }

    return expression;
}
