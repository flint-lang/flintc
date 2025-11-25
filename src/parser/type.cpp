#include "parser/type/type.hpp"

#include "parser/type/array_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"

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
    std::shared_ptr<Type> u8_type = get_primitive_type("u8");
    get_primitive_type("anyerror");
    get_primitive_type("int");
    get_primitive_type("float");

    add_type(std::make_shared<MultiType>(bool_type, 8));
    add_type(std::make_shared<MultiType>(u8_type, 2));
    add_type(std::make_shared<MultiType>(u8_type, 3));
    add_type(std::make_shared<MultiType>(u8_type, 4));
    add_type(std::make_shared<MultiType>(u8_type, 8));
    add_type(std::make_shared<MultiType>(i32_type, 2));
    add_type(std::make_shared<MultiType>(i32_type, 3));
    add_type(std::make_shared<MultiType>(i32_type, 4));
    add_type(std::make_shared<MultiType>(i32_type, 8));
    add_type(std::make_shared<MultiType>(i64_type, 2));
    add_type(std::make_shared<MultiType>(i64_type, 3));
    add_type(std::make_shared<MultiType>(i64_type, 4));
    add_type(std::make_shared<MultiType>(f32_type, 2));
    add_type(std::make_shared<MultiType>(f32_type, 3));
    add_type(std::make_shared<MultiType>(f32_type, 4));
    add_type(std::make_shared<MultiType>(f32_type, 8));
    add_type(std::make_shared<MultiType>(f64_type, 2));
    add_type(std::make_shared<MultiType>(f64_type, 3));
    add_type(std::make_shared<MultiType>(f64_type, 4));
    add_type(std::make_shared<ArrayType>(1, str_type));
}

void Type::clear_types() {
    types.clear();
}

bool Type::add_type(const std::shared_ptr<Type> &type_to_add) {
    std::unique_lock<std::shared_mutex> lock(types_mutex);
    return types.emplace(type_to_add->to_string(), type_to_add).second;
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
