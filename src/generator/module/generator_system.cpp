#include "generator/generator.hpp"
#include "lexer/builtins.hpp"

void Generator::Module::System::generate_system_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_system_command_function(builder, module, only_declarations);
}

void Generator::Module::System::generate_system_command_function(llvm::IRBuilder<> *builder, llvm::Module *module,
    const bool only_declarations) {
    // THE C IMPLEMENTATION
    // typedef struct {
    //     int exit_code;
    //     str *output;
    // } CommandResult;
    // static CommandResult system_command(str * command) {
    //     CommandResult result = {-1, NULL};
    //     const size_t BUFFER_SIZE = 4096;
    //     char buffer[BUFFER_SIZE];
    //
    //     // Allocate initial output buffer to be an empty string
    //     result.output = create_str(0);
    //
    //     // Create command with stderr redirection
    //     str *full_command = add_str_lit(command, " 2>&1", 5);
    //     char *c_command = get_c_str(full_command);
    //     free(full_command);
    //
    //     FILE *pipe = popen(c_command, "r");
    //     free(c_command);
    //
    //     if (!pipe) {
    //         free(result.output);
    //         result.output = NULL;
    //         return result;
    //     }
    //
    //     // Read output from pipe
    //     while (fgets(buffer, BUFFER_SIZE, pipe) != NULL) {
    //         // Append buffer to output
    //         int buffer_len = strlen(buffer);
    //         append_lit(&result.output, buffer, buffer_len);
    //     }
    //
    //     // Get command exit status
    //     int status = pclose(pipe);
    //     result.exit_code = status & 0xFF;
    //
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *add_str_lit_fn = String::string_manip_functions.at("add_str_lit");
    llvm::Function *get_c_str_fn = String::string_manip_functions.at("get_c_str");
    llvm::Function *append_lit_fn = String::string_manip_functions.at("append_lit");
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *popen_fn = c_functions.at(POPEN);
    llvm::Function *fgets_fn = c_functions.at(FGETS);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);
    llvm::Function *pclose_fn = c_functions.at(PCLOSE);

    const std::string return_type_str = "(i32, str)";
    std::optional<std::shared_ptr<Type>> result_type_ptr = Type::get_type_from_str(return_type_str);
    if (!result_type_ptr.has_value()) {
        const auto i32_type = Type::get_primitive_type("i32");
        const auto str_type_ptr = Type::get_primitive_type("str");
        const std::vector<std::shared_ptr<Type>> types = {i32_type, str_type_ptr};
        result_type_ptr = std::make_shared<GroupType>(types);
    }
    llvm::StructType *function_result_type = IR::add_and_or_get_type(result_type_ptr.value(), true);
    llvm::FunctionType *system_type = llvm::FunctionType::get( //
        function_result_type,                                  //
        {str_type->getPointerTo()},                            //
        false                                                  // No vaarg
    );
    llvm::Function *system_fn = llvm::Function::Create( //
        system_type,                                    //
        llvm::Function::ExternalLinkage,                //
        "__flint_system_command",                       //
        module                                          //
    );
    system_functions["system_command"] = system_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameters
    llvm::Argument *arg_command = system_fn->arg_begin();
    arg_command->setName("command");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", system_fn);
    llvm::BasicBlock *pipe_null_block = llvm::BasicBlock::Create(context, "pipe_null", system_fn);
    llvm::BasicBlock *pipe_valid_block = llvm::BasicBlock::Create(context, "pipe_valid", system_fn);
    llvm::BasicBlock *read_loop_header = llvm::BasicBlock::Create(context, "read_loop_header", system_fn);
    llvm::BasicBlock *read_loop_body = llvm::BasicBlock::Create(context, "read_loop_body", system_fn);
    llvm::BasicBlock *read_loop_exit = llvm::BasicBlock::Create(context, "read_loop_exit", system_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Create result struct on stack
    llvm::AllocaInst *result_struct = builder->CreateAlloca(function_result_type, nullptr, "result_struct");

    // Initialize error value to 0
    llvm::Value *error_value_ptr = builder->CreateStructGEP(function_result_type, result_struct, 0, "error_value_ptr");
    builder->CreateStore(builder->getInt32(0), error_value_ptr);

    // Initialize exit_code field to -1
    llvm::Value *exit_code_ptr = builder->CreateStructGEP(function_result_type, result_struct, 1, "exit_code_ptr");
    builder->CreateStore(builder->getInt32(-1), exit_code_ptr);

    // Create empty string for output
    llvm::Value *empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "empty_str");
    llvm::Value *output_ptr = builder->CreateStructGEP(function_result_type, result_struct, 2, "output_ptr");
    builder->CreateStore(empty_str, output_ptr);

    // Create command with stderr redirection: full_command = add_str_lit(command, " 2>&1")
    llvm::Value *redirect_str = IR::generate_const_string(*builder, " 2>&1");
    llvm::Value *full_command = builder->CreateCall(add_str_lit_fn, {arg_command, redirect_str, builder->getInt64(5)}, "full_command");

    // Get C string: c_command = get_c_str(full_command)
    llvm::Value *c_command = builder->CreateCall(get_c_str_fn, {full_command}, "c_command");

    // Free the full_command
    builder->CreateCall(free_fn, {full_command});

    // Create "r" string for popen mode
    llvm::Value *mode_str = IR::generate_const_string(*builder, "r");

    // Open pipe: pipe = popen(c_command, "r")
    llvm::Value *pipe = builder->CreateCall(popen_fn, {c_command, mode_str}, "pipe");

    // Free c_command
    builder->CreateCall(free_fn, {c_command});

    // Check if pipe is NULL
    llvm::Value *pipe_null_check = builder->CreateIsNull(pipe, "pipe_is_null");
    builder->CreateCondBr(pipe_null_check, pipe_null_block, pipe_valid_block);

    // Handle pipe NULL error
    builder->SetInsertPoint(pipe_null_block);
    // Free result.output
    llvm::Value *output_load_null = builder->CreateLoad(str_type->getPointerTo(), output_ptr, "output_load_null");
    builder->CreateCall(free_fn, {output_load_null});
    // Store error code in the result type
    builder->CreateStore(builder->getInt32(1), error_value_ptr);
    // Store empty string in the output
    builder->CreateStore(builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "empty_str"), output_ptr);
    // Return the result
    llvm::Value *result_ret_null = builder->CreateLoad(function_result_type, result_struct, "result_ret_null");
    builder->CreateRet(result_ret_null);

    // Continue with valid pipe
    builder->SetInsertPoint(pipe_valid_block);

    // Create buffer for reading: char buffer[BUFFER_SIZE]
    llvm::Value *buffer_size = builder->getInt32(4096); // Set buffer size to 4096
    llvm::AllocaInst *buffer = builder->CreateAlloca(builder->getInt8Ty(), buffer_size, "buffer");

    // Start the read loop
    builder->CreateBr(read_loop_header);

    // Read loop header
    builder->SetInsertPoint(read_loop_header);
    // fgets(buffer, BUFFER_SIZE, pipe)
    llvm::Value *read_result = builder->CreateCall(fgets_fn, {buffer, buffer_size, pipe}, "read_result");
    // Check if fgets returned NULL (end of pipe)
    llvm::Value *read_end_check = builder->CreateIsNull(read_result, "read_end_check");
    builder->CreateCondBr(read_end_check, read_loop_exit, read_loop_body);

    // Read loop body
    builder->SetInsertPoint(read_loop_body);
    // Load the current output
    llvm::Value *output_load = builder->CreateLoad(str_type->getPointerTo(), output_ptr, "output_load");
    // Append buffer to output: append_lit(&result.output, buffer)
    llvm::Value *output_addr = builder->CreateAlloca(str_type->getPointerTo(), nullptr, "output_addr");
    builder->CreateStore(output_load, output_addr);
    llvm::Value *buffer_len = builder->CreateCall(strlen_fn, {buffer}, "buffer_len");
    builder->CreateCall(append_lit_fn, {output_addr, buffer, buffer_len});
    // Update the output in result struct
    llvm::Value *updated_output = builder->CreateLoad(str_type->getPointerTo(), output_addr, "updated_output");
    builder->CreateStore(updated_output, output_ptr);
    // Loop back to read more
    builder->CreateBr(read_loop_header);

    // Read loop exit
    builder->SetInsertPoint(read_loop_exit);
    // Get command exit status: status = pclose(pipe)
    llvm::Value *status = builder->CreateCall(pclose_fn, {pipe}, "status");
    // Extract the low byte: result.exit_code = status & 0xFF
    llvm::Value *shifted_status = builder->CreateLShr(status, builder->getInt32(8), "shifted_status");
    llvm::Value *exit_code = builder->CreateAnd(shifted_status, builder->getInt32(0xFF), "exit_code");
    builder->CreateStore(exit_code, exit_code_ptr);

    // Return the result struct
    llvm::Value *result_ret = builder->CreateLoad(function_result_type, result_struct, "result_ret");
    builder->CreateRet(result_ret);
}
