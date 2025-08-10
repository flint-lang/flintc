#include "parser/type/type.hpp"

#include "error/error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "matcher/matcher.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/unknown_type.hpp"
#include "parser/type/variant_type.hpp"

#include <mutex>
#include <optional>
#include <string>

void Type::init_types() {
    std::shared_ptr<Type> i32_type = get_primitive_type("i32");
    get_primitive_type("u32");
    std::shared_ptr<Type> i64_type = get_primitive_type("i64");
    get_primitive_type("u64");
    std::shared_ptr<Type> f32_type = get_primitive_type("f32");
    std::shared_ptr<Type> f64_type = get_primitive_type("f64");
    std::shared_ptr<Type> bool_type = get_primitive_type("bool");
    std::shared_ptr<Type> str_type = get_primitive_type("str");
    get_primitive_type("__flint_type_str_lit");
    std::shared_ptr<Type> void_type = get_primitive_type("void");
    add_type(std::make_shared<OptionalType>(void_type));
    get_primitive_type("u8");
    get_primitive_type("anyerror");
    add_type(std::make_shared<MultiType>(bool_type, 8));
    add_type(std::make_shared<MultiType>(i32_type, 2));
    add_type(std::make_shared<MultiType>(i32_type, 3));
    add_type(std::make_shared<MultiType>(i32_type, 4));
    add_type(std::make_shared<MultiType>(i64_type, 2));
    add_type(std::make_shared<MultiType>(i64_type, 3));
    add_type(std::make_shared<MultiType>(i64_type, 4));
    add_type(std::make_shared<MultiType>(f32_type, 2));
    add_type(std::make_shared<MultiType>(f32_type, 3));
    add_type(std::make_shared<MultiType>(f32_type, 4));
    add_type(std::make_shared<MultiType>(f64_type, 2));
    add_type(std::make_shared<MultiType>(f64_type, 3));
    add_type(std::make_shared<MultiType>(f64_type, 4));
    add_type(std::make_shared<ArrayType>(1, str_type));
}

