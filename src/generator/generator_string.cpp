#include "generator/generator.hpp"
#include "lexer/builtins.hpp"
#include "llvm/IR/DerivedTypes.h"
#include <cstdint>

void Generator::String::generate_create_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // THE C IMPLEMENTATION:
    // str *create_str(const size_t len) {
    //     str *string = (str *)malloc(sizeof(str) + len);
    //     string->len = len;
    //     string->value = (char *)(string + 1);
    //     return string;
    // }
    llvm::Type *str_type = IR::get_type_from_str(builder->getContext(), "str_var").first;
    llvm::Function *malloc_fn = c_functions.at(MALLOC);

    llvm::FunctionType *create_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getInt64Ty(builder->getContext())},           // Argument size_t len
        false                                                      // No varargs
    );
    llvm::Function *create_str_fn = llvm::Function::Create( //
        create_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "create_str",                                       //
        module                                              //
    );

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", create_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the parameter (len)
    llvm::Argument *len_arg = create_str_fn->arg_begin();
    len_arg->setName("len");

    // Calculate malloc size: sizeof(str) + len
    llvm::DataLayout data_layout(module);
    uint64_t str_size = data_layout.getTypeAllocSize(str_type);
    llvm::Value *malloc_size = builder->CreateAdd( //
        builder->getInt64(str_size),               //
        len_arg,                                   //
        "malloc_size"                              //
    );

    // Call malloc
    llvm::Value *string_ptr = builder->CreateCall(malloc_fn, {malloc_size}, "string_ptr");

    // Set the len field: string->len = len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, string_ptr, 0, "len_ptr");
    builder->CreateStore(len_arg, len_ptr);

    // Return the string pointer
    builder->CreateRet(string_ptr);

    string_manip_functions["create_str"] = create_str_fn;
}

void Generator::String::generate_init_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // THE C IMPLEMENTATION:
    // str *init_str(const char *value, const size_t len) {
    //     str *string = create_str(len);
    //     memcpy(string->value, value, len);
    //     return string;
    // }
    llvm::Type *str_type = IR::get_type_from_str(builder->getContext(), "str_var").first;
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *init_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                // Return type str*
        {
            llvm::Type::getInt8Ty(builder->getContext())->getPointerTo(), // Argument char* value
            llvm::Type::getInt64Ty(builder->getContext())                 // Argument size_t len
        },                                                                //
        false                                                             // No varargs
    );
    llvm::Function *init_str_fn = llvm::Function::Create( //
        init_str_type,                                    //
        llvm::Function::ExternalLinkage,                  //
        "init_str",                                       //
        module                                            //
    );

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", init_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the parameter (len)
    llvm::Argument *len_arg = init_str_fn->arg_begin() + 1;
    len_arg->setName("len");

    // Create the string by calling the create_str function
    llvm::Value *string_ptr = builder->CreateCall(create_str_fn, {len_arg}, "string");

    // Get the parameter (value)
    llvm::Argument *value_arg = init_str_fn->arg_begin();
    value_arg->setName("value");

    // Get the pointer to the string value
    llvm::Value *string_val_ptr = builder->CreateStructGEP(str_type, string_ptr, 1, "string_val_ptr");

    // Call the memcpy function
    builder->CreateCall(memcpy_fn, {string_val_ptr, value_arg, len_arg});

    // Return the created string value
    builder->CreateRet(string_ptr);

    string_manip_functions["init_str"] = init_str_fn;
}

void Generator::String::generate_assign_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // THE C IMPLEMENTATION:
    // void assign_str(str **string, str *value) {
    //     free(*string);
    //     *string = value;
    // }
    llvm::Type *str_type = IR::get_type_from_str(builder->getContext(), "str_var").first;
    llvm::Function *free_fn = c_functions.at(FREE);

    llvm::FunctionType *assign_str_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(builder->getContext()),              //
        {
            str_type->getPointerTo()->getPointerTo(), // str**
            str_type->getPointerTo()                  // str*
        },                                            //
        false                                         // No varargs
    );
    llvm::Function *assign_str_fn = llvm::Function::Create( //
        assign_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "assign_str",                                       //
        module                                              //
    );

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", assign_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the string argument
    llvm::Argument *arg_string = assign_str_fn->arg_begin();
    arg_string->setName("string");

    // Get the value argument
    llvm::Argument *arg_value = assign_str_fn->arg_begin() + 1;
    arg_value->setName("value");

    // Load the current string pointer: str* old_string = *string
    llvm::Value *old_string_ptr = builder->CreateLoad(str_type->getPointerTo(), arg_string, "old_str_ptr");

    // Free the old string: free(old_string)
    builder->CreateCall(free_fn, {old_string_ptr});

    // Store the new value: *string = value
    builder->CreateStore(arg_value, arg_string);

    // Return void
    builder->CreateRetVoid();

    string_manip_functions["assign_str"] = assign_str_fn;
}

