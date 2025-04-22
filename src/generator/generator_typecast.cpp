#include "generator/generator.hpp"

void Generator::TypeCast::generate_helper_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    if (!only_declarations) {
        generate_count_digits_function(builder, module);
    }
    generate_bool_to_str(builder, module, only_declarations);
    generate_i32_to_str(builder, module, only_declarations);
    generate_u32_to_str(builder, module, only_declarations);
    generate_i64_to_str(builder, module, only_declarations);
    generate_u64_to_str(builder, module, only_declarations);
    generate_f32_to_str(builder, module, only_declarations);
    generate_f64_to_str(builder, module, only_declarations);
}

void Generator::TypeCast::generate_count_digits_function(llvm::IRBuilder<> *builder, llvm::Module *module) {
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
        "__flint_count_digits",                               //
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
    builder->CreateStore(n_arg, n);
    builder->CreateStore(builder->getInt64(0), count);
    builder->CreateBr(check_zero_block);

    // Check if n is 0
    builder->SetInsertPoint(check_zero_block);
    llvm::Value *n_value = builder->CreateLoad(llvm::Type::getInt64Ty(context), n, "n_val");
    llvm::Value *is_zero = builder->CreateICmpEQ(n_value, builder->getInt64(0), "is_zero");
    builder->CreateCondBr(is_zero, return_one_block, loop_block);

    // Return 1 if n is 0
    builder->SetInsertPoint(return_one_block);
    builder->CreateRet(builder->getInt64(1));

    // Loop header - check condition
    builder->SetInsertPoint(loop_block);
    llvm::Value *loop_n = builder->CreateLoad(llvm::Type::getInt64Ty(context), n, "loop_n");
    llvm::Value *loop_condition = builder->CreateICmpUGT(loop_n, builder->getInt64(0), "loop_condition");
    builder->CreateCondBr(loop_condition, loop_body_block, exit_block);

    // Loop body
    builder->SetInsertPoint(loop_body_block);
    // n = n / 10
    llvm::Value *n_val = builder->CreateLoad(llvm::Type::getInt64Ty(context), n, "n_val");
    llvm::Value *new_n = builder->CreateUDiv(n_val, builder->getInt64(10), "new_n");
    builder->CreateStore(new_n, n);

    // count++
    llvm::Value *count_val = builder->CreateLoad(llvm::Type::getInt64Ty(context), count, "count_val");
    llvm::Value *new_count = builder->CreateAdd(count_val, builder->getInt64(1), "new_count");
    builder->CreateStore(new_count, count);

    // Go back to loop header
    builder->CreateBr(loop_block);

    // Exit block - return count
    builder->SetInsertPoint(exit_block);
    llvm::Value *result = builder->CreateLoad(llvm::Type::getInt64Ty(context), count, "result");
    builder->CreateRet(result);

    // Store the function for later use
    typecast_functions["count_digits"] = count_digits_fn;
}

void Generator::TypeCast::generate_bool_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *bool_to_str(const bool b_value) {
    //     if (b_value) {
    //         return init_str("true", 4);
    //     } else {
    //         return init_str("false", 5);
    //     }
    // }
    llvm::Type *str_type = IR::get_type(Type::get_simple_type("str_var")).first;
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");

    llvm::FunctionType *bool_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return type: str*
        {llvm::Type::getInt1Ty(context)},                           // Argument: i1 b_value
        false                                                       // No varargs
    );
    llvm::Function *bool_to_str_fn = llvm::Function::Create( //
        bool_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "__flint_bool_to_str",                               //
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
    llvm::Value *true_string = IR::generate_const_string(*builder, "true");
    llvm::Value *true_str = builder->CreateCall(init_str_fn, {true_string, builder->getInt64(4)}, "true_str");
    builder->CreateRet(true_str);

    builder->SetInsertPoint(false_block);
    llvm::Value *false_string = IR::generate_const_string(*builder, "false");
    llvm::Value *false_str = builder->CreateCall(init_str_fn, {false_string, builder->getInt64(5)}, "false_str");
    builder->CreateRet(false_str);
}

