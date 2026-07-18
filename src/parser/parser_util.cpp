#include "analyzer/analyzer.hpp"
#include "debug.hpp"
#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/builtins.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/parser.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/interface_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/opaque_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include "profiler.hpp"
#include "single_executor_guard.hpp"

#include <algorithm>

bool Parser::add_next_main_node(FileNode &file_node, token_slice &tokens) {
    PROFILE_CUMULATIVE("Parser::add_next_main_node");
    token_slice definition_tokens = get_definition_tokens(tokens);
    tokens.first = definition_tokens.second;
    // Make sure that `tokens.first` does not point at the `end` iterator because it's content is undefined
    if (tokens.first == file_node_ptr->tokens.end()) {
        tokens.first = std::prev(tokens.first);
        ASSERT(tokens.first->token == TOK_EOF);
    }
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
            THROW_ERR(ErrImportNotAtTopLevel, ERR_PARSING, file_hash, definition_tokens);
            return false;
        }
        std::optional<ImportNode> import_node = create_import(definition_tokens);
        if (!import_node.has_value()) {
            return false;
        }
        for (const auto &imported_file : imported_files) {
            if (imported_file->path == import_node.value().path) {
                THROW_ERR(ErrImportSameFileTwice, ERR_PARSING, file_hash, &import_node.value());
                return false;
            }
        }
        // Check if the file exists if it's a file import
        if (std::holds_alternative<Hash>(import_node.value().path)) {
            if (!std::filesystem::exists(std::get<Hash>(import_node.value().path).path)) {
                THROW_ERR(ErrImportNonexistentFile, ERR_PARSING, file_hash, &import_node.value());
                return false;
            }
        }
        // Check if the given alias is already taken
        auto &aliased_imports = file_node_ptr->file_namespace->public_symbols.aliased_imports;
        if (import_node.value().alias.has_value() && aliased_imports.find(import_node.value().alias.value()) != aliased_imports.end()) {
            const ImportNode *aliased_import = nullptr;
            for (const auto &import : file_node_ptr->file_namespace->public_symbols.imports) {
                if (import->alias.has_value() && import->alias.value() == import_node.value().alias.value()) {
                    aliased_import = import.get();
                }
            }
            ASSERT(aliased_import != nullptr);
            THROW_ERR(                                                                                                         //
                ErrImportDuplicateAlias, ERR_PARSING, file_hash,                                                               //
                import_node->line, import_node->column, import_node->length, import_node.value().alias.value(), aliased_import //
            );
            return false;
        }
        if (std::holds_alternative<std::vector<std::string>>(import_node.value().path)) {
            const std::vector<std::string> &import_vec = std::get<std::vector<std::string>>(import_node.value().path);
            if (import_vec.size() == 2 && import_vec.front() == "Core") {
                // Check for imported core modules
                const std::string &module_str = import_vec.back();
                if (core_module_functions.find(module_str) == core_module_functions.end()) {
                    const auto &tok = definition_tokens.first + 3;
                    THROW_ERR(ErrImportUnexpectedCoreModule, ERR_PARSING, file_hash, tok->line, tok->column, module_str);
                    return false;
                }
            } else if (import_vec.size() == 2 && import_vec.front() == "Fip") {
                if (!FIP::resolve_module_import(&import_node.value())) {
                    return false;
                }
            }
        }
        std::optional<ImportNode *> added_import = file_node.add_import(import_node.value());
        if (!added_import.has_value()) {
            return false;
        }
        if (added_import.value()->alias.has_value()) {
            ASSERT(aliased_imports.find(added_import.value()->alias.value()) == aliased_imports.end());
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
        ASSERT(definition_tokens.first->token == TOK_TYPE_KEYWORD);
        ASSERT((definition_tokens.first + 1)->token == TOK_IDENTIFIER);
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
        std::optional<FunctionNode *> added_function = file_node.add_function(function_node.value(), core_namespaces);
        if (!added_function.has_value()) {
            return false;
        }
        add_open_function({added_function.value(), {}});
        return true;
    } else if (Matcher::tokens_match(definition_tokens, Matcher::opaque_definition)) {
        const bool is_opaque_keyword = definition_tokens.first->token == TOK_OPAQUE;
        const bool is_opaque_type = definition_tokens.first->token == TOK_TYPE           //
            && definition_tokens.first->type->get_variation() == Type::Variation::OPAQUE //
            && !definition_tokens.first->type->as<OpaqueType>()->name.has_value();
        if (is_opaque_keyword || is_opaque_type) {
            ASSERT(std::next(definition_tokens.first)->token == TOK_IDENTIFIER);
            const std::string opaque_type_name(std::next(definition_tokens.first)->lexme);
            std::shared_ptr<Type> opaque_type = std::make_shared<OpaqueType>(opaque_type_name, file_node_ptr->file_hash);
            if (!file_node_ptr->file_namespace->add_type(opaque_type)) {
                // Duplicate opaque type definition
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            return true;
        }
    }

    std::vector<Line> body_lines = get_body_lines(definition_indentation, tokens);
    if (body_lines.empty()) {
        // TODO: This prints the same error twice which is... not pretty
        // For example when writing `def main():` and the `print("Hello\n");` in the line below the error first is printed for the `def
        // main():` line and then again for the `print("Hello\n");` line. We somehow need to print this error only if the current line we
        // are at is valid at all, e.g. if it *should* have a body. In all other cases we should print a "Hey idk what the hell that line
        // is" error (for example with the print line at top-level)
        THROW_ERR(ErrMissingBody, ERR_PARSING, file_hash, tokens);
        return false;
    }
    if (Matcher::tokens_contain(definition_tokens, Matcher::function_definition)) {
        // Dont actually parse the function body, only its definition
        std::optional<FunctionNode> function_node = create_function(definition_tokens, {});
        if (!function_node.has_value()) {
            return false;
        }
        std::optional<FunctionNode *> added_function = file_node.add_function(function_node.value(), core_namespaces);
        if (!added_function.has_value()) {
            return false;
        }
        add_open_function({added_function.value(), body_lines});
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::data_definition)) {
        std::optional<DataNode> data_node = create_data(definition_tokens, body_lines);
        if (!data_node.has_value()) {
            return false;
        }
        std::optional<DataNode *> added_data = file_node.add_data(data_node.value());
        if (!added_data.has_value()) {
            return false;
        }
        add_open_data(added_data.value());
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::func_definition)) {
        std::optional<FuncNode> func_node = create_func(definition_tokens, body_lines);
        if (!func_node.has_value()) {
            return false;
        }
        std::optional<FuncNode *> added_func = file_node.add_func(func_node.value());
        if (!added_func.has_value()) {
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::interface_definition)) {
        std::optional<InterfaceNode> interface_node = create_interface(definition_tokens, body_lines);
        if (!interface_node.has_value()) {
            return false;
        }
        std::optional<InterfaceNode *> added_interface = file_node.add_interface(interface_node.value());
        if (!added_interface.has_value()) {
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::object_definition)) {
        std::optional<ObjectNode> object_node = create_object(definition_tokens, body_lines);
        if (!object_node.has_value()) {
            return false;
        }
        std::optional<ObjectNode *> added_object = file_node.add_object(object_node.value());
        if (!added_object.has_value()) {
            return false;
        }
        add_open_object({added_object.value(), body_lines});
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::enum_definition)) {
        std::optional<EnumNode> enum_node = create_enum(definition_tokens, body_lines);
        if (!enum_node.has_value()) {
            return false;
        }
        if (!file_node.add_enum(enum_node.value())) {
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::error_definition)) {
        std::optional<ErrorNode> error_node = create_error(definition_tokens, body_lines);
        if (!error_node.has_value()) {
            return false;
        }
        if (!file_node.add_error(error_node.value())) {
            return false;
        }
    } else if (Matcher::tokens_contain(definition_tokens, Matcher::variant_definition)) {
        std::optional<VariantNode> variant_node = create_variant(definition_tokens, body_lines);
        if (!variant_node.has_value()) {
            return false;
        }
        if (!file_node.add_variant(variant_node.value())) {
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
        THROW_ERR(ErrUnexpectedDefinition, ERR_PARSING, file_hash, definition_tokens);
        return false;
    }
    if (!annotation_queue.empty()) {
        DefinitionNode *last_definition = file_node.file_namespace->public_symbols.definitions.back().get();
        THROW_ERR(                                                                                    //
            ErrAnnoLeftover, ERR_PARSING, file_hash,                                                  //
            last_definition->line, last_definition->column, last_definition->length, annotation_queue //
        );
        return false;
    }
    return true;
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
            tokens.first = it;
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

    return body_lines;
}

