#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/tuple_type.hpp"
#include <algorithm>

bool Parser::add_next_main_node(FileNode &file_node, token_slice &tokens) {
    token_slice definition_tokens = get_definition_tokens(tokens);
    tokens.first = definition_tokens.second;

    // Find the indentation of the definition
    int definition_indentation = 0;
    for (auto tok = definition_tokens.first; tok != definition_tokens.second; ++tok) {
        if (tok->type == TOK_INDENT) {
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
                    module_str != "env") {
                    const auto &tok = definition_tokens.first + 3;
                    THROW_ERR(ErrDefUnexpectedCoreModule, ERR_PARSING, file_name, tok->line, tok->column, module_str);
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

    token_slice body_tokens = get_body_tokens(definition_indentation, tokens);
    tokens.first = body_tokens.second;
    if (Matcher::tokens_contain(definition_tokens, Matcher::function_definition)) {
        // Dont actually parse the function body, only its definition
        std::optional<FunctionNode> function_node = create_function(definition_tokens);
        if (!function_node.has_value()) {
            THROW_ERR(ErrDefFunctionCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        FunctionNode *added_function = file_node.add_function(function_node.value());
        add_open_function({added_function, clone_from_slice(body_tokens)});
        add_parsed_function(added_function, file_name);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::data_definition)) {
        std::optional<DataNode> data_node = create_data(definition_tokens, body_tokens);
        if (!data_node.has_value()) {
            THROW_ERR(ErrDefDataCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        DataNode *added_data = file_node.add_data(data_node.value());
        add_parsed_data(added_data, file_name);
        if (!Type::add_type(std::make_shared<DataType>(added_data))) {
            THROW_ERR(ErrDefDataRedefinition, ERR_PARSING, file_name,                                  //
                definition_tokens.first->line, definition_tokens.first->column, data_node.value().name //
            );
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::func_definition)) {
        std::optional<FuncNode> func_node = create_func(definition_tokens, body_tokens);
        if (!func_node.has_value()) {
            THROW_ERR(ErrDefFuncCreation, ERR_PARSING, file_name, definition_tokens);
            return false;
        }
        file_node.add_func(func_node.value());
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::entity_definition)) {
        create_entity_type entity_creation = create_entity(definition_tokens, body_tokens);
        file_node.add_entity(entity_creation.first);
        if (entity_creation.second.has_value()) {
            std::unique_ptr<DataNode> data_node_ptr = std::move(entity_creation.second.value().first);
            std::unique_ptr<FuncNode> func_node_ptr = std::move(entity_creation.second.value().second);
            file_node.add_data(*data_node_ptr);
            file_node.add_func(*func_node_ptr);
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::enum_definition)) {
        std::optional<EnumNode> enum_node = create_enum(definition_tokens, body_tokens);
        if (!enum_node.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        EnumNode *added_enum = file_node.add_enum(enum_node.value());
        if (!Type::add_type(std::make_shared<EnumType>(added_enum))) {
            // Enum redifinition
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::error_definition)) {
        ErrorNode error_node = create_error(definition_tokens, body_tokens);
        file_node.add_error(error_node);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::variant_definition)) {
        VariantNode variant_node = create_variant(definition_tokens, body_tokens);
        file_node.add_variant(variant_node);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::test_definition)) {
        std::optional<TestNode> test_node = create_test(definition_tokens);
        if (!test_node.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        TestNode *added_test = file_node.add_test(test_node.value());
        add_open_test({added_test, clone_from_slice(body_tokens)});
        add_parsed_test(added_test, file_name);
    } else {
        Debug::print_token_context_vector(definition_tokens, file_name);
        THROW_ERR(ErrUnexpectedDefinition, ERR_PARSING, file_name, definition_tokens);
        return false;
    }
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

token_slice Parser::get_body_tokens(unsigned int definition_indentation, const token_slice &tokens) {
    int end_idx = 0;
    unsigned int current_line = tokens.first->line;
    for (auto it = tokens.first; it != tokens.second; ++it) {
        if (it->line != current_line || it == tokens.first) {
            current_line = it->line;
            std::optional<unsigned int> indents_maybe = Matcher::get_leading_indents(tokens, current_line);
            if (indents_maybe.has_value() && indents_maybe.value() <= definition_indentation) {
                break;
            }
        }
        end_idx++;
    }
    if (end_idx == 0) {
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_name, tokens);
        std::exit(EXIT_FAILURE);
    }

    return {tokens.first, tokens.first + end_idx};
}

std::optional<std::tuple<                                          //
    std::string,                                                   // name
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>>, // args
    std::shared_ptr<Type>,                                         // type
    std::optional<bool>,                                           // is data (true), entity (false) or call (nullopt)
    bool                                                           // can_throw
    >>
Parser::create_call_or_initializer_base(Scope *scope, const token_slice &tokens, const std::optional<std::string> &alias_base) {
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

    std::string function_name;
    std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> arguments;

    for (auto tok = tokens.first; tok != tokens.second; ++tok) {
        // Get the function name
        if (tok->type == TOK_IDENTIFIER) {
            function_name = tok->lexme;
            break;
        } else if (std::distance(tokens.first, tok) == arg_range.value().first) {
            // Function with no name
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

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
                    THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, argument_tokens);
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
                THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, argument_tokens);
                return std::nullopt;
            }
            arguments.emplace_back(std::move(expression.value()), false);
        }
    }

    // Get all the argument types
    std::vector<std::shared_ptr<Type>> argument_types;
    argument_types.reserve(arguments.size());
    for (size_t i = 0; i < arguments.size(); i++) {
        // Typecast all string literals in the args to string variables
        if (arguments[i].first->type->to_string() == "__flint_type_str_lit") {
            arguments[i].first = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), arguments[i].first);
        }
        argument_types.emplace_back(arguments[i].first->type);
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
                    return std::make_tuple(function_name, std::move(arguments), Type::get_primitive_type("void"), std::nullopt, false);
                }
            }
        }
        // Check if the function has the same arguments as the function expects
        const auto &function_overloads = std::get<1>(builtin_function.value());
        // Check if any overloaded function exists
        std::optional<std::tuple<std::vector<std::shared_ptr<Type>>, std::vector<std::shared_ptr<Type>>, bool>> found_function;

        for (const auto &[param_types_str, return_types_str, can_throw] : function_overloads) {
            if (arguments.size() != param_types_str.size()) {
                continue;
            }
            std::vector<std::shared_ptr<Type>> param_types(param_types_str.size());
            std::transform(param_types_str.begin(), param_types_str.end(), param_types.begin(), Type::str_to_type);
            std::vector<std::shared_ptr<Type>> return_types(return_types_str.size());
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
                found_function = {param_types, return_types, can_throw};
            }
        }
        if (!found_function.has_value()) {
            token_slice err_tokens = {tokens.first + arg_range.value().first - 2, tokens.first + arg_range.value().second + 1};
            THROW_ERR(                                                                                          //
                ErrExprCallWrongArgsBuiltin, ERR_PARSING, file_name, err_tokens, function_name, argument_types, //
                file_node_ptr->imported_core_modules                                                            //
            );
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
            return std::make_tuple(function_name, std::move(arguments), group_type, std::nullopt, std::get<2>(fn));
        }
        return std::make_tuple(function_name, std::move(arguments), std::get<1>(fn).front(), std::nullopt, std::get<2>(fn));
    }

    // Check if there exists a type with the name of the "function" call
    std::optional<std::shared_ptr<Type>> complex_type = Type::get_type_from_str(function_name);
    if (complex_type.has_value()) {
        // Its a data or entity type
        if (const DataType *data_type = dynamic_cast<const DataType *>(complex_type.value().get())) {
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
                    std::optional<bool> castability = check_castability(field_type, arg_type);
                    if (castability.has_value() && castability.value()) {
                        arguments[i].first = std::make_unique<TypeCastNode>(field_type, arguments[i].first);
                    } else {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                }
            }
            return std::make_tuple(data_node->name, std::move(arguments), complex_type.value(), true, false);
        }
    }

    // Its definitely not a data type initializer, so it must be a function call
    // Get the acutal function this call targets, and check if it even exists
    auto function = get_function_from_call(function_name, argument_types);
    if (!function.has_value()) {
        THROW_ERR(ErrExprCallOfUndefinedFunction, ERR_PARSING, file_name, tokens, function_name);
        return std::nullopt;
    }
    // Check if the argument count does match the parameter count
    const unsigned int param_count = function.value().first->parameters.size();
    const unsigned int arg_count = arguments.size();
    if (param_count != arg_count) {
        THROW_ERR(ErrExprCallWrongArgCount, ERR_PARSING, file_name, tokens, function_name, param_count, arg_count);
        return std::nullopt;
    }
    // If we came until here, the argument types definitely match the function parameter types, otherwise no function would have been
    // found Lastly, update the arguments of the call with the information of the function definition, if the arguments should be
    // references Every non-primitive type is always a reference (except enum types, for now)
    for (size_t i = 0; i < arguments.size(); i++) {
        if (dynamic_cast<const EnumType *>(arguments[i].first->type.get()) != nullptr) {
            arguments[i].second = false;
        } else {
            arguments[i].second = keywords.find(arguments[i].first->type->to_string()) == keywords.end();
        }
        // Also, we check here if the variable is immutable but the function expects an mutable reference instead
        if (arguments[i].second) {
            // Its a complex data type, so its a reference
            if (const VariableNode *variable_node = dynamic_cast<const VariableNode *>(arguments[i].first.get())) {
                if (!std::get<2>(scope->variables.at(variable_node->name)) && std::get<2>(function.value().first->parameters[i])) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_name, 1, 1, variable_node->name);
                    return std::nullopt;
                }
            }
        }
    }

    std::vector<std::shared_ptr<Type>> return_types = function.value().first->return_types;
    if (return_types.empty()) {
        return std::make_tuple(function_name, std::move(arguments), Type::get_primitive_type("void"), std::nullopt, true);
    } else if (return_types.size() > 1) {
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(return_types);
        if (!Type::add_type(group_type)) {
            group_type = Type::get_type_from_str(group_type->to_string()).value();
        }
        return std::make_tuple(function_name, std::move(arguments), group_type, std::nullopt, true);
    }
    return std::make_tuple(function_name, std::move(arguments), return_types.front(), std::nullopt, true);
}

