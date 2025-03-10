#include "generator/generator.hpp"

void Generator::Builtin::generate_builtin_main(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // Create the FunctionNode of the main_custom function
    // (in order to forward-declare the user defined main function inside the absolute main module)
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<std::string> return_types;
    std::unique_ptr<Scope> scope;
    FunctionNode function_node = FunctionNode(false, false, "main_custom", parameters, return_types, scope);

    // Create the declaration of the custom main function
    llvm::StructType *custom_main_ret_type = IR::add_and_or_get_type(&builder->getContext(), function_node.return_types);
    llvm::FunctionType *custom_main_type = Function::generate_function_type(module->getContext(), &function_node);
    llvm::FunctionCallee custom_main_callee = module->getOrInsertFunction("main_custom", custom_main_type);

    llvm::FunctionType *main_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(module->getContext()),        // Return type: int
        {},                                                  // Takes nothing
        false                                                // no varargs
    );
    llvm::Function *main_function = llvm::Function::Create( //
        main_type,                                          //
        llvm::Function::ExternalLinkage,                    //
        "main",                                             //
        module                                              //
    );

    // Create the functions entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "entry",                                              //
        main_function                                         //
    );
    builder->SetInsertPoint(entry_block);

    // Create the return types of the call of the main function.
    llvm::AllocaInst *main_ret = builder->CreateAlloca(custom_main_ret_type, nullptr, "custom_main_ret");
    llvm::CallInst *main_call = builder->CreateCall(custom_main_callee, {});
    main_call_array[0] = main_call;
    builder->CreateStore(main_call, main_ret);

    // First, load the first return value of the return struct
    llvm::Value *err_ptr = builder->CreateStructGEP(custom_main_ret_type, main_ret, 0);
    llvm::LoadInst *err_val = builder->CreateLoad(llvm::Type::getInt32Ty(module->getContext()), err_ptr, "main_err_val");

    llvm::BasicBlock *current_block = builder->GetInsertBlock();
    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create( //
        builder->getContext(),                                //
        "main_catch",                                         //
        main_function                                         //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create( //
        builder->getContext(),                                //
        "main_merge",                                         //
        main_function                                         //
    );
    builder->SetInsertPoint(current_block);

    // Create the if check and compare the err value to 0
    llvm::ConstantInt *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder->getContext()), 0);
    llvm::Value *err_condition = builder->CreateICmpNE( //
        err_val,                                        //
        zero,                                           //
        "errcmp"                                        //
    );

    // Create the branching operation
    builder->CreateCondBr(err_condition, catch_block, merge_block)
        ->setMetadata("comment",
            llvm::MDNode::get(module->getContext(),
                llvm::MDString::get(module->getContext(),
                    "Branch to '" + catch_block->getName().str() + "' if 'main_custom' returned error")));

    // Generate the body of the catch block
    builder->SetInsertPoint(catch_block);

    // Function to create a string literal
    std::function<llvm::Value *(llvm::IRBuilder<> *, llvm::Function *, std::string)> //
        create_str_lit =                                                             //
        [](llvm::IRBuilder<> *builder, llvm::Function *parent, std::string message)  //
        -> llvm::Value * {
        std::variant<int, float, std::string, bool, char> value = message;
        LiteralNode message_literal = LiteralNode(value, "str");
        return Expression::generate_literal(*builder, parent, &message_literal);
    };

    // Create the message that an error has occured
    llvm::Value *message_begin_ptr = create_str_lit(builder, main_function, "ERROR: Program exited with exit code '");
    builder->CreateCall(print_functions.at("str"), {message_begin_ptr});
    // Print the actual error value
    builder->CreateCall(print_functions.at("i32"), {err_val});
    // Print the rest of the string
    llvm::Value *message_end_ptr = create_str_lit(builder, main_function, "'\n");
    builder->CreateCall(print_functions.at("str"), {message_end_ptr});

    builder->CreateBr(merge_block);
    builder->SetInsertPoint(merge_block);
    builder->CreateRet(err_val);
}

