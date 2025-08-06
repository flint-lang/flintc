#include "generator/generator.hpp"
#include "globals.hpp"

void Generator::Module::Arithmetic::generate_arithmetic_functions( //
    llvm::IRBuilder<> *builder,                                    //
    llvm::Module *module,                                          //
    const bool only_declarations                                   //
) {
    llvm::IntegerType *i8_type = llvm::Type::getInt8Ty(context);
    llvm::IntegerType *i32_type = llvm::Type::getInt32Ty(context);
    llvm::IntegerType *i64_type = llvm::Type::getInt64Ty(context);
    // Generate no arithmetic functions for the unsafe mode, as they are never called in unsafe mode annyway
    if (overflow_mode != ArithmeticOverflowMode::UNSAFE) {
        // i32 functions
        generate_int_safe_add(builder, module, only_declarations, i32_type, "i32");
        generate_int_safe_sub(builder, module, only_declarations, i32_type, "i32");
        generate_int_safe_mul(builder, module, only_declarations, i32_type, "i32");
        generate_int_safe_div(builder, module, only_declarations, i32_type, "i32");
        generate_int_safe_mod(builder, module, only_declarations, i32_type, "i32", true);
        // i64 functions
        generate_int_safe_add(builder, module, only_declarations, i64_type, "i64");
        generate_int_safe_sub(builder, module, only_declarations, i64_type, "i64");
        generate_int_safe_mul(builder, module, only_declarations, i64_type, "i64");
        generate_int_safe_div(builder, module, only_declarations, i64_type, "i64");
        generate_int_safe_mod(builder, module, only_declarations, i64_type, "i64", true);
        // u8 functions
        generate_uint_safe_add(builder, module, only_declarations, i8_type, "u8");
        generate_uint_safe_sub(builder, module, only_declarations, i8_type, "u8");
        generate_uint_safe_mul(builder, module, only_declarations, i8_type, "u8");
        generate_uint_safe_div(builder, module, only_declarations, i8_type, "u8");
        generate_int_safe_mod(builder, module, only_declarations, i8_type, "u8", false);
        // u32 functions
        generate_uint_safe_add(builder, module, only_declarations, i32_type, "u32");
        generate_uint_safe_sub(builder, module, only_declarations, i32_type, "u32");
        generate_uint_safe_mul(builder, module, only_declarations, i32_type, "u32");
        generate_uint_safe_div(builder, module, only_declarations, i32_type, "u32");
        generate_int_safe_mod(builder, module, only_declarations, i32_type, "u32", false);
        // u64 functions
        generate_uint_safe_add(builder, module, only_declarations, i64_type, "u64");
        generate_uint_safe_sub(builder, module, only_declarations, i64_type, "u64");
        generate_uint_safe_mul(builder, module, only_declarations, i64_type, "u64");
        generate_uint_safe_div(builder, module, only_declarations, i64_type, "u64");
        generate_int_safe_mod(builder, module, only_declarations, i64_type, "u64", false);

        // i32x2 functions
        generate_int_vector_safe_add(builder, module, only_declarations, llvm::VectorType::get(i32_type, 2, false), 2, "i32x2");
        generate_int_vector_safe_sub(builder, module, only_declarations, llvm::VectorType::get(i32_type, 2, false), 2, "i32x2");
        generate_int_vector_safe_mul(builder, module, only_declarations, llvm::VectorType::get(i32_type, 2, false), 2, "i32x2");
        generate_int_vector_safe_div(builder, module, only_declarations, llvm::VectorType::get(i32_type, 2, false), 2, "i32x2");
        // i32x3 functions
        generate_int_vector_safe_add(builder, module, only_declarations, llvm::VectorType::get(i32_type, 3, false), 3, "i32x3");
        generate_int_vector_safe_sub(builder, module, only_declarations, llvm::VectorType::get(i32_type, 3, false), 3, "i32x3");
        generate_int_vector_safe_mul(builder, module, only_declarations, llvm::VectorType::get(i32_type, 3, false), 3, "i32x3");
        generate_int_vector_safe_div(builder, module, only_declarations, llvm::VectorType::get(i32_type, 3, false), 3, "i32x3");
        // i32x4 functions
        generate_int_vector_safe_add(builder, module, only_declarations, llvm::VectorType::get(i32_type, 4, false), 4, "i32x4");
        generate_int_vector_safe_sub(builder, module, only_declarations, llvm::VectorType::get(i32_type, 4, false), 4, "i32x4");
        generate_int_vector_safe_mul(builder, module, only_declarations, llvm::VectorType::get(i32_type, 4, false), 4, "i32x4");
        generate_int_vector_safe_div(builder, module, only_declarations, llvm::VectorType::get(i32_type, 4, false), 4, "i32x4");
        // i32x8 functions
        generate_int_vector_safe_add(builder, module, only_declarations, llvm::VectorType::get(i32_type, 8, false), 8, "i32x8");
        generate_int_vector_safe_sub(builder, module, only_declarations, llvm::VectorType::get(i32_type, 8, false), 8, "i32x8");
        generate_int_vector_safe_mul(builder, module, only_declarations, llvm::VectorType::get(i32_type, 8, false), 8, "i32x8");
        generate_int_vector_safe_div(builder, module, only_declarations, llvm::VectorType::get(i32_type, 8, false), 8, "i32x8");
        // i64x2 functions
        generate_int_vector_safe_add(builder, module, only_declarations, llvm::VectorType::get(i64_type, 2, false), 2, "i64x2");
        generate_int_vector_safe_sub(builder, module, only_declarations, llvm::VectorType::get(i64_type, 2, false), 2, "i64x2");
        generate_int_vector_safe_mul(builder, module, only_declarations, llvm::VectorType::get(i64_type, 2, false), 2, "i64x2");
        generate_int_vector_safe_div(builder, module, only_declarations, llvm::VectorType::get(i64_type, 2, false), 2, "i64x2");
        // i64x3 functions
        generate_int_vector_safe_add(builder, module, only_declarations, llvm::VectorType::get(i64_type, 3, false), 3, "i64x3");
        generate_int_vector_safe_sub(builder, module, only_declarations, llvm::VectorType::get(i64_type, 3, false), 3, "i64x3");
        generate_int_vector_safe_mul(builder, module, only_declarations, llvm::VectorType::get(i64_type, 3, false), 3, "i64x3");
        generate_int_vector_safe_div(builder, module, only_declarations, llvm::VectorType::get(i64_type, 3, false), 3, "i64x3");
        // i64x4 functions
        generate_int_vector_safe_add(builder, module, only_declarations, llvm::VectorType::get(i64_type, 4, false), 4, "i64x4");
        generate_int_vector_safe_sub(builder, module, only_declarations, llvm::VectorType::get(i64_type, 4, false), 4, "i64x4");
        generate_int_vector_safe_mul(builder, module, only_declarations, llvm::VectorType::get(i64_type, 4, false), 4, "i64x4");
        generate_int_vector_safe_div(builder, module, only_declarations, llvm::VectorType::get(i64_type, 4, false), 4, "i64x4");
    }

    // Generate the pow functions at the very end, because they depend on the safe mult and div functions if not in unsafe mode
    generate_pow_function(builder, module, only_declarations, i8_type, false);
    generate_pow_function(builder, module, only_declarations, i32_type, false);
    generate_pow_function(builder, module, only_declarations, i32_type, true);
    generate_pow_function(builder, module, only_declarations, i64_type, false);
    generate_pow_function(builder, module, only_declarations, i64_type, true);
}

bool Generator::Module::Arithmetic::refresh_arithmetic_functions(llvm::Module *module) {
    for (auto &arithmetic_function : arithmetic_functions) {
        arithmetic_function.second = module->getFunction("__flint_" + arithmetic_function.first);
        if (arithmetic_function.second == nullptr) {
            return false;
        }
    }
    return true;
}

