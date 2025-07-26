#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/builtins.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "matcher/matcher.hpp"
#include "parser/parser.hpp"

#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <variant>

std::optional<bool> Parser::check_castability(const std::shared_ptr<Type> &lhs_type, const std::shared_ptr<Type> &rhs_type) {
    const GroupType *lhs_group = dynamic_cast<const GroupType *>(lhs_type.get());
    const GroupType *rhs_group = dynamic_cast<const GroupType *>(rhs_type.get());
    if (lhs_group == nullptr && rhs_group == nullptr) {
        // Both single type
        // If one of them is a multi-type, the other one has to be a single value with the same type as the base type of the mutli-type
        const MultiType *lhs_mult = dynamic_cast<const MultiType *>(lhs_type.get());
        const MultiType *rhs_mult = dynamic_cast<const MultiType *>(rhs_type.get());
        if (lhs_mult != nullptr && rhs_mult == nullptr && lhs_mult->base_type == rhs_type) {
            return true;
        } else if (lhs_mult == nullptr && rhs_mult != nullptr && lhs_type == rhs_mult->base_type) {
            return false;
        }
        if (lhs_type->to_string() == "__flint_type_str_lit" && rhs_type->to_string() == "str") {
            return false;
        } else if (lhs_type->to_string() == "str" && rhs_type->to_string() == "__flint_type_str_lit") {
            return true;
        }
        // Check one or both of the sides are optional types
        const OptionalType *lhs_opt = dynamic_cast<const OptionalType *>(lhs_type.get());
        const OptionalType *rhs_opt = dynamic_cast<const OptionalType *>(rhs_type.get());
        if (lhs_opt != nullptr || rhs_opt != nullptr) {
            // One of the sides is an optional, but first check if the lhs or rhs is a literal
            const std::string lhs_str = lhs_type->to_string();
            const std::string rhs_str = rhs_type->to_string();
            if (lhs_str == "void?" || rhs_str == "void?") {
                // If rhs is void? then rhs -> lhs, otherwise lhs -> rhs
                return rhs_str == "void?";
            }
            // None of the sides is a optional literal, so we need to check if one of the sides is the same type as the optional's base type
            if (lhs_opt != nullptr && lhs_opt->base_type == rhs_type) {
                return true; // rhs -> lhs
            } else if (rhs_opt != nullptr && rhs_opt->base_type == lhs_type) {
                return false; // lhs -> rhs
            } else {
                // Completely different optional types
                return std::nullopt;
            }
        }
        if (type_precedence.find(lhs_type->to_string()) == type_precedence.end() ||
            type_precedence.find(rhs_type->to_string()) == type_precedence.end()) {
            // Not castable, wrong arg types
            // TODO: Make the token list ant column and line the actual line and column the type mismatch occurs at
            token_list token_list = {TokenContext(TOK_EOF, 1, 1, "EOF")};
            token_slice tokens = {token_list.begin(), token_list.end()};
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, lhs_type, rhs_type);
            return std::nullopt;
        }
        const unsigned int lhs_precedence = type_precedence.at(lhs_type->to_string());
        const unsigned int rhs_precedence = type_precedence.at(rhs_type->to_string());
        return lhs_precedence > rhs_precedence;
    } else if (lhs_group == nullptr && rhs_group != nullptr) {
        // Left is no group, right is group
        // Check if left is a multi-type, then the right is castable to the left
        const MultiType *lhs_mult = dynamic_cast<const MultiType *>(lhs_type.get());
        if (lhs_mult == nullptr) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // The group must have the same size as the multi-type
        if (lhs_mult->width != rhs_group->types.size()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // All elements in the group must have the same type as the multi-type
        for (size_t i = 0; i < lhs_mult->width; i++) {
            if (lhs_mult->base_type != rhs_group->types[i]) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
        }
        return true;
    } else if (lhs_group != nullptr && rhs_group == nullptr) {
        // Left is group, right is no group
        // Check if right is a multi-type, then the left is castable to the right
        const MultiType *rhs_mult = dynamic_cast<const MultiType *>(rhs_type.get());
        if (rhs_mult == nullptr) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // The group must have the same size as the multi-type
        if (rhs_mult->width != lhs_group->types.size()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // All elements in the group must have the same type as the multi-type
        for (size_t i = 0; i < rhs_mult->width; i++) {
            if (rhs_mult->base_type != lhs_group->types[i]) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
        }
        return false;
    } else {
        // Both group
        // TODO
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return false;
    }
    return true;
}

bool Parser::check_castability(std::unique_ptr<ExpressionNode> &lhs, std::unique_ptr<ExpressionNode> &rhs) {
    if (lhs->type == rhs->type) {
        return true;
    }
    std::optional<bool> castability = check_castability(lhs->type, rhs->type);
    if (!castability.has_value()) {
        // Not castable
        return false;
    }
    if (castability.value()) {
        // The right type needs to be cast to the left type
        rhs = std::make_unique<TypeCastNode>(lhs->type, rhs);
    } else {
        // The left type needs to be cast to the right type
        lhs = std::make_unique<TypeCastNode>(rhs->type, lhs);
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
            if (std::holds_alternative<LitI64>(lhs->value)) {
                const long new_lit = std::get<LitI64>(lhs->value).value + std::get<LitI64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitI64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitF64>(lhs->value)) {
                const double new_lit = std::get<LitF64>(lhs->value).value + std::get<LitF64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitF64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitStr>(lhs->value)) {
                const std::string new_lit = std::get<LitStr>(lhs->value).value + std::get<LitStr>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitStr{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value + std::get<LitU8>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitU8{.value = new_lit}, lhs->type, true);
            }
            break;
        case TOK_MINUS:
            if (std::holds_alternative<LitI64>(lhs->value)) {
                const long new_lit = std::get<LitI64>(lhs->value).value - std::get<LitI64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitI64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitF64>(lhs->value)) {
                const double new_lit = std::get<LitF64>(lhs->value).value - std::get<LitF64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitF64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value - std::get<LitU8>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitU8{.value = new_lit}, lhs->type, true);
            }
            break;
        case TOK_MULT:
            if (std::holds_alternative<LitI64>(lhs->value)) {
                const long new_lit = std::get<LitI64>(lhs->value).value * std::get<LitI64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitI64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitF64>(lhs->value)) {
                const double new_lit = std::get<LitF64>(lhs->value).value * std::get<LitF64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitF64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value * std::get<LitU8>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitU8{.value = new_lit}, lhs->type, true);
            }
            break;
        case TOK_DIV:
            if (std::holds_alternative<LitI64>(lhs->value)) {
                const long new_lit = std::get<LitI64>(lhs->value).value / std::get<LitI64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitI64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitF64>(lhs->value)) {
                const double new_lit = std::get<LitF64>(lhs->value).value / std::get<LitF64>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitF64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = std::get<LitU8>(lhs->value).value / std::get<LitU8>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitU8{.value = new_lit}, lhs->type, true);
            }
            break;
        case TOK_POW:
            if (std::holds_alternative<LitI64>(lhs->value)) {
                const long new_lit = static_cast<long>(std::pow(std::get<LitI64>(lhs->value).value, std::get<LitI64>(rhs->value).value));
                return std::make_unique<LiteralNode>(LitI64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitF64>(lhs->value)) {
                const double new_lit = std::pow(std::get<LitF64>(lhs->value).value, std::get<LitF64>(rhs->value).value);
                return std::make_unique<LiteralNode>(LitF64{.value = new_lit}, lhs->type, true);
            } else if (std::holds_alternative<LitU8>(lhs->value)) {
                const char new_lit = static_cast<char>(std::pow(std::get<LitU8>(lhs->value).value, std::get<LitU8>(rhs->value).value));
                return std::make_unique<LiteralNode>(LitU8{.value = new_lit}, lhs->type, true);
            }
            break;
        case TOK_AND:
            if (std::holds_alternative<LitBool>(lhs->value)) {
                const bool new_lit = std::get<LitBool>(lhs->value).value && std::get<LitBool>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitBool{.value = new_lit}, lhs->type, true);
            }
            break;
        case TOK_OR:
            if (std::holds_alternative<LitBool>(lhs->value)) {
                const bool new_lit = std::get<LitBool>(lhs->value).value || std::get<LitBool>(rhs->value).value;
                return std::make_unique<LiteralNode>(LitBool{.value = new_lit}, lhs->type, true);
            }
            break;
    }
    return std::nullopt;
}