void Generator::Builtin::generate_builtin_prints(llvm::IRBuilder<> *builder, llvm::Module *module) {
    if (builtins[PRINT] != nullptr) {
        return;
    }
    llvm::FunctionType *printf_type = llvm::FunctionType::get(                     //
        llvm::Type::getInt32Ty(module->getContext()),                              // Return type: int
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(module->getContext())), // Takes char*
        true                                                                       // Variadic arguments
    );
    llvm::Function *printf_func = llvm::Function::Create( //
        printf_type,                                      //
        llvm::Function::ExternalLinkage,                  //
        "printf",                                         //
        module                                            //
    );
    builtins[PRINT] = printf_func;
    generate_builtin_print(builder, module, "i32", "%d");
    generate_builtin_print(builder, module, "i64", "%ld");
    generate_builtin_print(builder, module, "u32", "%u");
    generate_builtin_print(builder, module, "u64", "%lu");
    generate_builtin_print(builder, module, "f32", "%f");
    generate_builtin_print(builder, module, "f64", "%lf");
    generate_builtin_print(builder, module, "char", "%c");
    generate_builtin_print(builder, module, "str", "%s");
    generate_builtin_print_bool(builder, module);
}

void Generator::Builtin::generate_builtin_print( //
    llvm::IRBuilder<> *builder,                  //
    llvm::Module *module,                        //
    const std::string &type,                     //
    const std::string &format                    //
) {
    if (print_functions.at(type) != nullptr) {
        return;
    }

    // Create print function type
    llvm::FunctionType *print_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(module->getContext()),          // return void
        {IR::get_type_from_str(module->getContext(), type)},  // takes type
        false                                                 // no vararg
    );

    // Create the print_int function
    llvm::Function *print_function = llvm::Function::Create( //
        print_type,                                          //
        llvm::Function::ExternalLinkage,                     //
        "print_" + type,                                     //
        module                                               //
    );
    llvm::BasicBlock *block = llvm::BasicBlock::Create( //
        module->getContext(),                           //
        "entry",                                        //
        print_function                                  //
    );
    // Set insert point to the current block
    builder->SetInsertPoint(block);

    // Convert it to fit the correct format printf expects
    llvm::Value *arg = print_function->getArg(0);
    if (type == "f32") {
        arg = TypeCast::f32_to_f64(*builder, arg);
    } else if (type == "i64") {
        arg = builder->CreateSExtOrTrunc(arg, llvm::Type::getInt64Ty(module->getContext()));
    } else if (type == "u64") {
        arg = builder->CreateZExtOrTrunc(arg, llvm::Type::getInt64Ty(module->getContext()));
    }

    // Call printf with format string and argument
    llvm::Value *format_str = builder->CreateGlobalStringPtr(format);
    builder->CreateCall(builtins[PRINT], //
        {format_str, arg}                //
    );

    builder->CreateRetVoid();
    print_functions[type] = print_function;
}

void Generator::Builtin::generate_builtin_print_bool(llvm::IRBuilder<> *builder, llvm::Module *module) {
    if (print_functions.at("bool") != nullptr) {
        return;
    }

    // Create print function type
    llvm::FunctionType *print_type = llvm::FunctionType::get(  //
        llvm::Type::getVoidTy(module->getContext()),           // return void
        {IR::get_type_from_str(module->getContext(), "bool")}, // takes type
        false                                                  // no vararg
    );

    // Create the print_int function
    llvm::Function *print_function = llvm::Function::Create( //
        print_type,                                          //
        llvm::Function::ExternalLinkage,                     //
        "print_bool",                                        //
        module                                               //
    );
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "entry",                                              //
        print_function                                        //
    );
    llvm::BasicBlock *true_block = llvm::BasicBlock::Create( //
        module->getContext(),                                //
        "bool_true",                                         //
        print_function                                       //
    );
    llvm::BasicBlock *false_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "bool_false",                                         //
        print_function                                        //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "merge",                                              //
        print_function                                        //
    );

    llvm::Argument *arg = print_function->getArg(0);

    // Create the constant "true" string and the "false" string
    llvm::Value *format_str = builder->CreateGlobalStringPtr("%s");
    llvm::Value *str_true = builder->CreateGlobalStringPtr("true");
    llvm::Value *str_false = builder->CreateGlobalStringPtr("false");

    // The entry block, create condition and branch
    builder->SetInsertPoint(entry_block);
    builder->CreateCondBr(arg, true_block, false_block);

    // True block
    builder->SetInsertPoint(true_block);
    builder->CreateCall(builtins[PRINT], {format_str, str_true});
    builder->CreateBr(merge_block);

    // False block
    builder->SetInsertPoint(false_block);
    builder->CreateCall(builtins[PRINT], {format_str, str_false});
    builder->CreateBr(merge_block);

    // Merge block
    builder->SetInsertPoint(merge_block);
    builder->CreateRetVoid();

    print_functions["bool"] = print_function;
}

