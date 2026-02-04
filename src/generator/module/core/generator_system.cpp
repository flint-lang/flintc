#include "generator/generator.hpp"
#include "lexer/builtins.hpp"

static const Hash hash(std::string("system"));
static const std::string prefix = hash.to_string() + ".system.";

void Generator::Module::System::generate_system_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    if (!only_declarations) {
        llvm::PointerType *const ptr_ty = llvm::PointerType::get(context, 0);
        system_variables["stdout"] = module->getGlobalVariable("stdout");
        if (system_variables.at("stdout") == nullptr) {
            system_variables["stdout"] = new llvm::GlobalVariable(                            //
                *module, ptr_ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, "stdout" //
            );
        }
        system_variables["stderr"] = module->getGlobalVariable("stderr");
        if (system_variables.at("stderr") == nullptr) {
            system_variables["stderr"] = new llvm::GlobalVariable(                            //
                *module, ptr_ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, "stderr" //
            );
        }
        llvm::Type *const i32_ty = llvm::Type::getInt32Ty(context);
        system_variables["orig_stdout_fd"] = new llvm::GlobalVariable(                                             //
            *module, i32_ty, false, llvm::GlobalVariable::InternalLinkage, builder->getInt32(-1), "orig_stdout_fd" //
        );
        system_variables["orig_stderr_fd"] = new llvm::GlobalVariable(                                             //
            *module, i32_ty, false, llvm::GlobalVariable::InternalLinkage, builder->getInt32(-1), "orig_stderr_fd" //
        );
        system_variables["capture_file"] = new llvm::GlobalVariable(                                                              //
            *module, ptr_ty, false, llvm::GlobalVariable::InternalLinkage, llvm::ConstantPointerNull::get(ptr_ty), "capture_file" //
        );
    }
    generate_system_command_function(builder, module, only_declarations);
    generate_get_cwd_function(builder, module, only_declarations);
    generate_get_path_function(builder, module, only_declarations);
    generate_start_capture_function(builder, module, only_declarations);
    generate_end_capture_function(builder, module, only_declarations);
    generate_end_capture_lines_function(builder, module, only_declarations);
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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
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
        prefix + "system_command",                      //
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
    llvm::StructType *err_type = type_map.at("type.flint.err");
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
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *getcwd_fn = c_functions.at(GETCWD);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);
    llvm::Function *create_str_fn = Module::String::string_manip_functions.at("create_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    llvm::FunctionType *get_cwd_type = llvm::FunctionType::get(str_type->getPointerTo(), {}, false);
    llvm::Function *get_cwd_fn = llvm::Function::Create( //
        get_cwd_type,                                    //
        llvm::Function::ExternalLinkage,                 //
        prefix + "get_cwd",                              //
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

void Generator::Module::System::generate_get_path_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION [Linux]:
    // #define BUFFER_SIZE 256
    // str *get_path(const str *path) {
    //     char buffer[BUFFER_SIZE];
    //     size_t buffer_len = 0;
    //     const size_t path_len = path->len;
    //     if (path_len >= BUFFER_SIZE) {
    //         return create_str(0);
    //     }
    //     for (size_t i = 0; i < path_len; i++) {
    //         char ci = path->value[i];
    //         if (ci != '\\') {
    //             buffer[buffer_len++] = ci;
    //             continue;
    //         }
    //         // Check if the next character is a space, if not change the path to be a '/'
    //         if (i + 1 == path_len || path->value[i + 1] != ' ') {
    //             buffer[buffer_len++] = '/';
    //         } else {
    //             buffer[buffer_len++] = '\\';
    //         }
    //     }
    //     return init_str(buffer, buffer_len);
    // }
    //
    // THE C IMPLEMENTATION [Windows]:
    // str *get_path(const str *path) {
    //     char buffer[BUFFER_SIZE];
    //     size_t buffer_len = 0;
    //     const size_t path_len = path->len;
    //     if (path_len >= BUFFER_SIZE) {
    //         return create_str(0);
    //     }
    //     bool path_contains_space = false;
    //     for (size_t i = 0; i < path_len; i++) {
    //         char ci = path->value[i];
    //         if (ci == '\\' && i + 1 < path_len && path->value[i + 1] == ' ') {
    //             // If the next character is a space, replace the two characters with a single space and
    //             // set the 'path_contains_space' value to true
    //             buffer[buffer_len++] = ' ';
    //             i++;
    //             path_contains_space = true;
    //             continue;
    //         }
    //         if (ci == '/') {
    //             buffer[buffer_len++] = '\\';
    //             continue;
    //         } else if (ci == ' ') {
    //             path_contains_space = true;
    //         }
    //         buffer[buffer_len++] = ci;
    //     }
    //     if (path_contains_space) {
    //         if (buffer_len + 2 >= BUFFER_SIZE) {
    //             return create_str(0);
    //         }
    //         memmove(buffer + 1, buffer, buffer_len);
    //         buffer[0] = '"';
    //         buffer[buffer_len + 1] = '"';
    //         buffer_len += 2;
    //     }
    //     return init_str(buffer, buffer_len);
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
#ifdef __WIN32__
    llvm::Function *memmove_fn = c_functions.at(MEMMOVE);
#endif
    llvm::Function *create_str_fn = Module::String::string_manip_functions.at("create_str");
    llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");

    llvm::FunctionType *get_path_type = llvm::FunctionType::get(str_type->getPointerTo(), {str_type->getPointerTo()}, false);
    llvm::Function *get_path_fn = llvm::Function::Create(get_path_type, llvm::Function::ExternalLinkage, prefix + "get_path", module);
    system_functions["get_path"] = get_path_fn;
    if (only_declarations) {
        return;
    }

    // Get the path parameter
    llvm::Value *path_param = get_path_fn->arg_begin();
    path_param->setName("path");

    // Create all basic blocks at the top
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_path_fn);
    llvm::BasicBlock *size_fail_block = llvm::BasicBlock::Create(context, "size_fail", get_path_fn);
    llvm::BasicBlock *loop_init_block = llvm::BasicBlock::Create(context, "loop_init", get_path_fn);
    llvm::BasicBlock *loop_cond_block = llvm::BasicBlock::Create(context, "loop_cond", get_path_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", get_path_fn);
    llvm::BasicBlock *post_loop_block = llvm::BasicBlock::Create(context, "post_loop", get_path_fn);
#ifdef __WIN32__
    llvm::BasicBlock *check_next_space_block = llvm::BasicBlock::Create(context, "check_next_space", get_path_fn);
    llvm::BasicBlock *not_backslash_space_block = llvm::BasicBlock::Create(context, "not_backslash_space", get_path_fn);
    llvm::BasicBlock *windows_special_case_block = llvm::BasicBlock::Create(context, "windows_special_case", get_path_fn);
    llvm::BasicBlock *handle_slash_block = llvm::BasicBlock::Create(context, "handle_slash", get_path_fn);
    llvm::BasicBlock *handle_space_or_other_block = llvm::BasicBlock::Create(context, "handle_space_or_other", get_path_fn);
    llvm::BasicBlock *set_space_flag_block = llvm::BasicBlock::Create(context, "set_space_flag", get_path_fn);
    llvm::BasicBlock *store_normal_block = llvm::BasicBlock::Create(context, "store_normal", get_path_fn);
    llvm::BasicBlock *add_quotes_block = llvm::BasicBlock::Create(context, "add_quotes", get_path_fn);
    llvm::BasicBlock *return_block = llvm::BasicBlock::Create(context, "return", get_path_fn);
    llvm::BasicBlock *quote_fail_block = llvm::BasicBlock::Create(context, "quote_fail", get_path_fn);
    llvm::BasicBlock *quote_ok_block = llvm::BasicBlock::Create(context, "quote_ok", get_path_fn);
#else
    llvm::BasicBlock *check_backslash_space_block = llvm::BasicBlock::Create(context, "check_backslash_space", get_path_fn);
    llvm::BasicBlock *handle_other_block = llvm::BasicBlock::Create(context, "handle_other", get_path_fn);
    llvm::BasicBlock *convert_to_slash_block = llvm::BasicBlock::Create(context, "convert_to_slash", get_path_fn);
    llvm::BasicBlock *keep_backslash_block = llvm::BasicBlock::Create(context, "keep_backslash", get_path_fn);
#endif

    // Entry block: Allocate variables and check path length
    builder->SetInsertPoint(entry_block);
    llvm::AllocaInst *buffer = builder->CreateAlloca(llvm::ArrayType::get(builder->getInt8Ty(), 256), nullptr, "buffer");
    llvm::AllocaInst *buffer_len = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "buffer_len");
    llvm::AllocaInst *i_var = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    llvm::AllocaInst *path_contains_space = builder->CreateAlloca(builder->getInt1Ty(), nullptr, "path_contains_space");

    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, path_param, 0, "len_ptr");
    llvm::Value *path_len = builder->CreateLoad(builder->getInt64Ty(), len_ptr, "path_len");

    llvm::Value *size_check = builder->CreateICmpUGE(path_len, builder->getInt64(256), "size_check");
    builder->CreateCondBr(size_check, size_fail_block, loop_init_block);

    // Size fail: Return empty string
    builder->SetInsertPoint(size_fail_block);
    llvm::Value *empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "empty_str");
    builder->CreateRet(empty_str);

    // Loop init: Initialize variables
    builder->SetInsertPoint(loop_init_block);
    builder->CreateStore(builder->getInt64(0), buffer_len);
    builder->CreateStore(builder->getInt64(0), i_var);
    builder->CreateStore(builder->getInt1(false), path_contains_space);
    builder->CreateBr(loop_cond_block);

    // Loop cond: Check i < path_len
    builder->SetInsertPoint(loop_cond_block);
    llvm::Value *i_val = builder->CreateLoad(builder->getInt64Ty(), i_var, "i_val");
    llvm::Value *cond = builder->CreateICmpULT(i_val, path_len, "cond");
    builder->CreateCondBr(cond, loop_body_block, post_loop_block);

    // Loop body: Load current char
    builder->SetInsertPoint(loop_body_block);
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, path_param, 1, "value_ptr");
    llvm::Value *char_ptr = builder->CreateGEP(builder->getInt8Ty(), value_ptr, {i_val}, "char_ptr");
    llvm::Value *ci = builder->CreateLoad(builder->getInt8Ty(), char_ptr, "ci");

