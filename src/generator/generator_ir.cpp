#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "lexer/lexer_utils.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/entity_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"

#include <stack>

void Generator::IR::init_builtin_types() {
    if (type_map.find("type.str") == type_map.end()) {
        IR::create_struct_type("type.str",
            {
                llvm::Type::getInt64Ty(context),                        // the length / dimensionality
                llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data (characters / the lenghts followed by row-major array)
            } //
        );
    }
    if (type_map.find("type.flint.err") == type_map.end()) {
        llvm::Type *str_type = type_map.at("type.str");
        IR::create_struct_type("type.flint.err",
            {
                llvm::Type::getInt32Ty(context), // ErrType ID (0 for 'none')
                llvm::Type::getInt32Ty(context), // ErrValue
                str_type->getPointerTo()         // ErrMessage
            } //
        );
    }
}

llvm::StructType *Generator::IR::create_struct_type( //
    const std::string &type_name,                    //
    const std::vector<llvm::Type *> &elements,       //
    const bool is_packed                             //
) {
    assert(!type_name.empty());
    if (type_map.find(type_name) != type_map.end()) {
        return type_map.at(type_name);
    }
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info]: Adding type '" << type_name << "' to the type_map" << DEFAULT << std::endl;
    }
    if (llvm::StructType *exists = llvm::StructType::getTypeByName(context, type_name)) {
        type_map[type_name] = exists;
        return exists;
    }
    llvm::StructType *type = llvm::StructType::create( //
        context,                                       //
        elements,                                      //
        type_name,                                     //
        is_packed                                      //
    );
    type_map[type_name] = type;
    return type;
}

llvm::StructType *Generator::IR::add_and_or_get_type( //
    llvm::Module *module,                             //
    const std::shared_ptr<Type> &type,                //
    const bool is_return_type,                        //
    const bool is_extern                              //
) {
    const std::string type_str = type->get_type_string(is_return_type);
    if (type_map.find(type_str) != type_map.end()) {
        return type_map.at(type_str);
    }
    if (type_str == "type.ret.void") {
        type_map[type_str] = IR::create_struct_type(type_str, {type_map.at("type.flint.err")});
        return type_map.at(type_str);
    }
    if (llvm::StructType *exists = llvm::StructType::getTypeByName(context, type_str)) {
        type_map[type_str] = exists;
        return exists;
    }
    if (is_return_type) {
        std::vector<llvm::Type *> types_vec;
        types_vec.emplace_back(type_map.at("type.flint.err"));
        if (type->get_variation() == Type::Variation::GROUP) {
            const GroupType *group_type = type->as<GroupType>();
            for (const auto &elem : group_type->types) {
                auto elem_type = get_type(module, elem, is_extern);
                types_vec.emplace_back(elem_type.second.first ? elem_type.first->getPointerTo() : elem_type.first);
            }
        } else {
            auto ret_type = get_type(module, type, is_extern);
            types_vec.emplace_back(ret_type.second.first ? ret_type.first->getPointerTo() : ret_type.first);
        }
        type_map[type_str] = IR::create_struct_type(type_str, types_vec);
        return type_map.at(type_str);
    }
    auto llvm_type = get_type(module, type, is_extern);
    assert(llvm_type.first->isStructTy());
    type_map[type_str] = llvm::cast<llvm::StructType>(llvm_type.first);
    return type_map.at(type_str);
}

llvm::Value *Generator::IR::generate_bitwidth_change( //
    llvm::IRBuilder<> &builder,                       //
    llvm::Value *value,                               //
    const unsigned int from_bitwidth,                 //
    const unsigned int to_bitwidth,                   //
    llvm::Type *to_type                               //
) {
    llvm::Value *correct_bitwidth_value = value;
    if (from_bitwidth != to_bitwidth) {
        llvm::Type *from_type = value->getType();

        llvm::Value *int_value = value;
        if (!from_type->isIntegerTy()) {
            llvm::Type *int_from_type = builder.getIntNTy(from_bitwidth);
            int_value = builder.CreateBitCast(value, int_from_type);
        }
        if (from_bitwidth < to_bitwidth) {
            // Need to extend
            llvm::Type *target_int_type = builder.getIntNTy(to_bitwidth);
            // Zero-extended (fill with zeroes) - assuming unsigned interpretation
            correct_bitwidth_value = builder.CreateZExt(int_value, target_int_type);
        } else {
            // Need to truncate
            llvm::Type *target_int_type = builder.getIntNTy(to_bitwidth);
            correct_bitwidth_value = builder.CreateTrunc(int_value, target_int_type);
        }
    }
    // Finally, bitcast to the target type if different from what we have
    if (correct_bitwidth_value->getType() != to_type) {
        return builder.CreateBitCast(correct_bitwidth_value, to_type);
    }
    return correct_bitwidth_value;
}

