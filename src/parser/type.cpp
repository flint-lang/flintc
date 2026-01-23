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
    get_primitive_type("type.flint.str.lit");
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