void Generator::String::generate_assign_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // THE C IMPLEMENTATION:
    // void assign_lit(str **string, const char *value, const size_t len) {
    //     str *new_string = (str *)realloc(*string, sizeof(str) + len);
    //     *string = new_string;
    //     new_string->len = len;
    //     new_string->value = (char *)(new_string + 1);
    //     memcpy(new_string->value, value, len);
    // }
    llvm::Type *str_type = IR::get_type_from_str(builder->getContext(), "str_var").first;
    llvm::Function *realloc_fn = c_functions.at(REALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *assign_lit_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(builder->getContext()),              //
        {
            str_type->getPointerTo()->getPointerTo(),                    // Argument: str** string
            llvm::Type::getInt8Ty(module->getContext())->getPointerTo(), // Argument: char* value
            llvm::Type::getInt64Ty(module->getContext())                 // Argument: u64 len
        },                                                               //
        false                                                            // No varargs
    );
    llvm::Function *assign_lit_fn = llvm::Function::Create( //
        assign_lit_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "assign_lit",                                       //
        module                                              //
    );

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", assign_lit_fn);
    builder->SetInsertPoint(entry_block);

    // Get the string argument
    llvm::Argument *arg_string = assign_lit_fn->arg_begin();
    arg_string->setName("string");

    // Get the value argument
    llvm::Argument *arg_value = assign_lit_fn->arg_begin() + 1;
    arg_value->setName("value");

    // Get the len argument
    llvm::Argument *arg_len = assign_lit_fn->arg_begin() + 2;
    arg_len->setName("len");

    // Load the current string pointer: str* old_string = *string
    llvm::Value *old_string_ptr = builder->CreateLoad(str_type->getPointerTo(), arg_string, "old_string_ptr");

    // Calculate new size: sizeof(str) + len
    llvm::DataLayout data_layout(module);
    uint64_t str_size = data_layout.getTypeAllocSize(str_type);
    llvm::Value *new_size = builder->CreateAdd(builder->getInt64(str_size), arg_len, "new_size");

    // Call realloc: str* new_string = realloc(old_string, new_size)
    llvm::Value *new_string_ptr = builder->CreateCall(realloc_fn, {old_string_ptr, new_size}, "new_string_ptr");

    // Store the new string pointer back: *string = new_string
    builder->CreateStore(new_string_ptr, arg_string);

    // Set the len field: new_string->len = len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, new_string_ptr, 0, "len_ptr");
    builder->CreateStore(arg_len, len_ptr);

    // Get pointer to the string data area
    llvm::Value *data_ptr = builder->CreateStructGEP(str_type, new_string_ptr, 1, "data_ptr");

    // Call memcpy to copy the string content
    builder->CreateCall(memcpy_fn, {data_ptr, arg_value, arg_len}, "memcpy_result");

    // Return void
    builder->CreateRetVoid();

    // Store the function for later use
    string_manip_functions["assign_lit"] = assign_lit_fn;
}

