#include "parser/parser.hpp"

#include "error/error.hpp"
#include "parser/signature.hpp"

#include <algorithm>
#include <optional>

std::optional<FunctionNode> Parser::create_function(const token_list &definition) {
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<std::string> return_types;
    bool is_aligned = false;
    bool is_const = false;

    bool begin_params = false;
    bool begin_returns = false;
    auto tok_iterator = definition.begin();
    while (tok_iterator != definition.end()) {
        if (tok_iterator->type == TOK_ALIGNED) {
            is_aligned = true;
        }
        if (tok_iterator->type == TOK_CONST && name.empty()) {
            is_const = true;
        }
        // Finding the function name
        if (tok_iterator->type == TOK_DEF) {
            name = (tok_iterator + 1)->lexme;
        }
        // Adding the functions parameters
        if (tok_iterator->type == TOK_LEFT_PAREN && !begin_returns) {
            begin_params = true;
        }
        if (tok_iterator->type == TOK_RIGHT_PAREN && begin_params) {
            begin_params = false;
        }
        if (begin_params && Signature::tokens_match({TokenContext{tok_iterator->type, "", 0, 0}}, ESignature::TYPE) &&
            (tok_iterator + 1)->type == TOK_IDENTIFIER) {
            parameters.emplace_back(tok_iterator->lexme, (tok_iterator + 1)->lexme);
        }
        // Adding the functions return types
        if (tok_iterator->type == TOK_ARROW) {
            // Only one return type
            if (Signature::tokens_match({{(tok_iterator + 1)->type, "", 0, 0}}, ESignature::TYPE)) {
                return_types.emplace_back((tok_iterator + 1)->lexme);
                break;
            }
            begin_returns = true;
        }
        if (begin_returns && Signature::tokens_match({{tok_iterator->type, "", 0, 0}}, ESignature::TYPE)) {
            return_types.emplace_back(tok_iterator->lexme);
        }
        if (begin_returns && tok_iterator->type == TOK_RIGHT_PAREN) {
            break;
        }
        ++tok_iterator;
    }

    // Create the body scope
    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>();

    // Add the parameters to the list of variables
    for (const auto &param : parameters) {
        if (!body_scope->add_variable_type(param.second, param.first, body_scope->scope_id)) {
            // Variable already exists in the func definition list
            THROW_ERR(ErrVarFromRequiresList, ERR_PARSING, file_name, 0, 0, param.second);
            return std::nullopt;
        }
    }

    // Dont parse the body yet, it will be parsed in the second pass of the parser
    return FunctionNode(is_aligned, is_const, name, parameters, return_types, body_scope);
}