/******************************************************************************************************************************************
 * @region `I32`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::i32_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    // Compare with 0
    auto zero = llvm::ConstantInt::get(int_value->getType(), 0);
    auto is_negative = builder.CreateICmpSLT(int_value, zero, "is_neg");

    // Select between 0 and the value
    return builder.CreateSelect(is_negative, zero, int_value, "safe_i32_to_u32");
}

llvm::Value *Generator::TypeCast::i32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSExt(int_value, llvm::Type::getInt64Ty(context), "sext");
}

llvm::Value *Generator::TypeCast::i32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    // Compare with 0
    auto zero32 = llvm::ConstantInt::get(int_value->getType(), 0);
    auto is_negative = builder.CreateICmpSLT(int_value, zero32, "is_neg");

    // Zero-extend the value (for positive numbers)
    auto extended = builder.CreateZExt(int_value, llvm::Type::getInt64Ty(context), "zext");
    auto zero64 = llvm::ConstantInt::get(extended->getType(), 0);

    // Select between 0 and the extended value
    return builder.CreateSelect(is_negative, zero64, extended, "safe_i32_to_u64");
}

llvm::Value *Generator::TypeCast::i32_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getFloatTy(context), "sitofp");
}

llvm::Value *Generator::TypeCast::i32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getDoubleTy(context), "sitofp");
}

void Generator::TypeCast::generate_i32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *i32_to_str(const int32_t i_value) {
    //     // Handle special case of minimum value (can't be negated safely)
    //     if (i_value == INT32_MIN) {
    //         const char *min_str = "-2147483648";
    //         return init_str(min_str, 11); // Length of "-2147483648" is 11
    //     }
    //
    //     // Handle sign
    //     int is_negative = i_value < 0;
    //     uint32_t value = is_negative ? -i_value : i_value;
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
    llvm::Type *str_type = IR::get_type(Type::get_simple_type("str_var")).first;
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *count_digits_fn = typecast_functions.at("count_digits");
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    llvm::FunctionType *i32_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getInt32Ty(context)},                         // Argument: i32 n
        false                                                      // No varargs
    );
    llvm::Function *i32_to_str_fn = llvm::Function::Create( //
        i32_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_i32_to_str",                               //
        module                                              //
    );
    typecast_functions["i32_to_str"] = i32_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", i32_to_str_fn);
    llvm::BasicBlock *min_value_block = llvm::BasicBlock::Create(context, "min_value", i32_to_str_fn);
    llvm::BasicBlock *regular_case_block = llvm::BasicBlock::Create(context, "regular_case", i32_to_str_fn);
    llvm::BasicBlock *digit_loop_block = llvm::BasicBlock::Create(context, "digit_loop", i32_to_str_fn);
    llvm::BasicBlock *negative_sign_block = llvm::BasicBlock::Create(context, "negative_sign", i32_to_str_fn);
    llvm::BasicBlock *add_sign_block = llvm::BasicBlock::Create(context, "add_sign", i32_to_str_fn);
    llvm::BasicBlock *return_block = llvm::BasicBlock::Create(context, "return", i32_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *arg_ivalue = i32_to_str_fn->arg_begin();
    arg_ivalue->setName("i_value");

    // Create constant for INT32_MIN
    llvm::Value *int32_min = llvm::ConstantInt::get(builder->getInt32Ty(), INT32_MIN);

    // Check if i_value == INT32_MIN
    llvm::Value *is_min_value = builder->CreateICmpEQ(arg_ivalue, int32_min, "is_min_value");
    builder->CreateCondBr(is_min_value, min_value_block, regular_case_block);

    // Handle INT32_MIN special case
    builder->SetInsertPoint(min_value_block);
    llvm::Value *min_str_ptr = builder->CreateGlobalStringPtr("-2147483648", "min_str");
    llvm::Value *min_result = builder->CreateCall(init_str_fn, {min_str_ptr, builder->getInt64(11)}, "min_result");
    builder->CreateRet(min_result);

    // Regular case - handle sign
    builder->SetInsertPoint(regular_case_block);
    llvm::Value *is_negative = builder->CreateICmpSLT(arg_ivalue, builder->getInt32(0), "is_negative");
    llvm::Value *abs_value = builder->CreateSelect(is_negative, builder->CreateNeg(arg_ivalue, "negated"), arg_ivalue, "abs_value");

    // Convert to uint32_t for consistent handling
    llvm::Value *value = builder->CreateZExt(abs_value, builder->getInt64Ty(), "value_u64");

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
    builder->CreateStore(value, current_value_ptr);
    builder->CreateStore(buffer_end, current_buffer_ptr);

    // Start the digit loop
    builder->CreateBr(digit_loop_block);

    // Digit loop
    builder->SetInsertPoint(digit_loop_block);

    // Load current value
    llvm::Value *current_value = builder->CreateLoad(builder->getInt64Ty(), current_value_ptr, "current_value");

    // Calculate digit: value % 10
    llvm::Value *remainder = builder->CreateURem(current_value, builder->getInt64(10), "remainder");
    llvm::Value *digit_char =
        builder->CreateAdd(builder->getInt8('0'), builder->CreateTrunc(remainder, builder->getInt8Ty(), "digit"), "digit_char");

    // Decrement buffer pointer
    llvm::Value *buffer_ptr = builder->CreateLoad(builder->getPtrTy(), current_buffer_ptr, "buffer_ptr");
    llvm::Value *prev_buffer = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, builder->getInt32(-1), "prev_buffer");
    builder->CreateStore(prev_buffer, current_buffer_ptr);

    // Store digit at the decremented position
    builder->CreateStore(digit_char, prev_buffer);

    // Divide value by 10
    llvm::Value *next_value = builder->CreateUDiv(current_value, builder->getInt64(10), "next_value");
    builder->CreateStore(next_value, current_value_ptr);

    // Continue loop if value > 0
    llvm::Value *continue_loop = builder->CreateICmpUGT(next_value, builder->getInt64(0), "continue_loop");
    builder->CreateCondBr(continue_loop, digit_loop_block, negative_sign_block);

    // Add negative sign if needed
    builder->SetInsertPoint(negative_sign_block);
    llvm::Value *should_add_sign = builder->CreateICmpEQ(sign_len, builder->getInt64(1), "should_add_sign");
    builder->CreateCondBr(should_add_sign, add_sign_block, return_block);

    // Add negative sign
    builder->SetInsertPoint(add_sign_block);
    llvm::Value *sign_buffer_ptr = builder->CreateLoad(builder->getPtrTy(), current_buffer_ptr, "sign_buffer_ptr");
    llvm::Value *sign_prev_buffer = builder->CreateGEP(builder->getInt8Ty(), sign_buffer_ptr, builder->getInt32(-1), "sign_prev_buffer");
    builder->CreateStore(builder->getInt8('-'), sign_prev_buffer);
    builder->CreateBr(return_block);

    // Return the result of the function
    builder->SetInsertPoint(return_block);
    builder->CreateRet(result);
}

/******************************************************************************************************************************************
 * @region `U32`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::u32_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto int_max = llvm::ConstantInt::get(int_value->getType(), llvm::APInt::getSignedMaxValue(32));
    auto too_large = builder.CreateICmpUGT(int_value, int_max);
    return builder.CreateSelect(too_large, int_max, int_value);
}

llvm::Value *Generator::TypeCast::u32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateZExt(int_value, llvm::Type::getInt64Ty(context), "zext");
}

llvm::Value *Generator::TypeCast::u32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateZExt(int_value, llvm::Type::getInt64Ty(context), "zext");
}

llvm::Value *Generator::TypeCast::u32_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getFloatTy(context), "uitofp");
}

llvm::Value *Generator::TypeCast::u32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getDoubleTy(context), "uitofp");
}

void Generator::TypeCast::generate_u32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *u32_to_str(const uint32_t u_value) {
    //     // Count digits
    //     uint64_t u_value_64 = (uint64_t)u_value;
    //     size_t len = count_digits(u_value_64);
    //
    //     // Allocate string
    //     str *result = create_str(len);
    //     char *buffer = result->value + len; // Start at the end
    //
    //     // Handle special case for 0
    //     if (u_value == 0) {
    //         result->value[0] = '0';
    //         return result;
    //     }
    //
    //     // Write digits in reverse order
    //     uint32_t value = u_value;
    //     do {
    //         *--buffer = '0' + (value % 10);
    //         value /= 10;
    //     } while (value > 0);
    //
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type(Type::get_simple_type("str_var")).first;
    llvm::Function *count_digits_fn = typecast_functions.at("count_digits");
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    llvm::FunctionType *u32_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getInt32Ty(context)},                         // Argument: u32 n
        false                                                      // No varargs
    );
    llvm::Function *u32_to_str_fn = llvm::Function::Create( //
        u32_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_u32_to_str",                               //
        module                                              //
    );
    typecast_functions["u32_to_str"] = u32_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", u32_to_str_fn);
    llvm::BasicBlock *zero_case_block = llvm::BasicBlock::Create(context, "zero_case", u32_to_str_fn);
    llvm::BasicBlock *nonzero_case_block = llvm::BasicBlock::Create(context, "nonzero_case", u32_to_str_fn);
    llvm::BasicBlock *loop_block = llvm::BasicBlock::Create(context, "loop", u32_to_str_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", u32_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *arg_uvalue = u32_to_str_fn->arg_begin();
    arg_uvalue->setName("u_value");

    // Count digits - call count_digits(u_value)
    llvm::Value *u64_value = TypeCast::u32_to_u64(*builder, arg_uvalue);
    llvm::Value *len = builder->CreateCall(count_digits_fn, {u64_value}, "len");

    // Allocate string - call create_str(len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {len}, "result");

    // Check if u_value == 0
    llvm::Value *is_zero = builder->CreateICmpEQ(arg_uvalue, llvm::ConstantInt::get(builder->getInt32Ty(), 0), "is_zero");
    builder->CreateCondBr(is_zero, zero_case_block, nonzero_case_block);

    // Zero case block
    builder->SetInsertPoint(zero_case_block);
    // Get pointer to result->value (the flexible array member at index 1)
    llvm::Value *data_ptr_zero = builder->CreateStructGEP(str_type, result, 1, "data_ptr_zero");
    // Store '0' character
    builder->CreateStore(llvm::ConstantInt::get(builder->getInt8Ty(), '0'), data_ptr_zero);
    builder->CreateBr(exit_block);

    // Non-zero case block
    builder->SetInsertPoint(nonzero_case_block);
    // Get pointer to result->value (the flexible array member)
    llvm::Value *data_ptr = builder->CreateStructGEP(str_type, result, 1, "data_ptr");

    // Calculate buffer = result->value + len
    llvm::Value *buffer = builder->CreateGEP(builder->getInt8Ty(), data_ptr, len, "buffer");

    // Create local variable for value
    llvm::Value *current_value = builder->CreateAlloca(builder->getInt32Ty(), nullptr, "current_value");
    builder->CreateStore(arg_uvalue, current_value);

    // Create local variable for buffer
    llvm::Value *current_buffer = builder->CreateAlloca(builder->getPtrTy(), nullptr, "current_buffer");
    builder->CreateStore(buffer, current_buffer);

    // Branch to the loop
    builder->CreateBr(loop_block);

    // Loop block
    builder->SetInsertPoint(loop_block);
    // Load current value and buffer
    llvm::Value *value_load = builder->CreateLoad(builder->getInt32Ty(), current_value, "value_load");
    llvm::Value *buffer_load = builder->CreateLoad(builder->getPtrTy(), current_buffer, "buffer_load");

    // Calculate value % 10
    llvm::Value *remainder = builder->CreateURem(value_load, llvm::ConstantInt::get(builder->getInt32Ty(), 10), "remainder");

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
    builder->CreateStore(digit_char, buffer_prev);

    // Update buffer
    builder->CreateStore(buffer_prev, current_buffer);

    // Calculate value / 10
    llvm::Value *new_value = builder->CreateUDiv(value_load, llvm::ConstantInt::get(builder->getInt32Ty(), 10), "new_value");

    // Update value
    builder->CreateStore(new_value, current_value);

    // Check if value > 0
    llvm::Value *continue_loop = builder->CreateICmpUGT(new_value, llvm::ConstantInt::get(builder->getInt32Ty(), 0), "continue_loop");

    // Branch based on condition
    builder->CreateCondBr(continue_loop, loop_block, exit_block);

    // Exit block
    builder->SetInsertPoint(exit_block);
    builder->CreateRet(result);
}

/******************************************************************************************************************************************
 * @region `I64`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::i64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateTrunc(int_value, llvm::Type::getInt32Ty(context), "trunc");
}

llvm::Value *Generator::TypeCast::i64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto zero = llvm::ConstantInt::get(int_value->getType(), 0);
    auto uint32_max_64 = llvm::ConstantInt::get(int_value->getType(), 0xFFFFFFFFULL);

    // Clamp to 0 if negative
    auto is_negative = builder.CreateICmpSLT(int_value, zero);
    auto clamped_negative = builder.CreateSelect(is_negative, zero, int_value);

    // Clamp to UINT32_MAX if too large
    auto is_too_large = builder.CreateICmpSGT(clamped_negative, uint32_max_64);
    auto clamped = builder.CreateSelect(is_too_large, uint32_max_64, clamped_negative);

    // Finally truncate to 32 bits
    return builder.CreateTrunc(clamped, llvm::Type::getInt32Ty(context));
}

llvm::Value *Generator::TypeCast::i64_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto zero = llvm::ConstantInt::get(int_value->getType(), 0);
    auto is_negative = builder.CreateICmpSLT(int_value, zero);
    return builder.CreateSelect(is_negative, zero, int_value);
}

llvm::Value *Generator::TypeCast::i64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getFloatTy(context), "sitofp");
}

llvm::Value *Generator::TypeCast::i64_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getDoubleTy(context), "sitofp");
}

void Generator::TypeCast::generate_i64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *i64_to_str(const int64_t i_value) {
    //     // Handle special case of minimum value
    //     if (i_value == INT64_MIN) {
    //         const char *min_str = "-9223372036854775808";
    //         return init_str(min_str, 20); // Length of minimum int64 as string
    //     }
    //
    //     // Handle sign
    //     int is_negative = i_value < 0;
    //     uint64_t value = is_negative ? -i_value : i_value;
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
    llvm::Type *str_type = IR::get_type(Type::get_simple_type("str_var")).first;
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *count_digits_fn = typecast_functions.at("count_digits");
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    llvm::FunctionType *i64_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getInt64Ty(context)},                         // Argument: i64 i_value
        false                                                      // No varargs
    );
    llvm::Function *i64_to_str_fn = llvm::Function::Create( //
        i64_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_i64_to_str",                               //
        module                                              //
    );
    typecast_functions["i64_to_str"] = i64_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", i64_to_str_fn);
    llvm::BasicBlock *min_value_block = llvm::BasicBlock::Create(context, "min_value", i64_to_str_fn);
    llvm::BasicBlock *regular_case_block = llvm::BasicBlock::Create(context, "regular_case", i64_to_str_fn);
    llvm::BasicBlock *digit_loop_block = llvm::BasicBlock::Create(context, "digit_loop", i64_to_str_fn);
    llvm::BasicBlock *negative_sign_block = llvm::BasicBlock::Create(context, "negative_sign", i64_to_str_fn);
    llvm::BasicBlock *add_sign_block = llvm::BasicBlock::Create(context, "add_sign", i64_to_str_fn);
    llvm::BasicBlock *return_block = llvm::BasicBlock::Create(context, "return", i64_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *arg_ivalue = i64_to_str_fn->arg_begin();
    arg_ivalue->setName("i_value");

    // Create constant for INT64_MIN
    llvm::Value *int64_min = llvm::ConstantInt::get(builder->getInt64Ty(), INT64_MIN);

    // Check if i_value == INT64_MIN
    llvm::Value *is_min_value = builder->CreateICmpEQ(arg_ivalue, int64_min, "is_min_value");
    builder->CreateCondBr(is_min_value, min_value_block, regular_case_block);

    // Handle INT32_MIN special case
    builder->SetInsertPoint(min_value_block);
    llvm::Value *min_str_ptr = builder->CreateGlobalStringPtr("-9223372036854775808", "min_str");
    llvm::Value *min_result = builder->CreateCall(init_str_fn, {min_str_ptr, builder->getInt64(21)}, "min_result");
    builder->CreateRet(min_result);

    // Regular case - handle sign
    builder->SetInsertPoint(regular_case_block);
    llvm::Value *is_negative = builder->CreateICmpSLT(arg_ivalue, builder->getInt64(0), "is_negative");
    llvm::Value *abs_value = builder->CreateSelect(is_negative, builder->CreateNeg(arg_ivalue, "negated"), arg_ivalue, "abs_value");

    // Convert to uint64_t for consistent handling
    llvm::Value *value = builder->CreateZExt(abs_value, builder->getInt64Ty(), "value_u64");

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
    builder->CreateStore(value, current_value_ptr);
    builder->CreateStore(buffer_end, current_buffer_ptr);

    // Start the digit loop
    builder->CreateBr(digit_loop_block);

    // Digit loop
    builder->SetInsertPoint(digit_loop_block);

    // Load current value
    llvm::Value *current_value = builder->CreateLoad(builder->getInt64Ty(), current_value_ptr, "current_value");

    // Calculate digit: value % 10
    llvm::Value *remainder = builder->CreateURem(current_value, builder->getInt64(10), "remainder");
    llvm::Value *digit_char = builder->CreateAdd(                       //
        builder->getInt8('0'),                                          //
        builder->CreateTrunc(remainder, builder->getInt8Ty(), "digit"), //
        "digit_char"                                                    //
    );

    // Decrement buffer pointer
    llvm::Value *buffer_ptr = builder->CreateLoad(builder->getPtrTy(), current_buffer_ptr, "buffer_ptr");
    llvm::Value *prev_buffer = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, builder->getInt64(-1), "prev_buffer");
    builder->CreateStore(prev_buffer, current_buffer_ptr);

    // Store digit at the decremented position
    builder->CreateStore(digit_char, prev_buffer);

    // Divide value by 10
    llvm::Value *next_value = builder->CreateUDiv(current_value, builder->getInt64(10), "next_value");
    builder->CreateStore(next_value, current_value_ptr);

    // Continue loop if value > 0
    llvm::Value *continue_loop = builder->CreateICmpUGT(next_value, builder->getInt64(0), "continue_loop");
    builder->CreateCondBr(continue_loop, digit_loop_block, negative_sign_block);

    // Add negative sign if needed
    builder->SetInsertPoint(negative_sign_block);
    llvm::Value *should_add_sign = builder->CreateICmpEQ(sign_len, builder->getInt64(1), "should_add_sign");
    builder->CreateCondBr(should_add_sign, add_sign_block, return_block);

    // Add negative sign
    builder->SetInsertPoint(add_sign_block);
    llvm::Value *sign_buffer_ptr = builder->CreateLoad(builder->getPtrTy(), current_buffer_ptr, "sign_buffer_ptr");
    llvm::Value *sign_prev_buffer = builder->CreateGEP(builder->getInt8Ty(), sign_buffer_ptr, builder->getInt64(-1), "sign_prev_buffer");
    builder->CreateStore(builder->getInt8('-'), sign_prev_buffer);
    builder->CreateBr(return_block);

    // Return the result of the function
    builder->SetInsertPoint(return_block);
    builder->CreateRet(result);
}

/******************************************************************************************************************************************
 * @region `U64`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::u64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto int32_max_64 = llvm::ConstantInt::get(int_value->getType(), 0x7FFFFFFF);
    auto too_large = builder.CreateICmpUGT(int_value, int32_max_64);
    auto clamped = builder.CreateSelect(too_large, int32_max_64, int_value);
    return builder.CreateTrunc(clamped, llvm::Type::getInt32Ty(context));
}

llvm::Value *Generator::TypeCast::u64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto uint32_max_64 = llvm::ConstantInt::get(int_value->getType(), 0xFFFFFFFFULL);
    auto too_large = builder.CreateICmpUGT(int_value, uint32_max_64);
    auto clamped = builder.CreateSelect(too_large, uint32_max_64, int_value);
    return builder.CreateTrunc(clamped, llvm::Type::getInt32Ty(context));
}

llvm::Value *Generator::TypeCast::u64_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto int64_max = llvm::ConstantInt::get(int_value->getType(), llvm::APInt::getSignedMaxValue(64));
    auto too_large = builder.CreateICmpUGT(int_value, int64_max);
    return builder.CreateSelect(too_large, int64_max, int_value);
}

llvm::Value *Generator::TypeCast::u64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getFloatTy(context), "uitofp");
}

llvm::Value *Generator::TypeCast::u64_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getDoubleTy(context), "uitofp");
}

void Generator::TypeCast::generate_u64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // C IMPLEMENTATION:
    // str *u64_to_str(const uint64_t u_value) {
    //     // Count digits
    //     size_t len = count_digits(u_value);
    //
    //     // Allocate string
    //     str *result = create_str(len);
    //     char *buffer = result->value + len; // Start at the end
    //
    //     // Handle special case for 0
    //     if (u_value == 0) {
    //         result->value[0] = '0';
    //         return result;
    //     }
    //
    //     // Write digits in reverse order
    //     uint64_t value = u_value;
    //     do {
    //         *--buffer = '0' + (value % 10);
    //         value /= 10;
    //     } while (value > 0);
    //
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type(Type::get_simple_type("str_var")).first;
    llvm::Function *count_digits_fn = typecast_functions.at("count_digits");
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    llvm::FunctionType *u64_to_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getInt64Ty(context)},                         // Argument: u64 u_value
        false                                                      // No varargs
    );
    llvm::Function *u64_to_str_fn = llvm::Function::Create( //
        u64_to_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_u64_to_str",                               //
        module                                              //
    );
    typecast_functions["u64_to_str"] = u64_to_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", u64_to_str_fn);
    llvm::BasicBlock *zero_case_block = llvm::BasicBlock::Create(context, "zero_case", u64_to_str_fn);
    llvm::BasicBlock *nonzero_case_block = llvm::BasicBlock::Create(context, "nonzero_case", u64_to_str_fn);
    llvm::BasicBlock *loop_block = llvm::BasicBlock::Create(context, "loop", u64_to_str_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", u64_to_str_fn);

    // Set insert point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the function parameter
    llvm::Argument *arg_uvalue = u64_to_str_fn->arg_begin();
    arg_uvalue->setName("u_value");

    // Count digits - call count_digits(u_value)
    llvm::Value *len = builder->CreateCall(count_digits_fn, {arg_uvalue}, "len");

    // Allocate string - call create_str(len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {len}, "result");

    // Check if u_value == 0
    llvm::Value *is_zero = builder->CreateICmpEQ(arg_uvalue, llvm::ConstantInt::get(builder->getInt64Ty(), 0), "is_zero");
    builder->CreateCondBr(is_zero, zero_case_block, nonzero_case_block);

    // Zero case block
    builder->SetInsertPoint(zero_case_block);
    // Get pointer to result->value (the flexible array member at index 1)
    llvm::Value *data_ptr_zero = builder->CreateStructGEP(str_type, result, 1, "data_ptr_zero");
    // Store '0' character
    builder->CreateStore(llvm::ConstantInt::get(builder->getInt8Ty(), '0'), data_ptr_zero);
    builder->CreateBr(exit_block);

    // Non-zero case block
    builder->SetInsertPoint(nonzero_case_block);
    // Get pointer to result->value (the flexible array member)
    llvm::Value *data_ptr = builder->CreateStructGEP(str_type, result, 1, "data_ptr");

    // Calculate buffer = result->value + len
    llvm::Value *buffer = builder->CreateGEP(builder->getInt8Ty(), data_ptr, len, "buffer");

    // Create local variable for value
    llvm::Value *current_value = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "current_value");
    builder->CreateStore(arg_uvalue, current_value);

    // Create local variable for buffer
    llvm::Value *current_buffer = builder->CreateAlloca(builder->getPtrTy(), nullptr, "current_buffer");
    builder->CreateStore(buffer, current_buffer);

    // Branch to the loop
    builder->CreateBr(loop_block);

    // Loop block
    builder->SetInsertPoint(loop_block);
    // Load current value and buffer
    llvm::Value *value_load = builder->CreateLoad(builder->getInt64Ty(), current_value, "value_load");
    llvm::Value *buffer_load = builder->CreateLoad(builder->getPtrTy(), current_buffer, "buffer_load");

    // Calculate value % 10
    llvm::Value *remainder = builder->CreateURem(value_load, llvm::ConstantInt::get(builder->getInt64Ty(), 10), "remainder");

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
        llvm::ConstantInt::getSigned(builder->getInt64Ty(), -1), //
        "buffer_prev"                                            //
    );

    // Store the digit character
    builder->CreateStore(digit_char, buffer_prev);

    // Update buffer
    builder->CreateStore(buffer_prev, current_buffer);

    // Calculate value / 10
    llvm::Value *new_value = builder->CreateUDiv(value_load, llvm::ConstantInt::get(builder->getInt64Ty(), 10), "new_value");

    // Update value
    builder->CreateStore(new_value, current_value);

    // Check if value > 0
    llvm::Value *continue_loop = builder->CreateICmpUGT(new_value, llvm::ConstantInt::get(builder->getInt64Ty(), 0), "continue_loop");

    // Branch based on condition
    builder->CreateCondBr(continue_loop, loop_block, exit_block);

    // Exit block
    builder->SetInsertPoint(exit_block);
    builder->CreateRet(result);
}

/******************************************************************************************************************************************
 * @region `F32`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::f32_to_i32(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToSI(float_value, llvm::Type::getInt32Ty(context), "fptosi");
}

llvm::Value *Generator::TypeCast::f32_to_u32(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToUI(float_value, llvm::Type::getInt32Ty(context), "fptoui");
}

llvm::Value *Generator::TypeCast::f32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToSI(float_value, llvm::Type::getInt64Ty(context), "fptosi");
}

llvm::Value *Generator::TypeCast::f32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToUI(float_value, llvm::Type::getInt64Ty(context), "fptoui");
}

llvm::Value *Generator::TypeCast::f32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPExt(float_value, llvm::Type::getDoubleTy(context), "fpext");
}

void Generator::TypeCast::generate_f32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
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
    llvm::Type *str_type = IR::get_type(Type::get_simple_type("str_var")).first;
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
        "__flint_f32_to_str",                               //
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
        llvm::Value *nan_str = IR::generate_const_string(*builder, "nan");
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
        llvm::Value *neg_inf_str = IR::generate_const_string(*builder, "-inf");
        llvm::Value *pos_inf_str = IR::generate_const_string(*builder, "inf");

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
        llvm::Value *snprintf_format = IR::generate_const_string(*builder, "%.6e");
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
        builder->CreateStore(snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }
    // The no_exponent_block
    {
        builder->SetInsertPoint(no_exponent_block);
        llvm::Value *snprintf_format = IR::generate_const_string(*builder, "%.6f");
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
        builder->CreateStore(snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }

    // The exponent_merge_block
    builder->SetInsertPoint(exponent_merge_block);
    llvm::Value *last_non_zero = builder->CreateAlloca(builder->getInt32Ty(), 0, nullptr, "last_non_zero");
    llvm::Value *len_value = builder->CreateLoad(builder->getInt32Ty(), len_var, "len_val");
    llvm::Value *len_minus_1 = builder->CreateSub(len_value, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "len_m_1");
    builder->CreateStore(len_minus_1, last_non_zero);
    builder->CreateBr(loop_block);

    // The loop_block
    builder->SetInsertPoint(loop_block);
    // Load current value of last_non_zero
    llvm::Value *last_zero_val = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "last_zero_val");
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
        llvm::Value *cur_char = builder->CreateLoad(builder->getInt8Ty(), cur_char_ptr, "cur_char");
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
        last_zero_val = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "last_zero_val");
        llvm::Value *new_last_zero = builder->CreateSub(last_zero_val, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "new_last_zero");
        builder->CreateStore(new_last_zero, last_non_zero);
        // Jump back to loop condition
        builder->CreateBr(loop_block);
    }

    // The loop_merge_block
    {
        builder->SetInsertPoint(loop_merge_block);
        // Load final value of last_non_zero
        llvm::Value *final_last_zero = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "final_last_zero");
        // Get pointer to buffer[last_non_zero]
        llvm::Value *last_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, final_last_zero, "last_char_ptr");
        // Load the character
        llvm::Value *last_char = builder->CreateLoad(builder->getInt8Ty(), last_char_ptr, "last_char");
        // Check if the character is '.'
        llvm::Value *is_dot = builder->CreateICmpEQ(last_char, llvm::ConstantInt::get(builder->getInt8Ty(), '.'), "is_dot");
        // Branch based on whether the character is a decimal point
        builder->CreateCondBr(is_dot, decimal_case_block, return_block);
    }

    // The decimal_case_block - handle case where we need to remove decimal point
    {
        builder->SetInsertPoint(decimal_case_block);
        // Decrement last_non_zero one more time
        llvm::Value *decimal_last_zero = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "decimal_last_zero");
        llvm::Value *adjusted_last_zero = builder->CreateSub( //
            decimal_last_zero,                                //
            llvm::ConstantInt::get(builder->getInt32Ty(), 1), //
            "adjusted_last_zero"                              //
        );
        builder->CreateStore(adjusted_last_zero, last_non_zero);
        // Branch to return block
        builder->CreateBr(return_block);
    }

    // The return_block
    {
        builder->SetInsertPoint(return_block);
        // Calculate final length: last_non_zero + 1
        llvm::Value *final_last_zero = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "final_last_zero");
        llvm::Value *final_len = builder->CreateAdd(final_last_zero, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "final_len");
        // Convert to i64 for init_str
        llvm::Value *final_len_i64 = builder->CreateZExt(final_len, builder->getInt64Ty(), "final_len_i64");
        // Call init_str with buffer and calculated length
        llvm::Value *result = builder->CreateCall(init_str_fn, {buffer_ptr, final_len_i64}, "result");
        // Return the string
        builder->CreateRet(result);
    }
}

/******************************************************************************************************************************************
 * @region `F64`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::f64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToSI(double_value, llvm::Type::getInt32Ty(context), "fptosi");
}

llvm::Value *Generator::TypeCast::f64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToUI(double_value, llvm::Type::getInt32Ty(context), "fptoui");
}

llvm::Value *Generator::TypeCast::f64_to_i64(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToSI(double_value, llvm::Type::getInt64Ty(context), "fptosi");
}

llvm::Value *Generator::TypeCast::f64_to_u64(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToUI(double_value, llvm::Type::getInt64Ty(context), "fptoui");
}

llvm::Value *Generator::TypeCast::f64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPTrunc(double_value, llvm::Type::getFloatTy(context), "fptrunc");
}

void Generator::TypeCast::generate_f64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
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
    llvm::Type *str_type = IR::get_type(Type::get_simple_type("str_var")).first;
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
        "__flint_f64_to_str",                               //
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
        llvm::Value *nan_str = IR::generate_const_string(*builder, "nan");
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
        llvm::Value *neg_inf_str = IR::generate_const_string(*builder, "-inf");
        llvm::Value *pos_inf_str = IR::generate_const_string(*builder, "inf");

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
        llvm::Value *snprintf_format = IR::generate_const_string(*builder, "%.15e");
        llvm::CallInst *snprintf_ret = builder->CreateCall(snprintf_fn,
            {
                buffer_ptr,                                        //
                llvm::ConstantInt::get(builder->getInt64Ty(), 64), //
                snprintf_format,                                   //
                arg_dvalue                                         //
            },                                                     //
            "snprintf_ret_e"                                       //
        );
        builder->CreateStore(snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }
    // The no_exponent_block
    {
        builder->SetInsertPoint(no_exponent_block);
        llvm::Value *snprintf_format = IR::generate_const_string(*builder, "%.15f");
        llvm::Value *snprintf_ret = builder->CreateCall(snprintf_fn,
            {
                buffer_ptr,                                        //
                llvm::ConstantInt::get(builder->getInt64Ty(), 64), //
                snprintf_format,                                   //
                arg_dvalue                                         //
            },                                                     //
            "snprintf_ret_f"                                       //
        );
        builder->CreateStore(snprintf_ret, len_var);
        builder->CreateBr(exponent_merge_block);
    }

    // The exponent_merge_block
    builder->SetInsertPoint(exponent_merge_block);
    llvm::Value *last_non_zero = builder->CreateAlloca(builder->getInt32Ty(), 0, nullptr, "last_non_zero");
    llvm::Value *len_value = builder->CreateLoad(builder->getInt32Ty(), len_var, "len_val");
    llvm::Value *len_minus_1 = builder->CreateSub(len_value, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "len_m_1");
    builder->CreateStore(len_minus_1, last_non_zero);
    builder->CreateBr(loop_block);

    // The loop_block
    builder->SetInsertPoint(loop_block);
    // Load current value of last_non_zero
    llvm::Value *last_zero_val = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "last_zero_val");
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
        llvm::Value *cur_char = builder->CreateLoad(builder->getInt8Ty(), cur_char_ptr, "cur_char");
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
        last_zero_val = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "last_zero_val");
        llvm::Value *new_last_zero = builder->CreateSub(last_zero_val, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "new_last_zero");
        builder->CreateStore(new_last_zero, last_non_zero);
        // Jump back to loop condition
        builder->CreateBr(loop_block);
    }

    // The loop_merge_block
    {
        builder->SetInsertPoint(loop_merge_block);
        // Load final value of last_non_zero
        llvm::Value *final_last_zero = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "final_last_zero");
        // Get pointer to buffer[last_non_zero]
        llvm::Value *last_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer_ptr, final_last_zero, "last_char_ptr");
        // Load the character
        llvm::Value *last_char = builder->CreateLoad(builder->getInt8Ty(), last_char_ptr, "last_char");
        // Check if the character is '.'
        llvm::Value *is_dot = builder->CreateICmpEQ(last_char, llvm::ConstantInt::get(builder->getInt8Ty(), '.'), "is_dot");
        // Branch based on whether the character is a decimal point
        builder->CreateCondBr(is_dot, decimal_case_block, return_block);
    }

    // The decimal_case_block - handle case where we need to remove decimal point
    {
        builder->SetInsertPoint(decimal_case_block);
        // Decrement last_non_zero one more time
        llvm::Value *decimal_last_zero = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "decimal_last_zero");
        llvm::Value *adjusted_last_zero = builder->CreateSub( //
            decimal_last_zero,                                //
            llvm::ConstantInt::get(builder->getInt32Ty(), 1), //
            "adjusted_last_zero"                              //
        );
        builder->CreateStore(adjusted_last_zero, last_non_zero);
        // Branch to return block
        builder->CreateBr(return_block);
    }

    // The return_block
    {
        builder->SetInsertPoint(return_block);
        // Calculate final length: last_non_zero + 1
        llvm::Value *final_last_zero = builder->CreateLoad(builder->getInt32Ty(), last_non_zero, "final_last_zero");
        llvm::Value *final_len = builder->CreateAdd(final_last_zero, llvm::ConstantInt::get(builder->getInt32Ty(), 1), "final_len");
        // Convert to i64 for init_str
        llvm::Value *final_len_i64 = builder->CreateZExt(final_len, builder->getInt64Ty(), "final_len_i64");
        // Call init_str with buffer and calculated length
        llvm::Value *result = builder->CreateCall(init_str_fn, {buffer_ptr, final_len_i64}, "result");
        // Return the string
        builder->CreateRet(result);
    }
}
