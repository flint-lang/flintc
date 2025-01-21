#include "generator/generator.hpp"

llvm::StructType *Generator::IR::add_and_or_get_type(llvm::LLVMContext *context, const FunctionNode *function_node) {
    std::string return_types;
    for (auto return_it = function_node->return_types.begin(); return_it < function_node->return_types.end(); ++return_it) {
        return_types.append(*return_it);
        if (std::distance(return_it, function_node->return_types.end()) > 1) {
            return_types.append(",");
        }
    }
    if (return_types == "") {
        return_types = "void";
    }
    if (type_map.find(return_types) != type_map.end()) {
        return type_map[return_types];
    }

    // Get the return types
    std::vector<llvm::Type *> return_types_vec;
    return_types_vec.reserve(function_node->return_types.size() + 1);
    // First element is always the error code (i32)
    return_types_vec.push_back(llvm::Type::getInt32Ty(*context));
    // Rest of the elements are the return types
    for (const auto &ret_value : function_node->return_types) {
        return_types_vec.emplace_back(get_type_from_str(*context, ret_value));
    }
    llvm::ArrayRef<llvm::Type *> return_types_arr(return_types_vec);
    type_map[return_types] = llvm::StructType::create( //
        *context,                                      //
        return_types_arr,                              //
        "type_" + return_types,                        //
        true                                           //
    );
    return type_map[return_types];
}

void Generator::IR::generate_forward_declarations(llvm::IRBuilder<> &builder, llvm::Module *module, const FileNode &file_node) {
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
                throw_err(ERR_GENERATING);
                return nullptr;
            case TOK_INT:
                return llvm::Type::getInt32Ty(context);
            case TOK_FLINT:
                return llvm::Type::getFloatTy(context);
            case TOK_CHAR:
                return llvm::Type::getInt8Ty(context);
            case TOK_STR:
                // Pointer to an array of i8 values representing the characters
                return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
            case TOK_BOOL:
                return llvm::Type::getInt1Ty(context);
            case TOK_BYTE:
                return llvm::IntegerType::get(context, 8);
            case TOK_VOID:
                return llvm::Type::getVoidTy(context);
        }
    }
    // Pointer to more complex data type
    throw_err(ERR_NOT_IMPLEMENTED_YET);
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
    throw_err(ERR_GENERATING);
    return nullptr;
}

llvm::Value *Generator::IR::generate_pow_instruction( //
    llvm::IRBuilder<> &builder,                       //
    llvm::Function *parent,                           //
    llvm::Value *lhs,                                 //
    llvm::Value *rhs                                  //
) {
    return nullptr;
}