void Generator::Builtin::generate_builtin_test(llvm::IRBuilder<> *builder, llvm::Module *module) {
    llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(module->getContext()), 0);
    llvm::Value *one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(module->getContext()), 1);
    llvm::FunctionType *main_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(module->getContext()),        // Return type: int
        {},                                                  // Takes nothing
        false                                                // no varargs
    );
    llvm::Function *main_function = llvm::Function::Create( //
        main_type,                                          //
        llvm::Function::ExternalLinkage,                    //
        "main",                                             //
        module                                              //
    );

    // Create the functions entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "entry",                                              //
        main_function                                         //
    );
    builder->SetInsertPoint(entry_block);

    // Handle the case that there are no tests to run
    if (tests.empty()) {
        llvm::Value *msg = builder->CreateGlobalStringPtr("There are no tests to run!\n", "no_tests_msg", 0, module);
        builder->CreateCall(builtins[PRINT], {msg});
        builder->CreateRet(zero);
        return;
    }

    // Create the counter to count how many tests have failed
    llvm::AllocaInst *counter = builder->CreateAlloca( //
        llvm::Type::getInt32Ty(module->getContext()),  //
        nullptr,                                       //
        "err_counter"                                  //
    );
    builder->CreateStore(zero, counter);

    // Create the return struct of the test call. It only needs to be allocated once and will be reused by all test calls
    llvm::AllocaInst *test_alloca = builder->CreateAlloca(  //
        IR::add_and_or_get_type(&module->getContext(), {}), //
        nullptr,                                            //
        "test_alloca"                                       //
    );

    // Go through all files for all tests
    for (const auto &[file_name, test_list] : tests) {
        // Print which file we are currently at
        llvm::Value *success_fmt = builder->CreateGlobalStringPtr(" -> \"%s\" \033[32m✓ passed\033[0m\n", "success_str", 0, module);
        llvm::Value *fail_fmt = builder->CreateGlobalStringPtr(" -> \"%s\" \033[31m✗ failed\033[0m\n", "fail_str", 0, module);
        llvm::Value *file_name_value = builder->CreateGlobalStringPtr("\n" + file_name + ":\n");
        builder->CreateCall(builtins[PRINT], //
            {file_name_value}                //
        );

        // Run all tests and print whether they succeeded
        for (const auto &[test_name, test_function_name] : test_list) {
            llvm::BasicBlock *current_block = builder->GetInsertBlock();
            llvm::BasicBlock *succeed_block = llvm::BasicBlock::Create(module->getContext(), "test_success", main_function);
            llvm::BasicBlock *fail_block = llvm::BasicBlock::Create(module->getContext(), "test_fail", main_function);
            llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(module->getContext(), "merge", main_function);
            builder->SetInsertPoint(current_block);

            // Get the actual test function
            llvm::Function *test_function = module->getFunction(test_function_name);
            if (test_function == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return;
            }

            // Print the tests name
            llvm::Value *test_name_value = builder->CreateGlobalStringPtr(test_name);

            // Add a call to the actual function
            llvm::CallInst *test_call = builder->CreateCall( //
                test_function,                               //
                {},                                          //
                "call_test"                                  //
            );
            builder->CreateStore(test_call, test_alloca);
            llvm::Value *err_ptr = builder->CreateStructGEP(test_function->getReturnType(), test_alloca, 0, "test_err_ptr");
            llvm::Value *err_value = builder->CreateLoad(     //
                llvm::Type::getInt32Ty(module->getContext()), //
                err_ptr,                                      //
                "test_er_val"                                 //
            );

            // Create branching condition. Go to succeed block if the test_err_val is != 0, to the fail_block otherwise
            llvm::Value *comparison = builder->CreateICmpEQ(err_value, zero, "errcmp");
            // Branch to the succeed block or the fail block depending on the condition
            builder->CreateCondBr(comparison, succeed_block, fail_block);

            builder->SetInsertPoint(succeed_block);
            builder->CreateCall(builtins[PRINT], //
                {success_fmt, test_name_value}   //
            );
            builder->CreateBr(merge_block);

            builder->SetInsertPoint(fail_block);
            builder->CreateCall(builtins[PRINT], //
                {fail_fmt, test_name_value}      //
            );
            // Increment the fail counter only if the test has failed
            llvm::LoadInst *counter_value = builder->CreateLoad(llvm::Type::getInt32Ty(module->getContext()), counter, "counter_val");
            llvm::Value *new_counter_value = Arithmetic::int_safe_add(*builder, counter_value, one);
            builder->CreateStore(new_counter_value, counter);
            builder->CreateBr(merge_block);

            builder->SetInsertPoint(merge_block);
        }
    }

    // Create the comparison with zero
    llvm::Value *counter_value = builder->CreateLoad(llvm::Type::getInt32Ty(module->getContext()), counter, "counter_value");
    llvm::Value *is_zero = builder->CreateICmpEQ(counter_value, zero);

    // Create basic blocks for the two paths
    llvm::BasicBlock *current_block = builder->GetInsertBlock();
    llvm::BasicBlock *success_block = llvm::BasicBlock::Create(module->getContext(), "print_success", main_function);
    llvm::BasicBlock *fail_block = llvm::BasicBlock::Create(module->getContext(), "print_fail", main_function);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(module->getContext(), "merge", main_function);

    // Branch based on the comparison
    builder->SetInsertPoint(current_block);
    builder->CreateCondBr(is_zero, success_block, fail_block);

    // Success block
    builder->SetInsertPoint(success_block);
    llvm::Value *success_fmt = builder->CreateGlobalStringPtr("\n\033[32m✓ All tests passed!\033[0m\n", "success_fmt", 0, module);
    builder->CreateCall(builtins[PRINT], {success_fmt});
    builder->CreateBr(merge_block);

    // Fail block
    builder->SetInsertPoint(fail_block);
    llvm::Value *fail_fmt = builder->CreateGlobalStringPtr("\n\033[31m✗ %d tests failed!\033[0m\n", "fail_fmt", 0, module);
    builder->CreateCall(builtins[PRINT], {fail_fmt, counter_value});
    builder->CreateStore(one, counter);
    builder->CreateBr(merge_block);

    // Merge block and return
    builder->SetInsertPoint(merge_block);
    counter_value = builder->CreateLoad(llvm::Type::getInt32Ty(module->getContext()), counter);
    builder->CreateRet(counter_value);
}