llvm::MDNode *Generator::IR::generate_weights(unsigned int true_weight, unsigned int false_weight) {
    return llvm::MDNode::get(context,
        {
            llvm::MDString::get(context, "branch_weights"),                                                      //
            llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), true_weight)), // weight of overflow
            llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), false_weight)) // weight of no overflow
        });
}

void Generator::IR::generate_forward_declarations(llvm::Module *module, const FileNode &file_node) {
    file_function_names[file_node.file_name] = {};
    for (const std::unique_ptr<DefinitionNode> &node : file_node.file_namespace->public_symbols.definitions) {
        if (node->get_variation() == DefinitionNode::Variation::FUNCTION) {
            // Create a forward declaration for the function only if it is not the main function!
            auto *function_node = node->as<FunctionNode>();
            if (function_node->name == "_main") {
                continue;
            }
            llvm::FunctionType *function_type = Function::generate_function_type(module, function_node);
            std::string function_name = function_node->file_hash.to_string() + "." + function_node->name;
            if (function_node->mangle_id.has_value()) {
                assert(!function_node->is_extern);
                function_name += "." + std::to_string(function_node->mangle_id.value());
            }
            module->getOrInsertFunction(function_name, function_type);
            file_function_names.at(file_node.file_name).emplace_back(function_name);
        }
    }
}

