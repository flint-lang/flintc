#include "generator/generator.hpp"

static const std::string prefix = "flint.typecast.";

void Generator::Module::TypeCast::generate_typecast_functions( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    if (!only_declarations) {
        generate_count_digits_function(builder, module);
    }
    generate_bool_to_str(builder, module, only_declarations);
    generate_uN_to_str(builder, module, only_declarations, 8);
    generate_uN_to_str(builder, module, only_declarations, 16);
    generate_uN_to_str(builder, module, only_declarations, 32);
    generate_uN_to_str(builder, module, only_declarations, 64);
    generate_iN_to_str(builder, module, only_declarations, 8);
    generate_iN_to_str(builder, module, only_declarations, 16);
    generate_iN_to_str(builder, module, only_declarations, 32);
    generate_iN_to_str(builder, module, only_declarations, 64);
    generate_f32_to_str(builder, module, only_declarations);
    generate_f64_to_str(builder, module, only_declarations);
    generate_bool8_to_str_function(builder, module, only_declarations);
    generate_multitype_to_str(builder, module, only_declarations, "u8", 2);
    generate_multitype_to_str(builder, module, only_declarations, "u8", 3);
    generate_multitype_to_str(builder, module, only_declarations, "u8", 4);
    generate_multitype_to_str(builder, module, only_declarations, "u8", 8);
    generate_multitype_to_str(builder, module, only_declarations, "i32", 2);
    generate_multitype_to_str(builder, module, only_declarations, "i32", 3);
    generate_multitype_to_str(builder, module, only_declarations, "i32", 4);
    generate_multitype_to_str(builder, module, only_declarations, "i32", 8);
    generate_multitype_to_str(builder, module, only_declarations, "i64", 2);
    generate_multitype_to_str(builder, module, only_declarations, "i64", 3);
    generate_multitype_to_str(builder, module, only_declarations, "i64", 4);
    generate_multitype_to_str(builder, module, only_declarations, "f32", 2);
    generate_multitype_to_str(builder, module, only_declarations, "f32", 3);
    generate_multitype_to_str(builder, module, only_declarations, "f32", 4);
    generate_multitype_to_str(builder, module, only_declarations, "f32", 8);
    generate_multitype_to_str(builder, module, only_declarations, "f64", 2);
    generate_multitype_to_str(builder, module, only_declarations, "f64", 3);
    generate_multitype_to_str(builder, module, only_declarations, "f64", 4);
}

llvm::Value *Generator::Module::TypeCast::iN_to_uN_ext( //
    llvm::IRBuilder<> &builder,                         //
    llvm::Value *const expr,                            //
    const size_t N                                      //
) {
    // Compare with 0
    auto zero = llvm::ConstantInt::get(expr->getType(), 0);
    auto is_negative = builder.CreateICmpSLT(expr, zero, "is_neg");

    // Zero-extend the value (for positive numbers)
    auto extended = builder.CreateZExt(expr, llvm::Type::getIntNTy(context, N), "zext");
    auto zero16 = llvm::ConstantInt::get(extended->getType(), 0);

    // Select between 0 and the extended value
    const size_t src_N = expr->getType()->getIntegerBitWidth();
    return builder.CreateSelect(is_negative, zero16, extended, "safe_i" + std::to_string(src_N) + "_to_u" + std::to_string(N));
}

llvm::Value *Generator::Module::TypeCast::uN_to_uN_trunc( //
    llvm::IRBuilder<> &builder,                           //
    llvm::Value *const expr,                              //
    const size_t N                                        //
) {
    const size_t src_width = expr->getType()->getIntegerBitWidth();
    assert(src_width > N);
    const llvm::APInt max_int = llvm::APInt::getMaxValue(N).zext(src_width);
    auto max = llvm::ConstantInt::get(expr->getType(), max_int);
    auto too_large = builder.CreateICmpUGT(expr, max);
    auto clamped = builder.CreateSelect(too_large, max, expr);
    return builder.CreateTrunc(clamped, llvm::Type::getIntNTy(context, N));
}

llvm::Value *Generator::Module::TypeCast::uN_to_iN_trunc( //
    llvm::IRBuilder<> &builder,                           //
    llvm::Value *const expr,                              //
    const size_t N                                        //
) {
    const size_t src_width = expr->getType()->getIntegerBitWidth();
    assert(src_width > N);
    const llvm::APInt max_int = llvm::APInt::getSignedMaxValue(N).zext(src_width);
    auto max = llvm::ConstantInt::get(expr->getType(), max_int);
    auto too_large = builder.CreateICmpUGT(expr, max);
    auto clamped = builder.CreateSelect(too_large, max, expr);
    return builder.CreateTrunc(clamped, llvm::Type::getIntNTy(context, N));
}

llvm::Value *Generator::Module::TypeCast::iN_to_uN_trunc(llvm::IRBuilder<> &builder, //
    llvm::Value *const expr,                                                         //
    const size_t N                                                                   //
) {
    const size_t src_width = expr->getType()->getIntegerBitWidth();
    assert(src_width > N);
    auto zero = llvm::ConstantInt::get(expr->getType(), 0);
    const llvm::APInt max_int = llvm::APInt::getMaxValue(N).zext(src_width);
    auto max = llvm::ConstantInt::get(expr->getType(), max_int);

    // Clamp to 0 if negative
    auto is_negative = builder.CreateICmpSLT(expr, zero);
    auto clamped_negative = builder.CreateSelect(is_negative, zero, expr);

    // Clamp to max if too large
    auto is_too_large = builder.CreateICmpSGT(clamped_negative, max);
    auto clamped = builder.CreateSelect(is_too_large, max, clamped_negative);

    // Finally truncate to N bits
    return builder.CreateTrunc(clamped, llvm::Type::getIntNTy(context, N));
}

llvm::Value *Generator::Module::TypeCast::iN_to_iN_trunc( //
    llvm::IRBuilder<> &builder,                           //
    llvm::Value *const expr,                              //
    const size_t N                                        //
) {
    const size_t src_width = expr->getType()->getIntegerBitWidth();
    assert(src_width > N);
    const llvm::APInt min_int = llvm::APInt::getSignedMinValue(N).sext(src_width);
    const llvm::APInt max_int = llvm::APInt::getSignedMaxValue(N).sext(src_width);
    auto min = llvm::ConstantInt::get(expr->getType(), min_int);
    auto max = llvm::ConstantInt::get(expr->getType(), max_int);

    // Clamp to min if smaller
    auto is_smaller = builder.CreateICmpSLT(expr, min);
    auto clamped_min = builder.CreateSelect(is_smaller, min, expr);

    // Clamp to max if bigger
    auto is_bigger = builder.CreateICmpSGT(expr, max);
    auto clamped = builder.CreateSelect(is_bigger, max, clamped_min);

    // Finally truncate to N bits
    return builder.CreateTrunc(clamped, llvm::Type::getIntNTy(context, N));
}

llvm::Value *Generator::Module::TypeCast::uN_to_iN_same( //
    llvm::IRBuilder<> &builder,                          //
    llvm::Value *const expr                              //
) {
    const llvm::APInt max_int = llvm::APInt::getSignedMaxValue(expr->getType()->getIntegerBitWidth());
    auto max = llvm::ConstantInt::get(expr->getType(), max_int);
    auto too_large = builder.CreateICmpUGT(expr, max);
    return builder.CreateSelect(too_large, max, expr);
}

llvm::Value *Generator::Module::TypeCast::iN_to_uN_same( //
    llvm::IRBuilder<> &builder,                          //
    llvm::Value *const expr                              //
) {
    auto zero = llvm::ConstantInt::get(expr->getType(), 0);
    auto is_negative = builder.CreateICmpSLT(expr, zero, "is_neg");
    return builder.CreateSelect(is_negative, zero, expr, "safe_iN_to_uN_same");
}

