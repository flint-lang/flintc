#include "generator/generator.hpp"

llvm::Value *Generator::Logical::generate_not(llvm::IRBuilder<> &builder, llvm::Value *value_to_negate) {
    if (value_to_negate->getType()->isIntegerTy(1)) {
        // For i1 types, we can use direct NOT
        return builder.CreateNot(value_to_negate, "not");
    }
    // For other integer types
    // NOT is implemented as XOR with true (-1) in LLVM IR
    // or as comparing the value with false (0)

    // Method 1: Using XOR
    // return builder.CreateXor(value_to_negate, llvm::ConstantInt::get(value_to_negate->getType(), 1));

    // Method 2: Using ICmpEQ (preferred for boolean operations)
    return builder.CreateICmpEQ(value_to_negate, llvm::ConstantInt::get(value_to_negate->getType(), 0), "not");
}