void Generator::Module::Arithmetic::generate_pow_function( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::IntegerType *int_type,                           //
    const bool is_signed                                   //
) {
    // THE C IMPLEMENTATION
    // int pow(int base, int exp) {
    //     int _base = base;
    //     int _exp = exp;
    //     int result = 1;
    //     while (_exp > 0) {
    //         if (_exp % 2 == 1) {
    //             result *= _base;
    //         }
    //         _base *= _base;
    //         _exp /= 2;
    //     }
    //     return result;
    // }
    const std::string name = (is_signed ? "i" : "u") + std::to_string(int_type->getBitWidth());
    llvm::FunctionType *int_pow_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_pow_fn = llvm::Function::Create(int_pow_type, llvm::Function::ExternalLinkage, "__flint_" + name + "_pow", module);
    arithmetic_functions[name + "_pow"] = int_pow_fn;
    if (only_declarations) {
        return;
    }

    // Create the basic blocks for the function
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", int_pow_fn);
    llvm::BasicBlock *loop_condition = llvm::BasicBlock::Create(context, "loop_condition", int_pow_fn);
    llvm::BasicBlock *loop_body = llvm::BasicBlock::Create(context, "loop_body", int_pow_fn);
    llvm::BasicBlock *exp_uneven = llvm::BasicBlock::Create(context, "exp_uneven", int_pow_fn);
    llvm::BasicBlock *exp_merge = llvm::BasicBlock::Create(context, "exp_merge", int_pow_fn);
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "merge", int_pow_fn);
    builder->SetInsertPoint(entry);

    // Create constants
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *one = llvm::ConstantInt::get(int_type, 1);
    llvm::Value *two = llvm::ConstantInt::get(int_type, 2);

    // Get the parameters of the function
    llvm::Argument *arg_base = int_pow_fn->arg_begin();
    arg_base->setName("base");
    llvm::Argument *arg_exp = int_pow_fn->arg_begin() + 1;
    arg_exp->setName("exp");

    // Create local mutable copies of the base and exponent
    llvm::AllocaInst *base_mut = builder->CreateAlloca(int_type, 0, nullptr, "base_mut");
    IR::aligned_store(*builder, arg_base, base_mut);
    llvm::AllocaInst *exp_mut = builder->CreateAlloca(int_type, 0, nullptr, "exp_mut");
    IR::aligned_store(*builder, arg_exp, exp_mut);
    llvm::AllocaInst *result = builder->CreateAlloca(int_type, 0, nullptr, "result");
    IR::aligned_store(*builder, one, result);
    // Branch to the loop condition
    builder->CreateBr(loop_condition);

    // Check if exp > 0
    builder->SetInsertPoint(loop_condition);
    llvm::Value *exp_val = IR::aligned_load(*builder, int_type, exp_mut, "exp_val");
    llvm::Value *exp_gt_zero = nullptr;
    if (is_signed) {
        exp_gt_zero = builder->CreateICmpSGT(exp_val, zero, "exp_gt_zero");
    } else {
        exp_gt_zero = builder->CreateICmpUGT(exp_val, zero, "exp_gt_zero");
    }
    builder->CreateCondBr(exp_gt_zero, loop_body, merge);

    // Check if exp % 2 == 1
    builder->SetInsertPoint(loop_body);
    llvm::Value *base_val = IR::aligned_load(*builder, int_type, base_mut, "base_val");
    llvm::Value *exp_mod_2 = nullptr;
    if (is_signed) {
        exp_mod_2 = builder->CreateSRem(exp_val, two, "exp_mod_2");
    } else {
        exp_mod_2 = builder->CreateURem(exp_val, two, "exp_mod_2");
    }
    llvm::Value *exp_mod_2_eq_1 = builder->CreateICmpEQ(exp_mod_2, one, "mod_2_eq_1");
    builder->CreateCondBr(exp_mod_2_eq_1, exp_uneven, exp_merge);

    // Multiply the result with the base
    builder->SetInsertPoint(exp_uneven);
    llvm::Value *result_val = IR::aligned_load(*builder, int_type, result, "result_val");
    llvm::Value *res_times_base = nullptr;
    if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
        res_times_base = builder->CreateMul(result_val, base_val, "res_times_base");
    } else {
        res_times_base = builder->CreateCall(arithmetic_functions.at(name + "_safe_mul"), {result_val, base_val}, "res_times_base");
    }
    IR::aligned_store(*builder, res_times_base, result);
    builder->CreateBr(exp_merge);

    // Square base, cut exp in half
    builder->SetInsertPoint(exp_merge);
    llvm::Value *base_squared = nullptr;
    if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
        base_squared = builder->CreateMul(base_val, base_val, "base_squared");
    } else {
        base_squared = builder->CreateCall(arithmetic_functions.at(name + "_safe_mul"), {base_val, base_val}, "base_squared");
    }
    IR::aligned_store(*builder, base_squared, base_mut);
    llvm::Value *exp_half = nullptr;
    if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
        if (is_signed) {
            exp_half = builder->CreateSDiv(exp_val, two, "exp_half");
        } else {
            exp_half = builder->CreateUDiv(exp_val, two, "exp_half");
        }
    } else {
        exp_half = builder->CreateCall(arithmetic_functions.at(name + "_safe_div"), {exp_val, two}, "exp_half");
    }
    IR::aligned_store(*builder, exp_half, exp_mut);
    builder->CreateBr(loop_condition);

    // Return the result
    builder->SetInsertPoint(merge);
    result_val = IR::aligned_load(*builder, int_type, result, "result_ret_val");
    builder->CreateRet(result_val);
}

void Generator::Module::Arithmetic::generate_int_safe_add( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::IntegerType *int_type,                           //
    const std::string &name                                //
) {
    llvm::FunctionType *int_safe_add_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_add_fn = llvm::Function::Create( //
        int_safe_add_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "__flint_" + name + "_safe_add",                      //
        module                                                //
    );
    arithmetic_functions[name + "_safe_add"] = int_safe_add_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_safe_add_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", int_safe_add_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", int_safe_add_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_safe_add_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_safe_add_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get type-specific constants
    llvm::Constant *int_min = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMinValue(int_type->getIntegerBitWidth()));
    llvm::Constant *int_max = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMaxValue(int_type->getIntegerBitWidth()));

    // Add with overflow check
    llvm::Value *add = builder->CreateAdd(arg_lhs, arg_rhs, "iaddtmp");

    // Check for positive overflow:
    // If both numbers are positive and result is negative
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *lhs_pos = builder->CreateICmpSGE(arg_lhs, zero);
    llvm::Value *rhs_pos = builder->CreateICmpSGE(arg_rhs, zero);
    llvm::Value *res_neg = builder->CreateICmpSLT(add, zero);
    llvm::Value *pos_overflow = builder->CreateAnd(builder->CreateAnd(lhs_pos, rhs_pos), res_neg);

    // Check for negative overflow:
    // If both numbers are negative and result is positive
    llvm::Value *lhs_neg = builder->CreateICmpSLT(arg_lhs, zero);
    llvm::Value *rhs_neg = builder->CreateICmpSLT(arg_rhs, zero);
    llvm::Value *res_pos = builder->CreateICmpSGE(add, zero);
    llvm::Value *neg_overflow = builder->CreateAnd(builder->CreateAnd(lhs_neg, rhs_neg), res_pos);

    // Select appropriate result
    llvm::Value *use_max = pos_overflow;
    llvm::Value *use_min = neg_overflow;
    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *temp = builder->CreateSelect(use_max, int_max, add);
        llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
        builder->CreateRet(selection);
    } else {
        // Check if any overflow occurred
        llvm::Value *overflow_happened = builder->CreateOr(pos_overflow, neg_overflow);
        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        builder->CreateCondBr(overflow_happened, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " add overflow caught\n");
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " add underflow caught\n");
        llvm::Value *message = builder->CreateSelect(overflow_happened, overflow_message, underflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_safe_add'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                llvm::Value *temp = builder->CreateSelect(use_max, int_max, add);
                llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
                builder->CreateRet(selection);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_overflow_block);
        builder->CreateRet(add);
    }
}

