#include "parser/type/type.hpp"

#include "error/error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/simple_type.hpp"

#include <mutex>
#include <optional>
#include <string>

std::optional<std::shared_ptr<Type>> Type::add_and_or_get_type(token_list &tokens) {
    assert(!tokens.empty());
    const std::string type_str = Lexer::to_string(tokens);
    // Check if the map already contains the given key with a shared lock
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
    std::optional<Type> created_type = create_type(tokens);
    if (!created_type.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    types[type_str] = std::make_shared<Type>(created_type.value());
    return types.at(type_str);
}

std::optional<Type> Type::create_type(token_list &tokens) {
    // Every type should start with an identifier first
    assert(tokens.begin()->type == TOK_IDENTIFIER);
    // If the size of the token type list is 1, its definitely a simple type
    if (tokens.size() == 1) {
        SimpleType sp(tokens.begin()->lexme);
        return sp;
    }
    assert(tokens.size() > 1);
    // If the type list ends with a ], its definitely an array type
    if (tokens.back().type == TOK_RIGHT_BRACKET) {
        tokens.pop_back(); // Remove the ]
        // Now, check if the last element is either a literal or a [ token
        if (tokens.back().type == TOK_LEFT_BRACKET) {
            tokens.pop_back(); // Remove the [
            std::optional<std::shared_ptr<Type>> arr_type = add_and_or_get_type(tokens);
            if (!arr_type.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            ArrayType arr(std::nullopt, arr_type.value());
            return arr;
        } else if (tokens.back().type == TOK_INT_VALUE) {
            const size_t length = std::stoul(tokens.back().lexme);
            tokens.pop_back(); // Remove the int value
            assert(tokens.back().type == TOK_LEFT_BRACKET);
            tokens.pop_back(); // Remove the [
            std::optional<std::shared_ptr<Type>> arr_type = add_and_or_get_type(tokens);
            if (!arr_type.has_value()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            ArrayType arr(length, arr_type.value());
        } else {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
    }
    // If its not a simple type and not an array type its a complex type, e.g. Opt<..> for example. This is not supported yet
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return std::nullopt;
}
