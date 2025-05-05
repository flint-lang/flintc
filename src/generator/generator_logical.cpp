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

llvm::Value *Generator::Logical::generate_string_cmp_lt( //
    llvm::IRBuilder<> &builder,                          //
    llvm::Value *lhs,                                    //
    const ExpressionNode *lhs_expr,                      //
    llvm::Value *rhs,                                    //
    const ExpressionNode *rhs_expr                       //
) {
    llvm::Function *compare_str_fn = Module::String::string_manip_functions.at("compare_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    // First, make sure that both sides are actual 'str*' variables. If the lhs or rhs is a literal, we first call 'init_str'
    llvm::Value *lhs_val = lhs;
    llvm::Value *rhs_val = rhs;
    if (const auto *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr)) {
        lhs_val = builder.CreateCall(init_str_fn, {lhs, builder.getInt64(std::get<std::string>(lhs_lit->value).length())}, "lhs_str_lt");
    }
    if (const auto *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr)) {
        rhs_val = builder.CreateCall(init_str_fn, {rhs, builder.getInt64(std::get<std::string>(rhs_lit->value).length())}, "rhs_str_lt");
    }

    // Call compare_str(lhs, rhs)
    llvm::Value *compare_result = builder.CreateCall(compare_str_fn, {lhs_val, rhs_val}, "str_cmp_result");

    // Return compare_result < 0
    return builder.CreateICmpSLT(compare_result, builder.getInt32(0), "str_lt_result");
}

llvm::Value *Generator::Logical::generate_string_cmp_gt( //
    llvm::IRBuilder<> &builder,                          //
    llvm::Value *lhs,                                    //
    const ExpressionNode *lhs_expr,                      //
    llvm::Value *rhs,                                    //
    const ExpressionNode *rhs_expr                       //
) {
    llvm::Function *compare_str_fn = Module::String::string_manip_functions.at("compare_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    // First, make sure that both sides are actual 'str*' variables. If the lhs or rhs is a literal, we first call 'init_str'
    llvm::Value *lhs_val = lhs;
    llvm::Value *rhs_val = rhs;
    if (const auto *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr)) {
        lhs_val = builder.CreateCall(init_str_fn, {lhs, builder.getInt64(std::get<std::string>(lhs_lit->value).length())}, "lhs_str_gt");
    }
    if (const auto *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr)) {
        rhs_val = builder.CreateCall(init_str_fn, {rhs, builder.getInt64(std::get<std::string>(rhs_lit->value).length())}, "rhs_str_gt");
    }

    // Call compare_str(lhs, rhs)
    llvm::Value *compare_result = builder.CreateCall(compare_str_fn, {lhs_val, rhs_val}, "str_cmp_result");

    // Return compare_result > 0
    return builder.CreateICmpSGT(compare_result, builder.getInt32(0), "str_gt_result");
}

llvm::Value *Generator::Logical::generate_string_cmp_le( //
    llvm::IRBuilder<> &builder,                          //
    llvm::Value *lhs,                                    //
    const ExpressionNode *lhs_expr,                      //
    llvm::Value *rhs,                                    //
    const ExpressionNode *rhs_expr                       //
) {
    llvm::Function *compare_str_fn = Module::String::string_manip_functions.at("compare_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    // First, make sure that both sides are actual 'str*' variables. If the lhs or rhs is a literal, we first call 'init_str'
    llvm::Value *lhs_val = lhs;
    llvm::Value *rhs_val = rhs;
    if (const auto *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr)) {
        lhs_val = builder.CreateCall(init_str_fn, {lhs, builder.getInt64(std::get<std::string>(lhs_lit->value).length())}, "lhs_str_le");
    }
    if (const auto *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr)) {
        rhs_val = builder.CreateCall(init_str_fn, {rhs, builder.getInt64(std::get<std::string>(rhs_lit->value).length())}, "rhs_str_le");
    }

    // Call compare_str(lhs, rhs)
    llvm::Value *compare_result = builder.CreateCall(compare_str_fn, {lhs_val, rhs_val}, "str_cmp_result");

    // Return compare_result <= 0
    return builder.CreateICmpSLE(compare_result, builder.getInt32(0), "str_le_result");
}

