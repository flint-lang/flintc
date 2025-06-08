#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/parser.hpp"

#include "error/error.hpp"
#include "parser/type/tuple_type.hpp"

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
    while (tok_it != definition.second && std::next(tok_it) != definition.second && tok_it->type != TOK_LEFT_PAREN) {
        if (tok_it->type == TOK_ALIGNED) {
            is_aligned = true;
        }
        if (tok_it->type == TOK_CONST) {
            is_const = true;
        }
        if (tok_it->type == TOK_DEF) {
            name = std::next(tok_it)->lexme;
        }
        tok_it++;
    }
    if (tok_it == definition.second) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Check if the name is reserved
    if (name == "_start" || name == "_main") {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // Skip the left paren
    tok_it++;
    // Parse the parameters only if there are any parameters
    if (tok_it->type != TOK_RIGHT_PAREN) {
        // Set the last_param_begin to + 1 to skip the left paren
        token_list::iterator last_param_begin = tok_it;
        while (tok_it != definition.second && std::next(tok_it) != definition.second && tok_it->type != TOK_RIGHT_PAREN) {
            if (std::next(tok_it)->type == TOK_COMMA || std::next(tok_it)->type == TOK_RIGHT_PAREN) {
                // The current token is the parameter type
                const std::string param_name = tok_it->lexme;
                // The type is everything from the last param begin
                token_slice type_tokens = {last_param_begin, tok_it};
                bool is_mutable = false;
                if (type_tokens.first->type == TOK_CONST) {
                    type_tokens.first++;
                } else if (type_tokens.first->type == TOK_MUT) {
                    is_mutable = true;
                    type_tokens.first++;
                }
                const auto param_type = Type::get_type(type_tokens);
                if (!param_type.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                parameters.emplace_back(param_type.value(), param_name, is_mutable);
                last_param_begin = tok_it + 2;
            }
            tok_it++;
        }
        if (tok_it == definition.second) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }
    // Skip the right paren
    tok_it++;

    // Now the token should be an arrow, if not there are no return values
    if (tok_it->type == TOK_ARROW) {
        tok_it++;
        if (tok_it == definition.second) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (tok_it->type != TOK_LEFT_PAREN) {
            // There is only a single return type, so everything until the colon is considere the return type
            token_list::iterator begin_it = tok_it;
            while (tok_it != definition.second && tok_it->type != TOK_COLON) {
                tok_it++;
            }
            token_slice type_tokens = {begin_it, tok_it};
            const auto return_type = Type::get_type(type_tokens);
            if (!return_type.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            if (dynamic_cast<const TupleType *>(return_type.value().get())) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return_types.emplace_back(return_type.value());
        } else {
            // Skip the left paren
            tok_it++;
            // Parse the return types
            token_list::iterator last_type_begin = tok_it;
            while (tok_it != definition.second && std::next(tok_it) != definition.second && tok_it->type != TOK_RIGHT_PAREN) {
                if (std::next(tok_it)->type == TOK_COMMA || std::next(tok_it)->type == TOK_RIGHT_PAREN) {
                    // The type is everything from the last param begin
                    token_slice type_tokens = {last_type_begin, tok_it + 1};
                    const auto return_type = Type::get_type(type_tokens);
                    if (!return_type.has_value()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    return_types.emplace_back(return_type.value());
                    last_type_begin = tok_it + 2;
                }
                tok_it++;
            }
        }
    }

    // If its the main function, change its name
    if (name == "main") {
        if (main_function_parsed) {
            // Redefinition of the main function
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        main_function_parsed = true;
        name = "_main";

        // The parameter list either has to be empty or contain one `str[]` parameter
        main_function_has_args = !parameters.empty();
        if (parameters.size() > 1) {
            // Too many parameters for the main function
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        } else if (parameters.size() == 1 && std::get<0>(parameters.front())->to_string() != "str[]") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }

        // The main funcition is not allowed to return anything
        if (!return_types.empty()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

    // Create the body scope
    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>();

    // Add the parameters to the list of variables
    for (const auto &param : parameters) {
        if (!body_scope->add_variable(std::get<1>(param), std::get<0>(param), body_scope->scope_id, std::get<2>(param), true)) {
            // Variable already exists in the func definition list
            THROW_ERR(ErrVarFromRequiresList, ERR_PARSING, file_name, 0, 0, std::get<1>(param));
            return std::nullopt;
        }
    }

    // Dont parse the body yet, it will be parsed in the second pass of the parser
    return FunctionNode(is_aligned, is_const, name, parameters, return_types, body_scope);
}

std::optional<DataNode> Parser::create_data(const token_slice &definition, const token_slice &body) {
    bool is_shared = false;
    bool is_immutable = false;
    bool is_aligned = false;
    std::string name;
    token_slice body_mut = body;

    std::unordered_map<std::string, std::pair<std::shared_ptr<Type>, std::optional<std::string>>> fields;
    std::vector<std::string> order;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->type == TOK_SHARED) {
            is_shared = true;
        }
        if (def_it->type == TOK_IMMUTABLE) {
            is_immutable = true;
            // immutable data is shared by default
            is_shared = true;
        }
        if (def_it->type == TOK_ALIGNED) {
            is_aligned = true;
        }
        if (def_it->type == TOK_DATA) {
            name = (def_it + 1)->lexme;
        }
    }
    remove_leading_garbage(body_mut);
    auto body_it = body_mut.first;
    for (; body_it != body_mut.second; ++body_it) {
        if (Matcher::tokens_match({body_mut.first, body_it}, Matcher::type) && body_it->type == TOK_IDENTIFIER) {
            if (fields.find(body_it->lexme) != fields.end()) {
                // Field name duplication
                THROW_ERR(ErrDefDataDuplicateFieldName, ERR_PARSING, file_name, body_it->line, body_it->column, body_it->lexme);
                return std::nullopt;
            }
            // The type is everything from body_mut.first to next_it
            token_slice type_tokens = {body_mut.first, body_it};
            remove_leading_garbage(type_tokens);
            std::optional<std::shared_ptr<Type>> type = Type::get_type(type_tokens);
            if (!type.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            if ((body_it + 1)->type == TOK_EQUAL) {
                fields[body_it->lexme] = {type.value(), (body_it + 2)->lexme};
                body_it += 3;
            } else {
                fields[body_it->lexme] = {type.value(), std::nullopt};
                body_it++;
            }
            if (body_it->type != TOK_SEMICOLON) {
                // Missing semicolon
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            body_mut.first = body_it + 1;
            remove_leading_garbage(body_mut);
            body_it = body_mut.first;
        }

        if (body_it->type == TOK_IDENTIFIER && (body_it + 1)->type == TOK_LEFT_PAREN) {
            break;
        }
    }
    // Check if the initializer name is correct
    if (body_it->lexme != name) {
        THROW_ERR(ErrDefDataWrongConstructorName, ERR_PARSING, //
            file_name, body_it->line, body_it->column,         //
            name, body_it->lexme                               //
        );
        return std::nullopt;
    }
    ++body_it;

    // Skip the left paren
    ++body_it;

    for (; body_it != body.second; ++body_it) {
        if (body_it->type == TOK_IDENTIFIER) {
            if (std::find(order.begin(), order.end(), body_it->lexme) != order.end()) {
                // The same field name is written down twice in the initializer
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            order.emplace_back(body_it->lexme);
        } else if (body_it->type == TOK_RIGHT_PAREN) {
            break;
        } else if (body_it->type != TOK_COMMA) {
            // Not allowed token in data initializer
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

    std::vector<std::tuple<std::string, std::shared_ptr<Type>, std::optional<std::string>>> ordered_fields;
    ordered_fields.resize(fields.size());
    for (const auto &field : fields) {
        const size_t field_id = std::distance(order.begin(), std::find(order.begin(), order.end(), field.first));
        ordered_fields[field_id] = {field.first, field.second.first, field.second.second};
    }

    return DataNode(is_shared, is_immutable, is_aligned, name, ordered_fields);
}

std::optional<FuncNode> Parser::create_func(const token_slice &definition, const token_slice &body) {
    std::string name;
    std::vector<std::pair<std::string, std::string>> required_data;
    std::vector<std::unique_ptr<FunctionNode>> functions;

    bool requires_data = false;
    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->type == TOK_FUNC && (def_it + 1)->type == TOK_IDENTIFIER) {
            name = (def_it + 1)->lexme;
        }
        if (def_it->type == TOK_REQUIRES) {
            requires_data = true;
        }
        if (requires_data && def_it->type == TOK_IDENTIFIER && (def_it + 1)->type == TOK_IDENTIFIER) {
            required_data.emplace_back(def_it->lexme, (def_it + 1)->lexme);
        }
    }

    int current_line = -1;
    FuncNode func_node = FuncNode(name, required_data, {});
    for (auto body_it = body.first; body_it != body.second; ++body_it) {
        if (current_line == static_cast<int>(body_it->line)) {
            continue;
        }
        current_line = body_it->line;

        uint2 definition_ids = Matcher::get_tokens_line_range(body, current_line).value();
        token_slice function_definition = {body.first + definition_ids.first, body.first + definition_ids.second};

        unsigned int leading_indents = Matcher::get_leading_indents(function_definition, current_line).value();
        token_slice function_body = get_body_tokens(leading_indents, body);

        std::optional<FunctionNode> function = create_function(function_definition);
        if (!function.has_value()) {
            THROW_ERR(ErrDefFunctionCreation, ERR_PARSING, file_name, function_definition);
            return std::nullopt;
        }
        functions.emplace_back(std::make_unique<FunctionNode>(std::move(function.value())));
        add_open_function({functions.back().get(), clone_from_slice(function_body)});
    }
    func_node.functions = std::move(functions);
    return func_node;
}

Parser::create_entity_type Parser::create_entity(const token_slice &definition, const token_slice &body) {
    bool is_modular = Matcher::tokens_match(body, Matcher::entity_body);
    std::string name;
    std::vector<std::string> data_modules;
    std::vector<std::string> func_modules;
    std::vector<std::unique_ptr<LinkNode>> link_nodes;
    std::vector<std::pair<std::string, std::string>> parent_entities;
    std::vector<std::string> constructor_order;
    std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>> monolithic_nodes = std::nullopt;

    bool extract_parents = false;
    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->type == TOK_ENTITY && std::next(def_it)->type == TOK_IDENTIFIER) {
            name = std::next(def_it)->lexme;
        }
        if (def_it->type == TOK_LEFT_PAREN && std::next(def_it)->type == TOK_IDENTIFIER) {
            extract_parents = true;
            ++def_it;
        }
        if (extract_parents && def_it->type == TOK_IDENTIFIER && std::next(def_it)->type == TOK_IDENTIFIER) {
            parent_entities.emplace_back(def_it->lexme, std::next(def_it)->lexme);
        }
    }

    if (is_modular) {
        bool extracting_data = false;
        bool extracting_func = false;
        for (auto body_it = body.first; body_it != body.second; ++body_it) {
            if (body_it->type == TOK_DATA) {
                extracting_data = true;
            } else if (body_it->type == TOK_FUNC) {
                extracting_func = true;
            } else if (body_it->type == TOK_LINK) {
                unsigned int link_indentation = Matcher::get_leading_indents(body, body_it->line).value();
                token_slice tokens_after_link = {body_it + 2, body.second};
                token_slice link_tokens = get_body_tokens(link_indentation, tokens_after_link);
                link_nodes = create_links(link_tokens);
            }

            if (extracting_data) {
                if (body_it->type == TOK_IDENTIFIER) {
                    data_modules.emplace_back(body_it->lexme);
                    if ((body_it + 1)->type == TOK_SEMICOLON) {
                        extracting_data = false;
                    }
                }
            } else if (extracting_func) {
                if (body_it->type == TOK_IDENTIFIER) {
                    func_modules.emplace_back(body_it->lexme);
                    if ((body_it + 1)->type == TOK_SEMICOLON) {
                        extracting_func = false;
                    }
                }
            }
        }
    } else {
        DataNode data_node;
        FuncNode func_node;
        for (auto body_it = body.first; body_it != body.second; ++body_it) {
            if (body_it->type == TOK_DATA) {
                // TODO: Add a generic constructor for the data module
                unsigned int leading_indents = Matcher::get_leading_indents(body, body_it->line).value();
                token_slice data_body = get_body_tokens(leading_indents, body);
                token_list data_definition = {TokenContext{TOK_DATA, "", 0, 0}, TokenContext{TOK_IDENTIFIER, name + "__D", 0, 0}};
                data_node = create_data({data_definition.begin(), data_definition.end()}, data_body).value();
                data_modules.emplace_back(name + "__D");
            } else if (body_it->type == TOK_FUNC) {
                unsigned int leading_indents = Matcher::get_leading_indents(body, body_it->line).value();
                token_slice func_body = get_body_tokens(leading_indents, body);
                token_list func_definition = {TokenContext{TOK_FUNC, "", 0, 0}, TokenContext{TOK_IDENTIFIER, name + "__F", 0, 0},
                    TokenContext{TOK_REQUIRES, "", 0, 0}, TokenContext{TOK_LEFT_PAREN, "", 0, 0},
                    TokenContext{TOK_IDENTIFIER, name + "__D", 0, 0}, TokenContext{TOK_IDENTIFIER, "d", 0, 0},
                    TokenContext{TOK_RIGHT_PAREN, "", 0, 0}, TokenContext{TOK_COLON, "", 0, 0}};
                token_slice func_def_slice = {func_definition.begin(), func_definition.end()};
                // TODO: change the functions accesses to the data by placing "d." in front of every variable access.
                std::optional<FuncNode> created_func = create_func(func_def_slice, func_body);
                if (!created_func.has_value()) {
                    THROW_ERR(ErrDefFuncCreation, ERR_PARSING, file_name, func_def_slice);
                    return {};
                }
                func_node = std::move(created_func.value());
                func_modules.emplace_back(name + "__F");
            }
        }
    }

    uint2 constructor_token_ids = Matcher::get_match_ranges(body, Matcher::entity_body_constructor).at(0);
    for (auto it = body.first + constructor_token_ids.first; it != body.first + constructor_token_ids.second; it++) {
        if (it->type == TOK_IDENTIFIER) {
            if (std::next(it)->type == TOK_LEFT_PAREN && it->lexme != name) {
                THROW_ERR(ErrDefEntityWrongConstructorName, ERR_PARSING, file_name, //
                    it->line, it->column, name, it->lexme                           //
                );
            }
            constructor_order.emplace_back(it->lexme);
        }
    }
    EntityNode entity(name, data_modules, func_modules, std::move(link_nodes), parent_entities, constructor_order);
    std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>> return_value =
        std::make_pair(std::move(entity), std::move(monolithic_nodes));
    return return_value;
}

std::vector<std::unique_ptr<LinkNode>> Parser::create_links(const token_slice &body) {
    std::vector<std::unique_ptr<LinkNode>> links;

    std::vector<uint2> link_matches = Matcher::get_match_ranges(body, Matcher::entity_body_link);
    links.reserve(link_matches.size());
    for (size_t i = 0; i < link_matches.size(); i++) {
        links.emplace_back(std::make_unique<LinkNode>(create_link(body)));
    }

    return links;
}

LinkNode Parser::create_link(const token_slice &tokens) {
    std::vector<std::string> from_references;
    std::vector<std::string> to_references;

    std::vector<uint2> references = Matcher::get_match_ranges(tokens, Matcher::reference);

    for (unsigned int i = references.at(0).first; i < references.at(0).second; i++) {
        if ((tokens.first + i)->type == TOK_IDENTIFIER) {
            from_references.emplace_back((tokens.first + i)->lexme);
        }
    }
    for (unsigned int i = references.at(1).first; i < references.at(1).second; i++) {
        if ((tokens.first + i)->type == TOK_IDENTIFIER) {
            to_references.emplace_back((tokens.first + i)->lexme);
        }
    }

    return LinkNode(from_references, to_references);
}

std::optional<EnumNode> Parser::create_enum(const token_slice &definition, const token_slice &body) {
    std::string name;
    std::vector<std::string> values;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->type == TOK_ENUM && (def_it + 1)->type == TOK_IDENTIFIER) {
            name = (def_it + 1)->lexme;
            break;
        }
    }

    for (auto body_it = body.first; body_it != body.second; ++body_it) {
        if (body_it->type == TOK_IDENTIFIER) {
            if ((body_it + 1)->type == TOK_COMMA) {
                values.emplace_back(body_it->lexme);
            } else if ((body_it + 1)->type == TOK_SEMICOLON) {
                values.emplace_back(body_it->lexme);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name, body_it->line, body_it->column, //
                    expected, body_it->type);
                return std::nullopt;
            }
        }
    }

    return EnumNode(name, values);
}

ErrorNode Parser::create_error(const token_slice &definition, const token_slice &body) {
    std::string name;
    std::string parent_error;
    std::vector<std::string> error_types;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->type == TOK_ERROR && (def_it + 1)->type == TOK_IDENTIFIER) {
            name = (def_it + 1)->lexme;
        }
        if (def_it->type == TOK_LEFT_PAREN) {
            if ((def_it + 1)->type == TOK_IDENTIFIER && (def_it + 2)->type == TOK_RIGHT_PAREN) {
                parent_error = (def_it + 1)->lexme;
                break;
            }
            THROW_ERR(ErrDefErrOnlyOneParent, ERR_PARSING, file_name, definition);
        }
    }

    for (auto body_it = body.first; body_it != body.second; ++body_it) {
        if (body_it->type == TOK_IDENTIFIER) {
            if ((body_it + 1)->type == TOK_COMMA) {
                error_types.emplace_back(body_it->lexme);
            } else if ((body_it + 1)->type == TOK_SEMICOLON) {
                error_types.emplace_back(body_it->lexme);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name, body_it->line, body_it->column, //
                    expected, body_it->type);
            }
        };
    }

    return ErrorNode(name, parent_error, error_types);
}