llvm::Function *Generator::Builtin::generate_test_function(llvm::Module *module, const TestNode *test_node) {
    llvm::StructType *void_type = IR::add_and_or_get_type(&module->getContext(), {});

    // Create the function type
    llvm::FunctionType *test_type = llvm::FunctionType::get( //
        void_type,                                           // return { i32 }
        {},                                                  // takes nothing
        false                                                // no vararg
    );
    // Create the test function itself
    llvm::Function *test_function = llvm::Function::Create( //
        test_type,                                          //
        llvm::Function::ExternalLinkage,                    //
        "___test_" + std::to_string(test_node->test_id),    //
        module                                              //
    );

    // Create the entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "entry",                                              //
        test_function                                         //
    );

    // The test function has no parameters when called, it just returns whether it has succeeded through the error value
    llvm::IRBuilder<> builder(entry_block);
    std::unordered_map<std::string, llvm::AllocaInst *const> allocations;
    Allocation::generate_allocations(builder, test_function, test_node->scope.get(), allocations);
    // Normally generate the tests body
    Statement::generate_body(builder, test_function, test_node->scope.get(), allocations);

    // Check if the function has a terminator, if not add an "empty" return (only the error return)
    if (!test_function->empty() && test_function->back().getTerminator() == nullptr) {
        Statement::generate_return_statement(builder, test_function, test_node->scope.get(), allocations, {});
    }

    return test_function;
}