#ifdef __WIN32__
    // Windows: Check for backslash followed by space
    llvm::Value *is_backslash = builder->CreateICmpEQ(ci, builder->getInt8('\\'), "is_backslash");
    llvm::Value *next_i = builder->CreateAdd(i_val, builder->getInt64(1), "next_i");
    llvm::Value *has_next = builder->CreateICmpULT(next_i, path_len, "has_next");
    llvm::Value *next_i_valid = builder->CreateAnd(is_backslash, has_next, "next_i_valid");
    builder->CreateCondBr(next_i_valid, check_next_space_block, not_backslash_space_block);

    // Check next space
    builder->SetInsertPoint(check_next_space_block);
    llvm::Value *next_char_ptr = builder->CreateGEP(builder->getInt8Ty(), value_ptr, {next_i}, "next_char_ptr");
    llvm::Value *next_char = builder->CreateLoad(builder->getInt8Ty(), next_char_ptr, "next_char");
    llvm::Value *next_is_space = builder->CreateICmpEQ(next_char, builder->getInt8(' '), "next_is_space");
    builder->CreateCondBr(next_is_space, windows_special_case_block, not_backslash_space_block);

    // Windows special case
    builder->SetInsertPoint(windows_special_case_block);
    llvm::Value *buf_len_sc = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "buf_len_sc");
    llvm::Value *buf_ptr_sc = builder->CreateGEP(builder->getInt8Ty(), buffer, {buf_len_sc}, "buf_ptr_sc");
    builder->CreateStore(builder->getInt8(' '), buf_ptr_sc);
    llvm::Value *new_buf_len_sc = builder->CreateAdd(buf_len_sc, builder->getInt64(1), "new_buf_len_sc");
    builder->CreateStore(new_buf_len_sc, buffer_len);
    llvm::Value *new_i_sc = builder->CreateAdd(i_val, builder->getInt64(2), "new_i_sc");
    builder->CreateStore(new_i_sc, i_var);
    builder->CreateStore(builder->getInt1(true), path_contains_space);
    builder->CreateBr(loop_cond_block);

    // Not backslash space: Check for forward slash
    builder->SetInsertPoint(not_backslash_space_block);
    llvm::Value *is_forward_slash = builder->CreateICmpEQ(ci, builder->getInt8('/'), "is_forward_slash");
    builder->CreateCondBr(is_forward_slash, handle_slash_block, handle_space_or_other_block);

    // Handle slash
    builder->SetInsertPoint(handle_slash_block);
    llvm::Value *buf_len_slash = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "buf_len_slash");
    llvm::Value *buf_ptr_slash = builder->CreateGEP(builder->getInt8Ty(), buffer, {buf_len_slash}, "buf_ptr_slash");
    builder->CreateStore(builder->getInt8('\\'), buf_ptr_slash);
    llvm::Value *new_buf_len_slash = builder->CreateAdd(buf_len_slash, builder->getInt64(1), "new_buf_len_slash");
    builder->CreateStore(new_buf_len_slash, buffer_len);
    llvm::Value *new_i_slash = builder->CreateAdd(i_val, builder->getInt64(1), "new_i_slash");
    builder->CreateStore(new_i_slash, i_var);
    builder->CreateBr(loop_cond_block);

    // Handle space or other
    builder->SetInsertPoint(handle_space_or_other_block);
    llvm::Value *is_space = builder->CreateICmpEQ(ci, builder->getInt8(' '), "is_space");
    builder->CreateCondBr(is_space, set_space_flag_block, store_normal_block);

    // Set space flag
    builder->SetInsertPoint(set_space_flag_block);
    builder->CreateStore(builder->getInt1(true), path_contains_space);
    builder->CreateBr(store_normal_block);

    // Store normal
    builder->SetInsertPoint(store_normal_block);
    llvm::Value *buf_len_normal = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "buf_len_normal");
    llvm::Value *buf_ptr_normal = builder->CreateGEP(builder->getInt8Ty(), buffer, {buf_len_normal}, "buf_ptr_normal");
    builder->CreateStore(ci, buf_ptr_normal);
    llvm::Value *new_buf_len_normal = builder->CreateAdd(buf_len_normal, builder->getInt64(1), "new_buf_len_normal");
    builder->CreateStore(new_buf_len_normal, buffer_len);
    llvm::Value *new_i_normal = builder->CreateAdd(i_val, builder->getInt64(1), "new_i_normal");
    builder->CreateStore(new_i_normal, i_var);
    builder->CreateBr(loop_cond_block);