std::optional<llvm::Type *> Generator::IR::get_extern_type( //
    llvm::Module *module,                                   //
    const std::shared_ptr<Type> &type                       //
) {
    const auto type_variation = type->get_variation();
    if (type_variation == Type::Variation::MULTI) {
        const auto *multi_type = type->as<MultiType>();
        // Let's first look at how each multi-type behaves, considereing the 16 byte rule as well
        // They follow the same rules as above, as multi-types are passed as structs into FIP functions
        // We now see how each multi-type looks
        //
        //  bool8 -> i8 (handled in the normal `get_type` function)
        //  u8x2 -> i16
        //  u8x3 -> i24
        //  u8x4 -> i32
        //  u8x8 -> i64
        //
        //  i32x2 -> { i32, i32 } -> i64
        //  i32x3 -> { i32, i32, i32 } -> { i64, i32 }
        //  i32x4 -> { i32, i32, i32, i32 } -> { i64, i64 }
        //
        //  i64x2 -> { i64, i64 }
        //  i64x3 and i64x4 are greater than 16 bytes
        //
        //  f32x2 -> { f32, f32 } -> <2 x f32>
        //  f32x3 -> { f32, f32, f32 } -> { <2 x f32>, f32 }
        //  f32x4 -> { f32, f32, f32, f32 } -> { <2 x f32>, <2 x f32> }
        //
        //  f64x2 -> { f64, f64 }
        //  f64x3 and f64x4 are greater than 16 bytes
        //
        llvm::Type *element_type = get_type(module, multi_type->base_type).first;
        const std::string type_str = "type." + multi_type->to_string() + ".extern";
        if (type_str == "bool8") {
            // Handle the `bool8` type in the normal `get_type` function
            return std::nullopt;
        }
        if (multi_type->base_type->to_string() == "u8") {
            return llvm::Type::getIntNTy(context, multi_type->width * 8);
        }
        if (type_map.find(type_str) == type_map.end()) {
            std::vector<llvm::Type *> types;
            llvm::VectorType *vec2_type = llvm::VectorType::get(element_type, 2, false);
            const std::string base_type_str = multi_type->base_type->to_string();
            if (base_type_str == "f64" || base_type_str == "i64") {
                for (size_t i = 0; i < multi_type->width; i++) {
                    types.emplace_back(element_type);
                }
            } else if (multi_type->width == 2) {
                if (base_type_str == "f32") {
                    return vec2_type;
                } else if (base_type_str == "i32") {
                    return llvm::Type::getInt64Ty(context);
                }
            } else if (multi_type->width == 3) {
                if (base_type_str == "f32") {
                    types.emplace_back(vec2_type);
                } else if (base_type_str == "i32") {
                    types.emplace_back(llvm::Type::getInt64Ty(context));
                }
                types.emplace_back(element_type);
            } else {
                for (size_t i = 0; i < multi_type->width; i += 2) {
                    if (base_type_str == "f32") {
                        types.emplace_back(vec2_type);
                    } else {
                        types.emplace_back(llvm::Type::getInt64Ty(context));
                    }
                }
            }

            type_map[type_str] = IR::create_struct_type(type_str, types);
        }
        return type_map.at(type_str);
    }

    std::string type_str;
    switch (type_variation) {
        default:
            // This type does not need special handling as an extern type
            return std::nullopt;
        case Type::Variation::DATA: {
            const auto *data_type = type->as<DataType>();
            // Check if the type already exists in the map and return it directly if it does
            type_str = data_type->get_type_string() + ".extern";
            if (type_map.find(type_str) != type_map.end()) {
                return type_map[type_str];
            }
            break;
        }
        case Type::Variation::TUPLE: {
            const auto *tuple_type = type->as<TupleType>();
            // Check if the type already exists in the map and return it directly if it does
            type_str = tuple_type->get_type_string() + ".extern";
            if (type_map.find(type_str) != type_map.end()) {
                return type_map[type_str];
            }
            break;
        }
        case Type::Variation::GROUP: {
            const auto *group_type = type->as<GroupType>();
            // Check if the type already exists in the map and return it directly if it does
            type_str = group_type->get_type_string() + ".extern";
            if (type_map.find(type_str) != type_map.end()) {
                return type_map[type_str];
            }
            break;
        }
    }
    if (llvm::StructType *exists = llvm::StructType::getTypeByName(context, type_str)) {
        type_map[type_str] = exists;
        return exists;
    }
    // First we need to check the total size of the data structure. If it's bigger than 16 bytes the 16-byte-rule applies (we pass in
    // the struct by reference or we need to pass in a pointer to the returned struct as first argument)
    llvm::Type *_struct_type = get_type(module, type, false).first;
    assert(_struct_type->isStructTy());
    if (Allocation::get_type_size(module, _struct_type) > 16) {
        // The 16 byte rule applies, all values > 16 bytes are passed around as pointers
        return _struct_type->getPointerTo();
    }
    // The struct will be packed into n smaller values which are all <= 8 bytes in size
    // It follows these rules:
    //
    // 1. Types of same sizes will always be packed against each other without any padding, until the 8 bytes are full
    // 2. Types of different sizes will always be packed with padding in between them to keep them properly aligned
    // 3. Two floats filling up the whole 8 byte budget will result in a two-component vector `<2 x float>`
    // 4. The minimum size to fit the whole structure in applies
    //
    // Some example structures and their corresponding output types:
    //  { i8, i8, i8, i8, i8 } -> i40
    //      The four i8 values are packed together into a 40 bit integer
    //      They do not need any more space, so it stays a 40 bit integer
    //
    //  { f32, i32, i8 } -> { i64, i8 }
    //      The first i64 contains the encoded f32 and the i32, nothing special here
    //      Then the i8 follows directly, all looks normal here
    //
    //  { f32, i32, i8, i8 } -> { i64, i32 }
    //      The first i64 is just like above, nothing special
    //      But wait! Why is the second now an i32 and not an i16 as expected??? I have no idea
    //      Also, is it now [ i8, i8, _, _ ] or [ i8, _, i8, _ ] for the second one??
    //
    //  { f32, i32, i8, i8, i8 } -> { i64, i32 }
    //      This case should be pretty unabmiguous, the i32 should look like [ i8, i8, i8, _ ]
    //      But because of the previous example i am not so sure any more...
    //
    //  { f32, i32, i8, i8, i8, i8 } -> { i64, i32 }
    //      This is absolutely unambiguous
    //
    //  { f32, i32, i8, i8, i8, i8, i8 } -> { i64, i64 }
    //      Okay interesting, the second value is not `i40` any more...just like in the above cases, actually...
    //      So the second structure should look like [ i8, i8, i8, i8, i8, _, _, _ ] now
    //      This could also describe what happened above...
    //      i8 works fine, but i16 and i24 become i32.
    //      This tells me that only i8, i32 and i64 are valid values for the second value in the struct
    //
    //  { i8, i8, i8, f32, f32, i32 } -> { i64, i64 }
    //      The first three bytes of the first i64 contain the three i8 values
    //      The fourth byte of the first i64 is padding
    //      The next four bytes contain the f32 encoded into it
    //      The first four bytes of the second i64 contain the second f32 encoded
    //      The last four bytes of the second i64 contain the i32
    //      [ u8, u8, u8, _, f32 ], [ f32, i32 ]
    //
    //  { i8, i32, f32, i8 } -> { i64, i64 }
    //      The structure looks like this i would assume
    //      [ i8, _, _, _, i32 ], [ f32, i8, _, _, _ ]
    //
    //  { f32, f32, i32 } -> { <2 x f32>, i32 }
    //      The two float values become packed together into a two-component vector
    //      The i32 stays untouched
    //
    //  { i32, i32, i32 } -> { i64, i32 }
    //      i32s are not packed into vectors like floats, they are packed into an i64

    // We implement it as a two-phase approach. In the first phase we go through all elements and track indexing and needed padding etc
    // and put them onto a stack with the count of total elements in the stack and each element in the stack represents the offset of
    // the element within the output 8-byte pack, so we create two stacks because for data greater than 16 bytes the 16-byte-rule
    // applies
    // Padding is handled by just different offsets of the struct elements, the total size of the first stack is also tracked for
    // sub-64-byte packed results like packing 5 u8 values into one i40.
    llvm::StructType *struct_type = llvm::cast<llvm::StructType>(_struct_type);
    std::vector<llvm::Type *> elem_types = struct_type->elements();
    std::array<std::stack<unsigned int>, 2> stacks;
    unsigned int offset = 0;
    unsigned int first_size = 0;

    size_t elem_idx = 0;
    for (; elem_idx < elem_types.size(); elem_idx++) {
        size_t elem_size = Allocation::get_type_size(module, elem_types.at(elem_idx));
        // We can only pack the element into the stack if there is enough space left
        if (8 - offset < elem_size) {
            break;
        }
        // If the current offset is 0 we can simply put in the element into the stack without further checks
        if (offset == 0) {
            stacks[0].push(0);
            offset += elem_size;
            first_size += elem_size;
            continue;
        }
        // Now we simply need to check where in the 8 byte structure we need to put the elements in. We can do this by calculating the
        // padding based on the type alignment, the alignment is just the elem_size in our case because we work with types <= 8 bytes in
        // size.
        // If the offset is divisible by the element size it does not need to be changed. If it is not divisible we need to clamp it to
        // the next divisible offset value, so if current offset is 2 but element size is 4 the offset needs to be clamped to 4
        uint8_t elem_offset = offset;
        if (offset % elem_size != 0) {
            elem_offset = ((elem_size + offset) / elem_size) * elem_size;
        }
        if (elem_offset == 8) {
            // This element does not fit into this stack, we need to put it into the next stack
            break;
        }
        stacks[0].push(elem_offset);
        offset = elem_offset + elem_size;
        first_size = elem_offset + elem_size;
    }
    offset = 0;
    for (; elem_idx < elem_types.size(); elem_idx++) {
        size_t elem_size = Allocation::get_type_size(module, elem_types.at(elem_idx));
        // For the second stack we actually can assert that enough space is left in here, since otherwise the 16-byte rule should have
        // applied
        assert(8 - offset >= elem_size);
        // If the current offset is 0 we can simply put in the element into the stack without further checks
        if (offset == 0) {
            stacks[1].push(0);
            offset += elem_size;
            continue;
        }
        uint8_t elem_offset = offset;
        if (offset % elem_size != 0) {
            elem_offset = ((elem_size + offset) / elem_size) * elem_size;
        }
        // This element does not fit into this stack, this should not happen since it's the last stack
        assert(elem_offset != 8);
        stacks[1].push(elem_offset);
        offset = elem_offset + elem_size;
    }
    assert(elem_idx == elem_types.size());
    elem_idx = 0;
    // Now we reach the second phase, we have figured out how to put the elements into the stacks, now we can create the types
    if (stacks[1].empty()) {
        // Special case for when the number of elements in the first stack is equal to the size of the first structure. This means that
        // we pack multiple u8 values into one, for them we can get even i40, i48 or i56 results
        if (stacks[0].size() == first_size) {
            return llvm::Type::getIntNTy(context, first_size * 8);
        } else if (stacks[0].size() == 1) {
            return elem_types.front();
        } else if (stacks[0].size() == 2) {
            if (elem_types.front()->isFloatingPointTy() && elem_types.at(1)->isFloatingPointTy()) {
                return llvm::VectorType::get(elem_types.front(), 2, false);
            }
        }
    }
    // Because we only need to fill two 8-byte containers we can have an array here again
    std::array<llvm::Type *, 2> types;
    types[0] = nullptr;
    types[1] = nullptr;
    if (stacks[0].size() == 1) {
        if (elem_types.front()->isFloatingPointTy()) {
            types[0] = llvm::Type::getDoubleTy(context);
        } else {
            types[0] = llvm::Type::getInt64Ty(context);
        }
    } else if (stacks[0].size() == 2) {
        if (elem_types.front()->isFloatingPointTy() && elem_types.at(1)->isFloatingPointTy()) {
            types[0] = llvm::VectorType::get(elem_types.front(), 2, false);
        } else {
            types[0] = llvm::Type::getInt64Ty(context);
        }
    } else {
        types[0] = llvm::Type::getInt64Ty(context);
    }
    if (stacks[1].size() == 1) {
        types[1] = elem_types.at(stacks[0].size());
    } else if (stacks[1].size() == 2) {
        if (elem_types.at(stacks[0].size())->isFloatingPointTy() && elem_types.at(stacks[0].size() + 1)->isFloatingPointTy()) {
            types[1] = llvm::VectorType::get(elem_types.front(), 2, false);
        } else {
            types[1] = llvm::Type::getInt64Ty(context);
        }
    } else {
        types[1] = llvm::Type::getInt64Ty(context);
    }
    assert(types[0] != nullptr);
    assert(types[1] != nullptr);

    // Create a struct of the computed stack types
    std::vector<llvm::Type *> types_vec;
    types_vec.push_back(types[0]);
    types_vec.push_back(types[1]);
    llvm::StructType *out_struct = IR::create_struct_type(type_str, types_vec);
    type_map[type_str] = out_struct;
    return out_struct;
}