void Parser::collapse_types_in_slice(token_slice &slice, token_list &source) {
    ASSERT_ST
    for (auto it = slice.first; it != slice.second;) {
        // Erase all indentations within a line, which are not at the beginning of a line
        if (it->token == TOK_INDENT) {
            Line::delete_tokens(source, it, 1);
            continue;
        }
        // Check if the next token will definitely be not the begin of a type, like commas or a lot of other tokens. In that case no
        // expensive matching logic needs to be run, so we can safely skip that token entirely
        if (it->token != TOK_TYPE && it->token != TOK_DATA && it->token != TOK_VARIANT && it->token != TOK_FN &&
            it->token != TOK_IDENTIFIER) {
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
        if (Matcher::tokens_start_with(token_slice{it, slice.second}, Matcher::type)) {
            // It's a type token
            std::optional<uint2> type_range = Matcher::get_next_match_range(token_slice{it, slice.second}, Matcher::type);
            ASSERT(type_range.has_value());
            ASSERT(type_range.value().first == 0);
            if (type_range.value().second == 1) {
                // It's a primitive / simple type. Such types definitely need to exist already, so if it does not exists it's a regular
                // identifier. And if this token is already a type it means its a primitive type, so we can skip it as well
                if (it->token != TOK_TYPE) {
                    // Types of size 1 always need to be an identifier if they are not already a type (primitives)
                    ASSERT(it->token == TOK_IDENTIFIER);
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

void Parser::collapse_types_in_lines(std::vector<Line> &lines, token_list &source) {
    PROFILE_CUMULATIVE("Parser::collapse_types_in_lines");
    for (auto line : lines) {
        collapse_types_in_slice(line.tokens, source);
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
        case Type::Variation::OBJECT:
            // Object types resolve in a different stage
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
        case Type::Variation::FN:
            // Fn types resolve in a different stage
            break;
        case Type::Variation::GROUP: {
            auto *group_type = type_to_resolve->as<GroupType>();
            for (auto &type : group_type->types) {
                substitute_type_aliases(type);
            }
            break;
        }
        case Type::Variation::INTERFACE:
            // Interface types resolve in a different stage
            break;
        case Type::Variation::MULTI:
            // Multi types cannot contain type aliases
            break;
        case Type::Variation::OPAQUE:
            // Opaque types cannot contain other types
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
    const bool is_typed_call                                                                   //
) {
    PROFILE_CUMULATIVE("Parser::create_call_or_initializer_base");
    using types = std::vector<std::shared_ptr<Type>>;
    ASSERT(tokens.first->token == TOK_TYPE || tokens.first->token == TOK_IDENTIFIER);
    std::optional<uint2> arg_range = Matcher::balanced_range_extraction(        //
        tokens, Matcher::token(TOK_LEFT_PAREN), Matcher::token(TOK_RIGHT_PAREN) //
    );
    if (is_typed_call) {
        ASSERT(tokens.first->token == TOK_TYPE);
        [[maybe_unused]] const auto &type_var = tokens.first->type->get_variation();
        ASSERT(type_var == Type::Variation::FUNC || type_var == Type::Variation::OBJECT);
        ASSERT((tokens.first + 1)->token == TOK_DOT);
        ASSERT((tokens.first + 2)->token == TOK_IDENTIFIER);
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
        // passed. But the comma must be present at the top-level and not within one of the balanced range groups or inside of array
        // accesses or generics, for example
        const auto match_ranges = Matcher::get_match_ranges_in_range_outside_group(                               //
            tokens, Matcher::token(TOK_COMMA), arg_range.value(), Matcher::balancer_left, Matcher::balancer_right //
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
        auto &arg = arguments[i].first;
        if (arg->type->to_string() == "type.flint.str.lit") {
            arg = std::make_unique<TypeCastNode>(                                                                        //
                file_hash, ASTNode::PosTriple{arg->line, arg->column, arg->length}, Type::get_primitive_type("str"), arg //
            );
        }
        if (arg->type->get_variation() == Type::Variation::ALIAS) {
            argument_types.emplace_back(arg->type->as<AliasType>()->type);
        } else {
            argument_types.emplace_back(arg->type);
        }
    }

    // Check if it's an initializer
    const auto &name_token = is_typed_call ? tokens.first + 2 : tokens.first;
    if (name_token->token == TOK_TYPE) {
        if (name_token->type->get_variation() == Type::Variation::ALIAS) {
            name_token->type = name_token->type->as<AliasType>()->type;
        }
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
                    // If the last argument is a default node then *all* remaining fields will be default-constructed
                    if (                                                                     //
                        arguments.size() > fields.size()                                     //
                        || arguments.empty()                                                 //
                        || arguments.back().first->type->to_string() != "type.flint.default" //
                    ) {
                        THROW_ERR(ErrExprInitializerWrongArgCount, ERR_PARSING, file_hash, tokens, fields.size(), arguments.size());
                        return std::nullopt;
                    }
                    // It's a single default-initializer at the end of the initializer list, so we fill all remaining initializers with the
                    // default initializer
                    ASSERT(arguments.size() >= 1);
                    ASSERT(arguments.back().first->type->to_string() == "type.flint.default");
                    for (size_t i = arguments.size(); i < fields.size(); i++) {
                        arguments.emplace_back(arguments.at(i - 1).first->clone(scope->scope_id), false);
                    }
                }
                for (size_t i = 0; i < arguments.size(); i++) {
                    std::shared_ptr<Type> arg_type = arguments[i].first->type;
                    if (arg_type->to_string() == "type.flint.default") {
                        // We need to insert the default-value of the nth argument of the data type at the place of the arguments before
                        // continuing
                        if (!data_node->fields.at(i).initializer.has_value()) {
                            THROW_ERR(ErrExprDataInitializerMissingDefaultValue, ERR_PARSING, file_hash, tokens, fields.at(i).name);
                            return std::nullopt;
                        }
                        arguments[i].first.reset();
                        arguments[i].first = data_node->fields.at(i).initializer.value()->clone(scope->scope_id);
                        arg_type = arguments[i].first->type;
                    }
                    const auto &field_type = fields.at(i).type;
                    if (!check_castability(field_type, arguments[i].first, false)) {
                        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, field_type, arg_type);
                        return std::nullopt;
                    }
                }
                return CreateCallOrInitializerBaseRet{
                    .args = std::move(arguments),
                    .type = name_token->type,
                    .is_initializer = true,
                    .function = nullptr,
                    .instance_variable = std::nullopt,
                    .callable = std::nullopt,
                };
            }
            case Type::Variation::OBJECT: {
                const auto *object_type = name_token->type->as<ObjectType>();
                auto &data_modules = object_type->object_node->data_modules;
                const auto &constructor_order = object_type->object_node->constructor_order;
                // Check if all initializer arguments are equal to the expected data module types
                if (arguments.size() != data_modules.size()) {
                    THROW_ERR(ErrExprInitializerWrongArgCount, ERR_PARSING, file_hash, tokens, data_modules.size(), arguments.size());
                    return std::nullopt;
                }
                for (size_t i = 0; i < arguments.size(); i++) {
                    const std::shared_ptr<Type> &arg_type = arguments.at(i).first->type;
                    const DataNode *data_node = data_modules.at(constructor_order.at(i)).first;
                    const Namespace *data_namespace = Resolver::get_namespace_from_hash(data_node->file_hash);
                    const std::shared_ptr<Type> data_type = data_namespace->get_type_from_str(data_node->name).value();
                    if (!arg_type->equals(data_type)) {
                        THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, data_type, arg_type);
                        return std::nullopt;
                    }
                }
                return CreateCallOrInitializerBaseRet{
                    .args = std::move(arguments),
                    .type = name_token->type,
                    .is_initializer = true,
                    .function = nullptr,
                    .instance_variable = std::nullopt,
                    .callable = std::nullopt,
                };
            }
            case Type::Variation::MULTI: {
                const auto *multi_type = name_token->type->as<MultiType>();
                const std::shared_ptr<Type> base_type = multi_type->base_type;
                const unsigned int width = multi_type->width;
                if (arguments.size() == 1) {
                    // The "argument" needs to be a compatible multi-type
                    if (arguments[0].first->type->get_variation() != Type::Variation::MULTI) {
                        const auto &arg = arguments[0].first;
                        THROW_ERR(                                                           //
                            ErrExprCastInvalid, ERR_PARSING, file_hash,                      //
                            arg->line, arg->column, arg->length, name_token->type, arg->type //
                        );
                        return std::nullopt;
                    }
                    ASSERT(primitive_casting_table.find(name_token->type->to_string()) != primitive_casting_table.end());
                    [[maybe_unused]] const auto &to_types = primitive_casting_table.at(name_token->type->to_string());
                    ASSERT(std::find(to_types.begin(), to_types.end(), arguments[0].first->type->to_string()) != to_types.end());
                } else {
                    if (arguments.size() != width) {
                        THROW_ERR(ErrExprCastMultiLengthMismatch, ERR_PARSING, file_hash, tokens, width, arguments.size());
                        return std::nullopt;
                    }
                    for (size_t i = 0; i < arguments.size(); i++) {
                        if (!check_castability(base_type, arguments[i].first, false)) {
                            const auto &arg = arguments[i].first;
                            THROW_ERR(                                                           //
                                ErrExprCastInvalid, ERR_PARSING, file_hash,                      //
                                arg->line, arg->column, arg->length, name_token->type, arg->type //
                            );
                            return std::nullopt;
                        }
                    }
                }
                return CreateCallOrInitializerBaseRet{
                    .args = std::move(arguments),
                    .type = name_token->type,
                    .is_initializer = true,
                    .function = nullptr,
                    .instance_variable = std::nullopt,
                    .callable = std::nullopt,
                };
            }
        }
    }

    // If the call starts with `identifier.identifier(` and the first `identifier` is a variable, we then can check if that variable is of
    // type object or func module. If it's an object then it's an instanced call. Func modules will be handled at a later time but they
    // follow the same syntax.
    // The pattern `identifier.identifier` does neither match type-calls like FuncType.call since that's `type.identifier` or aliased calls,
    // as these are `alias.identifier`, so this means that this pattern is unique to instance calls
    const bool is_instance_call = tokens.first->token == TOK_IDENTIFIER //
        && (tokens.first + 1)->token == TOK_DOT                         //
        && (tokens.first + 2)->token == TOK_IDENTIFIER;
    std::optional<std::unique_ptr<ExpressionNode>> instance_variable = std::nullopt;
    auto tok = tokens.first;
    if (is_instance_call || is_typed_call) {
        tok++;
        ASSERT(tok->token == TOK_DOT);
        tok++;
    }

    // It's definitely a call
    ASSERT(tok->token == TOK_IDENTIFIER);
    const std::string function_name = is_typed_call                       //
        ? tokens.first->type->to_string() + "." + std::string(tok->lexme) //
        : std::string(tok->lexme);

    // Get the function from it's name and the argument types
    // It's not a builtin call, so we need to get the function from it's name
    const bool is_aliased = call_namespace->namespace_hash.to_string() != file_node_ptr->file_namespace->namespace_hash.to_string();
    std::vector<std::pair<FunctionNode *, size_t>> functions;
    std::unordered_set<const FuncNode *> func_nodes;
    if (is_instance_call) {
        std::optional<std::unique_ptr<ExpressionNode>> variable_node = create_variable(scope, {tokens.first, tokens.first + 1});
        if (!variable_node.has_value()) {
            return std::nullopt;
        }
        // Get the type of the variable next
        const auto &var_type = variable_node.value()->type;
        switch (var_type->get_variation()) {
            default:
                THROW_ERR(ErrExprCallOnWrongInstanceType, ERR_PARSING, file_hash, tokens);
                return std::nullopt;
            case Type::Variation::OBJECT: {
                const ObjectNode *object_node = var_type->as<ObjectType>()->object_node;
                for (const auto &func_module : object_node->func_modules) {
                    for (const auto &function : func_module->functions) {
                        // Remove the 'FuncType.' from the function's name to get the "actual" name of the function
                        const std::string fn_name = function->name.substr(func_module->name.size() + 1);
                        if (fn_name != function_name) {
                            continue;
                        }
                        const size_t implicit_param_count = func_module->required_data.size();
                        if (function->parameters.size() - implicit_param_count != argument_types.size()) {
                            // Wrong arg count
                            continue;
                        }
                        functions.emplace_back(function, implicit_param_count);
                        func_nodes.emplace(func_module);
                    }
                }
                // Search in the free-floating functions whether this is the function we call
                for (const auto &function : object_node->functions) {
                    // Remove the 'ObjectType.' from the function's name to get the "actual" name of the function
                    const std::string fn_name = function->name.substr(object_node->name.size() + 1);
                    if (fn_name != function_name) {
                        continue;
                    }
                    if (function->parameters.size() - 1 != argument_types.size()) {
                        // Wrong arg count
                        continue;
                    }
                    functions.emplace_back(function, 1);
                }
                break;
            }
            case Type::Variation::FUNC: {
                const FuncNode *func_node = var_type->as<FuncType>()->func_node;
                for (const auto &function : func_node->functions) {
                    // Remove the 'FuncType.' from the function's name to get the "actual" name of the function
                    const std::string fn_name = function->name.substr(func_node->name.size() + 1);
                    if (fn_name != function_name) {
                        continue;
                    }
                    const size_t implicit_param_count = func_node->required_data.size();
                    if (function->parameters.size() - implicit_param_count != argument_types.size()) {
                        // Wrong arg count
                        continue;
                    }
                    functions.emplace_back(function, implicit_param_count);
                    func_nodes.emplace(func_node);
                }
                break;
            }
            case Type::Variation::INTERFACE: {
                const InterfaceNode *interface_node = var_type->as<InterfaceType>()->interface_node;
                for (const auto &function : interface_node->functions) {
                    // Remove the 'InterfaceType.' from the function's name to get the "actual" name of the function
                    const std::string fn_name = function->name.substr(interface_node->name.size() + 1);
                    if (fn_name != function_name) {
                        continue;
                    }
                    if (function->parameters.size() != argument_types.size()) {
                        // Wrong arg count
                        continue;
                    }
                    functions.emplace_back(function, 0);
                }
                break;
            }
        }
        instance_variable = std::move(variable_node.value());
    } else {
        auto fns = call_namespace->get_functions_from_call_types(function_name, argument_types, is_aliased);
        for (FunctionNode *fn : fns) {
            functions.emplace_back(fn, 0);
        }
    }
    if (functions.empty()) {
        // Check if the called function is a callable by looking in this scope for a variable wihth the same name as the called function
        std::vector<std::pair<std::string, Scope::Variable>> potential_callables;
        for (const auto &[variable_name, variable] : scope->get_all_variables()) {
            if (variable_name != function_name) {
                continue;
            }
            if (variable.type->get_variation() != Type::Variation::FN) {
                continue;
            }
            const auto *fn_type = variable.type->as<FnType>();
            if (fn_type->params.size() != argument_types.size()) {
                continue;
            }
            bool types_match = true;
            for (size_t i = 0; i < argument_types.size(); i++) {
                const auto &param_type = fn_type->params.at(i).first;
                ASSERT(param_type->get_variation() != Type::Variation::ALIAS);
                const auto &arg_type = argument_types.at(i);
                ASSERT(arg_type->get_variation() != Type::Variation::ALIAS);
                if (param_type->equals(arg_type)) {
                    continue;
                }
                // Check if argument can be implicitly cast to parameter type
                if (!check_castability(param_type, arguments.at(i).first)) {
                    types_match = false;
                    break;
                }
            }
            if (types_match) {
                potential_callables.emplace_back(variable_name, variable);
            }
            // Shadowing is not allowed in Flint, which means that the first callable variable with the correct name must be the called
            // callable, as there exists no other callable with the same name
            break;
        }
        if (!potential_callables.empty()) {
            // There must be exactly one potential callable because of Flint's shadowing rules and the code above
            ASSERT(potential_callables.size() == 1);
            const auto &callable_var = potential_callables.front().second;
            const auto *fn_type = callable_var.type->as<FnType>();
            for (size_t i = 0; i < arguments.size(); i++) {
                // Set the 'is_reference' flag of the arguments
                arguments[i].second = arguments[i].first->type->is_reference();
            }
            std::shared_ptr<Type> return_type;
            if (fn_type->return_types.empty()) {
                return_type = Type::get_primitive_type("void");
            } else if (fn_type->return_types.size() == 1) {
                return_type = fn_type->return_types.front();
            } else {
                return_type = std::make_shared<GroupType>(fn_type->return_types);
                if (!file_node_ptr->file_namespace->add_type(return_type)) {
                    return_type = file_node_ptr->file_namespace->get_type_from_str(return_type->to_string()).value();
                }
            }
            return CreateCallOrInitializerBaseRet{
                .args = std::move(arguments),
                .type = return_type,
                .is_initializer = false,
                .function = nullptr,
                .instance_variable = std::nullopt,
                .callable = potential_callables.front().first,
            };
        }
    }
    if (functions.empty()) {
        THROW_ERR(ErrExprCallOfUndefinedFunction, ERR_PARSING, file_hash, tokens, function_name, argument_types);
        return std::nullopt;
    }
    if (functions.size() > 1) {
        // If there are multiple potential functions available this means that the arguments are implicitely castable to the argument types
        // of a few functions. In this case we check if there is an overload of the function which matches our argument types *exactly*. If
        // no function matches exactly then it's an error since the call is ambiguous and we cannot tell for sure which version to call.
        FunctionNode *exact_function = nullptr;
        size_t implicit_param_count;
        for (auto &fn : functions) {
            bool all_match = true;
            for (size_t i = 0; i < argument_types.size(); i++) {
                const auto &param_type = std::get<0>(fn.first->parameters.at(i + fn.second));
                if (!param_type->equals(argument_types.at(i))) {
                    all_match = false;
                    break;
                }
            }
            if (all_match) {
                ASSERT(exact_function == nullptr);
                exact_function = fn.first;
                implicit_param_count = fn.second;
            }
        }

        if (exact_function == nullptr) {
            THROW_ERR(ErrExprCallAmbiguous, ERR_PARSING, file_hash, tokens, functions);
            return std::nullopt;
        }
        functions.clear();
        functions.emplace_back(exact_function, implicit_param_count);
    }
    // If it's an instanced function we need to add the implicit arguments of the called function to the arguments of the call. The implicit
    // parameters are "field accesses" of the objects data fields
    // Object field accesses are not allowed by the user, but the compiler can create them just fine. They are prevented at the parsing
    // stage, but permitted at the codegen stage
    size_t arg_start_id = 0;
    if (is_instance_call) {
        ASSERT(instance_variable.has_value());
        switch (instance_variable.value()->type->get_variation()) {
            default:
                THROW_ERR(ErrExprCallOnWrongInstanceType, ERR_PARSING, file_hash, tokens);
                return std::nullopt;
            case Type::Variation::OBJECT: {
                if (func_nodes.empty()) {
                    // It's an instance call of a free-floating object function
                    std::unique_ptr<ExpressionNode> object_variable = std::make_unique<VariableNode>( //
                        file_hash,                                                                    //
                        get_pos_triple(token_slice{tokens.first, tokens.first + 1}),                  //
                        instance_variable.value()->as<VariableNode>()->name,                          //
                        instance_variable.value()->type,                                              //
                        instance_variable.value()->is_const                                           //
                    );
                    arguments.insert(arguments.begin(), std::make_pair(std::move(object_variable), true));
                    argument_types.insert(argument_types.begin(), instance_variable.value()->type);
                    arg_start_id++;
                    break;
                }
                ASSERT(func_nodes.size() == 1);
                const FuncNode *func_node = *func_nodes.begin();
                const ObjectNode *object_node = instance_variable.value()->type->as<ObjectType>()->object_node;
                for (size_t i = func_node->required_data.size(); i > 0; i--) {
                    // Get the index of the required data in the object
                    const auto &required_data_type = func_node->required_data.at(i - 1).type;
                    const DataNode *required_data_node = required_data_type->as<DataType>()->data_node;
                    size_t idx = 0;
                    for (const auto &[data_node, accessor] : object_node->data_modules) {
                        if (data_node == required_data_node) {
                            break;
                        }
                        idx++;
                    }
                    // Since this is an instance-call and the function thus comes from a func-module, the object is guaranteed to contain
                    // the required data here, if not something would have gone horribly wrong earlier in the object parsing stage.
                    ASSERT(idx != object_node->data_modules.size());
                    std::unique_ptr<ExpressionNode> base_expr = std::make_unique<VariableNode>( //
                        file_hash,                                                              //
                        get_pos_triple(token_slice{tokens.first, tokens.first + 1}),            //
                        instance_variable.value()->as<VariableNode>()->name,                    //
                        instance_variable.value()->type,                                        //
                        instance_variable.value()->is_const                                     //
                    );
                    std::unique_ptr<ExpressionNode> argument = std::make_unique<DataAccessNode>( //
                        file_hash,                                                               //
                        get_pos_triple(token_slice{tokens.first, tokens.first + 1}),             //
                        base_expr,                                                               //
                        std::nullopt,                                                            // Object fields have no name
                        idx,               // The index of the data in the object struct
                        required_data_type //
                    );
                    arguments.insert(arguments.begin(), std::make_pair(std::move(argument), true));
                    argument_types.insert(argument_types.begin(), required_data_type);
                    arg_start_id++;
                }
                break;
            }
            case Type::Variation::FUNC: {
                const FuncNode *func_node = *func_nodes.begin();
                for (size_t i = func_node->required_data.size(); i > 0; i--) {
                    // The indices of the required data are the same as the func module itself
                    const auto &required_data_type = func_node->required_data.at(i - 1).type;
                    std::unique_ptr<ExpressionNode> base_expr = std::make_unique<VariableNode>( //
                        file_hash,                                                              //
                        get_pos_triple(token_slice{tokens.first, tokens.first + 1}),            //
                        instance_variable.value()->as<VariableNode>()->name,                    //
                        instance_variable.value()->type,                                        //
                        instance_variable.value()->is_const                                     //
                    );
                    std::unique_ptr<ExpressionNode> argument = std::make_unique<DataAccessNode>( //
                        file_hash,                                                               //
                        get_pos_triple(token_slice{tokens.first, tokens.first + 1}),             //
                        base_expr,                                                               //
                        std::nullopt,                                                            // Func fields have no name
                        i - 1,                                                                   // The index of the data in the func struct
                        required_data_type                                                       //
                    );
                    arguments.insert(arguments.begin(), std::make_pair(std::move(argument), true));
                    argument_types.insert(argument_types.begin(), required_data_type);
                    arg_start_id++;
                }
                break;
            }
            case Type::Variation::INTERFACE:
                break;
        }
    }
    // Check if the argument count does match the parameter count
    FunctionNode *function = functions.front().first;
    const auto &parameters = function->parameters;
    [[maybe_unused]] const unsigned int param_count = parameters.size();
    [[maybe_unused]] const unsigned int arg_count = arguments.size();
    // Argument counts are guaranteed to match the param count because if they would not, the `get_functions_from_call_types` function would
    // have returned `an empty list`
    ASSERT(param_count == arg_count);
    // If we came until here, the argument types definitely match the function parameter types, or they can be cast to them (in the literal
    // case), otherwise no function would have been found
    // Lastly, update the arguments of the call with the information of the function definition, if the arguments should be references Every
    // non-primitive type is always a reference (except enum types, for now)
    // We also check if the argument type is of type 'int' or 'float' and simply change it to the function parameter type directly
    for (size_t i = arg_start_id; i < arguments.size(); i++) {
        const std::string arg_str = arguments[i].first->type->to_string();
        if (arg_str == "int" || arg_str == "float") {
            // Set the type of the argument to the expected parameter type, since we know it's compatible, otherwise the function would
            // never been suggested as a possible function to call in the first place
            arguments[i].first->type = std::get<0>(parameters[i]);
        }
        const auto &param_type = std::get<0>(parameters.at(i));
        if (!check_castability(param_type, arguments[i].first)) {
            THROW_ERR(ErrExprTypeMismatch, ERR_PARSING, file_hash, tokens, param_type, arguments[i].first->type);
            return std::nullopt;
        }
        arguments[i].second = arguments[i].first->type->is_reference();
        // Also, we check here if the variable is immutable but the function expects an mutable reference instead
        if (arguments[i].second) {
            // Its a complex data type, so its a reference
            tok = tokens.first + arg_range->first;
            // Now we need to get until the token where the error happened, e.g. the ith argument
            size_t depth = 0;
            size_t arg_id = i - arg_start_id;
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
                if (!scope->variables.at(variable_node->name).is_mutable && std::get<2>(parameters[i])) {
                    THROW_ERR(ErrVarMutatingConst, ERR_PARSING, file_hash, tok->line, tok->column, variable_node->name);
                    return std::nullopt;
                }
            }
        }
    }

    // Check if the targetted function inside the func module is virtual, it cannot be called directly in this case
    if (is_typed_call                                                   //
        && tokens.first->type->get_variation() == Type::Variation::FUNC //
        && !function->scope.has_value()                                 //
    ) {
        THROW_ERR(ErrExprCallOfVirtualFunction, ERR_PARSING, file_hash, tokens);
        return std::nullopt;
    }

    types return_types = function->return_types;
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
        .function = function,
        .instance_variable = std::move(instance_variable),
        .callable = std::nullopt,
    };
}

std::optional<Parser::CreateUnaryOpBaseRet> Parser::create_unary_op_base( //
    const Context &ctx,                                                   //
    std::shared_ptr<Scope> &scope,                                        //
    const token_slice &tokens                                             //
) {
    PROFILE_CUMULATIVE("Parser::create_unary_op_base");
    token_slice tokens_mut = tokens;
    remove_trailing_garbage(tokens_mut);
    // For an unary operator to work, the tokens now must have at least two tokens
    size_t tokens_size = get_slice_size(tokens_mut);
    if (tokens_size < 2) {
        THROW_ERR(ErrExprUnaryOpMissingExpr, ERR_PARSING, file_hash, tokens);
        return std::nullopt;
    }

    // Check if the unary operator is defined to the left of the expression or the right of it
    bool is_left;
    const uint2 left_range = {0, 1};
    const uint2 right_range = {tokens_size - 1, tokens_size};
    if (Matcher::tokens_contain_in_range(tokens_mut, Matcher::unary_pre_operator, left_range)) {
        is_left = true;
    } else if (Matcher::tokens_contain_in_range(tokens_mut, Matcher::unary_post_operator, right_range)) {
        is_left = false;
    } else {
        __builtin_unreachable();
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
    ASSERT(std::next(operator_tokens.first) == operator_tokens.second); // Assert operator_tokens.size() == 1
    Token operator_token = operator_tokens.first->token;

    // All other tokens now are the expression
    auto expression = create_expression(ctx, scope, tokens_mut);
    if (!expression.has_value()) {
        return std::nullopt;
    }
    return CreateUnaryOpBaseRet{
        .operation = operator_token,
        .base_expr = std::move(expression.value()),
        .is_left = is_left,
    };
}

std::optional<Parser::CreateFieldAccessBaseRet> Parser::create_field_access_base( //
    const Context &ctx,                                                           //
    std::shared_ptr<Scope> &scope,                                                //
    const token_slice &tokens,                                                    //
    const bool has_inbetween_operator                                             //
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
        ASSERT(std::prev(base_expr_tokens.second)->token == TOK_DOLLAR);
        long int_value = std::stol(std::string(base_expr_tokens.second->lexme));
        ASSERT(int_value >= 0);
        field_id = static_cast<unsigned int>(int_value);
        base_expr_tokens.second--;
    } else {
        THROW_ERR(                                                                                                             //
            ErrParsUnexpectedToken, ERR_PARSING, file_hash, base_expr_tokens.second->line,                                     //
            base_expr_tokens.second->column, std::vector<Token>{TOK_IDENTIFIER, TOK_INT_VALUE}, base_expr_tokens.second->token //
        );
        return std::nullopt;
    }
    base_expr_tokens.second--;
    if (base_expr_tokens.second->token != TOK_DOT) {
        THROW_ERR(                                                                                       //
            ErrParsUnexpectedToken, ERR_PARSING, file_hash, base_expr_tokens.second->line,               //
            base_expr_tokens.second->column, std::vector<Token>{TOK_DOT}, base_expr_tokens.second->token //
        );
        return std::nullopt;
    }
    if (has_inbetween_operator) {
        base_expr_tokens.second--;
        ASSERT(Matcher::token_match(base_expr_tokens.second->token, Matcher::inbetween_operator));
    }

    // Now everything left in the `base_expr_tokens` is our base expression, so we can parse it accordingly
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(ctx, scope, base_expr_tokens);
    if (!base_expr.has_value()) {
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
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, base_type,
                std::vector<std::pair<std::string, std::shared_ptr<Type>>>{
                    {"length", Type::get_primitive_type("u64")},
                    {"len", Type::get_primitive_type("u64")},
                });
            return std::nullopt;
        }
        return CreateFieldAccessBaseRet{
            .base_expr = std::move(base_expr.value()),
            .field_name = field_name,
            .field_id = 0,
            .field_type = Type::get_primitive_type("u64"),
        };
    }
    switch (base_type->get_variation()) {
        default:
            break;
        case Type::Variation::ARRAY: {
            const auto *array_type = base_type->as<ArrayType>();
            if (field_name != "length" && field_name != "len") {
                THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, base_type,
                    std::vector<std::pair<std::string, std::shared_ptr<Type>>>{
                        {"length", Type::get_primitive_type("u64")},
                        {"len", Type::get_primitive_type("u64")},
                    });
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
                return CreateFieldAccessBaseRet{
                    .base_expr = std::move(base_expr.value()),
                    .field_name = field_name,
                    .field_id = 1,
                    .field_type = group_type,
                };
            }
            return CreateFieldAccessBaseRet{
                .base_expr = std::move(base_expr.value()),
                .field_name = field_name,
                .field_id = 1,
                .field_type = Type::get_primitive_type("u64"),
            };
        }
        case Type::Variation::DATA: {
            const DataNode *data_node = base_type->as<DataType>()->data_node;
            // Check if the given field name exists in the data type
            field_id = 0;
            for (const auto &field : data_node->fields) {
                if (field.name == field_name) {
                    break;
                }
                field_id++;
            }
            if (data_node->fields.size() == field_id) {
                THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, base_type, std::nullopt);
                return std::nullopt;
            }
            const std::shared_ptr<Type> field_type = data_node->fields.at(field_id).type;
            return CreateFieldAccessBaseRet{
                .base_expr = std::move(base_expr.value()),
                .field_name = field_name,
                .field_id = field_id,
                .field_type = field_type,
            };
        }
        case Type::Variation::OBJECT: {
            const ObjectNode *object_node = base_type->as<ObjectType>()->object_node;
            ASSERT(base_expr.value()->get_variation() == ExpressionNode::Variation::VARIABLE);
            // The base variable has the "name" of the captured parent object type but the "type" of the child object (for the "self"
            // parameter)
            VariableNode *base_var = base_expr.value()->as<VariableNode>();
            const auto captured_object_it = scope->captured_object_identifiers.find(base_var->name);
            if (captured_object_it == scope->captured_object_identifiers.end()) {
                // Not a parent accessor — regular object variable, can't resolve field access
                THROW_ERR(ErrExprFieldAccessOnObject, ERR_PARSING, file_hash, tokens);
                return std::nullopt;
            }
            const std::shared_ptr<Type> captured_object_type = captured_object_it->second;
            ASSERT(captured_object_type->get_variation() == Type::Variation::OBJECT);
            const ObjectNode *parent_object = captured_object_type->as<ObjectType>()->object_node;
            for (const auto &[data_node, accessor] : parent_object->data_modules) {
                if (!accessor.has_value() || accessor.value() != field_name) {
                    continue;
                }
                for (size_t i = 0; i < object_node->data_modules.size(); i++) {
                    const auto &child_data_node = object_node->data_modules.at(i).first;
                    if (child_data_node != data_node) {
                        continue;
                    }
                    base_var->name = "self";
                    auto data_type = file_node_ptr->file_namespace->get_type_from_str(data_node->name);
                    ASSERT(data_type.has_value());
                    return CreateFieldAccessBaseRet{
                        .base_expr = std::move(base_expr.value()),
                        .field_name = std::nullopt,
                        .field_id = static_cast<unsigned int>(i),
                        .field_type = data_type.value(),
                    };
                }
                __builtin_unreachable();
            }
            // TODO: This should be a nested parent (like e.e.d) and I think we would need to iterate the parent objects parents for
            // this...
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return std::nullopt;
        }
        case Type::Variation::PRIMITIVE:
            if (base_type->to_string() != "anyerror") {
                break;
            }
            [[fallthrough]];
        case Type::Variation::ERROR_SET:
            if (field_name == "type_id") {
                return CreateFieldAccessBaseRet{
                    .base_expr = std::move(base_expr.value()),
                    .field_name = field_name,
                    .field_id = 0,
                    .field_type = Type::get_primitive_type("u32"),
                };
            } else if (field_name == "value_id") {
                return CreateFieldAccessBaseRet{
                    .base_expr = std::move(base_expr.value()),
                    .field_name = field_name,
                    .field_id = 1,
                    .field_type = Type::get_primitive_type("u32"),
                };
            } else if (field_name == "message") {
                return CreateFieldAccessBaseRet{
                    .base_expr = std::move(base_expr.value()),
                    .field_name = field_name,
                    .field_id = 2,
                    .field_type = Type::get_primitive_type("str"),
                };
            }
            break;
        case Type::Variation::MULTI: {
            const auto *multi_type = base_type->as<MultiType>();
            if (field_name == "") {
                field_name = "$" + std::to_string(field_id);
            }
            auto access = create_multi_type_access(tokens, base_type, field_name);
            if (!access.has_value()) {
                return std::nullopt;
            }
            return CreateFieldAccessBaseRet{
                .base_expr = std::move(base_expr.value()),
                .field_name = std::get<0>(access.value()),
                .field_id = std::get<1>(access.value()),
                .field_type = multi_type->base_type,
            };
        }
        case Type::Variation::TUPLE: {
            const auto *tuple_type = base_type->as<TupleType>();
            if (field_id >= tuple_type->types.size()) {
                auto it = base_expr_tokens.second + 1;
                THROW_ERR(ErrExprTupleAccessOOB, ERR_PARSING, file_hash, it->line, it->column, "$" + std::to_string(field_id), base_type);
                return std::nullopt;
            }
            const std::shared_ptr<Type> field_type = tuple_type->types.at(field_id);
            return CreateFieldAccessBaseRet{
                .base_expr = std::move(base_expr.value()),
                .field_name = std::nullopt,
                .field_id = field_id,
                .field_type = field_type,
            };
        }
        case Type::Variation::VARIANT:
            if (field_name == "active_type") {
                return CreateFieldAccessBaseRet{
                    .base_expr = std::move(base_expr.value()),
                    .field_name = field_name,
                    .field_id = 0,
                    .field_type = Type::get_primitive_type("u8"),
                };
            }
            break;
    }
    THROW_ERR(ErrExprFieldAccessNotAllowedOnType, ERR_PARSING, file_hash, tokens, base_type);
    return std::nullopt;
}

