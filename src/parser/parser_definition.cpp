#include "error/error_types/parsing/definitions/function/err_fn_main_too_many_args.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/parser.hpp"

#include "error/error.hpp"
#include "parser/type/tuple_type.hpp"

#include "fip.hpp"

#include <algorithm>
#include <optional>

std::optional<FunctionNode> Parser::create_function(const token_slice &definition) {
    std::string name;
    std::vector<std::tuple<std::shared_ptr<Type>, std::string, bool>> parameters;
    std::vector<std::shared_ptr<Type>> return_types;
    bool is_aligned = false;
    bool is_const = false;

    auto tok_it = definition.first;
    // Parse everything before the parameters
    while (tok_it != definition.second && std::next(tok_it) != definition.second && tok_it->token != TOK_LEFT_PAREN) {
        if (tok_it->token == TOK_ALIGNED) {
            is_aligned = true;
        }
        if (tok_it->token == TOK_CONST) {
            is_const = true;
        }
        if (tok_it->token == TOK_DEF) {
            name = std::next(tok_it)->lexme;
        }
        tok_it++;
    }
    assert(tok_it != definition.second);
    // Check if the name is reserved
    if (name == "_main") {
        token_slice err_tokens = {std::prev(tok_it), definition.second};
        THROW_ERR(ErrFnReservedName, ERR_PARSING, file_name, err_tokens, name);
        return std::nullopt;
    } else if (name == "main" && main_function_parsed) {
        // Redefinition of the main function
        token_slice err_tokens = {std::prev(tok_it), definition.second};
        THROW_ERR(ErrFnMainRedefinition, ERR_PARSING, file_name, err_tokens);
        return std::nullopt;
    }
    name = "fc_" + name;
    // Skip the left paren
    tok_it++;
    const auto arg_start_it = tok_it;
    // Parse the parameters only if there are any parameters
    if (tok_it->token != TOK_RIGHT_PAREN) {
        // Set the last_param_begin to + 1 to skip the left paren
        token_list::iterator last_param_begin = tok_it;
        unsigned int depth = 0;
        while (tok_it != definition.second && std::next(tok_it) != definition.second && tok_it->token != TOK_RIGHT_PAREN) {
            if (tok_it->token == TOK_LESS || tok_it->token == TOK_LEFT_BRACKET) {
                depth++;
                tok_it++;
                continue;
            } else if (tok_it->token == TOK_GREATER || tok_it->token == TOK_RIGHT_BRACKET) {
                depth--;
                tok_it++;
            }
            if (depth == 0 && (std::next(tok_it)->token == TOK_COMMA || std::next(tok_it)->token == TOK_RIGHT_PAREN)) {
                // The current token is the parameter type
                const std::string param_name(tok_it->lexme);
                // The type is everything from the last param begin
                token_slice type_tokens = {last_param_begin, tok_it};
                bool is_mutable = false;
                if (type_tokens.first->token == TOK_CONST) {
                    type_tokens.first++;
                } else if (type_tokens.first->token == TOK_MUT) {
                    is_mutable = true;
                    type_tokens.first++;
                }
                const auto param_type = Type::get_type(type_tokens);
                if (!param_type.has_value()) {
                    return std::nullopt;
                }
                parameters.emplace_back(param_type.value(), param_name, is_mutable);
                last_param_begin = tok_it + 2;
            }
            tok_it++;
        }
        assert(tok_it != definition.second);
    }
    const auto arg_end_it = tok_it;
    // Skip the right paren
    tok_it++;

    // Now the token should be an arrow, if not there are no return values
    auto ret_start_it = tok_it;
    if (tok_it->token == TOK_ARROW) {
        tok_it++;
        ret_start_it++;
        assert(tok_it != definition.second);
        if (tok_it->token != TOK_LEFT_PAREN) {
            // There is only a single return type, so everything until the colon is considere the return type
            std::optional<uint2> type_range = Matcher::get_next_match_range({tok_it, definition.second}, Matcher::type);
            assert(type_range.has_value());
            assert(type_range.value().first == 0);
            token_slice type_tokens = {tok_it, tok_it + type_range.value().second};
            const auto return_type = Type::get_type(type_tokens);
            if (!return_type.has_value()) {
                return std::nullopt;
            }
            if (dynamic_cast<const TupleType *>(return_type.value().get())) {
                THROW_ERR(ErrFnCannotReturnTuple, ERR_PARSING, file_name, type_tokens, return_type.value());
                return std::nullopt;
            }
            return_types.emplace_back(return_type.value());
            tok_it = type_tokens.second;
        } else {
            // Skip the left paren
            tok_it++;
            // Parse the return types
            token_list::iterator last_type_begin = tok_it;
            while (tok_it != definition.second && std::next(tok_it) != definition.second && tok_it->token != TOK_RIGHT_PAREN) {
                if (std::next(tok_it)->token == TOK_COMMA || std::next(tok_it)->token == TOK_RIGHT_PAREN) {
                    // The type is everything from the last param begin
                    token_slice type_tokens = {last_type_begin, tok_it + 1};
                    const auto return_type = Type::get_type(type_tokens);
                    if (!return_type.has_value()) {
                        return std::nullopt;
                    }
                    return_types.emplace_back(return_type.value());
                    last_type_begin = tok_it + 2;
                }
                tok_it++;
            }
            // Skip the right paren
            tok_it++;
        }
    }

    // Check if a curly brace follows, if it does then the error types follow
    std::vector<std::shared_ptr<Type>> error_types;
    error_types.emplace_back(Type::get_primitive_type("anyerror"));
    const auto brace_start_it = tok_it;
    if (tok_it->token == TOK_LEFT_BRACE) {
        tok_it++;
        while (tok_it->token != TOK_RIGHT_BRACE) {
            std::optional<std::shared_ptr<Type>> err_type = Type::get_type(token_slice{tok_it, tok_it + 1});
            if (!err_type.has_value()) {
                return std::nullopt;
            }
            error_types.emplace_back(err_type.value());
            if (std::next(tok_it)->token == TOK_RIGHT_BRACE) {
                break;
            }
            tok_it += 2;
        }
    }

    // If its the main function, change its name
    if (name == "fc_main") {
        name = "_main";
        if (error_types.size() > 1) {
            // The main function cannot throw user-defined errors, it can only throw errors of type "anyerror"
            const token_slice err_tokens = {brace_start_it, std::prev(definition.second)};
            THROW_ERR(ErrFnMainErrSet, ERR_PARSING, file_name, err_tokens);
            return std::nullopt;
        }

        // The parameter list either has to be empty or contain one `str[]` parameter
        main_function_has_args = !parameters.empty();
        if (parameters.size() > 1) {
            // Too many parameters for the main function
            const token_slice err_tokens = {arg_start_it, arg_end_it};
            THROW_ERR(ErrFnMainTooManyArgs, ERR_PARSING, file_name, err_tokens);
            return std::nullopt;
        } else if (parameters.size() == 1 && std::get<0>(parameters.front())->to_string() != "str[]") {
            // Wrong main argument type
            const token_slice err_tokens = {arg_start_it, arg_end_it};
            THROW_ERR(ErrFnMainWrongArgType, ERR_PARSING, file_name, err_tokens, std::get<0>(parameters.front()));
            return std::nullopt;
        }

        // The main funcition is not allowed to return anything
        if (!return_types.empty()) {
            const token_slice err_tokens = {ret_start_it, std::prev(definition.second)};
            THROW_ERR(ErrFnMainNoReturns, ERR_PARSING, file_name, err_tokens);
            return std::nullopt;
        }
        main_function_parsed = true;
    }

    // Create the body scope
    std::optional<std::shared_ptr<Scope>> body_scope = std::make_shared<Scope>();
    std::shared_ptr<Type> return_type = nullptr;
    if (return_types.size() > 1) {
        return_type = std::make_shared<GroupType>(return_types);
        if (Type::get_type_from_str(return_type->to_string()).has_value()) {
            return_type = Type::get_type_from_str(return_type->to_string()).value();
        } else {
            Type::add_type(return_type);
        }
    } else if (return_types.empty()) {
        return_type = Type::get_primitive_type("void");
    } else {
        return_type = return_types.front();
    }
    body_scope.value()->add_variable("__flint_return_type", return_type, 0, false, true);

    // Add the parameters to the list of variables
    for (const auto &param : parameters) {
        if (!body_scope.value()->add_variable(                                                                  //
                std::get<1>(param), std::get<0>(param), body_scope.value()->scope_id, std::get<2>(param), true) //
        ) {
            // Variable already exists in the func definition list
            THROW_ERR(ErrVarFromRequiresList, ERR_PARSING, file_name, 0, 0, std::get<1>(param));
            return std::nullopt;
        }
    }

    // Dont parse the body yet, it will be parsed in the second pass of the parser
    return FunctionNode(is_aligned, is_const, name, parameters, return_types, error_types, body_scope);
}