void Generator::Module::Arithmetic::generate_int_safe_sub( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::IntegerType *int_type,                           //
    const std::string &name                                //
) {
    llvm::FunctionType *int_safe_sub_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_sub_fn = llvm::Function::Create( //
        int_safe_sub_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "__flint_" + name + "_safe_sub",                      //
        module                                                //
    );
    arithmetic_functions[name + "_safe_sub"] = int_safe_sub_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_safe_sub_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", int_safe_sub_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", int_safe_sub_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_safe_sub_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_safe_sub_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get type-specific constants
    llvm::Value *int_min = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMinValue(int_type->getIntegerBitWidth()));
    llvm::Value *int_max = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMaxValue(int_type->getIntegerBitWidth()));

    // Subtract with overflow check
    llvm::Value *sub = builder->CreateSub(arg_lhs, arg_rhs, "isubtmp");

    // Check for positive overflow:
    // If lhs is positive and rhs is negative and result is negative
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *lhs_pos = builder->CreateICmpSGE(arg_lhs, zero);
    llvm::Value *rhs_neg = builder->CreateICmpSLT(arg_rhs, zero);
    llvm::Value *res_neg = builder->CreateICmpSLT(sub, zero);
    llvm::Value *pos_overflow = builder->CreateAnd(builder->CreateAnd(lhs_pos, rhs_neg), res_neg);

    // Check for negative overflow:
    // If lhs is negative and rhs is positive and result is positive
    llvm::Value *lhs_neg = builder->CreateICmpSLT(arg_lhs, zero);
    llvm::Value *rhs_pos = builder->CreateICmpSGE(arg_rhs, zero);
    llvm::Value *res_pos = builder->CreateICmpSGE(sub, zero);
    llvm::Value *neg_overflow = builder->CreateAnd(builder->CreateAnd(lhs_neg, rhs_pos), res_pos);

    // Select appropriate result
    llvm::Value *use_max = pos_overflow;
    llvm::Value *use_min = neg_overflow;
    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *temp = builder->CreateSelect(use_max, int_max, sub);
        llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
        builder->CreateRet(selection);
    } else {
        // Check if any overflow occurred
        llvm::Value *overflow_happened = builder->CreateOr(pos_overflow, neg_overflow);
        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        builder->CreateCondBr(overflow_happened, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " sub overflow caught\n");
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " sub underflow caught\n");
        llvm::Value *message = builder->CreateSelect(overflow_happened, overflow_message, underflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_safe_sub'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                llvm::Value *temp = builder->CreateSelect(use_max, int_max, sub);
                llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
                builder->CreateRet(selection);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_overflow_block);
        builder->CreateRet(sub);
    }
}