std::optional<std::tuple<std::string, unsigned int>> Parser::create_multi_type_access( //
    const token_slice &tokens,                                                         //
    const std::shared_ptr<Type> type,                                                  //
    const std::string &field_name                                                      //
) {
    PROFILE_CUMULATIVE("Parser::create_multi_type_access");
    const MultiType *multi_type = type->as<MultiType>();
    if (multi_type->width == 2) {
        // The fields are called x and y, but can be accessed via $N
        if (field_name == "x" || field_name == "s" || field_name == "i" || field_name == "u" || field_name == "$0") {
            return std::make_tuple("$0", 0);
        } else if (field_name == "y" || field_name == "t" || field_name == "j" || field_name == "v" || field_name == "$1") {
            return std::make_tuple("$1", 1);
        } else {
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, type,
                std::vector<std::pair<std::string, std::shared_ptr<Type>>>{
                    {"$0", multi_type->base_type},
                    {"x", multi_type->base_type},
                    {"s", multi_type->base_type},
                    {"u", multi_type->base_type},
                    {"i", multi_type->base_type},
                    {"", nullptr},
                    {"$1", multi_type->base_type},
                    {"y", multi_type->base_type},
                    {"t", multi_type->base_type},
                    {"v", multi_type->base_type},
                    {"j", multi_type->base_type},
                });
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
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, type,
                std::vector<std::pair<std::string, std::shared_ptr<Type>>>{
                    {"$0", multi_type->base_type},
                    {"x", multi_type->base_type},
                    {"r", multi_type->base_type},
                    {"s", multi_type->base_type},
                    {"i", multi_type->base_type},
                    {"", nullptr},
                    {"$1", multi_type->base_type},
                    {"y", multi_type->base_type},
                    {"g", multi_type->base_type},
                    {"t", multi_type->base_type},
                    {"j", multi_type->base_type},
                    {"", nullptr},
                    {"$2", multi_type->base_type},
                    {"z", multi_type->base_type},
                    {"b", multi_type->base_type},
                    {"p", multi_type->base_type},
                    {"k", multi_type->base_type},
                });
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
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, type,
                std::vector<std::pair<std::string, std::shared_ptr<Type>>>{
                    {"$0", multi_type->base_type},
                    {"x", multi_type->base_type},
                    {"r", multi_type->base_type},
                    {"s", multi_type->base_type},
                    {"i", multi_type->base_type},
                    {"", nullptr},
                    {"$1", multi_type->base_type},
                    {"y", multi_type->base_type},
                    {"g", multi_type->base_type},
                    {"t", multi_type->base_type},
                    {"j", multi_type->base_type},
                    {"", nullptr},
                    {"$2", multi_type->base_type},
                    {"z", multi_type->base_type},
                    {"b", multi_type->base_type},
                    {"p", multi_type->base_type},
                    {"k", multi_type->base_type},
                    {"", nullptr},
                    {"$3", multi_type->base_type},
                    {"w", multi_type->base_type},
                    {"a", multi_type->base_type},
                    {"q", multi_type->base_type},
                    {"l", multi_type->base_type},
                });
            return std::nullopt;
        }
    } else {
        // Widths of 16 are not supported by Flint yet
        ASSERT(multi_type->width == 8);
        const auto &field_map = std::vector<std::pair<std::string, std::shared_ptr<Type>>>{
            {"$0", multi_type->base_type},
            {"$1", multi_type->base_type},
            {"$2", multi_type->base_type},
            {"$3", multi_type->base_type},
            {"$4", multi_type->base_type},
            {"$5", multi_type->base_type},
            {"$6", multi_type->base_type},
            {"$7", multi_type->base_type},
        };
        // The fields are accessed via $N
        if (field_name.front() != '$') {
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, type, field_map);
            return std::nullopt;
        }
        if (field_name.size() != 2) {
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, type, field_map);
            return std::nullopt;
        }
        const char id = field_name.back() - '0';
        if (static_cast<unsigned int>(id) >= multi_type->width || id < 0) {
            THROW_ERR(ErrExprFieldNonexistent, ERR_PARSING, file_hash, tokens, field_name, type, field_map);
            return std::nullopt;
        }
        return std::make_tuple("$" + std::to_string(id), id);
    }
}