std::optional<FunctionNode> Parser::create_extern_function(const token_slice &definition) {
    // First we check if the definition starts with `extern`, it should
    assert(definition.first->token == TOK_EXTERN);
    std::optional<FunctionNode> fn = create_function({definition.first + 1, definition.second});
    if (!fn.has_value()) {
        return std::nullopt;
    }
    // "Delete" the scope of the function, it is not needed for declarations
    fn.value().scope = std::nullopt;
    // Remove the `fc_` prefix for external functions, only internal functions have the prefix
    fn.value().name = fn.value().name.substr(3);
    // Now check whether the FIP provides the searched for function in any of it's modules
    if (!FIP::resolve_function(&fn.value())) {
        return std::nullopt;
    }
    return fn;
}

std::optional<DataNode> Parser::create_data(const token_slice &definition, const std::vector<Line> &body) {
    bool is_shared = false;
    bool is_immutable = false;
    bool is_aligned = false;
    std::string name;

    std::unordered_map<std::string, std::shared_ptr<Type>> fields;
    std::vector<std::string> order;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->token == TOK_SHARED) {
            is_shared = true;
        }
        if (def_it->token == TOK_IMMUTABLE) {
            is_immutable = true;
            // immutable data is shared by default
            is_shared = true;
        }
        if (def_it->token == TOK_ALIGNED) {
            is_aligned = true;
        }
        if (def_it->token == TOK_DATA) {
            name = (def_it + 1)->lexme;
            break;
        }
    }
    for (auto line_it = body.begin(); line_it != body.end(); ++line_it) {
        for (auto token_it = line_it->tokens.first; token_it != line_it->tokens.second; ++token_it) {
            if (token_it->token == TOK_IDENTIFIER && std::next(token_it)->token == TOK_LEFT_PAREN) {
                // It's the initializer
                if (token_it->lexme != name) {
                    THROW_ERR(ErrDefDataWrongConstructorName, ERR_PARSING,                              //
                        file_name, token_it->line, token_it->column, name, std::string(token_it->lexme) //
                    );
                    return std::nullopt;
                }
                // Skip the identifier and the left paren
                token_it += 2;
                // The initializer line must end with a closing paren followed by a semicolon.
                if (std::prev(line_it->tokens.second)->token != TOK_SEMICOLON) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                if ((line_it->tokens.second - 2)->token != TOK_RIGHT_PAREN) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                for (; token_it != line_it->tokens.second - 2; ++token_it) {
                    if (token_it->token == TOK_IDENTIFIER) {
                        if (std::find(order.begin(), order.end(), token_it->lexme) != order.end()) {
                            // The same field name is written down twice in the initializer
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        order.emplace_back(token_it->lexme);
                    } else if (token_it->token != TOK_COMMA) {
                        // Not allowed token in field initializer
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                }
            } else if (Matcher::tokens_start_with({token_it, line_it->tokens.second}, Matcher::type)) {
                // It's a field
                std::optional<uint2> range = Matcher::get_next_match_range({token_it, line_it->tokens.second}, Matcher::type);
                if (!range.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                assert(range.value().first == 0);
                const token_slice type_tokens = {token_it, token_it + range.value().second};
                std::optional<std::shared_ptr<Type>> field_type = Type::get_type(type_tokens);
                if (!field_type.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                token_it += range.value().second;
                const std::string token_it_lexme(token_it->lexme);
                if (token_it->token != TOK_IDENTIFIER) {
                    // Missing field name
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                if (fields.find(token_it_lexme) != fields.end()) {
                    // Field name duplication
                    THROW_ERR(ErrDefDataDuplicateFieldName, ERR_PARSING, file_name, token_it->line, token_it->column, token_it_lexme);
                    return std::nullopt;
                }
                fields[token_it_lexme] = field_type.value();
            }
        }
    }

    std::vector<std::pair<std::string, std::shared_ptr<Type>>> ordered_fields;
    ordered_fields.resize(fields.size());
    for (const auto &field : fields) {
        const size_t field_id = std::distance(order.begin(), std::find(order.begin(), order.end(), field.first));
        ordered_fields[field_id] = {field.first, field.second};
    }

    return DataNode(is_shared, is_immutable, is_aligned, name, ordered_fields);
}

std::optional<FuncNode> Parser::create_func(        //
    [[maybe_unused]] const token_slice &definition, //
    [[maybe_unused]] const std::vector<Line> &body  //
) {
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

Parser::create_entity_type Parser::create_entity(   //
    [[maybe_unused]] const token_slice &definition, //
    [[maybe_unused]] const std::vector<Line> &body  //
) {
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return {};
}

std::vector<std::unique_ptr<LinkNode>> Parser::create_links([[maybe_unused]] const std::vector<Line> &body) {
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    std::vector<std::unique_ptr<LinkNode>> links;
    return links;
}

LinkNode Parser::create_link(const token_slice &tokens) {
    std::vector<std::string> from_references;
    std::vector<std::string> to_references;

    std::vector<uint2> references = Matcher::get_match_ranges(tokens, Matcher::reference);

    for (unsigned int i = references.at(0).first; i < references.at(0).second; i++) {
        if ((tokens.first + i)->token == TOK_IDENTIFIER) {
            from_references.emplace_back((tokens.first + i)->lexme);
        }
    }
    for (unsigned int i = references.at(1).first; i < references.at(1).second; i++) {
        if ((tokens.first + i)->token == TOK_IDENTIFIER) {
            to_references.emplace_back((tokens.first + i)->lexme);
        }
    }

    return LinkNode(from_references, to_references);
}

std::optional<EnumNode> Parser::create_enum(const token_slice &definition, const std::vector<Line> &body) {
    std::string name;
    std::vector<std::string> values;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->token == TOK_ENUM && (def_it + 1)->token == TOK_IDENTIFIER) {
            name = (def_it + 1)->lexme;
            break;
        }
    }

    for (auto body_it = body.front().tokens.first; body_it != body.back().tokens.second; ++body_it) {
        if (body_it->token == TOK_IDENTIFIER) {
            if ((body_it + 1)->token == TOK_COMMA) {
                values.emplace_back(body_it->lexme);
            } else if ((body_it + 1)->token == TOK_SEMICOLON) {
                if ((body_it + 2)->token != TOK_EOL) {
                    // There are more values following in the same line, so the ; is actually a wrong token and should be a , instead
                    THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name,                                           //
                        (body_it + 1)->line, (body_it + 1)->column, std::vector<Token>{TOK_COMMA}, (body_it + 1)->token //
                    );
                    return std::nullopt;
                }
                values.emplace_back(body_it->lexme);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name,                      //
                    (body_it + 1)->line, (body_it + 1)->column, expected, (body_it + 1)->token //
                );
                return std::nullopt;
            }
        }
    }

    return EnumNode(name, values);
}

std::optional<ErrorNode> Parser::create_error(const token_slice &definition, const std::vector<Line> &body) {
    std::string name;
    std::string parent_error = "anyerror";
    std::vector<std::string> error_types;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->token == TOK_ERROR && (def_it + 1)->token == TOK_IDENTIFIER) {
            name = (def_it + 1)->lexme;
        }
        if (def_it->token == TOK_LEFT_PAREN) {
            if ((def_it + 1)->token == TOK_IDENTIFIER && (def_it + 2)->token == TOK_RIGHT_PAREN) {
                parent_error = (def_it + 1)->lexme;
                break;
            }
            THROW_ERR(ErrDefErrOnlyOneParent, ERR_PARSING, file_name, definition);
            return std::nullopt;
        }
    }

    std::vector<std::string> default_messages;
    for (auto body_it = body.front().tokens.first; body_it != body.back().tokens.second; ++body_it) {
        if (body_it->token == TOK_IDENTIFIER) {
            if ((body_it + 1)->token == TOK_LEFT_PAREN) {
                // We have a default message
                if ((body_it + 2)->token != TOK_STR_VALUE) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                if ((body_it + 3)->token != TOK_RIGHT_PAREN) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                default_messages.emplace_back((body_it + 2)->lexme);
                error_types.emplace_back(body_it->lexme);
                body_it += 4;
                continue;
            }
            default_messages.emplace_back("");
            if ((body_it + 1)->token == TOK_COMMA) {
                error_types.emplace_back(body_it->lexme);
            } else if ((body_it + 1)->token == TOK_SEMICOLON) {
                if ((body_it + 2)->token != TOK_EOL) {
                    // There are more values following in the same line, so the ; is actually a wrong token and should be a , instead
                    THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name,                                           //
                        (body_it + 1)->line, (body_it + 1)->column, std::vector<Token>{TOK_COMMA}, (body_it + 1)->token //
                    );
                    return std::nullopt;
                }
                error_types.emplace_back(body_it->lexme);
                break;
            } else {
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name,                                                          //
                    (body_it + 1)->line, (body_it + 1)->column, std::vector<Token>{TOK_COMMA, TOK_SEMICOLON}, (body_it + 1)->token //
                );
                return std::nullopt;
            }
        };
    }

    return ErrorNode(name, parent_error, error_types, default_messages);
}