std::pair<llvm::Type *, std::pair<bool, bool>> Generator::IR::get_type( //
    llvm::Module *module,                                               //
    const std::shared_ptr<Type> &type,                                  //
    const bool is_extern                                                //
) {
    if (is_extern) {
        auto ext_type = get_extern_type(module, type);
        if (ext_type.has_value()) {
            return {ext_type.value(), {false, false}};
        }
    }
    switch (type->get_variation()) {
        case Type::Variation::ALIAS: {
            const auto *alias_type = type->as<AliasType>();
            return get_type(module, alias_type->type, is_extern);
        }
        case Type::Variation::ARRAY: {
            // Arrays are *always* of type 'str', as a 'str' is just one i64 followed by a byte array
            return {type_map.at("type.str")->getPointerTo(), {false, false}};
        }
        case Type::Variation::DATA: {
            const auto *data_type = type->as<DataType>();
            // Check if its a known data type
            const std::string type_str = data_type->get_type_string();
            if (type_map.find(type_str) != type_map.end()) {
                return {type_map.at(type_str), {true, true}};
            }
            llvm::StructType *struct_type = llvm::StructType::create(context, type_str);
            // Create an opaque struct type and store it in the map before trying to resolve the field types to prevent circles
            type_map[type_str] = struct_type;
            // Now process the field types
            std::vector<llvm::Type *> field_types;
            for (auto field_it = data_type->data_node->fields.begin(); field_it != data_type->data_node->fields.end(); ++field_it) {
                auto pair = get_type(module, field_it->second);
                if (pair.second.first && field_it->second->get_variation() != Type::Variation::OPTIONAL) {
                    field_types.emplace_back(pair.first->getPointerTo());
                } else {
                    field_types.emplace_back(pair.first);
                }
            }
            // Set the body of the struct now that we have all field types
            struct_type->setBody(field_types, false); // false = not packed
            return {struct_type, {true, true}};
        }
        case Type::Variation::ENTITY: {
            const auto *entity_type = type->as<EntityType>();
            // Check if it's a known entity type
            const std::string type_str = entity_type->get_type_string();
            if (type_map.find(type_str) != type_map.end()) {
                return {type_map.at(type_str), {false, false}};
            }
            // Create the entity type, it's just a struct containing pointers to the entities' defined data
            std::vector<llvm::Type *> field_types;
            for (const auto &data_node : entity_type->entity_node->data_modules) {
                Namespace *data_namespace = Resolver::get_namespace_from_hash(data_node->file_hash);
                std::shared_ptr<Type> data_type = data_namespace->get_type_from_str(data_node->name).value();
                field_types.emplace_back(IR::get_type(module, data_type).first->getPointerTo());
            }
            type_map[type_str] = IR::create_struct_type(type_str, field_types);
            return {type_map.at(type_str), {false, false}};
        }
        case Type::Variation::ENUM:
            return {llvm::Type::getInt32Ty(context), {false, false}};
        case Type::Variation::ERROR_SET:
            return {type_map.at("type.flint.err"), {false, true}};
        case Type::Variation::FUNC: {
            const auto *func_type = type->as<FuncType>();
            // Check if it's a known func type
            const std::string type_str = func_type->get_type_string();
            if (type_map.find(type_str) != type_map.end()) {
                return {type_map.at(type_str), {false, false}};
            }
            // Check if the func type even requires any data. If it does not then it's type cannot be created
            // TODO: Once CTDTs exist this check is no longer required as then any function could be linked to / hooked into any other
            // function and the call is determined at runtime via a vtable-like system. But until this is implemented all
            // func-modules-as-interfaces are static and compile-time known which means they need at least one required data to even be a
            // valid type, since a struct with no members would crash llvm
            if (func_type->func_node->required_data.empty()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return {nullptr, {false, false}};
            }
            // Create the func type, it's just a struct containing pointers to the defined data
            std::vector<llvm::Type *> field_types;
            for (const auto &[data_type, accessor_name] : func_type->func_node->required_data) {
                field_types.emplace_back(IR::get_type(module, data_type).first->getPointerTo());
            }
            type_map[type_str] = IR::create_struct_type(type_str, field_types);
            return {type_map.at(type_str), {false, false}};
        }
        case Type::Variation::GROUP: {
            const auto *group_type = type->as<GroupType>();
            const std::string type_str = group_type->get_type_string();
            std::vector<llvm::Type *> type_vector;
            for (const auto &tup_type : group_type->types) {
                auto pair = get_type(module, tup_type);
                if (pair.second.first && tup_type->get_variation() != Type::Variation::OPTIONAL) {
                    pair.first = pair.first->getPointerTo();
                }
                type_vector.emplace_back(pair.first);
            }
            if (type_map.find(type_str) == type_map.end()) {
                type_map[type_str] = IR::create_struct_type(type_str, type_vector);
            }
            return {type_map.at(type_str), {false, true}};
        }
        case Type::Variation::MULTI: {
            const auto *multi_type = type->as<MultiType>();
            if (type->to_string() == "bool8") {
                return {llvm::Type::getInt8Ty(context), {false, false}};
            }
            llvm::Type *element_type = get_type(module, multi_type->base_type).first;
            llvm::VectorType *vector_type = llvm::VectorType::get(element_type, multi_type->width, false);
            return {vector_type, {false, false}};
        }
        case Type::Variation::OPTIONAL: {
            const auto *optional_type = type->as<OptionalType>();
            const std::string opt_str = type->get_type_string();
            if (type_map.find(opt_str) == type_map.end()) {
                auto pair = get_type(module, optional_type->base_type);
                if (pair.second.first) {
                    pair.first = pair.first->getPointerTo();
                }
                type_map[opt_str] = IR::create_struct_type(opt_str, {llvm::Type::getInt1Ty(context), pair.first});
            }
            return {type_map.at(opt_str), {false, true}};
        }
        case Type::Variation::POINTER: {
            const auto *pointer_type = type->as<PointerType>();
            const auto pair = get_type(module, pointer_type->base_type);
            return {pair.first->getPointerTo(), {false, false}};
        }
        case Type::Variation::PRIMITIVE: {
            const auto *primitive_type = type->as<PrimitiveType>();
            if (primitive_type->type_name == "type.flint.str") {
                // A string is a struct of type 'type { i64, [0 x i8] }'
                return {type_map.at("type.str"), {false, false}};
            }
            if (primitive_type->type_name == "type.flint.str.lit") {
                return {llvm::Type::getInt8Ty(context)->getPointerTo(), {false, false}};
            }
            if (primitive_type->type_name == "anyerror") {
                return {type_map.at("type.flint.err"), {false, false}};
            }
            if (primitives.find(primitive_type->type_name) != primitives.end()) {
                switch (primitives.at(primitive_type->type_name)) {
                    default:
                        THROW_BASIC_ERR(ERR_GENERATING);
                        return {nullptr, {false, false}};
                    case TOK_I32:
                    case TOK_U32:
                        return {llvm::Type::getInt32Ty(context), {false, false}};
                    case TOK_I64:
                    case TOK_U64:
                        return {llvm::Type::getInt64Ty(context), {false, false}};
                    case TOK_F32:
                        return {llvm::Type::getFloatTy(context), {false, false}};
                    case TOK_F64:
                        return {llvm::Type::getDoubleTy(context), {false, false}};
                    case TOK_FLINT:
                        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                        return {nullptr, {false, false}};
                    case TOK_U8:
                        return {llvm::Type::getInt8Ty(context), {false, false}};
                    case TOK_STR: {
                        if (is_extern) {
                            // If it's an extern call we pass in the c_str
                            return {llvm::Type::getInt8Ty(context)->getPointerTo(), {false, false}};
                        }
                        // A string is a struct of type 'type { i64, [0 x i8] }'
                        return {type_map.at("type.str")->getPointerTo(), {false, false}};
                    }
                    case TOK_BOOL:
                        return {llvm::Type::getInt1Ty(context), {false, false}};
                    case TOK_VOID:
                        return {llvm::Type::getVoidTy(context), {false, false}};
                }
            }
            break;
        }
        case Type::Variation::RANGE:
            // TODO: Add this?
            break;
        case Type::Variation::TUPLE: {
            const auto *tuple_type = type->as<TupleType>();
            const std::string tuple_str = type->get_type_string();
            std::vector<llvm::Type *> type_vector;
            for (const auto &tup_type : tuple_type->types) {
                auto pair = get_type(module, tup_type);
                if (pair.second.first && tup_type->get_variation() != Type::Variation::OPTIONAL) {
                    pair.first = pair.first->getPointerTo();
                }
                type_vector.emplace_back(pair.first);
            }
            if (type_map.find(tuple_str) == type_map.end()) {
                type_map[tuple_str] = IR::create_struct_type(tuple_str, type_vector);
            }
            return {type_map.at(tuple_str), {false, true}};
        }
        case Type::Variation::UNKNOWN:
            // TODO: Add this?
            break;
        case Type::Variation::VARIANT: {
            const auto *variant_type = type->as<VariantType>();
            if (variant_type->is_err_variant) {
                return {type_map.at("type.flint.err"), {false, true}};
            }
            const std::string var_str = type->get_type_string();
            // Check if its a known data type
            if (type_map.find(var_str) == type_map.end()) {
                unsigned int max_size = 0;
                if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                    const auto &possible_types = std::get<VariantNode *const>(variant_type->var_or_list)->possible_types;
                    for (const auto &[_, variation] : possible_types) {
                        const unsigned int type_size = Allocation::get_type_size(module, get_type(module, variation).first);
                        if (type_size > max_size) {
                            max_size = type_size;
                        }
                    }
                } else {
                    const auto &possible_types = std::get<std::vector<std::shared_ptr<Type>>>(variant_type->var_or_list);
                    for (const auto &variation : possible_types) {
                        const unsigned int type_size = Allocation::get_type_size(module, get_type(module, variation).first);
                        if (type_size > max_size) {
                            max_size = type_size;
                        }
                    }
                }
                llvm::StructType *variant_struct_type = IR::create_struct_type(var_str,
                    {
                        llvm::Type::getInt8Ty(context),                                // The tag which type it is (starts at 1)
                        llvm::ArrayType::get(llvm::Type::getInt8Ty(context), max_size) // The actual data array, being *N* bytes of data
                    } //
                );
                type_map[var_str] = variant_struct_type;
            }
            return {type_map.at(var_str), {false, true}};
        }
    }
    // Pointer to non-supported type
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return {nullptr, {false, false}};
}

