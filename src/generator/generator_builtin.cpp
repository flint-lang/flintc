#include "generator/generator.hpp"
#include "lexer/builtins.hpp"
#include "parser/parser.hpp"
#include "llvm/IR/BasicBlock.h"

void Generator::Builtin::generate_builtin_main(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // Create the FunctionNode of the main function
    // (in order to forward-declare the user defined main function inside the absolute main module)
    std::vector<std::tuple<std::shared_ptr<Type>, std::string, bool>> parameters;
    if (Parser::main_function_has_args) {
        std::optional<std::shared_ptr<Type>> str_arr_type = Type::get_type_from_str("str[]");
        if (!str_arr_type.has_value()) {
            str_arr_type = std::make_shared<ArrayType>(1, Type::get_primitive_type("str"));
            Type::add_type(str_arr_type.value());
        }
        parameters.emplace_back(std::make_tuple(str_arr_type.value(), "args", false));
    }
    std::vector<std::shared_ptr<Type>> return_types;
    std::unique_ptr<Scope> scope;
    FunctionNode function_node = FunctionNode(false, false, "_main", parameters, return_types, scope);

    // Create the declaration of the custom main function
    llvm::StructType *custom_main_ret_type = IR::add_and_or_get_type(Type::get_primitive_type("i32"));
    llvm::FunctionType *custom_main_type = Function::generate_function_type(&function_node);
    llvm::FunctionCallee custom_main_callee = module->getOrInsertFunction(function_node.name, custom_main_type);

    llvm::FunctionType *main_type = nullptr;
    if (Parser::main_function_has_args) {
        main_type = llvm::FunctionType::get(                                               //
            builder->getInt32Ty(),                                                         // Return type: int
            {builder->getInt32Ty(), builder->getInt8Ty()->getPointerTo()->getPointerTo()}, // Takes int argc, char *argv[]
            false                                                                          // no varargs
        );
    } else {
        main_type = llvm::FunctionType::get( //
            builder->getInt32Ty(),           // Return type: int
            {},                              // Takes nothing
            false                            // no varargs
        );
    }
    llvm::Function *main_function = llvm::Function::Create( //
        main_type,                                          //
        llvm::Function::ExternalLinkage,                    //
        "main",                                             //
        module                                              //
    );

    // Create the functions entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        context,                                              //
        "entry",                                              //
        main_function                                         //
    );
    builder->SetInsertPoint(entry_block);

    // Create the return types of the call of the main function.
    llvm::AllocaInst *main_ret = builder->CreateAlloca(custom_main_ret_type, nullptr, "main_ret");
    if (Parser::main_function_has_args) {
        llvm::Argument *argc = main_function->args().begin();
        argc->setName("argc");
        llvm::Argument *argv = main_function->args().begin() + 1;
        argv->setName("argv");
        // Now get the string type
        llvm::Type *str_type = IR::get_type(Type::get_primitive_type("__flint_type_str_struct")).first;
        const llvm::DataLayout &data_layout = module->getDataLayout();
        const unsigned int str_size = data_layout.getTypeAllocSize(str_type) + 8;
        llvm::Value *arr_len = builder->CreateAdd(                                                      //
            builder->getInt64(str_size),                                                                //
            builder->CreateMul(builder->CreateSExt(argc, builder->getInt64Ty()), builder->getInt64(8)), //
            "arr_len"                                                                                   //
        );
        llvm::Value *arr_ptr = builder->CreateCall(c_functions.at(MALLOC), {arr_len}, "arr_ptr");
        // Store 1 in the dimensionality of the array
        llvm::Value *dim_ptr = builder->CreateStructGEP(str_type, arr_ptr, 0, "dim_ptr");
        builder->CreateStore(builder->getInt64(1), dim_ptr);
        // Store the argc in the value field of the array
        llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arr_ptr, 1, "len_ptr");
        builder->CreateStore(builder->CreateSExt(argc, builder->getInt64Ty()), len_ptr);

        // Create the arg_i running variable for the loop
        llvm::AllocaInst *arg_i = builder->CreateAlloca(builder->getInt32Ty(), 0, nullptr, "arg_i");
        builder->CreateStore(builder->getInt32(0), arg_i);

        llvm::BasicBlock *current_block = builder->GetInsertBlock();
        llvm::BasicBlock *arg_save_loop_cond_block = llvm::BasicBlock::Create(context, "arg_save_loop_cond", main_function);
        llvm::BasicBlock *arg_save_loop_body_block = llvm::BasicBlock::Create(context, "arg_save_loop_body", main_function);
        llvm::BasicBlock *arg_save_loop_exit_block = llvm::BasicBlock::Create(context, "arg_save_loop_exit", main_function);
        builder->SetInsertPoint(current_block);
        builder->CreateBr(arg_save_loop_cond_block);

        builder->SetInsertPoint(arg_save_loop_cond_block);
        llvm::Value *arg_i_val = builder->CreateLoad(builder->getInt32Ty(), arg_i, "arg_i_val");
        builder->CreateCondBr(builder->CreateICmpSLT(arg_i_val, argc), arg_save_loop_body_block, arg_save_loop_exit_block);

        builder->SetInsertPoint(arg_save_loop_body_block);
        llvm::Value *argv_element_ptr = builder->CreateGEP(          //
            builder->getInt8Ty()->getPointerTo()->getPointerTo(),    //
            argv,                                                    //
            {builder->CreateSExt(arg_i_val, builder->getInt64Ty())}, //
            "argv_element_ptr"                                       //
        );
        llvm::Value *argv_element = builder->CreateLoad(builder->getInt8Ty()->getPointerTo(), argv_element_ptr, "argv_element");
        llvm::Value *arg_length = builder->CreateCall(c_functions.at(STRLEN), {argv_element}, "arg_length");
        llvm::Value *created_str = builder->CreateCall(                                                     //
            Module::String::string_manip_functions.at("init_str"), {argv_element, arg_length}, "arg_string" //
        );
        llvm::Value *arg_ptr = builder->CreateGEP(                                                              //
            str_type->getPointerTo(), len_ptr, {builder->CreateAdd(arg_i_val, builder->getInt32(1))}, "arg_ptr" //
        );
        builder->CreateStore(created_str, arg_ptr);
        builder->CreateStore(builder->CreateAdd(arg_i_val, builder->getInt32(1)), arg_i);

        builder->CreateBr(arg_save_loop_cond_block);

        builder->SetInsertPoint(arg_save_loop_exit_block);
        llvm::CallInst *main_call = builder->CreateCall(custom_main_callee, {arr_ptr});
        main_call_array[0] = main_call;
        builder->CreateStore(main_call, main_ret);
    } else {
        llvm::CallInst *main_call = builder->CreateCall(custom_main_callee, {});
        main_call_array[0] = main_call;
        builder->CreateStore(main_call, main_ret);
    }

    // First, load the first return value of the return struct
    llvm::Value *err_ptr = builder->CreateStructGEP(custom_main_ret_type, main_ret, 0);
    llvm::LoadInst *err_val = builder->CreateLoad(llvm::Type::getInt32Ty(context), err_ptr, "main_err_val");

    llvm::BasicBlock *current_block = builder->GetInsertBlock();
    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(context, "main_catch", main_function);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "main_merge", main_function);
    builder->SetInsertPoint(current_block);

    // Create the if check and compare the err value to 0
    llvm::ConstantInt *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    llvm::Value *err_condition = builder->CreateICmpNE( //
        err_val,                                        //
        zero,                                           //
        "errcmp"                                        //
    );

    // Create the branching operation
    builder->CreateCondBr(err_condition, catch_block, merge_block)
        ->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context, "Branch to '" + catch_block->getName().str() + "' if 'main' returned error")));

    // Generate the body of the catch block
    builder->SetInsertPoint(catch_block);

    // Create the message that an error has occured
    llvm::Value *message_begin_ptr = IR::generate_const_string(*builder, "ERROR: Program exited with exit code '");
    builder->CreateCall(c_functions.at(PRINTF), {message_begin_ptr});
    // Print the actual error value
    builder->CreateCall(Module::Print::print_functions.at("i32"), {err_val});
    // Print the rest of the string
    llvm::Value *message_end_ptr = IR::generate_const_string(*builder, "'\n");
    builder->CreateCall(c_functions.at(PRINTF), {message_end_ptr});

    builder->CreateBr(merge_block);
    builder->SetInsertPoint(merge_block);
    builder->CreateCall(c_functions.at(EXIT), {err_val});
    builder->CreateUnreachable();
}