void Generator::Module::Arithmetic::generate_int_safe_mul( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::IntegerType *int_type,                           //
    const std::string &name                                //
) {
    // int64_t i64_safe_mul(int64_t lhs, int64_t rhs) {
    //     // Handle simple cases
    //     if (lhs == 0 || rhs == 0)
    //         return 0;
    //     if (lhs == 1)
    //         return rhs;
    //     if (lhs == -1) {
    //         // Careful with INT64_MIN
    //         if (rhs == INT64_MIN)
    //             return INT64_MAX;
    //         return -rhs;
    //     }
    //     if (rhs == 1)
    //         return lhs;
    //     if (rhs == -1) {
    //         // Careful with INT64_MIN
    //         if (lhs == INT64_MIN)
    //             return INT64_MAX;
    //         return -lhs;
    //     }
    //     // Perform multiplication
    //     int64_t result = lhs * rhs;
    //     // Check for overflow by doing reverse division
    //     // We do not need to check if lhs == 0, as this case has been handled earlier already
    //     if (result / lhs != rhs) {
    //         // Determine if it's positive or negative overflow
    //         if ((lhs < 0) == (rhs < 0)) {
    //             printf("i64 mul overflow caught\n");
    //             return INT64_MAX;
    //         } else {
    //             printf("i64 mul underflow caught\n");
    //             return INT64_MIN;
    //         }
    //     }
    //
    //     return result;
    // }
    llvm::FunctionType *int_safe_mul_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_mul_fn = llvm::Function::Create( //
        int_safe_mul_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "__flint_" + name + "_safe_mul",                      //
        module                                                //
    );
    arithmetic_functions[name + "_safe_mul"] = int_safe_mul_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameters
    llvm::Argument *arg_lhs = int_safe_mul_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_safe_mul_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Check if the bit width is below or equal to 32 bits, in this case we can cast to an i64 and just calculate it that way
    if (int_type->getBitWidth() <= 32) {
        generate_int_safe_mul_small(builder, int_safe_mul_fn, int_type, name, arg_lhs, arg_rhs);
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_safe_mul_fn);
    llvm::BasicBlock *one_side_zero_block = llvm::BasicBlock::Create(context, "one_side_zero", int_safe_mul_fn);
    llvm::BasicBlock *lhs_one_check_block = llvm::BasicBlock::Create(context, "lhs_one_check", int_safe_mul_fn);
    llvm::BasicBlock *lhs_one_block = llvm::BasicBlock::Create(context, "lhs_one", int_safe_mul_fn);
    llvm::BasicBlock *lhs_minus_one_check_block = llvm::BasicBlock::Create(context, "lhs_minus_one_check", int_safe_mul_fn);
    llvm::BasicBlock *lhs_minus_one_block = llvm::BasicBlock::Create(context, "lhs_minus_one", int_safe_mul_fn);
    llvm::BasicBlock *rhs_min_block = llvm::BasicBlock::Create(context, "rhs_min", int_safe_mul_fn);
    llvm::BasicBlock *rhs_not_min_block = llvm::BasicBlock::Create(context, "rhs_not_min", int_safe_mul_fn);
    llvm::BasicBlock *rhs_one_check_block = llvm::BasicBlock::Create(context, "rhs_one_check", int_safe_mul_fn);
    llvm::BasicBlock *rhs_one_block = llvm::BasicBlock::Create(context, "rhs_one", int_safe_mul_fn);
    llvm::BasicBlock *rhs_minus_one_check_block = llvm::BasicBlock::Create(context, "rhs_minus_one_check", int_safe_mul_fn);
    llvm::BasicBlock *rhs_minus_one_block = llvm::BasicBlock::Create(context, "rhs_minus_one", int_safe_mul_fn);
    llvm::BasicBlock *lhs_min_block = llvm::BasicBlock::Create(context, "lhs_min", int_safe_mul_fn);
    llvm::BasicBlock *lhs_not_min_block = llvm::BasicBlock::Create(context, "lhs_not_min", int_safe_mul_fn);
    llvm::BasicBlock *calculation_block = llvm::BasicBlock::Create(context, "calculation", int_safe_mul_fn);
    llvm::BasicBlock *overflow_block = llvm::BasicBlock::Create(context, "overflow", int_safe_mul_fn);
    llvm::BasicBlock *pos_overflow_block = llvm::BasicBlock::Create(context, "pos_overflow", int_safe_mul_fn);
    llvm::BasicBlock *neg_overflow_block = llvm::BasicBlock::Create(context, "neg_overflow", int_safe_mul_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", int_safe_mul_fn);

    // The constants we need
    llvm::Value *int_min = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMinValue(int_type->getIntegerBitWidth()));
    llvm::Value *int_max = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMaxValue(int_type->getIntegerBitWidth()));
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *one = llvm::ConstantInt::get(int_type, 1);
    llvm::Value *minus_one = llvm::ConstantInt::get(int_type, -1);

    // First, check if either the lhs or rhs is 0
    builder->SetInsertPoint(entry_block);
    llvm::Value *lhs_zero = builder->CreateICmpEQ(arg_lhs, zero, "lhs_zero");
    llvm::Value *rhs_zero = builder->CreateICmpEQ(arg_rhs, zero, "rhs_zero");
    llvm::Value *zero_block_check = builder->CreateOr(lhs_zero, rhs_zero, "one_side_zero");
    builder->CreateCondBr(zero_block_check, one_side_zero_block, lhs_one_check_block, IR::generate_weights(1, 100));

    // Return 0 if one side is zero
    builder->SetInsertPoint(one_side_zero_block);
    builder->CreateRet(zero);

    // Check if rhs is 1
    builder->SetInsertPoint(lhs_one_check_block);
    llvm::Value *lhs_one = builder->CreateICmpEQ(arg_lhs, one, "lhs_one");
    builder->CreateCondBr(lhs_one, lhs_one_block, lhs_minus_one_check_block, IR::generate_weights(1, 100));

    // Return rhs if lhs is one
    builder->SetInsertPoint(lhs_one_block);
    builder->CreateRet(arg_rhs);

    // Check if lhs is -1
    builder->SetInsertPoint(lhs_minus_one_check_block);
    llvm::Value *lhs_minus_one = builder->CreateICmpEQ(arg_lhs, minus_one, "lhs_minus_one");
    builder->CreateCondBr(lhs_minus_one, lhs_minus_one_block, rhs_one_check_block, IR::generate_weights(1, 100));

    {
        // Check if rhs is equal to int min
        builder->SetInsertPoint(lhs_minus_one_block);
        llvm::Value *rhs_eq_min = builder->CreateICmpEQ(arg_rhs, int_min, "rhs_eq_min");
        builder->CreateCondBr(rhs_eq_min, rhs_min_block, rhs_not_min_block, IR::generate_weights(1, 100));

        // Return int max in this case
        builder->SetInsertPoint(rhs_min_block);
        builder->CreateRet(int_max);

        // Just return the result in this case
        builder->SetInsertPoint(rhs_not_min_block);
        llvm::Value *result = builder->CreateMul(arg_lhs, arg_rhs, "result");
        builder->CreateRet(result);
    }

    // Check if the rhs is 1
    builder->SetInsertPoint(rhs_one_check_block);
    llvm::Value *rhs_one = builder->CreateICmpEQ(arg_rhs, one, "rhs_one");
    builder->CreateCondBr(rhs_one, rhs_one_block, rhs_minus_one_check_block, IR::generate_weights(1, 100));

    // Just return the lhs if the rhs is one
    builder->SetInsertPoint(rhs_one_block);
    builder->CreateRet(arg_lhs);

    // Check if the rhs is minus one
    builder->SetInsertPoint(rhs_minus_one_check_block);
    llvm::Value *rhs_minus_one = builder->CreateICmpEQ(arg_rhs, minus_one, "rhs_minus_one");
    builder->CreateCondBr(rhs_minus_one, rhs_minus_one_block, calculation_block, IR::generate_weights(1, 100));

    {
        // Check if lhs is equal to int min
        builder->SetInsertPoint(rhs_minus_one_block);
        llvm::Value *lhs_eq_min = builder->CreateICmpEQ(arg_lhs, int_min, "lhs_eq_min");
        builder->CreateCondBr(lhs_eq_min, lhs_min_block, lhs_not_min_block, IR::generate_weights(1, 100));

        // Return int max in this case
        builder->SetInsertPoint(lhs_min_block);
        builder->CreateRet(int_max);

        // Just return the result in this case
        builder->SetInsertPoint(lhs_not_min_block);
        llvm::Value *result = builder->CreateMul(arg_lhs, arg_rhs, "result");
        builder->CreateRet(result);
    }

    // Calculate the actual result of the multiplication and then check if an overflow has occured
    builder->SetInsertPoint(calculation_block);
    llvm::Value *result = builder->CreateMul(arg_lhs, arg_rhs, "result");
    llvm::Value *result_div_lhs = builder->CreateSDiv(result, arg_lhs, "result_div_lhs");
    llvm::Value *overflow_check = builder->CreateICmpNE(result_div_lhs, arg_rhs, "overflow_check");
    builder->CreateCondBr(overflow_check, overflow_block, merge_block, IR::generate_weights(1, 100));

    {
        // An overflow occured, check if the overflow was positive (overflow) or negative (underflow)
        builder->SetInsertPoint(overflow_block);
        llvm::Value *lhs_lt_zero = builder->CreateICmpSLT(arg_lhs, zero, "lhs_lt_zero");
        llvm::Value *rhs_lt_zero = builder->CreateICmpSLT(arg_rhs, zero, "rhs_lt_zero");
        llvm::Value *overflow_kind_check = builder->CreateICmpEQ(lhs_lt_zero, rhs_lt_zero, "overflow_kind_check");
        builder->CreateCondBr(overflow_kind_check, pos_overflow_block, neg_overflow_block);

        // Positive overflow, return int max and print the message to the console
        builder->SetInsertPoint(pos_overflow_block);
        if (overflow_mode == ArithmeticOverflowMode::SILENT) {
            builder->CreateRet(int_max);
        } else {
            llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " mult overflow caught\n");
            builder->CreateCall(c_functions.at(PRINTF), {overflow_message});
            switch (overflow_mode) {
                default:
                    assert(false && "Not allowed overflow mode in 'generate_int_safe_mul'");
                    return;
                case ArithmeticOverflowMode::PRINT: {
                    builder->CreateRet(int_max);
                    break;
                }
                case ArithmeticOverflowMode::CRASH:
                    builder->CreateCall(c_functions.at(ABORT));
                    builder->CreateUnreachable();
                    break;
            }
        }

        // Underflow, return int min and print message to the console
        builder->SetInsertPoint(neg_overflow_block);
        if (overflow_mode == ArithmeticOverflowMode::SILENT) {
            builder->CreateRet(int_min);
        } else {
            llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " mult underflow caught\n");
            builder->CreateCall(c_functions.at(PRINTF), {overflow_message});
            switch (overflow_mode) {
                default:
                    assert(false && "Not allowed overflow mode in 'generate_int_safe_mul'");
                    return;
                case ArithmeticOverflowMode::PRINT: {
                    builder->CreateRet(int_min);
                    break;
                }
                case ArithmeticOverflowMode::CRASH:
                    builder->CreateCall(c_functions.at(ABORT));
                    builder->CreateUnreachable();
                    break;
            }
        }
    }

    // No overflow happens, return the calculated number
    builder->SetInsertPoint(merge_block);
    builder->CreateRet(result);
}

