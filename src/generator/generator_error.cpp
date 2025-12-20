#include "generator/generator.hpp"
#include "parser/parser.hpp"

void Generator::Error::generate_error_functions(llvm::IRBuilder<> *builder, llvm::Module *module) {
    generate_get_err_type_str_function(builder, module);
    generate_get_err_val_str_function(builder, module);
    generate_get_err_str_function(builder, module);
}

void Generator::Error::generate_get_err_type_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    llvm::FunctionType *get_err_type_str_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),                  // returns char*
        {llvm::Type::getInt32Ty(context)},                               // Takes the type of the error
        false                                                            // No vaargs
    );
    llvm::Function *get_err_type_str_fn = llvm::Function::Create( //
        get_err_type_str_type,                                    //
        llvm::Function::ExternalLinkage,                          //
        "__flint_get_err_type_str",                               //
        module                                                    //
    );
    error_functions["get_err_type_str"] = get_err_type_str_fn;

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_err_type_str_fn);
    llvm::BasicBlock *default_block = llvm::BasicBlock::Create(context, "default", get_err_type_str_fn);
    llvm::BasicBlock *zero_block = llvm::BasicBlock::Create(context, "zero_case", get_err_type_str_fn);

    // Get the parameter (err_type)
    llvm::Argument *arg_err_type = get_err_type_str_fn->arg_begin();
    arg_err_type->setName("err_type");

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get all errors
    std::vector<const ErrorNode *> errors = Parser::get_all_errors();

    // Create the switch instruction (we'll add cases to it)
    // Number of cases = 1 (for zero) + number of errors
    llvm::SwitchInst *switch_inst = builder->CreateSwitch(arg_err_type, default_block, 1 + errors.size());

    // Add case for hash 0 -> return "error"
    switch_inst->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), zero_block);
    builder->SetInsertPoint(zero_block);
    llvm::Value *error_str = IR::generate_const_string(module, "error");
    builder->CreateRet(error_str);

    // Add cases for each error type
    for (const ErrorNode *error : errors) {
        llvm::BasicBlock *case_block = llvm::BasicBlock::Create(context, "case_" + error->name, get_err_type_str_fn);
        switch_inst->addCase(builder->getInt32(error->error_id), case_block);

        builder->SetInsertPoint(case_block);
        llvm::Value *type_str = IR::generate_const_string(module, error->name);
        builder->CreateRet(type_str);
    }

    // Default case: print error message and abort
    builder->SetInsertPoint(default_block);
    llvm::Value *unknown_err_msg = IR::generate_const_string(module, "Unknown error type hash: %u\n");
    builder->CreateCall(c_functions.at(PRINTF), {unknown_err_msg, arg_err_type});
    builder->CreateCall(c_functions.at(ABORT), {});
    builder->CreateUnreachable();
}

