#include "analyzer/analyzer.hpp"
#include "lexer/builtins.hpp"
#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/parser.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "profiler.hpp"

#include <algorithm>

bool Parser::add_next_main_node(FileNode &file_node, token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::add_next_main_node");
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

    if (Matcher::tokens_contain(definition_tokens, Matcher::token(TOK_ANNOTATION))) {
        return add_annotation(definition_tokens);
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::use_statement)) {
        if (definition_indentation > 0) {
            THROW_ERR(ErrUseClauselNotAtTopLevel, ERR_PARSING, file_hash, definition_tokens);
            return false;
        }
        std::optional<ImportNode> import_node = create_import(definition_tokens);
        if (!import_node.has_value()) {
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
        auto &aliased_imports = file_node_ptr->file_namespace->public_symbols.aliased_imports;
        if (import_node.value().alias.has_value() && aliased_imports.find(import_node.value().alias.value()) != aliased_imports.end()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        if (std::holds_alternative<std::vector<std::string>>(import_node.value().path)) {
            const std::vector<std::string> &import_vec = std::get<std::vector<std::string>>(import_node.value().path);
            if (import_vec.size() == 2 && import_vec.front() == "Core") {
                // Check for imported core modules
                const std::string &module_str = import_vec.back();
                if (core_module_functions.find(module_str) == core_module_functions.end()) {
                    const auto &tok = definition_tokens.first + 3;
                    THROW_ERR(ErrDefUnexpectedCoreModule, ERR_PARSING, file_hash, tok->line, tok->column, module_str);
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
            assert(aliased_imports.find(added_import.value()->alias.value()) == aliased_imports.end());
            // Add a nullopt to them, we actually resolve the imports in the `resolve_all_imports` function when all namespaces are
            // available
            aliased_imports[added_import.value()->alias.value()] = nullptr;
        }
        if (std::holds_alternative<Hash>(added_import.value()->path) ||                           //
            (std::get<std::vector<std::string>>(added_import.value()->path).size() != 2 &&        //
                std::get<std::vector<std::string>>(added_import.value()->path).front() != "Core") //
        ) {
            imported_files.emplace_back(added_import.value());
        }
        return true;
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::type_alias)) {
        assert(definition_tokens.first->token == TOK_TYPE_KEYWORD);
        assert((definition_tokens.first + 1)->token == TOK_IDENTIFIER);
        const std::string type_alias((definition_tokens.first + 1)->lexme);
        auto it = definition_tokens.first + 2;
        while (it->token != TOK_EOL) {
            it++;
        }
        // Everything from the second to the it token is the type
        const token_slice type_tokens{definition_tokens.first + 2, it};
        std::optional<std::shared_ptr<Type>> aliased_type = file_node_ptr->file_namespace->get_type(type_tokens);
        if (!aliased_type.has_value()) {
            return false;
        }
        std::shared_ptr<Type> type = std::make_shared<AliasType>(type_alias, aliased_type.value());
        if (!file_node_ptr->file_namespace->add_type(type)) {
            type = file_node_ptr->file_namespace->get_type_from_str(type->to_string()).value();
        }
        return true;
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::extern_function_declaration)) {
        std::optional<FunctionNode> function_node = create_extern_function(definition_tokens);
        if (!function_node.has_value()) {
            return false;
        }
        file_node.add_function(function_node.value());
        return true;
    }

    std::vector<Line> body_lines = get_body_lines(definition_indentation, tokens);
    if (body_lines.empty()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    if (Matcher::tokens_contain(definition_tokens, Matcher::function_definition)) {
        // Dont actually parse the function body, only its definition
        std::optional<FunctionNode> function_node = create_function(definition_tokens, {});
        if (!function_node.has_value()) {
            return false;
        }
        FunctionNode *added_function = file_node.add_function(function_node.value());
        add_open_function({added_function, body_lines});
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::data_definition)) {
        std::optional<DataNode> data_node = create_data(definition_tokens, body_lines);
        if (!data_node.has_value()) {
            return false;
        }
        std::optional<DataNode *> added_data = file_node.add_data(data_node.value());
        if (!added_data.has_value()) {
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::func_definition)) {
        std::optional<FuncNode> func_node = create_func(file_node, definition_tokens, body_lines);
        if (!func_node.has_value()) {
            return false;
        }
        std::optional<FuncNode *> added_func = file_node.add_func(func_node.value());
        if (!added_func.has_value()) {
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::entity_definition)) {
        std::optional<EntityNode> entity_node = create_entity(definition_tokens, body_lines);
        if (!entity_node.has_value()) {
            return false;
        }
        std::optional<EntityNode *> added_entity = file_node.add_entity(entity_node.value());
        if (!added_entity.has_value()) {
            return false;
        }
        if (added_entity.value()->is_monolithic) {
            assert(added_entity.value()->data_modules.size() == 1);
            assert(added_entity.value()->func_modules.size() == 1);
            DataNode *data_node_ptr = std::move(added_entity.value()->data_modules.front());
            FuncNode *func_node_ptr = std::move(added_entity.value()->func_modules.front());
            file_node.add_data(*data_node_ptr);
            file_node.add_func(*func_node_ptr);
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::enum_definition)) {
        std::optional<EnumNode> enum_node = create_enum(definition_tokens, body_lines);
        if (!enum_node.has_value()) {
            return false;
        }
        return file_node.add_enum(enum_node.value());
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::error_definition)) {
        std::optional<ErrorNode> error_node = create_error(definition_tokens, body_lines);
        if (!error_node.has_value()) {
            return false;
        }
        return file_node.add_error(error_node.value());
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::variant_definition)) {
        std::optional<VariantNode> variant_node = create_variant(definition_tokens, body_lines);
        if (!variant_node.has_value()) {
            return false;
        }
        return file_node.add_variant(variant_node.value());
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
        THROW_ERR(ErrUnexpectedDefinition, ERR_PARSING, file_hash, definition_tokens);
        return false;
    }
    return enusure_no_annotation_leftovers();
}