std::optional<VariableNode> Parser::create_variable(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    std::optional<VariableNode> var = std::nullopt;
    for (auto tok = tokens.first; tok != tokens.second; tok++) {
        if (tok->token == TOK_IDENTIFIER) {
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

std::optional<UnaryOpExpression> Parser::create_unary_op_expression(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    auto unary_op_values = create_unary_op_base(scope, tokens_mut);
    if (!unary_op_values.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    Token &token = std::get<0>(unary_op_values.value());
    std::unique_ptr<ExpressionNode> &expression = std::get<1>(unary_op_values.value());
    bool is_left = std::get<2>(unary_op_values.value());
    UnaryOpExpression un_op(token, expression, is_left);
    if (token == TOK_EXCLAMATION) {
        if (is_left) {
            // The ! operator is only allowed on the right of the expression
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const OptionalType *optional_type = dynamic_cast<const OptionalType *>(un_op.type.get());
        if (optional_type == nullptr) {
            // The post ! operator is only allowed on optional values
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const VariableNode *var_node = dynamic_cast<const VariableNode *>(un_op.operand.get());
        if (var_node == nullptr) {
            // Optional unwrapping is only allowed on variables for now
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return std::nullopt;
        }
        // Set the type of the unary op to the base type of the optional, as the unwrap will return the base type of it
        un_op.type = optional_type->base_type;
        return un_op;
    }
    return un_op;
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
        if (tokens.first->token == TOK_MINUS) {
            front_token = TOK_MINUS;
            tok = (tokens.first + 1);
        } else if (tokens.first->token == TOK_DOLLAR) {
            front_token = TOK_DOLLAR;
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
        }
    } else {
        tok = tokens.first;
    }

    if (Matcher::tokens_match({tok, tok + 1}, Matcher::literal)) {
        std::variant<unsigned long, long, unsigned int, int, double, float, std::string, bool, char, std::optional<void *>> value;
        switch (tok->token) {
            default:
                THROW_ERR(ErrValUnknownLiteral, ERR_PARSING, file_name, tok->line, tok->column, tok->lexme);
                return std::nullopt;
                break;
            case TOK_NONE: {
                std::shared_ptr<Type> void_type = Type::get_primitive_type("void");
                std::optional<std::shared_ptr<Type>> opt_type = Type::get_type_from_str("void?");
                assert(opt_type.has_value());
                return LiteralNode(LitOptional{}, opt_type.value());
            }
            case TOK_INT_VALUE: {
                if (front_token == TOK_MINUS) {
                    const long long lit_value = std::stoll(tok->lexme) * -1;
                    if (lit_value > static_cast<long long>(INT32_MAX) || lit_value < static_cast<long long>(INT32_MIN)) {
                        return LiteralNode(LitI64{.value = static_cast<long>(lit_value)}, Type::get_primitive_type("i64"));
                    } else {
                        return LiteralNode(LitI32{.value = static_cast<int>(lit_value)}, Type::get_primitive_type("i32"));
                    }
                } else {
                    const unsigned long long lit_value = std::stoll(tok->lexme);
                    if (lit_value > static_cast<unsigned long long>(UINT64_MAX)) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    } else if (lit_value > static_cast<unsigned long long>(INT64_MAX)) {
                        return LiteralNode(LitU64{.value = static_cast<unsigned long>(lit_value)}, Type::get_primitive_type("u64"));
                    } else if (lit_value > static_cast<unsigned long long>(UINT32_MAX)) {
                        return LiteralNode(LitI64{.value = static_cast<long>(lit_value)}, Type::get_primitive_type("i64"));
                    } else if (lit_value > static_cast<unsigned long long>(INT32_MAX)) {
                        return LiteralNode(LitU32{.value = static_cast<unsigned int>(lit_value)}, Type::get_primitive_type("u32"));
                    } else {
                        return LiteralNode(LitI32{.value = static_cast<int>(lit_value)}, Type::get_primitive_type("i32"));
                    }
                }
            }
            case TOK_FLINT_VALUE: {
                if (front_token == TOK_MINUS) {
                    return LiteralNode(LitF32{.value = std::stof(tok->lexme) * -1}, Type::get_primitive_type("f32"));
                } else {
                    return LiteralNode(LitF32{.value = std::stof(tok->lexme)}, Type::get_primitive_type("f32"));
                }
            }
            case TOK_STR_VALUE: {
                if (front_token == TOK_DOLLAR) {
                    return LiteralNode(LitStr{.value = tok->lexme}, Type::get_primitive_type("str"));
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
                    return LiteralNode(LitStr{.value = processed_str.str()}, Type::get_primitive_type("__flint_type_str_lit"));
                }
            }
            case TOK_TRUE: {
                value = true;
                return LiteralNode(LitBool{.value = true}, Type::get_primitive_type("bool"));
            }
            case TOK_FALSE: {
                value = false;
                return LiteralNode(LitBool{.value = false}, Type::get_primitive_type("bool"));
            }
            case TOK_CHAR_VALUE: {
                return LiteralNode(LitU8{.value = tok->lexme[0]}, Type::get_primitive_type("u8"));
            }
        }
    }
    return std::nullopt;
}

std::optional<StringInterpolationNode> Parser::create_string_interpolation(std::shared_ptr<Scope> scope,
    const std::string &interpol_string) {
    // First, get all balanced ranges of { } symbols which are not leaded by a \\ symbol
    std::vector<uint2> ranges = Matcher::balanced_ranges_vec(interpol_string, "([^\\\\]|^)\\{", "[^\\\\]\\}");
    std::vector<std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<LiteralNode>>> interpol_content;
    // If the ranges are empty, the interpolation does not contain any groups
    if (ranges.empty()) {
        interpol_content.emplace_back(std::make_unique<LiteralNode>(LitStr{.value = interpol_string}, Type::get_primitive_type("str")));
        return StringInterpolationNode(interpol_content);
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
            token_list lit_toks = {{TOK_STR_VALUE, 0, 0, interpol_string.substr(0, it->first)}};
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        } else if (it != ranges.begin() && it->first - std::prev(it)->second > 1) {
            // Add string in between } and { symbols
            size_t start_pos = std::prev(it)->second + 1; // Position after previous }
            size_t length = it->first - start_pos;        // Length until current {
            token_list lit_toks = {{TOK_STR_VALUE, 0, 0, interpol_string.substr(start_pos, length)}};
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        }

        // Extract the expression between { and }
        size_t expr_start = it->first + 1;               // Position after {
        size_t expr_length = it->second - it->first - 1; // Length between { and }
        Lexer lexer("string_interpolation", interpol_string.substr(expr_start, expr_length));
        token_list expr_tokens = lexer.scan();
        if (expr_tokens.empty()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        token_slice expr_tokens_slice = {expr_tokens.begin(), expr_tokens.end()};
        if (expr_tokens.back().token == TOK_EOF) {
            expr_tokens_slice.second--;
        }
        std::optional<std::unique_ptr<ExpressionNode>> expr = create_expression(scope, expr_tokens_slice);
        if (!expr.has_value()) {
            THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens_slice);
            return std::nullopt;
        }
        // Cast every expression inside to a str type (if it isn't already)
        if (expr.value()->type->to_string() == "str") {
            interpol_content.emplace_back(std::move(expr.value()));
        } else {
            interpol_content.emplace_back(std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), expr.value()));
        }

        // Add string after last } symbol
        if (std::next(it) == ranges.end() && it->second + 1 < interpol_string.length()) {
            size_t start_pos = it->second + 1; // Position after }
            token_list lit_toks = {{TOK_STR_VALUE, 0, 0, interpol_string.substr(start_pos)}};
            std::optional<LiteralNode> lit = create_literal({lit_toks.begin(), lit_toks.end()});
            interpol_content.emplace_back(std::make_unique<LiteralNode>(std::move(lit.value())));
        }
    }
    return StringInterpolationNode(interpol_content);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_call_expression( //
    std::shared_ptr<Scope> scope,                                              //
    const token_slice &tokens,                                                 //
    const std::optional<std::string> &alias_base                               //
) {
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    auto call_or_init_node_args = create_call_or_initializer_base(scope, tokens_mut, alias_base);
    if (!call_or_init_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }
    assert(!std::get<3>(call_or_init_node_args.value()));
    std::unique_ptr<CallNodeExpression> call_node = std::make_unique<CallNodeExpression>( //
        std::get<0>(call_or_init_node_args.value()),                                      // name
        std::get<1>(call_or_init_node_args.value()),                                      // args
        std::get<4>(call_or_init_node_args.value()),                                      // can_throw
        std::get<2>(call_or_init_node_args.value())                                       // type
    );
    call_node->scope_id = scope->scope_id;
    last_parsed_call = call_node.get();
    return call_node;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_initializer( //
    std::shared_ptr<Scope> scope,                                          //
    const token_slice &tokens,                                             //
    const std::optional<std::string> &alias_base                           //
) {
    token_slice tokens_mut = tokens;
    remove_surrounding_paren(tokens_mut);
    auto call_or_init_node_args = create_call_or_initializer_base(scope, tokens_mut, alias_base);
    if (!call_or_init_node_args.has_value()) {
        THROW_ERR(ErrExprCallCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }
    assert(std::get<3>(call_or_init_node_args.value()));
    std::vector<std::unique_ptr<ExpressionNode>> args;
    for (auto &arg : std::get<1>(call_or_init_node_args.value())) {
        args.emplace_back(std::move(arg.first));
    }
    std::unique_ptr<InitializerNode> initializer_node = std::make_unique<InitializerNode>( //
        std::get<2>(call_or_init_node_args.value()),                                       // type
        args                                                                               // args
    );
    return initializer_node;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_type_cast(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    assert(tokens.first->token == TOK_TYPE);
    assert(std::next(tokens.first)->token == TOK_LEFT_PAREN);
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
    assert(expr_range.value().second > expr_range.value().first);

    // Get the type the expression needs to be converted to
    std::shared_ptr<Type> to_type = tokens.first->type;
    std::string to_type_string = to_type->to_string();

    // Create the expression
    token_slice expr_tokens = {tokens_mut.first + expr_range.value().first, tokens_mut.first + expr_range.value().second};
    std::optional<std::unique_ptr<ExpressionNode>> expression = create_expression(scope, expr_tokens);
    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expr_tokens);
        return std::nullopt;
    }

    // Check if the expression already is the desired type, in that case just return the expression directly
    if (expression.value()->type == to_type) {
        return expression;
    }

    // Check if the type of the expression is castable at all
    if (primitive_casting_table.find(expression.value()->type->to_string()) == primitive_casting_table.end()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const std::vector<std::string_view> &to_types = primitive_casting_table.at(expression.value()->type->to_string());
    if (std::find(to_types.begin(), to_types.end(), to_type_string) == to_types.end()) {
        // The given expression type cannot be cast to the wanted type
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return std::make_unique<TypeCastNode>(to_type, expression.value());
}

std::optional<GroupExpressionNode> Parser::create_group_expression(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    // First, remove all trailing garbage from the expression tokens
    remove_trailing_garbage(tokens_mut);
    // Now, the first and the last token must be open and closing parenthesis respectively
    assert(tokens_mut.first->token == TOK_LEFT_PAREN);
    assert(std::prev(tokens_mut.second)->token == TOK_RIGHT_PAREN);
    // Remove the open and closing parenthesis
    tokens_mut.first++;
    tokens_mut.second--;

    // Get all balanced match ranges of commas in the group expression
    std::vector<uint2> match_ranges = Matcher::get_match_ranges(tokens_mut, Matcher::until_comma);
    // Its not a group expression, there is only one expression inside the parenthesis, this should never happen
    assert(!match_ranges.empty());
    // Remove all duplicates, because when the fourth token is a comma we get the ranges 0-3, 1-3 and 2-3, and we only care about the first
    // one, not all later ones
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
    assert(tokens_mut.first + match_ranges.back().second < tokens_mut.second);
    match_ranges.emplace_back(match_ranges.back().second, std::distance(tokens_mut.first, tokens_mut.second));

    // Decrement all second matches ranges to exclude all commas from the expression (except for the last match range, it has no comma at
    // its last position
    for (auto it = match_ranges.begin(); it != match_ranges.end() - 1; ++it) {
        it->second--;
    }

    // Parse all expressions in the group
    std::vector<std::unique_ptr<ExpressionNode>> expressions;
    for (const uint2 &match_range : match_ranges) {
        token_slice expression_tokens = {tokens_mut.first + match_range.first, tokens_mut.first + match_range.second};
        auto expr = create_expression(scope, expression_tokens);
        if (!expr.has_value()) {
            THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, expression_tokens);
            return std::nullopt;
        }
        expressions.emplace_back(std::move(expr.value()));
    }

    // Check if the types in the group are correct
    for (auto it = expressions.begin(); it != expressions.end(); ++it) {
        if (dynamic_cast<const GroupType *>((*it)->type.get())) {
            // Nested groups are not allowed
            THROW_ERR(ErrExprNestedGroup, ERR_PARSING, file_name, tokens);
            return std::nullopt;
        } else if ((*it)->type->to_string() == "__flint_type_str_lit") {
            *it = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), *it);
        }
    }
    return GroupExpressionNode(expressions);
}

std::optional<std::vector<std::unique_ptr<ExpressionNode>>> Parser::create_group_expressions( //
    std::shared_ptr<Scope> scope,                                                             //
    token_slice &tokens                                                                       //
) {
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

std::optional<DataAccessNode> Parser::create_data_access(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    auto field_access_base = create_field_access_base(scope, tokens_mut);
    if (!field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return DataAccessNode(                      //
        std::get<0>(field_access_base.value()), // base_expr
        std::get<1>(field_access_base.value()), // field_name
        std::get<2>(field_access_base.value()), // field_id
        std::get<3>(field_access_base.value())  // field_type
    );
}

std::optional<ArrayInitializerNode> Parser::create_array_initializer(std::shared_ptr<Scope> scope, const token_slice &tokens) {
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
    if (std::next(initializer_tokens.first) == initializer_tokens.second && initializer_tokens.first->token == TOK_UNDERSCORE) {
        initializer = std::make_unique<DefaultNode>(element_type.value());
    } else {
        initializer = create_expression(scope, initializer_tokens);
    }
    if (!initializer.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, initializer_tokens);
        return std::nullopt;
    }

    // The first token in the tokens list should be a left bracket
    assert(tokens_mut.first->token == TOK_LEFT_BRACKET);
    tokens_mut.first++;
    // The last token in the tokens list should be a right bracket
    assert(std::prev(tokens_mut.second)->token == TOK_RIGHT_BRACKET);
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

std::optional<OptionalChainNode> Parser::create_optional_chain(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    // First, we need to find the `?` token, everything left to that token is our base expression
    auto iterator = tokens.second - 1;
    while (iterator != tokens.first) {
        if (iterator->token == TOK_QUESTION) {
            break;
        }
        --iterator;
    }
    // If the iterator is the beginning this means that no `?` token is present in the list of tokens, this means something in the matcher
    // went wrong, not here in the parser
    assert(iterator != tokens.first);
    // Everything to the left of the iterator is the base expression and can be parsed as such
    const token_slice base_expr_tokens = {tokens.first, iterator};

    // Move past the `?` token
    iterator++;
    ChainOperation operation;
    std::shared_ptr<Type> result_type;
    // Now we need to check what the rhs of the optional chain is
    // TODO: Change the 'is_toplevel_chain_node' to something else, to detect whether it actually *is* the top level
    if (iterator->token == TOK_LEFT_BRACKET) {
        // It's an array access. First we need to make sure that the base expression is an array or string type
        auto base_expr = create_expression(scope, base_expr_tokens);
        if (!base_expr.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const OptionalType *optional_type = dynamic_cast<const OptionalType *>(base_expr.value()->type.get());
        if (optional_type == nullptr) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        unsigned int dimensionality = 1;
        if (const ArrayType *base_array_type = dynamic_cast<const ArrayType *>(optional_type->base_type.get())) {
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
        std::optional<std::vector<std::unique_ptr<ExpressionNode>>> indexing_expressions = create_group_expressions(scope, indexing_tokens);
        if (!indexing_expressions.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (indexing_expressions.value().size() != dimensionality) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        operation = ChainArrayAccess{std::move(indexing_expressions.value())};
        return OptionalChainNode(base_expr.value(), true, operation, result_type);
    } else if (iterator->token == TOK_DOT) {
        // It's a field access
        auto field_access_base = create_field_access_base(scope, tokens, true);
        if (!field_access_base.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        auto &field_name = std::get<1>(field_access_base.value());
        auto &field_id = std::get<2>(field_access_base.value());
        operation = ChainFieldAccess{field_name, field_id};
        result_type = std::get<3>(field_access_base.value());
        auto &base_expr = std::get<0>(field_access_base.value());
        return OptionalChainNode(base_expr, true, operation, result_type);
    }
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_optional_unwrap(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    // We first need to get the last exclamation operator as our separator for the base expression
    auto iterator = tokens.second - 1;
    while (iterator != tokens.first) {
        if (iterator->token == TOK_EXCLAMATION) {
            break;
        }
        --iterator;
    }
    assert(iterator != tokens.first);
    assert(iterator->token == TOK_EXCLAMATION);
    const token_slice base_expr_tokens = {tokens.first, iterator};
    // If nothing follows after the optional unwrap node we can return it directly
    if (iterator == tokens.second - 1) {
        auto base_expr = create_expression(scope, base_expr_tokens);
        if (!base_expr.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (!dynamic_cast<const OptionalType *>(base_expr.value()->type.get())) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<OptionalUnwrapNode>(base_expr.value());
    }
    // Skip the `!`
    ++iterator;

    if (iterator->token == TOK_LEFT_BRACKET) {
        // It's an array access. First we need to make sure that the base expression is an array or string type
        auto base_expr = create_expression(scope, base_expr_tokens);
        if (!base_expr.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const OptionalType *optional_type = dynamic_cast<const OptionalType *>(base_expr.value()->type.get());
        if (optional_type == nullptr) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        unsigned int dimensionality = 1;
        std::shared_ptr<Type> result_type;
        if (const ArrayType *base_array_type = dynamic_cast<const ArrayType *>(optional_type->base_type.get())) {
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
        std::optional<std::vector<std::unique_ptr<ExpressionNode>>> indexing_expressions = create_group_expressions(scope, indexing_tokens);
        if (!indexing_expressions.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (indexing_expressions.value().size() != dimensionality) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        std::unique_ptr<ExpressionNode> opt_unwrap = std::make_unique<OptionalUnwrapNode>(base_expr.value());
        return std::make_unique<ArrayAccessNode>(opt_unwrap, result_type, indexing_expressions.value());
    } else if (iterator->token == TOK_DOT && (iterator + 1)->token == TOK_LEFT_PAREN) {
        // It's a grouped field access
        auto grouped_access_base = create_grouped_access_base(scope, tokens, true);
        if (!grouped_access_base.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        auto &base_expr = std::get<0>(grouped_access_base.value());
        std::unique_ptr<ExpressionNode> opt_unwrap = std::make_unique<OptionalUnwrapNode>(base_expr);
        auto &field_names = std::get<1>(grouped_access_base.value());
        auto &field_ids = std::get<2>(grouped_access_base.value());
        auto &field_types = std::get<3>(grouped_access_base.value());
        return std::make_unique<GroupedDataAccessNode>(opt_unwrap, field_names, field_ids, field_types);
    } else if (iterator->token == TOK_DOT) {
        // It's a field access
        auto field_access_base = create_field_access_base(scope, tokens, true);
        if (!field_access_base.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        auto &base_expr = std::get<0>(field_access_base.value());
        std::unique_ptr<ExpressionNode> opt_unwrap = std::make_unique<OptionalUnwrapNode>(base_expr);
        auto &field_name = std::get<1>(field_access_base.value());
        auto &field_id = std::get<2>(field_access_base.value());
        auto &field_type = std::get<3>(field_access_base.value());
        return std::make_unique<DataAccessNode>(opt_unwrap, field_name, field_id, field_type);
    }
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

std::optional<ArrayAccessNode> Parser::create_array_access(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    // The array access must end with a closing bracket token. Then, everything from that closing bracket to the left until an opening
    // bracket is considered the indexing expressions. Everything that comes before that initial opening bracket is considered the base
    // expression.
    token_list toks = clone_from_slice(tokens);
    assert(std::prev(tokens.second)->token == TOK_RIGHT_BRACKET);
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
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const ArrayType *array_type = dynamic_cast<const ArrayType *>(base_expr.value()->type.get());
    const bool is_str_type = base_expr.value()->type->to_string() == "str";
    if (array_type == nullptr && !is_str_type) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Now we can parse the indexing expression(s)
    std::optional<std::vector<std::unique_ptr<ExpressionNode>>> indexing_expressions = create_group_expressions(scope, indexing_tokens);
    if (!indexing_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (is_str_type) {
        if (indexing_expressions.value().size() > 1) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return ArrayAccessNode(base_expr.value(), Type::get_primitive_type("u8"), indexing_expressions.value());
    }
    // The indexing expression size must match the array dimensionality
    if (indexing_expressions.value().size() != array_type->dimensionality) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    return ArrayAccessNode(base_expr.value(), array_type->type, indexing_expressions.value());
}

std::optional<GroupedDataAccessNode> Parser::create_grouped_data_access(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    auto grouped_field_access_base = create_grouped_access_base(scope, tokens_mut);
    if (!grouped_field_access_base.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    return GroupedDataAccessNode(                       //
        std::get<0>(grouped_field_access_base.value()), // base_expr
        std::get<1>(grouped_field_access_base.value()), // field_names
        std::get<2>(grouped_field_access_base.value()), // field_ids
        std::get<3>(grouped_field_access_base.value())  // field_types
    );
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_stacked_expression(std::shared_ptr<Scope> scope, const token_slice &tokens) {
    // Stacked expressions *end* with one of these patterns, if we match one of these patterns we can parse them
    if (Matcher::tokens_end_with(tokens, Matcher::data_access)) {
        std::optional<DataAccessNode> data_access = create_data_access(scope, tokens);
        if (!data_access.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<DataAccessNode>(std::move(data_access.value()));
    } else if (Matcher::tokens_end_with(tokens, Matcher::grouped_data_access)) {
        std::optional<GroupedDataAccessNode> group_access = create_grouped_data_access(scope, tokens);
        if (!group_access.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<GroupedDataAccessNode>(std::move(group_access.value()));
    } else if (Matcher::tokens_end_with(tokens, Matcher::array_access)) {
        std::optional<ArrayAccessNode> access = create_array_access(scope, tokens);
        if (!access.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_unique<ArrayAccessNode>(std::move(access.value()));
    } else {
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_pivot_expression( //
    std::shared_ptr<Scope> scope,                                               //
    const token_slice &tokens,                                                  //
    const std::optional<std::shared_ptr<Type>> &expected_type                   //
) {
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
        } else if (tokens_mut.first->token == TOK_UNDERSCORE) {
            if (!expected_type.has_value()) {
                // Default node at a place where it's type cannot be inferred
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<DefaultNode>(expected_type.value());
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
            assert(tokens_mut.first->token == TOK_DOLLAR && std::prev(tokens_mut.second)->token == TOK_STR_VALUE);
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
            assert(tokens_mut.first->token == TOK_IDENTIFIER);
            const std::string alias_base = tokens_mut.first->lexme;
            tokens_mut.first++;
            // Then a dot should follow
            assert(tokens_mut.first->token == TOK_DOT);
            tokens_mut.first++;
            auto call_node = create_call_expression(scope, tokens_mut, alias_base);
            if (!call_node.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::move(call_node.value());
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::function_call)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().second == token_size) {
            // Its only a call, when the paren group of the function is at the very end of the tokens, otherwise there is something
            // located on the right of the call still
            auto call_node = create_call_expression(scope, tokens_mut, std::nullopt);
            if (!call_node.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return call_node;
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::aliased_initializer)) {
        auto range = Matcher::balanced_range_extraction(tokens_mut, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN));
        if (range.has_value() && range.value().second == token_size) {
            assert(tokens_mut.first->token == TOK_IDENTIFIER);
            const std::string alias_base = tokens_mut.first->lexme;
            tokens_mut.first++;
            // Then a dot should follow
            assert(tokens_mut.first->token == TOK_DOT);
            tokens_mut.first++;
            auto initializer_node = create_initializer(scope, tokens_mut, alias_base);
            if (!initializer_node.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return initializer_node;
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
        if (primitives.find(tokens_mut.first->type->to_string()) == primitives.end()) {
            // It's an initializer
            std::optional<std::unique_ptr<ExpressionNode>> initializer = create_initializer(scope, tokens_mut, std::nullopt);
            if (!initializer.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return initializer;
        } else {
            // It's a regular type-cast (only primitive types can be cast and primitive types have no initializer
            std::optional<std::unique_ptr<ExpressionNode>> type_cast = create_type_cast(scope, tokens_mut);
            if (!type_cast.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return type_cast;
        }
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
    if (Matcher::tokens_match(tokens_mut, Matcher::type_field_access)) {
        if (token_size == 3 || (token_size == 4 && std::prev(tokens_mut.second)->token == TOK_INT_VALUE)) {
            assert(tokens_mut.first->token == TOK_TYPE);
            const std::shared_ptr<Type> type = tokens_mut.first->type;
            const EnumType *enum_type = dynamic_cast<const EnumType *>(type.get());
            if (enum_type != nullptr) {
                assert((tokens_mut.first + 1)->token == TOK_DOT);
                assert((tokens_mut.first + 2)->token == TOK_IDENTIFIER);
                const std::string &value = (tokens_mut.first + 2)->lexme;
                const auto &values = enum_type->enum_node->values;
                if (std::find(values.begin(), values.end(), value) == values.end()) {
                    // Unsupported enum value
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                return std::make_unique<LiteralNode>(LitEnum{.enum_type = type, .value = value}, type);
            }
        }
    }
    if (Matcher::tokens_match(tokens_mut, Matcher::data_access)) {
        if (token_size == 3 || (token_size == 4 && std::prev(tokens_mut.second)->token == TOK_INT_VALUE)) {
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
    if (Matcher::tokens_contain(tokens_mut, Matcher::token(TOK_QUESTION))) {
        if (!Matcher::tokens_contain(tokens_mut, Matcher::unary_operator) &&
            !Matcher::tokens_contain(tokens_mut, Matcher::binary_operator)) {
            std::optional<OptionalChainNode> chain = create_optional_chain(scope, tokens);
            if (!chain.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<OptionalChainNode>(std::move(chain.value()));
        }
    }
    if (Matcher::tokens_contain(tokens_mut, Matcher::token(TOK_EXCLAMATION))) {
        if (!Matcher::tokens_contain(tokens_mut, Matcher::unary_operator) &&
            !Matcher::tokens_contain(tokens_mut, Matcher::binary_operator)) {
            std::optional<std::unique_ptr<ExpressionNode>> unwrap = create_optional_unwrap(scope, tokens);
            if (!unwrap.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::move(unwrap.value());
        }
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
        if (std::prev(it)->token == TOK_LEFT_PAREN) {
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
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    // Extract the left and right parts of the expression
    token_slice lhs_tokens = {tokens_mut.first, tokens_mut.first + pivot_pos};
    token_slice rhs_tokens = {tokens_mut.first + pivot_pos + 1, tokens_mut.second};

    // Recursively parse both sides
    auto lhs = create_pivot_expression(scope, lhs_tokens, expected_type);
    if (!lhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, lhs_tokens);
        return std::nullopt;
    }

    auto rhs = create_pivot_expression(scope, rhs_tokens, expected_type);
    if (!rhs.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, rhs_tokens);
        return std::nullopt;
    }

    // Check if all parameter types actually match the argument types
    // If we came until here, the arg count definitely matches the parameter count
    if (lhs.value()->type != rhs.value()->type) {
        // Check if the operator is a optional default, in this case we need to check whether the lhs is an optional and whether the rhs
        // is the base type of the optional, otherwise it is considered an error
        if (pivot_token == TOK_OPT_DEFAULT) {
            const OptionalType *lhs_opt = dynamic_cast<const OptionalType *>(lhs.value()->type.get());
            if (lhs_opt == nullptr) {
                // ?? operator not possible on non-optional type
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            if (rhs.value()->type != lhs_opt->base_type) {
                // The rhs of the ?? operator must be the base type of the lhs optional
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_unique<BinaryOpNode>(pivot_token, lhs.value(), rhs.value(), lhs_opt->base_type);
        } else if (!check_castability(lhs.value(), rhs.value())) {
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
        lhs = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), lhs.value());
    }
    if (rhs.value()->type->to_string() == "__flint_type_str_lit") {
        rhs = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), rhs.value());
    }

    // Create the binary operator node
    if (Matcher::token_match(pivot_token, Matcher::relational_binop)) {
        return std::make_unique<BinaryOpNode>(pivot_token, lhs.value(), rhs.value(), Type::get_primitive_type("bool"));
    }
    return std::make_unique<BinaryOpNode>(pivot_token, lhs.value(), rhs.value(), lhs.value()->type);
}

std::optional<std::unique_ptr<ExpressionNode>> Parser::create_expression( //
    std::shared_ptr<Scope> scope,                                         //
    const token_slice &tokens,                                            //
    const std::optional<std::shared_ptr<Type>> &expected_type             //
) {
    token_slice expr_tokens = tokens;
    remove_trailing_garbage(expr_tokens);

    // Parse expression using precedence levels
    auto expression = create_pivot_expression(scope, expr_tokens, expected_type);

    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens);
        return std::nullopt;
    }

    // Check if the types are implicitely type castable, if they are, wrap the expression in a TypeCastNode
    if (expected_type.has_value() && expected_type.value() != expression.value()->type) {
        if (const OptionalType *optional_type = dynamic_cast<const OptionalType *>(expected_type.value().get())) {
            if (expression.value()->type == optional_type->base_type) {
                expression = std::make_unique<TypeCastNode>(expected_type.value(), expression.value());
            } else if (const OptionalType *expression_type = dynamic_cast<const OptionalType *>(expression.value()->type.get())) {
                if (expression_type->base_type != Type::get_primitive_type("void")) {
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                    return std::nullopt;
                }
                expression = std::make_unique<TypeCastNode>(expected_type.value(), expression.value());
            } else {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                return std::nullopt;
            }
        } else if (const VariantType *variant_type = dynamic_cast<const VariantType *>(expected_type.value().get())) {
            bool viable_type_found = false;
            for (const auto &[_, variation] : variant_type->variant_node->possible_types) {
                const std::string var_str = variation->to_string();
                const std::string expr_str = expression.value()->type->to_string();
                if (variation == expression.value()->type) {
                    expression = std::make_unique<TypeCastNode>(expected_type.value(), expression.value());
                    viable_type_found = true;
                    break;
                }
            }
            if (!viable_type_found) {
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_name, tokens, expected_type.value(), expression.value()->type);
                return std::nullopt;
            }
        } else if (primitive_implicit_casting_table.find(expression.value()->type->to_string()) != primitive_implicit_casting_table.end()) {
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