VariantNode Parser::create_variant(const token_slice &definition, const token_slice &body) {
    std::string name;
    std::vector<std::string> possible_types;

    for (auto def_it = definition.first; def_it != definition.second; ++def_it) {
        if (def_it->type == TOK_VARIANT && (def_it + 1)->type == TOK_IDENTIFIER) {
            name = (def_it + 1)->lexme;
            break;
        }
    }

    for (auto body_it = body.first; body_it != body.second; ++body_it) {
        if (body_it->type == TOK_IDENTIFIER) {
            if ((body_it + 1)->type == TOK_COMMA) {
                possible_types.emplace_back(body_it->lexme);
            } else if ((body_it + 1)->type == TOK_SEMICOLON) {
                possible_types.emplace_back(body_it->lexme);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name, body_it->line, body_it->column, //
                    expected, body_it->type);
            }
        }
    }

    return VariantNode(name, possible_types);
}

std::optional<TestNode> Parser::create_test(const token_slice &definition) {
    std::string test_name;
    // Extract the name of the test
    for (auto it = definition.first; it != definition.second; ++it) {
        if (it->type == TOK_TEST && std::next(it) != definition.second && std::next(it)->type == TOK_STR_VALUE) {
            test_name = std::next(it)->lexme;
        }
    }
    if (test_name == "") {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Create the body scope
    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>();

    // Check if this test already exists within this file
    if (!TestNode::check_test_name(file_name, test_name)) {
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
            if (iterator->type == TOK_STR_VALUE) {
                const size_t path_separator = iterator->lexme.find_last_of('/');
                if (path_separator == std::string::npos) {
                    import_path = std::make_pair(std::nullopt, iterator->lexme.substr(0, iterator->lexme.length()));
                } else {
                    import_path = std::make_pair(                                                                 //
                        iterator->lexme.substr(0, path_separator),                                                //
                        iterator->lexme.substr(path_separator + 1, iterator->lexme.length() - path_separator - 1) //
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
            if ((tokens.first + i)->type == TOK_IDENTIFIER) {
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
    if (iterator->type == TOK_AS) {
        ++iterator;
        if (iterator == tokens.second) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (iterator->type != TOK_IDENTIFIER) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        alias = iterator->lexme;
    }

    return ImportNode(import_path, alias);
}