#else
    // Linux: Check for backslash
    llvm::Value *is_backslash_linux = builder->CreateICmpEQ(ci, builder->getInt8('\\'), "is_backslash_linux");
    builder->CreateCondBr(is_backslash_linux, check_backslash_space_block, handle_other_block);

    // Check backslash space
    builder->SetInsertPoint(check_backslash_space_block);
    llvm::Value *next_i_linux = builder->CreateAdd(i_val, builder->getInt64(1), "next_i_linux");
    // Convert or keep
    llvm::Value *next_char_ptr_linux = builder->CreateGEP(builder->getInt8Ty(), value_ptr, {next_i_linux}, "next_char_ptr_linux");
    llvm::Value *next_char_linux = builder->CreateLoad(builder->getInt8Ty(), next_char_ptr_linux, "next_char_linux");
    llvm::Value *next_is_space_linux = builder->CreateICmpEQ(next_char_linux, builder->getInt8(' '), "next_is_space_linux");
    llvm::Value *should_convert = builder->CreateNot(next_is_space_linux, "should_convert");
    builder->CreateCondBr(should_convert, convert_to_slash_block, keep_backslash_block);

    // Convert to slash
    builder->SetInsertPoint(convert_to_slash_block);
    llvm::Value *buf_len_convert = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "buf_len_convert");
    llvm::Value *buf_ptr_convert = builder->CreateGEP(builder->getInt8Ty(), buffer, {buf_len_convert}, "buf_ptr_convert");
    builder->CreateStore(builder->getInt8('/'), buf_ptr_convert);
    llvm::Value *new_buf_len_convert = builder->CreateAdd(buf_len_convert, builder->getInt64(1), "new_buf_len_convert");
    builder->CreateStore(new_buf_len_convert, buffer_len);
    llvm::Value *new_i_convert = builder->CreateAdd(i_val, builder->getInt64(1), "new_i_convert");
    builder->CreateStore(new_i_convert, i_var);
    builder->CreateBr(loop_cond_block);

    // Keep backslash
    builder->SetInsertPoint(keep_backslash_block);
    llvm::Value *buf_len_keep = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "buf_len_keep");
    llvm::Value *buf_ptr_keep = builder->CreateGEP(builder->getInt8Ty(), buffer, {buf_len_keep}, "buf_ptr_keep");
    builder->CreateStore(builder->getInt8('\\'), buf_ptr_keep);
    llvm::Value *new_buf_len_keep = builder->CreateAdd(buf_len_keep, builder->getInt64(1), "new_buf_len_keep");
    builder->CreateStore(new_buf_len_keep, buffer_len);
    llvm::Value *new_i_keep = builder->CreateAdd(i_val, builder->getInt64(1), "new_i_keep");
    builder->CreateStore(new_i_keep, i_var);
    builder->CreateBr(loop_cond_block);

    // Handle other
    builder->SetInsertPoint(handle_other_block);
    llvm::Value *buf_len_other = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "buf_len_other");
    llvm::Value *buf_ptr_other = builder->CreateGEP(builder->getInt8Ty(), buffer, {buf_len_other}, "buf_ptr_other");
    builder->CreateStore(ci, buf_ptr_other);
    llvm::Value *new_buf_len_other = builder->CreateAdd(buf_len_other, builder->getInt64(1), "new_buf_len_other");
    builder->CreateStore(new_buf_len_other, buffer_len);
    llvm::Value *new_i_other = builder->CreateAdd(i_val, builder->getInt64(1), "new_i_other");
    builder->CreateStore(new_i_other, i_var);
    builder->CreateBr(loop_cond_block);
#endif

    // Post loop
    builder->SetInsertPoint(post_loop_block);
    llvm::Value *final_buffer_len = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "final_buffer_len");