std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> Parser::create_unary_op_base( //
    Scope *scope,                                                                                     //
    const token_slice &tokens                                                                         //
) {
    token_slice tokens_mut = tokens;
    remove_leading_garbage(tokens_mut);
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
    Token operator_token = operator_tokens.first->type;

    // All other tokens now are the expression
    auto expression = create_expression(scope, tokens_mut);
    if (!expression.has_value()) {
        THROW_ERR(ErrExprCreationFailed, ERR_PARSING, file_name, tokens_mut);
        return std::nullopt;
    }

    return std::make_tuple(operator_token, std::move(expression.value()), is_left);
}

std::optional<std::tuple<std::shared_ptr<Type>, std::string, std::optional<std::string>, unsigned int, std::shared_ptr<Type>>>
Parser::create_field_access_base( //
    Scope *scope,                 //
    token_slice &tokens           //
) {
    remove_leading_garbage(tokens);

    // The first token is the accessed variable name
    assert(tokens.first->type == TOK_IDENTIFIER);
    const std::string var_name = tokens.first->lexme;
    tokens.first++;

    // The next token should be a dot, skip it
    assert(tokens.first->type == TOK_DOT);
    tokens.first++;

    // Then there should be the name of the field to access, or a $N instead of it
    std::string field_name = "";
    unsigned int field_id = 0;
    if (tokens.first->type == TOK_IDENTIFIER) {
        field_name = tokens.first->lexme;
        tokens.first++;
    } else if (tokens.first->type == TOK_DOLLAR) {
        tokens.first++;
        assert(tokens.first->type == TOK_INT_VALUE);
        long int_value = std::stol(tokens.first->lexme);
        if (int_value < 0) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        field_id = static_cast<unsigned int>(int_value);
        tokens.first++;
    } else {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Is the variable name itself a type string?
    std::optional<std::shared_ptr<Type>> variable_type = Type::get_type_from_str(var_name);
    if (variable_type.has_value()) {
        // The variable name is not a variable but a type of its own
        if (const EnumType *enum_type = dynamic_cast<const EnumType *>(variable_type.value().get())) {
            const EnumNode *enum_node = enum_type->enum_node;
            auto it = std::find(enum_node->values.begin(), enum_node->values.end(), field_name);
            if (it == enum_node->values.end()) {
                // Enum value is non existent
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            const size_t val_id = std::distance(enum_node->values.begin(), it);
            return std::make_tuple(variable_type.value(), var_name, field_name, val_id, variable_type.value());
        }
        // Non-supported type to call `.` on it
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now get the data type from the data variables name
    variable_type = scope->get_variable_type(var_name);
    if (!variable_type.has_value()) {
        // The variable doesnt exist
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // If the variable is of type `str`, the only valid access is its `length` variable
    if (variable_type.value()->to_string() == "str") {
        if (field_name != "length") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple(variable_type.value(), var_name, "length", 0, Type::get_primitive_type("u64"));
    } else if (const ArrayType *array_type = dynamic_cast<const ArrayType *>(variable_type.value().get())) {
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
            return std::make_tuple(variable_type.value(), var_name, "length", 1, group_type);
        }
        return std::make_tuple(variable_type.value(), var_name, "length", 1, Type::get_primitive_type("u64"));
    } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(variable_type.value().get())) {
        if (field_name == "") {
            field_name = "$" + std::to_string(field_id);
        }
        auto access = create_multi_type_access(multi_type, field_name);
        if (!access.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple(          //
            variable_type.value(),       // Base type (multi-type)
            var_name,                    // Name of the mutli-type variable
            std::get<0>(access.value()), // Name of the accessed field
            std::get<1>(access.value()), // ID of the accessed field
            multi_type->base_type        // Type of the accessed field
        );
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(variable_type.value().get())) {
        // Its a data type
        const DataNode *data_node = data_type->data_node;

        // Now we can check if the given field name exists in the data
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
        return std::make_tuple(variable_type.value(), var_name, field_name, field_id, field_type);
    } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(variable_type.value().get())) {
        if (field_id >= tuple_type->types.size()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const std::shared_ptr<Type> field_type = tuple_type->types.at(field_id);
        return std::make_tuple(variable_type.value(), var_name, std::nullopt, field_id, field_type);
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

std::optional<std::tuple<std::shared_ptr<Type>, std::string, std::vector<std::string>, std::vector<unsigned int>,
    std::vector<std::shared_ptr<Type>>>>
Parser::create_grouped_access_base( //
    Scope *scope,                   //
    token_slice &tokens             //
) {
    remove_leading_garbage(tokens);

    // The first token is the accessed variable name
    assert(tokens.first->type == TOK_IDENTIFIER);
    const std::string var_name = tokens.first->lexme;
    tokens.first++;

    // The next token should be a dot, skip it
    assert(tokens.first->type == TOK_DOT);
    tokens.first++;

    // Then there should be an opening parenthesis the name of the field to access
    assert(tokens.first->type == TOK_LEFT_PAREN);
    tokens.first++;

    // Now, extract the names of all accessed fields
    std::vector<std::string> field_names;
    while (tokens.first != tokens.second && tokens.first->type != TOK_RIGHT_PAREN) {
        if (tokens.first->type == TOK_IDENTIFIER) {
            field_names.emplace_back(tokens.first->lexme);
        } else if (tokens.first->type == TOK_DOLLAR && std::next(tokens.first)->type == TOK_INT_VALUE) {
            field_names.emplace_back("$" + std::next(tokens.first)->lexme);
            tokens.first++;
        }
        tokens.first++;
    }
    if (field_names.empty()) {
        // Empty group access
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // Now remove the right paren
    assert(tokens.first->type == TOK_RIGHT_PAREN);
    tokens.first++;

    // Now get the data type from the data variables name
    const std::optional<std::shared_ptr<Type>> variable_type = scope->get_variable_type(var_name);
    if (!variable_type.has_value()) {
        // The variable doesnt exist
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

    // First, look if its a multi-type
    if (const MultiType *multi_type = dynamic_cast<const MultiType *>(variable_type.value().get())) {
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
        return std::make_tuple(variable_type.value(), var_name, access_field_names, field_ids, field_types);
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(variable_type.value().get())) {
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
        return std::make_tuple(variable_type.value(), var_name, field_names, field_ids, field_types);
    } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(variable_type.value().get())) {
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
        return std::make_tuple(variable_type.value(), var_name, field_names, field_ids, field_types);
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return std::nullopt;
}
