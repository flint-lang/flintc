#include "generator/generator.hpp"

void Generator::Module::Env::generate_env_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_get_env_function(builder, module, only_declarations);
    generate_set_env_function(builder, module, only_declarations);
}

void Generator::Module::Env::generate_get_env_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // str *get_env(const str *var) {
    //     char *c_var = get_c_str(var);
    //     char *c_env = getenv(c_var);
    //     free(c_var);
    //     if (c_env == NULL) {
    //         // Env variable not found, throw error
    //         return create_str(0);
    //     } else {
    //         return init_str(c_env, strlen(c_env));
    //     }
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *get_c_str_fn = String::string_manip_functions.at("get_c_str");
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *getenv_fn = c_functions.at(GETENV);
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(result_type_ptr, true);
    llvm::FunctionType *get_env_type = llvm::FunctionType::get( //
        function_result_type,                                   // return struct with error code
        {str_type->getPointerTo()},                             // Parameters: const str *var
        false                                                   // No vaarg
    );
    llvm::Function *get_env_fn = llvm::Function::Create(get_env_type, llvm::Function::ExternalLinkage, "__flint_get_env", module);
    env_functions["get_env"] = get_env_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *var_arg = get_env_fn->arg_begin();
    var_arg->setName("var");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_env_fn);
    llvm::BasicBlock *env_null_block = llvm::BasicBlock::Create(context, "env_null", get_env_fn);
    llvm::BasicBlock *env_found_block = llvm::BasicBlock::Create(context, "env_found", get_env_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str var to C string
    llvm::Value *c_var = builder->CreateCall(get_c_str_fn, {var_arg}, "c_var");

    // Get environment variable: c_env = getenv(c_var)
    llvm::Value *c_env = builder->CreateCall(getenv_fn, {c_var}, "c_env");

    // Free the c_var: free(c_var)
    builder->CreateCall(free_fn, {c_var});

    // Check if c_env is NULL
    llvm::Value *env_null = builder->CreateIsNull(c_env, "env_null");
    builder->CreateCondBr(env_null, env_null_block, env_found_block);

    // Handle environment variable not found - return error with empty string
    builder->SetInsertPoint(env_null_block);
    llvm::AllocaInst *ret_null_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_null_alloc");
    llvm::Value *ret_null_err_ptr = builder->CreateStructGEP(function_result_type, ret_null_alloc, 0, "ret_null_err_ptr");
    builder->CreateStore(builder->getInt32(136), ret_null_err_ptr); // Error code for env var not found
    llvm::Value *ret_null_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_null_empty_str");
    llvm::Value *ret_null_val_ptr = builder->CreateStructGEP(function_result_type, ret_null_alloc, 1, "ret_null_val_ptr");
    builder->CreateStore(ret_null_empty_str, ret_null_val_ptr);
    llvm::Value *ret_null_val = builder->CreateLoad(function_result_type, ret_null_alloc, "ret_null_val");
    builder->CreateRet(ret_null_val);

    // Handle environment variable found
    builder->SetInsertPoint(env_found_block);

    // Get length of c_env: strlen(c_env)
    llvm::Value *env_len = builder->CreateCall(strlen_fn, {c_env}, "env_len");

    // Create string from c_env: init_str(c_env, strlen(c_env))
    llvm::Value *result_str = builder->CreateCall(init_str_fn, {c_env, env_len}, "result_str");

    // Prepare successful return value
    llvm::AllocaInst *ret_success_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_success_alloc");
    llvm::Value *ret_success_err_ptr = builder->CreateStructGEP(function_result_type, ret_success_alloc, 0, "ret_success_err_ptr");
    builder->CreateStore(builder->getInt32(0), ret_success_err_ptr); // Success code
    llvm::Value *ret_success_val_ptr = builder->CreateStructGEP(function_result_type, ret_success_alloc, 1, "ret_success_val_ptr");
    builder->CreateStore(result_str, ret_success_val_ptr);
    llvm::Value *ret_success_val = builder->CreateLoad(function_result_type, ret_success_alloc, "ret_success_val");
    builder->CreateRet(ret_success_val);
}

void Generator::Module::Env::generate_set_env_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // bool set_env(const str *var, const str *content, const bool overwrite) {
    //     char *c_var = get_c_str(var);
    //     char *c_content = get_c_str(content);
    //     int success = setenv(c_var, c_content, overwrite);
    //     free(c_var);
    //     free(c_content);
    //     if (success != 0) {
    //         return false;
    //     }
    //     return true;
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *get_c_str_fn = String::string_manip_functions.at("get_c_str");
    llvm::Function *setenv_fn = c_functions.at(SETENV);
    llvm::Function *free_fn = c_functions.at(FREE);

    llvm::FunctionType *set_env_type = llvm::FunctionType::get( //
        llvm::Type::getInt1Ty(context),                         // Return type: bool
        {str_type->getPointerTo(),                              // Argument: const str* var
            str_type->getPointerTo(),                           // Argument: const str* content
            llvm::Type::getInt1Ty(context)},                    // Argument: bool overwrite
        false                                                   // No varargs
    );
    llvm::Function *set_env_fn = llvm::Function::Create( //
        set_env_type,                                    //
        llvm::Function::ExternalLinkage,                 //
        "__flint_set_env",                               //
        module                                           //
    );
    env_functions["set_env"] = set_env_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", set_env_fn);
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *var_arg = set_env_fn->arg_begin();
    var_arg->setName("var");
    llvm::Argument *content_arg = set_env_fn->arg_begin() + 1;
    content_arg->setName("content");
    llvm::Argument *overwrite_arg = set_env_fn->arg_begin() + 2;
    overwrite_arg->setName("overwrite");

    // Convert str var to C string
    llvm::Value *c_var = builder->CreateCall(get_c_str_fn, {var_arg}, "c_var");

    // Convert str content to C string
    llvm::Value *c_content = builder->CreateCall(get_c_str_fn, {content_arg}, "c_content");

    // Convert bool overwrite to int for setenv (setenv expects int, not bool)
    llvm::Value *overwrite_int = builder->CreateZExt(overwrite_arg, builder->getInt32Ty(), "overwrite_int");

    // Call setenv: success = setenv(c_var, c_content, overwrite)
    llvm::Value *success = builder->CreateCall(setenv_fn, {c_var, c_content, overwrite_int}, "success");

    // Free the c_var: free(c_var)
    builder->CreateCall(free_fn, {c_var});

    // Free the c_content: free(c_content)
    builder->CreateCall(free_fn, {c_content});

    // Check if success != 0 (failure)
    llvm::Value *is_failure = builder->CreateICmpNE(success, builder->getInt32(0), "is_failure");

    // Return !is_failure (true if success == 0, false if success != 0)
    llvm::Value *result = builder->CreateXor(is_failure, builder->getInt1(true), "result");
    builder->CreateRet(result);
}
