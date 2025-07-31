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

llvm::StructType *Generator::IR::add_and_or_get_type( //
    llvm::Module *module,                             //
    const std::shared_ptr<Type> &type,                //
    const bool is_return_type                         //
) {
    std::vector<std::shared_ptr<Type>> types;
    std::string types_str = is_return_type ? "ret_" : "";
    if (const GroupType *group_type = dynamic_cast<const GroupType *>(type.get())) {
        types = group_type->types;
    } else if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(type.get())) {
        types_str = "tuple_";
        types = tuple_type->types;
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
    if (types_str == "ret_") {
        types_str = "ret_void";
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
        auto ret_type = get_type(module, ret_value);
        if (ret_type.second) {
            types_vec.emplace_back(ret_type.first->getPointerTo());
        } else {
            types_vec.emplace_back(ret_type.first);
        }
    }
    llvm::ArrayRef<llvm::Type *> return_types_arr(types_vec);
    type_map[types_str] = llvm::StructType::create( //
        context,                                    //
        return_types_arr,                           //
        "type_" + types_str,                        //
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

std::pair<llvm::Type *, bool> Generator::IR::get_type(llvm::Module *module, const std::shared_ptr<Type> &type) {
    // Check if its a primitive or not. If it is not a primitive, its just a pointer type
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
            if (type_map.find("type_str") == type_map.end()) {
                llvm::StructType *str_type = llvm::StructType::create( //
                    context,                                           //
                    {
                        llvm::Type::getInt64Ty(context),                        // len of string
                        llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data
                    },                                                          //
                    "type_str",
                    false // is not packed, its padded
                );
                type_map["type_str"] = str_type;
            }
            return {type_map.at("type_str"), false};
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
                    // A string is a struct of type 'type { i64, [0 x i8] }'
                    if (type_map.find("type_str") == type_map.end()) {
                        llvm::StructType *str_type = llvm::StructType::create( //
                            context,                                           //
                            {
                                llvm::Type::getInt64Ty(context),                        // len of string
                                llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data
                            },                                                          //
                            "type_str",
                            false // is not packed, its padded
                        );
                        type_map["type_str"] = str_type;
                    }
                    return {type_map.at("type_str")->getPointerTo(), false};
                }
                case TOK_BOOL:
                    return {llvm::Type::getInt1Ty(context), false};
                case TOK_VOID:
                    return {llvm::Type::getVoidTy(context), false};
            }
        }
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(type.get())) {
        // Check if its a known data type
        std::vector<std::shared_ptr<Type>> types;
        for (const auto &field : data_type->data_node->fields) {
            types.emplace_back(std::get<1>(field));
        }
        std::shared_ptr<Type> type_ptr = nullptr;
        if (types.size() == 1) {
            type_ptr = types.front();
        } else {
            type_ptr = std::make_shared<GroupType>(types);
            if (!Type::add_type(type_ptr)) {
                type_ptr = Type::get_type_from_str(type_ptr->to_string()).value();
            }
        }
        return {add_and_or_get_type(module, type_ptr, false), true};
    } else if (dynamic_cast<ArrayType *>(type.get())) {
        // Arrays are *always* of type 'str', as a 'str' is just one i64 followed by a byte array
        if (type_map.find("type_str") == type_map.end()) {
            llvm::StructType *str_type = llvm::StructType::create( //
                context,                                           //
                {
                    llvm::Type::getInt64Ty(context),                        // the dimensionality
                    llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data (the lenghts followed by the row-major array)
                },                                                          //
                "type_str",
                false // is not packed, its padded
            );
            type_map["type_str"] = str_type;
        }
        return {type_map.at("type_str")->getPointerTo(), false};
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
            type_vector.emplace_back(get_type(module, tup_type).first);
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
            llvm::StructType *llvm_opt_type = llvm::StructType::create(                                                     //
                context, {llvm::Type::getInt1Ty(context), get_type(module, optional_type->base_type).first}, opt_str, false //
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
                "type_" + var_str,
                false // is not packed, its padded
            );
            type_map[var_str] = variant_struct_type;
        }
        return {type_map.at(var_str), true};
    }
    // Pointer to more complex data type
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

llvm::Value *Generator::IR::generate_const_string(llvm::IRBuilder<> &builder, const std::string &str) {
    // Create array type for the string (including null terminator)
    llvm::ArrayType *str_type = llvm::ArrayType::get( //
        builder.getInt8Ty(),                          //
        str.length() + 1                              // +1 for null terminator
    );
    // Allocate space for the string data on the stack
    llvm::AllocaInst *str_buf = builder.CreateAlloca(str_type, nullptr, "str_buf");
    // Create the constant string data
    llvm::Constant *str_constant = llvm::ConstantDataArray::getString(context, str);
    // Store the string data in the buffer
    builder.CreateStore(str_constant, str_buf);
    // Return the buffer pointer
    return str_buf;
}
llvm::Value *Generator::IR::generate_err_value( //
    llvm::IRBuilder<> &builder,                 //
    const unsigned int err_id,                  //
    const unsigned int err_value,               //
    const std::string &err_message              //
) {
    llvm::StructType *err_type = type_map.at("__flint_type_err");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
    llvm::Value *err_struct = get_default_value_of_type(err_type);
    err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_id), {0}, "insert_err_type_id");
    err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_value), {1}, "insert_err_value");
    llvm::Value *message_str = IR::generate_const_string(builder, err_message);
    llvm::Value *error_message = builder.CreateCall(init_str_fn, {message_str, builder.getInt64(err_message.size())}, "err_message");
    err_struct = builder.CreateInsertValue(err_struct, error_message, {2}, "insert_err_message");
    return err_struct;
}

void Generator::IR::generate_debug_print( //
    llvm::IRBuilder<> *builder,           //
    const std::string &message            //
) {
    if (!DEBUG_MODE) {
        return;
    }
    llvm::Value *msg_str = generate_const_string(*builder, "DEBUG: " + message + "\n");
    llvm::Function *print_fn = c_functions.at(PRINTF);
    builder->CreateCall(print_fn, {msg_str});
}
