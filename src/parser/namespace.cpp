#include "parser/ast/namespace.hpp"

#include "error/error.hpp"
#include "lexer/lexer.hpp"
#include "matcher/matcher.hpp"
#include "parser/parser.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/unknown_type.hpp"

std::optional<std::shared_ptr<Type>> Namespace::get_type_from_str(const std::string &type_str) const {
    // First check the global types since they are the most common
    if (const auto type = Type::get_type_from_str(type_str)) {
        return type;
    }
    // Check the public types of this namespace
    if (public_symbols.types.find(type_str) != public_symbols.types.end()) {
        return public_symbols.types.at(type_str);
    }
    // If it's not a public type maybe it's a private type
    if (private_symbols.types.find(type_str) != private_symbols.types.end()) {
        return private_symbols.types.at(type_str);
    }
    return std::nullopt;
}

std::optional<Namespace *> Namespace::get_namespace_from_alias(const std::string &alias) const {
    if (public_symbols.aliased_imports.find(alias) == public_symbols.aliased_imports.end()) {
        return std::nullopt;
    }
    return public_symbols.aliased_imports.at(alias);
}

std::vector<FunctionNode *> Namespace::get_functions_from_call_types( //
    const std::string &fn_name,                                       //
    const std::vector<std::shared_ptr<Type>> &arg_types,              //
    const bool is_aliased                                             //
) const {
    std::vector<FunctionNode *> available_functions;

    // Collect all available functions from public and private symbols
    for (const auto &definition : public_symbols.definitions) {
        if (definition->get_variation() == DefinitionNode::Variation::FUNCTION) {
            available_functions.emplace_back(definition->as<FunctionNode>());
        }
    }
    if (!is_aliased) {
        for (const auto &[hash, fn_vec] : private_symbols.functions) {
            for (auto *fn : fn_vec) {
                available_functions.emplace_back(fn);
            }
        }
    }

    std::vector<FunctionNode *> found_functions;
    // Filter functions based on name, parameter count, and type compatibility
    for (auto *fn : available_functions) {
        // Check if function name matches
        if (fn->name != fn_name) {
            continue;
        }

        // Check if parameter count matches
        if (fn->parameters.size() != arg_types.size()) {
            continue;
        }

        // Check if all argument types are compatible with parameter types
        bool all_params_match = true;
        for (size_t i = 0; i < arg_types.size(); i++) {
            const std::shared_ptr<Type> &param_type = std::get<0>(fn->parameters[i]);
            const std::shared_ptr<Type> &arg_type = arg_types[i];

            // Check if types are equal
            if (arg_type->equals(param_type)) {
                continue;
            }

            // Check if argument can be implicitly cast to parameter type
            const Parser::CastDirection castability = Parser::check_castability(arg_type, param_type);
            const std::string arg_type_str = arg_type->to_string();
            if (castability.kind != Parser::CastDirection::Kind::CAST_LHS_TO_RHS) {
                all_params_match = false;
                break;
            }
        }

        if (all_params_match) {
            found_functions.emplace_back(fn);
        }
    }
    return found_functions;
}

std::optional<std::shared_ptr<Type>> Namespace::get_type(const token_slice &tokens) {
    assert(tokens.first != tokens.second);
    const std::string type_str = Lexer::to_string(tokens);
    // Check if the map already contains the given key
    std::optional<std::shared_ptr<Type>> type = get_type_from_str(type_str);
    if (type.has_value()) {
        return type;
    }
    // Create the type
    type = create_type(tokens);
    if (!type.has_value()) {
        return std::nullopt;
    }
    if (type.value()->get_variation() == Type::Variation::UNKNOWN) {
        public_symbols.unknown_types[type_str] = type.value();
        return public_symbols.unknown_types.at(type_str);
    }
    if (can_be_global(type.value())) {
        Type::add_type(type.value());
        return type.value();
    }
    public_symbols.types[type_str] = type.value();
    return public_symbols.types.at(type_str);
}