void Generator::Module::Arithmetic::generate_int_safe_mul_small( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Function *int_safe_mul_fn,                             //
    llvm::IntegerType *int_type,                                 //
    const std::string &name,                                     //
    llvm::Argument *arg_lhs,                                     //
    llvm::Argument *arg_rhs                                      //
) {
    // THE C IMPLEMENTATION:
    // int32_t i32_safe_mul(int32_t lhs, int32_t rhs) {
    //     // Cast to i64, multiply, then check range
    //     int64_t result = (int64_t)lhs * (int64_t)rhs;
    //
    //     if (result > INT32_MAX) {
    //         printf("i32 mul overflow caught\n");
    //         return INT32_MAX;
    //     }
    //     if (result < INT32_MIN) {
    //         printf("i32 mul underflow caught\n");
    //         return INT32_MIN;
    //     }
    //
    //     return (int32_t)result;
    // }
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_safe_mul_fn);
    llvm::BasicBlock *overflow_block = llvm::BasicBlock::Create(context, "overflow", int_safe_mul_fn);
    llvm::BasicBlock *no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", int_safe_mul_fn);
    llvm::BasicBlock *underflow_block = llvm::BasicBlock::Create(context, "underflow", int_safe_mul_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", int_safe_mul_fn);
    builder->SetInsertPoint(entry_block);

    llvm::Value *int_max = llvm::ConstantInt::get(                              //
        builder->getInt64Ty(),                                                  //
        llvm::APInt::getSignedMaxValue(int_type->getIntegerBitWidth()).sext(64) //
    );
    llvm::Value *int_min = llvm::ConstantInt::get(                              //
        builder->getInt64Ty(),                                                  //
        llvm::APInt::getSignedMinValue(int_type->getIntegerBitWidth()).sext(64) //
    );

    // First, cast both the lhs and rhs to an i64
    llvm::Value *lhs_i64 = builder->CreateSExt(arg_lhs, builder->getInt64Ty(), "lhs_i64");
    llvm::Value *rhs_i64 = builder->CreateSExt(arg_rhs, builder->getInt64Ty(), "rhs_i64");

    // Then, do the multiplication and overflow check
    llvm::Value *result = builder->CreateMul(lhs_i64, rhs_i64, "result");
    llvm::Value *res_gt_max = builder->CreateICmpSGT(result, int_max, "res_gt_max");
    builder->CreateCondBr(res_gt_max, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

    // An overflow occured. Depending on the arithmetics mode we crash, clamp and or print
    builder->SetInsertPoint(overflow_block);
    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *int_max_cast = builder->CreateTrunc(int_max, int_type, "int_max_cast");
        builder->CreateRet(int_max_cast);
    } else {
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " mult overflow caught\n");
        builder->CreateCall(c_functions.at(PRINTF), {overflow_message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_safe_mul_small'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                llvm::Value *int_max_cast = builder->CreateTrunc(int_max, int_type, "int_max_cast");
                builder->CreateRet(int_max_cast);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }
    }

    // Check if its an underflow
    builder->SetInsertPoint(no_overflow_block);
    llvm::Value *res_lt_min = builder->CreateICmpSLT(result, int_min, "res_lt_min");
    builder->CreateCondBr(res_lt_min, underflow_block, merge_block, IR::generate_weights(1, 100));

    // An underflow occured. Depending on the arithmetics mode we crash, clamp and or print
    builder->SetInsertPoint(underflow_block);
    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *int_min_cast = builder->CreateTrunc(int_min, int_type, "int_min_cast");
        builder->CreateRet(int_min_cast);
    } else {
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " mult underflow caught\n");
        builder->CreateCall(c_functions.at(PRINTF), {underflow_message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_safe_mul_small'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                llvm::Value *int_min_cast = builder->CreateTrunc(int_min, int_type, "int_min_cast");
                builder->CreateRet(int_min_cast);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }
    }

    // No over- or underflow occured, so we can simply cast and return the result
    builder->SetInsertPoint(merge_block);
    llvm::Value *result_cast = builder->CreateTrunc(result, int_type, "result_cast");
    builder->CreateRet(result_cast);
}

void Generator::Module::Arithmetic::generate_int_safe_div( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::IntegerType *int_type,                           //
    const std::string &name                                //
) {
    llvm::FunctionType *int_safe_div_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_div_fn = llvm::Function::Create( //
        int_safe_div_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "__flint_" + name + "_safe_div",                      //
        module                                                //
    );
    arithmetic_functions[name + "_safe_div"] = int_safe_div_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_safe_div_fn);
    llvm::BasicBlock *error_block = nullptr;
    llvm::BasicBlock *no_error_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        error_block = llvm::BasicBlock::Create(context, "error", int_safe_div_fn);
        no_error_block = llvm::BasicBlock::Create(context, "no_error", int_safe_div_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_safe_div_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_safe_div_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Check for division by zero and MIN_INT/-1
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *minus_one = llvm::ConstantInt::get(int_type, -1);
    llvm::Value *min_int = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMinValue(int_type->getIntegerBitWidth()));

    llvm::Value *div_by_zero = builder->CreateICmpEQ(arg_rhs, zero);
    llvm::Value *is_min_int = builder->CreateICmpEQ(arg_lhs, min_int);
    llvm::Value *div_by_minus_one = builder->CreateICmpEQ(arg_rhs, minus_one);
    llvm::Value *would_overflow = builder->CreateAnd(is_min_int, div_by_minus_one);

    // Combine error conditions
    llvm::Value *error_happened = builder->CreateOr(div_by_zero, would_overflow);
    llvm::Value *div = builder->CreateSDiv(arg_lhs, arg_rhs, "idivtmp");

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *result = builder->CreateSelect(builder->CreateOr(div_by_zero, would_overflow), arg_lhs, div);
        builder->CreateRet(result);
    } else {
        // Change branch prediction, as errors are much less likely to happen
        builder->CreateCondBr(error_happened, error_block, no_error_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(error_block);
        llvm::Value *div_zero_message = IR::generate_const_string(*builder, name + " division by zero caught\n");
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " division overflow caught\n");
        llvm::Value *message = builder->CreateSelect(div_by_zero, div_zero_message, overflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_safe_div'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                builder->CreateRet(arg_lhs);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_error_block);
        builder->CreateRet(div);
    }
}

void Generator::Module::Arithmetic::generate_int_safe_mod( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::IntegerType *int_type,                           //
    const std::string &name,                               //
    const bool is_signed                                   //
) {
    llvm::FunctionType *int_safe_mod_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_mod_fn = llvm::Function::Create( //
        int_safe_mod_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "__flint_" + name + "_safe_mod",                      //
        module                                                //
    );
    arithmetic_functions[name + "_safe_mod"] = int_safe_mod_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_safe_mod_fn);
    llvm::BasicBlock *rhs_null_block = llvm::BasicBlock::Create(context, "rhs_null", int_safe_mod_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", int_safe_mod_fn);
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_safe_mod_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_safe_mod_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Check if rhs is 0
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *is_rhs_null = builder->CreateICmpEQ(arg_rhs, zero, "rhs_is_null");
    builder->CreateCondBr(is_rhs_null, rhs_null_block, merge_block);

    builder->SetInsertPoint(rhs_null_block);
    builder->CreateRet(zero);

    builder->SetInsertPoint(merge_block);
    llvm::Value *mod_res = nullptr;
    if (is_signed) {
        mod_res = builder->CreateSRem(arg_lhs, arg_rhs, "mod_res");
    } else {
        mod_res = builder->CreateURem(arg_lhs, arg_rhs, "mod_res");
    }
    builder->CreateRet(mod_res);
}

