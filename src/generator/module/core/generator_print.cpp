#include "generator/generator.hpp"

static const Hash hash(std::string("print"));
static const std::string prefix = hash.to_string() + ".print.";

void Generator::Module::Print::generate_print_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::U, 8);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::I, 8);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::U, 16);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::I, 16);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::U, 32);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::I, 32);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::U, 64);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::I, 64);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::F, 32);
    generate_print_function(builder, module, only_declarations, PrimitivePrintType::F, 64);
    generate_print_str_lit_function(builder, module, only_declarations);
    generate_print_str_var_function(builder, module, only_declarations);
    generate_print_bool_function(builder, module, only_declarations);
}

void Generator::Module::Print::generate_print_function( //
    llvm::IRBuilder<> *builder,                         //
    llvm::Module *module,                               //
    const bool only_declarations,                       //
    const PrimitivePrintType type,                      //
    const size_t N                                      //
) {
    assert(N <= 64);
    // Create print function type
    llvm::Type *arg_type = nullptr;
    char prefix_char = 'i';
    switch (type) {
        case PrimitivePrintType::I:
            prefix_char = 'i';
            arg_type = llvm::Type::getIntNTy(context, N);
            break;
        case PrimitivePrintType::U:
            prefix_char = 'u';
            arg_type = llvm::Type::getIntNTy(context, N);
            break;
        case PrimitivePrintType::F:
            prefix_char = 'f';
            if (N == 32) {
                arg_type = llvm::Type::getFloatTy(context);
            } else if (N == 64) {
                arg_type = llvm::Type::getDoubleTy(context);
            } else {
                assert(false);
            }
            break;
    }
    llvm::FunctionType *print_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                       // return void
        {arg_type},                                           // takes type
        false                                                 // no vararg
    );
    // Create the print_X function
    const std::string type_str = prefix_char + std::to_string(N);
    llvm::Function *print_function = llvm::Function::Create( //
        print_type,                                          //
        llvm::Function::ExternalLinkage,                     //
        prefix + type_str,                                   //
        module                                               //
    );
    print_functions[type_str] = print_function;
    if (only_declarations) {
        return;
    }

    llvm::BasicBlock *block = llvm::BasicBlock::Create( //
        context,                                        //
        "entry",                                        //
        print_function                                  //
    );

    // Set insert point to the current block
    builder->SetInsertPoint(block);

    // Convert it to fit the correct format printf expects
    llvm::Value *arg = print_function->getArg(0);
    std::string format = "";
    switch (type) {
        case PrimitivePrintType::I:
            if (N <= 32) {
                format = "%i";
                arg = builder->CreateSExtOrTrunc(arg, llvm::Type::getInt32Ty(context));
            } else if (N <= 64) {
                format = "%li";
                arg = builder->CreateSExtOrTrunc(arg, llvm::Type::getInt64Ty(context));
            }
            break;
        case PrimitivePrintType::U:
            if (N <= 32) {
                format = "%u";
                arg = builder->CreateZExtOrTrunc(arg, llvm::Type::getInt32Ty(context));
            } else if (N <= 64) {
                format = "%lu";
                arg = builder->CreateZExtOrTrunc(arg, llvm::Type::getInt64Ty(context));
            }
            break;
        case PrimitivePrintType::F:
            if (N == 32) {
                format = "%f";
                // arg = TypeCast::f32_to_f64(*builder, arg);
            } else if (N == 64) {
                format = "%d";
            }
            break;
    }

    // Call printf with format string and argument
    llvm::Value *format_str = IR::generate_const_string(module, format);
    builder->CreateCall(c_functions.at(PRINTF), //
        {format_str, arg}                       //
    );

    builder->CreateRetVoid();
}

void Generator::Module::Print::generate_print_str_lit_function( //
    llvm::IRBuilder<> *builder,                                 //
    llvm::Module *module,                                       //
    const bool only_declarations                                //
) {
    llvm::Type *str_lit_type = IR::get_type(module, Type::get_primitive_type("type.flint.str.lit")).first;

    // Create print function type
    llvm::FunctionType *print_str_lit_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                               // Return type: void
        {str_lit_type},                                               // Argument: char* literal
        false                                                         // No vaargs
    );
    // Create the print_str_lit function
    llvm::Function *print_str_lit_function = llvm::Function::Create( //
        print_str_lit_type,                                          //
        llvm::Function::ExternalLinkage,                             //
        prefix + "str_lit",                                          //
        module                                                       //
    );
    print_functions["type.flint.str.lit"] = print_str_lit_function;
    if (only_declarations) {
        return;
    }

    llvm::BasicBlock *block = llvm::BasicBlock::Create( //
        context,                                        //
        "entry",                                        //
        print_str_lit_function                          //
    );

    // Set insert point to the current block
    builder->SetInsertPoint(block);

    // Set the argument name of the function
    llvm::Value *arg_literal = print_str_lit_function->getArg(0);
    arg_literal->setName("literal");

    // Call printf with the literal directly
    builder->CreateCall(c_functions.at(PRINTF), {arg_literal});

    builder->CreateRetVoid();
}