llvm::Value *Generator::IR::get_default_value_of_type(llvm::IRBuilder<> &builder, llvm::Module *module, const std::shared_ptr<Type> &type) {
    const std::string type_string = type->to_string();
    if (type_string == "str") {
        return builder.CreateCall(Module::String::string_manip_functions.at("create_str"), {builder.getInt64(0)}, "empty_string");
    }
    return get_default_value_of_type(IR::get_type(module, type).first);
}

llvm::Value *Generator::IR::get_default_value_of_type(llvm::Type *type) {
    if (type->isVoidTy()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }
    return llvm::Constant::getNullValue(type);
}

llvm::Value *Generator::IR::generate_const_string(llvm::Module *module, const std::string &str) {
    std::string str_name = "string.global." + std::to_string(global_strings.size());
    const bool str_exists = global_strings.find(str) != global_strings.end();
    if (str_exists) {
        str_name = global_strings.at(str);
        if (auto *existing = module->getNamedGlobal(str_name)) {
            return existing;
        }
    }
    llvm::Constant *string_data = nullptr;
    llvm::Type *string_type = nullptr;
    if (!str_exists) {
        string_data = llvm::ConstantDataArray::getString(context, str, true);
        string_type = string_data->getType();
    } else {
        // Create the array type manually: [length+1 x i8]
        string_type = llvm::ArrayType::get(llvm::Type::getInt8Ty(context), str.length() + 1);
    }
    llvm::GlobalVariable *string_global = new llvm::GlobalVariable(                                             //
        *module, string_type, true,                                                                             //
        generating_builtin_module ? llvm::GlobalVariable::InternalLinkage : llvm::GlobalValue::ExternalLinkage, //
        string_data, str_name                                                                                   //
    );
    if (!str_exists) {
        global_strings[str] = str_name;
    }
    return string_global;
}