llvm::Value *Generator::Module::TypeCast::uN_to_f32(llvm::IRBuilder<> &builder, llvm::Value *const expr) {
    return builder.CreateUIToFP(expr, llvm::Type::getFloatTy(context), "uitof32");
}

llvm::Value *Generator::Module::TypeCast::uN_to_f64(llvm::IRBuilder<> &builder, llvm::Value *const expr) {
    return builder.CreateUIToFP(expr, llvm::Type::getDoubleTy(context), "uitof64");
}

llvm::Value *Generator::Module::TypeCast::iN_to_f32(llvm::IRBuilder<> &builder, llvm::Value *const expr) {
    return builder.CreateSIToFP(expr, llvm::Type::getFloatTy(context), "sitof32");
}

llvm::Value *Generator::Module::TypeCast::iN_to_f64(llvm::IRBuilder<> &builder, llvm::Value *const expr) {
    return builder.CreateSIToFP(expr, llvm::Type::getDoubleTy(context), "sitof64");
}

llvm::Value *Generator::Module::TypeCast::fN_to_iN(llvm::IRBuilder<> &builder, llvm::Value *float_value, const size_t N) {
    return builder.CreateFPToSI(float_value, llvm::Type::getIntNTy(context, N), "fptosi" + std::to_string(N));
}

llvm::Value *Generator::Module::TypeCast::fN_to_uN(llvm::IRBuilder<> &builder, llvm::Value *float_value, const size_t N) {
    return builder.CreateFPToUI(float_value, llvm::Type::getIntNTy(context, N), "fptoui" + std::to_string(N));
}

llvm::Value *Generator::Module::TypeCast::f32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPExt(float_value, llvm::Type::getDoubleTy(context), "fpext");
}

llvm::Value *Generator::Module::TypeCast::f64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPTrunc(double_value, llvm::Type::getFloatTy(context), "fptrunc");
}

void Generator::Module::TypeCast::generate_count_digits_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // C IMPLEMENTATION:
    // size_t count_digits(size_t n) {
    //     if (n == 0) {
    //         return 1;
    //     }
    //     size_t count = 0;
    //     while (n > 0) {
    //         n /= 10;
    //         count++;
    //     }
    //     return count;
    // }

    // Create function type: size_t (size_t)
    llvm::FunctionType *count_digits_type = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), // Return type: size_t (i64)
        {llvm::Type::getInt64Ty(context)},                                                           // Parameter: size_t (i64)
        false                                                                                        // Not varargs
    );

    // Create the function
    llvm::Function *count_digits_fn = llvm::Function::Create( //
        count_digits_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        prefix + "count_digits",                              //
        module                                                //
    );

    // Set parameter name
    llvm::Argument *n_arg = count_digits_fn->arg_begin();
    n_arg->setName("n");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", count_digits_fn);
    llvm::BasicBlock *check_zero_block = llvm::BasicBlock::Create(context, "check_zero", count_digits_fn);
    llvm::BasicBlock *return_one_block = llvm::BasicBlock::Create(context, "return_one", count_digits_fn);
    llvm::BasicBlock *loop_block = llvm::BasicBlock::Create(context, "loop", count_digits_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", count_digits_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", count_digits_fn);

    // Entry block: Allocate variables and check if n is 0
    builder->SetInsertPoint(entry_block);
    llvm::Value *n = builder->CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "n_var");
    llvm::Value *count = builder->CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "count_var");
    IR::aligned_store(*builder, n_arg, n);
    IR::aligned_store(*builder, builder->getInt64(0), count);
    builder->CreateBr(check_zero_block);

    // Check if n is 0
    builder->SetInsertPoint(check_zero_block);
    llvm::Value *n_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), n, "n_val");
    llvm::Value *is_zero = builder->CreateICmpEQ(n_value, builder->getInt64(0), "is_zero");
    builder->CreateCondBr(is_zero, return_one_block, loop_block);

    // Return 1 if n is 0
    builder->SetInsertPoint(return_one_block);
    builder->CreateRet(builder->getInt64(1));

    // Loop header - check condition
    builder->SetInsertPoint(loop_block);
    llvm::Value *loop_n = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), n, "loop_n");
    llvm::Value *loop_condition = builder->CreateICmpUGT(loop_n, builder->getInt64(0), "loop_condition");
    builder->CreateCondBr(loop_condition, loop_body_block, exit_block);

    // Loop body
    builder->SetInsertPoint(loop_body_block);
    // n = n / 10
    llvm::Value *n_val = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), n, "n_val");
    llvm::Value *new_n = builder->CreateUDiv(n_val, builder->getInt64(10), "new_n");
    IR::aligned_store(*builder, new_n, n);

    // count++
    llvm::Value *count_val = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), count, "count_val");
    llvm::Value *new_count = builder->CreateAdd(count_val, builder->getInt64(1), "new_count");
    IR::aligned_store(*builder, new_count, count);

    // Go back to loop header
    builder->CreateBr(loop_block);

    // Exit block - return count
    builder->SetInsertPoint(exit_block);
    llvm::Value *result = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), count, "result");
    builder->CreateRet(result);

    // Store the function for later use
    typecast_functions["count_digits"] = count_digits_fn;
}

void Generator::Module::TypeCast::generate_bool_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *bool_to_str(const bool b_value) {
    //     if (b_value) {
    //         return init_str("true", 4);
    //     } else {
    //         return init_str("false", 5);
    //     }
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");

    llvm::FunctionType *bool_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return type: str*
        {llvm::Type::getInt1Ty(context)},                           // Argument: i1 b_value
        false                                                       // No varargs
    );
    llvm::Function *bool_to_str_fn = llvm::Function::Create( //
        bool_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        prefix + "bool_to_str",                              //
        module                                               //
    );
    typecast_functions["bool_to_str"] = bool_to_str_fn;
    if (only_declarations) {
        return;
    }

    llvm::Argument *arg_bvalue = bool_to_str_fn->arg_begin();
    arg_bvalue->setName("b_value");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", bool_to_str_fn);
    llvm::BasicBlock *true_block = llvm::BasicBlock::Create(context, "true", bool_to_str_fn);
    llvm::BasicBlock *false_block = llvm::BasicBlock::Create(context, "false", bool_to_str_fn);
    builder->SetInsertPoint(entry_block);
    builder->CreateCondBr(arg_bvalue, true_block, false_block);

    builder->SetInsertPoint(true_block);
    llvm::Value *true_string = IR::generate_const_string(module, "true");
    llvm::Value *true_str = builder->CreateCall(init_str_fn, {true_string, builder->getInt64(4)}, "true_str");
    builder->CreateRet(true_str);

    builder->SetInsertPoint(false_block);
    llvm::Value *false_string = IR::generate_const_string(module, "false");
    llvm::Value *false_str = builder->CreateCall(init_str_fn, {false_string, builder->getInt64(5)}, "false_str");
    builder->CreateRet(false_str);
}

