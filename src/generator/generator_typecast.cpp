#include "generator/generator.hpp"

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
    return builder.CreateSExt(int_value, llvm::Type::getInt64Ty(builder.getContext()), "sext");
}

llvm::Value *Generator::TypeCast::i32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    // Compare with 0
    auto zero32 = llvm::ConstantInt::get(int_value->getType(), 0);
    auto is_negative = builder.CreateICmpSLT(int_value, zero32, "is_neg");

    // Zero-extend the value (for positive numbers)
    auto extended = builder.CreateZExt(int_value, llvm::Type::getInt64Ty(builder.getContext()), "zext");
    auto zero64 = llvm::ConstantInt::get(extended->getType(), 0);

    // Select between 0 and the extended value
    return builder.CreateSelect(is_negative, zero64, extended, "safe_i32_to_u64");
}

llvm::Value *Generator::TypeCast::i32_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getFloatTy(builder.getContext()), "sitofp");
}

llvm::Value *Generator::TypeCast::i32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getDoubleTy(builder.getContext()), "sitofp");
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
    return builder.CreateZExt(int_value, llvm::Type::getInt64Ty(builder.getContext()), "zext");
}

llvm::Value *Generator::TypeCast::u32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateZExt(int_value, llvm::Type::getInt64Ty(builder.getContext()), "zext");
}

llvm::Value *Generator::TypeCast::u32_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getFloatTy(builder.getContext()), "uitofp");
}

llvm::Value *Generator::TypeCast::u32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getDoubleTy(builder.getContext()), "uitofp");
}

/******************************************************************************************************************************************
 * @region `I64`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::i64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateTrunc(int_value, llvm::Type::getInt32Ty(builder.getContext()), "trunc");
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
    return builder.CreateTrunc(clamped, llvm::Type::getInt32Ty(builder.getContext()));
}

llvm::Value *Generator::TypeCast::i64_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto zero = llvm::ConstantInt::get(int_value->getType(), 0);
    auto is_negative = builder.CreateICmpSLT(int_value, zero);
    return builder.CreateSelect(is_negative, zero, int_value);
}

llvm::Value *Generator::TypeCast::i64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getFloatTy(builder.getContext()), "sitofp");
}

llvm::Value *Generator::TypeCast::i64_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSIToFP(int_value, llvm::Type::getDoubleTy(builder.getContext()), "sitofp");
}

/******************************************************************************************************************************************
 * @region `U64`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::u64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto int32_max_64 = llvm::ConstantInt::get(int_value->getType(), 0x7FFFFFFF);
    auto too_large = builder.CreateICmpUGT(int_value, int32_max_64);
    auto clamped = builder.CreateSelect(too_large, int32_max_64, int_value);
    return builder.CreateTrunc(clamped, llvm::Type::getInt32Ty(builder.getContext()));
}

llvm::Value *Generator::TypeCast::u64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto uint32_max_64 = llvm::ConstantInt::get(int_value->getType(), 0xFFFFFFFFULL);
    auto too_large = builder.CreateICmpUGT(int_value, uint32_max_64);
    auto clamped = builder.CreateSelect(too_large, uint32_max_64, int_value);
    return builder.CreateTrunc(clamped, llvm::Type::getInt32Ty(builder.getContext()));
}

llvm::Value *Generator::TypeCast::u64_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    auto int64_max = llvm::ConstantInt::get(int_value->getType(), llvm::APInt::getSignedMaxValue(64));
    auto too_large = builder.CreateICmpUGT(int_value, int64_max);
    return builder.CreateSelect(too_large, int64_max, int_value);
}

llvm::Value *Generator::TypeCast::u64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getFloatTy(builder.getContext()), "uitofp");
}

llvm::Value *Generator::TypeCast::u64_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateUIToFP(int_value, llvm::Type::getDoubleTy(builder.getContext()), "uitofp");
}

/******************************************************************************************************************************************
 * @region `F32`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::f32_to_i32(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToSI(float_value, llvm::Type::getInt32Ty(builder.getContext()), "fptosi");
}

llvm::Value *Generator::TypeCast::f32_to_u32(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToUI(float_value, llvm::Type::getInt32Ty(builder.getContext()), "fptoui");
}

llvm::Value *Generator::TypeCast::f32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToSI(float_value, llvm::Type::getInt64Ty(builder.getContext()), "fptosi");
}

llvm::Value *Generator::TypeCast::f32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPToUI(float_value, llvm::Type::getInt64Ty(builder.getContext()), "fptoui");
}

llvm::Value *Generator::TypeCast::f32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *float_value) {
    return builder.CreateFPExt(float_value, llvm::Type::getDoubleTy(builder.getContext()), "fpext");
}

/******************************************************************************************************************************************
 * @region `F64`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::f64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToSI(double_value, llvm::Type::getInt32Ty(builder.getContext()), "fptosi");
}

llvm::Value *Generator::TypeCast::f64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToUI(double_value, llvm::Type::getInt32Ty(builder.getContext()), "fptoui");
}

llvm::Value *Generator::TypeCast::f64_to_i64(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToSI(double_value, llvm::Type::getInt64Ty(builder.getContext()), "fptosi");
}

llvm::Value *Generator::TypeCast::f64_to_u64(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPToUI(double_value, llvm::Type::getInt64Ty(builder.getContext()), "fptoui");
}

llvm::Value *Generator::TypeCast::f64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *double_value) {
    return builder.CreateFPTrunc(double_value, llvm::Type::getFloatTy(builder.getContext()), "fptrunc");
}