void Generator::Module::Arithmetic::generate_uint_safe_add( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations,                           //
    llvm::IntegerType *int_type,                            //
    const std::string &name                                 //
) {
    llvm::FunctionType *uint_safe_add_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_add_fn = llvm::Function::Create( //
        uint_safe_add_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_" + name + "_safe_add",                       //
        module                                                 //
    );
    arithmetic_functions[name + "_safe_add"] = uint_safe_add_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", uint_safe_add_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", uint_safe_add_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", uint_safe_add_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = uint_safe_add_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = uint_safe_add_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Check if sum would overflow: if rhs > max - lhs
    auto max = llvm::ConstantInt::get(int_type, llvm::APInt::getMaxValue(int_type->getBitWidth())); // All bits set to 1
    auto diff = builder->CreateSub(max, arg_lhs, "diff");
    auto would_overflow = builder->CreateICmpUGT(arg_rhs, diff, "overflow_check");
    auto sum = builder->CreateAdd(arg_lhs, arg_rhs, "uaddtmp");

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *result = builder->CreateSelect(would_overflow, max, sum, "safe_uaddtmp");
        builder->CreateRet(result);
    } else {
        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        builder->CreateCondBr(would_overflow, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " add overflow caught\n");
        builder->CreateCall(c_functions.at(PRINTF), {overflow_message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_uint_safe_add'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                builder->CreateRet(max);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_overflow_block);
        builder->CreateRet(sum);
    }
}

void Generator::Module::Arithmetic::generate_uint_safe_sub( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations,                           //
    llvm::IntegerType *int_type,                            //
    const std::string &name                                 //
) {
    llvm::FunctionType *uint_safe_sub_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_sub_fn = llvm::Function::Create( //
        uint_safe_sub_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_" + name + "_safe_sub",                       //
        module                                                 //
    );
    arithmetic_functions[name + "_safe_sub"] = uint_safe_sub_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", uint_safe_sub_fn);
    llvm::BasicBlock *underflow_block = nullptr;
    llvm::BasicBlock *no_underflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        underflow_block = llvm::BasicBlock::Create(context, "underflow", uint_safe_sub_fn);
        no_underflow_block = llvm::BasicBlock::Create(context, "no_underflow", uint_safe_sub_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = uint_safe_sub_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = uint_safe_sub_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // compare if lhs >= rhs
    llvm::Value *cmp = builder->CreateICmpUGE(arg_lhs, arg_rhs, "cmp");
    // Perform normal subtraction
    llvm::Value *sub = builder->CreateSub(arg_lhs, arg_rhs, "usubtmp");
    // Select between 0 and subtraction result based on comparison
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *result = builder->CreateSelect(cmp, sub, zero, "safe_usubtmp");
        builder->CreateRet(result);
    } else {
        // Change branch prediction, as no underflow is much more likely to happen than an underflow
        builder->CreateCondBr(cmp, no_underflow_block, underflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(underflow_block);
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " sub underflow caught\n");
        builder->CreateCall(c_functions.at(PRINTF), {underflow_message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_uint_safe_sub'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                builder->CreateRet(zero);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_underflow_block);
        builder->CreateRet(sub);
    }
}

void Generator::Module::Arithmetic::generate_uint_safe_mul( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations,                           //
    llvm::IntegerType *int_type,                            //
    const std::string &name                                 //
) {
    llvm::FunctionType *uint_safe_mul_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_mul_fn = llvm::Function::Create( //
        uint_safe_mul_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_" + name + "_safe_mul",                       //
        module                                                 //
    );
    arithmetic_functions[name + "_safe_mul"] = uint_safe_mul_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", uint_safe_mul_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", uint_safe_mul_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", uint_safe_mul_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = uint_safe_mul_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = uint_safe_mul_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Check if either operand is 0
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *is_zero = builder->CreateOr(builder->CreateICmpEQ(arg_lhs, zero), builder->CreateICmpEQ(arg_rhs, zero));

    // Check for overflow: if rhs > max/lhs
    llvm::Value *max = llvm::ConstantInt::get(int_type, llvm::APInt::getMaxValue(int_type->getBitWidth())); // All bits set to 1
    llvm::Value *udiv = builder->CreateUDiv(max, arg_lhs);
    llvm::Value *would_overflow = builder->CreateICmpUGT(arg_rhs, udiv);

    // Combine checks
    llvm::Value *use_max = builder->CreateAnd(builder->CreateNot(is_zero), would_overflow);
    llvm::Value *mult = builder->CreateMul(arg_lhs, arg_rhs, "umultmp");

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *result = builder->CreateSelect(use_max, max, mult, "safe_umultmp");
        builder->CreateRet(result);
    } else {
        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        builder->CreateCondBr(use_max, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " mult overflow caught\n");
        builder->CreateCall(c_functions.at(PRINTF), {overflow_message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_uint_safe_mul'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                builder->CreateRet(max);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_overflow_block);
        builder->CreateRet(mult);
    }
}

void Generator::Module::Arithmetic::generate_uint_safe_div( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations,                           //
    llvm::IntegerType *int_type,                            //
    const std::string &name                                 //
) {
    llvm::FunctionType *uint_safe_div_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_div_fn = llvm::Function::Create( //
        uint_safe_div_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_" + name + "_safe_div",                       //
        module                                                 //
    );
    arithmetic_functions[name + "_safe_div"] = uint_safe_div_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", uint_safe_div_fn);
    llvm::BasicBlock *error_block = nullptr;
    llvm::BasicBlock *no_error_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        error_block = llvm::BasicBlock::Create(context, "error", uint_safe_div_fn);
        no_error_block = llvm::BasicBlock::Create(context, "no_error", uint_safe_div_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = uint_safe_div_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = uint_safe_div_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Check for division by zero
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);
    llvm::Value *div_by_zero = builder->CreateICmpEQ(arg_rhs, zero);

    llvm::Value *div = builder->CreateUDiv(arg_lhs, arg_rhs, "udivtmp");
    llvm::Value *max = llvm::ConstantInt::get(int_type, llvm::APInt::getMaxValue(int_type->getBitWidth())); // All bits set to 1

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *result = builder->CreateSelect(div_by_zero, max, div, "safe_udivtmp");
        builder->CreateRet(result);
    } else {
        // Change branch prediction, as no error is much more likely to happen than an error
        builder->CreateCondBr(div_by_zero, error_block, no_error_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(error_block);
        llvm::Value *div_zero_message = IR::generate_const_string(*builder, name + " division by zero caught\n");
        builder->CreateCall(c_functions.at(PRINTF), {div_zero_message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_uint_safe_div'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                builder->CreateRet(max);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_error_block);
        builder->CreateRet(div);
    }
}

void Generator::Module::Arithmetic::generate_int_vector_safe_add( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations,                                 //
    llvm::VectorType *vector_int_type,                            //
    const unsigned int vector_width,                              //
    const std::string &name                                       //
) {
    llvm::FunctionType *int_vector_safe_add_type = llvm::FunctionType::get(vector_int_type, {vector_int_type, vector_int_type}, false);
    llvm::Function *int_vector_safe_add_fn = llvm::Function::Create( //
        int_vector_safe_add_type,                                    //
        llvm::Function::ExternalLinkage,                             //
        "__flint_" + name + "_safe_add",                             //
        module                                                       //
    );
    arithmetic_functions[name + "_safe_add"] = int_vector_safe_add_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_vector_safe_add_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", int_vector_safe_add_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", int_vector_safe_add_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_vector_safe_add_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_vector_safe_add_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get the scalar type from the vector type
    llvm::Type *element_type = vector_int_type->getElementType();

    // Get type-specific constants
    llvm::Constant *scalar_int_min = llvm::ConstantInt::get(                             //
        element_type, llvm::APInt::getSignedMinValue(element_type->getIntegerBitWidth()) //
    );
    llvm::Constant *scalar_int_max = llvm::ConstantInt::get(                             //
        element_type, llvm::APInt::getSignedMaxValue(element_type->getIntegerBitWidth()) //
    );

    // Create vector constants by splat (repeating scalar values)
    llvm::Constant *int_min = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_int_min);
    llvm::Constant *int_max = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_int_max);
    llvm::Constant *zero = llvm::ConstantVector::getSplat(                                    //
        llvm::ElementCount::get(vector_width, false), llvm::ConstantInt::get(element_type, 0) //
    );

    // Add with overflow check
    llvm::Value *add = builder->CreateAdd(arg_lhs, arg_rhs, "vaddtmp");

    // Check for positive overflow:
    // If both numbers are positive and result is negative
    llvm::Value *lhs_pos = builder->CreateICmpSGE(arg_lhs, zero);
    llvm::Value *rhs_pos = builder->CreateICmpSGE(arg_rhs, zero);
    llvm::Value *res_neg = builder->CreateICmpSLT(add, zero);
    llvm::Value *pos_overflow = builder->CreateAnd(builder->CreateAnd(lhs_pos, rhs_pos), res_neg);

    // Check for negative overflow:
    // If both numbers are negative and result is positive
    llvm::Value *lhs_neg = builder->CreateICmpSLT(arg_lhs, zero);
    llvm::Value *rhs_neg = builder->CreateICmpSLT(arg_rhs, zero);
    llvm::Value *res_pos = builder->CreateICmpSGE(add, zero);
    llvm::Value *neg_overflow = builder->CreateAnd(builder->CreateAnd(lhs_neg, rhs_neg), res_pos);

    // Select appropriate result
    llvm::Value *use_max = pos_overflow;
    llvm::Value *use_min = neg_overflow;

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *temp = builder->CreateSelect(use_max, int_max, add);
        llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
        builder->CreateRet(selection);
    } else {
        // Check if any overflow occurred in any element
        llvm::Value *overflow_happened_vec = builder->CreateOr(pos_overflow, neg_overflow);

        // Convert vector of booleans to a single boolean using a reduction
        // First, we'll create the intrinsic declaration for vector_reduce_or
        llvm::Type *overflow_vec_type = overflow_happened_vec->getType();
        llvm::Function *reduce_or_fn = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::vector_reduce_or, {overflow_vec_type});

        // Now call the intrinsic to get a single boolean value
        llvm::Value *any_overflow = builder->CreateCall(reduce_or_fn, {overflow_happened_vec}, "any_overflow");

        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        builder->CreateCondBr(any_overflow, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " add overflow caught\n");
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " add underflow caught\n");

        // Determine if the overflow is positive or negative
        // For vectors, just check if any positive overflow happened
        llvm::Function *reduce_or_fn_pos = llvm::Intrinsic::getDeclaration(      //
            module, llvm::Intrinsic::vector_reduce_or, {pos_overflow->getType()} //
        );
        llvm::Value *any_pos_overflow = builder->CreateCall(reduce_or_fn_pos, {pos_overflow}, "any_pos_overflow");

        llvm::Value *message = builder->CreateSelect(any_pos_overflow, overflow_message, underflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});

        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_vector_safe_add'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                llvm::Value *temp = builder->CreateSelect(use_max, int_max, add);
                llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
                builder->CreateRet(selection);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_overflow_block);
        builder->CreateRet(add);
    }
}

