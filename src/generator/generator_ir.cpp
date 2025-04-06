#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "lexer/lexer_utils.hpp"

llvm::StructType *Generator::IR::add_and_or_get_type( //
    llvm::LLVMContext *context,                       //
    const std::vector<std::string> &types,            //
    const bool is_return_type                         //
) {
    std::string types_str = is_return_type ? "ret_" : "";
    for (auto it = types.begin(); it < types.end(); ++it) {
        types_str.append(*it);
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
        // First element is always the error code (i32)
        types_vec.push_back(llvm::Type::getInt32Ty(*context));
    } else {
        types_vec.reserve(types.size());
    }
    // Rest of the elements are the return types
    for (const auto &ret_value : types) {
        auto ret_type = get_type_from_str(*context, ret_value);
        if (ret_type.second) {
            types_vec.emplace_back(ret_type.first->getPointerTo());
        } else {
            types_vec.emplace_back(ret_type.first);
        }
    }
    llvm::ArrayRef<llvm::Type *> return_types_arr(types_vec);
    type_map[types_str] = llvm::StructType::create( //
        *context,                                   //
        return_types_arr,                           //
        "type_" + types_str,                        //
        true                                        //
    );
    return type_map[types_str];
}

void Generator::IR::generate_forward_declarations(llvm::Module *module, const FileNode &file_node) {
    unsigned int mangle_id = 1;
    file_function_mangle_ids[file_node.file_name] = {};
    file_function_names[file_node.file_name] = {};
    for (const std::unique_ptr<ASTNode> &node : file_node.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            // Create a forward declaration for the function only if it is not the main function!
            if (function_node->name != "main") {
                llvm::FunctionType *function_type = Function::generate_function_type(module->getContext(), function_node);
                module->getOrInsertFunction(function_node->name, function_type);
                file_function_mangle_ids.at(file_node.file_name).emplace(function_node->name, mangle_id++);
                file_function_names.at(file_node.file_name).emplace_back(function_node->name);
            }
        }
    }
}

std::pair<llvm::Type *, bool> Generator::IR::get_type_from_str(llvm::LLVMContext &context, const std::string &str) {
    // Check if its a primitive or not. If it is not a primitive, its just a pointer type
    if (str == "str_var") {
        // A string is a struct of type 'type { i64, [0 x i8] }'
        llvm::StructType *str_type;
        if (type_map.find("type_str") != type_map.end()) {
            str_type = type_map["type_str"];
        } else {
            str_type = llvm::StructType::create( //
                context,                         //
                {
                    llvm::Type::getInt64Ty(context),                        // len of string
                    llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // str data
                },                                                          //
                "type_str",
                false // is packed
            );
            type_map["type_str"] = str_type;
        }
        return {str_type, false};
    }
    if (keywords.find(str) != keywords.end()) {
        switch (keywords.at(str)) {
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
            case TOK_CHAR:
                return {llvm::Type::getInt8Ty(context), false};
            case TOK_STR:
                return {llvm::Type::getInt8Ty(context)->getPointerTo(), false};
            case TOK_BOOL:
                return {llvm::Type::getInt1Ty(context), false};
            case TOK_VOID:
                return {llvm::Type::getVoidTy(context), false};
        }
    }
    // Check if its a known data type
    if (data_nodes.find(str) != data_nodes.end()) {
        const DataNode *const data_node = data_nodes.at(str);
        std::vector<std::string> types;
        for (const auto &order_name : data_node->order) {
            types.emplace_back(data_node->fields.at(order_name).first);
        }
        return {add_and_or_get_type(&context, types, false), true};
    }
    // Pointer to more complex data type
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return {nullptr, false};
}

llvm::Value *Generator::IR::get_default_value_of_type(llvm::Type *type) {
    if (type->isIntegerTy()) {
        return llvm::ConstantInt::get(type, 0);
    }
    if (type->isFloatTy()) {
        return llvm::ConstantFP::get(type, 0.0);
    }
    if (type->isPointerTy()) {
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
    }
    // No conversion available
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}

llvm::Value *Generator::IR::generate_const_string(llvm::IRBuilder<> &builder, llvm::Function *parent, const std::string &str) {
    // Create array type for the string (including null terminator)
    llvm::ArrayType *str_type = llvm::ArrayType::get( //
        builder.getInt8Ty(),                          //
        str.length() + 1                              // +1 for null terminator
    );
    // Allocate space for the string data on the stack
    llvm::AllocaInst *str_buf = builder.CreateAlloca(str_type, nullptr, "str_buf");
    // Create the constant string data
    llvm::Constant *str_constant = llvm::ConstantDataArray::getString(parent->getContext(), str);
    // Store the string data in the buffer
    builder.CreateStore(str_constant, str_buf);
    // Return the buffer pointer
    return str_buf;
}

llvm::Value *Generator::IR::generate_pow_instruction( //
    [[maybe_unused]] llvm::IRBuilder<> &builder,      //
    [[maybe_unused]] llvm::Function *parent,          //
    [[maybe_unused]] llvm::Value *lhs,                //
    [[maybe_unused]] llvm::Value *rhs                 //
) {
    return nullptr;
}
