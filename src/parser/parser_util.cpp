#include "lexer/builtins.hpp"
#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include <algorithm>

bool Parser::add_next_main_node(FileNode &file_node, token_slice &tokens) {
    token_slice definition_tokens = get_definition_tokens(tokens);
    tokens.first = definition_tokens.second;
    if (std::prev(definition_tokens.second)->token == TOK_EOL) [[likely]] {
        definition_tokens.second--;
    }

    // Find the indentation of the definition
    int definition_indentation = 0;
    for (auto tok = definition_tokens.first; tok != definition_tokens.second; ++tok) {
        if (tok->token == TOK_INDENT) {
            definition_indentation++;
        } else {
            break;
        }
    }

    if (Matcher::tokens_contain(definition_tokens, Matcher::use_statement)) {
        if (definition_indentation > 0) {
            THROW_ERR(ErrUseStatementNotAtTopLevel, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        std::optional<ImportNode> import_node = create_import(definition_tokens);
        if (!import_node.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        for (const auto &imported_file : imported_files) {
            if (imported_file->path == import_node.value().path) {
                // The same use statement was written twice in the same file
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
        }
        // Check if the given alias is already taken
        if (import_node.value().alias.has_value() && aliases.find(import_node.value().alias.value()) != aliases.end()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        if (std::holds_alternative<std::vector<std::string>>(import_node.value().path)) {
            const std::vector<std::string> &import_vec = std::get<std::vector<std::string>>(import_node.value().path);
            if (import_vec.size() == 2 && import_vec.front() == "Core") {
                // Check for imported core modules
                const std::string &module_str = import_vec.back();
                if (module_str != "print" && module_str != "read" && module_str != "assert" && module_str != "filesystem" &&
                    module_str != "env" && module_str != "system") {
                    const auto &tok = definition_tokens.first + 3;
                    THROW_ERR(ErrDefUnexpectedCoreModule, ERR_PARSING, file_name, tok->line, tok->column, module_str);
                    return false;
                }
                if (!create_core_module_types(file_node, module_str)) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
            }
        }
        std::optional<ImportNode *> added_import = file_node.add_import(import_node.value());
        if (!added_import.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        if (added_import.value()->alias.has_value()) {
            aliases[added_import.value()->alias.value()] = added_import.value();
        }
        if (std::holds_alternative<std::pair<std::optional<std::string>, std::string>>(added_import.value()->path) || //
            (std::get<std::vector<std::string>>(added_import.value()->path).size() != 2 &&                            //
                std::get<std::vector<std::string>>(added_import.value()->path).front() != "Core")                     //
        ) {
            imported_files.emplace_back(added_import.value());
        }
        return true;
    }

    std::vector<Line> body_lines = get_body_lines(definition_indentation, tokens);
    if (body_lines.empty()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    if (Matcher::tokens_contain(definition_tokens, Matcher::function_definition)) {
        // Dont actually parse the function body, only its definition
        std::optional<FunctionNode> function_node = create_function(definition_tokens);
        if (!function_node.has_value()) {
            return false;
        }
        FunctionNode *added_function = file_node.add_function(function_node.value());
        add_open_function({added_function, body_lines});
        add_parsed_function(added_function, file_name);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::data_definition)) {
        std::optional<DataNode> data_node = create_data(definition_tokens, body_lines);
        if (!data_node.has_value()) {
            return false;
        }
        DataNode *added_data = file_node.add_data(data_node.value());
        add_parsed_data(added_data, file_name);
        if (!Type::add_type(std::make_shared<DataType>(added_data))) {
            auto it = definition_tokens.first;
            while (it->token != TOK_DATA) {
                ++it;
            }
            // Skip the `data` token so that `it` now points at the name of the data node
            ++it;
            THROW_ERR(ErrDefDataRedefinition, ERR_PARSING, file_name, it->line, it->column, added_data->name);
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::func_definition)) {
        std::optional<FuncNode> func_node = create_func(definition_tokens, body_lines);
        if (!func_node.has_value()) {
            return false;
        }
        file_node.add_func(func_node.value());
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::entity_definition)) {
        create_entity_type entity_creation = create_entity(definition_tokens, body_lines);
        file_node.add_entity(entity_creation.first);
        if (entity_creation.second.has_value()) {
            std::unique_ptr<DataNode> data_node_ptr = std::move(entity_creation.second.value().first);
            std::unique_ptr<FuncNode> func_node_ptr = std::move(entity_creation.second.value().second);
            file_node.add_data(*data_node_ptr);
            file_node.add_func(*func_node_ptr);
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::enum_definition)) {
        std::optional<EnumNode> enum_node = create_enum(definition_tokens, body_lines);
        if (!enum_node.has_value()) {
            return false;
        }
        EnumNode *added_enum = file_node.add_enum(enum_node.value());
        if (!Type::add_type(std::make_shared<EnumType>(added_enum))) {
            // Enum redifinition
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::error_definition)) {
        std::optional<ErrorNode> error_node = create_error(definition_tokens, body_lines);
        if (!error_node.has_value()) {
            return false;
        }
        ErrorNode *added_error = file_node.add_error(error_node.value());
        if (!Type::add_type(std::make_shared<ErrorSetType>(added_error))) {
            // Error Set redefinition or naming collision
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::variant_definition)) {
        std::optional<VariantNode> variant_node = create_variant(definition_tokens, body_lines);
        if (!variant_node.has_value()) {
            return false;
        }
        std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> added_variant = file_node.add_variant(variant_node.value());
        if (!Type::add_type(std::make_shared<VariantType>(added_variant, false))) {
            // Varaint type redefinition
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::test_definition)) {
        std::optional<TestNode> test_node = create_test(definition_tokens);
        if (!test_node.has_value()) {
            return false;
        }
        TestNode *added_test = file_node.add_test(test_node.value());
        add_open_test({added_test, body_lines});
        add_parsed_test(added_test, file_name);
    } else {
        Debug::print_token_context_vector(definition_tokens, file_name);
        THROW_ERR(ErrUnexpectedDefinition, ERR_PARSING, file_name, definition_tokens);
        return false;
    }
    return true;
}

bool Parser::create_core_module_types(FileNode &file_node, const std::string &core_lib_name) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock_guard(mutex);
    static std::unordered_map<std::string_view, bool> types_added = {
        {"read", false},
        {"assert", false},
        {"filesystem", false},
        {"env", false},
        {"system", false},
    };
    auto added = types_added.find(core_lib_name);
    if (added == types_added.end()) {
        return true;
    }
    if (added->second) {
        // Prevent duplicate addition of the types
        return true;
    }
    auto error_sets = core_module_error_sets.find(core_lib_name);
    assert(error_sets != core_module_error_sets.end());
    for (const auto &error_set : error_sets->second) {
        const std::string error_type_name(std::get<0>(error_set));
        const std::string parent_error(std::get<1>(error_set));
        std::vector<std::string> error_values;
        std::vector<std::string> default_messages;
        for (const auto &[error_value, error_message] : std::get<2>(error_set)) {
            error_values.emplace_back(error_value);
            default_messages.emplace_back(error_message);
        }
        ErrorNode error(error_type_name, parent_error, error_values, default_messages);
        ErrorNode *error_node = file_node.add_error(error);
        if (!Type::add_type(std::make_shared<ErrorSetType>(error_node))) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    }
    types_added.at(core_lib_name) = true;
    return true;
}

token_slice Parser::get_definition_tokens(const token_slice &tokens) {
    // Scan through all the tokens and first extract all tokens from this line
    int end_index = 0;
    unsigned int start_line = tokens.first->line;
    for (auto tok = tokens.first; tok != tokens.second; ++tok) {
        if (tok->line == start_line) {
            end_index++;
        } else {
            break;
        }
    }
    return {tokens.first, tokens.first + end_index};
}

std::vector<Line> Parser::get_body_lines(unsigned int definition_indentation, token_slice &tokens) {
    std::vector<Line> body_lines;
    auto current_line_start = tokens.first;
    unsigned int current_indent_lvl = 0;

    for (auto it = tokens.first; it != tokens.second;) {
        if (it->token == TOK_EOL) {
            token_slice current_line = {current_line_start, it};
            body_lines.emplace_back(current_indent_lvl, current_line);
            ++it;
            current_line_start = it;
            current_indent_lvl = 0;
            tokens.first = it;
            continue;
        } else if (it->token == TOK_EOF) {
            tokens.first = tokens.second;
            if (current_line_start == it) {
                break;
            }
            token_slice current_line = {current_line_start, it};
            body_lines.emplace_back(current_indent_lvl, current_line);
            break;
        }
        // Skip all the \t tokens but remember the indentation depth
        if (current_indent_lvl == 0) {
            while (it != tokens.second) {
                if (it->token == TOK_INDENT) {
                    current_indent_lvl++;
                } else {
                    current_line_start = it;
                    break;
                }
                ++it;
            }
            // Check if this line has a lower indentation than the definition, if so the body scope has ended and we return all lines until
            // now
            if (current_indent_lvl <= definition_indentation) {
                tokens.first = it - current_indent_lvl;
                break;
            }
        }
        ++it;
    }

    if (body_lines.empty()) {
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_name, tokens);
        std::exit(EXIT_FAILURE);
    }

    return body_lines;
}

void Parser::collapse_types_in_lines(std::vector<Line> &lines, token_list &source) {
    for (auto line : lines) {
        token_list toks = clone_from_slice(line.tokens);
        for (auto it = line.tokens.first; it != line.tokens.second;) {
            // Erase all indentations within a line, which are not at the beginning of a line
            if (it->token == TOK_INDENT) {
                Line::delete_tokens(source, it, 1);
                continue;
            }
            // Check if the next token will definitely be not the begin of a type, like commas or a lot of other tokens. In that case no
            // expensive matching logic needs to be run, so we can safely skip that token entirely
            if (it->token != TOK_TYPE && it->token != TOK_DATA && it->token != TOK_VARIANT && it->token != TOK_IDENTIFIER) {
                ++it;
                continue;
            }
            // Check if the next chunk is a type definition, if it is we replace all tokens forming the type with a single type token
            if (Matcher::tokens_start_with(token_slice{it, line.tokens.second}, Matcher::type)) {
                // It's a type token
                std::optional<uint2> type_range = Matcher::get_next_match_range(token_slice{it, line.tokens.second}, Matcher::type);
                assert(type_range.has_value());
                assert(type_range.value().first == 0);
                if (type_range.value().second == 1) {
                    // It's a primitive / simple type. Such types definitely need to exist already, so if it does not exists it's a regular
                    // identifier. And if this token is already a type it means its a primitive type, so we can skip it as well
                    if (it->token != TOK_TYPE) {
                        // Types of size 1 always need to be an identifier if they are not already a type (primitives)
                        assert(it->token == TOK_IDENTIFIER);
                        std::optional<std::shared_ptr<Type>> type = Type::get_type_from_str(std::string(it->lexme));
                        if (type.has_value()) {
                            *it = TokenContext(TOK_TYPE, it->line, it->column, it->file_id, type.value());
                        }
                    }
                } else if (it->token != TOK_IDENTIFIER || Type::get_type_from_str(std::string(it->lexme)).has_value()) {
                    // If its a bigger type and it starts with an identifier, the identifier itself must be a known type already. If the
                    // identifier is not a known type, this is an edge case like `i < 5 and x > 2` where `i<5 and x>` is interpreted as
                    // `T<..>`. So, `T` must be a known type in this case, otherwise the whole thing is no type. *or* it has to be a
                    // keyword, like `data` or `variant`. But when it's a keywords it's no identifier annyway.
                    std::optional<std::shared_ptr<Type>> type = Type::get_type(token_slice{it, it + type_range.value().second});
                    if (!type.has_value()) {
                        std::exit(EXIT_FAILURE);
                    }
                    // Change this token to be a type token
                    *it = TokenContext(TOK_TYPE, it->line, it->column, it->file_id, type.value());
                    // Erase all the following type tokens from the tokens list
                    Line::delete_tokens(source, it + 1, type_range.value().second - 1);
                }
            }
            ++it;
        }
    }
}

std::optional<std::tuple<                                          //
    std::string,                                                   // name
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>>, // args
    std::shared_ptr<Type>,                                         // type
    bool,                                                          // is initializer (true) or call (false)
    std::vector<std::shared_ptr<Type>>                             // error types
    >>
Parser::create_call_or_initializer_base(         //
    std::shared_ptr<Scope> scope,                //
    const token_slice &tokens,                   //
    const std::optional<std::string> &alias_base //
) {
    using types = std::vector<std::shared_ptr<Type>>;
    assert(tokens.first->token == TOK_TYPE || tokens.first->token == TOK_IDENTIFIER);
    std::optional<uint2> arg_range = Matcher::balanced_range_extraction(        //
        tokens, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
    );
    if (!arg_range.has_value()) {
        // Function call does not have opening and closing brackets ()
        return std::nullopt;
    }
    // remove the '(' and ')' tokens from the arg_range
    ++arg_range.value().first;
    --arg_range.value().second;

    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments;

    // Arguments are separated by commas. When the arg_range.first == arg_range.second, no arguments are passed
    if (arg_range.value().first < arg_range.value().second) {
        // if the args contain at least one comma, it is known that multiple arguments are passed. If not, only one is
        // passed. But the comma must be present at the top-level and not within one of the balanced range groups
        const auto match_ranges = Matcher::get_match_ranges_in_range_outside_group(                                               //
            tokens, Matcher::token(TOK_COMMA), arg_range.value(), Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
        );
        if (!match_ranges.empty()) {
            for (auto match = match_ranges.begin();; ++match) {
                token_slice argument_tokens;
                if (match == match_ranges.begin()) {
                    argument_tokens = {tokens.first + arg_range.value().first, tokens.first + match->first};
                } else if (match == match_ranges.end()) {
                    argument_tokens = {tokens.first + (match - 1)->second, tokens.first + arg_range.value().second};
                } else {
                    argument_tokens = {tokens.first + (match - 1)->second, tokens.first + match->first};
                }
                auto expression = create_expression(scope, argument_tokens);
                if (!expression.has_value()) {
                    return std::nullopt;
                }
                arguments.emplace_back(std::move(expression.value()), false);
                if (match == match_ranges.end()) {
                    break;
                }
            }
        } else {
            token_slice argument_tokens = {tokens.first + arg_range.value().first, tokens.first + arg_range.value().second};
            auto expression = create_expression(scope, argument_tokens);
            if (!expression.has_value()) {
                return std::nullopt;
            }
            arguments.emplace_back(std::move(expression.value()), false);
        }
    }

    // Get all the argument types
    types argument_types;
    argument_types.reserve(arguments.size());
    for (size_t i = 0; i < arguments.size(); i++) {
        // Typecast all string literals in the args to string variables
        if (arguments[i].first->type->to_string() == "__flint_type_str_lit") {
            arguments[i].first = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), arguments[i].first);
        }
        argument_types.emplace_back(arguments[i].first->type);
    }

    // Check if it's an initializer
    if (tokens.first->token == TOK_TYPE) {
        if (const DataType *data_type = dynamic_cast<const DataType *>(tokens.first->type.get())) {
            DataNode *const data_node = data_type->data_node;
            auto &fields = data_node->fields;
            // Now check if the initializer arguments are equal to the expected initializer fields
            if (fields.size() != arguments.size()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            for (size_t i = 0; i < arguments.size(); i++) {
                std::shared_ptr<Type> arg_type = arguments[i].first->type;
                if (dynamic_cast<const GroupType *>(arg_type.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                const auto &field_type = std::get<1>(fields.at(i));
                if (field_type != arg_type) {
                    if (dynamic_cast<const PrimitiveType *>(arg_type.get())) {
                        const std::string &arg_type_str = arg_type->to_string();
                        if (primitive_implicit_casting_table.find(arg_type_str) == primitive_implicit_casting_table.end()) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        const auto &to_types = primitive_implicit_casting_table.at(arg_type_str);
                        if (std::find(to_types.begin(), to_types.end(), field_type->to_string()) == to_types.end()) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        arguments[i].first = std::make_unique<TypeCastNode>(field_type, arguments[i].first);
                    } else {
                        std::optional<bool> castability = check_castability(field_type, arg_type);
                        if (castability.has_value() && castability.value()) {
                            arguments[i].first = std::make_unique<TypeCastNode>(field_type, arguments[i].first);
                        }
                    }
                }
            }
            return std::make_tuple(data_node->name, std::move(arguments), tokens.first->type, true, types{});
        } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(tokens.first->type.get())) {
            const std::shared_ptr<Type> base_type = multi_type->base_type;
            const unsigned int width = multi_type->width;
            if (arguments.size() != width) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            for (size_t i = 0; i < arguments.size(); i++) {
                std::shared_ptr<Type> &arg_type = arguments[i].first->type;
                if (arg_type == base_type) {
                    continue;
                }
                if (dynamic_cast<const GroupType *>(arg_type.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                std::optional<bool> castability = check_castability(base_type, arg_type);
                if (castability.has_value() && castability.value()) {
                    arguments[i].first = std::make_unique<TypeCastNode>(base_type, arguments[i].first);
                } else if (dynamic_cast<const PrimitiveType *>(arg_type.get())) {
                    const std::string &arg_type_str = arg_type->to_string();
                    if (primitive_implicit_casting_table.find(arg_type_str) == primitive_implicit_casting_table.end()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    const auto &to_types = primitive_implicit_casting_table.at(arg_type_str);
                    if (std::find(to_types.begin(), to_types.end(), base_type->to_string()) == to_types.end()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    arguments[i].first = std::make_unique<TypeCastNode>(base_type, arguments[i].first);
                }
            }
            return std::make_tuple(tokens.first->type->to_string(), std::move(arguments), tokens.first->type, true, types{});
        } else {
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return std::nullopt;
        }
    }

    // It's definitely a call
    std::string function_name;
    for (auto tok = tokens.first; tok != tokens.second; ++tok) {
        // Get the function name
        if (tok->token == TOK_IDENTIFIER) {
            function_name = tok->lexme;
            break;
        } else if (std::distance(tokens.first, tok) == arg_range.value().first) {
            // Function with no name
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

    // Check if its a call to a builtin function, if it is, get the return type of said function
    const auto builtin_function = get_builtin_function(function_name, file_node_ptr->imported_core_modules);
    if (builtin_function.has_value() && (std::get<2>(builtin_function.value()) == alias_base)) {
        if (function_name == "print" && argument_types.size() == 1 && argument_types.front()->to_string() == "str") {
            // Check if the string is a typecast of a string literal, if yes its a special print call
            if (TypeCastNode *type_cast = dynamic_cast<TypeCastNode *>(arguments.front().first.get())) {
                if (type_cast->expr->type->to_string() == "__flint_type_str_lit") {
                    arguments.front().first = std::move(type_cast->expr);
                    argument_types.front() = arguments.front().first->type;
                    return std::make_tuple(function_name, std::move(arguments), Type::get_primitive_type("void"), false, types{});
                }
            }
        }
        // Check if the function has the same arguments as the function expects
        const auto &function_overloads = std::get<1>(builtin_function.value());
        // Check if any overloaded function exists
        std::optional<std::tuple<types, types, types>> found_function;

        for (const auto &[param_types_str, return_types_str, error_types_str] : function_overloads) {
            if (arguments.size() != param_types_str.size()) {
                continue;
            }
            types param_types(param_types_str.size());
            std::transform(param_types_str.begin(), param_types_str.end(), param_types.begin(), Type::str_to_type);
            types return_types(return_types_str.size());
            std::transform(return_types_str.begin(), return_types_str.end(), return_types.begin(), Type::str_to_type);
            if (argument_types != param_types) {
                continue;
            }
            auto param_it = param_types.begin();
            auto arg_it = argument_types.begin();
            bool is_same = true;
            while (arg_it != argument_types.end()) {
                if (*param_it != *arg_it) {
                    is_same = false;
                }
                ++param_it;
                ++arg_it;
            }
            if (is_same) {
                types error_types(error_types_str.size());
                std::transform(error_types_str.begin(), error_types_str.end(), error_types.begin(), Type::str_to_type);
                found_function = {param_types, return_types, error_types};
            }
        }
        if (!found_function.has_value()) {
            token_slice err_tokens = {tokens.first + arg_range.value().first - 2, tokens.first + arg_range.value().second + 1};
            THROW_ERR(ErrExprCallOfUndefinedFunction, ERR_PARSING, file_name, err_tokens, function_name, argument_types);
            return std::nullopt;
        }
        auto &fn = found_function.value();
        if (std::get<1>(fn).size() > 1) {
            std::shared_ptr<Type> group_type = std::make_shared<GroupType>(std::get<1>(fn));
            if (!Type::add_type(group_type)) {
                // The type was already present, so we set the type of the group expression to the already present type to minimize type
                // duplication
                group_type = Type::get_type_from_str(group_type->to_string()).value();
            }
            return std::make_tuple(function_name, std::move(arguments), group_type, false, std::get<2>(fn));
        }
        return std::make_tuple(function_name, std::move(arguments), std::get<1>(fn).front(), false, std::get<2>(fn));
    }

    // Get the acutal function this call targets, and check if it even exists
    auto function = get_function_from_call(function_name, argument_types);
    if (!function.has_value()) {
        THROW_ERR(ErrExprCallOfUndefinedFunction, ERR_PARSING, file_name, tokens, function_name, argument_types);
        return std::nullopt;
    }
    // Check if the argument count does match the parameter count
    const unsigned int param_count = function.value().first->parameters.size();
    const unsigned int arg_count = arguments.size();
    // Argument counts are guaranteed to match the param count because if they would not, the `get_function_from_call` function would have
    // returned `std::nullopt`
    assert(param_count == arg_count);
    // If we came until here, the argument types definitely match the function parameter types, otherwise no function would have been
    // found Lastly, update the arguments of the call with the information of the function definition, if the arguments should be
    // references Every non-primitive type is always a reference (except enum types, for now)
    for (size_t i = 0; i < arguments.size(); i++) {
        if (dynamic_cast<const EnumType *>(arguments[i].first->type.get()) != nullptr) {
            arguments[i].second = false;
        } else {
            arguments[i].second = primitives.find(arguments[i].first->type->to_string()) == primitives.end();
        }
        // Also, we check here if the variable is immutable but the function expects an mutable reference instead
        if (arguments[i].second) {
            // Its a complex data type, so its a reference
            auto tok = tokens.first;
            for (; tok != tokens.second; ++tok) {
                // Get the function name
                if (tok->token == TOK_IDENTIFIER && tok->lexme == function_name) {
                    break;
                }
            }
            // Next is the open paren
            assert((++tok)->token == TOK_LEFT_PAREN);
            // Now we need to get until the token where the error happened, e.g. the ith argument
            ++tok;
            size_t depth = 0;
            size_t arg_id = i;
            while (arg_id > 0) {
                if (tok->token == TOK_COMMA && depth == 0) {
                    arg_id--;
                    if (arg_id == 0) {
                        break;
                    }
                } else if (tok->token == TOK_LEFT_PAREN || tok->token == TOK_LEFT_BRACKET) {
                    depth++;
                } else if (tok->token == TOK_RIGHT_PAREN || tok->token == TOK_RIGHT_BRACKET) {
                    depth--;
                }
                ++tok;
            }
            if (const VariableNode *variable_node = dynamic_cast<const VariableNode *>(arguments[i].first.get())) {
                if (!std::get<2>(scope->variables.at(variable_node->name)) && std::get<2>(function.value().first->parameters[i])) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, tok->line, tok->column, variable_node->name);
                    return std::nullopt;
                }
            }
        }
    }

    types return_types = function.value().first->return_types;
    auto error_types = function.value().first->error_types;
    if (return_types.empty()) {
        return std::make_tuple(function_name, std::move(arguments), Type::get_primitive_type("void"), false, error_types);
    } else if (return_types.size() > 1) {
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(return_types);
        if (!Type::add_type(group_type)) {
            group_type = Type::get_type_from_str(group_type->to_string()).value();
        }
        return std::make_tuple(function_name, std::move(arguments), group_type, false, error_types);
    }
    return std::make_tuple(function_name, std::move(arguments), return_types.front(), false, error_types);
}

std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> Parser::create_unary_op_base( //
    std::shared_ptr<Scope> scope,                                                                     //
    const token_slice &tokens                                                                         //
) {
    token_slice tokens_mut = tokens;
    remove_trailing_garbage(tokens_mut);
    // For an unary operator to work, the tokens now must have at least two tokens
    size_t tokens_size = get_slice_size(tokens_mut);
    if (tokens_size < 2) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Check if the unary operator is defined to the left of the expression or the right of it
    bool is_left;
    const uint2 left_range = {0, 1};
    const uint2 right_range = {tokens_size - 1, tokens_size};
    if (Matcher::tokens_contain_in_range(tokens_mut, Matcher::unary_operator, left_range)) {
        is_left = true;
    } else if (Matcher::tokens_contain_in_range(tokens_mut, Matcher::unary_operator, right_range)) {
        is_left = false;
    } else {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Extract the operator token
    token_slice operator_tokens;
    if (is_left) {
        operator_tokens = {tokens_mut.first + left_range.first, tokens_mut.first + left_range.second};
        tokens_mut.first = operator_tokens.second;
    } else {
        operator_tokens = {tokens_mut.first + right_range.first, tokens_mut.first + right_range.second};
        tokens_mut.second = operator_tokens.first;
    }
    assert(std::next(operator_tokens.first) == operator_tokens.second); // Assert operator_tokens.size() == 1
    Token operator_token = operator_tokens.first->token;

    // All other tokens now are the expression
    auto expression = create_expression(scope, tokens_mut);
    if (!expression.has_value()) {
        return std::nullopt;
    }

    return std::make_tuple(operator_token, std::move(expression.value()), is_left);
}

std::optional<std::tuple<std::unique_ptr<ExpressionNode>, std::optional<std::string>, unsigned int, std::shared_ptr<Type>>>
Parser::create_field_access_base(     //
    std::shared_ptr<Scope> scope,     //
    const token_slice &tokens,        //
    const bool has_inbetween_operator //
) {
    // We actually start at the end of the tokens and first check if it's a named access or an unnamed access, like a tuple access and
    // then everything to the left of the `.` is considered the base expression on which we then access the field
    std::string field_name = "";
    unsigned int field_id = 0;
    token_slice base_expr_tokens = {tokens.first, tokens.second - 1};

    if (base_expr_tokens.second->token == TOK_IDENTIFIER) {
        field_name = base_expr_tokens.second->lexme;
    } else if (base_expr_tokens.second->token == TOK_INT_VALUE) {
        assert(std::prev(base_expr_tokens.second)->token == TOK_DOLLAR);
        long int_value = std::stol(std::string(base_expr_tokens.second->lexme));
        if (int_value < 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        field_id = static_cast<unsigned int>(int_value);
        base_expr_tokens.second--;
    } else {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    base_expr_tokens.second--;
    if (base_expr_tokens.second->token != TOK_DOT) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (has_inbetween_operator) {
        base_expr_tokens.second--;
        if (!Matcher::token_match(base_expr_tokens.second->token, Matcher::inbetween_operator)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

    // Now everything left in the `base_expr_tokens` is our base expression, so we can parse it accordingly
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::shared_ptr<Type> base_type = base_expr.value()->type;
    if (has_inbetween_operator) {
        const OptionalType *optional_type = dynamic_cast<const OptionalType *>(base_type.get());
        assert(optional_type != nullptr);
        base_type = optional_type->base_type;
    }

    // If the base expresion is of type `str`, the only valid access is its `length` variable
    if (base_type->to_string() == "str") {
        if (field_name != "length") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple(std::move(base_expr.value()), "length", 0, Type::get_primitive_type("u64"));
    } else if (const ArrayType *array_type = dynamic_cast<const ArrayType *>(base_type.get())) {
        if (field_name != "length") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (array_type->dimensionality > 1) {
            std::shared_ptr<Type> u64_type = Type::get_primitive_type("u64");
            std::vector<std::shared_ptr<Type>> length_types;
            for (size_t i = 0; i < array_type->dimensionality; i++) {
                length_types.emplace_back(u64_type);
            }
            std::shared_ptr<Type> group_type = std::make_shared<GroupType>(length_types);
            if (!Type::add_type(group_type)) {
                group_type = Type::get_type_from_str(group_type->to_string()).value();
            }
            return std::make_tuple(std::move(base_expr.value()), "length", 1, group_type);
        }
        return std::make_tuple(std::move(base_expr.value()), "length", 1, Type::get_primitive_type("u64"));
    } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(base_type.get())) {
        if (field_name == "") {
            field_name = "$" + std::to_string(field_id);
        }
        auto access = create_multi_type_access(multi_type, field_name);
        if (!access.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple(           //
            std::move(base_expr.value()), // Base Expression
            std::get<0>(access.value()),  // Name of the accessed field
            std::get<1>(access.value()),  // ID of the accessed field
            multi_type->base_type         // Type of the accessed field
        );
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(base_type.get())) {
        // Its a data type
        const DataNode *data_node = data_type->data_node;

        // Now we can check if the given field name exists in the data type
        field_id = 0;
        for (const auto &field : data_node->fields) {
            if (std::get<0>(field) == field_name) {
                break;
            }
            field_id++;
        }
        if (data_node->fields.size() == field_id) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const std::shared_ptr<Type> field_type = std::get<1>(data_node->fields.at(field_id));
        return std::make_tuple(std::move(base_expr.value()), field_name, field_id, field_type);
    } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(base_type.get())) {
        if (field_id >= tuple_type->types.size()) {
            auto it = base_expr_tokens.second + 1; // 'it' now is the $ symbol
            THROW_ERR(ErrExprTupleAccessOOB, ERR_PARSING, file_name, it->line, it->column, "$" + std::to_string(field_id), base_type);
            return std::nullopt;
        }
        const std::shared_ptr<Type> field_type = tuple_type->types.at(field_id);
        return std::make_tuple(std::move(base_expr.value()), std::nullopt, field_id, field_type);
    } else if (dynamic_cast<const ErrorSetType *>(base_type.get()) || base_type->to_string() == "anyerror") {
        if (field_name == "type_id") {
            return std::make_tuple(std::move(base_expr.value()), field_name, 0, Type::get_primitive_type("u32"));
        } else if (field_name == "value_id") {
            return std::make_tuple(std::move(base_expr.value()), field_name, 1, Type::get_primitive_type("u32"));
        } else if (field_name == "message") {
            return std::make_tuple(std::move(base_expr.value()), field_name, 2, Type::get_primitive_type("str"));
        }
    } else if (dynamic_cast<const VariantType *>(base_type.get())) {
        if (field_name == "active_type") {
            return std::make_tuple(std::move(base_expr.value()), field_name, 0, Type::get_primitive_type("u8"));
        }
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return std::nullopt;
}

std::optional<std::tuple<std::string, unsigned int>> Parser::create_multi_type_access( //
    const MultiType *multi_type,                                                       //
    const std::string &field_name                                                      //
) {
    if (multi_type->width == 2) {
        // The fields are called x and y, but can be accessed via $N
        if (field_name == "x" || field_name == "$0") {
            return std::make_tuple("$0", 0);
        } else if (field_name == "y" || field_name == "$1") {
            return std::make_tuple("$1", 1);
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    } else if (multi_type->width == 3) {
        // The fields are called x, y and z, but can be accessed via $N
        if (field_name == "x" || field_name == "$0") {
            return std::make_tuple("$0", 0);
        } else if (field_name == "y" || field_name == "$1") {
            return std::make_tuple("$1", 1);
        } else if (field_name == "z" || field_name == "$2") {
            return std::make_tuple("$2", 2);
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    } else if (multi_type->width == 4) {
        // The fields are called r, g, b and a, but can be accessed via $N
        if (field_name == "r" || field_name == "$0") {
            return std::make_tuple("$0", 0);
        } else if (field_name == "g" || field_name == "$1") {
            return std::make_tuple("$1", 1);
        } else if (field_name == "b" || field_name == "$2") {
            return std::make_tuple("$2", 2);
        } else if (field_name == "a" || field_name == "$3") {
            return std::make_tuple("$3", 3);
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    } else {
        // The fields are accessed via $N
        if (field_name.front() != '$') {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        if (field_name.size() != 2) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const char id = field_name.back() - '0';
        if (static_cast<unsigned int>(id) >= multi_type->width || id < 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple("$" + std::to_string(id), id);
    }
}

std::optional<std::tuple<std::unique_ptr<ExpressionNode>, std::vector<std::string>, std::vector<unsigned int>,
    std::vector<std::shared_ptr<Type>>>>
Parser::create_grouped_access_base(   //
    std::shared_ptr<Scope> scope,     //
    const token_slice &tokens,        //
    const bool has_inbetween_operator //
) {
    // We start at the end of the token slice and move towards the front, and split the token slice in half to get the base expression
    // tokens and all tokens forming the grouped access `.(..)`
    assert((tokens.second - 1)->token == TOK_RIGHT_PAREN);
    token_slice base_expr_tokens = {tokens.first, tokens.second - 1};
    token_slice access_tokens = {tokens.second - 1, tokens.second - 1};
    unsigned int depth = 0;
    for (; base_expr_tokens.second != base_expr_tokens.first;) {
        if (base_expr_tokens.second->token == TOK_RIGHT_PAREN) {
            depth++;
        } else if (base_expr_tokens.second->token == TOK_LEFT_PAREN) {
            depth--;
            if (depth == 0) {
                // Move past the ( for the access tokens
                access_tokens.first++;
                // End at the . and not at the ( for the base expression
                base_expr_tokens.second--;
                assert(base_expr_tokens.second->token == TOK_DOT);
                break;
            }
        }
        base_expr_tokens.second--;
        access_tokens.first--;
    }
    if (has_inbetween_operator) {
        base_expr_tokens.second--;
        if (!Matcher::token_match(base_expr_tokens.second->token, Matcher::inbetween_operator)) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

    // Okay we now can parse the base expression beforehand, to be able to check it's type and decide whether a grouped access is
    // allowed at all
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::shared_ptr<Type> base_type = base_expr.value()->type;
    if (has_inbetween_operator) {
        const OptionalType *optional_type = dynamic_cast<const OptionalType *>(base_type.get());
        assert(optional_type != nullptr);
        base_type = optional_type->base_type;
    }

    // Now, extract the names of all accessed fields
    std::vector<std::string> field_names;
    while (access_tokens.first != access_tokens.second) {
        if (access_tokens.first->token == TOK_IDENTIFIER) {
            field_names.emplace_back(access_tokens.first->lexme);
        } else if (access_tokens.first->token == TOK_DOLLAR && std::next(access_tokens.first)->token == TOK_INT_VALUE) {
            assert(std::next(access_tokens.first)->lexme.find('_') == std::string::npos);
            field_names.emplace_back("$" + std::string(std::next(access_tokens.first)->lexme));
            access_tokens.first++;
        }
        access_tokens.first++;
    }
    if (field_names.empty()) {
        // Empty group access
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // First, look if its a multi-type
    if (const MultiType *multi_type = dynamic_cast<const MultiType *>(base_type.get())) {
        std::vector<std::string> access_field_names;
        std::vector<std::shared_ptr<Type>> field_types;
        std::vector<unsigned int> field_ids;
        for (const auto &field_name : field_names) {
            auto access = create_multi_type_access(multi_type, field_name);
            if (!access.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            access_field_names.emplace_back(std::get<0>(access.value()));
            field_types.emplace_back(multi_type->base_type);
            field_ids.emplace_back(std::get<1>(access.value()));
        }
        return std::make_tuple(std::move(base_expr.value()), access_field_names, field_ids, field_types);
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(base_type.get())) {
        // It should be a data type
        const DataNode *data_node = data_type->data_node;

        // Next, get the field types and field ids from the data node
        std::vector<std::shared_ptr<Type>> field_types;
        field_types.resize(field_names.size());
        std::vector<unsigned int> field_ids;
        field_ids.resize(field_names.size());
        size_t field_id = 0;
        for (const auto &field : data_node->fields) {
            const auto field_names_it = std::find(field_names.begin(), field_names.end(), std::get<0>(field));
            if (field_names_it != field_names.end()) {
                const size_t group_id = std::distance(field_names.begin(), field_names_it);
                field_types[group_id] = std::get<1>(field);
                field_ids[group_id] = field_id;
            }
            field_id++;
        }
        return std::make_tuple(std::move(base_expr.value()), field_names, field_ids, field_types);
    } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(base_type.get())) {
        std::vector<std::shared_ptr<Type>> field_types;
        std::vector<unsigned int> field_ids;
        for (size_t i = 0; i < field_names.size(); i++) {
            size_t field_id = std::stoul(field_names.at(i).substr(1, field_names.at(i).length() - 1));
            if (field_id >= tuple_type->types.size()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            field_types.emplace_back(tuple_type->types.at(field_id));
            field_ids.emplace_back(field_id);
        }
        return std::make_tuple(std::move(base_expr.value()), field_names, field_ids, field_types);
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return std::nullopt;
}