#ifdef __WIN32__
    // Windows: Check for adding quotes
    llvm::Value *has_space = builder->CreateLoad(builder->getInt1Ty(), path_contains_space, "has_space");
    builder->CreateCondBr(has_space, add_quotes_block, return_block);

    // Add quotes
    builder->SetInsertPoint(add_quotes_block);
    llvm::Value *with_quotes_len = builder->CreateAdd(final_buffer_len, builder->getInt64(2), "with_quotes_len");
    llvm::Value *quote_check = builder->CreateICmpUGE(with_quotes_len, builder->getInt64(256), "quote_check");
    builder->CreateCondBr(quote_check, quote_fail_block, quote_ok_block);

    // Quote fail
    builder->SetInsertPoint(quote_fail_block);
    llvm::Value *quote_fail_result = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "quote_fail_result");
    builder->CreateRet(quote_fail_result);

    // Quote ok
    builder->SetInsertPoint(quote_ok_block);
    llvm::Value *buf_first = builder->CreateGEP(builder->getInt8Ty(), buffer, {builder->getInt64(0)}, "buf_first");
    llvm::Value *buf_second = builder->CreateGEP(builder->getInt8Ty(), buffer, {builder->getInt64(1)}, "buf_second");
    builder->CreateCall(memmove_fn, {buf_second, buf_first, final_buffer_len});
    builder->CreateStore(builder->getInt8('"'), buf_first);
    llvm::Value *quote_pos = builder->CreateAdd(final_buffer_len, builder->getInt64(1), "quote_pos");
    llvm::Value *buf_last = builder->CreateGEP(builder->getInt8Ty(), buffer, {quote_pos}, "buf_last");
    builder->CreateStore(builder->getInt8('"'), buf_last);
    builder->CreateStore(with_quotes_len, buffer_len);
    builder->CreateBr(return_block);

    // Return block
    builder->SetInsertPoint(return_block);
    final_buffer_len = builder->CreateLoad(builder->getInt64Ty(), buffer_len, "final_buffer_len_updated");
#endif

    // Final return
    llvm::Value *result = builder->CreateCall(init_str_fn, {buffer, final_buffer_len}, "result");
    builder->CreateRet(result);
}

void Generator::Module::System::generate_start_capture_function( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Module *module,                                        //
    const bool only_declarations                                 //
) {
    // THE C IMPLEMENTATION:
    // void start_capture(void) {
    //     if (capture_file != NULL) {
    //         // Already capturing; ignore or handle error
    //         return;
    //     }
    //
    //     // Flush existing buffers
    //     fflush(stdout);
    //     fflush(stderr);
    //
    //     // Save original fds
    //     orig_stdout_fd = dup(fileno(stdout));
    //     orig_stderr_fd = dup(fileno(stderr));
    //
    //     // Create temp file for capture
    //     capture_file = tmpfile();
    //     if (capture_file == NULL) {
    //         // Handle error (e.g., perror("tmpfile")); for now, abort capture
    //         orig_stdout_fd = -1;
    //         orig_stderr_fd = -1;
    //         return;
    //     }
    //
    //     // Redirect stdout to capture file
    //     dup2(fileno(capture_file), fileno(stdout));
    //
    //     // Redirect stderr to stdout (unifies and preserves order)
    //     dup2(fileno(stdout), fileno(stderr));
    // }
    llvm::Function *const fflush_fn = c_functions.at(FFLUSH);
    llvm::Function *const dup_fn = c_functions.at(DUP);
    llvm::Function *const fileno_fn = c_functions.at(FILENO);
    llvm::Function *const tmpfile_fn = c_functions.at(TMPFILE);
    llvm::Function *const dup2_fn = c_functions.at(DUP2);

    llvm::GlobalVariable *const stdout_gv = system_variables.at("stdout");
    llvm::GlobalVariable *const stderr_gv = system_variables.at("stderr");
    llvm::GlobalVariable *const orig_stdout_fd_gv = system_variables.at("orig_stdout_fd");
    llvm::GlobalVariable *const orig_stderr_fd_gv = system_variables.at("orig_stderr_fd");
    llvm::GlobalVariable *const capture_file_gv = system_variables.at("capture_file");

    llvm::FunctionType *const start_capture_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
    llvm::Function *const start_capture_fn = llvm::Function::Create( //
        start_capture_type,                                          //
        llvm::Function::ExternalLinkage,                             //
        prefix + "start_capture",                                    //
        module                                                       //
    );
    system_functions["start_capture"] = start_capture_fn;
    if (only_declarations) {
        return;
    }

    // Create the basic blocks for the function
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", start_capture_fn);
    llvm::BasicBlock *const already_capturing_block = llvm::BasicBlock::Create(context, "already_capturing", start_capture_fn);
    llvm::BasicBlock *const flush_block = llvm::BasicBlock::Create(context, "flush", start_capture_fn);
    llvm::BasicBlock *const tmpfile_null_block = llvm::BasicBlock::Create(context, "tmpfile_null", start_capture_fn);
    llvm::BasicBlock *const redirect_block = llvm::BasicBlock::Create(context, "redirect", start_capture_fn);

    llvm::PointerType *const ptr_ty = llvm::PointerType::get(context, 0);
    llvm::ConstantPointerNull *const null_ptr = llvm::ConstantPointerNull::get(ptr_ty);

    // Entry: Check if already capturing (capture_file != NULL)
    builder->SetInsertPoint(entry_block);
    llvm::Type *capture_file_ptr_ty = capture_file_gv->getValueType();
    llvm::Value *capture_file = IR::aligned_load(*builder, capture_file_ptr_ty, capture_file_gv, "capture_file_load");
    llvm::Value *is_capturing = builder->CreateICmpNE(capture_file, null_ptr, "is_capturing");
    builder->CreateCondBr(is_capturing, already_capturing_block, flush_block);

    // Already capturing: return
    builder->SetInsertPoint(already_capturing_block);
    builder->CreateRetVoid();

    // Flush stdout and stderr
    builder->SetInsertPoint(flush_block);
    llvm::Value *stdout_ptr = builder->CreateLoad(ptr_ty, stdout_gv, "stdout_load");
    llvm::Value *stderr_ptr = builder->CreateLoad(ptr_ty, stderr_gv, "stderr_load");
    builder->CreateCall(fflush_fn, {stdout_ptr});
    builder->CreateCall(fflush_fn, {stderr_ptr});

    // Save original fds
    llvm::Value *stdout_fileno = builder->CreateCall(fileno_fn, {stdout_ptr}, "stdout_fileno");
    llvm::Value *orig_stdout = builder->CreateCall(dup_fn, {stdout_fileno}, "orig_stdout");
    builder->CreateStore(orig_stdout, orig_stdout_fd_gv);

    llvm::Value *stderr_fileno = builder->CreateCall(fileno_fn, {stderr_ptr}, "stderr_fileno");
    llvm::Value *orig_stderr = builder->CreateCall(dup_fn, {stderr_fileno}, "orig_stderr");
    builder->CreateStore(orig_stderr, orig_stderr_fd_gv);

    // Create temp file
    llvm::Value *temp_file = builder->CreateCall(tmpfile_fn, {}, "temp_file");
    builder->CreateStore(temp_file, capture_file_gv);

    // Check if temp_file == NULL
    llvm::Value *is_null = builder->CreateICmpEQ(temp_file, null_ptr, "tmpfile_is_null");
    builder->CreateCondBr(is_null, tmpfile_null_block, redirect_block);

    // tmpfile null: set orig fds to -1, return
    builder->SetInsertPoint(tmpfile_null_block);
    llvm::Value *neg_one = builder->getInt32(-1);
    builder->CreateStore(neg_one, orig_stdout_fd_gv);
    builder->CreateStore(neg_one, orig_stderr_fd_gv);
    builder->CreateRetVoid();

    // Redirect
    builder->SetInsertPoint(redirect_block);
    llvm::Value *capture_fileno = builder->CreateCall(fileno_fn, {temp_file}, "capture_fileno");
    llvm::Value *new_stdout_fileno = builder->CreateCall(fileno_fn, {stdout_ptr}, "new_stdout_fileno");
    builder->CreateCall(dup2_fn, {capture_fileno, new_stdout_fileno});

    llvm::Value *new_stderr_fileno = builder->CreateCall(fileno_fn, {stderr_ptr}, "new_stderr_fileno");
    builder->CreateCall(dup2_fn, {new_stdout_fileno, new_stderr_fileno});
    builder->CreateRetVoid();
}

