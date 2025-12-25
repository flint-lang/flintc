#include "generator/generator.hpp"
#include "lexer/builtins.hpp"

static const Hash hash(std::string("system"));
static const std::string hash_str = hash.to_string();

void Generator::Module::System::generate_system_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_system_command_function(builder, module, only_declarations);
    generate_get_cwd_function(builder, module, only_declarations);
}

void Generator::Module::System::generate_system_command_function( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations                                  //
) {
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
    //     char *c_command = (char *)full_command->value;
    //     FILE *pipe = popen(c_command, "r");
    //     free(full_command);
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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *add_str_lit_fn = String::string_manip_functions.at("add_str_lit");
    llvm::Function *append_lit_fn = String::string_manip_functions.at("append_lit");
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *popen_fn = c_functions.at(POPEN);
    llvm::Function *fgets_fn = c_functions.at(FGETS);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);
    llvm::Function *pclose_fn = c_functions.at(PCLOSE);

    const unsigned int ErrSystem = hash.get_type_id_from_str("ErrSystem");
    const std::vector<error_value> &ErrSystemValues = std::get<2>(core_module_error_sets.at("system").at(0));
    const unsigned int EmptyCommand = 0;
    const unsigned int SpawnFailed = 1;
    const std::string SpawnFailedMessage(ErrSystemValues.at(SpawnFailed).second);
    const std::string EmptyCommandMessage(ErrSystemValues.at(EmptyCommand).second);

    const std::string return_type_str = "(i32, str)";
    std::optional<std::shared_ptr<Type>> result_type_ptr = Type::get_type_from_str(return_type_str);
    if (!result_type_ptr.has_value()) {
        const auto i32_type = Type::get_primitive_type("i32");
        const auto str_type_ptr = Type::get_primitive_type("str");
        const std::vector<std::shared_ptr<Type>> types = {i32_type, str_type_ptr};
        result_type_ptr = std::make_shared<GroupType>(types);
    }
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr.value(), true);
    llvm::FunctionType *system_type = llvm::FunctionType::get( //
        function_result_type,                                  //
        {str_type->getPointerTo()},                            //
        false                                                  // No vaarg
    );
    llvm::Function *system_fn = llvm::Function::Create( //
        system_type,                                    //
        llvm::Function::ExternalLinkage,                //
        hash_str + ".system_command",                   //
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
    llvm::BasicBlock *empty_command_block = llvm::BasicBlock::Create(context, "empty_command", system_fn);
    llvm::BasicBlock *nonempty_command_block = llvm::BasicBlock::Create(context, "nonempty_command", system_fn);
#ifdef __WIN32__
    llvm::BasicBlock *replace_slash_block = llvm::BasicBlock::Create(context, "replace_slash", system_fn);
    llvm::BasicBlock *is_slash_to_replace_block = llvm::BasicBlock::Create(context, "is_slash_to_replace", system_fn);
    llvm::BasicBlock *oob_check_block = llvm::BasicBlock::Create(context, "oob_check", system_fn);
    llvm::BasicBlock *replace_slash_condition_block = llvm::BasicBlock::Create(context, "replace_slash_condition", system_fn);
    llvm::BasicBlock *replace_slash_merge_block = llvm::BasicBlock::Create(context, "replace_slash_merge", system_fn);
#endif
    llvm::BasicBlock *pipe_null_block = llvm::BasicBlock::Create(context, "pipe_null", system_fn);
    llvm::BasicBlock *pipe_valid_block = llvm::BasicBlock::Create(context, "pipe_valid", system_fn);
    llvm::BasicBlock *read_loop_header = llvm::BasicBlock::Create(context, "read_loop_header", system_fn);
    llvm::BasicBlock *read_loop_body = llvm::BasicBlock::Create(context, "read_loop_body", system_fn);
    llvm::BasicBlock *read_loop_exit = llvm::BasicBlock::Create(context, "read_loop_exit", system_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Create result struct on stack
    llvm::AllocaInst *result_struct = builder->CreateAlloca(function_result_type, nullptr, "result_struct");

    // Initialize error value to be empty
    llvm::Value *error_value_ptr = builder->CreateStructGEP(function_result_type, result_struct, 0, "error_value_ptr");
    llvm::StructType *err_type = type_map.at("__flint_type_err");
    llvm::Value *err_struct = IR::get_default_value_of_type(err_type);
    IR::aligned_store(*builder, err_struct, error_value_ptr);

    // Initialize exit_code field to -1
    llvm::Value *exit_code_ptr = builder->CreateStructGEP(function_result_type, result_struct, 1, "exit_code_ptr");
    IR::aligned_store(*builder, builder->getInt32(-1), exit_code_ptr);

    // Create empty string for output
    llvm::Value *empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "empty_str");
    llvm::Value *output_ptr = builder->CreateStructGEP(function_result_type, result_struct, 2, "output_ptr");
    IR::aligned_store(*builder, empty_str, output_ptr);

    // Check if the command is empty
    llvm::Value *arg_command_len_ptr = builder->CreateStructGEP(str_type, arg_command, 0, "command_len_ptr");
    llvm::Value *arg_command_len = IR::aligned_load(*builder, builder->getInt64Ty(), arg_command_len_ptr, "command_len");
    llvm::Value *is_command_empty = builder->CreateICmpEQ(arg_command_len, builder->getInt64(0), "is_command_empty");
    builder->CreateCondBr(is_command_empty, empty_command_block, nonempty_command_block);

    // Handle empty command error, throw ErrSystem.EmptyCommand
    builder->SetInsertPoint(empty_command_block);
    llvm::Value *err_value_empty = IR::generate_err_value(*builder, module, ErrSystem, EmptyCommand, EmptyCommandMessage);
    IR::aligned_store(*builder, err_value_empty, error_value_ptr);
    llvm::Value *result_ret_empty = IR::aligned_load(*builder, function_result_type, result_struct, "result_ret_null");
    builder->CreateRet(result_ret_empty);

    builder->SetInsertPoint(nonempty_command_block);
    llvm::Value *command_to_use = arg_command;
#ifdef __WIN32__
    // Replace all slashes in the command with backslashes as a do-while loop. First copy the argument into the new string value and then
    // modify that string inplace
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::AllocaInst *replace_idx_alloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "replace_idx");
    IR::aligned_store(*builder, builder->getInt64(0), replace_idx_alloca);
    llvm::Value *arg_command_value_ptr = builder->CreateStructGEP(str_type, arg_command, 1, "command_value_ptr");
    llvm::Value *command_copy = builder->CreateCall(init_str_fn, {arg_command_value_ptr, arg_command_len}, "command_copy_value");
    command_to_use = command_copy;
    llvm::Value *command_copy_value_ptr = builder->CreateStructGEP(str_type, command_copy, 1, "command_copy_value_ptr");
    builder->CreateBr(replace_slash_block);

    builder->SetInsertPoint(replace_slash_block);
    llvm::Value *replace_idx_value = IR::aligned_load(*builder, builder->getInt64Ty(), replace_idx_alloca, "replace_idx_value");
    llvm::Value *curr_char_ptr = builder->CreateGEP(builder->getInt8Ty(), command_copy_value_ptr, replace_idx_value, "curr_char_ptr");
    llvm::Value *curr_char = IR::aligned_load(*builder, builder->getInt8Ty(), curr_char_ptr, "curr_char");
    llvm::Value *curr_is_slash = builder->CreateICmpEQ(curr_char, builder->getInt8('/'), "curr_is_slash");
    builder->CreateCondBr(curr_is_slash, is_slash_to_replace_block, oob_check_block);

    builder->SetInsertPoint(is_slash_to_replace_block);
    IR::aligned_store(*builder, builder->getInt8('\\'), curr_char_ptr);
    builder->CreateBr(oob_check_block);

    builder->SetInsertPoint(oob_check_block);
    llvm::Value *next_idx_value = builder->CreateAdd(replace_idx_value, builder->getInt64(1), "next_idx_value");
    llvm::Value *is_oob = builder->CreateICmpEQ(next_idx_value, arg_command_len, "is_oob");
    builder->CreateCondBr(is_oob, replace_slash_merge_block, replace_slash_condition_block);

    builder->SetInsertPoint(replace_slash_condition_block);
    llvm::Value *next_char_ptr = builder->CreateGEP(builder->getInt8Ty(), command_copy_value_ptr, next_idx_value, "next_char_ptr");
    llvm::Value *next_char = IR::aligned_load(*builder, builder->getInt8Ty(), next_char_ptr, "next_char");
    llvm::Value *next_is_space = builder->CreateICmpEQ(next_char, builder->getInt8(' '), "next_is_space");
    IR::aligned_store(*builder, next_idx_value, replace_idx_alloca);
    builder->CreateCondBr(next_is_space, replace_slash_merge_block, replace_slash_block);

    builder->SetInsertPoint(replace_slash_merge_block);