void Generator::Module::TypeCast::generate_multitype_to_str( //
    llvm::IRBuilder<> *builder,                              //
    llvm::Module *module,                                    //
    const bool only_declarations,                            //
    const std::string &type_str,                             //
    const size_t width                                       //
) {
    // There exists no C function because C doesnt have multi-types. For now, we assume that `mult_val` is the passed in multi-value
    // THE C IMPLEMENTATION:
    // str *i32x2_to_str(int mult_val[2]) {
    //     // Create the strings for each value
    //     str *val_1_str = i32_to_str(mult_val[0]);
    //     str *val_2_str = i32_to_str(mult_val[1]);
    //     // Create and fill the result string
    //     str *i32x2_str = init_str("(", 1);
    //     append_str(i32x2_str, val_1_str);
    //     append_lit(i32x2_str, ", ", 2);
    //     append_str(i32x2_str, val_2_str);
    //     append_lit(i32_x2_str, ")", 1);
    //     free(val_1_str);
    //     free(val_2_str);
    //     return i32x2_str;
    // }
    const std::string multitype_string = type_str + "x" + std::to_string(width);
    llvm::Type *multi_type = IR::get_type(module, Type::get_type_from_str(multitype_string).value()).first;
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *elem_to_str_fn = typecast_functions.at(type_str + "_to_str");
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *append_str_fn = String::string_manip_functions.at("append_str");
    llvm::Function *append_lit_fn = String::string_manip_functions.at("append_lit");
    llvm::Function *free_fn = c_functions.at(FREE);

    const std::string typecast_function_name = multitype_string + "_to_str";
    llvm::FunctionType *multitype_to_str_type = llvm::FunctionType::get(str_type->getPointerTo(), {multi_type}, false);
    llvm::Function *multitype_to_str_fn = llvm::Function::Create(                                       //
        multitype_to_str_type, llvm::Function::ExternalLinkage, prefix + typecast_function_name, module //
    );
    typecast_functions[typecast_function_name] = multitype_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", multitype_to_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the argument
    llvm::Argument *arg_mult_val = multitype_to_str_fn->arg_begin();
    arg_mult_val->setName("mult_val");

    // Create the base strings of the multi-types values
    std::vector<llvm::Value *> value_strings;
    for (size_t i = 0; i < width; i++) {
        llvm::Value *element_value = builder->CreateExtractElement(arg_mult_val, i, "elem_" + std::to_string(i));
        value_strings.emplace_back(builder->CreateCall(elem_to_str_fn, {element_value}, "elem_" + std::to_string(i) + "_str"));
    }

    // Create the multi-type string with '(' in it
    llvm::Value *lparen_chars = IR::generate_const_string(module, "(");
    llvm::AllocaInst *multitype_str_alloca = builder->CreateAlloca(str_type->getPointerTo(), 0, nullptr, "mt_alloca");
    llvm::Value *multitype_str = builder->CreateCall(init_str_fn, {lparen_chars, builder->getInt64(1)}, multitype_string + "_str");
    IR::aligned_store(*builder, multitype_str, multitype_str_alloca);

    // Fill the multi-type string with the value strings and `, ` separators between them
    llvm::Value *comma_chars = IR::generate_const_string(module, ", ");
    for (size_t i = 0; i < width; i++) {
        if (i > 0) {
            builder->CreateCall(append_lit_fn, {multitype_str_alloca, comma_chars, builder->getInt64(2)});
        }
        builder->CreateCall(append_str_fn, {multitype_str_alloca, value_strings[i]});
    }

    // Append the end of the string with a ')' symbol
    llvm::Value *rparen_chars = IR::generate_const_string(module, ")");
    builder->CreateCall(append_lit_fn, {multitype_str_alloca, rparen_chars, builder->getInt64(1)});

    // Free the value strings
    for (size_t i = 0; i < width; i++) {
        builder->CreateCall(free_fn, {value_strings[i]});
    }

    // Return the result
    multitype_str = IR::aligned_load(*builder, str_type->getPointerTo(), multitype_str_alloca);
    builder->CreateRet(multitype_str);
}

void Generator::Module::TypeCast::generate_uN_to_str( //
    llvm::IRBuilder<> *builder,                       //
    llvm::Module *module,                             //
    const bool only_declarations, const size_t N      //
) {
    // C IMPLEMENTATION:
    // str *uN_to_str(const uintN_t u_value) {
    //     // Handle special case for 0
    //     if (u_value == 0) {
    //         return init_str(1, "0");
    //     }
    //
    //     // Count digits
    //     uint64_t u_value_64 = (uint64_t)u_value;
    //     size_t len = count_digits(u_value_64);
    //
    //     // Allocate string
    //     str *result = create_str(len);
    //     char *buffer = result->value + len; // Start at the end
    //
    //     // Write digits in reverse order
    //     uintN_t value = u_value;
    //     do {
    //         *--buffer = '0' + (value % 10);
    //         value /= 10;
    //     } while (value > 0);
    //
    //     return result;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *const init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *const count_digits_fn = typecast_functions.at("count_digits");
    llvm::Function *const create_str_fn = String::string_manip_functions.at("create_str");
    llvm::IntegerType *const uintN_t = llvm::IntegerType::getIntNTy(context, N);

    llvm::FunctionType *const uN_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                       // Return type: str*
        {uintN_t},                                                      // Argument: uN u_value
        false                                                           // No varargs
    );
    const std::string uN_to_str_fn_name = "u" + std::to_string(N) + "_to_str";
    llvm::Function *const uN_to_str_fn = llvm::Function::Create( //
        uN_to_str_type,                                          //
        llvm::Function::ExternalLinkage,                         //
        prefix + uN_to_str_fn_name,                              //
        module                                                   //
    );
    typecast_functions[uN_to_str_fn_name] = uN_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", uN_to_str_fn);
    llvm::BasicBlock *const zero_case_block = llvm::BasicBlock::Create(context, "zero_case", uN_to_str_fn);
    llvm::BasicBlock *const nonzero_case_block = llvm::BasicBlock::Create(context, "nonzero_case", uN_to_str_fn);
    llvm::BasicBlock *const loop_block = llvm::BasicBlock::Create(context, "loop", uN_to_str_fn);
    llvm::BasicBlock *const exit_block = llvm::BasicBlock::Create(context, "exit", uN_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *const arg_uvalue = uN_to_str_fn->arg_begin();
    arg_uvalue->setName("u_value");

    // Check if u_value == 0
    llvm::Value *is_zero = builder->CreateICmpEQ(arg_uvalue, llvm::ConstantInt::get(builder->getIntNTy(N), 0), "is_zero");
    builder->CreateCondBr(is_zero, zero_case_block, nonzero_case_block);

    // Zero case block
    builder->SetInsertPoint(zero_case_block);
    // Init the string "0" and return it
    llvm::Value *zero_string_lit = IR::generate_const_string(module, "0");
    llvm::Value *zero_string = builder->CreateCall(init_str_fn, {zero_string_lit, builder->getInt64(1)}, "zero_string");
    builder->CreateRet(zero_string);

    // Non-zero case block
    builder->SetInsertPoint(nonzero_case_block);
    // Count digits - call count_digits((uint64_t)u_value)
    llvm::Value *len = arg_uvalue;
    assert(N <= 64);
    if (N < 64) {
        llvm::Value *u64_value = builder->CreateZExt(arg_uvalue, builder->getInt64Ty());
        len = builder->CreateCall(count_digits_fn, {u64_value}, "len");
    } else {
        len = builder->CreateCall(count_digits_fn, {arg_uvalue}, "len");
    }

    // Allocate string - call create_str(len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {len}, "result");

    // Get pointer to result->value (the flexible array member)
    llvm::Value *data_ptr = builder->CreateStructGEP(str_type, result, 1, "data_ptr");

    // Create local variable for buffer
    llvm::Value *current_buffer = builder->CreateAlloca(builder->getPtrTy(), 0, nullptr, "current_buffer");
    // Calculate buffer = result->value + len
    llvm::Value *buffer = builder->CreateGEP(builder->getInt8Ty(), data_ptr, len, "buffer");
    // Store the calculated buffer in the variable
    IR::aligned_store(*builder, buffer, current_buffer);

    // Create local variable for value
    llvm::Value *current_value = builder->CreateAlloca(builder->getIntNTy(N), 0, nullptr, "current_value");
    IR::aligned_store(*builder, arg_uvalue, current_value);

    // Branch to the loop
    builder->CreateBr(loop_block);

    // Loop block
    builder->SetInsertPoint(loop_block);
    // Load current value and buffer
    llvm::Value *value_load = IR::aligned_load(*builder, builder->getIntNTy(N), current_value, "value_load");
    llvm::Value *buffer_load = IR::aligned_load(*builder, builder->getPtrTy(), current_buffer, "buffer_load");

    // Calculate value % 10
    llvm::Value *remainder = builder->CreateURem(value_load, llvm::ConstantInt::get(builder->getIntNTy(N), 10), "remainder");

    // Calculate '0' + (value % 10)
    llvm::Value *digit_char = builder->CreateAdd(              //
        llvm::ConstantInt::get(builder->getInt8Ty(), '0'),     //
        builder->CreateTrunc(remainder, builder->getInt8Ty()), //
        "digit_char"                                           //
    );

    // Decrement buffer
    llvm::Value *buffer_prev = builder->CreateGEP(               //
        builder->getInt8Ty(),                                    //
        buffer_load,                                             //
        llvm::ConstantInt::getSigned(builder->getInt32Ty(), -1), //
        "buffer_prev"                                            //
    );

    // Store the digit character
    IR::aligned_store(*builder, digit_char, buffer_prev);

    // Update buffer
    IR::aligned_store(*builder, buffer_prev, current_buffer);

    // Calculate value / 10
    llvm::Value *new_value = builder->CreateUDiv(value_load, llvm::ConstantInt::get(builder->getIntNTy(N), 10), "new_value");

    // Update value
    IR::aligned_store(*builder, new_value, current_value);

    // Check if value > 0
    llvm::Value *continue_loop = builder->CreateICmpUGT(new_value, llvm::ConstantInt::get(builder->getIntNTy(N), 0), "continue_loop");

    // Branch based on condition
    builder->CreateCondBr(continue_loop, loop_block, exit_block);

    // Exit block
    builder->SetInsertPoint(exit_block);
    builder->CreateRet(result);
}