void Generator::String::generate_add_str_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // THE C IMPLEMENTATION:
    // str *add_str_str(const str *lhs, const str *rhs) {
    //     str *result = create_str(lhs->len + rhs->len);
    //     memcpy(result->value, lhs->value, lhs->len);
    //     memcpy(result->value + lhs->len, rhs->value, rhs->len);
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type_from_str(builder->getContext(), "str_var").first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");

    llvm::FunctionType *add_str_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return Type: str*
        {
            str_type->getPointerTo(), // Argument: str* lhs
            str_type->getPointerTo()  // Argument: str* rhs
        },                            //
        false                         // No varargs
    );
    llvm::Function *add_str_str_fn = llvm::Function::Create( //
        add_str_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "add_str_str",                                       //
        module                                               //
    );

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", add_str_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the lhs argument
    llvm::Argument *arg_lhs = add_str_str_fn->arg_begin();
    arg_lhs->setName("lhs");

    // Get the rhs argument
    llvm::Argument *arg_rhs = add_str_str_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get lhs->len
    llvm::Value *lhs_len_ptr = builder->CreateStructGEP(str_type, arg_lhs, 0, "lhs_len_ptr");
    llvm::Value *lhs_len = builder->CreateLoad(builder->getInt64Ty(), lhs_len_ptr, "lhs_len");

    // Get rhs->len
    llvm::Value *rhs_len_ptr = builder->CreateStructGEP(str_type, arg_rhs, 0, "rhs_len_ptr");
    llvm::Value *rhs_len = builder->CreateLoad(builder->getInt64Ty(), rhs_len_ptr, "rhs_len");

    // Calculate total length: lhs->len + rhs->len
    llvm::Value *total_len = builder->CreateAdd(lhs_len, rhs_len, "total_len");

    // Create result string: str *result = create_str(total_len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {total_len}, "result");

    // Get lhs->value
    llvm::Value *lhs_value_ptr = builder->CreateStructGEP(str_type, arg_lhs, 1, "lhs_value_ptr");

    // Get result->value
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");

    // Copy first string: memcpy(result->value, lhs->value, lhs->len)
    builder->CreateCall(memcpy_fn, {result_value_ptr, lhs_value_ptr, lhs_len}, "memcpy1_result");

    // Calculate position for second string: result->value + lhs->len
    llvm::Value *second_pos = builder->CreateGEP(builder->getInt8Ty(), result_value_ptr, lhs_len, "second_pos");

    // Get rhs->value
    llvm::Value *rhs_value_ptr = builder->CreateStructGEP(str_type, arg_rhs, 1, "rhs_value_ptr");

    // Copy second string: memcpy(result->value + lhs->len, rhs->value, rhs->len)
    builder->CreateCall(memcpy_fn, {second_pos, rhs_value_ptr, rhs_len}, "memcpy2_result");

    // Return the result
    builder->CreateRet(result);

    // Store the function for later use
    string_manip_functions["add_str_str"] = add_str_str_fn;
}

void Generator::String::generate_add_str_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // THE C IMPLEMENTATION:
    // str *add_str_lit(const str *lhs, const char *rhs, const size_t rhs_len) {
    //     str *result = create_str(lhs->len + rhs_len);
    //     memcpy(result->value, lhs->value, lhs->len);
    //     memcpy(result->value + lhs->len, rhs, rhs_len);
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type_from_str(builder->getContext(), "str_var").first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");

    llvm::FunctionType *add_str_lit_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return Type: str*
        {
            str_type->getPointerTo(),                                     // Argument: str* lhs
            llvm::Type::getInt8Ty(builder->getContext())->getPointerTo(), // Argument: char* rhs
            llvm::Type::getInt64Ty(builder->getContext())                 // Argument: u64 rhs_len
        },                                                                //
        false                                                             // No varargs
    );
    llvm::Function *add_str_lit_fn = llvm::Function::Create( //
        add_str_lit_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "add_str_lit",                                       //
        module                                               //
    );

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", add_str_lit_fn);
    builder->SetInsertPoint(entry_block);

    // Get the lhs argument
    llvm::Argument *arg_lhs = add_str_lit_fn->arg_begin();
    arg_lhs->setName("lhs");

    // Get the rhs argument
    llvm::Argument *arg_rhs = add_str_lit_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get the rhs_len argument
    llvm::Argument *arg_rhs_len = add_str_lit_fn->arg_begin() + 2;
    arg_rhs_len->setName("rhs_len");

    // Get lhs->len
    llvm::Value *lhs_len_ptr = builder->CreateStructGEP(str_type, arg_lhs, 0, "lhs_len_ptr");
    llvm::Value *lhs_len = builder->CreateLoad(builder->getInt64Ty(), lhs_len_ptr, "lhs_len");

    // Calculate total length: lhs->len + rhs_len
    llvm::Value *total_len = builder->CreateAdd(lhs_len, arg_rhs_len, "total_len");

    // Create result string: str *result = create_str(total_len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {total_len}, "result");

    // Get lhs->value
    llvm::Value *lhs_value_ptr = builder->CreateStructGEP(str_type, arg_lhs, 1, "lhs_value_ptr");

    // Get result->value
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");

    // Copy first string: memcpy(result->value, lhs->value, lhs->len)
    builder->CreateCall(memcpy_fn, {result_value_ptr, lhs_value_ptr, lhs_len});

    // Calculate position for second string: result->value + lhs->len
    llvm::Value *second_pos = builder->CreateGEP(builder->getInt8Ty(), result_value_ptr, lhs_len, "second_pos");

    // Copy second string: memcpy(result->value + lhs->len, rhs, rhs_len)
    builder->CreateCall(memcpy_fn, {second_pos, arg_rhs, arg_rhs_len});

    // Return the result
    builder->CreateRet(result);

    // Store the function for later use
    string_manip_functions["add_str_lit"] = add_str_lit_fn;
}

