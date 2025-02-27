#include "generator/generator.hpp"

/**************************************************************************************************************************************
 * @region `I32` / `I64`
 *************************************************************************************************************************************/

llvm::Value *Generator::Arithmetic::int_safe_add(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    // Get type-specific constants
    auto int_min = llvm::ConstantInt::get(lhs->getType(), llvm::APInt::getSignedMinValue(lhs->getType()->getIntegerBitWidth()));
    auto int_max = llvm::ConstantInt::get(lhs->getType(), llvm::APInt::getSignedMaxValue(lhs->getType()->getIntegerBitWidth()));

    // Add with overflow check
    auto add = builder.CreateAdd(lhs, rhs, "iaddtmp");

    // Check for positive overflow:
    // If both numbers are positive and result is negative
    auto lhs_pos = builder.CreateICmpSGE(lhs, llvm::ConstantInt::get(lhs->getType(), 0));
    auto rhs_pos = builder.CreateICmpSGE(rhs, llvm::ConstantInt::get(rhs->getType(), 0));
    auto res_neg = builder.CreateICmpSLT(add, llvm::ConstantInt::get(add->getType(), 0));
    auto pos_overflow = builder.CreateAnd(builder.CreateAnd(lhs_pos, rhs_pos), res_neg);

    // Check for negative overflow:
    // If both numbers are negative and result is positive
    auto lhs_neg = builder.CreateICmpSLT(lhs, llvm::ConstantInt::get(lhs->getType(), 0));
    auto rhs_neg = builder.CreateICmpSLT(rhs, llvm::ConstantInt::get(rhs->getType(), 0));
    auto res_pos = builder.CreateICmpSGE(add, llvm::ConstantInt::get(add->getType(), 0));
    auto neg_overflow = builder.CreateAnd(builder.CreateAnd(lhs_neg, rhs_neg), res_pos);

    // Select appropriate result
    auto use_max = pos_overflow;
    auto use_min = neg_overflow;
    auto normal_case = builder.CreateNot(builder.CreateOr(use_max, use_min));

    auto temp = builder.CreateSelect(use_max, int_max, add);
    return builder.CreateSelect(use_min, int_min, temp);
}

llvm::Value *Generator::Arithmetic::int_safe_sub(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    // Get type-specific constants
    auto int_min = llvm::ConstantInt::get(lhs->getType(), llvm::APInt::getSignedMinValue(lhs->getType()->getIntegerBitWidth()));
    auto int_max = llvm::ConstantInt::get(lhs->getType(), llvm::APInt::getSignedMaxValue(lhs->getType()->getIntegerBitWidth()));

    // Subtract with overflow check
    auto sub = builder.CreateSub(lhs, rhs, "isubtmp");

    // Check for positive overflow:
    // If lhs is positive and rhs is negative and result is negative
    auto lhs_pos = builder.CreateICmpSGE(lhs, llvm::ConstantInt::get(lhs->getType(), 0));
    auto rhs_neg = builder.CreateICmpSLT(rhs, llvm::ConstantInt::get(rhs->getType(), 0));
    auto res_neg = builder.CreateICmpSLT(sub, llvm::ConstantInt::get(sub->getType(), 0));
    auto pos_overflow = builder.CreateAnd(builder.CreateAnd(lhs_pos, rhs_neg), res_neg);

    // Check for negative overflow:
    // If lhs is negative and rhs is positive and result is positive
    auto lhs_neg = builder.CreateICmpSLT(lhs, llvm::ConstantInt::get(lhs->getType(), 0));
    auto rhs_pos = builder.CreateICmpSGE(rhs, llvm::ConstantInt::get(rhs->getType(), 0));
    auto res_pos = builder.CreateICmpSGE(sub, llvm::ConstantInt::get(sub->getType(), 0));
    auto neg_overflow = builder.CreateAnd(builder.CreateAnd(lhs_neg, rhs_pos), res_pos);

    // Select appropriate result
    auto use_max = pos_overflow;
    auto use_min = neg_overflow;
    auto normal_case = builder.CreateNot(builder.CreateOr(use_max, use_min));

    auto temp = builder.CreateSelect(use_max, int_max, sub);
    return builder.CreateSelect(use_min, int_min, temp);
}