void Generator::Module::TypeCast::generate_iN_to_str( //
    llvm::IRBuilder<> *builder,                       //
    llvm::Module *module,                             //
    const bool only_declarations,                     //
    const size_t N                                    //
) {
    // C IMPLEMENTATION:
    // str *iN_to_str(const intN_t i_value) {
    //     // Handle special case of minimum value (can't be negated safely)
    //     if (i_value == INTN_MIN) {
    //         const char *min_str = "...";
    //         return init_str(min_str, M);
    //     }
    //
    //     // Handle sign
    //     int is_negative = i_value < 0;
    //     uintN_t value = is_negative ? -i_value : i_value;
    //
    //     // Count digits
    //     size_t num_digits = count_digits(value);
    //     size_t len = num_digits + (is_negative ? 1 : 0);
    //
    //     // Allocate string
    //     str *result = create_str(len);
    //     char *buffer = result->value + len; // Start at the end
    //
    //     // Write digits in reverse order
    //     do {
    //         *--buffer = '0' + (value % 10);
    //         value /= 10;
    //     } while (value > 0);
    //
    //     // Add sign if negative
    //     if (is_negative) {
    //         *--buffer = '-';
    //     }
    //
    //     return result;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *const init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *const count_digits_fn = typecast_functions.at("count_digits");
    llvm::Function *const create_str_fn = String::string_manip_functions.at("create_str");
    llvm::IntegerType *const intN_t = llvm::IntegerType::getIntNTy(context, N);

    llvm::FunctionType *const iN_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                       // Return type: str*
        {intN_t},                                                       // Argument: iN i_value
        false                                                           // No varargs
    );
    const std::string iN_to_str_fn_name = "i" + std::to_string(N) + "_to_str";
    llvm::Function *const iN_to_str_fn = llvm::Function::Create( //
        iN_to_str_type,                                          //
        llvm::Function::ExternalLinkage,                         //
        prefix + iN_to_str_fn_name,                              //
        module                                                   //
    );
    typecast_functions[iN_to_str_fn_name] = iN_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", iN_to_str_fn);
    llvm::BasicBlock *const min_value_block = llvm::BasicBlock::Create(context, "min_value", iN_to_str_fn);
    llvm::BasicBlock *const regular_case_block = llvm::BasicBlock::Create(context, "regular_case", iN_to_str_fn);
    llvm::BasicBlock *const digit_loop_block = llvm::BasicBlock::Create(context, "digit_loop", iN_to_str_fn);
    llvm::BasicBlock *const negative_sign_block = llvm::BasicBlock::Create(context, "negative_sign", iN_to_str_fn);
    llvm::BasicBlock *const add_sign_block = llvm::BasicBlock::Create(context, "add_sign", iN_to_str_fn);
    llvm::BasicBlock *const return_block = llvm::BasicBlock::Create(context, "return", iN_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *const arg_ivalue = iN_to_str_fn->arg_begin();
    arg_ivalue->setName("i_value");

    // Create constant for INTN_MIN
    const llvm::APInt min = llvm::APInt::getSignedMinValue(N);
    llvm::SmallVector<char, 64> strbuf;
    min.toStringSigned(strbuf);
    std::string min_str;
    llvm::raw_string_ostream min_str_stream(min_str);
    min.print(min_str_stream, true);
    min_str_stream.flush();
    auto intN_min = llvm::ConstantInt::get(intN_t, min);

    // Check if i_value == INTN_MIN
    llvm::Value *is_min_value = builder->CreateICmpEQ(arg_ivalue, intN_min, "is_min_value");
    builder->CreateCondBr(is_min_value, min_value_block, regular_case_block);

    // Handle INTN_MIN special case
    builder->SetInsertPoint(min_value_block);
    llvm::Value *min_str_ptr = IR::generate_const_string(module, min_str);
    llvm::Value *min_result = builder->CreateCall(init_str_fn, {min_str_ptr, builder->getInt64(min_str.size())}, "min_result");
    builder->CreateRet(min_result);

    // Regular case - handle sign
    builder->SetInsertPoint(regular_case_block);
    llvm::Value *is_negative = builder->CreateICmpSLT(arg_ivalue, builder->getIntN(N, 0), "is_negative");
    llvm::Value *abs_value = builder->CreateSelect(is_negative, builder->CreateNeg(arg_ivalue, "negated"), arg_ivalue, "abs_value");

    // Convert to uint32_t for consistent handling
    assert(N <= 64);
    llvm::Value *value = abs_value;
    if (N < 64) {
        value = builder->CreateZExt(abs_value, builder->getInt64Ty(), "value_u64");
    }

    // Count digits
    llvm::Value *num_digits = builder->CreateCall(count_digits_fn, {value}, "num_digits");

    // Calculate length: num_digits + (is_negative ? 1 : 0)
    llvm::Value *sign_len = builder->CreateSelect(is_negative, builder->getInt64(1), builder->getInt64(0), "sign_len");
    llvm::Value *total_len = builder->CreateAdd(num_digits, sign_len, "total_len");

    // Allocate string
    llvm::Value *result = builder->CreateCall(create_str_fn, {total_len}, "result");

    // Get pointer to the string data area
    llvm::Value *data_ptr = builder->CreateStructGEP(str_type, result, 1, "data_ptr");

    // Create a GEP to the end of the buffer (data + len)
    llvm::Value *buffer_end = builder->CreateGEP(builder->getInt8Ty(), data_ptr, total_len, "buffer_end");

    // Create variables for the loop
    llvm::Value *current_value_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "current_value_ptr");
    llvm::Value *current_buffer_ptr = builder->CreateAlloca(builder->getPtrTy(), nullptr, "current_buffer_ptr");

    // Initialize values
    IR::aligned_store(*builder, value, current_value_ptr);
    IR::aligned_store(*builder, buffer_end, current_buffer_ptr);

    // Start the digit loop
    builder->CreateBr(digit_loop_block);

    // Digit loop
    builder->SetInsertPoint(digit_loop_block);

    // Load current value
    llvm::Value *current_value = IR::aligned_load(*builder, builder->getInt64Ty(), current_value_ptr, "current_value");

    // Calculate digit: value % 10
    llvm::Value *remainder = builder->CreateURem(current_value, builder->getInt64(10), "remainder");
    llvm::Value *digit_char =
        builder->CreateAdd(builder->getInt8('0'), builder->CreateTrunc(remainder, builder->getInt8Ty(), "digit"), "digit_char");

    // Decrement buffer pointer
    llvm::Value *buffer_ptr = IR::aligned_load(*builder, builder->getPtrTy(), current_buffer_ptr, "buffer_ptr");
    llvm::Value *prev_buffer = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, builder->getInt32(-1), "prev_buffer");
    IR::aligned_store(*builder, prev_buffer, current_buffer_ptr);

    // Store digit at the decremented position
    IR::aligned_store(*builder, digit_char, prev_buffer);

    // Divide value by 10
    llvm::Value *next_value = builder->CreateUDiv(current_value, builder->getInt64(10), "next_value");
    IR::aligned_store(*builder, next_value, current_value_ptr);

    // Continue loop if value > 0
    llvm::Value *continue_loop = builder->CreateICmpUGT(next_value, builder->getInt64(0), "continue_loop");
    builder->CreateCondBr(continue_loop, digit_loop_block, negative_sign_block);

    // Add negative sign if needed
    builder->SetInsertPoint(negative_sign_block);
    llvm::Value *should_add_sign = builder->CreateICmpEQ(sign_len, builder->getInt64(1), "should_add_sign");
    builder->CreateCondBr(should_add_sign, add_sign_block, return_block);

    // Add negative sign
    builder->SetInsertPoint(add_sign_block);
    llvm::Value *sign_buffer_ptr = IR::aligned_load(*builder, builder->getPtrTy(), current_buffer_ptr, "sign_buffer_ptr");
    llvm::Value *sign_prev_buffer = builder->CreateGEP(builder->getInt8Ty(), sign_buffer_ptr, builder->getInt32(-1), "sign_prev_buffer");
    IR::aligned_store(*builder, builder->getInt8('-'), sign_prev_buffer);
    builder->CreateBr(return_block);

    // Return the result of the function
    builder->SetInsertPoint(return_block);
    builder->CreateRet(result);
}