token_slice Parser::get_definition_tokens(const token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::get_definition_tokens");
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
    PROFILE_CUMULATIVE("Parser::get_body_lines");
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
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_hash, tokens);
        std::exit(EXIT_FAILURE);
    }

    return body_lines;
}

void Parser::collapse_types_in_lines(std::vector<Line> &lines, token_list &source) {
    PROFILE_CUMULATIVE("Parser::collapse_types_in_lines");
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
            // Collapse the whole alias chain to a single alias or to a single type
            Namespace *alias_namespace = file_node_ptr->file_namespace.get();
            // First check if the next token is an identifier and if it's a known import alias
            if (it->token == TOK_IDENTIFIER && alias_namespace->get_namespace_from_alias(std::string(it->lexme)).has_value()) {
                alias_namespace = alias_namespace->get_namespace_from_alias(std::string(it->lexme)).value();
                *it = TokenContext(TOK_ALIAS, it->line, it->column, it->file_id, alias_namespace);
                while (std::next(it)->token == TOK_DOT && (it + 2)->token == TOK_IDENTIFIER) {
                    // Check if the next two tokens are another alias, for example in the expression `a.b.call()` we collapse the alias
                    // chain to a single alias, `b.call()` here. If it's an expression of `a.b.Type` instead it will collapse to a single
                    // `Type` instead, since we know which exact type from which file it targets
                    std::optional<Namespace *> next_alias_namespace = alias_namespace->get_namespace_from_alias( //
                        std::string((it + 2)->lexme)                                                             //
                    );
                    if (next_alias_namespace.has_value()) {
                        it->alias_namespace = next_alias_namespace.value();
                        alias_namespace = next_alias_namespace.value();
                        // Delete the `.b` since we changed the import of `a` to point to the `b` namespace directly
                        Line::delete_tokens(source, it + 1, 2);
                    } else {
                        break;
                    }
                }
                // Check if it's an imported type from another namespaces. Types from other namespaces will *always* only be an
                // `alias.identifier`, they can never be anything else since otherwise they would not be exportable.
                if (std::next(it)->token == TOK_DOT && (it + 2)->token == TOK_IDENTIFIER) {
                    // Check if the type exists in the imported aliased namespace
                    auto imported_type = alias_namespace->get_type_from_str(std::string((it + 2)->lexme));
                    if (imported_type.has_value()) {
                        *it = TokenContext(TOK_TYPE, it->line, it->column, it->file_id, imported_type.value());
                        Line::delete_tokens(source, it + 1, 2);
                    }
                }
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
                        std::optional<std::shared_ptr<Type>> type = file_node_ptr->file_namespace->get_type_from_str( //
                            std::string(it->lexme)                                                                    //
                        );
                        if (type.has_value()) {
                            *it = TokenContext(TOK_TYPE, it->line, it->column, it->file_id, type.value());
                        }
                    }
                } else if (it->token != TOK_IDENTIFIER ||
                    file_node_ptr->file_namespace->get_type_from_str(std::string(it->lexme)).has_value()) {
                    // If its a bigger type and it starts with an identifier, the identifier itself must be a known type already. If the
                    // identifier is not a known type, this is an edge case like `i < 5 and x > 2` where `i<5 and x>` is interpreted as
                    // `T<..>`. So, `T` must be a known type in this case, otherwise the whole thing is no type. *or* it has to be a
                    // keyword, like `data` or `variant`. But when it's a keywords it's no identifier annyway.
                    auto type = file_node_ptr->file_namespace->get_type(token_slice{it, it + type_range.value().second});
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

    // Substitute all types aliases
    for (auto line : lines) {
        token_list toks = clone_from_slice(line.tokens);
        for (auto it = line.tokens.first; it != line.tokens.second; ++it) {
            if (it->token != TOK_TYPE) {
                continue;
            }
            substitute_type_aliases(it->type);
        }
    }
}

