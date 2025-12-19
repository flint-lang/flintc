#include "generator/generator.hpp"

static const std::string hash = Hash(std::string("filesystem")).to_string();

void Generator::Module::FileSystem::generate_filesystem_functions( //
    llvm::IRBuilder<> *builder,                                    //
    llvm::Module *module,                                          //
    const bool only_declarations                                   //
) {
    generate_read_file_function(builder, module, only_declarations);
    generate_read_lines_function(builder, module, only_declarations);
    generate_file_exists_function(builder, module, only_declarations);
    generate_write_file_function(builder, module, only_declarations);
    generate_append_file_function(builder, module, only_declarations);
    generate_is_file_function(builder, module, only_declarations);
}

void Generator::Module::FileSystem::generate_read_file_function( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Module *module,                                        //
    const bool only_declarations                                 //
) {
    // THE C IMPLEMENTATION:
    // str *read_file(const str *path) {
    //     char *c_path = (char *(path->value;
    //     // Open the file for reading in binary mode
    //     FILE *file = fopen(c_path, "rb");
    //     // Get the file size
    //     if (fseek(file, 0, SEEK_END) != 0) {
    //         fclose(file);
    //         return NULL;
    //     }
    //     long file_size = ftell(file);
    //     if (file_size == -1) {
    //         fclose(file);
    //         return NULL;
    //     }
    //     // Return to the beginning of the file
    //     if (fseek(file, 0, SEEK_SET) != 0) {
    //         fclose(file);
    //         return NULL;
    //     }
    //     // Allocate memory for the file content
    //     str *content = create_str((size_t)file_size);
    //     size_t bytes_read = fread(content->value, 1, (size_t)file_size, file);
    //     fclose(file);
    //     if (bytes_read != (size_t)file_size) {
    //         free(content);
    //         return NULL; // File read error
    //     }
    //     return content;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *fopen_fn = c_functions.at(FOPEN);
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *fseek_fn = c_functions.at(FSEEK);
    llvm::Function *fclose_fn = c_functions.at(FCLOSE);
    llvm::Function *ftell_fn = c_functions.at(FTELL);
    llvm::Function *fread_fn = c_functions.at(FREAD);
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    const unsigned int ErrIO = Type::get_type_id_from_str("ErrIO");
    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotFound = 1;
    const unsigned int NotReadable = 2;
    const unsigned int UnexpectedEOF = 4;
    const std::string NotFoundMessage(ErrIOValues.at(NotFound).second);
    const std::string NotReadableMessage(ErrIOValues.at(NotReadable).second);
    const std::string UnexpectedEOFMessage(ErrIOValues.at(UnexpectedEOF).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *read_file_type = llvm::FunctionType::get( //
        function_result_type,                                     // Return type: str*
        {str_type->getPointerTo()},                               // Parameter: const str* path
        false                                                     // Not variadic
    );
    llvm::Function *read_file_fn = llvm::Function::Create(read_file_type, llvm::Function::ExternalLinkage, hash + ".read_file", module);
    fs_functions["read_file"] = read_file_fn;
    if (only_declarations) {
        return;
    }

    // Get the path parameter
    llvm::Argument *path_arg = read_file_fn->arg_begin();
    path_arg->setName("path");

    // Create all basic blocks first
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", read_file_fn);
    llvm::BasicBlock *file_null_block = llvm::BasicBlock::Create(context, "file_null", read_file_fn);
    llvm::BasicBlock *file_valid_block = llvm::BasicBlock::Create(context, "file_valid", read_file_fn);
    llvm::BasicBlock *seek_end_ok_block = llvm::BasicBlock::Create(context, "seek_end_ok", read_file_fn);
    llvm::BasicBlock *seek_end_error_block = llvm::BasicBlock::Create(context, "seek_end_error", read_file_fn);
    llvm::BasicBlock *ftell_ok_block = llvm::BasicBlock::Create(context, "ftell_ok", read_file_fn);
    llvm::BasicBlock *ftell_error_block = llvm::BasicBlock::Create(context, "ftell_error", read_file_fn);
    llvm::BasicBlock *seek_set_ok_block = llvm::BasicBlock::Create(context, "seek_set_ok", read_file_fn);
    llvm::BasicBlock *seek_set_error_block = llvm::BasicBlock::Create(context, "seek_set_error", read_file_fn);
    llvm::BasicBlock *read_ok_block = llvm::BasicBlock::Create(context, "read_ok", read_file_fn);
    llvm::BasicBlock *read_error_block = llvm::BasicBlock::Create(context, "read_error", read_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the c string from the value directly
    llvm::Value *c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "rb" string constant
    llvm::Value *mode_str = IR::generate_const_string(module, "rb");

    // Open file: file = fopen(c_path, "rb")
    llvm::Value *file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *file_null_check = builder->CreateIsNull(file, "file_is_null");
    builder->CreateCondBr(file_null_check, file_null_block, file_valid_block);

    // Handle NULL file, throw ErrIO.NotFound
    builder->SetInsertPoint(file_null_block);
    llvm::AllocaInst *ret_file_null_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_null_alloc");
    llvm::Value *ret_file_null_err_ptr = builder->CreateStructGEP(function_result_type, ret_file_null_alloc, 0, "ret_file_null_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrIO, NotFound, NotFoundMessage);
    IR::aligned_store(*builder, err_value, ret_file_null_err_ptr);
    llvm::Value *ret_file_null_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_file_null_empty_str");
    llvm::Value *ret_file_null_val_ptr = builder->CreateStructGEP(function_result_type, ret_file_null_alloc, 1, "ret_file_null_val_ptr");
    IR::aligned_store(*builder, ret_file_null_empty_str, ret_file_null_val_ptr);
    llvm::Value *ret_file_null_val = IR::aligned_load(*builder, function_result_type, ret_file_null_alloc, "ret_file_null_val");
    builder->CreateRet(ret_file_null_val);

    // Continue with valid file
    builder->SetInsertPoint(file_valid_block);

    // fseek(file, 0, SEEK_END)
    llvm::Value *seek_end = builder->getInt32(2); // SEEK_END is 2
    llvm::Value *seek_end_result = builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_end}, "seek_end_result");

    // Check if fseek failed
    llvm::Value *seek_end_check = builder->CreateICmpNE(seek_end_result, builder->getInt32(0), "seek_end_check");
    builder->CreateCondBr(seek_end_check, seek_end_error_block, seek_end_ok_block);

    // Handle fseek SEEK_END error, throw ErrIO.NotReadable
    builder->SetInsertPoint(seek_end_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *ret_seek_end_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_seek_end_alloc");
    llvm::Value *ret_seek_end_err_ptr = builder->CreateStructGEP(function_result_type, ret_seek_end_alloc, 0, "ret_seek_end_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, NotReadable, NotReadableMessage);
    IR::aligned_store(*builder, err_value, ret_seek_end_err_ptr);
    llvm::Value *ret_seek_end_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_seek_end_empty_str");
    llvm::Value *ret_seek_end_val_ptr = builder->CreateStructGEP(function_result_type, ret_seek_end_alloc, 1, "ret_seek_end_val_ptr");
    IR::aligned_store(*builder, ret_seek_end_empty_str, ret_seek_end_val_ptr);
    llvm::Value *ret_seek_end_val = IR::aligned_load(*builder, function_result_type, ret_seek_end_alloc, "ret_seek_end_val");
    builder->CreateRet(ret_seek_end_val);

    // Get file size
    builder->SetInsertPoint(seek_end_ok_block);
    llvm::Value *file_size = builder->CreateCall(ftell_fn, {file}, "file_size");

    // Check if ftell failed (file_size == -1)
    llvm::Value *minus_one = builder->getInt64(-1);
    llvm::Value *ftell_check = builder->CreateICmpEQ(file_size, minus_one, "ftell_check");
    builder->CreateCondBr(ftell_check, ftell_error_block, ftell_ok_block);

    // Handle ftell error, throw ErrIO.NotReadable
    builder->SetInsertPoint(ftell_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *ret_ftell_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_ftell_alloc");
    llvm::Value *ret_ftell_err_ptr = builder->CreateStructGEP(function_result_type, ret_ftell_alloc, 0, "ret_ftell_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, NotReadable, NotReadableMessage);
    IR::aligned_store(*builder, err_value, ret_ftell_err_ptr);
    llvm::Value *ret_ftell_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_ftell_empty_str");
    llvm::Value *ret_ftell_val_ptr = builder->CreateStructGEP(function_result_type, ret_ftell_alloc, 1, "ret_ftell_val_ptr");
    IR::aligned_store(*builder, ret_ftell_empty_str, ret_ftell_val_ptr);
    llvm::Value *ret_ftell_val = IR::aligned_load(*builder, function_result_type, ret_ftell_alloc, "ret_ftell_val");
    builder->CreateRet(ret_ftell_val);

    // Return to beginning of file
    builder->SetInsertPoint(ftell_ok_block);
    llvm::Value *seek_set = builder->getInt32(0); // SEEK_SET is 0
    llvm::Value *seek_set_result = builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_set}, "seek_set_result");

    // Check if fseek SEEK_SET failed
    llvm::Value *seek_set_check = builder->CreateICmpNE(seek_set_result, builder->getInt32(0), "seek_set_check");
    builder->CreateCondBr(seek_set_check, seek_set_error_block, seek_set_ok_block);

    // Handle fseek SEEK_SET error, throw ErrIO.NotReadable
    builder->SetInsertPoint(seek_set_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *ret_seek_set_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_seek_set_alloc");
    llvm::Value *ret_seek_set_err_ptr = builder->CreateStructGEP(function_result_type, ret_seek_set_alloc, 0, "ret_seek_set_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, NotReadable, NotReadableMessage);
    IR::aligned_store(*builder, err_value, ret_seek_set_err_ptr);
    llvm::Value *ret_seek_set_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_seek_set_empty_str");
    llvm::Value *ret_seek_set_val_ptr = builder->CreateStructGEP(function_result_type, ret_seek_set_alloc, 1, "ret_seek_set_val_ptr");
    IR::aligned_store(*builder, ret_seek_set_empty_str, ret_seek_set_val_ptr);
    llvm::Value *ret_seek_set_val = IR::aligned_load(*builder, function_result_type, ret_seek_set_alloc, "ret_seek_set_val");
    builder->CreateRet(ret_seek_set_val);

    // Allocate memory for file content
    builder->SetInsertPoint(seek_set_ok_block);

    // Create string to hold file content
    llvm::Value *content = builder->CreateCall(create_str_fn, {file_size}, "content");

    // Get content->value pointer
    llvm::Value *content_value_ptr = builder->CreateStructGEP(str_type, content, 1, "content_value_ptr");

    // Read file: fread(content->value, 1, file_size, file)
    llvm::Value *bytes_read = builder->CreateCall(fread_fn, {content_value_ptr, builder->getInt64(1), file_size, file}, "bytes_read");

    // Close file
    builder->CreateCall(fclose_fn, {file});

    // Check if read was successful (bytes_read == file_size)
    llvm::Value *read_check = builder->CreateICmpNE(bytes_read, file_size, "read_check");
    builder->CreateCondBr(read_check, read_error_block, read_ok_block);

    // Handle read error, throw ErrIO.UnexpectedEOF
    builder->SetInsertPoint(read_error_block);
    builder->CreateCall(free_fn, {content});
    llvm::AllocaInst *ret_read_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_read_alloc");
    llvm::Value *ret_read_err_ptr = builder->CreateStructGEP(function_result_type, ret_read_alloc, 0, "ret_read_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, UnexpectedEOF, UnexpectedEOFMessage);
    IR::aligned_store(*builder, err_value, ret_read_err_ptr);
    llvm::Value *ret_read_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_read_empty_str");
    llvm::Value *ret_read_val_ptr = builder->CreateStructGEP(function_result_type, ret_read_alloc, 1, "ret_read_val_ptr");
    IR::aligned_store(*builder, ret_read_empty_str, ret_read_val_ptr);
    llvm::Value *ret_read_val = IR::aligned_load(*builder, function_result_type, ret_read_alloc, "ret_read_val");
    builder->CreateRet(ret_read_val);

    // Success - return content
    builder->SetInsertPoint(read_ok_block);
    llvm::AllocaInst *ret_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_alloc");
    llvm::Value *ret_err_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 0, "ret_err_ptr");
    IR::aligned_store(*builder, builder->getInt32(0), ret_err_ptr);
    llvm::Value *ret_val_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 1, "ret_val_ptr");
    IR::aligned_store(*builder, content, ret_val_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloc, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::FileSystem::generate_read_lines_function( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations                                  //
) {
    // THE C IMPLEMENTATION:
    // str *read_file_lines(const str *path) {
    //     char *c_path = (char *)path->value;
    //     // Open the file for reading
    //     FILE *file = fopen(c_path, "r");
    //     if (!file) {
    //         return NULL; // File open error
    //     }
    //     // First pass: Count the number of lines
    //     size_t line_count = 0;
    //     int ch;
    //     bool_t in_line = FALSE;
    //     while ((ch = fgetc(file)) != EOF) {
    //         if (ch == '\n') {
    //             line_count++;
    //             in_line = FALSE;
    //         } else if (!in_line) {
    //             in_line = TRUE;
    //         }
    //     }
    //     // Handle the case where the last line doesn't end with a newline
    //     if (in_line) {
    //         line_count++;
    //     }
    //     // Reset file pointer to beginning
    //     rewind(file);
    //     // Create the array of strings
    //     size_t lengths[1] = {line_count};
    //     str *lines_array = create_arr(1, sizeof(str *), lengths);
    //     if (!lines_array) {
    //         fclose(file);
    //         return NULL;
    //     }
    //     // Initialize array with NULL pointers
    //     str *null_ptr = NULL;
    //     fill_arr_inline(lines_array, sizeof(str *), &null_ptr);
    //     // Read lines and populate the array
    //     size_t line_idx = 0;
    //     char buffer[4096]; // Buffer for reading lines
    //     size_t idx[1];     // For accessing array elements
    //     while (fgets(buffer, sizeof(buffer), file)) {
    //         size_t len = strlen(buffer);
    //         // Remove trailing newline if present
    //         if (len > 0 && buffer[len - 1] == '\n') {
    //             buffer[--len] = '\0';
    //         }
    //         // Create str for this line
    //         str *line = init_str(buffer, len);
    //         if (!line) {
    //             // Clean up on error
    //             for (size_t i = 0; i < line_idx; i++) {
    //                 idx[0] = i;
    //                 str *line_str = *(str **)access_arr(lines_array, sizeof(str *), idx);
    //                 free(line_str);
    //             }
    //             free(lines_array);
    //             fclose(file);
    //             return NULL;
    //         }
    //         // Store in array
    //         idx[0] = line_idx;
    //         str **elem_ptr = (str **)access_arr(lines_array, sizeof(str *), idx);
    //         *elem_ptr = line;
    //         line_idx++;
    //     }
    //     // Check if we read fewer lines than expected (e.g., due to errors)
    //     if (line_idx < line_count) {
    //         // Adjust the array size (this is a simplification - in real code you might want to reallocate)
    //         size_t *dim_lengths = (size_t *)lines_array->value;
    //         dim_lengths[0] = line_idx;
    //     }
    //     fclose(file);
    //     return lines_array;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *fopen_fn = c_functions.at(FOPEN);
    llvm::Function *fclose_fn = c_functions.at(FCLOSE);
    llvm::Function *fgetc_fn = c_functions.at(FGETC);
    llvm::Function *fgets_fn = c_functions.at(FGETS);
    llvm::Function *rewind_fn = c_functions.at(REWIND);
    llvm::Function *strlen_fn = c_functions.at(STRLEN);

    // Get string and array utility functions
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *create_arr_fn = Array::array_manip_functions.at("create_arr");
    llvm::Function *fill_arr_inline_fn = Array::array_manip_functions.at("fill_arr_inline");
    llvm::Function *access_arr_fn = Array::array_manip_functions.at("access_arr");

    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotFound = 1;
    const std::string NotFoundMessage(ErrIOValues.at(NotFound).second);

    const unsigned int ErrIOCount = 5;
    const unsigned int ErrFS = Type::get_type_id_from_str("ErrFS");
    const std::vector<error_value> &ErrFSValues = std::get<2>(core_module_error_sets.at("filesystem").at(1));
    const unsigned int TooLarge = 5;
    const std::string TooLargeMessage(ErrFSValues.at(TooLarge - ErrIOCount).second);

    // Define return type - str* (array of strings)
    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *read_lines_type = llvm::FunctionType::get(function_result_type, {str_type->getPointerTo()}, false);
    llvm::Function *read_lines_fn = llvm::Function::Create(read_lines_type, llvm::Function::ExternalLinkage, hash + ".file_lines", module);
    fs_functions["read_lines"] = read_lines_fn;
    if (only_declarations) {
        return;
    }

    // Get the path parameter
    llvm::Argument *path_arg = read_lines_fn->arg_begin();
    path_arg->setName("path");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", read_lines_fn);
    llvm::BasicBlock *file_ok_block = llvm::BasicBlock::Create(context, "file_ok", read_lines_fn);
    llvm::BasicBlock *file_fail_block = llvm::BasicBlock::Create(context, "file_fail", read_lines_fn);
    llvm::BasicBlock *count_lines_loop = llvm::BasicBlock::Create(context, "count_lines_loop", read_lines_fn);
    llvm::BasicBlock *count_lines_end = llvm::BasicBlock::Create(context, "count_lines_end", read_lines_fn);
    llvm::BasicBlock *check_last_line = llvm::BasicBlock::Create(context, "check_last_line", read_lines_fn);
    llvm::BasicBlock *inc_line_count = llvm::BasicBlock::Create(context, "inc_line_count", read_lines_fn);
    llvm::BasicBlock *array_create_ok = llvm::BasicBlock::Create(context, "array_create_ok", read_lines_fn);
    llvm::BasicBlock *array_create_fail = llvm::BasicBlock::Create(context, "array_create_fail", read_lines_fn);
    llvm::BasicBlock *read_lines_loop = llvm::BasicBlock::Create(context, "read_lines_loop", read_lines_fn);
    llvm::BasicBlock *read_line_body = llvm::BasicBlock::Create(context, "read_line_body", read_lines_fn);
    llvm::BasicBlock *check_newline = llvm::BasicBlock::Create(context, "check_newline", read_lines_fn);
    llvm::BasicBlock *remove_newline = llvm::BasicBlock::Create(context, "remove_newline", read_lines_fn);
    llvm::BasicBlock *after_newline_check = llvm::BasicBlock::Create(context, "after_newline_check", read_lines_fn);
    llvm::BasicBlock *init_str_fail = llvm::BasicBlock::Create(context, "init_str_fail", read_lines_fn);
    llvm::BasicBlock *cleanup_loop = llvm::BasicBlock::Create(context, "cleanup_loop", read_lines_fn);
    llvm::BasicBlock *cleanup_body = llvm::BasicBlock::Create(context, "cleanup_body", read_lines_fn);
    llvm::BasicBlock *cleanup_end = llvm::BasicBlock::Create(context, "cleanup_end", read_lines_fn);
    llvm::BasicBlock *store_line = llvm::BasicBlock::Create(context, "store_line", read_lines_fn);
    llvm::BasicBlock *size_check = llvm::BasicBlock::Create(context, "size_check", read_lines_fn);
    llvm::BasicBlock *adjust_size = llvm::BasicBlock::Create(context, "adjust_size", read_lines_fn);
    llvm::BasicBlock *return_result = llvm::BasicBlock::Create(context, "return_result", read_lines_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the C string from the value
    llvm::Value *c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "r" string constant for fopen mode
    llvm::Value *mode_str = IR::generate_const_string(module, "r");

    // Open file: file = fopen(c_path, "r")
    llvm::Value *file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open failure, throw ErrIO.NotFound
    builder->SetInsertPoint(file_fail_block);
    llvm::AllocaInst *ret_file_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_fail_alloc");
    llvm::Value *ret_file_fail_err_ptr = builder->CreateStructGEP(function_result_type, ret_file_fail_alloc, 0, "ret_file_fail_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrFS, NotFound, NotFoundMessage);
    IR::aligned_store(*builder, err_value, ret_file_fail_err_ptr);
    llvm::Value *ret_file_fail_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_file_fail_empty_str");
    llvm::Value *ret_file_fail_val_ptr = builder->CreateStructGEP(function_result_type, ret_file_fail_alloc, 1, "ret_file_fail_val_ptr");
    IR::aligned_store(*builder, ret_file_fail_empty_str, ret_file_fail_val_ptr);
    llvm::Value *ret_file_fail_val = IR::aligned_load(*builder, function_result_type, ret_file_fail_alloc, "ret_file_fail_val");
    builder->CreateRet(ret_file_fail_val);

    // Continue with successful file open - count lines
    builder->SetInsertPoint(file_ok_block);

    // Initialize line counting variables
    llvm::AllocaInst *line_count_var = builder->CreateAlloca(builder->getInt64Ty(), 0, "line_count_var");
    IR::aligned_store(*builder, builder->getInt64(0), line_count_var);

    llvm::AllocaInst *in_line_var = builder->CreateAlloca(builder->getInt1Ty(), 0, "in_line_var");
    IR::aligned_store(*builder, builder->getFalse(), in_line_var);

    llvm::AllocaInst *ch_var = builder->CreateAlloca(builder->getInt32Ty(), 0, "ch_var");

    // Start line counting loop
    builder->CreateBr(count_lines_loop);

    // Line counting loop header
    builder->SetInsertPoint(count_lines_loop);
    llvm::Value *ch = builder->CreateCall(fgetc_fn, {file}, "ch");
    IR::aligned_store(*builder, ch, ch_var);

    // Check if we hit EOF
    llvm::Value *is_eof = builder->CreateICmpEQ(ch, builder->getInt32(-1), "is_eof");
    builder->CreateCondBr(is_eof, check_last_line, count_lines_end);

    // Line counting loop body
    builder->SetInsertPoint(count_lines_end);

    // Check if character is newline
    llvm::Value *is_newline = builder->CreateICmpEQ(ch, builder->getInt32('\n'), "is_newline");

    // If newline, increment line count and reset in_line
    llvm::Value *current_line_count = IR::aligned_load(*builder, builder->getInt64Ty(), line_count_var, "current_line_count");
    llvm::Value *incremented_count = builder->CreateAdd(current_line_count, builder->getInt64(1), "incremented_count");

    // Use select for conditional stores
    llvm::Value *current_in_line = IR::aligned_load(*builder, builder->getInt1Ty(), in_line_var, "current_in_line");
    llvm::Value *new_line_count = builder->CreateSelect(is_newline, incremented_count, current_line_count, "new_line_count");
    IR::aligned_store(*builder, new_line_count, line_count_var);

    llvm::Value *new_in_line;
    if (is_newline->getType()->isIntegerTy(1)) {
        // If is_newline is already i1, use it directly with not operation
        new_in_line = builder->CreateSelect(                                                                       //
            is_newline, builder->getFalse(), builder->CreateOr(current_in_line, builder->getTrue()), "new_in_line" //
        );
    } else {
        // Convert is_newline to i1 type
        llvm::Value *is_newline_i1 = builder->CreateICmpNE(is_newline, builder->getInt32(0), "is_newline_i1");
        new_in_line = builder->CreateSelect(                                                                          //
            is_newline_i1, builder->getFalse(), builder->CreateOr(current_in_line, builder->getTrue()), "new_in_line" //
        );
    }
    IR::aligned_store(*builder, new_in_line, in_line_var);

    // Continue the loop
    builder->CreateBr(count_lines_loop);

    // Check if last line needs to be counted
    builder->SetInsertPoint(check_last_line);
    llvm::Value *final_in_line = IR::aligned_load(*builder, builder->getInt1Ty(), in_line_var, "final_in_line");
    builder->CreateCondBr(final_in_line, inc_line_count, array_create_ok);

    // Increment line count for the last line
    builder->SetInsertPoint(inc_line_count);
    llvm::Value *final_line_count = IR::aligned_load(*builder, builder->getInt64Ty(), line_count_var, "final_line_count");
    llvm::Value *final_incremented_count = builder->CreateAdd(final_line_count, builder->getInt64(1), "final_incremented_count");
    IR::aligned_store(*builder, final_incremented_count, line_count_var);
    builder->CreateBr(array_create_ok);

    // Create array of strings
    builder->SetInsertPoint(array_create_ok);

    // Rewind file to beginning
    builder->CreateCall(rewind_fn, {file});

    // Create array with 1 dimension of size line_count
    llvm::Value *final_count = IR::aligned_load(*builder, builder->getInt64Ty(), line_count_var, "final_count");

    // Create an array for the dimension lengths
    llvm::AllocaInst *lengths_alloca = builder->CreateAlloca(builder->getInt64Ty(), builder->getInt32(1), "lengths_alloca");
    IR::aligned_store(*builder, final_count, lengths_alloca);

    // Create the array of strings
    llvm::Value *lines_array = builder->CreateCall(create_arr_fn,
        {
            builder->getInt64(1),              // 1 dimension
            builder->getInt64(sizeof(void *)), // Size of str pointer
            lengths_alloca                     // Array of dimension lengths
        },                                     //
        "lines_array"                          //
    );

    // Check if array creation was successful
    llvm::Value *array_null = builder->CreateIsNull(lines_array, "array_null");
    builder->CreateCondBr(array_null, array_create_fail, read_lines_loop);

    // Handle array creation failure, throw ErrFS.TooLarge
    builder->SetInsertPoint(array_create_fail);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *ret_array_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_array_fail_alloc");
    llvm::Value *ret_array_fail_err_ptr = builder->CreateStructGEP(function_result_type, ret_array_fail_alloc, 0, "ret_array_fail_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrFS, TooLarge, TooLargeMessage);
    IR::aligned_store(*builder, err_value, ret_array_fail_err_ptr);
    llvm::Value *ret_array_fail_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_array_fail_empty_str");
    llvm::Value *ret_array_fail_val_ptr = builder->CreateStructGEP(function_result_type, ret_array_fail_alloc, 1, "ret_array_fail_val_ptr");
    IR::aligned_store(*builder, ret_array_fail_empty_str, ret_array_fail_val_ptr);
    llvm::Value *ret_array_fail_val = IR::aligned_load(*builder, function_result_type, ret_array_fail_alloc, "ret_array_fail_val");
    builder->CreateRet(ret_array_fail_val);

    // Initialize array with NULL pointers
    builder->SetInsertPoint(read_lines_loop);

    // Create a NULL str pointer to fill array
    llvm::AllocaInst *null_str_ptr = builder->CreateAlloca(str_type->getPointerTo(), 0, "null_str_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(str_type->getPointerTo()), null_str_ptr);

    // Fill array with NULL pointers
    builder->CreateCall(fill_arr_inline_fn,
        {
            lines_array,                       // Array to fill
            builder->getInt64(sizeof(void *)), // Size of element
            null_str_ptr                       // Value to fill with
        });

    // Allocate buffer for reading lines (4096 bytes)
    llvm::AllocaInst *buffer = builder->CreateAlloca(builder->getInt8Ty(), builder->getInt32(4096), "buffer");

    // Create an array for accessing array elements (1 index)
    llvm::AllocaInst *idx_alloca = builder->CreateAlloca(builder->getInt64Ty(), builder->getInt32(1), "idx_alloca");

    // Initialize line index
    llvm::AllocaInst *line_idx_var = builder->CreateAlloca(builder->getInt64Ty(), 0, "line_idx_var");
    IR::aligned_store(*builder, builder->getInt64(0), line_idx_var);

    // Start reading lines
    builder->CreateBr(read_line_body);

    // Read lines loop
    builder->SetInsertPoint(read_line_body);

    // Call fgets(buffer, 4096, file)
    llvm::Value *fgets_result = builder->CreateCall(fgets_fn, {buffer, builder->getInt32(4096), file}, "fgets_result");

    // Check if fgets returned NULL (EOF or error)
    llvm::Value *fgets_null = builder->CreateIsNull(fgets_result, "fgets_null");
    builder->CreateCondBr(fgets_null, size_check, check_newline);

    // Check for newline and remove it
    builder->SetInsertPoint(check_newline);

    // Get line length: strlen(buffer)
    llvm::Value *line_len = builder->CreateCall(strlen_fn, {buffer}, "line_len");

    // Check if line ends with newline
    llvm::Value *has_newline = builder->CreateICmpNE(line_len, builder->getInt64(0), "has_len");
    builder->CreateCondBr(has_newline, remove_newline, after_newline_check);

    // Remove trailing newline
    builder->SetInsertPoint(remove_newline);

    // Get last character index: len - 1
    llvm::Value *last_idx = builder->CreateSub(line_len, builder->getInt64(1), "last_idx");

    // Get pointer to last character
    llvm::Value *last_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer, last_idx, "last_char_ptr");

    // Load the last character
    llvm::Value *last_char = IR::aligned_load(*builder, builder->getInt8Ty(), last_char_ptr, "last_char");

    // Check if last character is newline
    llvm::Value *is_last_newline = builder->CreateICmpEQ(last_char, builder->getInt8('\n'), "is_last_newline");

    // If newline, create new length by decrementing
    llvm::Value *new_len = builder->CreateSelect(is_last_newline, builder->CreateSub(line_len, builder->getInt64(1)), line_len, "new_len");

    // If newline, replace it with null terminator
    IR::aligned_store(*builder, builder->CreateSelect(is_last_newline, builder->getInt8(0), last_char), last_char_ptr);

    // Continue with or without newline
    builder->CreateBr(after_newline_check);

    // Create string for the line
    builder->SetInsertPoint(after_newline_check);

    // Use final line length (after potential newline removal)
    llvm::PHINode *final_len = builder->CreatePHI(builder->getInt64Ty(), 2, "final_len");
    final_len->addIncoming(line_len, check_newline);
    final_len->addIncoming(new_len, remove_newline);

    // Create string from buffer: init_str(buffer, len)
    llvm::Value *line_str = builder->CreateCall(init_str_fn, {buffer, final_len}, "line_str");

    // Check if string creation was successful
    llvm::Value *line_null = builder->CreateIsNull(line_str, "line_null");
    builder->CreateCondBr(line_null, init_str_fail, store_line);

    // Handle string creation failure - clean up previous lines
    builder->SetInsertPoint(init_str_fail);

    // Load current line index
    llvm::Value *cleanup_line_idx = IR::aligned_load(*builder, builder->getInt64Ty(), line_idx_var, "cleanup_line_idx");

    // Initialize loop counter for cleanup
    llvm::AllocaInst *cleanup_i = builder->CreateAlloca(builder->getInt64Ty(), 0, "cleanup_i");
    IR::aligned_store(*builder, builder->getInt64(0), cleanup_i);

    // Start cleanup loop
    builder->CreateBr(cleanup_loop);

    // Cleanup loop header
    builder->SetInsertPoint(cleanup_loop);
    llvm::Value *i = IR::aligned_load(*builder, builder->getInt64Ty(), cleanup_i, "i");
    llvm::Value *cleanup_done = builder->CreateICmpUGE(i, cleanup_line_idx, "cleanup_done");
    builder->CreateCondBr(cleanup_done, cleanup_end, cleanup_body);

    // Cleanup loop body
    builder->SetInsertPoint(cleanup_body);

    // Store index for array access
    IR::aligned_store(*builder, i, idx_alloca);

    // Access array element: access_arr(lines_array, sizeof(str*), idx)
    llvm::Value *elem_ptr = builder->CreateCall(access_arr_fn, {lines_array, builder->getInt64(sizeof(void *)), idx_alloca}, "elem_ptr");

    // Load the string pointer
    llvm::Value *elem_str_ptr = IR::aligned_load(*builder, str_type->getPointerTo(), elem_ptr, "elem_str_ptr");

    // Free the string
    builder->CreateCall(free_fn, {elem_str_ptr});

    // Increment cleanup counter
    llvm::Value *next_i = builder->CreateAdd(i, builder->getInt64(1), "next_i");
    IR::aligned_store(*builder, next_i, cleanup_i);

    // Continue cleanup loop
    builder->CreateBr(cleanup_loop);

    // Cleanup finished, free array and return NULL
    builder->SetInsertPoint(cleanup_end);
    builder->CreateCall(free_fn, {lines_array});
    builder->CreateCall(fclose_fn, {file});

    // Throw error ErrFS.TooLarge
    llvm::AllocaInst *ret_init_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_init_fail_alloc");
    llvm::Value *ret_init_fail_err_ptr = builder->CreateStructGEP(function_result_type, ret_init_fail_alloc, 0, "ret_init_fail_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrFS, TooLarge, TooLargeMessage);
    IR::aligned_store(*builder, err_value, ret_init_fail_err_ptr);
    llvm::Value *ret_init_fail_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_init_fail_empty_str");
    llvm::Value *ret_init_fail_val_ptr = builder->CreateStructGEP(function_result_type, ret_init_fail_alloc, 1, "ret_init_fail_val_ptr");
    IR::aligned_store(*builder, ret_init_fail_empty_str, ret_init_fail_val_ptr);
    llvm::Value *ret_init_fail_val = IR::aligned_load(*builder, function_result_type, ret_init_fail_alloc, "ret_init_fail_val");
    builder->CreateRet(ret_init_fail_val);

    // Store line in array
    builder->SetInsertPoint(store_line);

    // Get current line index
    llvm::Value *current_idx = IR::aligned_load(*builder, builder->getInt64Ty(), line_idx_var, "current_idx");

    // Store index for array access
    IR::aligned_store(*builder, current_idx, idx_alloca);

    // Access array element: access_arr(lines_array, sizeof(str*), idx)
    llvm::Value *line_elem_ptr = builder->CreateCall(                                                //
        access_arr_fn, {lines_array, builder->getInt64(sizeof(void *)), idx_alloca}, "line_elem_ptr" //
    );

    // Store the string pointer in the array
    IR::aligned_store(*builder, line_str, line_elem_ptr);

    // Increment line index
    llvm::Value *next_line_idx = builder->CreateAdd(current_idx, builder->getInt64(1), "next_line_idx");
    IR::aligned_store(*builder, next_line_idx, line_idx_var);

    // Continue reading next line
    builder->CreateBr(read_line_body);

    // Check if we read the expected number of lines
    builder->SetInsertPoint(size_check);

    // Get final line count
    llvm::Value *expected_count = IR::aligned_load(*builder, builder->getInt64Ty(), line_count_var, "expected_count");
    llvm::Value *actual_count = IR::aligned_load(*builder, builder->getInt64Ty(), line_idx_var, "actual_count");

    // Check if actual count is less than expected
    llvm::Value *count_mismatch = builder->CreateICmpULT(actual_count, expected_count, "count_mismatch");
    builder->CreateCondBr(count_mismatch, adjust_size, return_result);

    // Adjust array size if needed
    builder->SetInsertPoint(adjust_size);

    // Get pointer to dimension lengths in array (first element of array->value)
    llvm::Value *array_value_ptr = builder->CreateStructGEP(str_type, lines_array, 1, "array_value_ptr");
    llvm::Value *dim_lengths = IR::aligned_load(*builder, builder->getInt8Ty()->getPointerTo(), array_value_ptr, "dim_lengths");
    llvm::Value *dim_lengths_cast = builder->CreateBitCast(dim_lengths, builder->getInt64Ty()->getPointerTo(), "dim_lengths_cast");

    // Update first dimension length
    llvm::Value *dim_first = builder->CreateGEP(builder->getInt64Ty(), dim_lengths_cast, builder->getInt32(0), "dim_first");
    IR::aligned_store(*builder, actual_count, dim_first);

    // Return array
    builder->CreateBr(return_result);

    // Return the array of lines
    builder->SetInsertPoint(return_result);
    builder->CreateCall(fclose_fn, {file});

    llvm::AllocaInst *ret_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_alloc");
    llvm::Value *ret_err_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 0, "ret_err_ptr");
    llvm::StructType *err_type = type_map.at("__flint_type_err");
    llvm::Value *err_struct = IR::get_default_value_of_type(err_type);
    IR::aligned_store(*builder, err_struct, ret_err_ptr);
    llvm::Value *ret_val_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 1, "ret_val_ptr");
    IR::aligned_store(*builder, lines_array, ret_val_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloc, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::FileSystem::generate_file_exists_function( //
    llvm::IRBuilder<> *builder,                                    //
    llvm::Module *module,                                          //
    const bool only_declarations                                   //
) {
    // THE C IMPLEMENTATION:
    // bool file_exists(const str *path) {
    //     char *c_path = (char *)path->value;
    //     // Try to open the file
    //     FILE *file = fopen(c_path, "r");
    //     // Check if file opened successfully
    //     if (file) {
    //         fclose(file);
    //         return true;
    //     }
    //     return false;
    // }
    // Get required function pointers
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *fopen_fn = c_functions.at(FOPEN);
    llvm::Function *fclose_fn = c_functions.at(FCLOSE);

    llvm::FunctionType *file_exists_type = llvm::FunctionType::get( //
        llvm::Type::getInt1Ty(context),                             // return bool
        {str_type->getPointerTo()},                                 // str* path
        false                                                       // No vaarg
    );
    llvm::Function *file_exists_fn = llvm::Function::Create(                             //
        file_exists_type, llvm::Function::ExternalLinkage, hash + ".file_exists", module //
    );
    fs_functions["file_exists"] = file_exists_fn;
    if (only_declarations) {
        return;
    }

    // Get the path parameter
    llvm::Argument *path_arg = file_exists_fn->arg_begin();
    path_arg->setName("path");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", file_exists_fn);
    llvm::BasicBlock *file_ok_block = llvm::BasicBlock::Create(context, "file_ok", file_exists_fn);
    llvm::BasicBlock *file_fail_block = llvm::BasicBlock::Create(context, "file_fail", file_exists_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "r" string constant for fopen mode
    llvm::Value *mode_str = IR::generate_const_string(module, "r");

    // Open file: file = fopen(c_path, "r")
    llvm::Value *file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open success
    builder->SetInsertPoint(file_ok_block);
    builder->CreateCall(fclose_fn, {file});
    builder->CreateRet(builder->getTrue()); // Return true

    // Handle file open failure
    builder->SetInsertPoint(file_fail_block);
    builder->CreateRet(builder->getFalse()); // Return false
}

void Generator::Module::FileSystem::generate_write_file_function( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations                                  //
) {
    // THE C IMPLEMENTATION:
    // void write_file(const str *path, const str *content) {
    //     char *c_path = (char *)path->value;
    //     // Open the file for writing - this will create a new file or overwrite an existing one
    //     FILE *file = fopen(c_path, "wb");
    //     if (!file) {
    //         return; // File open error
    //     }
    //     // Write content to the file
    //     fwrite(content->value, 1, content->len, file);
    //     // Close the file
    //     fclose(file);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *fopen_fn = c_functions.at(FOPEN);
    llvm::Function *fwrite_fn = c_functions.at(FWRITE);
    llvm::Function *fclose_fn = c_functions.at(FCLOSE);
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotWritable = 3;
    const std::string NotWritableMessage(ErrIOValues.at(NotWritable).second);

    const unsigned int ErrIOCount = 5;
    const unsigned int ErrFS = Type::get_type_id_from_str("ErrFS");
    const std::vector<error_value> &ErrFSValues = std::get<2>(core_module_error_sets.at("filesystem").at(1));
    const unsigned int InvalidPath = 6;
    const std::string InvalidPathMessage(ErrFSValues.at(InvalidPath - ErrIOCount).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *write_file_type = llvm::FunctionType::get( //
        function_result_type,                                      // Return type: struct with error code
        {str_type->getPointerTo(), str_type->getPointerTo()},      // Parameters: const str *path, const str *content
        false                                                      // Not variadic
    );
    llvm::Function *write_file_fn = llvm::Function::Create(write_file_type, llvm::Function::ExternalLinkage, hash + ".write_file", module);
    fs_functions["write_file"] = write_file_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *path_arg = write_file_fn->arg_begin();
    llvm::Argument *content_arg = write_file_fn->arg_begin() + 1;
    path_arg->setName("path");
    content_arg->setName("content");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", write_file_fn);
    llvm::BasicBlock *file_fail_block = llvm::BasicBlock::Create(context, "file_fail", write_file_fn);
    llvm::BasicBlock *file_ok_block = llvm::BasicBlock::Create(context, "file_ok", write_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "wb" string constant for fopen mode
    llvm::Value *mode_str = IR::generate_const_string(module, "wb");

    // Open file: file = fopen(c_path, "wb")
    llvm::Value *file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open failure, throw ErrFS.InvalidPath
    builder->SetInsertPoint(file_fail_block);
    llvm::AllocaInst *ret_file_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_fail_alloc");
    llvm::Value *ret_file_fail_err_ptr = builder->CreateStructGEP(function_result_type, ret_file_fail_alloc, 0, "ret_file_fail_err_ptr");
    llvm::Value *error_value = IR::generate_err_value(*builder, module, ErrFS, InvalidPath, InvalidPathMessage);
    IR::aligned_store(*builder, error_value, ret_file_fail_err_ptr);
    llvm::Value *ret_file_fail_val = IR::aligned_load(*builder, function_result_type, ret_file_fail_alloc, "ret_file_fail_val");
    builder->CreateRet(ret_file_fail_val);

    // Write content to file
    builder->SetInsertPoint(file_ok_block);

    // Get content->len
    llvm::Value *content_len_ptr = builder->CreateStructGEP(str_type, content_arg, 0, "content_len_ptr");
    llvm::Value *content_len = IR::aligned_load(*builder, builder->getInt64Ty(), content_len_ptr, "content_len");

    // Get content->value
    llvm::Value *content_value_ptr = builder->CreateStructGEP(str_type, content_arg, 1, "content_value_ptr");

    // Write to file: fwrite(content->value, 1, content->len, file)
    llvm::Value *bytes_written = builder->CreateCall(                                            //
        fwrite_fn, {content_value_ptr, builder->getInt64(1), content_len, file}, "bytes_written" //
    );

    // Close the file
    builder->CreateCall(fclose_fn, {file});

    // Check if write was successful (bytes_written == content_len)
    llvm::Value *write_check = builder->CreateICmpEQ(bytes_written, content_len, "write_check");

    // For write failure, return error 132
    llvm::AllocaInst *ret_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_alloc");
    llvm::Value *ret_err_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 0, "ret_err_ptr");
    error_value = IR::generate_err_value(*builder, module, ErrFS, NotWritable, NotWritableMessage);
    llvm::StructType *err_type = type_map.at("__flint_type_err");
    llvm::Value *no_error_value = IR::get_default_value_of_type(err_type);
    llvm::Value *ret_err_val = builder->CreateSelect(write_check, no_error_value, error_value);
    IR::aligned_store(*builder, ret_err_val, ret_err_ptr);

    // Return empty string in the data portion regardless of success/failure
    llvm::Value *ret_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_empty_str");
    llvm::Value *ret_val_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 1, "ret_val_ptr");
    IR::aligned_store(*builder, ret_empty_str, ret_val_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloc, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::FileSystem::generate_append_file_function( //
    llvm::IRBuilder<> *builder,                                    //
    llvm::Module *module,                                          //
    const bool only_declarations                                   //
) {
    // THE C IMPLEMENTATION:
    // void append_file(const str *path, const str *content) {
    //     char *c_path = (char *)path->value;
    //     // Open the file for appending
    //     FILE *file = fopen(c_path, "ab");
    //     if (!file) {
    //         return; // File open error
    //     }
    //     // Append content to the file
    //     fwrite(content->value, 1, content->len, file);
    //     // Close the file
    //     fclose(file);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *fopen_fn = c_functions.at(FOPEN);
    llvm::Function *fwrite_fn = c_functions.at(FWRITE);
    llvm::Function *fclose_fn = c_functions.at(FCLOSE);

    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotWritable = 3;
    const std::string NotWritableMessage(ErrIOValues.at(NotWritable).second);

    const unsigned int ErrIOCount = 5;
    const unsigned int ErrFS = Type::get_type_id_from_str("ErrFS");
    const std::vector<error_value> &ErrFSValues = std::get<2>(core_module_error_sets.at("filesystem").at(1));
    const unsigned int InvalidPath = 6;
    const std::string InvalidPathMessage(ErrFSValues.at(InvalidPath - ErrIOCount).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("void");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *append_file_type = llvm::FunctionType::get( //
        function_result_type,                                       // return struct with error code
        {str_type->getPointerTo(), str_type->getPointerTo()},       // Parameters: const str *path, const str *content
        false                                                       // No vaarg
    );
    llvm::Function *append_file_fn = llvm::Function::Create(                             //
        append_file_type, llvm::Function::ExternalLinkage, hash + ".append_file", module //
    );
    fs_functions["append_file"] = append_file_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *path_arg = append_file_fn->arg_begin();
    llvm::Argument *content_arg = append_file_fn->arg_begin() + 1;
    path_arg->setName("path");
    content_arg->setName("content");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", append_file_fn);
    llvm::BasicBlock *file_fail_block = llvm::BasicBlock::Create(context, "file_fail", append_file_fn);
    llvm::BasicBlock *file_ok_block = llvm::BasicBlock::Create(context, "file_ok", append_file_fn);
    llvm::BasicBlock *write_fail_block = llvm::BasicBlock::Create(context, "write_fail", append_file_fn);
    llvm::BasicBlock *write_ok_block = llvm::BasicBlock::Create(context, "write_ok", append_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "ab" string constant for fopen mode (append binary)
    llvm::Value *mode_str = IR::generate_const_string(module, "ab");

    // Open file: file = fopen(c_path, "ab")
    llvm::Value *file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open failure, throw ErrFS.InvalidPath
    builder->SetInsertPoint(file_fail_block);
    llvm::AllocaInst *ret_file_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_fail_alloc");
    llvm::Value *ret_file_fail_err_ptr = builder->CreateStructGEP(function_result_type, ret_file_fail_alloc, 0, "ret_file_fail_err_ptr");
    llvm::Value *error_value = IR::generate_err_value(*builder, module, ErrFS, InvalidPath, InvalidPathMessage);
    IR::aligned_store(*builder, error_value, ret_file_fail_err_ptr);
    llvm::Value *ret_file_fail_val = IR::aligned_load(*builder, function_result_type, ret_file_fail_alloc, "ret_file_fail_val");
    builder->CreateRet(ret_file_fail_val);

    // Append content to file
    builder->SetInsertPoint(file_ok_block);

    // Get content->len
    llvm::Value *content_len_ptr = builder->CreateStructGEP(str_type, content_arg, 0, "content_len_ptr");
    llvm::Value *content_len = IR::aligned_load(*builder, builder->getInt64Ty(), content_len_ptr, "content_len");

    // Get content->value
    llvm::Value *content_value_ptr = builder->CreateStructGEP(str_type, content_arg, 1, "content_value_ptr");

    // Write to file: fwrite(content->value, 1, content->len, file)
    llvm::Value *bytes_written = builder->CreateCall(                                            //
        fwrite_fn, {content_value_ptr, builder->getInt64(1), content_len, file}, "bytes_written" //
    );

    // Close the file
    builder->CreateCall(fclose_fn, {file});

    // Check if write was successful (bytes_written == content_len)
    llvm::Value *write_check = builder->CreateICmpEQ(bytes_written, content_len, "write_check");
    builder->CreateCondBr(write_check, write_ok_block, write_fail_block);

    // Throw ErrFS.NotWritable when the written bytes differ with the length
    builder->SetInsertPoint(write_fail_block);
    llvm::AllocaInst *ret_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_alloc");
    llvm::Value *ret_err_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 0, "ret_err_ptr");
    error_value = IR::generate_err_value(*builder, module, ErrFS, NotWritable, NotWritableMessage);
    IR::aligned_store(*builder, error_value, ret_err_ptr);
    llvm::Value *ret_val = IR::aligned_load(*builder, function_result_type, ret_alloc, "ret_val");
    builder->CreateRet(ret_val);

    // Return a zeroinitialized return value if everything went okay, as this function has a void return type annyway
    builder->SetInsertPoint(write_ok_block);
    builder->CreateRet(IR::get_default_value_of_type(function_result_type));
}

void Generator::Module::FileSystem::generate_is_file_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // bool is_file(const str *path) {
    //     char *c_path = (char *)path->value;
    //     // Try to open as a file
    //     FILE *file = fopen(c_path, "rb");
    //
    //     if (file) {
    //         // Check if it's actually a file by trying to read from it
    //         char buffer[1];
    //         size_t read_result = fread(buffer, 1, 1, file);
    //         // Seek back to the beginning
    //         fseek(file, 0, SEEK_SET);
    //         fclose(file);
    //
    //         // If we can read from it or it's an empty file, it's a regular file
    //         return TRUE;
    //     }
    //
    //     return FALSE;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *fopen_fn = c_functions.at(FOPEN);
    llvm::Function *fread_fn = c_functions.at(FREAD);
    llvm::Function *fseek_fn = c_functions.at(FSEEK);
    llvm::Function *fclose_fn = c_functions.at(FCLOSE);

    llvm::FunctionType *is_file_type = llvm::FunctionType::get( //
        llvm::Type::getInt1Ty(context),                         // return bool
        {str_type->getPointerTo()},                             // str *path
        false                                                   // No vaarg
    );
    llvm::Function *is_file_fn = llvm::Function::Create(                         //
        is_file_type, llvm::Function::ExternalLinkage, hash + ".is_file", module //
    );
    fs_functions["is_file"] = is_file_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameter
    llvm::Argument *path_arg = is_file_fn->arg_begin();
    path_arg->setName("path");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", is_file_fn);
    llvm::BasicBlock *file_fail_block = llvm::BasicBlock::Create(context, "file_fail", is_file_fn);
    llvm::BasicBlock *file_ok_block = llvm::BasicBlock::Create(context, "file_ok", is_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "rb" string constant for fopen mode (read binary)
    llvm::Value *mode_str = IR::generate_const_string(module, "rb");

    // Open file: file = fopen(c_path, "rb")
    llvm::Value *file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open failure - return false
    builder->SetInsertPoint(file_fail_block);
    builder->CreateRet(builder->getFalse());

    // File opened successfully, check if it's a real file
    builder->SetInsertPoint(file_ok_block);

    // Create a buffer to read one byte
    llvm::AllocaInst *buffer = builder->CreateAlloca(builder->getInt8Ty(), nullptr, "buffer");

    // Try to read 1 byte: fread(buffer, 1, 1, file)
    builder->CreateCall(fread_fn, {buffer, builder->getInt64(1), builder->getInt64(1), file}, "read_result");

    // Seek back to beginning: fseek(file, 0, SEEK_SET)
    llvm::Value *seek_set = builder->getInt32(0);
    builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_set});

    // Close the file: fclose(file)
    builder->CreateCall(fclose_fn, {file});

    // If we got here, it's a file - return true
    builder->CreateRet(builder->getTrue());
}
