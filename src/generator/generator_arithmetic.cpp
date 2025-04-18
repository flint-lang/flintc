#include "generator/generator.hpp"

void Generator::Arithmetic::generate_arithmetic_functions(llvm::IRBuilder<> *builder, llvm::Module *module) {
    generate_int_safe_add(builder, module, builder->getInt32Ty(), "i32");
    generate_int_safe_add(builder, module, builder->getInt64Ty(), "i64");
    generate_int_safe_sub(builder, module, builder->getInt32Ty(), "i32");
    generate_int_safe_sub(builder, module, builder->getInt64Ty(), "i64");
    generate_int_safe_mul(builder, module, builder->getInt32Ty(), "i32");
    generate_int_safe_mul(builder, module, builder->getInt64Ty(), "i64");
    generate_int_safe_div(builder, module, builder->getInt32Ty(), "i32");
    generate_int_safe_div(builder, module, builder->getInt64Ty(), "i64");
    generate_uint_safe_add(builder, module, builder->getInt32Ty(), "u32");
    generate_uint_safe_add(builder, module, builder->getInt64Ty(), "u64");
    generate_uint_safe_sub(builder, module, builder->getInt32Ty(), "u32");
    generate_uint_safe_sub(builder, module, builder->getInt64Ty(), "u64");
    generate_uint_safe_mul(builder, module, builder->getInt32Ty(), "u32");
    generate_uint_safe_mul(builder, module, builder->getInt64Ty(), "u64");
    generate_uint_safe_div(builder, module, builder->getInt32Ty(), "u32");
    generate_uint_safe_div(builder, module, builder->getInt64Ty(), "u64");
}