void Generator::Error::generate_get_err_val_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    llvm::FunctionType *get_err_val_str_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),                 // returns char*
        {
            llvm::Type::getInt32Ty(context), // Takes the error type id
            llvm::Type::getInt32Ty(context)  // Takes the value id
        },
        false // No vaargs
    );
    llvm::Function *get_err_val_str_fn = llvm::Function::Create( //
        get_err_val_str_type,                                    //
        llvm::Function::ExternalLinkage,                         //
        "__flint_get_err_val_str",                               //
        module                                                   //
    );
    error_functions["get_err_val_str"] = get_err_val_str_fn;

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_err_val_str_fn);
    llvm::BasicBlock *default_block = llvm::BasicBlock::Create(context, "default", get_err_val_str_fn);
    llvm::BasicBlock *zero_block = llvm::BasicBlock::Create(context, "zero_case", get_err_val_str_fn);

    // Get the parameter (err_type)
    llvm::Argument *arg_err_type = get_err_val_str_fn->arg_begin();
    arg_err_type->setName("err_type");

    // Get the parameter (err_val)
    llvm::Argument *arg_err_val = get_err_val_str_fn->arg_begin() + 1;
    arg_err_val->setName("err_val");

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get all errors
    std::vector<const ErrorNode *> errors = Parser::get_all_errors();

    // Create the switch instruction (we'll add cases to it)
    // Number of cases = 1 (for zero) + number of errors
    llvm::SwitchInst *switch_inst = builder->CreateSwitch(arg_err_type, default_block, 1 + errors.size());

    // Add case for hash 0 -> return "error"
    switch_inst->addCase(builder->getInt32(0), zero_block);
    builder->SetInsertPoint(zero_block);
    llvm::Value *error_str = IR::generate_const_string(module, "anyerror");
    builder->CreateRet(error_str);

    // Add cases for each error type
    for (const ErrorNode *error : errors) {
        llvm::BasicBlock *case_block = llvm::BasicBlock::Create(context, "case_" + error->name, get_err_val_str_fn);
        switch_inst->addCase(builder->getInt32(error->error_id), case_block);

        // Now we do a small check if the error set is inherited from a different error set. If it is then we check if the error value is
        // inside the base error's range of values. If it is then we call the same function recursively with the new hash of the inherited
        // error. If the value is bigger or equal to the number of values in the base error then we switch on it. It cannot be outside the
        // range of the values, this should not be possible so we abort in that case too
        builder->SetInsertPoint(case_block);
        const std::optional<const ErrorNode *> parent_error = error->get_parent_node();
        llvm::Value *normalized_err_val = arg_err_val;
        if (parent_error.has_value()) {
            const unsigned int value_count = parent_error.value()->get_value_count();
            llvm::Value *parent_value_count = builder->getInt32(value_count);
            llvm::Value *is_parent_err = builder->CreateICmpULT(arg_err_val, parent_value_count);
            llvm::BasicBlock *is_parent_error_block = llvm::BasicBlock::Create(context, "case_" + error->name + "_is_parent_error");
            llvm::BasicBlock *is_this_error_block = llvm::BasicBlock::Create(context, "case_" + error->name + "_is_this_error");
            builder->CreateCondBr(is_parent_err, is_parent_error_block, is_this_error_block);
            is_parent_error_block->insertInto(get_err_val_str_fn);
            is_this_error_block->insertInto(get_err_val_str_fn);

            // Recrusively call itself with the error id of the parent error and the error value forwarded as-is
            builder->SetInsertPoint(is_parent_error_block);
            llvm::Value *new_parent_id = builder->getInt32(parent_error.value()->error_id);
            llvm::Value *value_from_parent = builder->CreateCall(get_err_val_str_fn, {new_parent_id, arg_err_val}, "value_from_parent");
            builder->CreateRet(value_from_parent);

            // Continue with normal flow, but we need to substract the number of values in the parent from the error value to make the
            // switch below able to work
            builder->SetInsertPoint(is_this_error_block);
            normalized_err_val = builder->CreateSub(arg_err_val, parent_value_count, "normalized_err_val");
        }

        // We switch on the normalized error value and depending on it's value we return one of the possible error values, so we need to
        // prepare a switch for all possible values
        llvm::BasicBlock *default_value_block = llvm::BasicBlock::Create(context, "case_" + error->name + "_default");
        llvm::SwitchInst *value_switch_inst = builder->CreateSwitch(normalized_err_val, default_value_block, error->values.size());

        for (size_t i = 0; i < error->values.size(); i++) {
            const std::string &value = error->values.at(i);
            llvm::BasicBlock *value_case_block = llvm::BasicBlock::Create(                        //
                context, "case_" + error->name + "_case_" + std::to_string(i), get_err_val_str_fn //
            );
            builder->SetInsertPoint(value_case_block);
            value_switch_inst->addCase(builder->getInt32(i), value_case_block);
            builder->CreateRet(IR::generate_const_string(module, value));
        }

        default_value_block->insertInto(get_err_val_str_fn);
        builder->SetInsertPoint(default_value_block);
        llvm::Value *unknown_err_msg = IR::generate_const_string(module, "Unknown error value '%u' on error id '%u'\n");
        builder->CreateCall(c_functions.at(PRINTF), {unknown_err_msg, arg_err_val, arg_err_type});
        builder->CreateCall(c_functions.at(ABORT), {});
        builder->CreateUnreachable();
    }

    // Default case: print error message and abort
    builder->SetInsertPoint(default_block);
    llvm::Value *unknown_err_msg = IR::generate_const_string(module, "Unknown error type hash: %u\n");
    builder->CreateCall(c_functions.at(PRINTF), {unknown_err_msg, arg_err_type});
    builder->CreateCall(c_functions.at(ABORT), {});
    builder->CreateUnreachable();
}