void Generator::Module::TypeCast::generate_bool8_to_str_function( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations                                  //
) {
    // The string result of the bool8 value will always be of size 8. Each character of the string is either '0' or '1', depending of the
    // bit value of each element
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    llvm::FunctionType *bool8_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                    // Return type: str*
        {llvm::Type::getInt8Ty(context)},                            // Argument: bool8 b8
        false                                                        // No varargs
    );
    llvm::Function *bool8_to_str_fn = llvm::Function::Create( //
        bool8_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        prefix + "bool8_to_str",                              //
        module                                                //
    );
    typecast_functions["bool8_to_str"] = bool8_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create entry point
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", bool8_to_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *arg_b8 = bool8_to_str_fn->arg_begin();
    arg_b8->setName("b8");

    // Create a string of size 8
    llvm::Value *b8_str = builder->CreateCall(create_str_fn, {builder->getInt64(8)}, "b8_str");
    llvm::Value *zero_char = builder->getInt8(48);
    llvm::Value *one_char = builder->getInt8(49);

    // Get the pointer to the actual data of the string
    llvm::Value *str_data_ptr = builder->CreateStructGEP(str_type, b8_str, 1, "str_data_ptr");

    // Set all characters of the string
    for (unsigned int i = 0; i < 8; i++) {
        // Shift right by i bits and AND with 1 to extract the i-th bit
        llvm::Value *bit_i = builder->CreateAnd(builder->CreateLShr(arg_b8, builder->getInt8(i)), builder->getInt8(1), "extract_bit");
        // Convert the result (i8 with value 0 or 1) to i1 (boolean)
        llvm::Value *bool_bit = builder->CreateTrunc(bit_i, builder->getInt1Ty(), "to_bool");
        // Get the pointer to the current character
        llvm::Value *char_ptr = builder->CreateGEP(builder->getInt8Ty(), str_data_ptr, builder->getInt32(7 - i), "char_ptr");
        // Store either one or zero at the given character depending on the bool bit
        IR::aligned_store(*builder, builder->CreateSelect(bool_bit, one_char, zero_char), char_ptr);
    }

    // Return the created string
    builder->CreateRet(b8_str);
}