void Parser::substitute_type_aliases(std::shared_ptr<Type> &type_to_resolve) {
    switch (type_to_resolve->get_variation()) {
        case Type::Variation::ALIAS: {
            const auto *alias_type = type_to_resolve->as<AliasType>();
            type_to_resolve = alias_type->type;
            break;
        }
        case Type::Variation::ARRAY: {
            auto *array_type = type_to_resolve->as<ArrayType>();
            substitute_type_aliases(array_type->type);
            break;
        }
        case Type::Variation::DATA:
            // Data types resolve in a different stage
            break;
        case Type::Variation::ENTITY:
            // Entity types resolve in a different stage
            break;
        case Type::Variation::ENUM:
            // Enums cannot contain a type alias
            break;
        case Type::Variation::ERROR_SET:
            // Error sets cannot contain a type alias
            break;
        case Type::Variation::FUNC:
            // Func types resolve in a different stage
            break;
        case Type::Variation::GROUP: {
            auto *group_type = type_to_resolve->as<GroupType>();
            for (auto &type : group_type->types) {
                substitute_type_aliases(type);
            }
            break;
        }
        case Type::Variation::MULTI:
            // Multi types cannot contain type aliases
            break;
        case Type::Variation::OPTIONAL: {
            auto *optional_type = type_to_resolve->as<OptionalType>();
            substitute_type_aliases(optional_type->base_type);
            break;
        }
        case Type::Variation::POINTER: {
            auto *pointer_type = type_to_resolve->as<PointerType>();
            substitute_type_aliases(pointer_type->base_type);
            break;
        }
        case Type::Variation::PRIMITIVE:
            // Primitive types cannot contain type aliases
            break;
        case Type::Variation::RANGE: {
            auto *range_type = type_to_resolve->as<RangeType>();
            substitute_type_aliases(range_type->bound_type);
            break;
        }
        case Type::Variation::TUPLE: {
            auto *tuple_type = type_to_resolve->as<TupleType>();
            for (auto &type : tuple_type->types) {
                substitute_type_aliases(type);
            }
            break;
        }
        case Type::Variation::UNKNOWN:
            // We do not handle unknown types here
            break;
        case Type::Variation::VARIANT: {
            auto *variant_type = type_to_resolve->as<VariantType>();
            if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                // Is variant type definition
                auto *const var_type = std::get<VariantNode *const>(variant_type->var_or_list);
                for (auto &type : var_type->possible_types) {
                    substitute_type_aliases(type.second);
                }
            } else {
                // Is variant list
                auto &types = std::get<std::vector<std::shared_ptr<Type>>>(variant_type->var_or_list);
                for (auto &type : types) {
                    substitute_type_aliases(type);
                }
            }
            break;
        }
    }
}