void Generator::Error::generate_get_err_str_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    llvm::Function *strlen_fn = c_functions.at(STRLEN);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *get_err_type_str_fn = error_functions.at("get_err_type_str");
    llvm::Function *get_err_val_str_fn = error_functions.at("get_err_val_str");
    llvm::Function *create_str_fn = Module::String::string_manip_functions.at("create_str");

    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::StructType *error_type = type_map.at("__flint_type_err");
    llvm::FunctionType *get_err_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // returns str*
        {error_type},                                               // Takes the error to create a string from
        false                                                       // No vaargs
    );
    llvm::Function *get_err_str_fn = llvm::Function::Create( //
        get_err_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "__flint_get_err_str",                               //
        module                                               //
    );
    error_functions["get_err_str"] = get_err_str_fn;

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_err_str_fn);

    // Get the parameter (err)
    llvm::Argument *arg_err = get_err_str_fn->arg_begin();
    arg_err->setName("err");

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the error type id and value id from the error argument
    llvm::Value *err_type_id = builder->CreateExtractValue(arg_err, 0, "err_type_id");
    llvm::Value *err_value_id = builder->CreateExtractValue(arg_err, 1, "err_value_id");
    // Get the error type and value strings
    llvm::Value *err_type_str = builder->CreateCall(get_err_type_str_fn, {err_type_id}, "err_type_str");
    llvm::Value *err_val_str = builder->CreateCall(get_err_val_str_fn, {err_type_id, err_value_id}, "err_val_str");
    // Find out the length of both strings
    llvm::Value *err_type_str_len = builder->CreateCall(strlen_fn, {err_type_str}, "err_type_str_len");
    llvm::Value *err_val_str_len = builder->CreateCall(strlen_fn, {err_val_str}, "err_val_str_len");
    // Calculate the length of the new string with the format `<ErrType>.<ErrValue>`
    llvm::Value *err_str_len = builder->CreateAdd(             //
        builder->CreateAdd(err_type_str_len, err_val_str_len), //
        builder->getInt64(1),                                  //
        "err_str_len"                                          //
    );
    llvm::Value *err_str = builder->CreateCall(create_str_fn, {err_str_len}, "err_str");
    llvm::Value *err_str_type_ptr = builder->CreateStructGEP(str_type, err_str, 1, "err_str_type_ptr");
    builder->CreateCall(memcpy_fn, {err_str_type_ptr, err_type_str, err_type_str_len});
    llvm::Value *dot_ptr = builder->CreateGEP(builder->getInt8Ty(), err_str_type_ptr, err_type_str_len, "dot_ptr");
    IR::aligned_store(*builder, builder->getInt8('.'), dot_ptr);
    llvm::Value *err_str_val_ptr = builder->CreateGEP(builder->getInt8Ty(), dot_ptr, builder->getInt32(1));
    builder->CreateCall(memcpy_fn, {err_str_val_ptr, err_val_str, err_val_str_len});
    builder->CreateRet(err_str);
}
