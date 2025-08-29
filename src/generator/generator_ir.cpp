#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "lexer/lexer_utils.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include "llvm/IR/Constants.h"

#include <stack>

llvm::StructType *Generator::IR::add_and_or_get_type( //
    llvm::Module *module,                             //
    const std::shared_ptr<Type> &type,                //
    const bool is_return_type,                        //
    const bool is_extern                              //
) {
    std::vector<std::shared_ptr<Type>> types;
    std::string types_str = is_return_type ? "ret." : "";
    if (const GroupType *group_type = dynamic_cast<const GroupType *>(type.get())) {
        types = group_type->types;
    } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(type.get())) {
        types_str = "tuple.";
        types = tuple_type->types;
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(type.get())) {
        if (!is_return_type) {
            types_str = "data." + data_type->data_node->name;
            for (auto &[_, field_type] : data_type->data_node->fields) {
                types.emplace_back(field_type);
            }
        } else {
            types.emplace_back(type);
        }
    } else if (type->to_string() != "void") {
        types.emplace_back(type);
    }
    for (auto it = types.begin(); it < types.end(); ++it) {
        types_str.append((*it)->to_string());
        if (std::distance(it, types.end()) > 1) {
            types_str.append("_");
        }
    }
    // If its a return type it can be void, if not it cant
    assert(is_return_type || types_str != "");
    if (types_str == "ret.") {
        types_str = "ret.void";
    }
    if (type_map.find(types_str) != type_map.end()) {
        return type_map[types_str];
    }

    // Get the return types
    std::vector<llvm::Type *> types_vec;
    if (is_return_type) {
        types_vec.reserve(types.size() + 1);
        // First element is always the error struct { i32, i32, str* }
        types_vec.push_back(type_map.at("__flint_type_err"));
    } else {
        types_vec.reserve(types.size());
    }
    // Rest of the elements are the return types
    for (const auto &ret_value : types) {
        auto ret_type = get_type(module, ret_value, is_extern);
        if (ret_type.second && !dynamic_cast<const OptionalType *>(ret_value.get())) {
            types_vec.emplace_back(ret_type.first->getPointerTo());
        } else {
            types_vec.emplace_back(ret_type.first);
        }
    }
    llvm::ArrayRef<llvm::Type *> return_types_arr(types_vec);
    type_map[types_str] = llvm::StructType::create( //
        context,                                    //
        return_types_arr,                           //
        "type." + types_str,                        //
        false                                       //
    );
    return type_map[types_str];
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
    unsigned int mangle_id = 1;
    file_function_mangle_ids[file_node.file_name] = {};
    file_function_names[file_node.file_name] = {};
    for (const std::unique_ptr<ASTNode> &node : file_node.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            // Create a forward declaration for the function only if it is not the main function!
            if (function_node->name != "_main") {
                llvm::FunctionType *function_type = Function::generate_function_type(module, function_node);
                module->getOrInsertFunction(function_node->name, function_type);
                file_function_mangle_ids.at(file_node.file_name).emplace(function_node->name, mangle_id++);
                file_function_names.at(file_node.file_name).emplace_back(function_node->name);
            }
        }
    }
}

