#include "generator/generator.hpp"

static const Hash hash(std::string("math"));
static const std::string prefix = hash.to_string() + ".math.";

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

    llvm::Type *f32_type = llvm::Type::getFloatTy(context);
    llvm::Type *f64_type = llvm::Type::getDoubleTy(context);
    generate_min_function(builder, module, only_declarations, i32_type, "u32");
    generate_min_function(builder, module, only_declarations, i64_type, "u64");
    generate_min_function(builder, module, only_declarations, i32_type, "i32");
    generate_min_function(builder, module, only_declarations, i64_type, "i64");
    generate_fmin_function(builder, module, only_declarations, f32_type, "f32");
    generate_fmin_function(builder, module, only_declarations, f64_type, "f64");

    generate_max_function(builder, module, only_declarations, i32_type, "u32");
    generate_max_function(builder, module, only_declarations, i64_type, "u64");
    generate_max_function(builder, module, only_declarations, i32_type, "i32");
    generate_max_function(builder, module, only_declarations, i64_type, "i64");
    generate_fmax_function(builder, module, only_declarations, f32_type, "f32");
    generate_fmax_function(builder, module, only_declarations, f64_type, "f64");
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

    // Create function type
    llvm::FunctionType *abs_int_type = llvm::FunctionType::get( //
        type,                                                   // return iX
        {type},                                                 //  iX x
        false                                                   // no vaarg
    );
    // Create the function
    llvm::Function *abs_int_fn = llvm::Function::Create( //
        abs_int_type,                                    //
        llvm::Function::ExternalLinkage,                 //
        prefix + "abs_" + name,                          //
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

void Generator::Module::Math::generate_min_function( //
    llvm::IRBuilder<> *builder,                      //
    llvm::Module *module,                            //
    const bool only_declarations,                    //
    llvm::Type *type,                                //
    const std::string &name                          //
) {
    // Create function type
    llvm::FunctionType *min_type = llvm::FunctionType::get( //
        type,                                               // return type
        {type, type},                                       //  type x, type y
        false                                               // no vaarg
    );
    // Create the function
    llvm::Function *min_fn = llvm::Function::Create( //
        min_type,                                    //
        llvm::Function::ExternalLinkage,             //
        prefix + "min_" + name,                      //
        module                                       //
    );
    const std::string fn_name = "min_" + name;
    math_functions[fn_name] = min_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *arg_x = min_fn->arg_begin();
    arg_x->setName("x");

    llvm::Argument *arg_y = min_fn->arg_begin() + 1;
    arg_y->setName("y");

    // Create basic blocks for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", min_fn);
    llvm::BasicBlock *x_less_block = llvm::BasicBlock::Create(context, "x_less", min_fn);
    llvm::BasicBlock *y_less_block = llvm::BasicBlock::Create(context, "y_less", min_fn);

    builder->SetInsertPoint(entry_block);
    llvm::Value *x_lt_y = nullptr;
    switch (name[0]) {
        case 'u':
            x_lt_y = builder->CreateICmpULT(arg_x, arg_y);
            break;
        case 'i':
            x_lt_y = builder->CreateICmpSLT(arg_x, arg_y);
            break;
        default:
            assert(false);
            break;
    }
    builder->CreateCondBr(x_lt_y, x_less_block, y_less_block);

    builder->SetInsertPoint(x_less_block);
    builder->CreateRet(arg_x);

    builder->SetInsertPoint(y_less_block);
    builder->CreateRet(arg_y);
}

void Generator::Module::Math::generate_fmin_function( //
    llvm::IRBuilder<> *builder,                       //
    llvm::Module *module,                             //
    const bool only_declarations,                     //
    llvm::Type *type,                                 //
    const std::string &name                           //
) {
    // Create function type
    llvm::FunctionType *min_type = llvm::FunctionType::get( //
        type,                                               // return type
        {type, type},                                       //  type x, type y
        false                                               // no vaarg
    );
    // Create the function
    llvm::Function *min_fn = llvm::Function::Create( //
        min_type,                                    //
        llvm::Function::ExternalLinkage,             //
        prefix + "min_" + name,                      //
        module                                       //
    );
    const std::string fn_name = "min_" + name;
    math_functions[fn_name] = min_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *arg_x = min_fn->arg_begin();
    arg_x->setName("x");

    llvm::Argument *arg_y = min_fn->arg_begin() + 1;
    arg_y->setName("y");

    // Create basic blocks for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", min_fn);
    llvm::BasicBlock *y_nan_block = llvm::BasicBlock::Create(context, "y_nan", min_fn);
    llvm::BasicBlock *check_block = llvm::BasicBlock::Create(context, "check", min_fn);
    llvm::BasicBlock *x_less_block = llvm::BasicBlock::Create(context, "x_less", min_fn);
    llvm::BasicBlock *y_less_block = llvm::BasicBlock::Create(context, "y_less", min_fn);

    builder->SetInsertPoint(entry_block);
    // Check if y is nan by comparing it to itself
    llvm::Value *is_nan = builder->CreateFCmpONE(arg_y, arg_y);
    builder->CreateCondBr(is_nan, y_nan_block, check_block, IR::generate_weights(1, 100));

    builder->SetInsertPoint(y_nan_block);
    builder->CreateRet(arg_x);

    builder->SetInsertPoint(check_block);
    llvm::Value *x_lt_y = builder->CreateFCmpOLT(arg_x, arg_y);

    builder->CreateCondBr(x_lt_y, x_less_block, y_less_block);

    builder->SetInsertPoint(x_less_block);
    builder->CreateRet(arg_x);

    builder->SetInsertPoint(y_less_block);
    builder->CreateRet(arg_y);
}

void Generator::Module::Math::generate_max_function( //
    llvm::IRBuilder<> *builder,                      //
    llvm::Module *module,                            //
    const bool only_declarations,                    //
    llvm::Type *type,                                //
    const std::string &name                          //
) {
    // Create function type
    llvm::FunctionType *max_type = llvm::FunctionType::get( //
        type,                                               // return type
        {type, type},                                       //  type x, type y
        false                                               // no vaarg
    );
    // Create the function
    llvm::Function *max_fn = llvm::Function::Create( //
        max_type,                                    //
        llvm::Function::ExternalLinkage,             //
        prefix + "max_" + name,                      //
        module                                       //
    );
    const std::string fn_name = "max_" + name;
    math_functions[fn_name] = max_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *arg_x = max_fn->arg_begin();
    arg_x->setName("x");

    llvm::Argument *arg_y = max_fn->arg_begin() + 1;
    arg_y->setName("y");

    // Create basic blocks for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", max_fn);
    llvm::BasicBlock *x_greater_block = llvm::BasicBlock::Create(context, "x_greater", max_fn);
    llvm::BasicBlock *y_greater_block = llvm::BasicBlock::Create(context, "y_greater", max_fn);

    builder->SetInsertPoint(entry_block);
    llvm::Value *x_gt_y = nullptr;
    switch (name[0]) {
        case 'u':
            x_gt_y = builder->CreateICmpUGT(arg_x, arg_y);
            break;
        case 'i':
            x_gt_y = builder->CreateICmpSGT(arg_x, arg_y);
            break;
        default:
            assert(false);
            break;
    }
    builder->CreateCondBr(x_gt_y, x_greater_block, y_greater_block);

    builder->SetInsertPoint(x_greater_block);
    builder->CreateRet(arg_x);

    builder->SetInsertPoint(y_greater_block);
    builder->CreateRet(arg_y);
}

void Generator::Module::Math::generate_fmax_function( //
    llvm::IRBuilder<> *builder,                       //
    llvm::Module *module,                             //
    const bool only_declarations,                     //
    llvm::Type *type,                                 //
    const std::string &name                           //
) {
    // Create function type
    llvm::FunctionType *max_type = llvm::FunctionType::get( //
        type,                                               // return type
        {type, type},                                       //  type x, type y
        false                                               // no vaarg
    );
    // Create the function
    llvm::Function *max_fn = llvm::Function::Create( //
        max_type,                                    //
        llvm::Function::ExternalLinkage,             //
        prefix + "max_" + name,                      //
        module                                       //
    );
    const std::string fn_name = "max_" + name;
    math_functions[fn_name] = max_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *arg_x = max_fn->arg_begin();
    arg_x->setName("x");

    llvm::Argument *arg_y = max_fn->arg_begin() + 1;
    arg_y->setName("y");

    // Create basic blocks for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", max_fn);
    llvm::BasicBlock *y_nan_block = llvm::BasicBlock::Create(context, "y_nan", max_fn);
    llvm::BasicBlock *check_block = llvm::BasicBlock::Create(context, "check", max_fn);
    llvm::BasicBlock *x_greater_block = llvm::BasicBlock::Create(context, "x_greater", max_fn);
    llvm::BasicBlock *y_greater_block = llvm::BasicBlock::Create(context, "y_greater", max_fn);

    builder->SetInsertPoint(entry_block);
    // Check if y is nan by comparing it to itself
    llvm::Value *is_nan = builder->CreateFCmpONE(arg_y, arg_y);
    builder->CreateCondBr(is_nan, y_nan_block, check_block, IR::generate_weights(1, 100));

    builder->SetInsertPoint(y_nan_block);
    builder->CreateRet(arg_x);

    builder->SetInsertPoint(check_block);
    llvm::Value *x_gt_y = builder->CreateFCmpOGT(arg_x, arg_y);

    builder->CreateCondBr(x_gt_y, x_greater_block, y_greater_block);

    builder->SetInsertPoint(x_greater_block);
    builder->CreateRet(arg_x);

    builder->SetInsertPoint(y_greater_block);
    builder->CreateRet(arg_y);
}