void Generator::Module::Print::generate_print_str_var_function(llvm::IRBuilder<> *builder, llvm::Module *module,
    const bool only_declarations) {
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;

    // Create print function type
    llvm::FunctionType *print_str_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                           // Return Type: void
        {str_type->getPointerTo()},                               // Argument: str* string
        false                                                     // No varargs
    );
    // Create the print_int function
    llvm::Function *print_str_function = llvm::Function::Create( //
        print_str_type,                                          //
        llvm::Function::ExternalLinkage,                         //
        prefix + "str",                                          //
        module                                                   //
    );
    print_functions["str"] = print_str_function;
    if (only_declarations) {
        return;
    }

    llvm::BasicBlock *block = llvm::BasicBlock::Create( //
        context,                                        //
        "entry",                                        //
        print_str_function                              //
    );

    // Set insert point to the current block
    builder->SetInsertPoint(block);

    // Convert it to fit the correct format printf expects
    llvm::Value *arg_string = print_str_function->getArg(0);
    arg_string->setName("string");

    llvm::Value *str_len_ptr = builder->CreateStructGEP(str_type, arg_string, 0, "str_len_ptr");
    llvm::Value *str_len = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), str_len_ptr, "str_len");
    llvm::Value *str_len_val = TypeCast::uN_to_iN_trunc(*builder, str_len, 32);

    llvm::Value *str_val_ptr = builder->CreateStructGEP(str_type, arg_string, 1, "str_val_ptr");

    // Call printf with format string and argument
    llvm::Value *format_str = IR::generate_const_string(module, "%.*s");
    builder->CreateCall(c_functions.at(PRINTF), //
        {format_str, str_len_val, str_val_ptr}  //
    );

    builder->CreateRetVoid();
}

void Generator::Module::Print::generate_print_bool_function(llvm::IRBuilder<> *builder, llvm::Module *module,
    const bool only_declarations) {
    // Create print function type
    llvm::FunctionType *print_type = llvm::FunctionType::get(           //
        llvm::Type::getVoidTy(context),                                 // return void
        {IR::get_type(module, Type::get_primitive_type("bool")).first}, // takes type
        false                                                           // no vararg
    );
    // Create the print_int function
    llvm::Function *print_function = llvm::Function::Create( //
        print_type,                                          //
        llvm::Function::ExternalLinkage,                     //
        prefix + "bool",                                     //
        module                                               //
    );
    print_functions["bool"] = print_function;
    if (only_declarations) {
        return;
    }

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        context,                                              //
        "entry",                                              //
        print_function                                        //
    );
    llvm::BasicBlock *true_block = llvm::BasicBlock::Create( //
        context,                                             //
        "bool_true",                                         //
        print_function                                       //
    );
    llvm::BasicBlock *false_block = llvm::BasicBlock::Create( //
        context,                                              //
        "bool_false",                                         //
        print_function                                        //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create( //
        context,                                              //
        "merge",                                              //
        print_function                                        //
    );

    llvm::Argument *arg = print_function->getArg(0);

    // The entry block, create condition and branch
    builder->SetInsertPoint(entry_block);
    llvm::Value *format_str = IR::generate_const_string(module, "%s");
    builder->CreateCondBr(arg, true_block, false_block);

    // True block
    builder->SetInsertPoint(true_block);
    llvm::Value *str_true = IR::generate_const_string(module, "true");
    builder->CreateCall(c_functions.at(PRINTF), {format_str, str_true});
    builder->CreateBr(merge_block);

    // False block
    builder->SetInsertPoint(false_block);
    llvm::Value *str_false = IR::generate_const_string(module, "false");
    builder->CreateCall(c_functions.at(PRINTF), {format_str, str_false});
    builder->CreateBr(merge_block);

    // Merge block
    builder->SetInsertPoint(merge_block);
    builder->CreateRetVoid();
}