std::optional<llvm::Type *> Generator::IR::get_extern_type( //
    llvm::Module *module,                                   //
    const std::shared_ptr<Type> &type                       //
) {
    if (const DataType *data_type = dynamic_cast<const DataType *>(type.get())) {
        // Check if the type already exists in the map and return it directly if it does
        const std::string type_str = "type.data." + data_type->to_string() + ".extern";
        if (type_map.find(type_str) != type_map.end()) {
            return type_map[type_str];
        }
        // First we need to check the total size of the data structure. If it's bigger than 16 bytes the 16-byte-rule applies (we pass in
        // the struct by reference or we need to pass in a pointer to the returned struct as first argument)
        llvm::Type *_struct_type = get_type(module, type, false).first;
        assert(_struct_type->isStructTy());
        if (Allocation::get_type_size(module, _struct_type) > 16) {
            // The 16 byte rule applies, but we will not support it for now
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return std::nullopt;
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
                stacks[0].push(0);
                offset += elem_size;
                continue;
            }
            uint8_t elem_offset = ((elem_size + offset) / elem_size) * elem_size;
            // This element does not fit into this stack, this should not happen since it's the last stack
            assert(elem_offset != 8);
            stacks[0].push(elem_offset);
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
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;

        // Create a struct of the computed stack types
        // llvm::StructType *out_struct = llvm::StructType::create(context, slots, type_str, false);
        // type_map[type_str] = out_struct;
        // return out_struct;
    } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(type.get())) {
        // Let's first look at how each multi-type behaves, considereing the 16 byte rule as well
        // They follow the same rules as above, as multi-types are passed as structs into FIP functions
        // We now see how each multi-type looks
        //
        //  bool8 -> i8 (handled in the normal `get_type` function)
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

            llvm::StructType *struct_type = llvm::StructType::create( //
                context, types, type_str, false                       //
            );
            type_map[type_str] = struct_type;
        }
        return type_map.at(type_str);
    }
    return std::nullopt;
}