std::optional<VariantNode> Parser::create_variant(const token_slice &definition, const std::vector<Line> &body) {
    assert(definition.first->token == TOK_VARIANT);
    assert((definition.first + 1)->token == TOK_IDENTIFIER);
    assert((definition.first + 2)->token == TOK_COLON);
    assert((definition.first + 3) == definition.second);
    const std::string name((definition.first + 1)->lexme);

    std::vector<std::pair<std::optional<std::string>, std::shared_ptr<Type>>> possible_types;
    for (auto body_it = body.front().tokens.first; body_it != body.back().tokens.second;) {
        if (body_it->token == TOK_COMMA) {
            ++body_it;
            continue;
        }
        if ((body_it + 1)->token == TOK_LEFT_PAREN) {
            // Is tagged
            std::string tag = "";
            if (body_it->token == TOK_IDENTIFIER) {
                tag = body_it->lexme;
            } else if (body_it->token == TOK_TYPE) {
                tag = body_it->type->to_string();
            } else {
                // Not-allowed token found as tag
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            ++body_it;
            assert(body_it->token == TOK_LEFT_PAREN);
            if (std::next(body_it)->token == TOK_RIGHT_PAREN) {
                // Empty tagged variant
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            std::vector<std::shared_ptr<Type>> types;
            while (body_it != body.back().tokens.second && (body_it->token == TOK_COMMA || body_it->token == TOK_LEFT_PAREN)) {
                body_it++;
                token_slice type_tokens = {body_it, body.back().tokens.second};
                if (std::next(body_it) == body.back().tokens.second || !Matcher::tokens_start_with(type_tokens, Matcher::type)) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                const std::optional<uint2> type_range = Matcher::get_next_match_range(type_tokens, Matcher::type);
                if (!type_range.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                assert(type_range.value().first == 0);
                type_tokens.second = body_it + type_range.value().second;
                token_list toks = clone_from_slice(type_tokens);
                const std::optional<std::shared_ptr<Type>> type = Type::get_type(type_tokens);
                if (!type.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                types.emplace_back(type.value());
                body_it = type_tokens.second;
            }
            if (body_it == body.back().tokens.second || body_it->token != TOK_RIGHT_PAREN) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            body_it++;
            if (types.size() > 1) {
                std::shared_ptr<Type> tuple_type = std::make_shared<TupleType>(types);
                if (!Type::add_type(tuple_type)) {
                    tuple_type = Type::get_type_from_str(tuple_type->to_string()).value();
                }
                possible_types.emplace_back(tag, tuple_type);
            } else {
                possible_types.emplace_back(tag, types.front());
            }
            if (body_it->token == TOK_SEMICOLON) {
                break;
            }
            continue;
        } else if (Matcher::tokens_start_with({body_it, body.back().tokens.second}, Matcher::type)) {
            // Is untagged
            std::optional<std::string> notag;
            token_slice type_tokens = {body_it, body.back().tokens.second};
            token_list toks = clone_from_slice(type_tokens);
            std::optional<uint2> type_range = Matcher::get_next_match_range(type_tokens, Matcher::type);
            // Now we can adjust the type token's end with our range and get the type and add it to the list
            assert(type_range.value().first == 0);
            type_tokens.second = body_it + type_range.value().second;
            const std::optional<std::shared_ptr<Type>> type = Type::get_type(type_tokens);
            if (!type.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            possible_types.emplace_back(notag, type.value());
            body_it = type_tokens.second;
            if (body_it->token == TOK_SEMICOLON) {
                break;
            }
        }
        ++body_it;
    }

    return VariantNode(name, possible_types);
}

std::optional<TestNode> Parser::create_test(const token_slice &definition) {
    std::string test_name;
    // Extract the name of the test
    auto it = definition.first;
    for (; it != definition.second; ++it) {
        if (it->token == TOK_TEST && std::next(it) != definition.second && std::next(it)->token == TOK_STR_VALUE) {
            ++it;
            test_name = it->lexme;
            break;
        }
    }
    if (test_name == "") {
        // Empty test names are not allowed
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Create the body scope
    std::shared_ptr<Scope> body_scope = std::make_shared<Scope>();

    // Check if this test already exists within this file
    if (!TestNode::check_test_name(file_name, test_name)) {
        THROW_ERR(ErrTestRedefinition, ERR_PARSING, file_name, it->line, it->column, test_name);
        return std::nullopt;
    }

    // Dont parse the body yet, it will be parsed as part of the second pass of the parser
    return TestNode(file_name, test_name, body_scope);
}

std::optional<ImportNode> Parser::create_import(const token_slice &tokens) {
    std::variant<std::pair<std::optional<std::string>, std::string>, std::vector<std::string>> import_path;
    std::optional<std::string> alias;

    auto iterator = tokens.first;
    if (Matcher::tokens_contain(tokens, Matcher::token(TOK_STR_VALUE))) {
        for (; iterator != tokens.second; ++iterator) {
            if (iterator->token == TOK_STR_VALUE) {
                const size_t path_separator = iterator->lexme.find_last_of('/');
                if (path_separator == std::string::npos) {
                    import_path = std::make_pair(std::nullopt, std::string(iterator->lexme));
                } else {
                    import_path = std::make_pair(                               //
                        std::string(iterator->lexme.substr(0, path_separator)), //
                        std::string(iterator->lexme.substr(path_separator + 1)) //
                    );
                }
                ++iterator;
                break;
            }
        }
    } else {
        const auto matches = Matcher::get_match_ranges(tokens, Matcher::use_reference).front();
        std::vector<std::string> path;
        for (unsigned int i = matches.first; i < matches.second; i++) {
            if ((tokens.first + i)->token == TOK_IDENTIFIER) {
                path.emplace_back((tokens.first + i)->lexme);
            }
        }
        if (path.empty()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        iterator = tokens.first + matches.second;
        import_path = path;
    }

    // Check if an alias follows
    if (iterator->token == TOK_AS) {
        ++iterator;
        if (iterator == tokens.second) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (iterator->token != TOK_IDENTIFIER) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        alias = iterator->lexme;
    }

    return ImportNode(import_path, alias);
}