bool Generator::IR::generate_enum_value_strings( //
    llvm::Module *module,                        //
    const std::string &hash,                     //
    const std::string &enum_name,                //
    const std::vector<std::string> &enum_values  //
) {
    // Generate the type name array for later typecast lookups
    // Create individual string constants
    const std::string key = hash + "." + enum_name;
    if (enum_name_arrays_map.find(key) != enum_name_arrays_map.end()) {
        return true;
    }
    std::vector<llvm::Constant *> string_pointers;
    llvm::Type *const i8_ptr_type = llvm::Type::getInt8Ty(context)->getPointerTo();

    for (size_t i = 0; i < enum_values.size(); ++i) {
        // Create the string constant data
        llvm::Constant *string_data = llvm::ConstantDataArray::getString(context, enum_values[i], true);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
        // Create a global variable to hold the string
        llvm::GlobalVariable *string_global = new llvm::GlobalVariable(                             //
            *module, string_data->getType(), true, llvm::GlobalValue::ExternalLinkage, string_data, //
            hash + ".enum." + enum_name + ".name." + std::to_string(i)                              //
        );
#pragma GCC diagnostic pop

        // Get pointer to the string data (cast to i8*)
        llvm::Constant *string_ptr = llvm::ConstantExpr::getBitCast(string_global, i8_ptr_type);
        string_pointers.push_back(string_ptr);
    }

    // Create the array type and global array
    llvm::ArrayType *array_type = llvm::ArrayType::get(i8_ptr_type, enum_values.size());
    llvm::Constant *string_array = llvm::ConstantArray::get(array_type, string_pointers);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
    llvm::GlobalVariable *global_string_array = new llvm::GlobalVariable(                                                   //
        *module, array_type, true, llvm::GlobalValue::ExternalLinkage, string_array, hash + ".enum." + enum_name + ".names" //
    );
#pragma GCC diagnostic pop

    // Optional Windows compatibility settings
#ifdef __WIN32__
    global_string_array->setDLLStorageClass(llvm::GlobalValue::DefaultStorageClass);
#endif
    enum_name_arrays_map[key] = global_string_array;
    return true;
}