void Generator::Arithmetic::generate_int_safe_add( //
    llvm::IRBuilder<> *builder,                    //
    llvm::Module *module,                          //
    llvm::Type *int_type,                          //
    const std::string &name                        //
) {
    llvm::FunctionType *int_safe_add_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_add_fn = llvm::Function::Create( //
        int_safe_add_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        name + "_safe_add",                                   //
        module                                                //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", int_safe_add_fn);
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
    builder->CreateNot(builder->CreateOr(pos_overflow, neg_overflow));

    llvm::Value *temp = builder->CreateSelect(use_max, int_max, add);
    llvm::Value *selection = builder->CreateSelect(use_min, int_min, temp);
    builder->CreateRet(selection);

    arithmetic_functions[name + "_safe_add"] = int_safe_add_fn;
}

void Generator::Arithmetic::generate_int_safe_sub( //
    llvm::IRBuilder<> *builder,                    //
    llvm::Module *module,                          //
    llvm::Type *int_type,                          //
    const std::string &name                        //
) {
    llvm::FunctionType *int_safe_sub_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_sub_fn = llvm::Function::Create( //
        int_safe_sub_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        name + "_safe_sub",                                   //
        module                                                //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", int_safe_sub_fn);
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
    builder->CreateNot(builder->CreateOr(use_max, use_min));

    llvm::Value *temp = builder->CreateSelect(use_max, int_max, sub);
    llvm::Value *result = builder->CreateSelect(use_min, int_min, temp);
    builder->CreateRet(result);

    arithmetic_functions[name + "_safe_sub"] = int_safe_sub_fn;
}

void Generator::Arithmetic::generate_int_safe_mul( //
    llvm::IRBuilder<> *builder,                    //
    llvm::Module *module,                          //
    llvm::Type *int_type,                          //
    const std::string &name                        //
) {
    llvm::FunctionType *int_safe_mul_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_mul_fn = llvm::Function::Create( //
        int_safe_mul_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        name + "_safe_mul",                                   //
        module                                                //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", int_safe_mul_fn);
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

    llvm::Value *temp = builder->CreateSelect(use_max, int_max, mult);
    llvm::Value *result = builder->CreateSelect(use_min, int_min, temp);
    builder->CreateRet(result);

    arithmetic_functions[name + "_safe_mul"] = int_safe_mul_fn;
}

void Generator::Arithmetic::generate_int_safe_div( //
    llvm::IRBuilder<> *builder,                    //
    llvm::Module *module,                          //
    llvm::Type *int_type,                          //
    const std::string &name                        //
) {
    llvm::FunctionType *int_safe_div_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *int_safe_div_fn = llvm::Function::Create( //
        int_safe_div_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        name + "_safe_div",                                   //
        module                                                //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", int_safe_div_fn);
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

    llvm::Value *div = builder->CreateSDiv(arg_lhs, arg_rhs, "idivtmp");

    // Create phi node or basic blocks for proper error handling
    // This is simplified - you might want to add proper error handling
    llvm::Value *result = builder->CreateSelect(builder->CreateOr(div_by_zero, would_overflow), arg_lhs, div);
    builder->CreateRet(result);

    arithmetic_functions[name + "_safe_div"] = int_safe_div_fn;
}

void Generator::Arithmetic::generate_uint_safe_add( //
    llvm::IRBuilder<> *builder,                     //
    llvm::Module *module,                           //
    llvm::Type *int_type,                           //
    const std::string &name                         //
) {
    llvm::FunctionType *uint_safe_add_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_add_fn = llvm::Function::Create( //
        uint_safe_add_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        name + "_safe_add",                                    //
        module                                                 //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", uint_safe_add_fn);
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
    llvm::Value *result = builder->CreateSelect(would_overflow, max, sum, "safe_uaddtmp");
    builder->CreateRet(result);

    arithmetic_functions[name + "_safe_add"] = uint_safe_add_fn;
}

void Generator::Arithmetic::generate_uint_safe_sub( //
    llvm::IRBuilder<> *builder,                     //
    llvm::Module *module,                           //
    llvm::Type *int_type,                           //
    const std::string &name                         //
) {
    llvm::FunctionType *uint_safe_sub_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_sub_fn = llvm::Function::Create( //
        uint_safe_sub_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        name + "_safe_sub",                                    //
        module                                                 //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", uint_safe_sub_fn);
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
    llvm::Value *result = builder->CreateSelect(cmp, sub, zero, "safe_usubtmp");
    builder->CreateRet(result);

    arithmetic_functions[name + "_safe_sub"] = uint_safe_sub_fn;
}

void Generator::Arithmetic::generate_uint_safe_mul( //
    llvm::IRBuilder<> *builder,                     //
    llvm::Module *module,                           //
    llvm::Type *int_type,                           //
    const std::string &name                         //
) {
    llvm::FunctionType *uint_safe_mul_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_mul_fn = llvm::Function::Create( //
        uint_safe_mul_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        name + "_safe_mul",                                    //
        module                                                 //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", uint_safe_mul_fn);
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
    llvm::Value *result = builder->CreateSelect(use_max, max, mult, "safe_umultmp");
    builder->CreateRet(result);

    arithmetic_functions[name + "_safe_mul"] = uint_safe_mul_fn;
}

void Generator::Arithmetic::generate_uint_safe_div( //
    llvm::IRBuilder<> *builder,                     //
    llvm::Module *module,                           //
    llvm::Type *int_type,                           //
    const std::string &name                         //
) {
    llvm::FunctionType *uint_safe_div_type = llvm::FunctionType::get(int_type, {int_type, int_type}, false);
    llvm::Function *uint_safe_div_fn = llvm::Function::Create( //
        uint_safe_div_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        name + "_safe_div",                                    //
        module                                                 //
    );
    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(builder->getContext(), "entry", uint_safe_div_fn);
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

    llvm::Value *result = builder->CreateSelect(div_by_zero, max, div, "safe_udivtmp");
    builder->CreateRet(result);

    arithmetic_functions[name + "_safe_div"] = uint_safe_div_fn;
}