void Generator::Module::System::generate_end_capture_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // str *end_capture(void) {
    //     if (capture_file == NULL) {
    //         // Not capturing; return empty string
    //         return create_str(0);
    //     }
    //
    //     // Flush buffers to ensure all output is in the file
    //     fflush(stdout);
    //     fflush(stderr);
    //
    //     // Restore original fds
    //     dup2(orig_stdout_fd, fileno(stdout));
    //     dup2(orig_stderr_fd, fileno(stderr));
    //     close(orig_stdout_fd);
    //     close(orig_stderr_fd);
    //     orig_stdout_fd = -1;
    //     orig_stderr_fd = -1;
    //
    //     // Rewind and read the capture file
    //     rewind(capture_file);
    //     str *captured = create_str(0);
    //     char buffer[4096];
    //     size_t bytes_read;
    //     while ((bytes_read = fread(buffer, 1, sizeof(buffer), capture_file)) > 0) {
    //         append_lit(&captured, buffer, bytes_read);
    //     }
    //
    //     // Clean up
    //     fclose(capture_file);
    //     capture_file = NULL;
    //
    //     return captured;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *const fflush_fn = c_functions.at(FFLUSH);
    llvm::Function *const dup2_fn = c_functions.at(DUP2);
    llvm::Function *const fileno_fn = c_functions.at(FILENO);
    llvm::Function *const close_fn = c_functions.at(CLOSE);
    llvm::Function *const rewind_fn = c_functions.at(REWIND);
    llvm::Function *const fread_fn = c_functions.at(FREAD);
    llvm::Function *const fclose_fn = c_functions.at(FCLOSE);

    llvm::Function *const create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *const append_lit_fn = String::string_manip_functions.at("append_lit");

    llvm::GlobalVariable *const stdout_gv = system_variables.at("stdout");
    llvm::GlobalVariable *const stderr_gv = system_variables.at("stderr");
    llvm::GlobalVariable *const orig_stdout_fd_gv = system_variables.at("orig_stdout_fd");
    llvm::GlobalVariable *const orig_stderr_fd_gv = system_variables.at("orig_stderr_fd");
    llvm::GlobalVariable *const capture_file_gv = system_variables.at("capture_file");

    llvm::FunctionType *const end_capture_type = llvm::FunctionType::get(str_type->getPointerTo(), {}, false);
    llvm::Function *const end_capture_fn = llvm::Function::Create( //
        end_capture_type,                                          //
        llvm::Function::ExternalLinkage,                           //
        prefix + "end_capture",                                    //
        module                                                     //
    );
    system_functions["end_capture"] = end_capture_fn;
    if (only_declarations) {
        return;
    }

    llvm::PointerType *const ptr_ty = llvm::PointerType::get(context, 0);
    llvm::IntegerType *const i32_ty = builder->getInt32Ty();
    llvm::ConstantPointerNull *const null_ptr = llvm::ConstantPointerNull::get(ptr_ty);
    llvm::ConstantInt *const neg_one = builder->getInt32(-1);
    llvm::ConstantInt *const zero_i64 = builder->getInt64(0);
    llvm::ConstantInt *const one_i64 = builder->getInt64(1);
    llvm::ConstantInt *const buffer_size = builder->getInt64(4096);

    // Create the basic blocks for the function
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", end_capture_fn);
    llvm::BasicBlock *const not_capturing_block = llvm::BasicBlock::Create(context, "not_capturing", end_capture_fn);
    llvm::BasicBlock *const flush_block = llvm::BasicBlock::Create(context, "flush", end_capture_fn);
    llvm::BasicBlock *const restore_block = llvm::BasicBlock::Create(context, "restore", end_capture_fn);
    llvm::BasicBlock *const read_loop_header = llvm::BasicBlock::Create(context, "read_loop_header", end_capture_fn);
    llvm::BasicBlock *const read_loop_body = llvm::BasicBlock::Create(context, "read_loop_body", end_capture_fn);
    llvm::BasicBlock *const read_loop_exit = llvm::BasicBlock::Create(context, "read_loop_exit", end_capture_fn);

    // Entry: Check if capturing (capture_file != NULL)
    builder->SetInsertPoint(entry_block);
    llvm::Value *capture_file = IR::aligned_load(*builder, ptr_ty, capture_file_gv, "capture_file_load");
    llvm::Value *is_null = builder->CreateICmpEQ(capture_file, null_ptr, "is_null");
    builder->CreateCondBr(is_null, not_capturing_block, flush_block);

    // Not capturing: return empty str
    builder->SetInsertPoint(not_capturing_block);
    llvm::Value *empty_str = builder->CreateCall(create_str_fn, {zero_i64}, "empty_str");
    builder->CreateRet(empty_str);

    // Flush stdout and stderr
    builder->SetInsertPoint(flush_block);
    llvm::Value *stdout_ptr = IR::aligned_load(*builder, ptr_ty, stdout_gv, "stdout_load");
    llvm::Value *stderr_ptr = IR::aligned_load(*builder, ptr_ty, stderr_gv, "stderr_load");
    builder->CreateCall(fflush_fn, {stdout_ptr});
    builder->CreateCall(fflush_fn, {stderr_ptr});
    builder->CreateBr(restore_block);

    // Restore original fds and clean up orig fds
    builder->SetInsertPoint(restore_block);
    llvm::Value *orig_stdout_fd = builder->CreateLoad(i32_ty, orig_stdout_fd_gv, "orig_stdout_fd_load");
    llvm::Value *stdout_fileno = builder->CreateCall(fileno_fn, {stdout_ptr}, "stdout_fileno");
    builder->CreateCall(dup2_fn, {orig_stdout_fd, stdout_fileno});

    llvm::Value *orig_stderr_fd = builder->CreateLoad(i32_ty, orig_stderr_fd_gv, "orig_stderr_fd_load");
    llvm::Value *stderr_fileno = builder->CreateCall(fileno_fn, {stderr_ptr}, "stderr_fileno");
    builder->CreateCall(dup2_fn, {orig_stderr_fd, stderr_fileno});

    builder->CreateCall(close_fn, {orig_stdout_fd});
    builder->CreateCall(close_fn, {orig_stderr_fd});

    builder->CreateStore(neg_one, orig_stdout_fd_gv);
    builder->CreateStore(neg_one, orig_stderr_fd_gv);

    // Rewind capture_file
    builder->CreateCall(rewind_fn, {capture_file});

    // Create empty captured str
    llvm::AllocaInst *captured_alloca = builder->CreateAlloca(ptr_ty, 0, nullptr, "captured_alloca");
    llvm::Value *captured = builder->CreateCall(create_str_fn, {zero_i64}, "captured");
    IR::aligned_store(*builder, captured, captured_alloca);

    // Allocate buffer
    llvm::AllocaInst *buffer = builder->CreateAlloca(builder->getInt8Ty(), buffer_size, "buffer");

    builder->CreateBr(read_loop_header);

    // Read loop header: bytes_read = fread(buffer, 1, 4096, capture_file)
    builder->SetInsertPoint(read_loop_header);
    llvm::Value *bytes_read = builder->CreateCall(fread_fn, {buffer, one_i64, buffer_size, capture_file}, "bytes_read");
    llvm::Value *read_gt_zero = builder->CreateICmpUGT(bytes_read, zero_i64, "read_gt_zero");
    builder->CreateCondBr(read_gt_zero, read_loop_body, read_loop_exit);

    // Read loop body: append_lit(&captured, buffer, bytes_read)
    builder->SetInsertPoint(read_loop_body);
    builder->CreateCall(append_lit_fn, {captured_alloca, buffer, bytes_read});
    builder->CreateBr(read_loop_header);

    // Read loop exit: fclose, set capture_file = NULL, ret captured
    builder->SetInsertPoint(read_loop_exit);
    builder->CreateCall(fclose_fn, {capture_file});
    builder->CreateStore(null_ptr, capture_file_gv);
    captured = IR::aligned_load(*builder, ptr_ty, captured_alloca, "captured_ret");
    builder->CreateRet(captured);
}