llvm::Value *Generator::IR::generate_err_value( //
    llvm::IRBuilder<> &builder,                 //
    llvm::Module *module,                       //
    const unsigned int err_id,                  //
    const unsigned int err_value,               //
    const std::string &err_message              //
) {
    llvm::StructType *err_type = type_map.at("type.flint.err");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
    llvm::Value *err_struct = get_default_value_of_type(err_type);
    err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_id), {0}, "insert_err_type_id");
    err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_value), {1}, "insert_err_value");
    llvm::Value *message_str = IR::generate_const_string(module, err_message);
    llvm::Value *error_message = builder.CreateCall(init_str_fn, {message_str, builder.getInt64(err_message.size())}, "err_message");
    err_struct = builder.CreateInsertValue(err_struct, error_message, {2}, "insert_err_message");
    return err_struct;
}

void Generator::IR::generate_debug_print(    //
    llvm::IRBuilder<> *builder,              //
    llvm::Module *module,                    //
    const std::string &format,               //
    const std::vector<llvm::Value *> &values //
) {
    if (!DEBUG_MODE) {
        return;
    }
    llvm::Function *printf_fn = c_functions.at(PRINTF);
    llvm::Value *debug_str = generate_const_string(module, "DEBUG: ");
    builder->CreateCall(printf_fn, {debug_str});

    llvm::Value *format_str = generate_const_string(module, format);
    std::vector<llvm::Value *> args{format_str};
    args.insert(args.end(), values.begin(), values.end());
    builder->CreateCall(printf_fn, {args});

    llvm::Value *endl_str = generate_const_string(module, "\n");
    builder->CreateCall(printf_fn, {endl_str});
}

llvm::LoadInst *Generator::IR::aligned_load(llvm::IRBuilder<> &builder, llvm::Type *type, llvm::Value *ptr, const std::string &name) {
    const unsigned int alignment = Allocation::calculate_type_alignment(type);
    llvm::LoadInst *load_inst = builder.CreateLoad(type, ptr, name);
    load_inst->setAlignment(llvm::Align(alignment));
    return load_inst;
}

llvm::StoreInst *Generator::IR::aligned_store(llvm::IRBuilder<> &builder, llvm::Value *value, llvm::Value *ptr) {
    const unsigned int alignment = Allocation::calculate_type_alignment(value->getType());
    llvm::StoreInst *store_inst = builder.CreateStore(value, ptr);
    store_inst->setAlignment(llvm::Align(alignment));
    return store_inst;
}