#endif

    // Create command with stderr redirection: full_command = add_str_lit(command, " 2>&1", 5)
    llvm::Value *redirect_str = IR::generate_const_string(module, " 2>&1");
    llvm::Value *full_command = builder->CreateCall(add_str_lit_fn, {command_to_use, redirect_str, builder->getInt64(5)}, "full_command");

    // Get C string: c_command = (char *)full_command->value
    llvm::Value *c_command = builder->CreateStructGEP(str_type, full_command, 1, "c_command");

    // Create "r" string for popen mode
    llvm::Value *mode_str = IR::generate_const_string(module, "r");

    // Open pipe: pipe = popen(c_command, "r")
    llvm::Value *pipe = builder->CreateCall(popen_fn, {c_command, mode_str}, "pipe");

    // Free the full_command
    builder->CreateCall(free_fn, {full_command});

    // Check if pipe is NULL
    llvm::Value *pipe_null_check = builder->CreateIsNull(pipe, "pipe_is_null");
    builder->CreateCondBr(pipe_null_check, pipe_null_block, pipe_valid_block);

    // Handle pipe NULL error, throw ErrSystem.SpawnFailed
    builder->SetInsertPoint(pipe_null_block);
#ifdef __WIN32__
    builder->CreateCall(free_fn, command_copy);