void Generator::String::generate_add_lit_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // THE C IMPLEMENTATION:
    // str *add_lit_str(const char *lhs, const size_t lhs_len, const str *rhs) {
    //     str *result = create_str(lhs_len + rhs->len);
    //     memcpy(result->value, lhs, lhs_len);
    //     memcpy(result->value + lhs_len, rhs->value, rhs->len);
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type_from_str(builder->getContext(), "str_var").first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");

    llvm::FunctionType *add_lit_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return Type: str*
        {
            llvm::Type::getInt8Ty(builder->getContext())->getPointerTo(), // Argument: char* lhs
            llvm::Type::getInt64Ty(builder->getContext()),                // Argument: u64 lhs_len
            str_type->getPointerTo()                                      // Argument: str* rhs
        },                                                                //
        false                                                             // No varargs
    );
    llvm::Function *add_lit_str_fn = llvm::Function::Create( //
        add_lit_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "add_lit_str",                                       //
        module                                               //
    );

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", add_lit_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the lhs argument
    llvm::Argument *arg_lhs = add_lit_str_fn->arg_begin();
    arg_lhs->setName("lhs");

    // Get the lhs_len argument
    llvm::Argument *arg_lhs_len = add_lit_str_fn->arg_begin() + 1;
    arg_lhs_len->setName("lhs_len");

    // Get the rhs argument
    llvm::Argument *arg_rhs = add_lit_str_fn->arg_begin() + 2;
    arg_rhs->setName("rhs");

    // Get rhs->len
    llvm::Value *rhs_len_ptr = builder->CreateStructGEP(str_type, arg_rhs, 0, "rhs_len_ptr");
    llvm::Value *rhs_len = builder->CreateLoad(builder->getInt64Ty(), rhs_len_ptr, "rhs_len");

    // Calculate total length: lhs_len + rhs->len
    llvm::Value *total_len = builder->CreateAdd(arg_lhs_len, rhs_len, "total_len");

    // Create result string: str *result = create_str(total_len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {total_len}, "result");

    // Get result->value
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");

    // Copy first string: memcpy(result->value, lhs, lhs_len)
    builder->CreateCall(memcpy_fn, {result_value_ptr, arg_lhs, arg_lhs_len}, "memcpy1_result");

    // Calculate position for second string: result->value + lhs_len
    llvm::Value *second_pos = builder->CreateGEP(builder->getInt8Ty(), result_value_ptr, arg_lhs_len, "second_pos");

    // Get rhs->value
    llvm::Value *rhs_value_ptr = builder->CreateStructGEP(str_type, arg_rhs, 1, "rhs_value_ptr");

    // Copy second string: memcpy(result->value + lhs_len, rhs->value, rhs->len)
    builder->CreateCall(memcpy_fn, {second_pos, rhs_value_ptr, rhs_len}, "memcpy2_result");

    // Return the result
    builder->CreateRet(result);

    // Store the function for later use
    string_manip_functions["add_lit_str"] = add_lit_str_fn;
}

