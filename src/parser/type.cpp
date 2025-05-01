#include "parser/type/type.hpp"

#include "error/error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/simple_type.hpp"

#include <mutex>
#include <optional>
#include <string>

void Type::init_types() {
    get_simple_type("i32");
    get_simple_type("u32");
    get_simple_type("i64");
    get_simple_type("u64");
    get_simple_type("f32");
    get_simple_type("f64");
    get_simple_type("bool");
    get_simple_type("str");
    get_simple_type("void");
    get_simple_type("char");
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
    std::optional<std::shared_ptr<Type>> created_type = create_type(tokens, true);
    if (!created_type.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    types[type_str] = created_type.value();
    return types.at(type_str);
}

std::shared_ptr<Type> Type::get_simple_type(const std::string &type_str) {
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
    // Create a simple type from the type_str
    types[type_str] = std::make_shared<SimpleType>(type_str);
    return types.at(type_str);
}

std::optional<std::shared_ptr<Type>> Type::get_type_from_str(const std::string &type_str) {
    std::shared_lock<std::shared_mutex> shared_lock(types_mutex);
    if (types.count(type_str) == 0) {
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

std::optional<std::shared_ptr<Type>> Type::create_type(const token_slice &tokens, const bool mutex_already_locked) {
    token_slice tokens_mut = tokens;
    // If the size of the token type list is 1, its definitely a simple type
    if (std::next(tokens_mut.first) == tokens_mut.second) {
        return std::make_shared<SimpleType>(tokens_mut.first->lexme);
    }
    // If the type list ends with a ], its definitely an array type
    if (std::prev(tokens_mut.second)->type == TOK_RIGHT_BRACKET) {
        tokens_mut.second--; // Remove the ]
        // Now, check how many commas follow (in reverse order) to get the dimensionality
        size_t dimensionality = 1;
        while (std::prev(tokens_mut.second)->type == TOK_COMMA) {
            dimensionality++;
            tokens_mut.second--;
        }
        // Now, check if the last element is either a literal or a [ token
        if (std::prev(tokens_mut.second)->type == TOK_LEFT_BRACKET) {
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
    }
    // If its not a simple type and not an array type its a complex type, e.g. Opt<..> for example. This is not supported yet
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}