void Generator::Builtin::generate_c_functions(llvm::Module *module) {
    // printf
    {
        llvm::FunctionType *printf_type = llvm::FunctionType::get(        //
            llvm::Type::getInt32Ty(context),                              // Return type: int
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)), // Takes char*
            true                                                          // Variadic arguments
        );
        llvm::Function *printf_func = llvm::Function::Create( //
            printf_type,                                      //
            llvm::Function::ExternalLinkage,                  //
            "printf",                                         //
            module                                            //
        );
        c_functions[PRINTF] = printf_func;
    }
    // malloc
    {
        llvm::FunctionType *malloc_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context)->getPointerTo(),        // Return type: void*
            {llvm::Type::getInt64Ty(context)},                     // Arguments: u64
            false                                                  // No vaarg
        );
        llvm::Function *malloc_fn = llvm::Function::Create(malloc_type, llvm::Function::ExternalLinkage, "malloc", module);
        c_functions[MALLOC] = malloc_fn;
    }
    // free
    {
        llvm::FunctionType *free_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context),                      // Return type: void
            {llvm::Type::getVoidTy(context)->getPointerTo()},    // Argument: void*
            false                                                // No vaarg
        );
        llvm::Function *free_fn = llvm::Function::Create(free_type, llvm::Function::ExternalLinkage, "free", module);
        c_functions[FREE] = free_fn;
    }
    // memcpy
    {
        llvm::FunctionType *memcpy_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context)->getPointerTo(),        // Return type: void*
            {
                llvm::Type::getVoidTy(context)->getPointerTo(), // Argument: void*
                llvm::Type::getVoidTy(context)->getPointerTo(), // Argument: const void*
                llvm::Type::getInt64Ty(context)                 // Argument: u64
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *memcpy_fn = llvm::Function::Create(memcpy_type, llvm::Function::ExternalLinkage, "memcpy", module);
        c_functions[MEMCPY] = memcpy_fn;
    }
    // realloc
    {
        llvm::FunctionType *realloc_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context)->getPointerTo(),         // Return type: void*
            {
                llvm::Type::getVoidTy(context)->getPointerTo(), // Argument: void*
                llvm::Type::getInt64Ty(context)                 // Argument: u64
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *realloc_fn = llvm::Function::Create(realloc_type, llvm::Function::ExternalLinkage, "realloc", module);
        c_functions[REALLOC] = realloc_fn;
    }
    // snprintf
    {
        llvm::FunctionType *snprintf_type = llvm::FunctionType::get( //
            llvm::Type::getInt32Ty(context),                         // Return type: i32
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(), // Argument: char* (buffer)
                llvm::Type::getInt64Ty(context),                // Argument: u64 (max_len)
                llvm::Type::getInt8Ty(context)->getPointerTo()  // Argument: char* (format)
            },                                                  //
            true                                                // vaarg
        );
        llvm::Function *snprintf_fn = llvm::Function::Create(snprintf_type, llvm::Function::ExternalLinkage, "snprintf", module);
        c_functions[SNPRINTF] = snprintf_fn;
    }
    // memcmp
    {
        llvm::FunctionType *memcmp_type = llvm::FunctionType::get( //
            llvm::Type::getInt32Ty(context),                       //
            {
                llvm::Type::getVoidTy(context)->getPointerTo(), // Argument: void* (memory address 1)
                llvm::Type::getVoidTy(context)->getPointerTo(), // Argument: void* (memory address 2)
                llvm::Type::getInt64Ty(context)                 // Argument: u64 (size to compare)
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *memcmp_fn = llvm::Function::Create(memcmp_type, llvm::Function::ExternalLinkage, "memcmp", module);
        c_functions[MEMCMP] = memcmp_fn;
    }
    // exit
    {
        // Create the exit call (calling the C exit() function)
        llvm::FunctionType *exit_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context),                      // return void
            llvm::Type::getInt32Ty(context),                     // takes i32
            false                                                // No vaarg
        );
        llvm::Function *exit_fn = llvm::Function::Create(exit_type, llvm::Function::ExternalLinkage, "exit", module);
        c_functions[EXIT] = exit_fn;
    }
    // abort
    {
        llvm::FunctionType *abort_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
        llvm::Function *abort_fn = llvm::Function::Create(abort_type, llvm::Function::ExternalLinkage, "abort", module);
        c_functions[ABORT] = abort_fn;
    }
    // fgetc
    {
        llvm::FunctionType *fgetc_type = llvm::FunctionType::get( //
            llvm::Type::getInt32Ty(context),                      // return i32
            {llvm::Type::getVoidTy(context)->getPointerTo()},     // FILE* stream (as void*)
            false                                                 // no vaarg
        );
        llvm::Function *fgetc_fn = llvm::Function::Create(fgetc_type, llvm::Function::ExternalLinkage, "fgetc", module);
        c_functions[FGETC] = fgetc_fn;
    }
    // memmove
    {
        llvm::FunctionType *memmove_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context)->getPointerTo(),         // return void*
            {
                llvm::Type::getVoidTy(context)->getPointerTo(), // void* dest
                llvm::Type::getVoidTy(context)->getPointerTo(), // void* src
                llvm::Type::getInt64Ty(context)                 // i64 n
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *memmove_fn = llvm::Function::Create(memmove_type, llvm::Function::ExternalLinkage, "memmove", module);
        c_functions[MEMMOVE] = memmove_fn;
    }
    // strtol
    {
        llvm::FunctionType *strtol_type = llvm::FunctionType::get( //
            llvm::Type::getInt64Ty(context),                       // return i64
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(),                 // char* buffer
                llvm::Type::getInt8Ty(context)->getPointerTo()->getPointerTo(), // char** endptr
                llvm::Type::getInt32Ty(context)                                 // i32 base
            },                                                                  //
            false                                                               // No vaarg
        );
        llvm::Function *strtol_fn = llvm::Function::Create(strtol_type, llvm::Function::ExternalLinkage, "strtol", module);
        c_functions[STRTOL] = strtol_fn;
    }
    // strtoul
    {
        llvm::FunctionType *strtoul_type = llvm::FunctionType::get( //
            llvm::Type::getInt64Ty(context),                        // return u64
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(),                 // char* buffer
                llvm::Type::getInt8Ty(context)->getPointerTo()->getPointerTo(), // char** endptr
                llvm::Type::getInt32Ty(context)                                 // i32 base
            },                                                                  //
            false                                                               // No vaarg
        );
        llvm::Function *strtoul_fn = llvm::Function::Create(strtoul_type, llvm::Function::ExternalLinkage, "strtoul", module);
        c_functions[STRTOUL] = strtoul_fn;
    }
    // strtof
    {
        llvm::FunctionType *strtof_type = llvm::FunctionType::get( //
            llvm::Type::getFloatTy(context),                       // return f32
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(),                // char* buffer
                llvm::Type::getInt8Ty(context)->getPointerTo()->getPointerTo() // char** endptr
            },                                                                 //
            false                                                              // No vaarg
        );
        llvm::Function *strtof_fn = llvm::Function::Create(strtof_type, llvm::Function::ExternalLinkage, "strtof", module);
        c_functions[STRTOF] = strtof_fn;
    }
    // strtod
    {
        llvm::FunctionType *strtod_type = llvm::FunctionType::get( //
            llvm::Type::getDoubleTy(context),                      // return f64
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(),                 // char* buffer
                llvm::Type::getInt8Ty(context)->getPointerTo()->getPointerTo(), // char** endptr
                llvm::Type::getInt32Ty(context)                                 // i32 base
            },                                                                  //
            false                                                               // No vaarg
        );
        llvm::Function *strtod_fn = llvm::Function::Create(strtod_type, llvm::Function::ExternalLinkage, "strtod", module);
        c_functions[STRTOD] = strtod_fn;
    }
    // strlen
    {
        llvm::FunctionType *strlen_type = llvm::FunctionType::get( //
            llvm::Type::getInt64Ty(context),                       // return u64
            {llvm::Type::getInt8Ty(context)->getPointerTo()},      // char* str
            false                                                  // No vaarg
        );
        llvm::Function *strlen_fn = llvm::Function::Create(strlen_type, llvm::Function::ExternalLinkage, "strlen", module);
        c_functions[STRLEN] = strlen_fn;
    }
    // fopen
    {
        llvm::FunctionType *fopen_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context)->getPointerTo(),       // return FILE*
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(), // char* filename
                llvm::Type::getInt8Ty(context)->getPointerTo()  // char* modes
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *fopen_fn = llvm::Function::Create(fopen_type, llvm::Function::ExternalLinkage, "fopen", module);
        c_functions[FOPEN] = fopen_fn;
    }
    // fseek
    {
        llvm::FunctionType *fseek_type = llvm::FunctionType::get( //
            llvm::Type::getInt32Ty(context),                      // return i32
            {
                llvm::Type::getVoidTy(context)->getPointerTo(), // FILE* stream
                llvm::Type::getInt64Ty(context),                // i64 offset
                llvm::Type::getInt32Ty(context)                 // i32 whence (whatever that means)
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *fseek_fn = llvm::Function::Create(fseek_type, llvm::Function::ExternalLinkage, "fseek", module);
        c_functions[FSEEK] = fseek_fn;
    }
    // fclose
    {
        llvm::FunctionType *fclose_type = llvm::FunctionType::get( //
            llvm::Type::getInt32Ty(context),                       // return i32
            {llvm::Type::getVoidTy(context)->getPointerTo()},      // FILE* stream
            false                                                  // No vaarg
        );
        llvm::Function *fclose_fn = llvm::Function::Create(fclose_type, llvm::Function::ExternalLinkage, "fclose", module);
        c_functions[FCLOSE] = fclose_fn;
    }
    // ftell
    {
        llvm::FunctionType *ftell_type = llvm::FunctionType::get( //
            llvm::Type::getInt64Ty(context),                      // return i64
            {llvm::Type::getVoidTy(context)->getPointerTo()},     // FILE* stream
            false                                                 // No vaarg
        );
        llvm::Function *ftell_fn = llvm::Function::Create(ftell_type, llvm::Function::ExternalLinkage, "ftell", module);
        c_functions[FTELL] = ftell_fn;
    }
    // fread
    {
        llvm::FunctionType *fread_type = llvm::FunctionType::get( //
            llvm::Type::getInt64Ty(context),                      // return u64
            {
                llvm::Type::getVoidTy(context)->getPointerTo(), // void* ptr
                llvm::Type::getInt64Ty(context),                // u64 size
                llvm::Type::getInt64Ty(context),                // u64 n
                llvm::Type::getVoidTy(context)->getPointerTo()  // FILE* stream
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *fread_fn = llvm::Function::Create(fread_type, llvm::Function::ExternalLinkage, "fread", module);
        c_functions[FREAD] = fread_fn;
    }
    // rewind
    {
        llvm::FunctionType *rewind_type = llvm::FunctionType::get( //
            llvm::Type::getVoidTy(context),                        // return void
            {llvm::Type::getVoidTy(context)->getPointerTo()},      // FILE* stream
            false                                                  // No vaarg
        );
        llvm::Function *rewind_fn = llvm::Function::Create(rewind_type, llvm::Function::ExternalLinkage, "rewind", module);
        c_functions[REWIND] = rewind_fn;
    }
    // fgets
    {
        llvm::FunctionType *fgets_type = llvm::FunctionType::get( //
            llvm::Type::getInt8Ty(context)->getPointerTo(),       // return char*
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(), // char* s
                llvm::Type::getInt32Ty(context),                // i32 n
                llvm::Type::getVoidTy(context)->getPointerTo()  // FILE* stream
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *fgets_fn = llvm::Function::Create(fgets_type, llvm::Function::ExternalLinkage, "fgets", module);
        c_functions[FGETS] = fgets_fn;
    }
    // fwrite
    {
        llvm::FunctionType *fwrite_type = llvm::FunctionType::get( //
            llvm::Type::getInt64Ty(context),                       // return u64
            {
                llvm::Type::getVoidTy(context)->getPointerTo(), // void* ptr
                llvm::Type::getInt64Ty(context),                // u64 size
                llvm::Type::getInt64Ty(context),                // u64 n
                llvm::Type::getVoidTy(context)->getPointerTo()  // FILE* stream
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *fwrite_fn = llvm::Function::Create(fwrite_type, llvm::Function::ExternalLinkage, "fwrite", module);
        c_functions[FWRITE] = fwrite_fn;
    }
    // getenv
    {
        llvm::FunctionType *getenv_type = llvm::FunctionType::get( //
            llvm::Type::getInt8Ty(context)->getPointerTo(),        // return char*
            {llvm::Type::getInt8Ty(context)->getPointerTo()},      // char* name
            false                                                  // No vaarg
        );
        llvm::Function *getenv_fn = llvm::Function::Create(getenv_type, llvm::Function::ExternalLinkage, "getenv", module);
        c_functions[GETENV] = getenv_fn;
    }
    // setenv
    {
        llvm::FunctionType *setenv_type = llvm::FunctionType::get( //
            llvm::Type::getInt32Ty(context),                       // return i32
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(), // char* name
                llvm::Type::getInt8Ty(context)->getPointerTo(), // char* value
                llvm::Type::getInt32Ty(context)                 // i32 replace
            },                                                  //
            false                                               // No vaarg
        );
        llvm::Function *setenv_fn = llvm::Function::Create(setenv_type, llvm::Function::ExternalLinkage, "setenv", module);
        c_functions[SETENV] = setenv_fn;
    }
}

bool Generator::Builtin::refresh_c_functions(llvm::Module *module) {
    c_functions[PRINTF] = module->getFunction("printf");
    c_functions[MALLOC] = module->getFunction("malloc");
    c_functions[FREE] = module->getFunction("free");
    c_functions[MEMCPY] = module->getFunction("memcpy");
    c_functions[REALLOC] = module->getFunction("realloc");
    c_functions[SNPRINTF] = module->getFunction("snprintf");
    c_functions[MEMCMP] = module->getFunction("memcmp");
    c_functions[EXIT] = module->getFunction("exit");
    c_functions[ABORT] = module->getFunction("abort");
    c_functions[FGETC] = module->getFunction("fgetc");
    c_functions[MEMMOVE] = module->getFunction("memmove");
    c_functions[STRTOL] = module->getFunction("strtol");
    c_functions[STRTOUL] = module->getFunction("strtoul");
    c_functions[STRTOF] = module->getFunction("strtof");
    c_functions[STRTOD] = module->getFunction("strtod");
    c_functions[STRLEN] = module->getFunction("strlen");
    c_functions[FOPEN] = module->getFunction("fopen");
    c_functions[FSEEK] = module->getFunction("fseek");
    c_functions[FCLOSE] = module->getFunction("fclose");
    c_functions[FTELL] = module->getFunction("ftell");
    c_functions[FREAD] = module->getFunction("fread");
    c_functions[REWIND] = module->getFunction("rewind");
    c_functions[FGETS] = module->getFunction("fgets");
    c_functions[FWRITE] = module->getFunction("fwrite");
    c_functions[GETENV] = module->getFunction("getenv");
    c_functions[SETENV] = module->getFunction("setenv");
    for (auto &c_function : c_functions) {
        if (c_function.second == nullptr) {
            return false;
        }
    }
    return true;
}

void Generator::Builtin::generate_builtin_test(llvm::IRBuilder<> *builder, llvm::Module *module) {
    llvm::Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    llvm::Value *one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
    llvm::FunctionType *main_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(context),                     // Return type: int
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
        context,                                              //
        "entry",                                              //
        main_function                                         //
    );
    builder->SetInsertPoint(entry_block);

    // Handle the case that there are no tests to run
    if (tests.empty()) {
        llvm::Value *msg = IR::generate_const_string(*builder, "There are no tests to run\n");
        builder->CreateCall(c_functions.at(PRINTF), {msg});
        builder->CreateCall(c_functions.at(EXIT), {zero});
        builder->CreateUnreachable();
        return;
    }

    // Create the counter to count how many tests have failed
    llvm::AllocaInst *counter = builder->CreateAlloca( //
        llvm::Type::getInt32Ty(context),               //
        nullptr,                                       //
        "err_counter"                                  //
    );
    builder->CreateStore(zero, counter);

    // Create the return struct of the test call. It only needs to be allocated once and will be reused by all test calls
    llvm::AllocaInst *test_alloca = builder->CreateAlloca(         //
        IR::add_and_or_get_type(Type::get_primitive_type("void")), //
        nullptr,                                                   //
        "test_alloca"                                              //
    );

    // Go through all files for all tests
    for (const auto &[file_name, test_list] : tests) {
        // Print which file we are currently at
        llvm::Value *success_fmt = IR::generate_const_string(*builder, " -> \"%s\" \033[32m✓ passed\033[0m\n");
        llvm::Value *fail_fmt = IR::generate_const_string(*builder, " -> \"%s\" \033[31m✗ failed\033[0m\n");
        llvm::Value *file_name_value = IR::generate_const_string(*builder, "\n" + file_name + ":\n");
        builder->CreateCall(c_functions.at(PRINTF), //
            {file_name_value}                       //
        );

        // Run all tests and print whether they succeeded
        for (const auto &[test_name, test_function_name] : test_list) {
            llvm::BasicBlock *current_block = builder->GetInsertBlock();
            llvm::BasicBlock *succeed_block = llvm::BasicBlock::Create(context, "test_success", main_function);
            llvm::BasicBlock *fail_block = llvm::BasicBlock::Create(context, "test_fail", main_function);
            llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", main_function);
            builder->SetInsertPoint(current_block);

            // Get the actual test function
            llvm::Function *test_function = module->getFunction(test_function_name);
            if (test_function == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return;
            }

            // Print the tests name
            llvm::Value *test_name_value = IR::generate_const_string(*builder, test_name);

            // Add a call to the actual function
            llvm::CallInst *test_call = builder->CreateCall( //
                test_function,                               //
                {},                                          //
                "call_test"                                  //
            );
            builder->CreateStore(test_call, test_alloca);
            llvm::Value *err_ptr = builder->CreateStructGEP(test_function->getReturnType(), test_alloca, 0, "test_err_ptr");
            llvm::Value *err_value = builder->CreateLoad( //
                llvm::Type::getInt32Ty(context),          //
                err_ptr,                                  //
                "test_er_val"                             //
            );

            // Create branching condition. Go to succeed block if the test_err_val is == 0, to the fail_block otherwise
            llvm::Value *comparison = builder->CreateICmpEQ(err_value, zero, "errcmp");
            // Branch to the succeed block or the fail block depending on the condition
            builder->CreateCondBr(comparison, succeed_block, fail_block);

            builder->SetInsertPoint(succeed_block);
            builder->CreateCall(c_functions.at(PRINTF), //
                {success_fmt, test_name_value}          //
            );
            builder->CreateBr(merge_block);

            builder->SetInsertPoint(fail_block);
            builder->CreateCall(c_functions.at(PRINTF), //
                {fail_fmt, test_name_value}             //
            );
            // Increment the fail counter only if the test has failed
            llvm::LoadInst *counter_value = builder->CreateLoad(llvm::Type::getInt32Ty(context), counter, "counter_val");
            llvm::Value *new_counter_value = builder->CreateCall(                                                    //
                Module::Arithmetic::arithmetic_functions.at("i32_safe_add"), {counter_value, one}, "new_counter_val" //
            );
            builder->CreateStore(new_counter_value, counter);
            builder->CreateBr(merge_block);

            builder->SetInsertPoint(merge_block);
        }
    }

    // Create the comparison with zero
    llvm::Value *counter_value = builder->CreateLoad(llvm::Type::getInt32Ty(context), counter, "counter_value");
    llvm::Value *is_zero = builder->CreateICmpEQ(counter_value, zero);

    // Create basic blocks for the two paths
    llvm::BasicBlock *current_block = builder->GetInsertBlock();
    llvm::BasicBlock *success_block = llvm::BasicBlock::Create(context, "print_success", main_function);
    llvm::BasicBlock *fail_block = llvm::BasicBlock::Create(context, "print_fail", main_function);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", main_function);

    // Branch based on the comparison
    builder->SetInsertPoint(current_block);
    builder->CreateCondBr(is_zero, success_block, fail_block);

    // Success block
    builder->SetInsertPoint(success_block);
    llvm::Value *success_fmt = IR::generate_const_string(*builder, "\n\033[32m✓ All tests passed!\033[0m\n");
    builder->CreateCall(c_functions.at(PRINTF), {success_fmt});
    builder->CreateBr(merge_block);

    // Fail block
    builder->SetInsertPoint(fail_block);
    llvm::Value *fail_fmt = IR::generate_const_string(*builder, "\n\033[31m✗ %d tests failed!\033[0m\n");
    builder->CreateCall(c_functions.at(PRINTF), {fail_fmt, counter_value});
    builder->CreateStore(one, counter);
    builder->CreateBr(merge_block);

    // Merge block and exit
    builder->SetInsertPoint(merge_block);
    counter_value = builder->CreateLoad(llvm::Type::getInt32Ty(context), counter);
    builder->CreateCall(c_functions.at(EXIT), {counter_value});
    builder->CreateUnreachable();
}