#endif
    llvm::Value *output_load_null = IR::aligned_load(*builder, str_type->getPointerTo(), output_ptr, "output_load_null");
    builder->CreateCall(free_fn, {output_load_null});
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrSystem, SpawnFailed, SpawnFailedMessage);
    IR::aligned_store(*builder, err_value, error_value_ptr);
    IR::aligned_store(*builder, builder->getInt32(0), exit_code_ptr);
    IR::aligned_store(*builder, builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "empty_str"), output_ptr);
    llvm::Value *result_ret_null = IR::aligned_load(*builder, function_result_type, result_struct, "result_ret_null");
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
    llvm::Value *output_load = IR::aligned_load(*builder, str_type->getPointerTo(), output_ptr, "output_load");
    // Append buffer to output: append_lit(&result.output, buffer)
    llvm::Value *output_addr = builder->CreateAlloca(str_type->getPointerTo(), nullptr, "output_addr");
    IR::aligned_store(*builder, output_load, output_addr);
    llvm::Value *buffer_len = builder->CreateCall(strlen_fn, {buffer}, "buffer_len");
    builder->CreateCall(append_lit_fn, {output_addr, buffer, buffer_len});
    // Update the output in result struct
    llvm::Value *updated_output = IR::aligned_load(*builder, str_type->getPointerTo(), output_addr, "updated_output");
    IR::aligned_store(*builder, updated_output, output_ptr);
    // Loop back to read more
    builder->CreateBr(read_loop_header);

    // Read loop exit
    builder->SetInsertPoint(read_loop_exit);
    // Get command exit status: status = pclose(pipe)
    llvm::Value *status = builder->CreateCall(pclose_fn, {pipe}, "status");
    // Extract the low byte: result.exit_code = status & 0xFF
