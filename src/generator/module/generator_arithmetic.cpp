#include "generator/generator.hpp"
#include "globals.hpp"

void Generator::Module::Arithmetic::generate_arithmetic_functions(llvm::IRBuilder<> *builder, llvm::Module *module,
    const bool only_declarations) {
    if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
        // Generate no arithmetic functions for the unsafe mode, as they are never called in unsafe mode annyway
        return;
    }
    llvm::Type *i32_type = llvm::Type::getInt32Ty(context);
    llvm::Type *i64_type = llvm::Type::getInt64Ty(context);
    // i32 functions
    generate_int_safe_add(builder, module, only_declarations, i32_type, "i32");
    generate_int_safe_sub(builder, module, only_declarations, i32_type, "i32");
    generate_int_safe_mul(builder, module, only_declarations, i32_type, "i32");
    generate_int_safe_div(builder, module, only_declarations, i32_type, "i32");
    // i64 functions
    generate_int_safe_add(builder, module, only_declarations, i64_type, "i64");
    generate_int_safe_sub(builder, module, only_declarations, i64_type, "i64");
    generate_int_safe_mul(builder, module, only_declarations, i64_type, "i64");
    generate_int_safe_div(builder, module, only_declarations, i64_type, "i64");
    // u32 functions
    generate_uint_safe_add(builder, module, only_declarations, i32_type, "u32");
    generate_uint_safe_sub(builder, module, only_declarations, i32_type, "u32");
    generate_uint_safe_mul(builder, module, only_declarations, i32_type, "u32");
    generate_uint_safe_div(builder, module, only_declarations, i32_type, "u32");
    // u64 functions
    generate_uint_safe_add(builder, module, only_declarations, i64_type, "u64");
    generate_uint_safe_sub(builder, module, only_declarations, i64_type, "u64");
    generate_uint_safe_mul(builder, module, only_declarations, i64_type, "u64");
    generate_uint_safe_div(builder, module, only_declarations, i64_type, "u64");

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

void Generator::Module::Arithmetic::generate_int_safe_add( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::Type *int_type,                                  //
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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no overflow
            });
        builder->CreateCondBr(overflow_happened, overflow_block, no_overflow_block, branch_weights);

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
    llvm::Type *int_type,                                  //
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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no overflow
            });
        builder->CreateCondBr(overflow_happened, overflow_block, no_overflow_block, branch_weights);

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
    llvm::Type *int_type,                                  //
    const std::string &name                                //
) {
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

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", int_safe_mul_fn);
    llvm::BasicBlock *overflow_block = nullptr;
    llvm::BasicBlock *no_overflow_block = nullptr;
    if (overflow_mode != ArithmeticOverflowMode::SILENT) {
        overflow_block = llvm::BasicBlock::Create(context, "overflow", int_safe_mul_fn);
        no_overflow_block = llvm::BasicBlock::Create(context, "no_overflow", int_safe_mul_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the parameters
    llvm::Argument *arg_lhs = int_safe_mul_fn->arg_begin();
    arg_lhs->setName("lhs");
    llvm::Argument *arg_rhs = int_safe_mul_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    llvm::Value *int_min = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMinValue(int_type->getIntegerBitWidth()));
    llvm::Value *int_max = llvm::ConstantInt::get(int_type, llvm::APInt::getSignedMaxValue(int_type->getIntegerBitWidth()));
    llvm::Value *zero = llvm::ConstantInt::get(int_type, 0);

    // Basic multiplication
    llvm::Value *mult = builder->CreateMul(arg_lhs, arg_rhs, "imultmp");

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
        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no overflow
            });
        builder->CreateCondBr(wrong_sign, overflow_block, no_overflow_block, branch_weights);

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " mul overflow caught\n");
        llvm::Value *underflow_message = IR::generate_const_string(*builder, name + " mul underflow caught\n");
        llvm::Value *message = builder->CreateSelect(use_max, overflow_message, underflow_message);
        builder->CreateCall(c_functions.at(PRINTF), {message});
        switch (overflow_mode) {
            default:
                assert(false && "Not allowed overflow mode in 'generate_int_safe_mul'");
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

void Generator::Module::Arithmetic::generate_int_safe_div( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    llvm::Type *int_type,                                  //
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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of error
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no error
            });
        builder->CreateCondBr(error_happened, error_block, no_error_block, branch_weights);

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

void Generator::Module::Arithmetic::generate_uint_safe_add( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations,                           //
    llvm::Type *int_type,                                   //
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
    auto max = llvm::ConstantInt::get(int_type, -1); // All bits set to 1
    auto diff = builder->CreateSub(max, arg_lhs, "diff");
    auto would_overflow = builder->CreateICmpUGT(arg_rhs, diff, "overflow_check");
    auto sum = builder->CreateAdd(arg_lhs, arg_rhs, "uaddtmp");

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *result = builder->CreateSelect(would_overflow, max, sum, "safe_uaddtmp");
        builder->CreateRet(result);
    } else {
        // Change branch prediction, as no overflow is much more likely to happen than an overflow
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no overflow
            });
        builder->CreateCondBr(would_overflow, overflow_block, no_overflow_block, branch_weights);

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
    llvm::Type *int_type,                                   //
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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of underflow
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no underflow
            });
        builder->CreateCondBr(cmp, no_underflow_block, underflow_block, branch_weights);

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
    llvm::Type *int_type,                                   //
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
    llvm::Value *max = llvm::ConstantInt::get(int_type, -1);
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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no overflow
            });
        builder->CreateCondBr(use_max, overflow_block, no_overflow_block, branch_weights);

        builder->SetInsertPoint(overflow_block);
        llvm::Value *overflow_message = IR::generate_const_string(*builder, name + " mul overflow caught\n");
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
    llvm::Type *int_type,                                   //
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
    llvm::Value *max = llvm::ConstantInt::get(int_type, -1);

    if (overflow_mode == ArithmeticOverflowMode::SILENT) {
        llvm::Value *result = builder->CreateSelect(div_by_zero, max, div, "safe_udivtmp");
        builder->CreateRet(result);
    } else {
        // Change branch prediction, as no error is much more likely to happen than an error
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),                                   //
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 1)),  // weight of error
                llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(builder->getInt32Ty(), 100)) // weight of no error
            });
        builder->CreateCondBr(div_by_zero, error_block, no_error_block, branch_weights);

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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),       //
                llvm::ConstantAsMetadata::get(builder->getInt32(1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(builder->getInt32(100)) // weight of no overflow
            });
        builder->CreateCondBr(any_overflow, overflow_block, no_overflow_block, branch_weights);

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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),       //
                llvm::ConstantAsMetadata::get(builder->getInt32(1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(builder->getInt32(100)) // weight of no overflow
            });
        builder->CreateCondBr(any_overflow, overflow_block, no_overflow_block, branch_weights);

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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),       //
                llvm::ConstantAsMetadata::get(builder->getInt32(1)),  // weight of overflow
                llvm::ConstantAsMetadata::get(builder->getInt32(100)) // weight of no overflow
            });
        builder->CreateCondBr(any_wrong_sign, overflow_block, no_overflow_block, branch_weights);

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
        llvm::MDNode *branch_weights = llvm::MDNode::get(context,
            {
                llvm::MDString::get(context, "branch_weights"),       //
                llvm::ConstantAsMetadata::get(builder->getInt32(1)),  // weight of error
                llvm::ConstantAsMetadata::get(builder->getInt32(100)) // weight of no error
            });
        builder->CreateCondBr(any_error, error_block, no_error_block, branch_weights);

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