std::optional<Parser::CreateCallOrInitializerBaseRet> Parser::create_call_or_initializer_base( //
    const Context &ctx,                                                                        //
    std::shared_ptr<Scope> &scope,                                                             //
    const token_slice &tokens,                                                                 //
    const Namespace *call_namespace,                                                           //
    const bool is_func_module_call                                                             //
) {
    PROFILE_CUMULATIVE("Parser::create_call_or_initializer_base");
    using types = std::vector<std::shared_ptr<Type>>;
    assert(tokens.first->token == TOK_TYPE || tokens.first->token == TOK_IDENTIFIER);
    std::optional<uint2> arg_range = Matcher::balanced_range_extraction(        //
        tokens, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
    );
    std::string func_module_name = "";
    if (is_func_module_call) {
        assert(tokens.first->token == TOK_TYPE);
        assert(tokens.first->type->get_variation() == Type::Variation::FUNC);
        assert((tokens.first + 1)->token == TOK_DOT);
        assert((tokens.first + 2)->token == TOK_IDENTIFIER);
        func_module_name = std::string(tokens.first->type->to_string());
    }
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
        Context local_ctx = ctx;
        local_ctx.level = ContextLevel::UNKNOWN;
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
                auto expression = create_expression(local_ctx, scope, argument_tokens);
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
            auto expression = create_expression(local_ctx, scope, argument_tokens);
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
    const auto &name_token = is_func_module_call ? tokens.first + 2 : tokens.first;
    if (name_token->token == TOK_TYPE) {
        switch (tokens.first->type->get_variation()) {
            default: {
                [[maybe_unused]] const Type::Variation variation = name_token->type->get_variation();
                [[maybe_unused]] const std::string type_str = name_token->type->to_string();
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            case Type::Variation::DATA: {
                const DataNode *data_node = name_token->type->as<DataType>()->data_node;
                auto &fields = data_node->fields;
                // Now check if the initializer arguments are equal to the expected initializer fields
                if (fields.size() != arguments.size()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                for (size_t i = 0; i < arguments.size(); i++) {
                    std::shared_ptr<Type> arg_type = arguments[i].first->type;
                    if (arg_type->get_variation() == Type::Variation::GROUP) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    const auto &field_type = std::get<1>(fields.at(i));
                    if (!check_castability(field_type, arguments.at(i).first, false)) {
                        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, field_type, arg_type);
                        return std::nullopt;
                    }
                }
                return CreateCallOrInitializerBaseRet{
                    .args = std::move(arguments),
                    .type = name_token->type,
                    .is_initializer = true,
                    .function = nullptr,
                };
            }
            case Type::Variation::MULTI: {
                const auto *multi_type = name_token->type->as<MultiType>();
                const std::shared_ptr<Type> base_type = multi_type->base_type;
                const unsigned int width = multi_type->width;
                if (arguments.size() != width) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                for (size_t i = 0; i < arguments.size(); i++) {
                    if (!check_castability(base_type, arguments[i].first, false)) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                }
                return CreateCallOrInitializerBaseRet{
                    .args = std::move(arguments),
                    .type = name_token->type,
                    .is_initializer = true,
                    .function = nullptr,
                };
            }
        }
    }

    // It's definitely a call
    std::string function_name;
    for (auto tok = tokens.first; tok != tokens.second; ++tok) {
        // Get the function name
        if (tok->token == TOK_IDENTIFIER) {
            if (is_func_module_call) {
                function_name = tokens.first->type->to_string() + "." + std::string(tok->lexme);
            } else {
                function_name = tok->lexme;
            }
            break;
        } else if (std::distance(tokens.first, tok) == arg_range.value().first) {
            // Function with no name
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }

    // Get the function from it's name and the argument types
    // It's not a builtin call, so we need to get the function from it's name
    const bool is_aliased = call_namespace->namespace_hash.to_string() != file_node_ptr->file_namespace->namespace_hash.to_string();
    auto functions = call_namespace->get_functions_from_call_types(function_name, argument_types, is_aliased);
    if (functions.empty()) {
        THROW_ERR(ErrExprCallOfUndefinedFunction, ERR_PARSING, file_hash, tokens, function_name, argument_types);
        return std::nullopt;
    }
    if (functions.size() > 1) {
        // If there are multiple potential functions available this means that the arguments are implicitely castable to the argument types
        // of a few functions. In this case we check if there is an overload of the function which matches our argument types *exactly*. If
        // no function matches exactly then it's an error since the call is ambiguous and we cannot tell for sure which version to call.
        FunctionNode *exact_function = nullptr;
        for (FunctionNode *fn : functions) {
            bool all_match = true;
            for (size_t i = 0; i < fn->parameters.size(); i++) {
                const auto &param_type = std::get<0>(fn->parameters.at(i));
                if (!param_type->equals(argument_types.at(i))) {
                    all_match = false;
                    break;
                }
            }
            if (all_match) {
                if (exact_function != nullptr) {
                    // We found two exact functions, which is an error too
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                exact_function = fn;
            }
        }

        if (exact_function == nullptr) {
            // Ambigue call to function, could be one of multiple called functions
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        functions.clear();
        functions.emplace_back(exact_function);
    }
    // Check if the argument count does match the parameter count
    [[maybe_unused]] const unsigned int param_count = functions.front()->parameters.size();
    [[maybe_unused]] const unsigned int arg_count = arguments.size();
    // Argument counts are guaranteed to match the param count because if they would not, the `get_functions_from_call_types` function would
    // have returned `an empty list`
    assert(param_count == arg_count);
    // If we came until here, the argument types definitely match the function parameter types, or they can be cast to them (in the literal
    // case), otherwise no function would have been found
    // Lastly, update the arguments of the call with the information of the function definition, if the arguments should be references Every
    // non-primitive type is always a reference (except enum types, for now)
    // We also check if the argument type is of type 'int' or 'float' and simply change it to the function parameter type directly
    for (size_t i = 0; i < arguments.size(); i++) {
        const std::string arg_str = arguments[i].first->type->to_string();
        if (arg_str == "int" || arg_str == "float") {
            // Set the type of the argument to the expected parameter type, since we know it's compatible, otherwise the function would
            // never been suggested as a possible function to call in the first place
            arguments[i].first->type = std::get<0>(functions.front()->parameters[i]);
        }
        if (arguments[i].first->type->get_variation() == Type::Variation::ENUM) {
            arguments[i].second = false;
        } else {
            arguments[i].second = primitives.find(arguments[i].first->type->to_string()) == primitives.end();
        }
        // Also, we check here if the variable is immutable but the function expects an mutable reference instead
        if (arguments[i].second) {
            // Its a complex data type, so its a reference
            auto tok = tokens.first + arg_range->first;
            // Now we need to get until the token where the error happened, e.g. the ith argument
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
            if (arguments[i].first->get_variation() == ExpressionNode::Variation::VARIABLE) {
                const auto *variable_node = arguments[i].first->as<VariableNode>();
                if (!std::get<2>(scope->variables.at(variable_node->name)) && std::get<2>(functions.front()->parameters[i])) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_hash, tok->line, tok->column, variable_node->name);
                    return std::nullopt;
                }
            }
        }
    }

    types return_types = functions.front()->return_types;
    std::shared_ptr<Type> return_type;
    if (return_types.empty()) {
        return_type = Type::get_primitive_type("void");
    } else if (return_types.size() > 1) {
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(return_types);
        if (!file_node_ptr->file_namespace->add_type(group_type)) {
            group_type = file_node_ptr->file_namespace->get_type_from_str(group_type->to_string()).value();
        }
        return_type = group_type;
    } else {
        return_type = return_types.front();
    }
    return CreateCallOrInitializerBaseRet{
        .args = std::move(arguments),
        .type = return_type,
        .is_initializer = false,
        .function = functions.front(),
    };
}

std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> Parser::create_unary_op_base( //
    const Context &ctx,                                                                               //
    std::shared_ptr<Scope> &scope,                                                                    //
    const token_slice &tokens                                                                         //
) {
    PROFILE_CUMULATIVE("Parser::create_unary_op_base");
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
    auto expression = create_expression(ctx, scope, tokens_mut);
    if (!expression.has_value()) {
        return std::nullopt;
    }

    return std::make_tuple(operator_token, std::move(expression.value()), is_left);
}

std::optional<std::tuple<std::unique_ptr<ExpressionNode>, std::optional<std::string>, unsigned int, std::shared_ptr<Type>>>
Parser::create_field_access_base(     //
    const Context &ctx,               //
    std::shared_ptr<Scope> &scope,    //
    const token_slice &tokens,        //
    const bool has_inbetween_operator //
) {
    PROFILE_CUMULATIVE("Parser::creaet_field_access_base");
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
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(ctx, scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::shared_ptr<Type> base_type = base_expr.value()->type;
    if (has_inbetween_operator) {
        const auto *optional_type = base_type->as<OptionalType>();
        base_type = optional_type->base_type;
    }

    // If the base expresion is of type `str`, the only valid access is its `length` variable
    if (base_type->to_string() == "str") {
        if (field_name != "length" && field_name != "len") {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_tuple(std::move(base_expr.value()), field_name, 0, Type::get_primitive_type("u64"));
    }
    switch (base_type->get_variation()) {
        default:
            break;
        case Type::Variation::ARRAY: {
            const auto *array_type = base_type->as<ArrayType>();
            if (field_name != "length" && field_name != "len") {
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
                if (!file_node_ptr->file_namespace->add_type(group_type)) {
                    group_type = file_node_ptr->file_namespace->get_type_from_str(group_type->to_string()).value();
                }
                return std::make_tuple(std::move(base_expr.value()), field_name, 1, group_type);
            }
            return std::make_tuple(std::move(base_expr.value()), field_name, 1, Type::get_primitive_type("u64"));
        }
        case Type::Variation::DATA: {
            const DataNode *data_node = base_type->as<DataType>()->data_node;
            // Check if the given field name exists in the data type
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
        }
        case Type::Variation::PRIMITIVE:
            if (base_type->to_string() != "anyerror") {
                break;
            }
            [[fallthrough]];
        case Type::Variation::ERROR_SET:
            if (field_name == "type_id") {
                return std::make_tuple(std::move(base_expr.value()), field_name, 0, Type::get_primitive_type("u32"));
            } else if (field_name == "value_id") {
                return std::make_tuple(std::move(base_expr.value()), field_name, 1, Type::get_primitive_type("u32"));
            } else if (field_name == "message") {
                return std::make_tuple(std::move(base_expr.value()), field_name, 2, Type::get_primitive_type("str"));
            }
            break;
        case Type::Variation::MULTI: {
            const auto *multi_type = base_type->as<MultiType>();
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
        }
        case Type::Variation::TUPLE: {
            const auto *tuple_type = base_type->as<TupleType>();
            if (field_id >= tuple_type->types.size()) {
                auto it = base_expr_tokens.second + 1; // 'it' now is the $ symbol
                THROW_ERR(ErrExprTupleAccessOOB, ERR_PARSING, file_hash, it->line, it->column, "$" + std::to_string(field_id), base_type);
                return std::nullopt;
            }
            const std::shared_ptr<Type> field_type = tuple_type->types.at(field_id);
            return std::make_tuple(std::move(base_expr.value()), std::nullopt, field_id, field_type);
        }
        case Type::Variation::VARIANT:
            if (field_name == "active_type") {
                return std::make_tuple(std::move(base_expr.value()), field_name, 0, Type::get_primitive_type("u8"));
            }
            break;
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return std::nullopt;
}

std::optional<std::tuple<std::string, unsigned int>> Parser::create_multi_type_access( //
    const MultiType *multi_type,                                                       //
    const std::string &field_name                                                      //
) {
    PROFILE_CUMULATIVE("Parser::create_multi_type_access");
    if (multi_type->width == 2) {
        // The fields are called x and y, but can be accessed via $N
        if (field_name == "x" || field_name == "s" || field_name == "i" || field_name == "u" || field_name == "$0") {
            return std::make_tuple("$0", 0);
        } else if (field_name == "y" || field_name == "t" || field_name == "j" || field_name == "v" || field_name == "$1") {
            return std::make_tuple("$1", 1);
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    } else if (multi_type->width == 3) {
        // The fields are called x, y and z, but can be accessed via $N
        if (field_name == "x" || field_name == "r" || field_name == "s" || field_name == "i" || field_name == "$0") {
            return std::make_tuple("$0", 0);
        } else if (field_name == "y" || field_name == "g" || field_name == "t" || field_name == "j" || field_name == "$1") {
            return std::make_tuple("$1", 1);
        } else if (field_name == "z" || field_name == "b" || field_name == "p" || field_name == "k" || field_name == "$2") {
            return std::make_tuple("$2", 2);
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    } else if (multi_type->width == 4) {
        // The fields are called r, g, b and a, but can be accessed via $N
        if (field_name == "x" || field_name == "r" || field_name == "s" || field_name == "i" || field_name == "$0") {
            return std::make_tuple("$0", 0);
        } else if (field_name == "y" || field_name == "g" || field_name == "t" || field_name == "j" || field_name == "$1") {
            return std::make_tuple("$1", 1);
        } else if (field_name == "z" || field_name == "b" || field_name == "p" || field_name == "k" || field_name == "$2") {
            return std::make_tuple("$2", 2);
        } else if (field_name == "w" || field_name == "a" || field_name == "q" || field_name == "l" || field_name == "$3") {
            return std::make_tuple("$3", 3);
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    } else {
        // Widths of 16 are not supported by Flint yet
        assert(multi_type->width == 8);
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
    const Context &ctx,               //
    std::shared_ptr<Scope> &scope,    //
    const token_slice &tokens,        //
    const bool has_inbetween_operator //
) {
    PROFILE_CUMULATIVE("Parser::create_grouped_access_base");
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
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(ctx, scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::shared_ptr<Type> base_type = base_expr.value()->type;
    if (has_inbetween_operator) {
        const auto *optional_type = base_type->as<OptionalType>();
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

    switch (base_type->get_variation()) {
        default:
            break;
        case Type::Variation::DATA: {
            const DataNode *data_node = base_type->as<DataType>()->data_node;
            // Get the field types and field ids from the data node
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
        }
        case Type::Variation::MULTI: {
            const auto *multi_type = base_type->as<MultiType>();
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
        }
        case Type::Variation::TUPLE: {
            const auto *tuple_type = base_type->as<TupleType>();
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
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return std::nullopt;
}

bool Parser::ensure_castability_multiple(                      //
    const std::shared_ptr<Type> &to_type,                      //
    std::vector<std::unique_ptr<ExpressionNode>> &expressions, //
    const token_slice &tokens                                  //
) {
    PROFILE_CUMULATIVE("Parser::ensure_castability_multiple");
    // Every expression in the length expressions needs to be castable a `u64` type, if it's not of that type already we need to cast it
    for (auto &expr : expressions) {
        if (expr->type == to_type) {
            continue;
        }
        const std::string type_str = expr->type->to_string();
        if (type_str == "int" || type_str == "float") {
            expr->type = to_type;
            continue;
        }
        if (expr->type->get_variation() == Type::Variation::RANGE) {
            continue;
        }
        const CastDirection castability = check_primitive_castability(expr->type, to_type);
        switch (castability.kind) {
            default: {
                const size_t n = expressions.size();
                if (n > 1) {
                    const std::vector<std::shared_ptr<Type>> types(n, to_type);
                    const std::shared_ptr<Type> group_type = std::make_shared<GroupType>(types);
                    const std::shared_ptr<Type> expected_type;
                    THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, group_type, expected_type);
                    return false;
                }
                THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, to_type, expr->type);
                return false;
            }
            case CastDirection::Kind::CAST_LHS_TO_RHS:
                expr = std::make_unique<TypeCastNode>(to_type, expr);
                break;
        }
    }
    return true;
}

bool Parser::add_annotation(const token_slice &tokens) {
    assert(tokens.first->token == TOK_ANNOTATION);
    assert((tokens.first + 1)->token == TOK_IDENTIFIER);
    const std::string annotation_name((tokens.first + 1)->lexme);

    if (annotation_map.find(annotation_name) == annotation_map.end()) {
        // Unknown annotation
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    const AnnotationKind kind = annotation_map.at(annotation_name);
    // Check if the queue already contains this annotation kind, it is not allowed to define the same annotation twice
    for (const auto &annotation : annotation_queue) {
        if (annotation.kind == kind) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
    }

    // Parse the arguments of the annotation, if there are any
    std::vector<std::string> arguments;
    auto it = tokens.first + 2;
    if (it != tokens.second) {
        // Arguments will follow
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return false;
    }

    annotation_queue.emplace_back(kind, arguments);
    return true;
}

bool Parser::enusure_no_annotation_leftovers() {
    if (annotation_queue.empty()) {
        return true;
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return false;
}