void Generator::Module::Arithmetic::generate_int_vector_safe_sub( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations,                                 //
    llvm::VectorType *vector_int_type,                            //
    const unsigned int vector_width,                              //
    const std::string &name                                       //
) {
    llvm::FunctionType *int_vector_safe_sub_type = llvm::FunctionType::get(vector_int_type, {vector_int_type, vector_int_type}, false);
    llvm::Function *int_vector_safe_sub_fn = llvm::Function::Create( //
        int_vector_safe_sub_type,                                    //
        llvm::Function::ExternalLinkage,                             //
        "__flint_" + name + "_safe_sub",                             //
        module                                                       //
    );
    arithmetic_functions[name + "_safe_sub"] = int_vector_safe_sub_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_vector_safe_sub_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", int_vector_safe_sub_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", int_vector_safe_sub_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_vector_safe_sub_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_vector_safe_sub_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get the scalar type from the vector type
    llvm::Type *element_type = vector_int_type->getElementType();

    // Get type-specific constants
    llvm::Constant *scalar_int_min = llvm::ConstantInt::get(                             //
        element_type, llvm::APInt::getSignedMinValue(element_type->getIntegerBitWidth()) //
    );
    llvm::Constant *scalar_int_max = llvm::ConstantInt::get(                             //
        element_type, llvm::APInt::getSignedMaxValue(element_type->getIntegerBitWidth()) //
    );

    // Create vector constants by splat (repeating scalar values)
    llvm::Constant *int_min = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_int_min);
    llvm::Constant *int_max = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_int_max);
    llvm::Constant *zero = llvm::ConstantVector::getSplat(                                    //
        llvm::ElementCount::get(vector_width, false), llvm::ConstantInt::get(element_type, 0) //
    );

    // Subtract with overflow check
    llvm::Value *sub = builder->CreateSub(arg_lhs, arg_rhs, "vsubtmp");

    // Check for positive overflow:
    // If lhs is positive and rhs is negative and result is negative
    llvm::Value *lhs_pos = builder->CreateICmpSGE(arg_lhs, zero);
    llvm::Value *rhs_neg = builder->CreateICmpSLT(arg_rhs, zero);
    llvm::Value *res_neg = builder->CreateICmpSLT(sub, zero);
    llvm::Value *pos_overflow = builder->CreateAnd(builder->CreateAnd(lhs_pos, rhs_neg), res_neg);

    // Check for negative overflow:
    // If lhs is negative and rhs is positive and result is positive
    llvm::Value *lhs_neg = builder->CreateICmpSLT(arg_lhs, zero);
    llvm::Value *rhs_pos = builder->CreateICmpSGE(arg_rhs, zero);
    llvm::Value *res_pos = builder->CreateICmpSGE(sub, zero);
    llvm::Value *neg_overflow = builder->CreateAnd(builder->CreateAnd(lhs_neg, rhs_pos), res_pos);

    // Select appropriate result
    llvm::Value *use_max = pos_overflow;
    llvm::Value *use_min = neg_overflow;

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *temp = builder->CreateSelect(use_max, int_max, sub);
        llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
        builder->CreateRet(selection);
    } else {
        // Check if any overflow occurred in any element
        llvm::Value *overflow_happened_vec = builder->CreateOr(pos_overflow, neg_overflow);

        // Convert vector of booleans to a single boolean using a reduction
        // First, we'll create the intrinsic declaration for vector_reduce_or
        llvm::Type *overflow_vec_type = overflow_happened_vec->getType();
        llvm::Function *reduce_or_fn = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::vector_reduce_or, {overflow_vec_type});

        // Now call the intrinsic to get a single boolean value
        llvm::Value *any_overflow = builder->CreateCall(reduce_or_fn, {overflow_happened_vec}, "any_overflow");

        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        builder->CreateCondBr(any_overflow, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " sub overflow caught\n");
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " sub underflow caught\n");

        // Determine if the overflow is positive or negative
        // For vectors, just check if any positive overflow happened
        llvm::Function *reduce_or_fn_pos = llvm::Intrinsic::getDeclaration(      //
            module, llvm::Intrinsic::vector_reduce_or, {pos_overflow->getType()} //
        );
        llvm::Value *any_pos_overflow = builder->CreateCall(reduce_or_fn_pos, {pos_overflow}, "any_pos_overflow");

        llvm::Value *message = builder->CreateSelect(any_pos_overflow, overflow_message, underflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});

        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_vector_safe_sub'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                llvm::Value *temp = builder->CreateSelect(use_max, int_max, sub);
                llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
                builder->CreateRet(selection);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_overflow_block);
        builder->CreateRet(sub);
    }
}

