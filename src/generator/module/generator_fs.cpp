#include "generator/generator.hpp"

void Generator::Module::FS::generate_filesystem_functions( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations                           //
) {
    generate_read_file_function(builder, module, only_declarations);
    generate_read_file_lines_function(builder, module, only_declarations);
}

void Generator::Module::FS::generate_read_file_function( //
    llvm::IRBuilder<> *builder,                          //
    llvm::Module *module,                                //
    const bool only_declarations                         //
) {
    // THE C IMPLEMENTATION:
    // str *read_file(const str *path) {
    //     // Convert the str to null-terminated C string
    //     char *c_path = (char *)malloc(path->len + 1);
    //     memcpy(c_path, path->value, path->len);
    //     c_path[path->len] = '\0';
    //     // Open the file for reading in binary mode
    //     FILE *file = fopen(c_path, "rb");
    //     free(c_path);
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
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *malloc_fn = c_functions.at(MALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *fopen_fn = c_functions.at(FOPEN);
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *fseek_fn = c_functions.at(FSEEK);
    llvm::Function *fclose_fn = c_functions.at(FCLOSE);
    llvm::Function *ftell_fn = c_functions.at(FTELL);
    llvm::Function *fread_fn = c_functions.at(FREAD);
    llvm::Function *create_str_fn = String::string_manip_functions.at("create_str");

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *function_result_type = IR::add_and_or_get_type(result_type_ptr, true);
    llvm::FunctionType *read_file_type = llvm::FunctionType::get( //
        function_result_type,                                     // Return type: str*
        {str_type->getPointerTo()},                               // Parameter: const str* path
        false                                                     // Not variadic
    );
    llvm::Function *read_file_fn = llvm::Function::Create(read_file_type, llvm::Function::ExternalLinkage, "__flint_read_file", module);
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

    // Get path->len
    llvm::Value *path_len_ptr = builder->CreateStructGEP(str_type, path_arg, 0, "path_len_ptr");
    llvm::Value *path_len = builder->CreateLoad(builder->getInt64Ty(), path_len_ptr, "path_len");

    // Calculate allocation size: path->len + 1 (for null terminator)
    llvm::Value *c_path_size = builder->CreateAdd(path_len, builder->getInt64(1), "c_path_size");

    // Allocate memory for C string: c_path = malloc(path->len + 1)
    llvm::Value *c_path = builder->CreateCall(malloc_fn, {c_path_size}, "c_path");

    // Get path->value
    llvm::Value *path_value_ptr = builder->CreateStructGEP(str_type, path_arg, 1, "path_value_ptr");

    // Copy path content: memcpy(c_path, path->value, path->len)
    builder->CreateCall(memcpy_fn, {c_path, path_value_ptr, path_len});

    // Add null terminator: c_path[path->len] = '\0'
    llvm::Value *null_ptr = builder->CreateGEP(builder->getInt8Ty(), c_path, path_len, "null_ptr");
    builder->CreateStore(builder->getInt8(0), null_ptr);

    // Create "rb" string constant
    llvm::Value *mode_str = builder->CreateGlobalStringPtr("rb", "rb_mode");

    // Open file: file = fopen(c_path, "rb")
    llvm::Value *file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Free the c_path: free(c_path)
    builder->CreateCall(free_fn, {c_path});

    // Check if file is NULL
    llvm::Value *file_null_check = builder->CreateIsNull(file, "file_is_null");
    builder->CreateCondBr(file_null_check, file_null_block, file_valid_block);

    // Handle NULL file, throw 120
    builder->SetInsertPoint(file_null_block);
    llvm::AllocaInst *ret_file_null_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_null_alloc");
    llvm::Value *ret_file_null_err_ptr = builder->CreateStructGEP(function_result_type, ret_file_null_alloc, 0, "ret_file_null_err_ptr");
    builder->CreateStore(builder->getInt32(120), ret_file_null_err_ptr);
    llvm::Value *ret_file_null_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_file_null_empty_str");
    llvm::Value *ret_file_null_val_ptr = builder->CreateStructGEP(function_result_type, ret_file_null_alloc, 1, "ret_file_null_val_ptr");
    builder->CreateStore(ret_file_null_empty_str, ret_file_null_val_ptr);
    llvm::Value *ret_file_null_val = builder->CreateLoad(function_result_type, ret_file_null_alloc, "ret_file_null_val");
    builder->CreateRet(ret_file_null_val);

    // Continue with valid file
    builder->SetInsertPoint(file_valid_block);

    // fseek(file, 0, SEEK_END)
    llvm::Value *seek_end = builder->getInt32(2); // SEEK_END is 2
    llvm::Value *seek_end_result = builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_end}, "seek_end_result");

    // Check if fseek failed
    llvm::Value *seek_end_check = builder->CreateICmpNE(seek_end_result, builder->getInt32(0), "seek_end_check");
    builder->CreateCondBr(seek_end_check, seek_end_error_block, seek_end_ok_block);

    // Handle fseek SEEK_END error, throw 121
    builder->SetInsertPoint(seek_end_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *ret_seek_end_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_seek_end_alloc");
    llvm::Value *ret_seek_end_err_ptr = builder->CreateStructGEP(function_result_type, ret_seek_end_alloc, 0, "ret_seek_end_err_ptr");
    builder->CreateStore(builder->getInt32(121), ret_seek_end_err_ptr);
    llvm::Value *ret_seek_end_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_seek_end_empty_str");
    llvm::Value *ret_seek_end_val_ptr = builder->CreateStructGEP(function_result_type, ret_seek_end_alloc, 1, "ret_seek_end_val_ptr");
    builder->CreateStore(ret_seek_end_empty_str, ret_seek_end_val_ptr);
    llvm::Value *ret_seek_end_val = builder->CreateLoad(function_result_type, ret_seek_end_alloc, "ret_seek_end_val");
    builder->CreateRet(ret_seek_end_val);

    // Get file size
    builder->SetInsertPoint(seek_end_ok_block);
    llvm::Value *file_size = builder->CreateCall(ftell_fn, {file}, "file_size");

    // Check if ftell failed (file_size == -1)
    llvm::Value *minus_one = builder->getInt64(-1);
    llvm::Value *ftell_check = builder->CreateICmpEQ(file_size, minus_one, "ftell_check");
    builder->CreateCondBr(ftell_check, ftell_error_block, ftell_ok_block);

    // Handle ftell error, throw 122
    builder->SetInsertPoint(ftell_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *ret_ftell_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_ftell_alloc");
    llvm::Value *ret_ftell_err_ptr = builder->CreateStructGEP(function_result_type, ret_ftell_alloc, 0, "ret_ftell_err_ptr");
    builder->CreateStore(builder->getInt32(122), ret_ftell_err_ptr);
    llvm::Value *ret_ftell_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_ftell_empty_str");
    llvm::Value *ret_ftell_val_ptr = builder->CreateStructGEP(function_result_type, ret_ftell_alloc, 1, "ret_ftell_val_ptr");
    builder->CreateStore(ret_ftell_empty_str, ret_ftell_val_ptr);
    llvm::Value *ret_ftell_val = builder->CreateLoad(function_result_type, ret_ftell_alloc, "ret_ftell_val");
    builder->CreateRet(ret_ftell_val);

    // Return to beginning of file
    builder->SetInsertPoint(ftell_ok_block);
    llvm::Value *seek_set = builder->getInt32(0); // SEEK_SET is 0
    llvm::Value *seek_set_result = builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_set}, "seek_set_result");

    // Check if fseek SEEK_SET failed
    llvm::Value *seek_set_check = builder->CreateICmpNE(seek_set_result, builder->getInt32(0), "seek_set_check");
    builder->CreateCondBr(seek_set_check, seek_set_error_block, seek_set_ok_block);

    // Handle fseek SEEK_SET error, throw 123
    builder->SetInsertPoint(seek_set_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *ret_seek_set_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_seek_set_alloc");
    llvm::Value *ret_seek_set_err_ptr = builder->CreateStructGEP(function_result_type, ret_seek_set_alloc, 0, "ret_seek_set_err_ptr");
    builder->CreateStore(builder->getInt32(123), ret_seek_set_err_ptr);
    llvm::Value *ret_seek_set_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_seek_set_empty_str");
    llvm::Value *ret_seek_set_val_ptr = builder->CreateStructGEP(function_result_type, ret_seek_set_alloc, 1, "ret_seek_set_val_ptr");
    builder->CreateStore(ret_seek_set_empty_str, ret_seek_set_val_ptr);
    llvm::Value *ret_seek_set_val = builder->CreateLoad(function_result_type, ret_seek_set_alloc, "ret_seek_set_val");
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

    // Handle read error, throw 124
    builder->SetInsertPoint(read_error_block);
    builder->CreateCall(free_fn, {content});
    llvm::AllocaInst *ret_read_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_read_alloc");
    llvm::Value *ret_read_err_ptr = builder->CreateStructGEP(function_result_type, ret_read_alloc, 0, "ret_read_err_ptr");
    builder->CreateStore(builder->getInt32(124), ret_read_err_ptr);
    llvm::Value *ret_read_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_read_empty_str");
    llvm::Value *ret_read_val_ptr = builder->CreateStructGEP(function_result_type, ret_read_alloc, 1, "ret_read_val_ptr");
    builder->CreateStore(ret_read_empty_str, ret_read_val_ptr);
    llvm::Value *ret_read_val = builder->CreateLoad(function_result_type, ret_read_alloc, "ret_read_val");
    builder->CreateRet(ret_read_val);

    // Success - return content
    builder->SetInsertPoint(read_ok_block);
    llvm::AllocaInst *ret_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_alloc");
    llvm::Value *ret_err_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 0, "ret_err_ptr");
    builder->CreateStore(builder->getInt32(0), ret_err_ptr);
    llvm::Value *ret_val_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 1, "ret_val_ptr");
    builder->CreateStore(content, ret_val_ptr);
    llvm::Value *ret_val = builder->CreateLoad(function_result_type, ret_alloc, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::FS::generate_read_file_lines_function( //
    [[maybe_unused]] llvm::IRBuilder<> *builder,               //
    [[maybe_unused]] llvm::Module *module,                     //
    [[maybe_unused]] const bool only_declarations              //
) {
    // THE C IMPLEMENTATION:
    // str *read_file_lines(const str *path) {
    //     // Convert str path to null-terminated C string
    //     char *c_path = (char *)malloc(path->len + 1);
    //     if (!c_path) {
    //         return NULL; // Memory allocation failure
    //     }
    //     memcpy(c_path, path->value, path->len);
    //     c_path[path->len] = '\0';
    //     // Open the file for reading
    //     FILE *file = fopen(c_path, "r");
    //     free(c_path); // We don't need the path string anymore
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
}