std::optional<Parser::CreateGroupedAccessBaseRet> Parser::create_grouped_access_base( //
    const Context &ctx,                                                               //
    std::shared_ptr<Scope> &scope,                                                    //
    const token_slice &tokens,                                                        //
    const bool has_inbetween_operator                                                 //
) {
    PROFILE_CUMULATIVE("Parser::create_grouped_access_base");
    // We start at the end of the token slice and move towards the front, and split the token slice in half to get the base expression
    // tokens and all tokens forming the grouped access `.(..)`
    ASSERT((tokens.second - 1)->token == TOK_RIGHT_PAREN);
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
                ASSERT(base_expr_tokens.second->token == TOK_DOT);
                break;
            }
        }
        base_expr_tokens.second--;
        access_tokens.first--;
    }
    if (has_inbetween_operator) {
        base_expr_tokens.second--;
        ASSERT(Matcher::token_match(base_expr_tokens.second->token, Matcher::inbetween_operator));
    }

    // Okay we now can parse the base expression beforehand, to be able to check it's type and decide whether a grouped access is
    // allowed at all
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(ctx, scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        return std::nullopt;
    }
    std::shared_ptr<Type> base_type = base_expr.value()->type;
    if (has_inbetween_operator) {
        const auto *optional_type = base_type->as<OptionalType>();
        base_type = optional_type->base_type;
    }

    // Check if the "base expression" is a type. Grouped accesses are possible on a base of certain types (like enums for example) and
    // result in different expressions, not a grouped access per-se but in different expressions like a "normal" enum literal for example
    if (base_expr.value()->get_variation() == ExpressionNode::Variation::TYPE) {
        const std::shared_ptr<Type> &type = base_expr.value()->type;
        // Its a grouped enum access, like `EnumType.(VAL1, VAL2, VAL3)`
        // All other types other than enums are not supported yet
        if (type->get_variation() != Type::Variation::ENUM) {
            THROW_ERR(ErrExprFieldAccessNotAllowedOnType, ERR_PARSING, file_hash, tokens, base_type);
            return std::nullopt;
        }
        const auto *enum_type = type->as<EnumType>();
        auto tok_it = access_tokens.first;
        const auto &enum_values = enum_type->enum_node->values;
        std::vector<std::string> values;
        while (tok_it->token != TOK_RIGHT_PAREN) {
            if (tok_it->token == TOK_COMMA) {
                ++tok_it;
                continue;
            } else if (tok_it->token != TOK_IDENTIFIER) {
                THROW_ERR(                                                                          //
                    ErrParsUnexpectedToken, ERR_PARSING, file_hash,                                 //
                    tok_it->line, tok_it->column, std::vector<Token>{TOK_IDENTIFIER}, tok_it->token //
                );
                return std::nullopt;
            }
            const std::string value(tok_it->lexme);
            bool enum_contains_tag = false;
            for (size_t i = 0; i < enum_values.size(); i++) {
                if (enum_values.at(i).first == value) {
                    enum_contains_tag = true;
                    break;
                }
            }
            if (!enum_contains_tag) {
                THROW_ERR(ErrExprEnumTagNotPresent, ERR_PARSING, file_hash, token_slice{tok_it, tok_it + 1}, value, type);
                return std::nullopt;
            }
            values.emplace_back(value);
            ++tok_it;
        }
        LitValue lit_value = LitEnum{.enum_type = type, .values = values};
        return CreateGroupedAccessBaseRet{
            .alternative_expression = std::make_unique<LiteralNode>(file_hash, get_pos_triple(tokens), lit_value, type),
            .base_expr = nullptr,
            .field_names = {},
            .field_ids = {},
            .field_types = {},
        };
    }

    // Now, extract the names of all accessed fields
    std::vector<std::string> field_names;
    while (access_tokens.first != access_tokens.second) {
        if (access_tokens.first->token == TOK_IDENTIFIER) {
            field_names.emplace_back(access_tokens.first->lexme);
        } else if (access_tokens.first->token == TOK_DOLLAR && std::next(access_tokens.first)->token == TOK_INT_VALUE) {
            ASSERT(std::next(access_tokens.first)->lexme.find('_') == std::string::npos);
            field_names.emplace_back("$" + std::string(std::next(access_tokens.first)->lexme));
            access_tokens.first++;
        }
        access_tokens.first++;
    }
    ASSERT(!field_names.empty());

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
                const auto field_names_it = std::find(field_names.begin(), field_names.end(), field.name);
                if (field_names_it != field_names.end()) {
                    const size_t group_id = std::distance(field_names.begin(), field_names_it);
                    field_types[group_id] = field.type;
                    field_ids[group_id] = field_id;
                }
                field_id++;
            }
            return CreateGroupedAccessBaseRet{
                .alternative_expression = std::nullopt,
                .base_expr = std::move(base_expr.value()),
                .field_names = field_names,
                .field_ids = field_ids,
                .field_types = field_types,
            };
        }
        case Type::Variation::MULTI: {
            const auto *multi_type = base_type->as<MultiType>();
            std::vector<std::string> access_field_names;
            std::vector<std::shared_ptr<Type>> field_types;
            std::vector<unsigned int> field_ids;
            for (const auto &field_name : field_names) {
                auto access = create_multi_type_access(tokens, base_type, field_name);
                if (!access.has_value()) {
                    return std::nullopt;
                }
                access_field_names.emplace_back(std::get<0>(access.value()));
                field_types.emplace_back(multi_type->base_type);
                field_ids.emplace_back(std::get<1>(access.value()));
            }
            return CreateGroupedAccessBaseRet{
                .alternative_expression = std::nullopt,
                .base_expr = std::move(base_expr.value()),
                .field_names = access_field_names,
                .field_ids = field_ids,
                .field_types = field_types,
            };
        }
        case Type::Variation::TUPLE: {
            const auto *tuple_type = base_type->as<TupleType>();
            std::vector<std::shared_ptr<Type>> field_types;
            std::vector<unsigned int> field_ids;
            for (size_t i = 0; i < field_names.size(); i++) {
                size_t field_id = std::stoul(field_names.at(i).substr(1, field_names.at(i).length() - 1));
                if (field_id >= tuple_type->types.size()) {
                    const auto it = access_tokens.first + field_id * 2;
                    THROW_ERR(                                                          //
                        ErrExprTupleAccessOOB, ERR_PARSING, file_hash,                  //
                        it->line, it->column, "$" + std::to_string(field_id), base_type //
                    );
                    return std::nullopt;
                }
                field_types.emplace_back(tuple_type->types.at(field_id));
                field_ids.emplace_back(field_id);
            }
            return CreateGroupedAccessBaseRet{
                .alternative_expression = std::nullopt,
                .base_expr = std::move(base_expr.value()),
                .field_names = field_names,
                .field_ids = field_ids,
                .field_types = field_types,
            };
        }
    }
    THROW_ERR(ErrExprFieldAccessNotAllowedOnType, ERR_PARSING, file_hash, tokens, base_type);
    return std::nullopt;
}

