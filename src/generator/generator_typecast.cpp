#include "generator/generator.hpp"

/******************************************************************************************************************************************
 * @region `I32`
 *****************************************************************************************************************************************/

llvm::Value *Generator::TypeCast::i32_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateBitCast(int_value, llvm::Type::getInt32Ty(builder.getContext()), "bitcast");
}

llvm::Value *Generator::TypeCast::i32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSExt(int_value, llvm::Type::getInt64Ty(builder.getContext()), "sext");
}

llvm::Value *Generator::TypeCast::i32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateSExt(int_value, llvm::Type::getInt64Ty(builder.getContext()), "sext");
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
    return builder.CreateBitCast(int_value, llvm::Type::getInt32Ty(builder.getContext()), "bitcast");
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
    return builder.CreateTrunc(int_value, llvm::Type::getInt32Ty(builder.getContext()), "trunc");
}

llvm::Value *Generator::TypeCast::i64_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateBitCast(int_value, llvm::Type::getInt64Ty(builder.getContext()), "bitcast");
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
    return builder.CreateTrunc(int_value, llvm::Type::getInt32Ty(builder.getContext()), "trunc");
}

llvm::Value *Generator::TypeCast::u64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateTrunc(int_value, llvm::Type::getInt32Ty(builder.getContext()), "trunc");
}

llvm::Value *Generator::TypeCast::u64_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value) {
    return builder.CreateBitCast(int_value, llvm::Type::getInt64Ty(builder.getContext()), "bitcast");
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