llvm::Value *Generator::Logical::generate_string_cmp_ge( //
    llvm::IRBuilder<> &builder,                          //
    llvm::Value *lhs,                                    //
    const ExpressionNode *lhs_expr,                      //
    llvm::Value *rhs,                                    //
    const ExpressionNode *rhs_expr                       //
) {
    llvm::Function *compare_str_fn = Module::String::string_manip_functions.at("compare_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    // First, make sure that both sides are actual 'str*' variables. If the lhs or rhs is a literal, we first call 'init_str'
    llvm::Value *lhs_val = lhs;
    llvm::Value *rhs_val = rhs;
    if (const auto *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr)) {
        lhs_val = builder.CreateCall(init_str_fn, {lhs, builder.getInt64(std::get<std::string>(lhs_lit->value).length())}, "lhs_str_ge");
    }
    if (const auto *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr)) {
        rhs_val = builder.CreateCall(init_str_fn, {rhs, builder.getInt64(std::get<std::string>(rhs_lit->value).length())}, "rhs_str_ge");
    }

    // Call compare_str(lhs, rhs)
    llvm::Value *compare_result = builder.CreateCall(compare_str_fn, {lhs_val, rhs_val}, "str_cmp_result");

    // Return compare_result >= 0
    return builder.CreateICmpSGE(compare_result, builder.getInt32(0), "str_ge_result");
}

llvm::Value *Generator::Logical::generate_string_cmp_eq( //
    llvm::IRBuilder<> &builder,                          //
    llvm::Value *lhs,                                    //
    const ExpressionNode *lhs_expr,                      //
    llvm::Value *rhs,                                    //
    const ExpressionNode *rhs_expr                       //
) {
    llvm::Function *compare_str_fn = Module::String::string_manip_functions.at("compare_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    // First, make sure that both sides are actual 'str*' variables. If the lhs or rhs is a literal, we first call 'init_str'
    llvm::Value *lhs_val = lhs;
    llvm::Value *rhs_val = rhs;
    if (const auto *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr)) {
        lhs_val = builder.CreateCall(init_str_fn, {lhs, builder.getInt64(std::get<std::string>(lhs_lit->value).length())}, "lhs_str_eq");
    }
    if (const auto *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr)) {
        rhs_val = builder.CreateCall(init_str_fn, {rhs, builder.getInt64(std::get<std::string>(rhs_lit->value).length())}, "rhs_str_eq");
    }

    // Call compare_str(lhs, rhs)
    llvm::Value *compare_result = builder.CreateCall(compare_str_fn, {lhs_val, rhs_val}, "str_cmp_result");

    // Return compare_result == 0
    return builder.CreateICmpEQ(compare_result, builder.getInt32(0), "str_eq_result");
}

llvm::Value *Generator::Logical::generate_string_cmp_neq( //
    llvm::IRBuilder<> &builder,                           //
    llvm::Value *lhs,                                     //
    const ExpressionNode *lhs_expr,                       //
    llvm::Value *rhs,                                     //
    const ExpressionNode *rhs_expr                        //
) {
    llvm::Function *compare_str_fn = Module::String::string_manip_functions.at("compare_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    // First, make sure that both sides are actual 'str*' variables. If the lhs or rhs is a literal, we first call 'init_str'
    llvm::Value *lhs_val = lhs;
    llvm::Value *rhs_val = rhs;
    if (const auto *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr)) {
        lhs_val = builder.CreateCall(init_str_fn, {lhs, builder.getInt64(std::get<std::string>(lhs_lit->value).length())}, "lhs_str_neq");
    }
    if (const auto *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr)) {
        rhs_val = builder.CreateCall(init_str_fn, {rhs, builder.getInt64(std::get<std::string>(rhs_lit->value).length())}, "rhs_str_neq");
    }

    // Call compare_str(lhs, rhs)
    llvm::Value *compare_result = builder.CreateCall(compare_str_fn, {lhs_val, rhs_val}, "str_cmp_result");

    // Return compare_result != 0
    return builder.CreateICmpNE(compare_result, builder.getInt32(0), "str_neq_result");
}
