#include "error/error_type.hpp"
#include "error/error_types/parsing/definitions/data/err_def_data_duplicate_field_name.hpp"
#include "lexer/token.hpp"
#include "lexer/token_context.hpp"
#include "matcher/matcher.hpp"
#include "parser/parser.hpp"

#include "error/error.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/interface_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/tuple_type.hpp"

#include "fip.hpp"
#include "parser/type/unknown_type.hpp"

#include <algorithm>
#include <optional>
#include <string>

std::optional<FunctionNode> Parser::create_function(                                                //
    const token_slice &definition,                                                                  //
    const std::optional<std::pair<std::string, std::vector<FuncNode::RequiredData>>> &required_data //
) {
    PROFILE_CUMULATIVE("Parser::create_function");
    std::string name;
    std::vector<std::tuple<std::shared_ptr<Type>, std::string, bool>> parameters;
    std::vector<std::shared_ptr<Type>> return_types;
    bool is_const = false;
    bool is_extern = false;

    auto tok_it = definition.first;
    // Parse everything before the parameters
    bool def_missing = true;
    while (tok_it != definition.second && std::next(tok_it) != definition.second && tok_it->token != TOK_LEFT_PAREN) {
        if (tok_it->token == TOK_CONST) {
            is_const = true;
        }
        if (tok_it->token == TOK_EXTERN) {
            is_extern = true;
        }
        if (tok_it->token == TOK_DEF) {
            name = std::next(tok_it)->lexme;
            def_missing = false;
        }
        tok_it++;
    }
    if (def_missing) {
        THROW_ERR(ErrFnDefMissing, ERR_PARSING, file_hash, definition);
        return std::nullopt;
    }
    ASSERT(tok_it != definition.second);
    // Add implicit required data parameters with mutability based on is_const
    if (required_data.has_value()) {
        for (auto &data_param : required_data.value().second) {
            parameters.emplace_back(data_param.type, data_param.accessor_name, !is_const);
        }
    }
    // Check if the name is reserved
    if (name == "_main") {
        token_slice err_tokens = {std::prev(tok_it), definition.second};
        THROW_ERR(ErrFnReservedName, ERR_PARSING, file_hash, err_tokens, name);
        return std::nullopt;
    } else if (name == "main" && main_function.load() != nullptr) {
        // Redefinition of the main function
        token_slice err_tokens = {std::prev(tok_it), definition.second};
        THROW_ERR(ErrFnMainRedefinition, ERR_PARSING, file_hash, err_tokens, main_function.load());
        return std::nullopt;
    }
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
                continue;
            }
            if (depth == 0 && (std::next(tok_it)->token == TOK_COMMA || std::next(tok_it)->token == TOK_RIGHT_PAREN)) {
                // The current token is the parameter name
                const std::string param_name(tok_it->lexme);
                if (required_data.has_value()) {
                    // Check if there are any overlaps with required data
                    for (const auto &required_data_value : required_data.value().second) {
                        if (required_data_value.accessor_name == param_name) {
                            THROW_ERR(                                                                                                  //
                                ErrFnParamShadowsRequiredData, ERR_PARSING, file_hash, token_slice{last_param_begin, std::next(tok_it)} //
                            );
                            return std::nullopt;
                        }
                    }
                }
                // The type is everything from the last param begin
                token_slice type_tokens = {last_param_begin, tok_it};
                bool is_mutable = false;
                if (type_tokens.first->token == TOK_CONST) {
                    type_tokens.first++;
                } else if (type_tokens.first->token == TOK_MUT) {
                    is_mutable = true;
                    type_tokens.first++;
                }
                const auto param_type = file_node_ptr->file_namespace->get_type(type_tokens);
                if (!param_type.has_value()) {
                    return std::nullopt;
                }
                parameters.emplace_back(param_type.value(), param_name, is_mutable);
                last_param_begin = tok_it + 2;
            }
            tok_it++;
        }
        ASSERT(tok_it != definition.second);
    }
    const auto arg_end_it = tok_it;
    // Skip the right paren
    tok_it++;

    // Now the token should be an arrow, if not there are no return values
    auto ret_start_it = tok_it;
    if (tok_it->token == TOK_ARROW) {
        tok_it++;
        ret_start_it++;
        ASSERT(tok_it != definition.second);
        if (tok_it->token != TOK_LEFT_PAREN) {
            // There is only a single return type, so everything until the colon is considere the return type
            std::optional<uint2> type_range = Matcher::get_next_match_range({tok_it, definition.second}, Matcher::type);
            ASSERT(type_range.has_value());
            ASSERT(type_range.value().first == 0);
            token_slice type_tokens = {tok_it, tok_it + type_range.value().second};
            const auto return_type = file_node_ptr->file_namespace->get_type(type_tokens);
            if (!return_type.has_value()) {
                return std::nullopt;
            }
            if (return_type.value()->get_variation() == Type::Variation::TUPLE) {
                THROW_ERR(ErrFnCannotReturnTuple, ERR_PARSING, file_hash, type_tokens, return_type.value());
                return std::nullopt;
            }
            return_types.emplace_back(return_type.value());
            tok_it = type_tokens.second;
        } else {
            // Skip the left paren
            tok_it++;
            // Parse the return types
            token_list::iterator last_type_begin = tok_it;
            unsigned int depth = 0;
            while (tok_it != definition.second) {
                if (tok_it->token == TOK_LESS || tok_it->token == TOK_LEFT_BRACKET) {
                    depth++;
                    tok_it++;
                    continue;
                } else if (tok_it->token == TOK_GREATER || tok_it->token == TOK_RIGHT_BRACKET) {
                    depth--;
                    tok_it++;
                    continue;
                } else if (depth == 0 && (tok_it->token == TOK_COMMA || tok_it->token == TOK_RIGHT_PAREN)) {
                    // The type is everything from the last param begin
                    token_slice type_tokens = {last_type_begin, tok_it};
                    const auto return_type = file_node_ptr->file_namespace->get_type(type_tokens);
                    if (!return_type.has_value()) {
                        return std::nullopt;
                    }
                    return_types.emplace_back(return_type.value());
                    last_type_begin = tok_it + 1;
                    if (tok_it->token == TOK_RIGHT_PAREN) {
                        break;
                    }
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
            std::optional<std::shared_ptr<Type>> err_type = file_node_ptr->file_namespace->get_type(token_slice{tok_it, tok_it + 1});
            if (!err_type.has_value()) {
                return std::nullopt;
            }
            error_types.emplace_back(err_type.value());
            const bool is_last = std::next(tok_it)->token == TOK_RIGHT_BRACE;
            tok_it += 2;
            if (is_last) {
                break;
            }
        }
    }
    // Check if a body should follow (`:`) or if it's just a function declaration (`;`)
    if (tok_it->token != TOK_COLON && tok_it->token != TOK_SEMICOLON) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    const bool is_declaration = tok_it->token == TOK_SEMICOLON;

    // If its the main function, change its name
    if (name == "main") {
        if (is_declaration) {
            THROW_ERR(ErrFnMainInvalid, ERR_PARSING, file_hash, definition);
            return std::nullopt;
        }
        if (required_data.has_value()) {
            // It is not allowed to define the main function within a func module
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        name = "_main";
        if (error_types.size() > 1) {
            // The main function cannot throw user-defined errors, it can only throw errors of type "anyerror"
            const token_slice err_tokens = {brace_start_it, std::prev(definition.second)};
            THROW_ERR(ErrFnMainErrSet, ERR_PARSING, file_hash, err_tokens);
            return std::nullopt;
        }

        // The parameter list either has to be empty or contain one `str[]` parameter
        if (parameters.size() > 1) {
            // Too many parameters for the main function
            const token_slice err_tokens = {arg_start_it, arg_end_it};
            THROW_ERR(ErrFnMainTooManyArgs, ERR_PARSING, file_hash, err_tokens);
            return std::nullopt;
        } else if (parameters.size() == 1 && std::get<0>(parameters.front())->to_string() != "str[]") {
            // Wrong main argument type
            const token_slice err_tokens = {arg_start_it, arg_end_it};
            THROW_ERR(ErrFnMainWrongArgType, ERR_PARSING, file_hash, err_tokens, std::get<0>(parameters.front()));
            return std::nullopt;
        }

        // The main funcition is not allowed to return anything except i32
        if (!return_types.empty() && (return_types.size() > 1 || return_types.front()->to_string() != "i32")) {
            const token_slice err_tokens = {ret_start_it, std::prev(definition.second)};
            THROW_ERR(ErrFnMainInvalid, ERR_PARSING, file_hash, err_tokens);
            return std::nullopt;
        }
        main_file_hash = file_hash;
    }

    // Create the body scope
    std::optional<std::shared_ptr<Scope>> body_scope = std::nullopt;
    if (!is_declaration) {
        body_scope = std::make_shared<Scope>();
        std::shared_ptr<Type> return_type = nullptr;
        if (return_types.size() > 1) {
            return_type = std::make_shared<GroupType>(return_types);
            if (!file_node_ptr->file_namespace->add_type(return_type)) {
                return_type = file_node_ptr->file_namespace->get_type_from_str(return_type->to_string()).value();
            }
        } else if (return_types.empty()) {
            return_type = Type::get_primitive_type("void");
        } else {
            return_type = return_types.front();
        }
        body_scope.value()->add_variable("flint.return_type",
            {
                .type = return_type,
                .scope_id = 0,
                .scope_segment = 0,
                .is_mutable = false,
                .is_persistent = false,
                .is_fn_param = true,
                .is_pseudo_variable = true,
            });

        // Add the parameters to the list of variables
        for (const auto &param : parameters) {
            if (!body_scope.value()->add_variable(std::get<1>(param),
                    {
                        .type = std::get<0>(param),
                        .scope_id = body_scope.value()->scope_id,
                        .scope_segment = 0,
                        .is_mutable = std::get<2>(param),
                        .is_persistent = false,
                        .is_fn_param = true,
                    }) //
            ) {
                // Variable already exists in the func definition list
                THROW_ERR(ErrVarFromRequiresList, ERR_PARSING, file_hash, 0, 0, std::get<1>(param));
                return std::nullopt;
            }
        }
    }
    // Dont parse the body yet, it will be parsed in the second pass of the parser
    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    std::optional<size_t> mid = std::nullopt;
    if (name != "_main" && !is_extern) {
        mid = next_mangle_id;
        next_mangle_id++;
    }
    if (required_data.has_value()) {
        // If there is required data then this function is defined inside a func module, the name needs to be changed accordingly
        name = required_data.value().first + "." + name;
    }
    return FunctionNode(                                                                         //
        file_hash, line, column, length,                                                         //
        is_const, is_extern, false, name, parameters, return_types, error_types, body_scope, mid //
    );
}

std::optional<FunctionNode> Parser::create_extern_function(const token_slice &definition) {
    PROFILE_CUMULATIVE("Parser::create_extern_function");
    // First we check if the definition starts with `extern`, it should
    ASSERT(definition.first->token == TOK_EXTERN);
    std::optional<FunctionNode> fn = create_function(definition, {});
    if (!fn.has_value()) {
        return std::nullopt;
    }
    // Correctly set the line and column of the function definition
    fn.value().line = definition.first->line;
    fn.value().column = definition.first->column;
    fn.value().length = definition.second->column - definition.first->column;
    // "Delete" the scope of the function, it is not needed for declarations
    fn.value().scope = std::nullopt;
    // Checking whether the function exists in FIP is done when parsing all open functions
    return fn;
}

std::optional<DataNode> Parser::create_data(const token_slice &definition, const std::vector<Line> &body) {
    PROFILE_CUMULATIVE("Parser::create_data");
    bool is_const = false;
    bool is_shared = false;
    std::string name;

    std::vector<DataNode::Field> fields;
    std::vector<std::string> order;

    for (                                                                                                    //
        auto def_it = definition.first;                                                                      //
        def_it != definition.second && (def_it == definition.first || std::prev(def_it)->token != TOK_DATA); //
        ++def_it                                                                                             //
    ) {
        switch (def_it->token) {
            default:
                break;
            case TOK_CONST:
                is_const = true;
                break;
            case TOK_SHARED:
                is_shared = true;
                break;
            case TOK_DATA:
                name = (def_it + 1)->lexme;
                break;
        }
    }
    for (auto line_it = body.begin(); line_it != body.end(); ++line_it) {
        for (auto token_it = line_it->tokens.first; token_it != line_it->tokens.second; ++token_it) {
            if (token_it->token == TOK_IDENTIFIER && std::next(token_it)->token == TOK_LEFT_PAREN) {
                if (is_const || is_shared) {
                    // Const and shared data is not allowed to have an initalizer
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                // It's the initializer
                if (token_it->lexme != name) {
                    THROW_ERR(ErrDefDataWrongConstructorName, ERR_PARSING,                              //
                        file_hash, token_it->line, token_it->column, name, std::string(token_it->lexme) //
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
                            THROW_ERR(                                                         //
                                ErrDefDataDuplicateFieldName, ERR_PARSING, file_hash,          //
                                token_it->line, token_it->column, std::string(token_it->lexme) //
                            );
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
                ASSERT(range.value().first == 0);
                unsigned int type_advance = range.value().second;
                token_slice type_tokens = {token_it, token_it + type_advance};
                std::optional<std::shared_ptr<Type>> field_type = file_node_ptr->file_namespace->get_type(type_tokens);
                // Check the field type comes from an aliased import file, like `a.b.Type` or `a.Type` for example
                // We cannot just use `collapse_types_in_slice` for data because we are in an early pass where the outer "main" iterator is
                // not allowed to be invalidated, unlike later on phases where all iterators are already line-based (and thus the delete
                // callback is properly executed for re-validating the bounds)
                const bool type_unknown = !field_type.has_value() || field_type.value()->get_variation() == Type::Variation::UNKNOWN;
                if (type_advance == 1 && type_tokens.first->token == TOK_IDENTIFIER && type_unknown) {
                    const std::string first_name(type_tokens.first->lexme);
                    const auto result = resolve_alias_in_type(first_name, type_tokens.second, line_it->tokens.second);
                    if (result.extra_tokens > 0) {
                        type_advance += result.extra_tokens;
                        type_tokens = {token_it, token_it + type_advance};
                        field_type = result.resolved_type;
                    }
                }
                if (!field_type.has_value()) {
                    THROW_ERR(ErrUnknownType, ERR_PARSING, file_hash, type_tokens);
                    return std::nullopt;
                }
                token_it += type_advance;
                const std::string token_it_lexme(token_it->lexme);
                if (token_it->token != TOK_IDENTIFIER) {
                    // Missing field name
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                for (const auto &field : fields) {
                    if (field.name == token_it_lexme) {
                        // Field name duplication
                        THROW_ERR(ErrDefDataDuplicateFieldName, ERR_PARSING, file_hash, token_it->line, token_it->column, token_it_lexme);
                        return std::nullopt;
                    }
                }
                // Check if a ; follows, then it's a "normal" field, if a '=' follows then the field has a default value. It is parsed in
                // the second phase of the parser.
                token_it++;
                if (token_it->token != TOK_SEMICOLON && token_it->token != TOK_EQUAL) {
                    // Unexpected token after field definition
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                std::optional<std::unique_ptr<ExpressionNode>> field_initializer = std::nullopt;
                std::optional<token_slice> field_initializer_tokens = std::nullopt;
                if (token_it->token == TOK_EQUAL) {
                    // Get all tokens of the expression by simply moving the token until it points to a semicolon
                    ++token_it;
                    field_initializer_tokens = {token_it, token_it};
                    while (token_it->token != TOK_SEMICOLON) {
                        token_it++;
                    }
                    field_initializer_tokens.value().second = token_it;
                }
                if (is_const && !field_initializer_tokens.has_value()) {
                    // Every field must have an initializer if the data is const
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                fields.emplace_back(DataNode::Field{
                    .name = token_it_lexme,
                    .type = field_type.value(),
                    .initializer_tokens = field_initializer_tokens,
                    .initializer = std::move(field_initializer),
                });
            }
        }
    }

    std::vector<DataNode::Field> ordered_fields;
    ordered_fields.resize(fields.size());
    if (order.empty()) {
        if (!is_const && !is_shared) {
            // Empty order on data type whose constructor should contain values
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        ordered_fields = std::move(fields);
    } else {
        if (order.size() != fields.size()) {
            // Not all fields are part of the initializer, at least one is missing
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        for (auto &field : fields) {
            const size_t field_id = std::distance(order.begin(), std::find(order.begin(), order.end(), field.name));
            // TODO: This crashes when we have a field in the constructor that's not present in the field list of the data
            ordered_fields[field_id] = std::move(field);
        }
    }

    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    return DataNode(file_hash, line, column, length, is_const, is_shared, name, ordered_fields);
}

std::optional<FuncNode> Parser::create_func(const token_slice &definition, const std::vector<Line> &body) {
    PROFILE_CUMULATIVE("Parser::create_func");
    token_slice token_mut = definition;
    ASSERT(token_mut.first->token == TOK_FUNC);
    token_mut.first++;
    ASSERT(token_mut.first->token == TOK_IDENTIFIER);
    const std::string func_name(token_mut.first->lexme);
    token_mut.first++;
    std::vector<FuncNode::RequiredData> required_data;
    if (token_mut.first->token == TOK_REQUIRES) {
        auto tok_it = token_mut.first + 1;
        ASSERT(tok_it->token == TOK_LEFT_PAREN);
        tok_it++;
        while (tok_it != token_mut.second && tok_it->token != TOK_RIGHT_PAREN) {
            // The current token is the type
            const auto required_data_type = file_node_ptr->file_namespace->get_type({tok_it, tok_it + 1});
            if (!required_data_type.has_value()) {
                return std::nullopt;
            }
            if (required_data_type.value()->get_variation() != Type::Variation::DATA       //
                && required_data_type.value()->get_variation() != Type::Variation::UNKNOWN //
            ) {
                THROW_ERR(                                                                       //
                    ErrDefFuncRequiredTypeNotData, ERR_PARSING, file_hash,                       //
                    tok_it->line, tok_it->column, required_data_type.value()->to_string().size() //
                );
                return std::nullopt;
            }
            // The next token is the required data accessor name
            ASSERT((tok_it + 1)->token == TOK_IDENTIFIER);
            const std::string access_name((tok_it + 1)->lexme);
            for (const auto &present : required_data) {
                if (present.type->equals(required_data_type.value())) {
                    THROW_ERR(                                                      //
                        ErrDefFuncRequiringSameDataTwice, ERR_PARSING, file_hash,   //
                        token_slice{tok_it, tok_it + 2}, required_data_type.value() //
                    );
                    return std::nullopt;
                }
            }
            required_data.emplace_back(FuncNode::RequiredData{
                .type = required_data_type.value(),
                .accessor_name = access_name,
                .line = tok_it->line,
                .column = tok_it->column,
            });
            if ((tok_it + 2)->token == TOK_RIGHT_PAREN) {
                tok_it += 2;
            } else {
                tok_it += 3;
            }
        }
        ASSERT(tok_it != definition.second);
    }

    std::vector<FunctionNode *> functions;
    std::vector<Line> body_mut = body;
    while (!body_mut.empty()) {
        const Line function_definition_line = body_mut.front();
        body_mut.erase(body_mut.begin());
        std::pair<std::string, std::vector<FuncNode::RequiredData>> required_data_pair{func_name, required_data};
        std::optional<FunctionNode> fn = create_function(function_definition_line.tokens, required_data_pair);
        if (!fn.has_value()) {
            return std::nullopt;
        }
        std::optional<FunctionNode *> added_function = file_node_ptr->add_function(fn.value(), core_namespaces);
        if (!added_function.has_value()) {
            return std::nullopt;
        }
        std::vector<Line> function_body_lines;
        for (auto it = body_mut.begin(); it != body_mut.end();) {
            if (it->indent_lvl > function_definition_line.indent_lvl) {
                function_body_lines.emplace_back(*it);
                body_mut.erase(it);
                continue;
            }
            break;
        }
        if (function_body_lines.empty()) {
            THROW_ERR(ErrDefFuncContansVirtualFunction, ERR_PARSING, file_hash, function_definition_line.tokens);
            return std::nullopt;
        }
        add_open_function({added_function.value(), function_body_lines});
        functions.emplace_back(added_function.value());
    }
    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    return FuncNode(   //
        file_hash,     //
        line,          //
        column,        //
        length,        //
        func_name,     //
        required_data, //
        functions      //
    );
}

std::optional<InterfaceNode> Parser::create_interface(const token_slice &definition, const std::vector<Line> &body) {
    PROFILE_CUMULATIVE("Parser::create_interface");
    token_slice token_mut = definition;
    ASSERT(token_mut.first->token == TOK_INTERFACE);
    token_mut.first++;
    ASSERT(token_mut.first->token == TOK_IDENTIFIER);
    const std::string interface_name(token_mut.first->lexme);
    token_mut.first++;

    std::vector<FunctionNode *> functions;
    std::vector<Line> body_mut = body;
    while (!body_mut.empty()) {
        const Line function_definition_line = body_mut.front();
        body_mut.erase(body_mut.begin());
        const std::pair<std::string, std::vector<FuncNode::RequiredData>> required_data = {interface_name, {}};
        std::optional<FunctionNode> fn = create_function(function_definition_line.tokens, required_data);
        if (!fn.has_value()) {
            return std::nullopt;
        }
        std::optional<FunctionNode *> added_function = file_node_ptr->add_function(fn.value(), core_namespaces);
        if (!added_function.has_value()) {
            return std::nullopt;
        }
        std::vector<Line> function_body_lines;
        for (auto it = body_mut.begin(); it != body_mut.end();) {
            if (it->indent_lvl > function_definition_line.indent_lvl) {
                function_body_lines.emplace_back(*it);
                body_mut.erase(it);
                continue;
            }
            break;
        }
        if (!function_body_lines.empty()) {
            THROW_ERR(ErrDefInterfaceContainsConcreteFunction, ERR_PARSING, file_hash, function_definition_line.tokens);
            return std::nullopt;
        }
        functions.emplace_back(added_function.value());
    }
    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    return InterfaceNode( //
        file_hash,        //
        line,             //
        column,           //
        length,           //
        interface_name,   //
        functions         //
    );
}

std::optional<ObjectNode> Parser::create_object(const token_slice &definition, const std::vector<Line> &body) {
    PROFILE_CUMULATIVE("Parser::create_object");
    auto tok_it = definition.first;
    ASSERT(tok_it->token == TOK_OBJECT);
    tok_it++;
    ASSERT(tok_it->token == TOK_IDENTIFIER);
    const std::string object_name(tok_it->lexme);
    tok_it++;
    std::unordered_map<std::string, ObjectNode::ImplementedInterface> interfaces;
    if (tok_it->token == TOK_IMPLEMENTS) {
        tok_it++;
        ASSERT(tok_it->token == TOK_LEFT_PAREN);
        tok_it++;
        while (tok_it != definition.second && tok_it->token != TOK_RIGHT_PAREN) {
            // The current token is the type
            const auto interface_type = file_node_ptr->file_namespace->get_type({tok_it, tok_it + 1});
            if (!interface_type.has_value()) {
                return std::nullopt;
            }
            if (interface_type.value()->get_variation() != Type::Variation::INTERFACE  //
                && interface_type.value()->get_variation() != Type::Variation::UNKNOWN //
            ) {
                THROW_ERR(ErrDefObjectImplementedTypeNotInterface, ERR_PARSING, file_hash,
                    ASTNode::PosTriple{
                        tok_it->line,
                        tok_it->column,
                        static_cast<unsigned int>(interface_type.value()->to_string().size()),
                    } //
                );
                return std::nullopt;
            }
            // Check if this interface type is already present in the interfaces list
            for (const auto &[interface_name, interface] : interfaces) {
                if (interface_name == interface_type.value()->to_string()) {
                    THROW_ERR(ErrDefObjectDuplicateInterface, ERR_PARSING, file_hash, tok_it->line, tok_it->column, interface_name);
                    return std::nullopt;
                }
            }
            interfaces[interface_type.value()->to_string()] = ObjectNode::ImplementedInterface{
                .pos =
                    ASTNode::PosTriple{
                        .line = tok_it->line,
                        .column = tok_it->column,
                        .length = static_cast<uint32_t>(interface_type.value()->to_string().size()),
                    },
                .interface = interface_type.value()->get_variation() == Type::Variation::INTERFACE //
                    ? interface_type.value()->as<InterfaceType>()->interface_node                  //
                    : nullptr,
            };
            tok_it++;
            if (tok_it->token != TOK_COMMA && tok_it->token != TOK_RIGHT_PAREN) {
                THROW_ERR(                                                                                      //
                    ErrParsUnexpectedToken, ERR_PARSING, file_hash,                                             //
                    tok_it->line, tok_it->column, std::vector<Token>{TOK_COMMA, TOK_RIGHT_PAREN}, tok_it->token //
                );
                return std::nullopt;
            }
            if (tok_it->token == TOK_COMMA) {
                // Skip comma, but don't skip right paren
                tok_it++;
            }
        }
        ASSERT(tok_it != definition.second);
    }

    // Process all free-floating functions, if any are following
    std::vector<FunctionNode *> functions;
    auto line_it = body.begin();
    while (line_it != body.end()) {
        if (!Matcher::tokens_contain(line_it->tokens, Matcher::function_definition)) {
            const size_t definition_indent = line_it->indent_lvl;
            line_it++;
            while (line_it != body.end() && line_it->indent_lvl > definition_indent) {
                line_it++;
            }
            continue;
        }
        // Get all the body lines
        std::vector<Line> body_lines;
        const auto &definition_tokens = line_it->tokens;
        const size_t definition_indent_lvl = line_it->indent_lvl;
        line_it++;
        while (line_it != body.end() && line_it->indent_lvl > definition_indent_lvl) {
            body_lines.emplace_back(*line_it);
            line_it++;
        }
        if (body_lines.empty()) {
            THROW_ERR(ErrMissingBody, ERR_PARSING, file_hash, definition_tokens);
            return std::nullopt;
        }
        // Dont actually parse the function body, only its definition
        std::shared_ptr<Type> object_type = std::make_shared<UnknownType>(object_name);
        if (!file_node_ptr->file_namespace->add_type(object_type)) {
            object_type = file_node_ptr->file_namespace->get_type_from_str(object_type->to_string()).value();
        }
        const std::optional<std::pair<std::string, std::vector<FuncNode::RequiredData>>> required_data = std::make_pair( //
            object_name,
            std::vector<FuncNode::RequiredData>{FuncNode::RequiredData{
                .type = object_type,
                .accessor_name = "self",
                .line = 0,
                .column = 0,
            }} //
        );
        std::optional<FunctionNode> function_node = create_function(definition_tokens, required_data);
        if (!function_node.has_value()) {
            return std::nullopt;
        }
        // Check if the same function (with same signature) already exists within this object as a free-floating function
        for (const FunctionNode *function : functions) {
            if (function->name != function_node.value().name) {
                continue;
            }
            if (function->parameters.size() != function_node.value().parameters.size()) {
                continue;
            }
            bool all_match = true;
            for (size_t i = 0; i < function->parameters.size(); i++) {
                const auto &p1 = function->parameters.at(i);
                const auto &p2 = function_node.value().parameters.at(i);
                if (!std::get<0>(p1)->equals(std::get<0>(p2))) {
                    all_match = false;
                    break;
                }
            }
            if (all_match) {
                // Duplicate function inside object definition, the free-floating function already exists
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
        }
        std::optional<FunctionNode *> added_function = file_node_ptr->add_function(function_node.value(), core_namespaces);
        if (!added_function.has_value()) {
            return std::nullopt;
        }
        add_open_function({added_function.value(), body_lines});
        functions.emplace_back(added_function.value());
    }

    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    return ObjectNode(file_hash, line, column, length, object_name, functions, interfaces);
}

std::optional<EnumNode> Parser::create_enum(const token_slice &definition, const std::vector<Line> &body) {
    PROFILE_CUMULATIVE("Parser::create_enum");
    std::string name;
    std::vector<std::pair<std::string, unsigned int>> values;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->token == TOK_ENUM && (def_it + 1)->token == TOK_IDENTIFIER) {
            name = (def_it + 1)->lexme;
            break;
        }
    }

    for (auto body_it = body.front().tokens.first; body_it != body.back().tokens.second; ++body_it) {
        if (body_it->token == TOK_IDENTIFIER) {
            // Check if this tag has already been used
            for (const auto &[t, v] : values) {
                if (t == body_it->lexme) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
            }
            const std::string tag(body_it->lexme);
            unsigned int value = values.empty() ? 0 : values.back().second + 1;
            if ((body_it + 1)->token == TOK_EQUAL && (body_it + 2)->token == TOK_INT_VALUE) {
                value = std::stoi(std::string((body_it + 2)->lexme));
                // Check if this value has already been used
                for (const auto &[t, v] : values) {
                    if (v == value) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                }
                // Skip the tag and the =, so body_it now points at the integer value
                body_it += 2;
            }
            if ((body_it + 1)->token == TOK_COMMA) {
                values.emplace_back(tag, value);
            } else if ((body_it + 1)->token == TOK_SEMICOLON) {
                if ((body_it + 2)->token != TOK_EOL) {
                    // There are more values following in the same line, so the ; is actually a wrong token and should be a , instead
                    THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_hash,                                           //
                        (body_it + 1)->line, (body_it + 1)->column, std::vector<Token>{TOK_COMMA}, (body_it + 1)->token //
                    );
                    return std::nullopt;
                }
                values.emplace_back(tag, value);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_hash,                      //
                    (body_it + 1)->line, (body_it + 1)->column, expected, (body_it + 1)->token //
                );
                return std::nullopt;
            }
        }
    }

    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    return EnumNode(file_hash, line, column, length, name, values);
}

std::optional<ErrorNode> Parser::create_error(const token_slice &definition, const std::vector<Line> &body) {
    PROFILE_CUMULATIVE("Parser::create_error");
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
            THROW_ERR(ErrDefErrOnlyOneParent, ERR_PARSING, file_hash, definition);
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
                    THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_hash,                                           //
                        (body_it + 1)->line, (body_it + 1)->column, std::vector<Token>{TOK_COMMA}, (body_it + 1)->token //
                    );
                    return std::nullopt;
                }
                error_types.emplace_back(body_it->lexme);
                break;
            } else {
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_hash,                                                          //
                    (body_it + 1)->line, (body_it + 1)->column, std::vector<Token>{TOK_COMMA, TOK_SEMICOLON}, (body_it + 1)->token //
                );
                return std::nullopt;
            }
        };
    }

    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    return ErrorNode(file_hash, line, column, length, name, parent_error, error_types, default_messages);
}

std::optional<VariantNode> Parser::create_variant(const token_slice &definition, const std::vector<Line> &body) {
    PROFILE_CUMULATIVE("Parser::create_variant");
    ASSERT(definition.first->token == TOK_VARIANT);
    ASSERT((definition.first + 1)->token == TOK_IDENTIFIER);
    ASSERT((definition.first + 2)->token == TOK_COLON);
    ASSERT((definition.first + 3) == definition.second);
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
            ASSERT(body_it->token == TOK_LEFT_PAREN);
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
                ASSERT(type_range.value().first == 0);
                type_tokens.second = body_it + type_range.value().second;
                token_list toks = clone_from_slice(type_tokens);
                std::optional<std::shared_ptr<Type>> type = file_node_ptr->file_namespace->get_type(type_tokens);
                // Check the field type comes from an aliased import file, like `a.b.Type` or `a.Type` for example
                // We cannot just use `collapse_types_in_slice` for data because we are in an early pass where the outer "main" iterator is
                // not allowed to be invalidated, unlike later on phases where all iterators are already line-based (and thus the delete
                // callback is properly executed for re-validating the bounds)
                const bool type_unknown = !type.has_value() || type.value()->get_variation() == Type::Variation::UNKNOWN;
                if (type_range.value().second == 1 && type_tokens.first->token == TOK_IDENTIFIER && type_unknown) {
                    const std::string first_name(type_tokens.first->lexme);
                    const auto result = resolve_alias_in_type(first_name, type_tokens.second, body.back().tokens.second);
                    if (result.extra_tokens > 0) {
                        type_tokens.second = body_it + type_range.value().second + result.extra_tokens;
                        type = result.resolved_type;
                    }
                }
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
                if (!file_node_ptr->file_namespace->add_type(tuple_type)) {
                    tuple_type = file_node_ptr->file_namespace->get_type_from_str(tuple_type->to_string()).value();
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
            ASSERT(type_range.value().first == 0);
            type_tokens.second = body_it + type_range.value().second;
            std::optional<std::shared_ptr<Type>> type = file_node_ptr->file_namespace->get_type(type_tokens);
            // Check the field type comes from an aliased import file, like `a.b.Type` or `a.Type` for example
            // We cannot just use `collapse_types_in_slice` for data because we are in an early pass where the outer "main" iterator is
            // not allowed to be invalidated, unlike later on phases where all iterators are already line-based (and thus the delete
            // callback is properly executed for re-validating the bounds)
            const bool type_unknown = !type.has_value() || type.value()->get_variation() == Type::Variation::UNKNOWN;
            if (type_range.value().second == 1 && type_tokens.first->token == TOK_IDENTIFIER && type_unknown) {
                const std::string first_name(type_tokens.first->lexme);
                const auto result = resolve_alias_in_type(first_name, type_tokens.second, body.back().tokens.second);
                if (result.extra_tokens > 0) {
                    type_tokens.second = body_it + type_range.value().second + result.extra_tokens;
                    type = result.resolved_type;
                }
            }
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

    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    return VariantNode(file_hash, line, column, length, name, possible_types);
}

std::optional<TestNode> Parser::create_test(const token_slice &definition) {
    PROFILE_CUMULATIVE("Parser::create_test");
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
    body_scope->add_variable("flint.return_type",
        {
            .type = Type::get_primitive_type("void"),
            .scope_id = 0,
            .scope_segment = 0,
            .is_mutable = false,
            .is_persistent = false,
            .is_fn_param = true,
            .is_pseudo_variable = true,
        });

    // Check if this test already exists within this file
    if (!TestNode::check_test_name(file_name, test_name)) {
        THROW_ERR(ErrTestRedefinition, ERR_PARSING, file_hash, it->line, it->column, test_name);
        return std::nullopt;
    }

    // Dont parse the body yet, it will be parsed as part of the second pass of the parser
    const unsigned int line = definition.first->line;
    const unsigned int column = definition.first->column;
    const unsigned int length = definition.second->column - definition.first->column;
    auto annotations = AnnotationNode::extract_consumable(annotation_queue, TestNode::consumable_annotations);
    return TestNode(file_hash, line, column, length, annotations, test_name, body_scope);
}

std::optional<ImportNode> Parser::create_import(const token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::create_import");
    std::variant<Hash, std::vector<std::string>> import_path;
    std::optional<std::string> alias;

    auto iterator = tokens.first;
    if (Matcher::tokens_contain(tokens, Matcher::token(TOK_STR_VALUE))) {
        for (; iterator != tokens.second; ++iterator) {
            if (iterator->token == TOK_STR_VALUE) {
                const size_t path_separator = iterator->lexme.find_last_of('/');
                if (path_separator == std::string::npos) {
                    import_path = Hash(file_hash.path.parent_path() / iterator->lexme);
                } else {
                    const std::string lhs = std::string(iterator->lexme.substr(0, path_separator));
                    const std::string filename = std::string(iterator->lexme.substr(path_separator + 1));

                    // Check if the relative path ever escapes the CWD
                    const std::filesystem::path cwd = std::filesystem::current_path();
                    std::filesystem::path checking_path = file_hash.path.parent_path() / lhs;
                    // Normalize to resolve all ".." components
                    checking_path = checking_path.lexically_normal();

                    // Check if the normalized path is within CWD by getting the relative path
                    auto rel_path = checking_path.lexically_relative(cwd);
                    if (!rel_path.empty()) {
                        std::string rel_str = rel_path.string();
                        // If relative path starts with "..", then checking_path is outside cwd
                        if (rel_str.size() >= 2 && rel_str[0] == '.' && rel_str[1] == '.') {
                            THROW_ERR(ErrImportExitedCWD, ERR_PARSING, file_hash, token_slice{iterator, tokens.second});
                            return std::nullopt;
                        }
                    }

                    const std::filesystem::path file_path = file_hash.path.parent_path() / lhs / filename;
                    import_path = Hash(file_path);
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

    const unsigned int line = tokens.first->line;
    const unsigned int column = tokens.first->column;
    const unsigned int length = tokens.second->column - tokens.first->column;
    return ImportNode(file_hash, line, column, length, import_path, alias);
}

Parser::AliasLookupResult Parser::resolve_alias_in_type( //
    const std::string &first_name,                       //
    token_list::iterator lookahead_it,                   //
    token_list::iterator line_end                        //
) const {
    AliasLookupResult result;
    auto ns = file_node_ptr->file_namespace->get_namespace_from_alias(first_name);
    if (!ns.has_value()) {
        return result;
    }
    std::string full_type_name = first_name;
    unsigned int extra_tokens = 0;
    while (                                         //
        lookahead_it != line_end &&                 //
        lookahead_it->token == TOK_DOT &&           //
        (lookahead_it + 1) != line_end &&           //
        (lookahead_it + 1)->token == TOK_IDENTIFIER //
    ) {
        const std::string type_name((lookahead_it + 1)->lexme);
        full_type_name += "." + type_name;
        if (ns.value() == nullptr) {
            // Namespace alias not resolved yet (Phase 1); continue walking to capture full name
            extra_tokens += 2;
            lookahead_it += 2;
            continue;
        }
        const auto next_ns = ns.value()->get_namespace_from_alias(type_name);
        if (next_ns.has_value()) {
            ns = next_ns.value();
            extra_tokens += 2;
            lookahead_it += 2;
            continue;
        }
        auto imported_type = ns.value()->get_type_from_str(type_name);
        if (imported_type.has_value()) {
            result.resolved_type = imported_type.value();
        }
        extra_tokens += 2;
        break;
    }
    if (extra_tokens > 0) {
        if (result.resolved_type == nullptr) {
            // Create an unknown type if the type was unable to be resolved
            result.resolved_type = std::make_shared<UnknownType>(full_type_name);
            if (!file_node_ptr->file_namespace->add_type(result.resolved_type)) {
                result.resolved_type = file_node_ptr->file_namespace->get_type_from_str(result.resolved_type->to_string()).value();
            }
        }
        result.extra_tokens = extra_tokens;
    }
    return result;
}