llvm::Value *Generator::Arithmetic::int_safe_mul(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    auto int_min = llvm::ConstantInt::get(lhs->getType(), llvm::APInt::getSignedMinValue(lhs->getType()->getIntegerBitWidth()));
    auto int_max = llvm::ConstantInt::get(lhs->getType(), llvm::APInt::getSignedMaxValue(lhs->getType()->getIntegerBitWidth()));
    auto zero = llvm::ConstantInt::get(lhs->getType(), 0);

    // Basic multiplication
    auto mult = builder.CreateMul(lhs, rhs, "imultmp");

    // Check signs
    auto lhs_is_neg = builder.CreateICmpSLT(lhs, zero);
    auto rhs_is_neg = builder.CreateICmpSLT(rhs, zero);
    auto result_should_be_pos = builder.CreateICmpEQ(lhs_is_neg, rhs_is_neg);

    // Check if result has wrong sign (indicates overflow)
    auto result_is_neg = builder.CreateICmpSLT(mult, zero);
    auto wrong_sign = builder.CreateICmpNE(result_should_be_pos, builder.CreateNot(result_is_neg));

    // Select appropriate result
    auto use_max = builder.CreateAnd(wrong_sign, result_should_be_pos);
    auto use_min = builder.CreateAnd(wrong_sign, builder.CreateNot(result_should_be_pos));

    auto temp = builder.CreateSelect(use_max, int_max, mult);
    return builder.CreateSelect(use_min, int_min, temp);
}

llvm::Value *Generator::Arithmetic::int_safe_div(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    // Check for division by zero and MIN_INT/-1
    auto zero = llvm::ConstantInt::get(lhs->getType(), 0);
    auto minus_one = llvm::ConstantInt::get(lhs->getType(), -1);
    auto min_int = llvm::ConstantInt::get(lhs->getType(), llvm::APInt::getSignedMinValue(lhs->getType()->getIntegerBitWidth()));

    auto div_by_zero = builder.CreateICmpEQ(rhs, zero);
    auto is_min_int = builder.CreateICmpEQ(lhs, min_int);
    auto div_by_minus_one = builder.CreateICmpEQ(rhs, minus_one);
    auto would_overflow = builder.CreateAnd(is_min_int, div_by_minus_one);

    auto div = builder.CreateSDiv(lhs, rhs, "idivtmp");

    // Create phi node or basic blocks for proper error handling
    // This is simplified - you might want to add proper error handling
    return builder.CreateSelect(                       //
        builder.CreateOr(div_by_zero, would_overflow), //
        lhs,                                           // or some error value
        div                                            //
    );
}

/**************************************************************************************************************************************
 * @region `U32` / `U64`
 *************************************************************************************************************************************/

llvm::Value *Generator::Arithmetic::uint_safe_add(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    // Check if sum would overflow: if rhs > max - lhs
    auto max = llvm::ConstantInt::get(lhs->getType(), -1); // All bits set to 1
    auto diff = builder.CreateSub(max, lhs, "diff");
    auto would_overflow = builder.CreateICmpUGT(rhs, diff, "overflow_check");
    auto sum = builder.CreateAdd(lhs, rhs, "uaddtmp");
    return builder.CreateSelect(would_overflow, max, sum, "safe_uaddtmp");
}

llvm::Value *Generator::Arithmetic::uint_safe_sub(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    // compare if lhs >= rhs
    auto cmp = builder.CreateICmpUGE(lhs, rhs, "cmp");
    // Perform normal subtraction
    auto sub = builder.CreateSub(lhs, rhs, "usubtmp");
    // Select between 0 and subtraction result based on comparison
    auto zero = llvm::ConstantInt::get(lhs->getType(), 0);
    return builder.CreateSelect(cmp, sub, zero, "safe_usubtmp");
}

llvm::Value *Generator::Arithmetic::uint_safe_mul(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    // Check if either operand is 0
    auto zero = llvm::ConstantInt::get(lhs->getType(), 0);
    auto is_zero = builder.CreateOr(builder.CreateICmpEQ(lhs, zero), builder.CreateICmpEQ(rhs, zero));

    // Check for overflow: if rhs > max/lhs
    auto max = llvm::ConstantInt::get(lhs->getType(), -1);
    auto udiv = builder.CreateUDiv(max, lhs);
    auto would_overflow = builder.CreateICmpUGT(rhs, udiv);

    // Combine checks
    auto use_max = builder.CreateAnd(builder.CreateNot(is_zero), would_overflow);

    auto mult = builder.CreateMul(lhs, rhs, "umultmp");
    return builder.CreateSelect(use_max, max, mult, "safe_umultmp");
}

llvm::Value *Generator::Arithmetic::uint_safe_div(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs) {
    // Check for division by zero
    auto zero = llvm::ConstantInt::get(lhs->getType(), 0);
    auto div_by_zero = builder.CreateICmpEQ(rhs, zero);

    auto div = builder.CreateUDiv(lhs, rhs, "udivtmp");
    auto max = llvm::ConstantInt::get(lhs->getType(), -1);

    return builder.CreateSelect(div_by_zero, max, div, "safe_udivtmp");
}