bool Namespace::add_type(const std::shared_ptr<Type> &type) {
    // First we check if the type already exists in the global type map
    const std::string type_string = type->to_string();
    if (const auto global_type = Type::get_type_from_str(type_string)) {
        // Type already existed
        return false;
    }

    // Then check if the type contains any user-defined types. If not it will definitely be stored in the global type map
    if (can_be_global(type)) {
        std::unique_lock<std::shared_mutex> lock(Type::types_mutex);
        return Type::types.emplace(type->to_string(), type).second;
    }

    // If it contains user-defined types we check if it's already present in the public type section of this file and if it's not
    // contained yet we add it
    if (public_symbols.types.find(type_string) != public_symbols.types.end()) {
        return false;
    }

    // Add the type to the public section of the namespace
    public_symbols.types[type_string] = type;
    return true;
}

bool Namespace::resolve_type(std::shared_ptr<Type> &type) {
    switch (type->get_variation()) {
        default:
            // No need to resolve the other types since they cannot contain unknown types
            break;
        case Type::Variation::ARRAY: {
            auto *array_type = type->as<ArrayType>();
            if (!resolve_type(array_type->type)) {
                return false;
            }
            break;
        }
        case Type::Variation::GROUP: {
            auto *group_type = type->as<GroupType>();
            for (auto &elem_type : group_type->types) {
                if (!resolve_type(elem_type)) {
                    return false;
                }
            }
            break;
        }
        case Type::Variation::OPTIONAL: {
            auto *optional_type = type->as<OptionalType>();
            if (!resolve_type(optional_type->base_type)) {
                return false;
            }
            break;
        }
        case Type::Variation::POINTER: {
            auto *pointer_type = type->as<PointerType>();
            if (!resolve_type(pointer_type->base_type)) {
                return false;
            }
            break;
        }
        case Type::Variation::TUPLE: {
            auto *tuple_type = type->as<TupleType>();
            for (auto &elem_type : tuple_type->types) {
                if (!resolve_type(elem_type)) {
                    return false;
                }
            }
            break;
        }
        case Type::Variation::UNKNOWN: {
            const auto *unknown_type = type->as<UnknownType>();
            // Get the "real" type from the parameter type
            auto type_maybe = get_type_from_str(unknown_type->type_str);
            if (!type_maybe.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            type = type_maybe.value();
            break;
        }
        case Type::Variation::VARIANT: {
            const auto *variant_type = type->as<VariantType>();
            for (auto &[_, var_type] : variant_type->get_possible_types()) {
                if (!resolve_type(var_type)) {
                    return false;
                }
            }
            break;
        }
    }
    return true;
}

std::optional<std::shared_ptr<Type>> Namespace::create_type(const token_slice &tokens) {
    token_slice tokens_mut = tokens;
    // If the size of the token type list is 1, its definitely a simple type
    if (std::next(tokens_mut.first) == tokens_mut.second) {
        if (Matcher::token_match(tokens_mut.first->token, Matcher::type_prim)) {
            // Its definitely a primitive type, but all primitive types should have been created by default annyway, so this should not be
            // possible
            assert(false);
        } else if (Matcher::token_match(tokens_mut.first->token, Matcher::type_prim_mult)) {
            // Its a multi-type
            const std::string type_string(tokens_mut.first->lexme);
            // The last character should be a number
            const char width_char = type_string.back();
            assert(width_char >= '0' && width_char <= '9');
            // Skip the last character (being the number) as well as the `x`, if the last character is an 'x'
            const bool ends_with_x = type_string.at(type_string.size() - 2) == 'x';
            const std::string type_str = type_string.substr(0, type_string.size() - (ends_with_x ? 2 : 1));
            const std::shared_ptr<Type> base_type = get_type_from_str(type_str).value();
            const unsigned int width = width_char - '0';
            return std::make_shared<MultiType>(base_type, width);
        }
        // Its a data, entity or any other type that only has one string as its descriptor, and this type has not been added yet. This means
        // that its an up until now unknown type. This should only happen in the definition phase.
        return std::make_shared<UnknownType>(std::string(tokens_mut.first->lexme));
    }
    // If the type list ends with a ], its definitely an array type
    if (std::prev(tokens_mut.second)->token == TOK_RIGHT_BRACKET) {
        tokens_mut.second--; // Remove the ]
        // Now, check how many commas follow (in reverse order) to get the dimensionality
        size_t dimensionality = 1;
        while (std::prev(tokens_mut.second)->token == TOK_COMMA) {
            dimensionality++;
            tokens_mut.second--;
        }
        // Now, check if the last element is either a literal or a [ token
        if (std::prev(tokens_mut.second)->token == TOK_LEFT_BRACKET) {
            tokens_mut.second--; // Remove the [
            std::optional<std::shared_ptr<Type>> arr_type = get_type(tokens_mut);
            if (!arr_type.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return std::make_shared<ArrayType>(dimensionality, arr_type.value());
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    } else if (std::prev(tokens_mut.second)->token == TOK_GREATER) {
        // Its a nested type
        if (tokens_mut.first->token == TOK_DATA) {
            // Its a tuple type
            tokens_mut.first++;
            // Now should come a '<' token
            if (tokens_mut.first->token != TOK_LESS) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            tokens_mut.first++;
            // Now get all sub-types balancedly
            std::vector<std::shared_ptr<Type>> subtypes;
            int depth = 1;
            auto type_start = tokens_mut.first;
            while (tokens_mut.first != tokens_mut.second) {
                if (tokens_mut.first->token == TOK_LESS || tokens_mut.first->token == TOK_LEFT_BRACKET) {
                    depth++;
                    tokens_mut.first++;
                } else if (tokens_mut.first->token == TOK_GREATER || tokens_mut.first->token == TOK_RIGHT_BRACKET) {
                    depth--;
                    tokens_mut.first++;
                    if (depth < 0 && tokens_mut.first != tokens_mut.second) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    if (depth == 0) {
                        auto type_result = get_type({type_start, tokens_mut.first - 1});
                        if (!type_result.has_value()) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        subtypes.emplace_back(type_result.value());
                    } else if (depth == 1) {
                        auto type_result = get_type({type_start, tokens_mut.first});
                        if (!type_result.has_value()) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        subtypes.emplace_back(type_result.value());
                    }
                } else if (depth == 1 && tokens_mut.first->token == TOK_COMMA) {
                    auto type_result = get_type({type_start, tokens_mut.first});
                    if (!type_result.has_value()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return std::nullopt;
                    }
                    subtypes.emplace_back(type_result.value());
                    tokens_mut.first++;
                    type_start = tokens_mut.first;
                } else {
                    tokens_mut.first++;
                }
            }
            if (subtypes.empty()) {
                // Empty tuples are not allowed
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            } else if (subtypes.size() == 1) {
                // Tuples of size 1 are not allowed
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            // Check if the tuple is the same type as a multi-type, such tuples are not allowed either
            const std::shared_ptr<Type> &first_type = subtypes.front();
            const std::string front_type_str = first_type->to_string();
            if (front_type_str == "bool" || front_type_str == "i32" || front_type_str == "f32" || front_type_str == "i64" ||
                front_type_str == "f64") {
                const size_t subtypes_size = subtypes.size();
                if (subtypes_size == 2 || subtypes_size == 3 || subtypes_size == 4 || subtypes_size == 8) {
                    // Now check if all types are equal
                    bool all_types_same = true;
                    for (const auto &subtype : subtypes) {
                        if (!subtype->equals(first_type)) {
                            all_types_same = false;
                            break;
                        }
                    }
                    if (all_types_same) {
                        // Its a multi-type but defined as a tuple, which is not valid
                        const Hash &file_hash = Resolver::file_ids.at(tokens_mut.first->file_id);
                        THROW_ERR(ErrTypeTupleMultiTypeOverlap, ERR_PARSING, file_hash, tokens);
                        return std::nullopt;
                    }
                }
            }
            return std::make_shared<TupleType>(subtypes);
        } else if (tokens_mut.first->token == TOK_VARIANT) {
            // It's a inline-defined variant, which has no support for tags, so this makes the parsing of the type quite a lot easier
            tokens_mut.first++;
            if (tokens_mut.first->token != TOK_LESS) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            tokens_mut.first++;
            // Now get all the possible types of the variant, but we need to check them all for uniqueness too
            std::vector<std::shared_ptr<Type>> possible_types;
            while (tokens_mut.first != tokens_mut.second) {
                if (tokens_mut.first->token == TOK_COMMA) {
                    tokens_mut.first++;
                } else if (tokens_mut.first->token == TOK_GREATER) {
                    break;
                }
                token_slice type_tokens = {tokens_mut.first, tokens_mut.second};
                std::optional<uint2> next_range = Matcher::get_next_match_range(type_tokens, Matcher::type);
                if (!next_range.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                assert(next_range.value().first == 0);
                type_tokens.second = type_tokens.first + next_range.value().second;
                tokens_mut.first = type_tokens.second;
                std::optional<std::shared_ptr<Type>> type = get_type(type_tokens);
                if (!type.has_value()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                if (std::find(possible_types.begin(), possible_types.end(), type.value()) != possible_types.end()) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return std::nullopt;
                }
                possible_types.emplace_back(type.value());
            }
            std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> variant_type = possible_types;
            return std::make_shared<VariantType>(variant_type, false);
        }
    } else if (std::prev(tokens_mut.second)->token == TOK_QUESTION) {
        // It's an optional type
        if (tokens_mut.first == std::prev(tokens_mut.second)) {
            // A single trailing ? which is not part of any other type, a ? cannot stand alone
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // Everything to the left of the question mark is the base type of the optional
        std::optional<std::shared_ptr<Type>> base_type = get_type({tokens_mut.first, std::prev(tokens_mut.second)});
        if (!base_type.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_shared<OptionalType>(base_type.value());
    } else if (std::prev(tokens_mut.second)->token == TOK_MULT) {
        // It's a pointer type
        if (tokens_mut.first == std::prev(tokens_mut.second)) {
            // A single * without anything before it, this should not be possible
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        // Everything to the left of the * is the base type of the pointer
        std::optional<std::shared_ptr<Type>> base_type = get_type({tokens_mut.first, std::prev(tokens_mut.second)});
        if (!base_type.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_shared<PointerType>(base_type.value());
    }

    // The type can not be parsed and does not exist yet
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}

bool Namespace::can_be_global(const std::shared_ptr<Type> &type) {
    switch (type->get_variation()) {
        default:
            return true;
        case Type::Variation::ARRAY: {
            const auto *array_type = type->as<ArrayType>();
            return can_be_global(array_type->type);
        }
        case Type::Variation::DATA:
            // Data is always user-defined
            return false;
        case Type::Variation::ENUM:
            // Enums are always user-defined
            return false;
        case Type::Variation::ERROR_SET:
            // Error Sets are always user-defined
            return false;
        case Type::Variation::GROUP: {
            const auto *group_type = type->as<GroupType>();
            for (const auto &elem_type : group_type->types) {
                if (!can_be_global(elem_type)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::OPTIONAL: {
            const auto *optional_type = type->as<OptionalType>();
            return can_be_global(optional_type->base_type);
        }
        case Type::Variation::POINTER: {
            const auto *pointer_type = type->as<PointerType>();
            return can_be_global(pointer_type->base_type);
        }
        case Type::Variation::TUPLE: {
            const auto *tuple_type = type->as<TupleType>();
            for (const auto &elem_type : tuple_type->types) {
                if (!can_be_global(elem_type)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::UNKNOWN:
            return false;
        case Type::Variation::VARIANT: {
            const auto *variant_type = type->as<VariantType>();
            if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                // User-defined variants can never be global
                return false;
            } else {
                const auto &type_list = std::get<std::vector<std::shared_ptr<Type>>>(variant_type->var_or_list);
                for (const auto &possible_type : type_list) {
                    if (!can_be_global(possible_type)) {
                        return false;
                    }
                }
                return true;
            }
        }
    }
    return false;
}

std::optional<FunctionNode *> Namespace::find_core_function( //
    const std::string &module_name,                          //
    const std::string &fn_name,                              //
    const std::vector<std::shared_ptr<Type>> &arg_types      //
) const {
    auto &functions_map = private_symbols.functions;
    const Hash module_hash(module_name);
    if (functions_map.find(module_hash) == functions_map.end()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    std::vector<FunctionNode *> functions = functions_map.at(module_hash);
    for (auto &fn : functions) {
        if (fn->name != fn_name) {
            continue;
        }
        if (fn->parameters.size() != arg_types.size()) {
            continue;
        }
        bool match = true;
        for (size_t i = 0; i < arg_types.size(); i++) {
            if (std::get<0>(fn->parameters[i]) != arg_types[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            return fn;
        }
    }
    return std::nullopt;
}