void Generator::Module::System::generate_end_capture_lines_function( //
    llvm::IRBuilder<> *builder,                                      //
    llvm::Module *module,                                            //
    const bool only_declarations                                     //
) {
    // THE C IMPLEMENTATION:
    // str *system_end_capture_lines(void) {
    //     // Union type to be able to bitcast a pointer to an integer
    //     typedef union ptr_bitcast_t {
    //         str *ptr;
    //         size_t bits;
    //     } ptr_bitcast_t;
    //
    //     size_t line_count = 0;
    //     if (capture_file == NULL) {
    //         // Not capturing; return empty array
    //         return create_arr(1, sizeof(str *), &line_count);
    //     }
    //
    //     // End the capture and get it as a single string
    //     str *captured_buffer = system_end_capture();
    //     // Check how many lines there are in the captured buffer
    //     size_t last_newline = 0;
    //     char *const captured_buffer_value = captured_buffer->value;
    //     for (size_t i = 0; i < captured_buffer->len; i++) {
    //         if (captured_buffer_value[i] == '\n') {
    //             line_count++;
    //             last_newline = i;
    //         }
    //     }
    //     const bool contains_trailing_line = last_newline + 1 < captured_buffer->len;
    //     if (contains_trailing_line) {
    //         line_count++;
    //     }
    //     // Create the output array
    //     str *output_array = create_arr(1, sizeof(str *), &line_count);
    //     // Go through the buffer and create new strings and store them in the output
    //     // array
    //     size_t output_id = 0;
    //     size_t line_start = 0;
    //     for (size_t i = 0; i < captured_buffer->len; i++) {
    //         if (captured_buffer_value[i] == '\n') {
    //             str *line_string = get_str_slice(output_buffer, line_start, i);
    //             ptr_bitcast_t cast = (ptr_bitcast_t){.ptr = line_string};
    //             assign_arr_val_at(output_array, sizeof(str *), &output_id, cast.bits);
    //             line_start = i + 1;
    //             output_id++;
    //         }
    //     }
    //     if (contains_trailing_line) {
    //         str *line_string = get_str_slice(output_buffer, line_start, captured_buffer->len);
    //         ptr_bitcast_t cast = (ptr_bitcast_t){.ptr = line_string};
    //         assign_arr_val_at(output_array, sizeof(str *), &output_id, cast.bits);
    //     }
    //     free(captured_buffer);
    //     return output_array;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
    llvm::Function *const free_fn = c_functions.at(FREE);

    llvm::Function *const create_arr_fn = Array::array_manip_functions.at("create_arr");
    llvm::Function *const end_capture_fn = system_functions.at("end_capture");
    llvm::Function *const assign_arr_val_at_fn = Array::array_manip_functions.at("assign_arr_val_at");
    llvm::Function *const get_str_slice_fn = String::string_manip_functions.at("get_str_slice");

    llvm::GlobalVariable *const capture_file_gv = system_variables.at("capture_file");

    llvm::FunctionType *const end_capture_lines_type = llvm::FunctionType::get(str_type->getPointerTo(), {}, false);
    llvm::Function *const end_capture_lines_fn = llvm::Function::Create( //
        end_capture_lines_type,                                          //
        llvm::Function::ExternalLinkage,                                 //
        prefix + "end_capture_lines",                                    //
        module                                                           //
    );
    system_functions["end_capture_lines"] = end_capture_lines_fn;
    if (only_declarations) {
        return;
    }

    llvm::PointerType *const ptr_ty = llvm::PointerType::get(context, 0);
    llvm::Type *const i64_ty = builder->getInt64Ty();
    llvm::Type *const i8_ty = builder->getInt8Ty();
    llvm::Constant *const null_ptr = llvm::ConstantPointerNull::get(ptr_ty);
    llvm::Constant *const zero_i64 = builder->getInt64(0);
    llvm::Constant *const one_i64 = builder->getInt64(1);
    llvm::Constant *const sizeof_ptr = builder->getInt64(Allocation::get_type_size(module, ptr_ty));
    llvm::Constant *const newline_const = builder->getInt8('\n');

    // Create the basic blocks for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", end_capture_lines_fn);
    llvm::BasicBlock *not_capturing_block = llvm::BasicBlock::Create(context, "not_capturing", end_capture_lines_fn);
    llvm::BasicBlock *capture_block = llvm::BasicBlock::Create(context, "capture", end_capture_lines_fn);
    llvm::BasicBlock *count_loop_cond_block = llvm::BasicBlock::Create(context, "count_loop_cond", end_capture_lines_fn);
    llvm::BasicBlock *count_loop_body_block = llvm::BasicBlock::Create(context, "count_loop_body", end_capture_lines_fn);
    llvm::BasicBlock *count_newline_block = llvm::BasicBlock::Create(context, "count_newline", end_capture_lines_fn);
    llvm::BasicBlock *count_loop_continue_block = llvm::BasicBlock::Create(context, "count_loop_continue", end_capture_lines_fn);
    llvm::BasicBlock *count_loop_exit_block = llvm::BasicBlock::Create(context, "count_loop_exit", end_capture_lines_fn);
    llvm::BasicBlock *create_array_block = llvm::BasicBlock::Create(context, "create_array", end_capture_lines_fn);
    llvm::BasicBlock *assign_loop_cond_block = llvm::BasicBlock::Create(context, "assign_loop_cond", end_capture_lines_fn);
    llvm::BasicBlock *assign_loop_body_block = llvm::BasicBlock::Create(context, "assign_loop_body", end_capture_lines_fn);
    llvm::BasicBlock *assign_newline_block = llvm::BasicBlock::Create(context, "assign_newline", end_capture_lines_fn);
    llvm::BasicBlock *assign_loop_continue_block = llvm::BasicBlock::Create(context, "assign_loop_continue", end_capture_lines_fn);
    llvm::BasicBlock *assign_loop_exit_block = llvm::BasicBlock::Create(context, "assign_loop_exit", end_capture_lines_fn);
    llvm::BasicBlock *trailing_assign_block = llvm::BasicBlock::Create(context, "trailing_assign", end_capture_lines_fn);
    llvm::BasicBlock *cleanup_block = llvm::BasicBlock::Create(context, "cleanup", end_capture_lines_fn);

    // Entry: Check if capturing (capture_file != NULL)
    builder->SetInsertPoint(entry_block);
    llvm::Value *capture_file = IR::aligned_load(*builder, ptr_ty, capture_file_gv, "capture_file_load");
    llvm::Value *is_null = builder->CreateICmpEQ(capture_file, null_ptr, "is_null");
    builder->CreateCondBr(is_null, not_capturing_block, capture_block);

    // Not capturing: return create_arr(1, sizeof_ptr, &0)
    builder->SetInsertPoint(not_capturing_block);
    llvm::AllocaInst *zero_count_alloca = builder->CreateAlloca(i64_ty, nullptr, "zero_count_alloca");
    builder->CreateStore(zero_i64, zero_count_alloca);
    llvm::Value *empty_arr = builder->CreateCall(create_arr_fn, {one_i64, sizeof_ptr, zero_count_alloca}, "empty_arr");
    builder->CreateRet(empty_arr);

    // Capture: call end_capture
    builder->SetInsertPoint(capture_block);
    llvm::Value *captured_buffer = builder->CreateCall(end_capture_fn, {}, "captured_buffer");

    // Alloca line_count = 0, last_newline = 0, count_i = 0
    llvm::AllocaInst *line_count_alloca = builder->CreateAlloca(i64_ty, nullptr, "line_count_alloca");
    builder->CreateStore(zero_i64, line_count_alloca);
    llvm::AllocaInst *last_newline_alloca = builder->CreateAlloca(i64_ty, nullptr, "last_newline_alloca");
    builder->CreateStore(zero_i64, last_newline_alloca);
    llvm::AllocaInst *count_i_alloca = builder->CreateAlloca(i64_ty, nullptr, "count_i_alloca");
    builder->CreateStore(zero_i64, count_i_alloca);

    // Load len
    llvm::Value *buffer_len_ptr = builder->CreateStructGEP(str_type, captured_buffer, 0, "buffer_len_ptr");
    llvm::Value *buffer_len = IR::aligned_load(*builder, i64_ty, buffer_len_ptr, "buffer_len");

    // captured_buffer_value = getelementptr to value[0]
    llvm::Value *buffer_value_ptr = builder->CreateStructGEP(str_type, captured_buffer, 1, "buffer_value_ptr");

    builder->CreateBr(count_loop_cond_block);

    // Count loop cond: i < len
    builder->SetInsertPoint(count_loop_cond_block);
    llvm::Value *count_i = IR::aligned_load(*builder, i64_ty, count_i_alloca, "count_i");
    llvm::Value *count_cond = builder->CreateICmpULT(count_i, buffer_len, "count_cond");
    builder->CreateCondBr(count_cond, count_loop_body_block, count_loop_exit_block);

    // Count loop body
    builder->SetInsertPoint(count_loop_body_block);
    llvm::Value *char_ptr = builder->CreateGEP(i8_ty, buffer_value_ptr, count_i, "char_ptr");
    llvm::Value *curr_char = IR::aligned_load(*builder, i8_ty, char_ptr, "curr_char");
    llvm::Value *is_newline = builder->CreateICmpEQ(curr_char, newline_const, "is_newline");
    builder->CreateCondBr(is_newline, count_newline_block, count_loop_continue_block);

    // Count newline
    builder->SetInsertPoint(count_newline_block);
    llvm::Value *line_count_load = IR::aligned_load(*builder, i64_ty, line_count_alloca, "line_count_load");
    llvm::Value *line_count_inc = builder->CreateAdd(line_count_load, one_i64, "line_count_inc");
    builder->CreateStore(line_count_inc, line_count_alloca);
    builder->CreateStore(count_i, last_newline_alloca);
    builder->CreateBr(count_loop_continue_block);

    // Count loop continue
    builder->SetInsertPoint(count_loop_continue_block);
    llvm::Value *next_i_count = builder->CreateAdd(count_i, one_i64, "next_i_count");
    builder->CreateStore(next_i_count, count_i_alloca);
    builder->CreateBr(count_loop_cond_block);

    // Count loop exit: check trailing
    builder->SetInsertPoint(count_loop_exit_block);
    llvm::Value *last_newline = IR::aligned_load(*builder, i64_ty, last_newline_alloca, "last_newline_load");
    llvm::Value *last_newline_p1 = builder->CreateAdd(last_newline, builder->getInt64(1), "last_newline_p1");
    llvm::Value *has_trailing = builder->CreateICmpULT(last_newline_p1, buffer_len, "has_trailing");
    line_count_load = IR::aligned_load(*builder, i64_ty, line_count_alloca, "line_count_load");
    llvm::Value *line_count_inc_trailing = builder->CreateAdd(line_count_load, one_i64, "line_count_inc_trailing");
    llvm::Value *line_count_final = builder->CreateSelect(has_trailing, line_count_inc_trailing, line_count_load, "line_count_final");
    builder->CreateStore(line_count_final, line_count_alloca);
    builder->CreateBr(create_array_block);

    // Create array
    builder->SetInsertPoint(create_array_block);
    llvm::Value *output_array = builder->CreateCall(create_arr_fn, {one_i64, sizeof_ptr, line_count_alloca}, "output_array");

    // Alloca output_id = 0, line_start = 0, assign_i = 0
    llvm::AllocaInst *output_id_alloca = builder->CreateAlloca(i64_ty, nullptr, "output_id_alloca");
    builder->CreateStore(zero_i64, output_id_alloca);
    llvm::AllocaInst *line_start_alloca = builder->CreateAlloca(i64_ty, nullptr, "line_start_alloca");
    builder->CreateStore(zero_i64, line_start_alloca);
    llvm::AllocaInst *assign_i_alloca = builder->CreateAlloca(i64_ty, nullptr, "assign_i_alloca");
    builder->CreateStore(zero_i64, assign_i_alloca);

    builder->CreateBr(assign_loop_cond_block);

    // Assign loop cond: i < len
    builder->SetInsertPoint(assign_loop_cond_block);
    llvm::Value *assign_i = IR::aligned_load(*builder, i64_ty, assign_i_alloca, "assign_i");
    llvm::Value *assign_cond = builder->CreateICmpULT(assign_i, buffer_len, "assign_cond");
    builder->CreateCondBr(assign_cond, assign_loop_body_block, assign_loop_exit_block);

    // Assign loop body
    builder->SetInsertPoint(assign_loop_body_block);
    llvm::Value *assign_char_ptr = builder->CreateGEP(i8_ty, buffer_value_ptr, assign_i, "assign_char_ptr");
    llvm::Value *assign_curr_char = IR::aligned_load(*builder, i8_ty, assign_char_ptr, "assign_curr_char");
    llvm::Value *assign_is_newline = builder->CreateICmpEQ(assign_curr_char, newline_const, "assign_is_newline");
    builder->CreateCondBr(assign_is_newline, assign_newline_block, assign_loop_continue_block);

    // Assign newline
    builder->SetInsertPoint(assign_newline_block);
    llvm::Value *line_start = IR::aligned_load(*builder, i64_ty, line_start_alloca, "line_start_load");
    llvm::Value *line_string = builder->CreateCall(get_str_slice_fn, {captured_buffer, line_start, assign_i}, "line_string");
    llvm::Value *line_string_bits = builder->CreatePtrToInt(line_string, i64_ty, "line_string_bits");
    llvm::AllocaInst *output_id_ptr = builder->CreateAlloca(i64_ty, nullptr, "output_id_ptr");
    llvm::Value *output_id_load = IR::aligned_load(*builder, i64_ty, output_id_alloca, "output_id_load");
    builder->CreateStore(output_id_load, output_id_ptr);
    builder->CreateCall(assign_arr_val_at_fn, {output_array, sizeof_ptr, output_id_ptr, line_string_bits});
    llvm::Value *next_output_id = builder->CreateAdd(output_id_load, one_i64, "next_output_id");
    builder->CreateStore(next_output_id, output_id_alloca);
    llvm::Value *assign_i_p1 = builder->CreateAdd(assign_i, builder->getInt64(1), "assign_i_p1");
    builder->CreateStore(assign_i_p1, line_start_alloca);
    builder->CreateBr(assign_loop_continue_block);

    // Assign loop continue
    builder->SetInsertPoint(assign_loop_continue_block);
    llvm::Value *next_i_assign = builder->CreateAdd(assign_i, one_i64, "next_i_assign");
    builder->CreateStore(next_i_assign, assign_i_alloca);
    builder->CreateBr(assign_loop_cond_block);

    // Assign loop exit: check trailing
    builder->SetInsertPoint(assign_loop_exit_block);
    last_newline_p1 = builder->CreateAdd(last_newline, builder->getInt64(1), "last_newline_p1");
    llvm::Value *final_has_trailing = builder->CreateICmpULT(last_newline_p1, buffer_len, "final_has_trailing");
    builder->CreateCondBr(final_has_trailing, trailing_assign_block, cleanup_block);

    // Trailing assign
    builder->SetInsertPoint(trailing_assign_block);
    llvm::Value *trailing_line_start = IR::aligned_load(*builder, i64_ty, line_start_alloca, "trailing_line_start_load");
    llvm::Value *trailing_line_string =
        builder->CreateCall(get_str_slice_fn, {captured_buffer, trailing_line_start, buffer_len}, "trailing_line_string");
    llvm::Value *trailing_bits = builder->CreatePtrToInt(trailing_line_string, i64_ty, "trailing_bits");
    llvm::AllocaInst *trailing_id_ptr = builder->CreateAlloca(i64_ty, nullptr, "trailing_id_ptr");
    llvm::Value *trailing_id_load = IR::aligned_load(*builder, i64_ty, output_id_alloca, "trailing_id_load");
    builder->CreateStore(trailing_id_load, trailing_id_ptr);
    builder->CreateCall(assign_arr_val_at_fn, {output_array, sizeof_ptr, trailing_id_ptr, trailing_bits});
    builder->CreateBr(cleanup_block);

    // Cleanup: free captured_buffer, ret output_array
    builder->SetInsertPoint(cleanup_block);
    builder->CreateCall(free_fn, {captured_buffer});
    builder->CreateRet(output_array);
}