void Generator::String::generate_string_manip_functions(llvm::IRBuilder<> *builder, llvm::Module *module) {
    generate_create_str_function(builder, module);
    generate_init_str_function(builder, module);
    generate_assign_str_function(builder, module);
    generate_assign_lit_function(builder, module);
    generate_add_str_str_function(builder, module);
    generate_add_str_lit_function(builder, module);
    generate_add_lit_str_function(builder, module);
}

llvm::Value *Generator::String::generate_string_declaration( //
    llvm::IRBuilder<> &builder,                              //
    llvm::Value *rhs,                                        //
    std::optional<const ExpressionNode *> rhs_expr           //
) {
    // If there is no initalizer we need to create a new str struct with length 0 and put absolutely nothing into the value field of it
    if (!rhs_expr.has_value()) {
        llvm::Function *create_str_fn = string_manip_functions.at("create_str");
        llvm::Value *zero = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
        return builder.CreateCall(create_str_fn, {zero}, "empty_str");
    }

    // If there is a rhs, we first check if its a literal node or not. If its a literal string the declaration looks differently than if
    // its a variable
    if (const LiteralNode *literal = dynamic_cast<const LiteralNode *>(rhs_expr.value())) {
        // Get the `init_str` function
        llvm::Function *init_str_fn = string_manip_functions.at("init_str");

        // Get the length of the literal
        const size_t len = std::get<std::string>(literal->value).length();
        llvm::Value *len_val = llvm::ConstantInt::get(builder.getInt64Ty(), len);

        // Call the `init_str` function
        return builder.CreateCall(init_str_fn, {rhs, len_val}, "str_init");
    } else {
        // Just return the rhs, as this is a declaration the lhs definitely doesnt have a value saved on it
        return rhs;
    }
}

void Generator::String::generate_string_assignment( //
    llvm::IRBuilder<> &builder,                     //
    llvm::Value *const lhs,                         //
    const AssignmentNode *assignment_node,          //
    llvm::Value *expression                         //
) {
    // If the rhs is a string literal we need to do different code than if it is a string variable. The expression contains a pointer to the
    // str in memory if its a variable, otherwise it will contain a pointer to the chars (char*).
    if (const LiteralNode *lit = dynamic_cast<const LiteralNode *>(assignment_node->expression.get())) {
        // Get the `assign_lit` function
        llvm::Function *assign_lit_fn = string_manip_functions.at("assign_lit");

        // Get the size of the string literal
        const size_t len = std::get<std::string>(lit->value).length();
        llvm::Value *len_val = llvm::ConstantInt::get(builder.getInt64Ty(), len);

        // Call the `assign_lit` function
        builder.CreateCall(assign_lit_fn, {lhs, expression, len_val});
    } else {
        // Get the `assign_str` function
        llvm::Function *assign_str_fn = string_manip_functions.at("assign_str");

        // Call the `assign_str` function
        builder.CreateCall(assign_str_fn, {lhs, expression});
    }
}

llvm::Value *Generator::String::generate_string_addition( //
    llvm::IRBuilder<> &builder,                           //
    llvm::Value *lhs,                                     //
    const ExpressionNode *lhs_expr,                       //
    llvm::Value *rhs,                                     //
    const ExpressionNode *rhs_expr                        //
) {
    // It highly depends on whether the lhs and / or the rhs are string literals or variables
    const LiteralNode *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr);
    const LiteralNode *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr);
    if (lhs_lit == nullptr && rhs_lit == nullptr) {
        // Both sides are variables
        llvm::Function *add_str_str_fn = string_manip_functions.at("add_str_str");
        return builder.CreateCall(add_str_str_fn, {lhs, rhs}, "add_str_str_res");
    } else if (lhs_lit == nullptr && rhs_lit != nullptr) {
        // Only rhs is literal
        llvm::Function *add_str_lit_fn = string_manip_functions.at("add_str_lit");
        return builder.CreateCall(add_str_lit_fn, {lhs, rhs}, "add_str_lit_res");
    } else if (lhs != nullptr && rhs_lit == nullptr) {
        // Only lhs is literal
        llvm::Function *add_lit_str_fn = string_manip_functions.at("add_lit_str");
        return builder.CreateCall(add_lit_str_fn, {lhs, rhs}, "add_lit_str_res");
    }

    // Both sides are literals, this shouldnt be possible, as they should have been folded already
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}