void Generator::Module::Arithmetic::generate_int_vector_safe_mul( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations,                                 //
    llvm::VectorType *vector_int_type,                            //
    const unsigned int vector_width,                              //
    const std::string &name                                       //
) {
    llvm::FunctionType *int_vector_safe_mul_type = llvm::FunctionType::get(vector_int_type, {vector_int_type, vector_int_type}, false);
    llvm::Function *int_vector_safe_mul_fn = llvm::Function::Create( //
        int_vector_safe_mul_type,                                    //
        llvm::Function::ExternalLinkage,                             //
        "__flint_" + name + "_safe_mul",                             //
        module                                                       //
    );
    arithmetic_functions[name + "_safe_mul"] = int_vector_safe_mul_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_vector_safe_mul_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", int_vector_safe_mul_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", int_vector_safe_mul_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_vector_safe_mul_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_vector_safe_mul_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get the scalar type from the vector type
    llvm::Type *element_type = vector_int_type->getElementType();

    // Get type-specific constants
    llvm::Constant *scalar_int_min = llvm::ConstantInt::get(                             //
        element_type, llvm::APInt::getSignedMinValue(element_type->getIntegerBitWidth()) //
    );
    llvm::Constant *scalar_int_max = llvm::ConstantInt::get(                             //
        element_type, llvm::APInt::getSignedMaxValue(element_type->getIntegerBitWidth()) //
    );

    // Create vector constants by splat (repeating scalar values)
    llvm::Constant *int_min = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_int_min);
    llvm::Constant *int_max = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_int_max);
    llvm::Constant *zero = llvm::ConstantVector::getSplat(                                    //
        llvm::ElementCount::get(vector_width, false), llvm::ConstantInt::get(element_type, 0) //
    );

    // Basic multiplication
    llvm::Value *mult = builder->CreateMul(arg_lhs, arg_rhs, "vmultmp");

    // Check signs
    llvm::Value *lhs_is_neg = builder->CreateICmpSLT(arg_lhs, zero);
    llvm::Value *rhs_is_neg = builder->CreateICmpSLT(arg_rhs, zero);
    llvm::Value *result_should_be_pos = builder->CreateICmpEQ(lhs_is_neg, rhs_is_neg);

    // Check if result has wrong sign (indicates overflow)
    llvm::Value *result_is_neg = builder->CreateICmpSLT(mult, zero);
    llvm::Value *wrong_sign = builder->CreateICmpNE(result_should_be_pos, builder->CreateNot(result_is_neg));

    // Select appropriate result
    llvm::Value *use_max = builder->CreateAnd(wrong_sign, result_should_be_pos);
    llvm::Value *use_min = builder->CreateAnd(wrong_sign, builder->CreateNot(result_should_be_pos));

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *temp = builder->CreateSelect(use_max, int_max, mult);
        llvm::Value *result = builder->CreateSelect(use_min, int_min, temp);
        builder->CreateRet(result);
    } else {
        // Check if any overflow occurred in any element
        // Convert vector of booleans to a single boolean using a reduction
        llvm::Type *wrong_sign_vec_type = wrong_sign->getType();
        llvm::Function *reduce_or_fn = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::vector_reduce_or, {wrong_sign_vec_type});
        llvm::Value *any_wrong_sign = builder->CreateCall(reduce_or_fn, {wrong_sign}, "any_wrong_sign");

        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        builder->CreateCondBr(any_wrong_sign, overflow_block, no_overflow_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " mul overflow caught\n");
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " mul underflow caught\n");

        // Determine if we should display overflow or underflow message
        // Check if any element has use_max set (which indicates positive overflow)
        llvm::Function *reduce_or_fn_max = llvm::Intrinsic::getDeclaration( //
            module, llvm::Intrinsic::vector_reduce_or, {use_max->getType()} //
        );
        llvm::Value *any_use_max = builder->CreateCall(reduce_or_fn_max, {use_max}, "any_use_max");

        llvm::Value *message = builder->CreateSelect(any_use_max, overflow_message, underflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});

        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_vector_safe_mul'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                llvm::Value *temp = builder->CreateSelect(use_max, int_max, mult);
                llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
                builder->CreateRet(selection);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_overflow_block);
        builder->CreateRet(mult);
    }
}

void Generator::Module::Arithmetic::generate_int_vector_safe_div( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations,                                 //
    llvm::VectorType *vector_int_type,                            //
    const unsigned int vector_width,                              //
    const std::string &name                                       //
) {
    llvm::FunctionType *int_vector_safe_div_type = llvm::FunctionType::get(vector_int_type, {vector_int_type, vector_int_type}, false);
    llvm::Function *int_vector_safe_div_fn = llvm::Function::Create( //
        int_vector_safe_div_type,                                    //
        llvm::Function::ExternalLinkage,                             //
        "__flint_" + name + "_safe_div",                             //
        module                                                       //
    );
    arithmetic_functions[name + "_safe_div"] = int_vector_safe_div_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_vector_safe_div_fn);
    llvm::BasicBlock *error_block = nullptr;
    llvm::BasicBlock *no_error_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        error_block = llvm::BasicBlock::Create(context, "error", int_vector_safe_div_fn);
        no_error_block = llvm::BasicBlock::Create(context, "no_error", int_vector_safe_div_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_vector_safe_div_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_vector_safe_div_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get the scalar type from the vector type
    llvm::Type *element_type = vector_int_type->getElementType();

    // Create vector constants
    llvm::Constant *scalar_zero = llvm::ConstantInt::get(element_type, 0);
    llvm::Constant *scalar_minus_one = llvm::ConstantInt::get(element_type, -1);
    llvm::Constant *scalar_min_int =
        llvm::ConstantInt::get(element_type, llvm::APInt::getSignedMinValue(element_type->getIntegerBitWidth()));

    llvm::Constant *zero = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_zero);
    llvm::Constant *minus_one = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_minus_one);
    llvm::Constant *min_int = llvm::ConstantVector::getSplat(llvm::ElementCount::get(vector_width, false), scalar_min_int);

    // Check for division by zero and MIN_INT/-1
    llvm::Value *div_by_zero = builder->CreateICmpEQ(arg_rhs, zero);
    llvm::Value *is_min_int = builder->CreateICmpEQ(arg_lhs, min_int);
    llvm::Value *div_by_minus_one = builder->CreateICmpEQ(arg_rhs, minus_one);
    llvm::Value *would_overflow = builder->CreateAnd(is_min_int, div_by_minus_one);

    // Combine error conditions
    llvm::Value *error_happened_vec = builder->CreateOr(div_by_zero, would_overflow);

    // Create the division
    llvm::Value *div = builder->CreateSDiv(arg_lhs, arg_rhs, "vdivtmp");

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        // In silent mode, just select the original left operand when there's an error
        llvm::Value *result = builder->CreateSelect(error_happened_vec, arg_lhs, div);
        builder->CreateRet(result);
    } else {
        // Check if any error occurred in any element
        llvm::Type *error_vec_type = error_happened_vec->getType();
        llvm::Function *reduce_or_fn = llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::vector_reduce_or, {error_vec_type});
        llvm::Value *any_error = builder->CreateCall(reduce_or_fn, {error_happened_vec}, "any_error");

        // Change branch prediction, as errors are much less likely to happen
        builder->CreateCondBr(any_error, error_block, no_error_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(error_block);
        llvm::Value *div_zero_message = IR::generate_const_string(*builder, name + " division by zero caught\n");
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " division overflow caught\n");

        // Determine if we should display division by zero or overflow message
        // Check if any element has division by zero
        llvm::Function *reduce_or_fn_zero =
            llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::vector_reduce_or, {div_by_zero->getType()});
        llvm::Value *any_div_by_zero = builder->CreateCall(reduce_or_fn_zero, {div_by_zero}, "any_div_by_zero");

        llvm::Value *message = builder->CreateSelect(any_div_by_zero, div_zero_message, overflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});

        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_vector_safe_div'");
                return;
            case ArithmeticOverflowMode::PRINT: {
                // For the PRINT mode, we use the original lhs values when there's an error
                llvm::Value *result = builder->CreateSelect(error_happened_vec, arg_lhs, div);
                builder->CreateRet(result);
                break;
            }
            case ArithmeticOverflowMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
        }

        builder->SetInsertPoint(no_error_block);
        builder->CreateRet(div);
    }
}