std::optional<Parser::CreateArrayAccessBaseRet> Parser::create_array_access_base( //
    const Context &ctx,                                                           //
    std::shared_ptr<Scope> &scope,                                                //
    const token_slice &tokens,                                                    //
    const bool has_inbetween_operator                                             //
) {
    PROFILE_CUMULATIVE("Parser::create_array_access_base");
    // Array accesses happen at the end of the expression, so we extract indexing expressions etc from left to right and then parse the
    // base expression last. The last token should be a ] symbol
    ASSERT(std::prev(tokens.second)->token == TOK_RIGHT_BRACKET);
    // Then we search in a balanced way for the [ symbol and count how many , symbols we came across at depth 1. This is the number of
    // indexing expressions in the tokens. Then, when we know the "bounds" of the array access we can parse the base expression and the
    // indexing expressions respectively
    token_slice base_expr_tokens = tokens;
    token_slice indexing_tokens = {tokens.second - 2, tokens.second - 1};
    uint32_t depth = 1;
    bool continue_cond = indexing_tokens.first != tokens.first;
    while (continue_cond) {
        switch (indexing_tokens.first->token) {
            default:
                break;
            case TOK_RIGHT_BRACKET:
                depth++;
                break;
            case TOK_LEFT_BRACKET:
                depth--;
                if (depth == 0) {
                    continue_cond = false;
                }
                break;
        }
        continue_cond &= indexing_tokens.first != tokens.first;
        if (continue_cond) {
            indexing_tokens.first--;
        }
    }
    if (indexing_tokens.first == tokens.first) {
        // No [ symbol found, this should not be possible because then the matcher should not have matched an array access
        UNREACHABLE();
        return std::nullopt;
    }
    ASSERT(indexing_tokens.first->token == TOK_LEFT_BRACKET);
    base_expr_tokens.second = indexing_tokens.first++;
    // Remove the '.' of the base expr, for grouped array accesses
    const bool is_grouped_access = std::prev(base_expr_tokens.second)->token == TOK_DOT;
    if (is_grouped_access) {
        base_expr_tokens.second--;
    }
    if (has_inbetween_operator) {
        base_expr_tokens.second--;
        ASSERT(Matcher::token_match(base_expr_tokens.second->token, Matcher::inbetween_operator));
    }

    // Parse the base expression first before parsing the indexing expressions
    std::optional<std::unique_ptr<ExpressionNode>> base_expr = create_expression(ctx, scope, base_expr_tokens);
    if (!base_expr.has_value()) {
        return std::nullopt;
    }
    if (base_expr.value()->type->get_variation() != Type::Variation::ARRAY && base_expr.value()->type->to_string() != "str") {
        THROW_ERR(ErrExprArrayAccessNotAllowedOnType, ERR_PARSING, file_hash, tokens, base_expr.value()->type);
        return std::nullopt;
    }

    // Now parse the indexing expressions
    auto indexing_expressions = create_group_expressions(_ctx_, scope, indexing_tokens);
    if (!indexing_expressions.has_value()) {
        return std::nullopt;
    }
    // Every expression in the indexing expressions needs to be castable a `u64` type, if it's not of that type already we need to cast
    // it.
    const std::shared_ptr<Type> u64_ty = Type::get_primitive_type("u64");
    if (is_grouped_access) {
        // If the array access is grouped then every indexing expression needs to be castable to a group of u64 types
        // Grouped array accesses do not need the below dimensionality-changes and special-case stuff like regular array accesses which
        // could reduce the dimensionality of the array.
        std::shared_ptr<Type> base_type;
        std::shared_ptr<Type> index_type = u64_ty;
        if (base_expr.value()->type->to_string() == "str") {
            base_type = Type::get_primitive_type("u8");
        } else {
            const auto *array_type = base_expr.value()->type->as<ArrayType>();
            base_type = array_type->type;
            if (array_type->dimensionality > 1) {
                const std::vector<std::shared_ptr<Type>> index_types(array_type->dimensionality, u64_ty);
                index_type = std::make_shared<GroupType>(index_types);
                if (!Type::add_type(index_type)) {
                    index_type = Type::get_type_from_str(index_type->to_string()).value();
                }
            }
        }
        // All indexing expressions need to be castable to either the index type
        if (!ensure_castability_multiple(index_type, indexing_expressions.value(), indexing_tokens)) {
            return std::nullopt;
        }

        const std::vector<std::shared_ptr<Type>> result_types(indexing_expressions.value().size(), base_type);
        std::shared_ptr<Type> result_type = std::make_shared<GroupType>(result_types);
        if (!Type::add_type(result_type)) {
            result_type = Type::get_type_from_str(result_type->to_string()).value();
        }

        return CreateArrayAccessBaseRet{
            .base_expr = std::move(base_expr.value()),
            .indexing_exprs = std::move(indexing_expressions.value()),
            .result_type = result_type,
        };
    }
    if (!ensure_castability_multiple(u64_ty, indexing_expressions.value(), indexing_tokens)) {
        return std::nullopt;
    }
    // Calculate the new dimensionality of the result based on the number of range expressions in the indexing expressions
    uint32_t dimensionality = 0;
    for (const auto &expr : indexing_expressions.value()) {
        if (expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
            dimensionality++;
        }
    }
    auto base_type = base_expr.value()->type;
    if (has_inbetween_operator) {
        base_type = base_type->as<OptionalType>()->base_type;
    }
    const bool base_is_str = base_type->to_string() == "str";
    if (base_is_str) {
        base_type = Type::get_primitive_type("u8");
    } else {
        base_type = base_type->as<ArrayType>()->type;
    }
    std::shared_ptr<Type> result_type = nullptr;
    if (dimensionality > 0) {
        if (base_is_str) {
            ASSERT(dimensionality == 1);
            result_type = Type::get_primitive_type("str");
        } else {
            // TODO: Known Sizes
            result_type = std::make_shared<ArrayType>(dimensionality, base_type, std::nullopt);
            if (!file_node_ptr->file_namespace->add_type(result_type)) {
                result_type = file_node_ptr->file_namespace->get_type_from_str(result_type->to_string()).value();
            }
        }
    } else {
        result_type = base_type;
    }
    if (has_inbetween_operator) {
        result_type = std::make_shared<OptionalType>(result_type);
        if (!file_node_ptr->file_namespace->add_type(result_type)) {
            result_type = file_node_ptr->file_namespace->get_type_from_str(result_type->to_string()).value();
        }
    }
    return CreateArrayAccessBaseRet{
        .base_expr = std::move(base_expr.value()),
        .indexing_exprs = std::move(indexing_expressions.value()),
        .result_type = result_type,
    };
}