std::pair<llvm::Type *, bool> Generator::IR::get_type( //
    llvm::Module *module,                              //
    const std::shared_ptr<Type> &type,                 //
    const bool is_extern                               //
) {
    if (is_extern) {
        auto ext_type = get_extern_type(module, type);
        if (ext_type.has_value()) {
            return {ext_type.value(), false};
        }
    }
    if (dynamic_cast<const ErrorSetType *>(type.get())) {
        if (type_map.find("__flint_type_err") == type_map.end()) {
            llvm::Type *str_type = get_type(module, Type::get_primitive_type("str")).first;
            type_map["__flint_type_err"] = llvm::StructType::create( //
                context,                                             //
                {
                    llvm::Type::getInt32Ty(context), // ErrType ID (0 for 'none')
                    llvm::Type::getInt32Ty(context), // ErrValue
                    str_type->getPointerTo()         // ErrMessage
                },                                   //
                "__flint_type_err",                  //
                false                                // is not packed, it's padded
            );
        }
        return {type_map.at("__flint_type_err"), false};
    } else if (const PrimitiveType *primitive_type = dynamic_cast<const PrimitiveType *>(type.get())) {
        if (primitive_type->type_name == "__flint_type_str_struct") {
            // A string is a struct of type 'type { i64, [0 x i8] }'
            if (type_map.find("type.str") == type_map.end()) {
                llvm::StructType *str_type = llvm::StructType::create( //
                    context,                                           //
                    {
                        llvm::Type::getInt64Ty(context),                        // len of string
                        llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data
                    },                                                          //
                    "type.str",
                    false // is not packed, its padded
                );
                type_map["type.str"] = str_type;
            }
            return {type_map.at("type.str"), false};
        }
        if (primitive_type->type_name == "__flint_type_str_lit") {
            return {llvm::Type::getInt8Ty(context)->getPointerTo(), false};
        }
        if (primitive_type->type_name == "anyerror") {
            return {type_map.at("__flint_type_err"), false};
        }
        if (primitives.find(primitive_type->type_name) != primitives.end()) {
            switch (primitives.at(primitive_type->type_name)) {
                default:
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return {nullptr, false};
                case TOK_I32:
                case TOK_U32:
                    return {llvm::Type::getInt32Ty(context), false};
                case TOK_I64:
                case TOK_U64:
                    return {llvm::Type::getInt64Ty(context), false};
                case TOK_F32:
                    return {llvm::Type::getFloatTy(context), false};
                case TOK_F64:
                    return {llvm::Type::getDoubleTy(context), false};
                case TOK_FLINT:
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return {nullptr, false};
                case TOK_U8:
                    return {llvm::Type::getInt8Ty(context), false};
                case TOK_STR: {
                    if (is_extern) {
                        // If it's an extern call we pass in the c_str
                        return {llvm::Type::getInt8Ty(context)->getPointerTo(), false};
                    }
                    // A string is a struct of type 'type { i64, [0 x i8] }'
                    if (type_map.find("type.str") == type_map.end()) {
                        llvm::StructType *str_type = llvm::StructType::create( //
                            context,                                           //
                            {
                                llvm::Type::getInt64Ty(context),                        // len of string
                                llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data
                            },                                                          //
                            "type.str",
                            false // is not packed, its padded
                        );
                        type_map["type.str"] = str_type;
                    }
                    return {type_map.at("type.str")->getPointerTo(), false};
                }
                case TOK_BOOL:
                    return {llvm::Type::getInt1Ty(context), false};
                case TOK_VOID:
                    return {llvm::Type::getVoidTy(context), false};
            }
        }
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(type.get())) {
        // Check if its a known data type
        const std::string type_str = "type.data." + data_type->data_node->name;
        if (type_map.find(type_str) != type_map.end()) {
            return {type_map.at(type_str), true};
        }
        // Create an opaque struct type and store it in the map before trying to resolve the field types to prevent circles
        llvm::StructType *struct_type = llvm::StructType::create(context, type_str);
        type_map[type_str] = struct_type;
        // Now process the field types
        std::vector<llvm::Type *> field_types;
        for (auto field_it = data_type->data_node->fields.begin(); field_it != data_type->data_node->fields.end(); ++field_it) {
            auto pair = get_type(module, field_it->second);
            if (pair.second && !dynamic_cast<const OptionalType *>(field_it->second.get())) {
                field_types.emplace_back(pair.first->getPointerTo());
            } else {
                field_types.emplace_back(pair.first);
            }
        }
        // Set the body of the struct now that we have all field types
        struct_type->setBody(field_types, false); // false = not packed
        return {struct_type, true};
    } else if (dynamic_cast<ArrayType *>(type.get())) {
        // Arrays are *always* of type 'str', as a 'str' is just one i64 followed by a byte array
        if (type_map.find("type.str") == type_map.end()) {
            llvm::StructType *str_type = llvm::StructType::create( //
                context,                                           //
                {
                    llvm::Type::getInt64Ty(context),                        // the dimensionality
                    llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data (the lenghts followed by the row-major array)
                },                                                          //
                "type.str",
                false // is not packed, its padded
            );
            type_map["type.str"] = str_type;
        }
        return {type_map.at("type.str")->getPointerTo(), false};
    } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(type.get())) {
        if (type->to_string() == "bool8") {
            return {llvm::Type::getInt8Ty(context), false};
        }
        llvm::Type *element_type = get_type(module, multi_type->base_type).first;
        llvm::VectorType *vector_type = llvm::VectorType::get(element_type, multi_type->width, false);
        return {vector_type, false};
    } else if (dynamic_cast<const EnumType *>(type.get())) {
        return {llvm::Type::getInt32Ty(context), false};
    } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(type.get())) {
        std::string tuple_str = type->to_string();
        std::vector<llvm::Type *> type_vector;
        for (const auto &tup_type : tuple_type->types) {
            auto pair = get_type(module, tup_type);
            if (pair.second && !dynamic_cast<const OptionalType *>(tup_type.get())) {
                pair.first = pair.first->getPointerTo();
            }
            type_vector.emplace_back(pair.first);
        }
        if (type_map.find(tuple_str) == type_map.end()) {
            llvm::ArrayRef<llvm::Type *> type_array(type_vector);
            llvm::StructType *llvm_tuple_type = llvm::StructType::create(context, type_array, tuple_str, false);
            type_map[tuple_str] = llvm_tuple_type;
        }
        return {type_map.at(tuple_str), true};
    } else if (const OptionalType *optional_type = dynamic_cast<const OptionalType *>(type.get())) {
        const std::string opt_str = type->to_string();
        if (type_map.find(opt_str) == type_map.end()) {
            auto pair = get_type(module, optional_type->base_type);
            if (pair.second) {
                pair.first = pair.first->getPointerTo();
            }
            llvm::StructType *llvm_opt_type = llvm::StructType::create(                                  //
                context, {llvm::Type::getInt1Ty(context), pair.first}, "type.optional." + opt_str, false //
            );
            type_map[opt_str] = llvm_opt_type;
        }
        return {type_map.at(opt_str), true};
    } else if (const VariantType *variant_type = dynamic_cast<const VariantType *>(type.get())) {
        if (variant_type->is_err_variant) {
            if (type_map.find("__flint_type_err") == type_map.end()) {
                llvm::Type *str_type = get_type(module, Type::get_primitive_type("str")).first;
                type_map["__flint_type_err"] = llvm::StructType::create( //
                    context,                                             //
                    {
                        llvm::Type::getInt32Ty(context), // ErrType ID (0 for 'none')
                        llvm::Type::getInt32Ty(context), // ErrValue
                        str_type->getPointerTo()         // ErrMessage
                    },                                   //
                    "__flint_type_err",                  //
                    false                                // is not packed, it's padded
                );
            }
            return {type_map.at("__flint_type_err"), false};
        }
        const std::string var_str = type->to_string();
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
            llvm::StructType *variant_struct_type = llvm::StructType::create( //
                context,                                                      //
                {
                    llvm::Type::getInt8Ty(context),                                // The tag which type it is (starts at 1)
                    llvm::ArrayType::get(llvm::Type::getInt8Ty(context), max_size) // The actual data array, being *N* bytes of data
                },                                                                 //
                "type.variant." + var_str,
                false // is not packed, its padded
            );
            type_map[var_str] = variant_struct_type;
        }
        return {type_map.at(var_str), true};
    }
    // Pointer to non-supported type
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return {nullptr, false};
}

