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
        types_vec.emplace_back(get_type_from_str(*context, ret_value));
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

llvm::Type *Generator::IR::get_type_from_str(llvm::LLVMContext &context, const std::string &str) {
    // Check if its a primitive or not. If it is not a primitive, its just a pointer type
    if (keywords.find(str) != keywords.end()) {
        switch (keywords.at(str)) {
            default:
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            case TOK_I32:
            case TOK_U32:
                return llvm::Type::getInt32Ty(context);
            case TOK_I64:
            case TOK_U64:
                return llvm::Type::getInt64Ty(context);
            case TOK_F32:
            case TOK_F64:
                return llvm::Type::getFloatTy(context);
            case TOK_FLINT:
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            case TOK_CHAR:
                return llvm::Type::getInt8Ty(context);
            case TOK_STR:
                // Pointer to an array of i8 values representing the characters
                return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
            case TOK_BOOL:
                return llvm::Type::getInt1Ty(context);
            case TOK_VOID:
                return llvm::Type::getVoidTy(context);
        }
    }
    // Pointer to more complex data type
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return nullptr;
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

llvm::Value *Generator::IR::generate_pow_instruction( //
    [[maybe_unused]] llvm::IRBuilder<> &builder,      //
    [[maybe_unused]] llvm::Function *parent,          //
    [[maybe_unused]] llvm::Value *lhs,                //
    [[maybe_unused]] llvm::Value *rhs                 //
) {
    return nullptr;
}