bool Type::resolve_type(std::shared_ptr<Type> &type) {
    if (UnknownType *unknown_type = dynamic_cast<UnknownType *>(type.get())) {
        // Get the "real" type from the parameter type
        auto type_maybe = Type::get_type_from_str(unknown_type->type_str);
        if (!type_maybe.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        type = type_maybe.value();
    } else if (VariantType *variant_type = dynamic_cast<VariantType *>(type.get())) {
        for (auto &[_, var_type] : variant_type->get_possible_types()) {
            if (!resolve_type(var_type)) {
                return false;
            }
        }
    } else if (TupleType *tuple_type = dynamic_cast<TupleType *>(type.get())) {
        for (auto &elem_type : tuple_type->types) {
            if (!resolve_type(elem_type)) {
                return false;
            }
        }
    } else if (OptionalType *optional_type = dynamic_cast<OptionalType *>(type.get())) {
        if (!resolve_type(optional_type->base_type)) {
            return false;
        }
    } else if (GroupType *group_type = dynamic_cast<GroupType *>(type.get())) {
        for (auto &elem_type : group_type->types) {
            if (!resolve_type(elem_type)) {
                return false;
            }
        }
    } else if (ArrayType *array_type = dynamic_cast<ArrayType *>(type.get())) {
        if (!resolve_type(array_type->type)) {
            return false;
        }
    }
    return true;
}

bool Type::add_type(const std::shared_ptr<Type> &type_to_add) {
    std::unique_lock<std::shared_mutex> lock(types_mutex);
    return types.emplace(type_to_add->to_string(), type_to_add).second;
}

std::optional<std::shared_ptr<Type>> Type::get_type(const token_slice &tokens, const bool mutex_already_locked) {
    assert(tokens.first != tokens.second);
    const std::string type_str = Lexer::to_string(tokens);
    // Check if the map already contains the given key with a shared lock
    {
        std::shared_lock<std::shared_mutex> shared_lock;
        if (!mutex_already_locked) {
            shared_lock = std::shared_lock<std::shared_mutex>(types_mutex);
        }
        if (types.find(type_str) != types.end()) {
            return types.at(type_str);
        }
        if (unknown_types.find(type_str) != unknown_types.end()) {
            return unknown_types.at(type_str);
        }
    }
    // If the types map does not contain the type yet, lock the map to make it non-accessible while we modify it
    std::unique_lock<std::shared_mutex> lock;
    if (!mutex_already_locked) {
        lock = std::unique_lock<std::shared_mutex>(types_mutex);
    }
    if (types.find(type_str) != types.end()) {
        // Another thread might already have added the type
        return types.at(type_str);
    }
    if (unknown_types.find(type_str) != unknown_types.end()) {
        return unknown_types.at(type_str);
    }
    std::optional<std::shared_ptr<Type>> created_type = create_type(tokens, true);
    if (!created_type.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (dynamic_cast<const UnknownType *>(created_type.value().get())) {
        unknown_types[type_str] = created_type.value();
        return unknown_types.at(type_str);
    }
    types[type_str] = created_type.value();
    return types.at(type_str);
}

std::shared_ptr<Type> Type::get_primitive_type(const std::string &type_str) {
    // Check if string is empty
    assert(!type_str.empty() && "Identifier cannot be empty");
    // Check first character (must be letter or underscore)
    assert((std::isalpha(static_cast<unsigned char>(type_str[0])) || type_str[0] == '_') &&
        "Identifier must start with a letter or underscore");
    // Check remaining characters
    for (size_t i = 1; i < type_str.size(); ++i) {
        assert((std::isalnum(static_cast<unsigned char>(type_str[i])) || type_str[i] == '_') &&
            "Identifier must contain only letters, digits, or underscores");
    }
    // Check if the given type already exists in the types map
    {
        std::shared_lock<std::shared_mutex> shared_lock(types_mutex);
        if (types.find(type_str) != types.end()) {
            return types.at(type_str);
        }
    }
    // If the types map does not contain the type yet, lock the map to make it non-accessible while we modify it
    std::unique_lock<std::shared_mutex> lock(types_mutex);
    if (types.find(type_str) != types.end()) {
        // Another thread might already have added the type
        return types.at(type_str);
    }
    // Create a primitive type from the type_str
    types[type_str] = std::make_shared<PrimitiveType>(type_str);
    return types.at(type_str);
}

std::optional<std::shared_ptr<Type>> Type::get_type_from_str(const std::string &type_str) {
    std::shared_lock<std::shared_mutex> shared_lock(types_mutex);
    if (types.find(type_str) == types.end()) {
        return std::nullopt;
    }
    return types.at(type_str);
}

std::shared_ptr<Type> Type::str_to_type(const std::string_view &str) {
    std::optional<std::shared_ptr<Type>> result = Type::get_type_from_str(std::string(str));
    if (!result.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return nullptr;
    }
    return result.value();
};

uint32_t Type::get_type_id_from_str(const std::string &name) {
    // 31-bit hash container
    struct Hash31 {
        uint32_t hash : 31;
        uint32_t unused : 1;
    };

    // FNV-1a hash algorithm constants
    constexpr uint32_t FNV_PRIME = 16777619u;
    constexpr uint32_t FNV_OFFSET_BASIS = 18652613u; // 2166136261 truncated to 31 bits

    // Initialize with the FNV offset basis (truncated to 31 bits automatically)
    Hash31 field;
    field.hash = FNV_OFFSET_BASIS;
    field.unused = 0;

    for (char c : name) {
        field.hash ^= static_cast<unsigned char>(c);
        field.hash *= FNV_PRIME;
    }

    // Shift left and handle zero case
    uint32_t result = static_cast<uint32_t>(field.hash) << 1;
    return (result == 0) ? 1 : result;
}

void Type::clear_unknown_types() {
    unknown_types.clear();
}

std::optional<std::shared_ptr<Type>> Type::create_type(const token_slice &tokens, const bool mutex_already_locked) {
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
            assert(types.find(type_str) != types.end());
            const std::shared_ptr<Type> base_type = types.at(type_str);
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
            std::optional<std::shared_ptr<Type>> arr_type = get_type(tokens_mut, mutex_already_locked);
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
                        auto type_result = get_type({type_start, tokens_mut.first - 1}, mutex_already_locked);
                        if (!type_result.has_value()) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        subtypes.emplace_back(type_result.value());
                    } else if (depth == 1) {
                        auto type_result = get_type({type_start, tokens_mut.first}, mutex_already_locked);
                        if (!type_result.has_value()) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return std::nullopt;
                        }
                        subtypes.emplace_back(type_result.value());
                    }
                } else if (depth == 1 && tokens_mut.first->token == TOK_COMMA) {
                    auto type_result = get_type({type_start, tokens_mut.first}, mutex_already_locked);
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
                        if (subtype != first_type) {
                            all_types_same = false;
                            break;
                        }
                    }
                    if (all_types_same) {
                        // Its a multi-type but defined as a tuple, which is not valid
                        THROW_BASIC_ERR(ERR_PARSING);
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
                std::optional<std::shared_ptr<Type>> type = Type::get_type(type_tokens, mutex_already_locked);
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
        std::optional<std::shared_ptr<Type>> base_type = get_type({tokens_mut.first, std::prev(tokens_mut.second)}, mutex_already_locked);
        if (!base_type.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        return std::make_shared<OptionalType>(base_type.value());
    }

    // The type can not be parsed and does not exist yet
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}