bool Parser::ensure_castability_multiple(                      //
    const std::shared_ptr<Type> &to_type,                      //
    std::vector<std::unique_ptr<ExpressionNode>> &expressions, //
    const token_slice &tokens                                  //
) {
    PROFILE_CUMULATIVE("Parser::ensure_castability_multiple");
    // Every expression in the length expressions needs to be castable a `u64` type, if it's not of that type already we need to cast it
    for (auto &expr : expressions) {
        if (expr->type->equals(to_type)) {
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
                expr = std::make_unique<TypeCastNode>(file_hash, ASTNode::PosTriple{expr->line, expr->column, expr->length}, to_type, expr);
                break;
        }
    }
    return true;
}

bool Parser::add_annotation(const token_slice &tokens) {
    ASSERT(tokens.first->token == TOK_ANNOTATION);
    ASSERT((tokens.first + 1)->token == TOK_IDENTIFIER);
    const std::string annotation_name((tokens.first + 1)->lexme);

    if (annotation_map.find(annotation_name) == annotation_map.end()) {
        THROW_ERR(ErrAnnoUnknown, ERR_PARSING, file_hash, tokens, annotation_name);
        return false;
    }
    const AnnotationKind kind = annotation_map.at(annotation_name);
    // Check if the queue already contains this annotation kind, it is not allowed to define the same annotation twice
    for (const auto &annotation : annotation_queue) {
        if (annotation.kind == kind) {
            THROW_ERR(ErrAnnoDuplicate, ERR_PARSING, file_hash, tokens, annotation_name);
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

std::optional<size_t> Parser::get_size_from_expr(const std::unique_ptr<ExpressionNode> &expr) {
    if (!expr->is_comptime_known()) {
        return std::nullopt;
    }
    const ExpressionNode *expression = expr.get();
    // Somehow get the size, the length expr should be either a literal or a cast literal
    if (expression->get_variation() == ExpressionNode::Variation::TYPE_CAST) {
        expression = expr->as<TypeCastNode>()->expr.get();
    }
    if (expression->get_variation() != ExpressionNode::Variation::LITERAL) {
        // Comptime is not advanced enough for anything else yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    const LiteralNode *lit = expression->as<LiteralNode>();
    if (!std::holds_alternative<LitInt>(lit->value)) {
        // Comptime is not advanced enough for anything else yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    const APInt &size = std::get<LitInt>(lit->value).value;
    return size.to_iN<size_t>();
}
