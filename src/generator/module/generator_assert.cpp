#include "generator/generator.hpp"

void Generator::Module::Assert::generate_assert_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_assert_function(builder, module, only_declarations);
}

void Generator::Module::Assert::generate_assert_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // void assert(bool condition) {
    //     if (!condition) {
    //         THROW_ERR;
    //     }
    // }

    const std::shared_ptr<Type> result_type_ptr = Type::get_primitive_type("void");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrAssert = Type::get_type_id_from_str("ErrAssert");
    const std::vector<error_value> &ErrAssertValues = std::get<2>(core_module_error_sets.at("assert").at(0));
    const unsigned int AssertionFailed = 0;
    const std::string AssertionFailedMessage(ErrAssertValues.at(AssertionFailed).second);
    llvm::FunctionType *assert_type = llvm::FunctionType::get(function_result_type, {llvm::Type::getInt1Ty(context)}, false);
    llvm::Function *assert_fn = llvm::Function::Create( //
        assert_type,                                    //
        llvm::Function::ExternalLinkage,                //
        "__flint_assert",                               //
        module                                          //
    );
    assert_functions["assert"] = assert_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", assert_fn);
    llvm::BasicBlock *error_block = llvm::BasicBlock::Create(context, "error", assert_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", assert_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the parameter (len)
    llvm::Argument *arg_condition = assert_fn->arg_begin();
    arg_condition->setName("condition");

    // Branch to the exit block when condition is met or to the error block when it is not met
    builder->CreateCondBr(arg_condition, exit_block, error_block);

    // Error block: throw ErrAssert.AssertionFailed
    builder->SetInsertPoint(error_block);
    llvm::AllocaInst *assert_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "assert_ret_alloca", true);
    llvm::Value *assert_err_ptr = builder->CreateStructGEP(function_result_type, assert_ret_alloca, 0, "assert_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, ErrAssert, AssertionFailed, AssertionFailedMessage);
    IR::aligned_store(*builder, err_value, assert_err_ptr);
    llvm::Value *assert_ret_val = IR::aligned_load(*builder, function_result_type, assert_ret_alloca, "assert_ret_val");
    builder->CreateRet(assert_ret_val);

    // Return a default struct for a non-error value
    builder->SetInsertPoint(exit_block);
    llvm::AllocaInst *ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}