llvm::Value *Generator::IR::get_default_value_of_type(llvm::IRBuilder<> &builder, llvm::Module *module, const std::shared_ptr<Type> &type) {
    const std::string type_string = type->to_string();
    if (type_string == "str") {
        return builder.CreateCall(Module::String::string_manip_functions.at("create_str"), {builder.getInt64(0)}, "empty_string");
    }
    return get_default_value_of_type(IR::get_type(module, type).first);
}

llvm::Value *Generator::IR::get_default_value_of_type(llvm::Type *type) {
    if (type->isIntegerTy()) {
        return llvm::ConstantInt::get(type, 0);
    }
    if (type->isFloatTy() || type->isDoubleTy()) {
        return llvm::ConstantFP::get(type, 0.0);
    }
    if (type->isPointerTy()) {
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
    }
    if (type->isStructTy()) {
        return llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(type));
    }
    if (type->isVectorTy()) {
        llvm::VectorType *vector_type = llvm::cast<llvm::VectorType>(type);
        llvm::Type *element_type = vector_type->getElementType();
        llvm::Constant *zero_element = llvm::cast<llvm::Constant>(get_default_value_of_type(element_type));
        // Create a splat (all elements are the same)
        return llvm::ConstantVector::getSplat(vector_type->getElementCount(), zero_element);
    }
    // No conversion available
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
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

llvm::Value *Generator::IR::generate_err_value( //
    llvm::IRBuilder<> &builder,                 //
    llvm::Module *module,                       //
    const unsigned int err_id,                  //
    const unsigned int err_value,               //
    const std::string &err_message              //
) {
    llvm::StructType *err_type = type_map.at("__flint_type_err");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
    llvm::Value *err_struct = get_default_value_of_type(err_type);
    err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_id), {0}, "insert_err_type_id");
    err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_value), {1}, "insert_err_value");
    llvm::Value *message_str = IR::generate_const_string(module, err_message);
    llvm::Value *error_message = builder.CreateCall(init_str_fn, {message_str, builder.getInt64(err_message.size())}, "err_message");
    err_struct = builder.CreateInsertValue(err_struct, error_message, {2}, "insert_err_message");
    return err_struct;
}

void Generator::IR::generate_debug_print( //
    llvm::IRBuilder<> *builder,           //
    llvm::Module *module,                 //
    const std::string &message            //
) {
    if (!DEBUG_MODE) {
        return;
    }
    llvm::Value *msg_str = generate_const_string(module, "DEBUG: " + message + "\n");
    llvm::Function *print_fn = c_functions.at(PRINTF);
    builder->CreateCall(print_fn, {msg_str});
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
