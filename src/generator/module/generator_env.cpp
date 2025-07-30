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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *get_c_str_fn = String::string_manip_functions.at("get_c_str");
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *getenv_fn = c_functions.at(GETENV);
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);

    const unsigned int ErrEnv = Type::get_type_id_from_str("ErrEnv");
    const std::vector<error_value> &ErrEnvValues = std::get<2>(core_module_error_sets.at("env").at(0));
    const unsigned int VarNotFound = 0;
    const std::string VarNotFoundMessage(ErrEnvValues.at(VarNotFound).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
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

    // Handle environment variable not found, throw ErrEnv.VarNotFound
    builder->SetInsertPoint(env_null_block);
    llvm::AllocaInst *ret_null_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_null_alloc");
    llvm::Value *ret_null_err_ptr = builder->CreateStructGEP(function_result_type, ret_null_alloc, 0, "ret_null_err_ptr");
    llvm::Value *error_value = IR::generate_err_value(*builder, ErrEnv, VarNotFound, VarNotFoundMessage);
    builder->CreateStore(error_value, ret_null_err_ptr);
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
    llvm::StructType *err_type = type_map.at("__flint_type_err");
    llvm::Value *err_struct = IR::get_default_value_of_type(err_type);
    builder->CreateStore(err_struct, ret_success_err_ptr);
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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *get_c_str_fn = String::string_manip_functions.at("get_c_str");
    llvm::Function *setenv_fn = c_functions.at(SETENV);
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);

    const unsigned int ErrEnv = Type::get_type_id_from_str("ErrEnv");
    const std::vector<error_value> &ErrEnvValues = std::get<2>(core_module_error_sets.at("env").at(0));
    const unsigned int InvalidName = 1;
    const unsigned int InvalidValue = 2;
    const std::string InvalidNameMessage(ErrEnvValues.at(InvalidName).second);
    const std::string InvalidValueMessage(ErrEnvValues.at(InvalidValue).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("bool");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *set_env_type = llvm::FunctionType::get( //
        function_result_type,                                   // Return type: bool {ErrEnv}
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
    llvm::BasicBlock *name_fail_block = llvm::BasicBlock::Create(context, "name_fail", set_env_fn);
    llvm::BasicBlock *name_ok_block = llvm::BasicBlock::Create(context, "name_ok", set_env_fn);
    llvm::BasicBlock *value_fail_block = llvm::BasicBlock::Create(context, "value_fail", set_env_fn);
    llvm::BasicBlock *value_ok_block = llvm::BasicBlock::Create(context, "value_ok_block", set_env_fn);

    // Get the parameters
    llvm::Argument *var_arg = set_env_fn->arg_begin();
    var_arg->setName("var");
    llvm::Argument *content_arg = set_env_fn->arg_begin() + 1;
    content_arg->setName("content");
    llvm::Argument *overwrite_arg = set_env_fn->arg_begin() + 2;
    overwrite_arg->setName("overwrite");

    // Convert str var to C string
    builder->SetInsertPoint(entry_block);
    llvm::Value *c_var = builder->CreateCall(get_c_str_fn, {var_arg}, "c_var");
    llvm::Value *c_var_len = builder->CreateCall(strlen_fn, {c_var}, "c_var_len");
    llvm::Value *var_len_ptr = builder->CreateStructGEP(str_type, var_arg, 0, "var_len_ptr");
    llvm::Value *var_len = builder->CreateLoad(builder->getInt64Ty(), var_len_ptr, "var_len");
    llvm::Value *var_len_eq = builder->CreateICmpEQ(c_var_len, var_len, "var_len_eq");
    builder->CreateCondBr(var_len_eq, name_ok_block, name_fail_block, IR::generate_weights(100, 0));

    // Free the C string and return an error if the var string contains a null character, throw ErrEnv.InvalidName
    builder->SetInsertPoint(name_fail_block);
    builder->CreateCall(free_fn, {c_var});
    llvm::AllocaInst *ret_name_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_name_fail_alloc");
    llvm::Value *ret_name_fail_err_ptr = builder->CreateStructGEP(function_result_type, ret_name_fail_alloc, 0, "ret_name_fail_err_ptr");
    llvm::Value *error_value = IR::generate_err_value(*builder, ErrEnv, InvalidName, InvalidNameMessage);
    builder->CreateStore(error_value, ret_name_fail_err_ptr);
    llvm::Value *ret_name_fail_val_ptr = builder->CreateStructGEP(function_result_type, ret_name_fail_alloc, 1, "ret_name_fail_val_ptr");
    builder->CreateStore(builder->getInt1(false), ret_name_fail_val_ptr);
    llvm::Value *ret_name_fail_val = builder->CreateLoad(function_result_type, ret_name_fail_alloc, "ret_name_fail_val");
    builder->CreateRet(ret_name_fail_val);

    // Convert str content to C string
    builder->SetInsertPoint(name_ok_block);
    llvm::Value *c_content = builder->CreateCall(get_c_str_fn, {content_arg}, "c_content");
    llvm::Value *c_content_len = builder->CreateCall(strlen_fn, {c_content}, "c_content_len");
    llvm::Value *content_len_ptr = builder->CreateStructGEP(str_type, content_arg, 0, "content_len_ptr");
    llvm::Value *content_len = builder->CreateLoad(builder->getInt64Ty(), content_len_ptr, "content_len");
    llvm::Value *content_len_eq = builder->CreateICmpEQ(c_content_len, content_len, "content_len_eq");
    builder->CreateCondBr(content_len_eq, value_ok_block, value_fail_block, IR::generate_weights(100, 0));

    // Free the C strings and throw Err.InvalidValue if the content string contains a null character
    builder->SetInsertPoint(value_fail_block);
    builder->CreateCall(free_fn, {c_var});
    builder->CreateCall(free_fn, {c_content});
    llvm::AllocaInst *ret_value_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_value_fail_alloc");
    llvm::Value *ret_value_fail_err_ptr = builder->CreateStructGEP(function_result_type, ret_value_fail_alloc, 0, "ret_value_fail_err_ptr");
    error_value = IR::generate_err_value(*builder, ErrEnv, InvalidValue, InvalidValueMessage);
    builder->CreateStore(error_value, ret_value_fail_err_ptr);
    llvm::Value *ret_value_fail_val_ptr = builder->CreateStructGEP(function_result_type, ret_value_fail_alloc, 1, "ret_value_fail_val_ptr");
    builder->CreateStore(builder->getInt1(false), ret_value_fail_val_ptr);
    llvm::Value *ret_value_fail_val = builder->CreateLoad(function_result_type, ret_value_fail_alloc, "ret_value_fail_val");
    builder->CreateRet(ret_value_fail_val);

    // Convert bool overwrite to int for setenv (setenv expects int, not bool)
    builder->SetInsertPoint(value_ok_block);
    llvm::Value *overwrite_int = builder->CreateZExt(overwrite_arg, builder->getInt32Ty(), "overwrite_int");
    llvm::Value *success = builder->CreateCall(setenv_fn, {c_var, c_content, overwrite_int}, "success");

    // Free the c strings
    builder->CreateCall(free_fn, {c_var});
    builder->CreateCall(free_fn, {c_content});

    // Check if success != 0 (failure)
    llvm::Value *is_failure = builder->CreateICmpNE(success, builder->getInt32(0), "is_failure");

    // Return !is_failure (true if success == 0, false if success != 0)
    llvm::Value *result = builder->CreateXor(is_failure, builder->getInt1(true), "result");

    // Return the result
    llvm::AllocaInst *ret_result_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_result_alloc");
    llvm::Value *ret_result_err_ptr = builder->CreateStructGEP(function_result_type, ret_result_alloc, 0, "ret_result_err_ptr");
    llvm::StructType *err_type = type_map.at("__flint_type_err");
    llvm::Value *err_struct = IR::get_default_value_of_type(err_type);
    builder->CreateStore(err_struct, ret_result_err_ptr);
    llvm::Value *ret_result_val_ptr = builder->CreateStructGEP(function_result_type, ret_result_alloc, 1, "ret_result_val_ptr");
    builder->CreateStore(result, ret_result_val_ptr);
    llvm::Value *ret_result_val = builder->CreateLoad(function_result_type, ret_result_alloc, "ret_result_val");
    builder->CreateRet(ret_result_val);
}
