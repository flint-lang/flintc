#include "generator/generator.hpp"

void Generator::Module::Math::generate_math_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    math_functions["sin_f32"] = c_functions.at(SINF);
    math_functions["sin_f64"] = c_functions.at(SIN);

    math_functions["cos_f32"] = c_functions.at(COSF);
    math_functions["cos_f64"] = c_functions.at(COS);

    math_functions["sqrt_f32"] = c_functions.at(SQRTF);
    math_functions["sqrt_f64"] = c_functions.at(SQRT);

    llvm::IntegerType *i32_type = llvm::Type::getInt32Ty(context);
    generate_abs_int_function(builder, module, only_declarations, i32_type, "i32");
    llvm::IntegerType *i64_type = llvm::Type::getInt64Ty(context);
    generate_abs_int_function(builder, module, only_declarations, i64_type, "i64");
    math_functions["abs_f32"] = c_functions.at(FABSF);
    math_functions["abs_f64"] = c_functions.at(FABS);
}

void Generator::Module::Math::generate_abs_int_function( //
    llvm::IRBuilder<> *builder,                          //
    llvm::Module *module,                                //
    const bool only_declarations,                        //
    llvm::IntegerType *type,                             //
    const std::string &name                              //
) {
    llvm::Function *abs_fn = nullptr;
    if (type->getBitWidth() == 32) {
        abs_fn = c_functions.at(ABS);
    } else if (type->getBitWidth() == 64) {
        abs_fn = c_functions.at(LABS);
    } else {
        assert(false);
    }

    // Create print function type
    llvm::FunctionType *abs_int_type = llvm::FunctionType::get( //
        type,                                                   // return iX
        {type},                                                 //  iX x
        false                                                   // no vaarg
    );
    // Create the print_int function
    llvm::Function *abs_int_fn = llvm::Function::Create( //
        abs_int_type,                                    //
        llvm::Function::ExternalLinkage,                 //
        "__flint_abs_" + name,                           //
        module                                           //
    );
    const std::string fn_name = "abs_" + name;
    math_functions[fn_name] = abs_int_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameter
    llvm::Argument *arg_x = abs_int_fn->arg_begin();
    arg_x->setName("x");

    // Create basic blocks for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", abs_int_fn);
    llvm::BasicBlock *is_min_block = llvm::BasicBlock::Create(context, "is_min", abs_int_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", abs_int_fn);

    builder->SetInsertPoint(entry_block);

    llvm::Value *int_min = llvm::ConstantInt::get(                                                   //
        builder->getIntNTy(type->getBitWidth()), llvm::APInt::getSignedMinValue(type->getBitWidth()) //
    );
    llvm::Value *is_min = builder->CreateICmpEQ(int_min, arg_x);
    builder->CreateCondBr(is_min, is_min_block, merge_block, IR::generate_weights(1, 100));

    builder->SetInsertPoint(is_min_block);
    llvm::Value *int_max = llvm::ConstantInt::get(                                                   //
        builder->getIntNTy(type->getBitWidth()), llvm::APInt::getSignedMaxValue(type->getBitWidth()) //
    );
    builder->CreateRet(int_max);

    builder->SetInsertPoint(merge_block);
    llvm::Value *abs_value = builder->CreateCall(abs_fn, {arg_x}, "abs_val");
    builder->CreateRet(abs_value);
}