std::optional<DataNode> Parser::create_data(const token_list &definition, const token_list &body) {
    bool is_shared = false;
    bool is_immutable = false;
    bool is_aligned = false;
    std::string name;

    std::unordered_map<std::string, std::pair<std::string, std::optional<std::string>>> fields;
    std::vector<std::string> order;

    auto definition_iterator = definition.begin();
    while (definition_iterator != definition.end()) {
        if (definition_iterator->type == TOK_SHARED) {
            is_shared = true;
        }
        if (definition_iterator->type == TOK_IMMUTABLE) {
            is_immutable = true;
            // immutable data is shared by default
            is_shared = true;
        }
        if (definition_iterator->type == TOK_ALIGNED) {
            is_aligned = true;
        }
        if (definition_iterator->type == TOK_DATA) {
            name = (definition_iterator + 1)->lexme;
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    bool parsing_constructor = false;
    while (body_iterator != body.end()) {
        if (Signature::tokens_match({TokenContext{body_iterator->type, "", 0, 0}}, ESignature::TYPE) &&
            (body_iterator + 1)->type == TOK_IDENTIFIER) {
            if (fields.find((body_iterator + 1)->lexme) != fields.end()) {
                // Field name duplication
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            fields[(body_iterator + 1)->lexme] = {body_iterator->lexme, std::nullopt};
            if ((body_iterator + 2)->type == TOK_EQUAL) {
                fields[(body_iterator + 1)->lexme].second = (body_iterator + 3)->lexme;
            }
        }

        if (body_iterator->type == TOK_IDENTIFIER && (body_iterator + 1)->type == TOK_LEFT_PAREN) {
            if (body_iterator->lexme != name) {
                THROW_ERR(ErrDefDataWrongConstructorName, ERR_PARSING,     //
                    file_name, body_iterator->line, body_iterator->column, //
                    name, body_iterator->lexme                             //
                );
                return std::nullopt;
            }
            parsing_constructor = true;
            ++body_iterator;
        }
        if (parsing_constructor && body_iterator->type == TOK_IDENTIFIER) {
            order.emplace_back(body_iterator->lexme);
        }
        if (body_iterator->type == TOK_RIGHT_PAREN) {
            break;
        }

        ++body_iterator;
    }

    return DataNode(is_shared, is_immutable, is_aligned, name, fields, order);
}

std::optional<FuncNode> Parser::create_func(const token_list &definition, token_list &body) {
    std::string name;
    std::vector<std::pair<std::string, std::string>> required_data;
    std::vector<std::unique_ptr<FunctionNode>> functions;

    auto definition_iterator = definition.begin();
    bool requires_data = false;
    while (definition_iterator != definition.end()) {
        if (definition_iterator->type == TOK_FUNC && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
        }
        if (definition_iterator->type == TOK_REQUIRES) {
            requires_data = true;
        }
        if (requires_data && definition_iterator->type == TOK_IDENTIFIER && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            required_data.emplace_back(definition_iterator->lexme, (definition_iterator + 1)->lexme);
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    int current_line = -1;
    FuncNode func_node = FuncNode(name, required_data, {});
    while (body_iterator != body.end()) {
        if (current_line == static_cast<int>(body_iterator->line)) {
            ++body_iterator;
            continue;
        }
        current_line = body_iterator->line;

        uint2 definition_ids = Signature::get_tokens_line_range(body, current_line).value();
        token_list function_definition = extract_from_to(definition_ids.first, definition_ids.second, body);

        unsigned int leading_indents = Signature::get_leading_indents(function_definition, current_line).value();
        token_list function_body = get_body_tokens(leading_indents, body);

        std::optional<FunctionNode> function = create_function(function_definition);
        if (!function.has_value()) {
            THROW_ERR(ErrDefFunctionCreation, ERR_PARSING, file_name, function_definition);
            return std::nullopt;
        }
        functions.emplace_back(std::make_unique<FunctionNode>(std::move(function.value())));
        add_open_function({functions.back().get(), function_body});
        ++body_iterator;
    }
    func_node.functions = std::move(functions);
    return func_node;
}

Parser::create_entity_type Parser::create_entity(const token_list &definition, token_list &body) {
    bool is_modular = Signature::tokens_match(body, ESignature::ENTITY_BODY);
    std::string name;
    std::vector<std::string> data_modules;
    std::vector<std::string> func_modules;
    std::vector<std::unique_ptr<LinkNode>> link_nodes;
    std::vector<std::pair<std::string, std::string>> parent_entities;
    std::vector<std::string> constructor_order;
    std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>> monolithic_nodes = std::nullopt;

    auto definition_iterator = definition.begin();
    bool extract_parents = false;
    while (definition_iterator != definition.end()) {
        if (definition_iterator->type == TOK_ENTITY && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
        }
        if (definition_iterator->type == TOK_LEFT_PAREN && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            extract_parents = true;
            ++definition_iterator;
        }
        if (extract_parents && definition_iterator->type == TOK_IDENTIFIER && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            parent_entities.emplace_back(definition_iterator->lexme, (definition_iterator + 1)->lexme);
        }
        ++definition_iterator;
    }

    if (is_modular) {
        bool extracting_data = false;
        bool extracting_func = false;
        auto body_iterator = body.begin();
        while (body_iterator != body.end()) {
            if (body_iterator->type == TOK_DATA) {
                extracting_data = true;
            } else if (body_iterator->type == TOK_FUNC) {
                extracting_func = true;
            } else if (body_iterator->type == TOK_LINK) {
                unsigned int link_indentation = Signature::get_leading_indents(body, body_iterator->line).value();
                // copy all tokens from the body after the link declaration
                token_list tokens_after_link;
                tokens_after_link.reserve(body.size() - std::distance(body.begin(), body_iterator + 2));
                std::copy(body_iterator + 2, body.end(), tokens_after_link.begin());
                token_list link_tokens = get_body_tokens(link_indentation, tokens_after_link);
                link_nodes = create_links(link_tokens);
            }

            if (extracting_data) {
                if (body_iterator->type == TOK_IDENTIFIER) {
                    data_modules.emplace_back(body_iterator->lexme);
                    if ((body_iterator + 1)->type == TOK_SEMICOLON) {
                        extracting_data = false;
                    }
                }
            } else if (extracting_func) {
                if (body_iterator->type == TOK_IDENTIFIER) {
                    func_modules.emplace_back(body_iterator->lexme);
                    if ((body_iterator + 1)->type == TOK_SEMICOLON) {
                        extracting_func = false;
                    }
                }
            }
            ++body_iterator;
        }
    } else {
        DataNode data_node;
        FuncNode func_node;
        auto body_iterator = body.begin();
        while (body_iterator != body.end()) {
            if (body_iterator->type == TOK_DATA) {
                // TODO: Add a generic constructor for the data module
                unsigned int leading_indents = Signature::get_leading_indents(body, body_iterator->line).value();
                token_list data_body = get_body_tokens(leading_indents, body);
                token_list data_definition = {TokenContext{TOK_DATA, "", 0, 0}, TokenContext{TOK_IDENTIFIER, name + "__D", 0, 0}};
                data_node = create_data(data_definition, data_body).value();
                data_modules.emplace_back(name + "__D");
            } else if (body_iterator->type == TOK_FUNC) {
                unsigned int leading_indents = Signature::get_leading_indents(body, body_iterator->line).value();
                token_list func_body = get_body_tokens(leading_indents, body);
                token_list func_definition = {TokenContext{TOK_FUNC, "", 0, 0}, TokenContext{TOK_IDENTIFIER, name + "__F", 0, 0},
                    TokenContext{TOK_REQUIRES, "", 0, 0}, TokenContext{TOK_LEFT_PAREN, "", 0, 0},
                    TokenContext{TOK_IDENTIFIER, name + "__D", 0, 0}, TokenContext{TOK_IDENTIFIER, "d", 0, 0},
                    TokenContext{TOK_RIGHT_PAREN, "", 0, 0}, TokenContext{TOK_COLON, "", 0, 0}};
                // TODO: change the functions accesses to the data by placing "d." in front of every variable access.
                std::optional<FuncNode> created_func = create_func(func_definition, func_body);
                if (!created_func.has_value()) {
                    THROW_ERR(ErrDefFuncCreation, ERR_PARSING, file_name, func_definition);
                    return {};
                }
                func_node = std::move(created_func.value());
                func_modules.emplace_back(name + "__F");
            }
            ++body_iterator;
        }
    }

    uint2 constructor_token_ids = Signature::get_match_ranges(body, ESignature::ENTITY_BODY_CONSTRUCTOR).at(0);
    for (auto it = body.begin() + constructor_token_ids.first; it != body.begin() + constructor_token_ids.second; it++) {
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

std::vector<std::unique_ptr<LinkNode>> Parser::create_links(token_list &body) {
    std::vector<std::unique_ptr<LinkNode>> links;

    std::vector<uint2> link_matches = Signature::get_match_ranges(body, ESignature::ENTITY_BODY_LINK);
    links.reserve(link_matches.size());
    for (size_t i = 0; i < link_matches.size(); i++) {
        links.emplace_back(std::make_unique<LinkNode>(create_link(body)));
    }

    return links;
}

LinkNode Parser::create_link(const token_list &tokens) {
    std::vector<std::string> from_references;
    std::vector<std::string> to_references;

    std::vector<uint2> references = Signature::get_match_ranges(tokens, ESignature::REFERENCE);

    for (unsigned int i = references.at(0).first; i < references.at(0).second; i++) {
        if (tokens.at(i).type == TOK_IDENTIFIER) {
            from_references.emplace_back(tokens.at(i).lexme);
        }
    }
    for (unsigned int i = references.at(1).first; i < references.at(1).second; i++) {
        if (tokens.at(i).type == TOK_IDENTIFIER) {
            to_references.emplace_back(tokens.at(i).lexme);
        }
    }

    return LinkNode(from_references, to_references);
}

EnumNode Parser::create_enum(const token_list &definition, const token_list &body) {
    std::string name;
    std::vector<std::string> values;

    auto definition_iterator = definition.begin();
    while (definition_iterator != definition.end()) {
        if (definition_iterator->type == TOK_ENUM && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
            break;
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    while (body_iterator != body.end()) {
        if (body_iterator->type == TOK_IDENTIFIER) {
            if ((body_iterator + 1)->type == TOK_COMMA) {
                values.emplace_back(body_iterator->lexme);
            } else if ((body_iterator + 1)->type == TOK_SEMICOLON) {
                values.emplace_back(body_iterator->lexme);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name, body_iterator->line, body_iterator->column, //
                    expected, body_iterator->type);
            }
        }
        ++body_iterator;
    }

    return EnumNode(name, values);
}

ErrorNode Parser::create_error(const token_list &definition, const token_list &body) {
    std::string name;
    std::string parent_error;
    std::vector<std::string> error_types;

    auto definition_iterator = definition.begin();
    while (definition_iterator != definition.end()) {
        if (definition_iterator->type == TOK_ERROR && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
        }
        if (definition_iterator->type == TOK_LEFT_PAREN) {
            if ((definition_iterator + 1)->type == TOK_IDENTIFIER && (definition_iterator + 2)->type == TOK_RIGHT_PAREN) {
                parent_error = (definition_iterator + 1)->lexme;
                break;
            }
            THROW_ERR(ErrDefErrOnlyOneParent, ERR_PARSING, file_name, definition);
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    while (body_iterator != body.end()) {
        if (body_iterator->type == TOK_IDENTIFIER) {
            if ((body_iterator + 1)->type == TOK_COMMA) {
                error_types.emplace_back(body_iterator->lexme);
            } else if ((body_iterator + 1)->type == TOK_SEMICOLON) {
                error_types.emplace_back(body_iterator->lexme);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name, body_iterator->line, body_iterator->column, //
                    expected, body_iterator->type);
            }
        }
        ++body_iterator;
    }

    return ErrorNode(name, parent_error, error_types);
}

VariantNode Parser::create_variant(const token_list &definition, const token_list &body) {
    std::string name;
    std::vector<std::string> possible_types;

    auto definition_iterator = definition.begin();
    while (definition_iterator != definition.end()) {
        if (definition_iterator->type == TOK_VARIANT && (definition_iterator + 1)->type == TOK_IDENTIFIER) {
            name = (definition_iterator + 1)->lexme;
            break;
        }
        ++definition_iterator;
    }

    auto body_iterator = body.begin();
    while (body_iterator != body.end()) {
        if (body_iterator->type == TOK_IDENTIFIER) {
            if ((body_iterator + 1)->type == TOK_COMMA) {
                possible_types.emplace_back(body_iterator->lexme);
            } else if ((body_iterator + 1)->type == TOK_SEMICOLON) {
                possible_types.emplace_back(body_iterator->lexme);
                break;
            } else {
                const std::vector<Token> expected = {TOK_COMMA, TOK_SEMICOLON};
                THROW_ERR(ErrParsUnexpectedToken, ERR_PARSING, file_name, body_iterator->line, body_iterator->column, //
                    expected, body_iterator->type);
            }
        }
        ++body_iterator;
    }

    return VariantNode(name, possible_types);
}

std::optional<TestNode> Parser::create_test(const token_list &definition) {
    std::string test_name;
    // Extract the name of the test
    for (auto it = definition.begin(); it != definition.end(); ++it) {
        if (it->type == TOK_TEST && std::next(it) != definition.end() && std::next(it)->type == TOK_STR_VALUE) {
            test_name = std::next(it)->lexme;
        }
    }
    if (test_name == "") {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Create the body scope
    std::unique_ptr<Scope> body_scope = std::make_unique<Scope>();

    // Dont parse the body yet, it will be parsed as part of the second pass of the parser
    return TestNode(file_name, test_name, body_scope);
}

ImportNode Parser::create_import(const token_list &tokens) {
    std::variant<std::string, std::vector<std::string>> import_path;

    if (Signature::tokens_contain(tokens, TOK_STR_VALUE)) {
        for (const auto &tok : tokens) {
            if (tok.type == TOK_STR_VALUE) {
                import_path = tok.lexme;
                break;
            }
        }
    } else {
        const std::string reference = Signature::get_regex_string(                             //
            {"((", TOK_FLINT, ")|(", TOK_IDENTIFIER, "))", "(", TOK_DOT, TOK_IDENTIFIER, ")*"} //
        );
        const auto matches = Signature::get_match_ranges(tokens, reference).at(0);
        std::vector<std::string> path;
        if (tokens.at(matches.first).type == TOK_FLINT) {
            path.emplace_back("flint");
        }
        for (unsigned int i = matches.first; i < matches.second; i++) {
            if (tokens.at(i).type == TOK_IDENTIFIER) {
                path.emplace_back(tokens.at(i).lexme);
            }
        }
        import_path = path;
    }

    return ImportNode(import_path);
}