void Generator::Module::TypeCast::generate_f32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *f32_to_str(const float f_value) {
    //     // Handle special cases
    //     if (f_value != f_value) {
    //         return init_str("nan", 3); // NaN check
    //     }
    //
    //     // Use a union for type punning (equivalent to bitcast in LLVM IR)
    //     union {
    //         float f;
    //         uint32_t bits;
    //     } u;
    //     u.f = f_value;
    //
    //     // Infinity check
    //     if ((u.bits & 0x7FFFFFFF) == 0x7F800000) {
    //         return (u.bits & 0x80000000) ? init_str("-inf", 4) : init_str("inf", 3);
    //     }
    //
    //     // For now, use a buffer large enough for any float representation
    //     char buffer[32];
    //     int len = 0;
    //     const float f_pow = f_value * f_value;
    //     if (f_pow < 1e-8f || f_pow > 1e12f) {
    //         len = snprintf(buffer, sizeof(buffer), "%.6e", f_value);
    //     } else {
    //         len = snprintf(buffer, sizeof(buffer), "%.6f", f_value);
    //     }
    //
    //     // Trim trailing zeros after decimal point
    //     int last_non_zero = len - 1;
    //     while (last_non_zero > 0 && buffer[last_non_zero] == '0') {
    //         last_non_zero--;
    //     }
    //
    //     // If we ended up at the decimal point, remove it too
    //     if (buffer[last_non_zero] == '.') {
    //         last_non_zero--;
    //     }
    //
    //     return init_str(buffer, last_non_zero + 1);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *snprintf_fn = c_functions.at(SNPRINTF);

    llvm::FunctionType *f32_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getFloatTy(context)},                         // Argument: f32 f_value
        false                                                      // No varargs
    );
    llvm::Function *f32_to_str_fn = llvm::Function::Create( //
        f32_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        prefix + "f32_to_str",                              //
        module                                              //
    );
    typecast_functions["f32_to_str"] = f32_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", f32_to_str_fn);
    llvm::BasicBlock *nan_block = llvm::BasicBlock::Create(context, "nan_case", f32_to_str_fn);
    llvm::BasicBlock *nan_merge_block = llvm::BasicBlock::Create(context, "nan_merge", f32_to_str_fn);
    llvm::BasicBlock *inf_block = llvm::BasicBlock::Create(context, "inf_case", f32_to_str_fn);
    llvm::BasicBlock *inf_merge_block = llvm::BasicBlock::Create(context, "inf_merge", f32_to_str_fn);
    llvm::BasicBlock *exponent_block = llvm::BasicBlock::Create(context, "exponent_case", f32_to_str_fn);
    llvm::BasicBlock *no_exponent_block = llvm::BasicBlock::Create(context, "no_exponent_case", f32_to_str_fn);
    llvm::BasicBlock *exponent_merge_block = llvm::BasicBlock::Create(context, "exponent_merge", f32_to_str_fn);
    llvm::BasicBlock *loop_block = llvm::BasicBlock::Create(context, "loop", f32_to_str_fn);
    llvm::BasicBlock *check_char_block = llvm::BasicBlock::Create(context, "check_char", f32_to_str_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", f32_to_str_fn);
    llvm::BasicBlock *loop_merge_block = llvm::BasicBlock::Create(context, "loop_merge", f32_to_str_fn);
    llvm::BasicBlock *decimal_case_block = llvm::BasicBlock::Create(context, "decimal_case", f32_to_str_fn);
    llvm::BasicBlock *return_block = llvm::BasicBlock::Create(context, "return", f32_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *arg_fvalue = f32_to_str_fn->arg_begin();
    arg_fvalue->setName("f_value");

    llvm::Value *is_nan_condition = builder->CreateFCmpUNE(arg_fvalue, arg_fvalue, "is_nan_cmp");
    builder->CreateCondBr(is_nan_condition, nan_block, nan_merge_block);

    // The nan_block
    {
        builder->SetInsertPoint(nan_block);
        llvm::Value *nan_str = IR::generate_const_string(module, "nan");
        llvm::Value *nan_str_value = builder->CreateCall(                          //
            init_str_fn,                                                           //
            {nan_str, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 3)}, //
            "nan_str_value"                                                        //
        );
        builder->CreateRet(nan_str_value);
    }

    // The nan_merge_block
    builder->SetInsertPoint(nan_merge_block);
    // First get all the bit values of the floating point number through bitcasting
    llvm::Value *bits = builder->CreateBitCast(arg_fvalue, builder->getInt32Ty(), "bits");

    // Create the bitmask constants
    llvm::Value *abs_mask = builder->getInt32(0x7FFFFFFF);    // Mask to clear the sign bit
    llvm::Value *inf_pattern = builder->getInt32(0x7F800000); // Bit pattern for infinity
    llvm::Value *sign_mask = builder->getInt32(0x80000000);   // Mask to extract the sign bit

    // Perform bitwise operations
    llvm::Value *abs_bits = builder->CreateAnd(bits, abs_mask, "abs_bits");       // bits & 0x7FFFFFFF
    llvm::Value *is_inf = builder->CreateICmpEQ(abs_bits, inf_pattern, "is_inf"); // (bits & 0x7FFFFFFF) == 0x7F800000
    builder->CreateCondBr(is_inf, inf_block, inf_merge_block);

    // The inf_block
    {
        builder->SetInsertPoint(inf_block);
        // Check if negative infinity
        llvm::Value *sign_bit = builder->CreateAnd(bits, sign_mask, "sign_bit"); // bits & 0x80000000
        llvm::Value *is_neg_inf = builder->CreateICmpNE(sign_bit, builder->getInt32(0), "is_neg_inf");

        // Create the two possible strings and select based on sign
        llvm::Value *neg_inf_str = IR::generate_const_string(module, "-inf");
        llvm::Value *pos_inf_str = IR::generate_const_string(module, "inf");

        // Create string for negative infinity
        llvm::Value *neg_inf_value = builder->CreateCall(init_str_fn, {neg_inf_str, builder->getInt64(4)}, "neg_inf_value");

        // Create string for positive infinity
        llvm::Value *pos_inf_value = builder->CreateCall(init_str_fn, {pos_inf_str, builder->getInt64(3)}, "pos_inf_value");

        // Select the appropriate string based on sign
        llvm::Value *inf_result = builder->CreateSelect(is_neg_inf, neg_inf_value, pos_inf_value, "inf_result");
        builder->CreateRet(inf_result);
    }

    // The inf_merge_block
    builder->SetInsertPoint(inf_merge_block);
    // First, create the char buffer[32]
    llvm::AllocaInst *buffer = builder->CreateAlloca(   //
        llvm::ArrayType::get(builder->getInt8Ty(), 32), // Type: [32 x i8]
        nullptr,                                        // No array size (it's fixed)
        "buffer"                                        //
    );
    buffer->setAlignment(llvm::Align(8));
    llvm::Value *buffer_ptr = builder->CreateBitCast(buffer, builder->getInt8Ty()->getPointerTo(), "buffer_ptr");
    // Create the len variable
    llvm::AllocaInst *len_var = builder->CreateAlloca(builder->getInt32Ty(), 0, nullptr, "len_var");
    llvm::Value *f_pow = builder->CreateFMul(arg_fvalue, arg_fvalue, "f_pow");
    llvm::Constant *min_pow = llvm::ConstantFP::get(builder->getFloatTy(), static_cast<double>(1.0e-8f));
    llvm::Constant *max_pow = llvm::ConstantFP::get(builder->getFloatTy(), static_cast<double>(1.0e12f));
    llvm::Value *is_too_small = builder->CreateFCmpOLT(f_pow, min_pow, "is_too_small");
    llvm::Value *is_too_large = builder->CreateFCmpOGT(f_pow, max_pow, "is_too_large");
    llvm::Value *exponent_condition = builder->CreateOr(is_too_small, is_too_large, "exponent_condition");
    builder->CreateCondBr(exponent_condition, exponent_block, no_exponent_block);

    // The exponent_block
    {
        builder->SetInsertPoint(exponent_block);
        llvm::Value *snprintf_format = IR::generate_const_string(module, "%.6e");
        llvm::Value *f_value_as_d = f32_to_f64(*builder, arg_fvalue);
        llvm::CallInst *snprintf_ret = builder->CreateCall(snprintf_fn,
            {
                buffer_ptr,                                        //
                llvm::ConstantInt::get(builder->getInt64Ty(), 32), //
                snprintf_format,                                   //
                f_value_as_d                                       //
            },                                                     //
            "snprintf_ret_e"                                       //
        );
        IR::aligned_store(*builder, snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }
    // The no_exponent_block
    {
        builder->SetInsertPoint(no_exponent_block);
        llvm::Value *snprintf_format = IR::generate_const_string(module, "%.6f");
        llvm::Value *f_value_as_d = f32_to_f64(*builder, arg_fvalue);
        llvm::Value *snprintf_ret = builder->CreateCall(snprintf_fn,
            {
                buffer_ptr,                                        //
                llvm::ConstantInt::get(builder->getInt64Ty(), 32), //
                snprintf_format,                                   //
                f_value_as_d                                       //
            },                                                     //
            "snprintf_ret_f"                                       //
        );
        IR::aligned_store(*builder, snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }

    // The exponent_merge_block
    builder->SetInsertPoint(exponent_merge_block);
    llvm::Value *last_non_zero = builder->CreateAlloca(builder->getInt32Ty(), 0, nullptr, "last_non_zero");
    llvm::Value *len_value = IR::aligned_load(*builder, builder->getInt32Ty(), len_var, "len_val");
    llvm::Value *len_minus_1 = builder->CreateSub(len_value, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "len_m_1");
    IR::aligned_store(*builder, len_minus_1, last_non_zero);
    builder->CreateBr(loop_block);

    // The loop_block
    builder->SetInsertPoint(loop_block);
    // Load current value of last_non_zero
    llvm::Value *last_zero_val = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "last_zero_val");
    // Check if last_non_zero > 0
    llvm::Value *is_valid_index = builder->CreateICmpSGT( //
        last_zero_val,                                    //
        llvm::ConstantInt::get(builder->getInt32Ty(), 0), //
        "is_valid_index"                                  //
    );
    builder->CreateCondBr(is_valid_index, check_char_block, loop_merge_block);

    // The check_char_block
    {
        builder->SetInsertPoint(check_char_block);
        // Get pointer to buffer[last_non_zero]
        llvm::Value *cur_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, last_zero_val, "cur_char_ptr");
        // Load the current character
        llvm::Value *cur_char = IR::aligned_load(*builder, builder->getInt8Ty(), cur_char_ptr, "cur_char");
        // Check if the current character is '0'
        llvm::Value *is_zero = builder->CreateICmpEQ(cur_char, llvm::ConstantInt::get(builder->getInt8Ty(), '0'), "is_zero");
        // Combine conditions: should continue if index > 0 && char == '0'
        llvm::Value *should_continue = builder->CreateAnd(is_valid_index, is_zero, "should_continue");
        // Branch to loop body or merge
        builder->CreateCondBr(should_continue, loop_body_block, loop_merge_block);
    }

    // The loop_body_block
    {
        builder->SetInsertPoint(loop_body_block);
        // Decrement last_non_zero
        last_zero_val = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "last_zero_val");
        llvm::Value *new_last_zero = builder->CreateSub(last_zero_val, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "new_last_zero");
        IR::aligned_store(*builder, new_last_zero, last_non_zero);
        // Jump back to loop condition
        builder->CreateBr(loop_block);
    }

    // The loop_merge_block
    {
        builder->SetInsertPoint(loop_merge_block);
        // Load final value of last_non_zero
        llvm::Value *final_last_zero = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "final_last_zero");
        // Get pointer to buffer[last_non_zero]
        llvm::Value *last_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, final_last_zero, "last_char_ptr");
        // Load the character
        llvm::Value *last_char = IR::aligned_load(*builder, builder->getInt8Ty(), last_char_ptr, "last_char");
        // Check if the character is '.'
        llvm::Value *is_dot = builder->CreateICmpEQ(last_char, llvm::ConstantInt::get(builder->getInt8Ty(), '.'), "is_dot");
        // Branch based on whether the character is a decimal point
        builder->CreateCondBr(is_dot, decimal_case_block, return_block);
    }

    // The decimal_case_block - handle case where we need to remove decimal point
    {
        builder->SetInsertPoint(decimal_case_block);
        // Decrement last_non_zero one more time
        llvm::Value *decimal_last_zero = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "decimal_last_zero");
        llvm::Value *adjusted_last_zero = builder->CreateSub( //
            decimal_last_zero,                                //
            llvm::ConstantInt::get(builder->getInt32Ty(), 1), //
            "adjusted_last_zero"                              //
        );
        IR::aligned_store(*builder, adjusted_last_zero, last_non_zero);
        // Branch to return block
        builder->CreateBr(return_block);
    }

    // The return_block
    {
        builder->SetInsertPoint(return_block);
        // Calculate final length: last_non_zero + 1
        llvm::Value *final_last_zero = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "final_last_zero");
        llvm::Value *final_len = builder->CreateAdd(final_last_zero, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "final_len");
        // Convert to i64 for init_str
        llvm::Value *final_len_i64 = builder->CreateZExt(final_len, builder->getInt64Ty(), "final_len_i64");
        // Call init_str with buffer and calculated length
        llvm::Value *result = builder->CreateCall(init_str_fn, {buffer_ptr, final_len_i64}, "result");
        // Return the string
        builder->CreateRet(result);
    }
}