#ifdef __WIN32__
    // Raw exit code on Windows
    llvm::Value *exit_code = status;
#else
    llvm::Value *shifted_status = builder->CreateLShr(status, builder->getInt32(8), "shifted_status");
    llvm::Value *exit_code = builder->CreateAnd(shifted_status, builder->getInt32(0xFF), "exit_code");
#endif
    IR::aligned_store(*builder, exit_code, exit_code_ptr);

    // Return the result struct
    llvm::Value *result_ret = IR::aligned_load(*builder, function_result_type, result_struct, "result_ret");
#ifdef __WIN32__
    builder->CreateCall(free_fn, command_copy);
#endif
    builder->CreateRet(result_ret);
}

void Generator::Module::System::generate_get_cwd_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // str *get_cwd() {
    //     // Maximum path length (PATH_MAX is POSIX, MAX_PATH is Windows)
    //     char buffer[PATH_MAX];
    // #ifdef __WIN32__
    //     if (_getcwd(buffer, sizeof(buffer)) == NULL) {
    // #else
    //     if (getcwd(buffer, sizeof(buffer)) == NULL) {
    // #endif
    //         // Could not get cwd, return empty string or handle error as needed
    //         return create_str(0);
    //     }
    //     return init_str(buffer, strlen(buffer));
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *getcwd_fn = c_functions.at(GETCWD);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);
    llvm::Function *create_str_fn = Module::String::string_manip_functions.at("create_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    llvm::FunctionType *get_cwd_type = llvm::FunctionType::get(str_type->getPointerTo(), {}, false);
    llvm::Function *get_cwd_fn = llvm::Function::Create( //
        get_cwd_type,                                    //
        llvm::Function::ExternalLinkage,                 //
        hash_str + ".get_cwd",                           //
        module                                           //
    );
    system_functions["get_cwd"] = get_cwd_fn;
    if (only_declarations) {
        return;
    }

    // Create the basic blocks for the function
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", get_cwd_fn);
    llvm::BasicBlock *const getcwd_fail_block = llvm::BasicBlock::Create(context, "getcwd_fail", get_cwd_fn);
    llvm::BasicBlock *const getcwd_ok_block = llvm::BasicBlock::Create(context, "getcwd_ok", get_cwd_fn);

    // Allocate the buffer on the stack
    builder->SetInsertPoint(entry_block);
    llvm::AllocaInst *buffer = builder->CreateAlloca(llvm::ArrayType::get(builder->getInt8Ty(), 256), nullptr, "buffer");
    llvm::Value *getcwd_result = builder->CreateCall(getcwd_fn, {buffer, builder->getInt32(256)}, "getcwd_result");
    llvm::Value *nullpointer = llvm::ConstantPointerNull::get(builder->getInt8Ty()->getPointerTo());
    llvm::Value *getcwd_failed = builder->CreateICmpEQ(getcwd_result, nullpointer, "getcwd_failed");
    builder->CreateCondBr(getcwd_failed, getcwd_fail_block, getcwd_ok_block);

    builder->SetInsertPoint(getcwd_fail_block);
    llvm::Value *empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "empty_str");
    builder->CreateRet(empty_str);

    builder->SetInsertPoint(getcwd_ok_block);
    llvm::Value *cwd_str_len = builder->CreateCall(strlen_fn, {buffer}, "cwd_str_len");
    llvm::Value *cwd_str = builder->CreateCall(init_str_fn, {buffer, cwd_str_len}, "cwd_str");
    builder->CreateRet(cwd_str);
}
