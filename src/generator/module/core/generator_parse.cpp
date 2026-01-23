#include "generator/generator.hpp"
#include "lexer/builtins.hpp"
#include <cstdlib>

static const Hash hash(std::string("parse"));
static const std::string hash_str = hash.to_string();

void Generator::Module::Parse::generate_parse_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_parse_uint_function(builder, module, only_declarations, 8);
    generate_parse_int_function(builder, module, only_declarations, 32);
    generate_parse_uint_function(builder, module, only_declarations, 32);
    generate_parse_int_function(builder, module, only_declarations, 64);
    generate_parse_uint_function(builder, module, only_declarations, 64);
    generate_parse_f32_function(builder, module, only_declarations);
    generate_parse_f64_function(builder, module, only_declarations);
}

void Generator::Module::Parse::generate_parse_int_function( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations,                           //
    const size_t bit_width                                  //
) {
    // THE C IMPLEMENTATION:
    // intN_t parse_iN(const str* input) {
    //     long len = input->len;
    //     char *endptr = NULL;
    //     errno = 0;
    //     long value = strtol(&input->value, &endptr, 10);
    //     if (endptr < &input->value + len) {
    //         printf("Not whole buffer read!\n");
    //         abort();
    //     }
    //     if (errno == ERANGE) {
    //         printf("Int value out of bounds!\n");
    //         abort();
    //     }
    //     if (value <= MIN(iN)) {
    //         printf("Int out of bounds of target type!\n");
    //         abort();
    //     }
    //     if (value >= MAX(iN)) {
    //         printf("Int out of bounds of target type!\n");
    //         abort();
    //     }
    //     return (intN_t)value;
    // }
    llvm::Function *strtol_fn = c_functions.at(STRTOL);
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;

    const std::shared_ptr<Type> result_type_ptr = Type::get_primitive_type("i" + std::to_string(bit_width));
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrParse = hash.get_type_id_from_str("ErrParse");
    const std::vector<error_value> &ErrParseValues = std::get<2>(core_module_error_sets.at("parse").front());
    const unsigned int OutOfBounds = 0;
    const unsigned int InvalidCharacter = 1;
    const std::string OutOfBoundsMessage(ErrParseValues.at(OutOfBounds).second);
    const std::string InvalidCharacterMessage(ErrParseValues.at(InvalidCharacter).second);
    llvm::FunctionType *parse_uN_type = llvm::FunctionType::get(function_result_type, {str_type->getPointerTo()}, false);
    llvm::Function *parse_uN_fn = llvm::Function::Create(  //
        parse_uN_type,                                     //
        llvm::Function::ExternalLinkage,                   //
        hash_str + ".parse_i" + std::to_string(bit_width), //
        module                                             //
    );
    const std::string fn_name = std::string("parse_i" + std::to_string(bit_width));
    parse_functions[fn_name] = parse_uN_fn;
    if (only_declarations) {
        return;
    }

    // Get the input argument
    llvm::Argument *arg_input = parse_uN_fn->arg_begin();
    arg_input->setName("input");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", parse_uN_fn);
    llvm::BasicBlock *parse_error_block = llvm::BasicBlock::Create(context, "parse_error", parse_uN_fn);
    llvm::BasicBlock *errno_check_block = llvm::BasicBlock::Create(context, "errno_check", parse_uN_fn);
    llvm::BasicBlock *errno_fail_block = llvm::BasicBlock::Create(context, "errno_fail", parse_uN_fn);
    llvm::BasicBlock *parse_ok_block = llvm::BasicBlock::Create(context, "parse_ok");
    llvm::BasicBlock *below_min_block = llvm::BasicBlock::Create(context, "below_min");
    llvm::BasicBlock *above_min_block = llvm::BasicBlock::Create(context, "above_min");
    llvm::BasicBlock *above_max_block = llvm::BasicBlock::Create(context, "above_max");
    if (bit_width < 64) {
        parse_ok_block->insertInto(parse_uN_fn);
        below_min_block->insertInto(parse_uN_fn);
        above_min_block->insertInto(parse_uN_fn);
        above_max_block->insertInto(parse_uN_fn);
    }
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", parse_uN_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the length of the input string
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_input, 0);
    llvm::Value *len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Create endptr variable: char *endptr = NULL
    llvm::Value *endptr_ptr = builder->CreateAlloca(builder->getInt8Ty()->getPointerTo(), nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(builder->getInt8Ty()->getPointerTo()), endptr_ptr);

    // Call strtol: long value = strtol(input.c_str, &endptr, 10)
    llvm::Value *input_cstr = builder->CreateStructGEP(str_type, arg_input, 1);
    llvm::Constant *errno_const = module->getOrInsertGlobal("errno", builder->getInt32Ty());
    llvm::GlobalVariable *errno_addr = llvm::cast<llvm::GlobalVariable>(errno_const);
    errno_addr->setThreadLocalMode(llvm::GlobalValue::GeneralDynamicTLSModel);
    IR::aligned_store(*builder, builder->getInt32(0), errno_addr);
    llvm::Value *value = builder->CreateCall(strtol_fn, {input_cstr, endptr_ptr, builder->getInt32(10)}, "value");

    // Load the endptr value after strtol call
    llvm::Value *endptr = IR::aligned_load(*builder, builder->getInt8Ty()->getPointerTo(), endptr_ptr, "endptr");

    // Calculate input.c_str + len (end of the input c string)
    llvm::Value *input_end = builder->CreateGEP(builder->getInt8Ty(), input_cstr, len, "cstr_end");

    // Check if endptr < input.c_str + len (not all input was parsed)
    llvm::Value *endptr_lt_end = builder->CreateICmpULT(endptr, input_end, "endptr_lt_end");

    // Branch if an parse error occured
    builder->CreateCondBr(endptr_lt_end, parse_error_block, errno_check_block);

    // Parse error: throw ErrParse.InvalidCharacter
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    llvm::Value *parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrParse, InvalidCharacter, InvalidCharacterMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Check if errno had an error, e.g. if the input was outside the range of an `i64`
    builder->SetInsertPoint(errno_check_block);
    llvm::Value *errno_val = builder->CreateLoad(builder->getInt32Ty(), errno_addr);
    llvm::Value *is_range_error = builder->CreateICmpEQ(errno_val, builder->getInt32(ERANGE));
    if (bit_width < 64) {
        builder->CreateCondBr(is_range_error, errno_fail_block, parse_ok_block);
    } else {
        builder->CreateCondBr(is_range_error, errno_fail_block, exit_block);
    }

    // Errno error: throw ErrParse.OutOfBounds
    builder->SetInsertPoint(errno_fail_block);
    parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    if (bit_width < 64) {
        // Parsing was okay, check if the value is below MIN(iN)
        builder->SetInsertPoint(parse_ok_block);
        llvm::Value *min = llvm::ConstantInt::get(builder->getInt64Ty(), llvm::APInt::getSignedMinValue(bit_width).sext(64));
        llvm::Value *lt_min = builder->CreateICmpSLT(value, min, "lt_min");
        builder->CreateCondBr(lt_min, below_min_block, above_min_block);

        // Parse error: throw ErrParse.OutOfBounds
        builder->SetInsertPoint(below_min_block);
        parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
        parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
        err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
        IR::aligned_store(*builder, err_value, parse_err_ptr);
        parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
        builder->CreateRet(parse_ret_val);

        builder->SetInsertPoint(above_min_block);
        llvm::Value *max = llvm::ConstantInt::get(builder->getInt64Ty(), llvm::APInt::getSignedMaxValue(bit_width).sext(64));
        llvm::Value *gt_max = builder->CreateICmpSGT(value, max, "gt_max");
        builder->CreateCondBr(gt_max, above_max_block, exit_block);

        // Parse error: throw ErrParse.OutOfBounds
        builder->SetInsertPoint(above_max_block);
        parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
        parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
        err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
        IR::aligned_store(*builder, err_value, parse_err_ptr);
        parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
        builder->CreateRet(parse_ret_val);
    }

    // Exit block: return the uN value
    builder->SetInsertPoint(exit_block);
    llvm::AllocaInst *ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    llvm::Type *int_type = builder->getIntNTy(bit_width);
    llvm::Value *trunc_val = bit_width < 64 ? builder->CreateTrunc(value, int_type) : value;
    IR::aligned_store(*builder, trunc_val, val_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::Parse::generate_parse_uint_function( //
    llvm::IRBuilder<> *builder,                              //
    llvm::Module *module,                                    //
    const bool only_declarations,                            //
    const size_t bit_width                                   //
) {
    // THE C IMPLEMENTATION:
    // uintN_t parse_uN(const str* input) {
    //     long len = input->len;
    //     char *endptr = NULL;
    //     errno = 0;
    //     long value = strtol(&input->value, &endptr, 10);
    //     if (endptr < &input->value + len) {
    //         printf("Not whole buffer read!\n");
    //         abort();
    //     }
    //     if (errno == ERANGE) {
    //         printf("Uint value out of bounds!\n");
    //         abort();
    //     }
    //     if (value < 0) {
    //         printf("Uint values cannot be negative!\n");
    //         abort();
    //     }
    //     if (value >= MAX(uN)) {
    //         printf("Uint out of bounds of target type!\n");
    //         abort();
    //     }
    //     return (uintN_t)value;
    // }
    llvm::Function *strtol_fn = c_functions.at(STRTOL);
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;

    const std::shared_ptr<Type> result_type_ptr = Type::get_primitive_type("u" + std::to_string(bit_width));
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrParse = hash.get_type_id_from_str("ErrParse");
    const std::vector<error_value> &ErrParseValues = std::get<2>(core_module_error_sets.at("parse").front());
    const unsigned int OutOfBounds = 0;
    const unsigned int InvalidCharacter = 1;
    const std::string OutOfBoundsMessage(ErrParseValues.at(OutOfBounds).second);
    const std::string InvalidCharacterMessage(ErrParseValues.at(InvalidCharacter).second);
    llvm::FunctionType *parse_uN_type = llvm::FunctionType::get(function_result_type, {str_type->getPointerTo()}, false);
    llvm::Function *parse_uN_fn = llvm::Function::Create(  //
        parse_uN_type,                                     //
        llvm::Function::ExternalLinkage,                   //
        hash_str + ".parse_u" + std::to_string(bit_width), //
        module                                             //
    );
    const std::string fn_name = std::string("parse_u" + std::to_string(bit_width));
    parse_functions[fn_name] = parse_uN_fn;
    if (only_declarations) {
        return;
    }

    // Get the input argument
    llvm::Argument *arg_input = parse_uN_fn->arg_begin();
    arg_input->setName("input");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", parse_uN_fn);
    llvm::BasicBlock *parse_error_block = llvm::BasicBlock::Create(context, "parse_error", parse_uN_fn);
    llvm::BasicBlock *errno_check_block = llvm::BasicBlock::Create(context, "errno_check", parse_uN_fn);
    llvm::BasicBlock *errno_fail_block = llvm::BasicBlock::Create(context, "errno_fail", parse_uN_fn);
    llvm::BasicBlock *parse_ok_block = llvm::BasicBlock::Create(context, "parse_ok");
    llvm::BasicBlock *below_0_block = llvm::BasicBlock::Create(context, "below_0");
    llvm::BasicBlock *above_0_block = llvm::BasicBlock::Create(context, "above_0");
    llvm::BasicBlock *above_max_block = llvm::BasicBlock::Create(context, "above_max");
    if (bit_width < 64) {
        parse_ok_block->insertInto(parse_uN_fn);
        below_0_block->insertInto(parse_uN_fn);
        above_0_block->insertInto(parse_uN_fn);
        above_max_block->insertInto(parse_uN_fn);
    }
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", parse_uN_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the length of the input string
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_input, 0);
    llvm::Value *len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Create endptr variable: char *endptr = NULL
    llvm::Value *endptr_ptr = builder->CreateAlloca(builder->getInt8Ty()->getPointerTo(), nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(builder->getInt8Ty()->getPointerTo()), endptr_ptr);

    // Call strtol: long value = strtol(input.c_str, &endptr, 10)
    llvm::Value *input_cstr = builder->CreateStructGEP(str_type, arg_input, 1);
    llvm::Constant *errno_const = module->getOrInsertGlobal("errno", builder->getInt32Ty());
    llvm::GlobalVariable *errno_addr = llvm::cast<llvm::GlobalVariable>(errno_const);
    errno_addr->setThreadLocalMode(llvm::GlobalValue::GeneralDynamicTLSModel);
    IR::aligned_store(*builder, builder->getInt32(0), errno_addr);
    llvm::Value *value = builder->CreateCall(strtol_fn, {input_cstr, endptr_ptr, builder->getInt32(10)}, "value");

    // Load the endptr value after strtol call
    llvm::Value *endptr = IR::aligned_load(*builder, builder->getInt8Ty()->getPointerTo(), endptr_ptr, "endptr");

    // Calculate input.c_str + len (end of the input c string)
    llvm::Value *input_end = builder->CreateGEP(builder->getInt8Ty(), input_cstr, len, "cstr_end");

    // Check if endptr < input.c_str + len (not all input was parsed)
    llvm::Value *endptr_lt_end = builder->CreateICmpULT(endptr, input_end, "endptr_lt_end");

    // Branch if an parse error occured
    builder->CreateCondBr(endptr_lt_end, parse_error_block, errno_check_block);

    // Parse error: throw ErrParse.InvalidCharacter
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    llvm::Value *parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrParse, InvalidCharacter, InvalidCharacterMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Check if errno had an error, e.g. if the input was outside the range of an `u64`
    builder->SetInsertPoint(errno_check_block);
    llvm::Value *errno_val = builder->CreateLoad(builder->getInt32Ty(), errno_addr);
    llvm::Value *is_range_error = builder->CreateICmpEQ(errno_val, builder->getInt32(ERANGE));
    if (bit_width < 64) {
        builder->CreateCondBr(is_range_error, errno_fail_block, parse_ok_block);
    } else {
        builder->CreateCondBr(is_range_error, errno_fail_block, exit_block);
    }

    // Errno error: throw ErrParse.OutOfBounds
    builder->SetInsertPoint(errno_fail_block);
    parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    if (bit_width < 64) {
        // Parsing was okay, check if the value is below 0
        builder->SetInsertPoint(parse_ok_block);
        llvm::Value *lt_0 = builder->CreateICmpSLT(value, builder->getInt64(0), "lt_0");
        builder->CreateCondBr(lt_0, below_0_block, above_0_block);

        // Parse error: throw ErrParse.OutOfBounds
        builder->SetInsertPoint(below_0_block);
        parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
        parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
        err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
        IR::aligned_store(*builder, err_value, parse_err_ptr);
        parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
        builder->CreateRet(parse_ret_val);

        builder->SetInsertPoint(above_0_block);
        llvm::Value *max = llvm::ConstantInt::get(builder->getInt64Ty(), llvm::APInt::getMaxValue(bit_width).zext(64));
        llvm::Value *gt_max = builder->CreateICmpSGT(value, max, "gt_max");
        builder->CreateCondBr(gt_max, above_max_block, exit_block);

        // Parse error: throw ErrParse.OutOfBounds
        builder->SetInsertPoint(above_max_block);
        parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
        parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
        err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
        IR::aligned_store(*builder, err_value, parse_err_ptr);
        parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
        builder->CreateRet(parse_ret_val);
    }

    // Exit block: return the uN value
    builder->SetInsertPoint(exit_block);
    llvm::AllocaInst *ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    llvm::Type *int_type = builder->getIntNTy(bit_width);
    llvm::Value *trunc_val = bit_width < 64 ? builder->CreateTrunc(value, int_type) : value;
    IR::aligned_store(*builder, trunc_val, val_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::Parse::generate_parse_f32_function( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations                            //
) {
    // THE C IMPLEMENTATION:
    // float parse_f32(const str* input) {
    //     size_t len = input->len;
    //     char *endptr = NULL;
    //     errno = 0;
    //     float value = strtof(&input->value, &endptr);
    //     if (endptr < &input->value + len) {
    //         printf("Not whole buffer parsed!\n");
    //         abort();
    //     }
    //     if (errno == ERANGE) {
    //         printf("Floating point value out of bounds!\n");
    //         abort();
    //     }
    //     return value;
    // }
    llvm::Function *strtof_fn = c_functions.at(STRTOF);
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;

    const std::shared_ptr<Type> result_type_ptr = Type::get_primitive_type("f32");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrParse = hash.get_type_id_from_str("ErrParse");
    const std::vector<error_value> &ErrParseValues = std::get<2>(core_module_error_sets.at("parse").front());
    const unsigned int OutOfBounds = 0;
    const unsigned int InvalidCharacter = 1;
    const std::string OutOfBoundsMessage(ErrParseValues.at(OutOfBounds).second);
    const std::string InvalidCharacterMessage(ErrParseValues.at(InvalidCharacter).second);
    llvm::FunctionType *parse_f32_type = llvm::FunctionType::get(function_result_type, {str_type->getPointerTo()}, false);
    llvm::Function *parse_f32_fn = llvm::Function::Create( //
        parse_f32_type,                                    //
        llvm::Function::ExternalLinkage,                   //
        hash_str + ".parse_f32",                           //
        module                                             //
    );
    parse_functions["parse_f32"] = parse_f32_fn;
    if (only_declarations) {
        return;
    }

    // Get the input argument
    llvm::Argument *arg_input = parse_f32_fn->arg_begin();
    arg_input->setName("input");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", parse_f32_fn);
    llvm::BasicBlock *parse_error_block = llvm::BasicBlock::Create(context, "parse_error", parse_f32_fn);
    llvm::BasicBlock *errno_check_block = llvm::BasicBlock::Create(context, "errno_check", parse_f32_fn);
    llvm::BasicBlock *errno_fail_block = llvm::BasicBlock::Create(context, "errno_fail", parse_f32_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", parse_f32_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the length of the input string
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_input, 0);
    llvm::Value *len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Create endptr variable: char *endptr = NULL
    llvm::Value *endptr_ptr = builder->CreateAlloca(builder->getInt8Ty()->getPointerTo(), nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(builder->getInt8Ty()->getPointerTo()), endptr_ptr);

    // Call strtof: float value = strtof(input.c_str, &endptr)
    llvm::Value *input_cstr = builder->CreateStructGEP(str_type, arg_input, 1);
    llvm::Constant *errno_const = module->getOrInsertGlobal("errno", builder->getInt32Ty());
    llvm::GlobalVariable *errno_addr = llvm::cast<llvm::GlobalVariable>(errno_const);
    errno_addr->setThreadLocalMode(llvm::GlobalValue::GeneralDynamicTLSModel);
    IR::aligned_store(*builder, builder->getInt32(0), errno_addr);
    llvm::Value *value = builder->CreateCall(strtof_fn, {input_cstr, endptr_ptr}, "value");

    // Load the endptr value after strtof call
    llvm::Value *endptr = IR::aligned_load(*builder, builder->getInt8Ty()->getPointerTo(), endptr_ptr, "endptr");

    // Calculate input.c_str + len (end of the input c string)
    llvm::Value *input_end = builder->CreateGEP(builder->getInt8Ty(), input_cstr, len, "cstr_end");

    // Check if endptr < input.c_str + len (not all input was parsed)
    llvm::Value *endptr_lt_end = builder->CreateICmpULT(endptr, input_end, "endptr_lt_end");

    // Branch if an parse error occured
    builder->CreateCondBr(endptr_lt_end, parse_error_block, errno_check_block);

    // Parse warning: throw ErrParse.InvalidCharacter
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    llvm::Value *parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrParse, InvalidCharacter, InvalidCharacterMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Check if errno had an error, e.g. if the input was outside the range of an `f32`
    builder->SetInsertPoint(errno_check_block);
    llvm::Value *errno_val = builder->CreateLoad(builder->getInt32Ty(), errno_addr);
    llvm::Value *is_range_error = builder->CreateICmpEQ(errno_val, builder->getInt32(ERANGE));
    builder->CreateCondBr(is_range_error, errno_fail_block, exit_block);

    // Errno error: throw ErrParse.OutOfBounds
    builder->SetInsertPoint(errno_fail_block);
    parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Exit block: return the float value
    builder->SetInsertPoint(exit_block);
    llvm::AllocaInst *ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    IR::aligned_store(*builder, value, val_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::Parse::generate_parse_f64_function( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations                            //
) {
    // THE C IMPLEMENTATION:
    // float parse_f64(const str* input) {
    //     size_t len = input->len;
    //     char *endptr = NULL;
    //     errno = 0;
    //     double value = strtod(&input->value, &endptr);
    //     // The whole string should have been parsed
    //     if (endptr < &input->value + len) {
    //         printf("Not whole buffer parsed!\n");
    //         abort();
    //     }
    //     if (errno == ERANGE) {
    //         printf("Floating point value out of bounds!\n");
    //         abort();
    //     }
    //     return value;
    // }
    llvm::Function *strtod_fn = c_functions.at(STRTOD);
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;

    const std::shared_ptr<Type> result_type_ptr = Type::get_primitive_type("f64");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrParse = hash.get_type_id_from_str("ErrParse");
    const std::vector<error_value> &ErrParseValues = std::get<2>(core_module_error_sets.at("parse").front());
    const unsigned int OutOfBounds = 0;
    const unsigned int InvalidCharacter = 1;
    const std::string OutOfBoundsMessage(ErrParseValues.at(OutOfBounds).second);
    const std::string InvalidCharacterMessage(ErrParseValues.at(InvalidCharacter).second);
    llvm::FunctionType *parse_f64_type = llvm::FunctionType::get(function_result_type, {str_type->getPointerTo()}, false);
    llvm::Function *parse_f64_fn = llvm::Function::Create( //
        parse_f64_type,                                    //
        llvm::Function::ExternalLinkage,                   //
        hash_str + ".parse_f64",                           //
        module                                             //
    );
    parse_functions["parse_f64"] = parse_f64_fn;
    if (only_declarations) {
        return;
    }

    // Get the input argument
    llvm::Argument *arg_input = parse_f64_fn->arg_begin();
    arg_input->setName("input");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", parse_f64_fn);
    llvm::BasicBlock *parse_error_block = llvm::BasicBlock::Create(context, "parse_error", parse_f64_fn);
    llvm::BasicBlock *errno_check_block = llvm::BasicBlock::Create(context, "errno_check", parse_f64_fn);
    llvm::BasicBlock *errno_fail_block = llvm::BasicBlock::Create(context, "errno_fail", parse_f64_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", parse_f64_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the length of the input string
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_input, 0);
    llvm::Value *len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Create endptr variable: char *endptr = NULL
    llvm::Value *endptr_ptr = builder->CreateAlloca(builder->getInt8Ty()->getPointerTo(), nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(builder->getInt8Ty()->getPointerTo()), endptr_ptr);

    // Call strtof: float value = strtod(input.c_str, &endptr)
    llvm::Value *input_cstr = builder->CreateStructGEP(str_type, arg_input, 1);
    llvm::Constant *errno_const = module->getOrInsertGlobal("errno", builder->getInt32Ty());
    llvm::GlobalVariable *errno_addr = llvm::cast<llvm::GlobalVariable>(errno_const);
    errno_addr->setThreadLocalMode(llvm::GlobalValue::GeneralDynamicTLSModel);
    IR::aligned_store(*builder, builder->getInt32(0), errno_addr);
    llvm::Value *value = builder->CreateCall(strtod_fn, {input_cstr, endptr_ptr}, "value");

    // Load the endptr value after strtof call
    llvm::Value *endptr = IR::aligned_load(*builder, builder->getInt8Ty()->getPointerTo(), endptr_ptr, "endptr");

    // Calculate input.c_str + len (end of the input c string)
    llvm::Value *input_end = builder->CreateGEP(builder->getInt8Ty(), input_cstr, len, "cstr_end");

    // Check if endptr < input.c_str + len (not all input was parsed)
    llvm::Value *endptr_lt_end = builder->CreateICmpULT(endptr, input_end, "endptr_lt_end");

    // Branch if an parse error occured
    builder->CreateCondBr(endptr_lt_end, parse_error_block, errno_check_block);

    // Parse warning: throw ErrParse.InvalidCharacter
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    llvm::Value *parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrParse, InvalidCharacter, InvalidCharacterMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Check if errno had an error, e.g. if the input was outside the range of an `f64`
    builder->SetInsertPoint(errno_check_block);
    llvm::Value *errno_val = builder->CreateLoad(builder->getInt32Ty(), errno_addr);
    llvm::Value *is_range_error = builder->CreateICmpEQ(errno_val, builder->getInt32(ERANGE));
    builder->CreateCondBr(is_range_error, errno_fail_block, exit_block);

    // Errno error: throw ErrParse.OutOfBounds
    builder->SetInsertPoint(errno_fail_block);
    parse_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrParse, OutOfBounds, OutOfBoundsMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Exit block: return the double value
    builder->SetInsertPoint(exit_block);
    llvm::AllocaInst *ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    IR::aligned_store(*builder, value, val_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}