void Generator::Module::TypeCast::generate_f64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *f64_to_str(const double d_value) {
    //     // Handle special cases
    //     if (d_value != d_value) {
    //         return init_str("nan", 3); // NaN check
    //     }
    //
    //     // Use a union for type punning (equivalent to bitcast in LLVM IR)
    //     union {
    //         double d;
    //         uint64_t bits;
    //     } u;
    //     u.d = d_value;
    //
    //     // Infinity check
    //     if ((u.bits & 0x7FFFFFFFFFFFFFFF) == 0x7FF0000000000000) {
    //         return (u.bits & 0x8000000000000000) ? init_str("-inf", 4) : init_str("inf", 3);
    //     }
    //
    //     // For now, use a buffer large enough for any double representation
    //     char buffer[64];
    //     int len = 0;
    //     const double d_pow = d_value * d_value;
    //     if (d_pow < 1e-8f || d_pow > 1e30f) {
    //         len = snprintf(buffer, sizeof(buffer), "%.15e", d_value);
    //     } else {
    //         len = snprintf(buffer, sizeof(buffer), "%.15f", d_value);
    //     }
    //
    //     // Trim trailing zeros after decimal point
    //     int last_non_zero = len - 1;
    //     while (last_non_zero > 0 && buffer[last_non_zero] == '0') {
    //         last_non_zero--;
    //     }
    //
    //     // If we ended up at the decimal point, remove it too
    //     if (buffer[last_non_zero] == '.') {
    //         last_non_zero--;
    //     }
    //
    //     return init_str(buffer, last_non_zero + 1);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *snprintf_fn = c_functions.at(SNPRINTF);

    llvm::FunctionType *f64_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getDoubleTy(context)},                        // Argument: f64 d_value
        false                                                      // No varargs
    );
    llvm::Function *f64_to_str_fn = llvm::Function::Create( //
        f64_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        prefix + "f64_to_str",                              //
        module                                              //
    );
    typecast_functions["f64_to_str"] = f64_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", f64_to_str_fn);
    llvm::BasicBlock *nan_block = llvm::BasicBlock::Create(context, "nan_case", f64_to_str_fn);
    llvm::BasicBlock *nan_merge_block = llvm::BasicBlock::Create(context, "nan_merge", f64_to_str_fn);
    llvm::BasicBlock *inf_block = llvm::BasicBlock::Create(context, "inf_case", f64_to_str_fn);
    llvm::BasicBlock *inf_merge_block = llvm::BasicBlock::Create(context, "inf_merge", f64_to_str_fn);
    llvm::BasicBlock *exponent_block = llvm::BasicBlock::Create(context, "exponent_case", f64_to_str_fn);
    llvm::BasicBlock *no_exponent_block = llvm::BasicBlock::Create(context, "no_exponent_case", f64_to_str_fn);
    llvm::BasicBlock *exponent_merge_block = llvm::BasicBlock::Create(context, "exponent_merge", f64_to_str_fn);
    llvm::BasicBlock *loop_block = llvm::BasicBlock::Create(context, "loop", f64_to_str_fn);
    llvm::BasicBlock *check_char_block = llvm::BasicBlock::Create(context, "check_char", f64_to_str_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", f64_to_str_fn);
    llvm::BasicBlock *loop_merge_block = llvm::BasicBlock::Create(context, "loop_merge", f64_to_str_fn);
    llvm::BasicBlock *decimal_case_block = llvm::BasicBlock::Create(context, "decimal_case", f64_to_str_fn);
    llvm::BasicBlock *return_block = llvm::BasicBlock::Create(context, "return", f64_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *arg_dvalue = f64_to_str_fn->arg_begin();
    arg_dvalue->setName("d_value");

    llvm::Value *is_nan_condition = builder->CreateFCmpUNE(arg_dvalue, arg_dvalue, "is_nan_cmp");
    builder->CreateCondBr(is_nan_condition, nan_block, nan_merge_block);

    // The nan_block
    {
        builder->SetInsertPoint(nan_block);
        llvm::Value *nan_str = IR::generate_const_string(module, "nan");
        llvm::Value *nan_str_value = builder->CreateCall(                          //
            init_str_fn,                                                           //
            {nan_str, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 3)}, //
            "nan_str_value"                                                        //
        );
        builder->CreateRet(nan_str_value);
    }

    // The nan_merge_block
    builder->SetInsertPoint(nan_merge_block);
    // First get all the bit values of the floating point number through bitcasting
    llvm::Value *bits = builder->CreateBitCast(arg_dvalue, builder->getInt64Ty(), "bits");

    // Create the bitmask constants
    llvm::Value *abs_mask = builder->getInt64(0x7FFFFFFFFFFFFFFF);    // Mask to clear the sign bit
    llvm::Value *inf_pattern = builder->getInt64(0x7FF0000000000000); // Bit pattern for infinity
    llvm::Value *sign_mask = builder->getInt64(0x8000000000000000);   // Mask to extract the sign bit

    // Perform bitwise operations
    llvm::Value *abs_bits = builder->CreateAnd(bits, abs_mask, "abs_bits");       // bits & 0x7FFFFFFFFFFFFFFF
    llvm::Value *is_inf = builder->CreateICmpEQ(abs_bits, inf_pattern, "is_inf"); // (bits & 0x7FFFFFFFFFFFFFFF) == 0x7FF0000000000000
    builder->CreateCondBr(is_inf, inf_block, inf_merge_block);

    // The inf_block
    {
        builder->SetInsertPoint(inf_block);
        // Check if negative infinity
        llvm::Value *sign_bit = builder->CreateAnd(bits, sign_mask, "sign_bit"); // bits & 0x8000000000000000
        llvm::Value *is_neg_inf = builder->CreateICmpNE(sign_bit, builder->getInt64(0), "is_neg_inf");

        // Create the two possible strings and select based on sign
        llvm::Value *neg_inf_str = IR::generate_const_string(module, "-inf");
        llvm::Value *pos_inf_str = IR::generate_const_string(module, "inf");

        // Create string for negative infinity
        llvm::Value *neg_inf_value = builder->CreateCall(init_str_fn, {neg_inf_str, builder->getInt64(4)}, "neg_inf_value");

        // Create string for positive infinity
        llvm::Value *pos_inf_value = builder->CreateCall(init_str_fn, {pos_inf_str, builder->getInt64(3)}, "pos_inf_value");

        // Select the appropriate string based on sign
        llvm::Value *inf_result = builder->CreateSelect(is_neg_inf, neg_inf_value, pos_inf_value, "inf_result");
        builder->CreateRet(inf_result);
    }

    // The inf_merge_block
    builder->SetInsertPoint(inf_merge_block);
    // First, create the char buffer[32]
    llvm::AllocaInst *buffer = builder->CreateAlloca(   //
        llvm::ArrayType::get(builder->getInt8Ty(), 64), // Type: [64 x i8]
        nullptr,                                        // No array size (it's fixed)
        "buffer"                                        //
    );
    buffer->setAlignment(llvm::Align(8));
    llvm::Value *buffer_ptr = builder->CreateBitCast(buffer, builder->getInt8Ty()->getPointerTo(), "buffer_ptr");
    // Create the len variable
    llvm::AllocaInst *len_var = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "len_var");
    llvm::Value *d_pow = builder->CreateFMul(arg_dvalue, arg_dvalue, "d_pow");
    llvm::Constant *min_pow = llvm::ConstantFP::get(builder->getDoubleTy(), static_cast<double>(1.0e-8f));
    llvm::Constant *max_pow = llvm::ConstantFP::get(builder->getDoubleTy(), static_cast<double>(1.0e30f));
    llvm::Value *is_too_small = builder->CreateFCmpOLT(d_pow, min_pow, "is_too_small");
    llvm::Value *is_too_large = builder->CreateFCmpOGT(d_pow, max_pow, "is_too_large");
    llvm::Value *exponent_condition = builder->CreateOr(is_too_small, is_too_large, "exponent_condition");
    builder->CreateCondBr(exponent_condition, exponent_block, no_exponent_block);

    // The exponent_block
    {
        builder->SetInsertPoint(exponent_block);
        llvm::Value *snprintf_format = IR::generate_const_string(module, "%.15e");
        llvm::CallInst *snprintf_ret = builder->CreateCall(snprintf_fn,
            {
                buffer_ptr,                                        //
                llvm::ConstantInt::get(builder->getInt64Ty(), 64), //
                snprintf_format,                                   //
                arg_dvalue                                         //
            },                                                     //
            "snprintf_ret_e"                                       //
        );
        IR::aligned_store(*builder, snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }
    // The no_exponent_block
    {
        builder->SetInsertPoint(no_exponent_block);
        llvm::Value *snprintf_format = IR::generate_const_string(module, "%.15f");
        llvm::Value *snprintf_ret = builder->CreateCall(snprintf_fn,
            {
                buffer_ptr,                                        //
                llvm::ConstantInt::get(builder->getInt64Ty(), 64), //
                snprintf_format,                                   //
                arg_dvalue                                         //
            },                                                     //
            "snprintf_ret_f"                                       //
        );
        IR::aligned_store(*builder, snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }

    // The exponent_merge_block
    builder->SetInsertPoint(exponent_merge_block);
    llvm::Value *last_non_zero = builder->CreateAlloca(builder->getInt32Ty(), 0, nullptr, "last_non_zero");
    llvm::Value *len_value = IR::aligned_load(*builder, builder->getInt32Ty(), len_var, "len_val");
    llvm::Value *len_minus_1 = builder->CreateSub(len_value, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "len_m_1");
    IR::aligned_store(*builder, len_minus_1, last_non_zero);
    builder->CreateBr(loop_block);

    // The loop_block
    builder->SetInsertPoint(loop_block);
    // Load current value of last_non_zero
    llvm::Value *last_zero_val = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "last_zero_val");
    // Check if last_non_zero > 0
    llvm::Value *is_valid_index = builder->CreateICmpSGT( //
        last_zero_val,                                    //
        llvm::ConstantInt::get(builder->getInt32Ty(), 0), //
        "is_valid_index"                                  //
    );
    builder->CreateCondBr(is_valid_index, check_char_block, loop_merge_block);

    // The check_char_block
    {
        builder->SetInsertPoint(check_char_block);
        // Get pointer to buffer[last_non_zero]
        llvm::Value *cur_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, last_zero_val, "cur_char_ptr");
        // Load the current character
        llvm::Value *cur_char = IR::aligned_load(*builder, builder->getInt8Ty(), cur_char_ptr, "cur_char");
        // Check if the current character is '0'
        llvm::Value *is_zero = builder->CreateICmpEQ(cur_char, llvm::ConstantInt::get(builder->getInt8Ty(), '0'), "is_zero");
        // Combine conditions: should continue if index > 0 && char == '0'
        llvm::Value *should_continue = builder->CreateAnd(is_valid_index, is_zero, "should_continue");
        // Branch to loop body or merge
        builder->CreateCondBr(should_continue, loop_body_block, loop_merge_block);
    }

    // The loop_body_block
    {
        builder->SetInsertPoint(loop_body_block);
        // Decrement last_non_zero
        last_zero_val = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "last_zero_val");
        llvm::Value *new_last_zero = builder->CreateSub(last_zero_val, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "new_last_zero");
        IR::aligned_store(*builder, new_last_zero, last_non_zero);
        // Jump back to loop condition
        builder->CreateBr(loop_block);
    }

    // The loop_merge_block
    {
        builder->SetInsertPoint(loop_merge_block);
        // Load final value of last_non_zero
        llvm::Value *final_last_zero = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "final_last_zero");
        // Get pointer to buffer[last_non_zero]
        llvm::Value *last_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, final_last_zero, "last_char_ptr");
        // Load the character
        llvm::Value *last_char = IR::aligned_load(*builder, builder->getInt8Ty(), last_char_ptr, "last_char");
        // Check if the character is '.'
        llvm::Value *is_dot = builder->CreateICmpEQ(last_char, llvm::ConstantInt::get(builder->getInt8Ty(), '.'), "is_dot");
        // Branch based on whether the character is a decimal point
        builder->CreateCondBr(is_dot, decimal_case_block, return_block);
    }

    // The decimal_case_block - handle case where we need to remove decimal point
    {
        builder->SetInsertPoint(decimal_case_block);
        // Decrement last_non_zero one more time
        llvm::Value *decimal_last_zero = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "decimal_last_zero");
        llvm::Value *adjusted_last_zero = builder->CreateSub( //
            decimal_last_zero,                                //
            llvm::ConstantInt::get(builder->getInt32Ty(), 1), //
            "adjusted_last_zero"                              //
        );
        IR::aligned_store(*builder, adjusted_last_zero, last_non_zero);
        // Branch to return block
        builder->CreateBr(return_block);
    }

    // The return_block
    {
        builder->SetInsertPoint(return_block);
        // Calculate final length: last_non_zero + 1
        llvm::Value *final_last_zero = IR::aligned_load(*builder, builder->getInt32Ty(), last_non_zero, "final_last_zero");
        llvm::Value *final_len = builder->CreateAdd(final_last_zero, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "final_len");
        // Convert to i64 for init_str
        llvm::Value *final_len_i64 = builder->CreateZExt(final_len, builder->getInt64Ty(), "final_len_i64");
        // Call init_str with buffer and calculated length
        llvm::Value *result = builder->CreateCall(init_str_fn, {buffer_ptr, final_len_i64}, "result");
        // Return the string
        builder->CreateRet(result);
    }
}
