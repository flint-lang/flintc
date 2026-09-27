#include "generator/generator.hpp"
#include "llvm/IR/Constants.h"

static const Hash hash(std::string("filesystem"));
static const std::string prefix = hash.to_string() + ".filesystem.";

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
    //     char *c_path = (char *)(path->value);
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
    //     normalize_crlf(content);
    //     return content;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::Function *const fopen_fn = c_functions.at(FOPEN);
    llvm::Function *const free_fn = c_functions.at(FREE);
    llvm::Function *const fseek_fn = c_functions.at(FSEEK);
    llvm::Function *const fclose_fn = c_functions.at(FCLOSE);
    llvm::Function *const ftell_fn = c_functions.at(FTELL);
    llvm::Function *const fread_fn = c_functions.at(FREAD);
    llvm::Function *const create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *const normalize_crlf_fn = String::string_manip_functions.at("normalize_crlf");

    const unsigned int ErrIO = hash.get_type_id_from_str("ErrIO");
    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotFound = 1;
    const unsigned int NotReadable = 2;
    const unsigned int UnexpectedEOF = 4;
    const std::string NotFoundMessage(ErrIOValues.at(NotFound).second);
    const std::string NotReadableMessage(ErrIOValues.at(NotReadable).second);
    const std::string UnexpectedEOFMessage(ErrIOValues.at(UnexpectedEOF).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *const read_file_type = llvm::FunctionType::get( //
        function_result_type,                                           // Return type: str*
        {PTR_TY},                                                       // Parameter: const str* path
        false                                                           // Not variadic
    );
    llvm::Function *const read_file_fn = llvm::Function::Create(                      //
        read_file_type, llvm::Function::ExternalLinkage, prefix + "read_file", module //
    );
    fs_functions["read_file"] = read_file_fn;
    if (only_declarations) {
        return;
    }

    // Get the path parameter
    llvm::Argument *const path_arg = read_file_fn->arg_begin();
    path_arg->setName("path");

    // Create all basic blocks first
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", read_file_fn);
    llvm::BasicBlock *const file_null_block = llvm::BasicBlock::Create(context, "file_null", read_file_fn);
    llvm::BasicBlock *const file_valid_block = llvm::BasicBlock::Create(context, "file_valid", read_file_fn);
    llvm::BasicBlock *const seek_end_ok_block = llvm::BasicBlock::Create(context, "seek_end_ok", read_file_fn);
    llvm::BasicBlock *const seek_end_error_block = llvm::BasicBlock::Create(context, "seek_end_error", read_file_fn);
    llvm::BasicBlock *const ftell_ok_block = llvm::BasicBlock::Create(context, "ftell_ok", read_file_fn);
    llvm::BasicBlock *const ftell_error_block = llvm::BasicBlock::Create(context, "ftell_error", read_file_fn);
    llvm::BasicBlock *const seek_set_ok_block = llvm::BasicBlock::Create(context, "seek_set_ok", read_file_fn);
    llvm::BasicBlock *const seek_set_error_block = llvm::BasicBlock::Create(context, "seek_set_error", read_file_fn);
    llvm::BasicBlock *const read_error_block = llvm::BasicBlock::Create(context, "read_error", read_file_fn);
    llvm::BasicBlock *const read_ok_block = llvm::BasicBlock::Create(context, "read_ok", read_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get the c string from the value directly
    llvm::Value *const c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "rb" string constant
    llvm::Value *const mode_str = IR::generate_const_string(module, "rb");

    // Open file: file = fopen(c_path, "rb")
    llvm::Value *const file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *const file_null_check = builder->CreateIsNull(file, "file_is_null");
    builder->CreateCondBr(file_null_check, file_null_block, file_valid_block);

    // Handle NULL file, throw ErrIO.NotFound
    builder->SetInsertPoint(file_null_block);
    llvm::AllocaInst *const ret_file_null_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_null_alloc");
    llvm::Value *const ret_file_null_err_ptr = builder->CreateStructGEP(      //
        function_result_type, ret_file_null_alloc, 0, "ret_file_null_err_ptr" //
    );
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrIO, NotFound, NotFoundMessage);
    IR::aligned_store(*builder, err_value, ret_file_null_err_ptr);
    llvm::Value *const ret_file_null_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_file_null_empty_str");
    llvm::Value *const ret_file_null_val_ptr = builder->CreateStructGEP(      //
        function_result_type, ret_file_null_alloc, 1, "ret_file_null_val_ptr" //
    );
    IR::aligned_store(*builder, ret_file_null_empty_str, ret_file_null_val_ptr);
    llvm::Value *const ret_file_null_val = IR::aligned_load(*builder, function_result_type, ret_file_null_alloc, "ret_file_null_val");
    builder->CreateRet(ret_file_null_val);

    // Continue with valid file
    builder->SetInsertPoint(file_valid_block);

    // fseek(file, 0, SEEK_END)
    llvm::Value *const seek_end = builder->getInt32(2); // SEEK_END is 2
    llvm::Value *const seek_end_result = builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_end}, "seek_end_result");

    // Check if fseek failed
    llvm::Value *const seek_end_check = builder->CreateICmpNE(seek_end_result, builder->getInt32(0), "seek_end_check");
    builder->CreateCondBr(seek_end_check, seek_end_error_block, seek_end_ok_block);

    // Handle fseek SEEK_END error, throw ErrIO.NotReadable
    builder->SetInsertPoint(seek_end_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *const ret_seek_end_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_seek_end_alloc");
    llvm::Value *const ret_seek_end_err_ptr = builder->CreateStructGEP(function_result_type, ret_seek_end_alloc, 0, "ret_seek_end_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, NotReadable, NotReadableMessage);
    IR::aligned_store(*builder, err_value, ret_seek_end_err_ptr);
    llvm::Value *const ret_seek_end_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_seek_end_empty_str");
    llvm::Value *const ret_seek_end_val_ptr = builder->CreateStructGEP(function_result_type, ret_seek_end_alloc, 1, "ret_seek_end_val_ptr");
    IR::aligned_store(*builder, ret_seek_end_empty_str, ret_seek_end_val_ptr);
    llvm::Value *const ret_seek_end_val = IR::aligned_load(*builder, function_result_type, ret_seek_end_alloc, "ret_seek_end_val");
    builder->CreateRet(ret_seek_end_val);

    // Get file size
    builder->SetInsertPoint(seek_end_ok_block);
    llvm::Value *const file_size = builder->CreateCall(ftell_fn, {file}, "file_size");

    // Check if ftell failed (file_size == -1)
    llvm::Value *const minus_one = builder->getInt64(-1);
    llvm::Value *const ftell_check = builder->CreateICmpEQ(file_size, minus_one, "ftell_check");
    builder->CreateCondBr(ftell_check, ftell_error_block, ftell_ok_block);

    // Handle ftell error, throw ErrIO.NotReadable
    builder->SetInsertPoint(ftell_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *const ret_ftell_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_ftell_alloc");
    llvm::Value *const ret_ftell_err_ptr = builder->CreateStructGEP(function_result_type, ret_ftell_alloc, 0, "ret_ftell_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, NotReadable, NotReadableMessage);
    IR::aligned_store(*builder, err_value, ret_ftell_err_ptr);
    llvm::Value *const ret_ftell_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_ftell_empty_str");
    llvm::Value *const ret_ftell_val_ptr = builder->CreateStructGEP(function_result_type, ret_ftell_alloc, 1, "ret_ftell_val_ptr");
    IR::aligned_store(*builder, ret_ftell_empty_str, ret_ftell_val_ptr);
    llvm::Value *const ret_ftell_val = IR::aligned_load(*builder, function_result_type, ret_ftell_alloc, "ret_ftell_val");
    builder->CreateRet(ret_ftell_val);

    // Return to beginning of file
    builder->SetInsertPoint(ftell_ok_block);
    llvm::Value *const seek_set = builder->getInt32(0); // SEEK_SET is 0
    llvm::Value *const seek_set_result = builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_set}, "seek_set_result");

    // Check if fseek SEEK_SET failed
    llvm::Value *const seek_set_check = builder->CreateICmpNE(seek_set_result, builder->getInt32(0), "seek_set_check");
    builder->CreateCondBr(seek_set_check, seek_set_error_block, seek_set_ok_block);

    // Handle fseek SEEK_SET error, throw ErrIO.NotReadable
    builder->SetInsertPoint(seek_set_error_block);
    builder->CreateCall(fclose_fn, {file});
    llvm::AllocaInst *const ret_seek_set_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_seek_set_alloc");
    llvm::Value *const ret_seek_set_err_ptr = builder->CreateStructGEP(function_result_type, ret_seek_set_alloc, 0, "ret_seek_set_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, NotReadable, NotReadableMessage);
    IR::aligned_store(*builder, err_value, ret_seek_set_err_ptr);
    llvm::Value *const ret_seek_set_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_seek_set_empty_str");
    llvm::Value *const ret_seek_set_val_ptr = builder->CreateStructGEP(function_result_type, ret_seek_set_alloc, 1, "ret_seek_set_val_ptr");
    IR::aligned_store(*builder, ret_seek_set_empty_str, ret_seek_set_val_ptr);
    llvm::Value *const ret_seek_set_val = IR::aligned_load(*builder, function_result_type, ret_seek_set_alloc, "ret_seek_set_val");
    builder->CreateRet(ret_seek_set_val);

    // Allocate memory for file content
    builder->SetInsertPoint(seek_set_ok_block);

    // Create string to hold file content
    llvm::Value *const content = builder->CreateCall(create_str_fn, {file_size}, "content");

    // Get content->value pointer
    llvm::Value *const content_value_ptr = builder->CreateStructGEP(str_type, content, 1, "content_value_ptr");

    // Read file: fread(content->value, 1, file_size, file)
    llvm::Value *const bytes_read = builder->CreateCall(fread_fn, {content_value_ptr, builder->getInt64(1), file_size, file}, "bytes_read");

    // Close file
    builder->CreateCall(fclose_fn, {file});

    // Check if read was successful (bytes_read == file_size)
    llvm::Value *const read_check = builder->CreateICmpNE(bytes_read, file_size, "read_check");
    builder->CreateCondBr(read_check, read_error_block, read_ok_block);

    // Handle read error, throw ErrIO.UnexpectedEOF
    builder->SetInsertPoint(read_error_block);
    builder->CreateCall(free_fn, {content});
    llvm::AllocaInst *const ret_read_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_read_alloc");
    llvm::Value *const ret_read_err_ptr = builder->CreateStructGEP(function_result_type, ret_read_alloc, 0, "ret_read_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrIO, UnexpectedEOF, UnexpectedEOFMessage);
    IR::aligned_store(*builder, err_value, ret_read_err_ptr);
    llvm::Value *const ret_read_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_read_empty_str");
    llvm::Value *const ret_read_val_ptr = builder->CreateStructGEP(function_result_type, ret_read_alloc, 1, "ret_read_val_ptr");
    IR::aligned_store(*builder, ret_read_empty_str, ret_read_val_ptr);
    llvm::Value *const ret_read_val = IR::aligned_load(*builder, function_result_type, ret_read_alloc, "ret_read_val");
    builder->CreateRet(ret_read_val);

    builder->SetInsertPoint(read_ok_block);
    builder->CreateCall(normalize_crlf_fn, {content});
    llvm::Value *ret_val = llvm::ConstantAggregateZero::get(function_result_type);
    ret_val = builder->CreateInsertValue(ret_val, builder->getInt32(0), 0);
    ret_val = builder->CreateInsertValue(ret_val, content, 1, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::FileSystem::generate_read_lines_function( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations                                  //
) {
    // THE C IMPLEMENTATION:
    // str *read_lines(const str *path) {                                                               B0 [entry]
    //     char *c_path = (char *)path->value;
    //     FILE *file = fopen(c_path, "r");
    //     if (file == NULL) {                                                                          B1
    //         return NULL;
    //     }                                                                                            B2
    //
    //     // Count the number of lines
    //     size_t line_count = 0;
    //     int ch;
    //     bool_t in_line = FALSE;
    //     while ((ch = fgetc(file)) != EOF) {                                                          B3 [cond], B4 [body]
    //         if (ch == '\n') {                                                                        B5
    //             line_count++;
    //             in_line = FALSE;
    //         } else if (!in_line) {                                                                   B6 [cond], B7
    //             in_line = TRUE;
    //         }
    //     }                                                                                            B8 [merge]
    //     // Does not end with a new line
    //     if (in_line) {                                                                               B9
    //         line_count++;
    //     }                                                                                            B10
    //     rewind(file);
    //     str *lines_array = create_arr(1, sizeof(str *), &line_count);
    //     if (lines_array == NULL) {                                                                   B11
    //         fclose(file);
    //         return NULL;
    //     }                                                                                            B12
    //
    //     // Initialize array with NULL pointers
    //     str *null_ptr = NULL;
    //     fill_arr(                                       //
    //         (char*)((size_t *)lines_array->value) + 1), // char* data
    //         1,                                          // size_t dim
    //         (size_t *)lines_array->value,               // size_t* dim_lengths
    //         sizeof(void *),                             // size_t value_size
    //         &null_ptr,                                  // void* value
    //         0                                           // int32_t type_id
    //     );
    //
    //     // Read lines and populate the array
    //     size_t line_idx = 0;
    //     char buffer[4096];
    //     while (fgets(buffer, sizeof(buffer), file)) {                                                B13 [cond], B14 [body]
    //         size_t len = strlen(buffer);
    //         // Remove trailing newline and carry linefeed if present
    //         if (len > 0 && buffer[len - 1] == '\n') {                                                B15, B16
    //             buffer[--len] = '\0';
    //             if (len > 0 && buffer[len - 1] == '\r') {                                            B17, B18
    //                 buffer[--len] = '\0';
    //             }
    //         }                                                                                        B19
    //         size_t *dim_lengths = (size_t *)lines_array->value;
    //         char *data = (char *)(dim_lengths + 1);
    //         str *line = init_str(buffer, len);
    //         if (line == NULL) {                                                                      B20
    //             for (size_t i = 0; i < line_idx; i++) {                                              B21 [cond], B22 [body]
    //                 str *line_str = *(str **)access_arr(sizeof(str *), data, 1, dim_lengths, &i);
    //                 free(line_str);
    //             }                                                                                    B23 [merge]
    //             free(lines_array);
    //             fclose(file);
    //             return NULL;
    //         }                                                                                        B24
    //         str **elem_ptr = (str **)access_arr(sizeof(str *), data, 1, dim_lengths, &line_idx);
    //         *elem_ptr = line;
    //         line_idx++;
    //     }                                                                                            B25 [merge]
    //     // Check if we read fewer lines than expected and adjust array length
    //     if (line_idx < line_count) {                                                                 B26
    //         size_t *dim_lengths = (size_t *)lines_array->value;
    //         *dim_lengths = line_idx;
    //     }                                                                                            B27
    //     fclose(file);
    //     return lines_array;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::Function *const free_fn = c_functions.at(FREE);
    llvm::Function *const fopen_fn = c_functions.at(FOPEN);
    llvm::Function *const fclose_fn = c_functions.at(FCLOSE);
    llvm::Function *const fgetc_fn = c_functions.at(FGETC);
    llvm::Function *const fgets_fn = c_functions.at(FGETS);
    llvm::Function *const rewind_fn = c_functions.at(REWIND);
    llvm::Function *const strlen_fn = c_functions.at(STRLEN);
    llvm::Function *const create_str_fn = String::string_manip_functions.at("create_str");
    llvm::Function *const init_str_fn = String::string_manip_functions.at("init_str");
    llvm::Function *const create_arr_fn = Array::array_manip_functions.at("create_arr");
    llvm::Function *const fill_arr_fn = Array::array_manip_functions.at("fill_arr");
    llvm::Function *const access_arr_fn = Array::array_manip_functions.at("access_arr");

    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotFound = 1;
    const std::string NotFoundMessage(ErrIOValues.at(NotFound).second);

    const unsigned int ErrIOCount = 5;
    const unsigned int ErrFS = hash.get_type_id_from_str("ErrFS");
    const std::vector<error_value> &ErrFSValues = std::get<2>(core_module_error_sets.at("filesystem").at(1));
    const unsigned int TooLarge = 5;
    const std::string TooLargeMessage(ErrFSValues.at(TooLarge - ErrIOCount).second);

    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, Type::get_primitive_type("str"), true);
    llvm::FunctionType *const read_lines_type = llvm::FunctionType::get(function_result_type, {PTR_TY}, false);
    llvm::Function *const read_lines_fn = llvm::Function::Create(                       //
        read_lines_type, llvm::Function::ExternalLinkage, prefix + "read_lines", module //
    );
    fs_functions["read_lines"] = read_lines_fn;
    if (only_declarations) {
        return;
    }

    llvm::Argument *const arg_path = read_lines_fn->arg_begin();
    arg_path->setName("path");

    // Create basic blocks
    llvm::BasicBlock *const b0_entry = llvm::BasicBlock::Create(context, "B0_entry", read_lines_fn);
    llvm::BasicBlock *const b1 = llvm::BasicBlock::Create(context, "B1", read_lines_fn);
    llvm::BasicBlock *const b2 = llvm::BasicBlock::Create(context, "B2", read_lines_fn);
    llvm::BasicBlock *const b3_cond = llvm::BasicBlock::Create(context, "B3_cond", read_lines_fn);
    llvm::BasicBlock *const b4_body = llvm::BasicBlock::Create(context, "B4_body", read_lines_fn);
    llvm::BasicBlock *const b5 = llvm::BasicBlock::Create(context, "B5", read_lines_fn);
    llvm::BasicBlock *const b6_if = llvm::BasicBlock::Create(context, "B6_if", read_lines_fn);
    llvm::BasicBlock *const b7 = llvm::BasicBlock::Create(context, "B7", read_lines_fn);
    llvm::BasicBlock *const b8_merge = llvm::BasicBlock::Create(context, "B8_merge", read_lines_fn);
    llvm::BasicBlock *const b9 = llvm::BasicBlock::Create(context, "B9", read_lines_fn);
    llvm::BasicBlock *const b10 = llvm::BasicBlock::Create(context, "B10", read_lines_fn);
    llvm::BasicBlock *const b11 = llvm::BasicBlock::Create(context, "B11", read_lines_fn);
    llvm::BasicBlock *const b12 = llvm::BasicBlock::Create(context, "B12", read_lines_fn);
    llvm::BasicBlock *const b13_cond = llvm::BasicBlock::Create(context, "B13_cond", read_lines_fn);
    llvm::BasicBlock *const b14_body = llvm::BasicBlock::Create(context, "B14_body", read_lines_fn);
    llvm::BasicBlock *const b15 = llvm::BasicBlock::Create(context, "B15", read_lines_fn);
    llvm::BasicBlock *const b16 = llvm::BasicBlock::Create(context, "B16", read_lines_fn);
    llvm::BasicBlock *const b17 = llvm::BasicBlock::Create(context, "B17", read_lines_fn);
    llvm::BasicBlock *const b18 = llvm::BasicBlock::Create(context, "B18", read_lines_fn);
    llvm::BasicBlock *const b19 = llvm::BasicBlock::Create(context, "B19", read_lines_fn);
    llvm::BasicBlock *const b20 = llvm::BasicBlock::Create(context, "B20", read_lines_fn);
    llvm::BasicBlock *const b21_cond = llvm::BasicBlock::Create(context, "B21_cond", read_lines_fn);
    llvm::BasicBlock *const b22_body = llvm::BasicBlock::Create(context, "B22_body", read_lines_fn);
    llvm::BasicBlock *const b23_merge = llvm::BasicBlock::Create(context, "B23_merge", read_lines_fn);
    llvm::BasicBlock *const b24 = llvm::BasicBlock::Create(context, "B24", read_lines_fn);
    llvm::BasicBlock *const b25_merge = llvm::BasicBlock::Create(context, "B25_merge", read_lines_fn);
    llvm::BasicBlock *const b26 = llvm::BasicBlock::Create(context, "B26", read_lines_fn);
    llvm::BasicBlock *const b27 = llvm::BasicBlock::Create(context, "B27", read_lines_fn);

    builder->SetInsertPoint(b0_entry);
    llvm::Value *const ptr_size = builder->getInt64(Allocation::get_type_size(module, PTR_TY));
    llvm::Value *const null_ptr = llvm::ConstantPointerNull::get(PTR_TY);
    llvm::Value *const c_path = builder->CreateStructGEP(str_type, arg_path, 1, "b0_c_path");
    llvm::Value *const mode_str = IR::generate_const_string(module, "r");
    llvm::Value *const file_ptr = builder->CreateCall(fopen_fn, {c_path, mode_str}, "b0_file_ptr");
    llvm::Value *const file_ptr_eq_null = builder->CreateICmpEQ(file_ptr, null_ptr, "b0_file_ptr_eq_null");
    builder->CreateCondBr(file_ptr_eq_null, b1, b2);

    {
        builder->SetInsertPoint(b1);
        llvm::Value *ret_val = llvm::ConstantAggregateZero::get(function_result_type);
        llvm::Value *const err_value = IR::generate_err_value(*builder, module, ErrFS, NotFound, NotFoundMessage);
        ret_val = builder->CreateInsertValue(ret_val, err_value, 0);
        llvm::Value *const empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "b1_empty_str");
        ret_val = builder->CreateInsertValue(ret_val, empty_str, 1, "b1_ret_val");
        builder->CreateRet(ret_val);
    }

    builder->SetInsertPoint(b2);
    llvm::AllocaInst *const line_count_alloca = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "line_count_alloca");
    llvm::AllocaInst *const ch_alloca = builder->CreateAlloca(builder->getInt32Ty(), 0, nullptr, "ch_alloca");
    llvm::AllocaInst *const in_line_alloca = builder->CreateAlloca(builder->getInt1Ty(), 0, nullptr, "in_line_alloca");
    IR::aligned_store(*builder, builder->getInt64(0), line_count_alloca);
    IR::aligned_store(*builder, builder->getInt1(false), in_line_alloca);
    builder->CreateBr(b3_cond);

    {
        builder->SetInsertPoint(b3_cond);
        llvm::Value *const ch_val = builder->CreateCall(fgetc_fn, {file_ptr}, "b3_ch_val");
        IR::aligned_store(*builder, ch_val, ch_alloca);
        llvm::Value *const eof = builder->getInt32(-1);
        llvm::Value *const ch_eq_eof = builder->CreateICmpEQ(ch_val, eof, "ch_eq_eof");
        builder->CreateCondBr(ch_eq_eof, b8_merge, b4_body);

        builder->SetInsertPoint(b4_body);
        llvm::Value *const ch_eq_eol = builder->CreateICmpEQ(ch_val, builder->getInt32('\n'), "ch_eq_eol");
        builder->CreateCondBr(ch_eq_eol, b5, b6_if);

        builder->SetInsertPoint(b5);
        llvm::Value *const line_count = IR::aligned_load(*builder, builder->getInt64Ty(), line_count_alloca, "b5_line_count");
        llvm::Value *const line_count_p1 = builder->CreateAdd(line_count, builder->getInt64(1), "b5_line_count_p1");
        IR::aligned_store(*builder, line_count_p1, line_count_alloca);
        IR::aligned_store(*builder, builder->getInt1(false), in_line_alloca);
        builder->CreateBr(b3_cond);

        builder->SetInsertPoint(b6_if);
        llvm::Value *const in_line = IR::aligned_load(*builder, builder->getInt1Ty(), in_line_alloca, "b6_in_line");
        builder->CreateCondBr(in_line, b3_cond, b7);

        builder->SetInsertPoint(b7);
        IR::aligned_store(*builder, builder->getInt1(true), in_line_alloca);
        builder->CreateBr(b3_cond);
    }

    builder->SetInsertPoint(b8_merge);
    llvm::Value *const in_line = IR::aligned_load(*builder, builder->getInt1Ty(), in_line_alloca, "b8_in_line");
    builder->CreateCondBr(in_line, b9, b10);

    {
        builder->SetInsertPoint(b9);
        llvm::Value *const line_count = IR::aligned_load(*builder, builder->getInt64Ty(), line_count_alloca, "b9_line_count");
        llvm::Value *const line_count_p1 = builder->CreateAdd(line_count, builder->getInt64(1), "b9_line_count_p1");
        IR::aligned_store(*builder, line_count_p1, line_count_alloca);
        builder->CreateBr(b10);
    }

    builder->SetInsertPoint(b10);
    builder->CreateCall(rewind_fn, {file_ptr});
    llvm::Value *const lines_array = builder->CreateCall(                                     //
        create_arr_fn, {builder->getInt64(1), ptr_size, line_count_alloca}, "b10_lines_array" //
    );
    llvm::Value *const lines_array_null = builder->CreateICmpEQ(lines_array, null_ptr, "b10_lines_array_null");
    builder->CreateCondBr(lines_array_null, b11, b12);

    {
        builder->SetInsertPoint(b11);
        builder->CreateCall(fclose_fn, {file_ptr});
        llvm::Value *ret_val = llvm::ConstantAggregateZero::get(function_result_type);
        llvm::Value *const err_value = IR::generate_err_value(*builder, module, ErrFS, TooLarge, TooLargeMessage);
        ret_val = builder->CreateInsertValue(ret_val, err_value, 0);
        llvm::Value *const empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "b11_empty_str");
        ret_val = builder->CreateInsertValue(ret_val, empty_str, 1, "b11_ret_val");
        builder->CreateRet(ret_val);
    }

    builder->SetInsertPoint(b12);
    llvm::AllocaInst *const null_ptr_alloca = builder->CreateAlloca(PTR_TY, 0, nullptr, "null_ptr_alloca");
    IR::aligned_store(*builder, null_ptr, null_ptr_alloca);
    llvm::Value *const arr_dim_lengths = builder->CreateStructGEP(str_type, lines_array, 1, "b12_arr_dim_lengths");
    llvm::Value *const arr_data = builder->CreateGEP(builder->getInt64Ty(), arr_dim_lengths, builder->getInt64(1), "b12_arr_data");
    builder->CreateCall(fill_arr_fn,
        {
            arr_data,             // char* data
            builder->getInt64(1), // size_t dim
            arr_dim_lengths,      // size_t* dim_lengths
            ptr_size,             // size_t value_size
            null_ptr_alloca,      // void* value
            builder->getInt32(0)  // i32 type_id
        });
    llvm::AllocaInst *const line_idx_alloca = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "line_idx_alloca");
    IR::aligned_store(*builder, builder->getInt64(0), line_idx_alloca);
    llvm::AllocaInst *const buffer_alloca = builder->CreateAlloca(builder->getInt8Ty(), builder->getInt32(4096), "buffer_alloca");
    builder->CreateBr(b13_cond);

    {
        builder->SetInsertPoint(b13_cond);
        llvm::Value *const fgets_ret = builder->CreateCall(                               //
            fgets_fn, {buffer_alloca, builder->getInt32(4096), file_ptr}, "b13_fgets_ret" //
        );
        llvm::Value *const fgets_ret_eq_null = builder->CreateICmpEQ(fgets_ret, null_ptr, "fgets_ret_eq_null");
        builder->CreateCondBr(fgets_ret_eq_null, b25_merge, b14_body);

        builder->SetInsertPoint(b14_body);
        llvm::Value *const len = builder->CreateCall(strlen_fn, {buffer_alloca}, "b14_len");
        llvm::Value *const len_gt_0 = builder->CreateICmpUGT(len, builder->getInt64(0), "b14_len_gt_0");
        builder->CreateCondBr(len_gt_0, b15, b19);

        builder->SetInsertPoint(b15);
        llvm::Value *const len_m1 = builder->CreateSub(len, builder->getInt64(1), "b15_len_m1");
        llvm::Value *const buffer_at_len_m1_ptr = builder->CreateGEP(               //
            builder->getInt8Ty(), buffer_alloca, len_m1, "b15_buffer_at_len_m1_ptr" //
        );
        llvm::Value *const buffer_at_len_m1 = IR::aligned_load(                          //
            *builder, builder->getInt8Ty(), buffer_at_len_m1_ptr, "b15_buffer_at_len_m1" //
        );
        llvm::Value *const buffer_at_len_m1_eq_lf = builder->CreateICmpEQ(         //
            buffer_at_len_m1, builder->getInt8('\n'), "b15_buffer_at_len_m1_eq_lf" //
        );
        builder->CreateCondBr(buffer_at_len_m1_eq_lf, b16, b19);

        builder->SetInsertPoint(b16);
        IR::aligned_store(*builder, builder->getInt8(0), buffer_at_len_m1_ptr);
        llvm::Value *const len_m1_gt_0 = builder->CreateICmpUGT(len_m1, builder->getInt64(0), "b16_len_m1_gt_0");
        builder->CreateCondBr(len_m1_gt_0, b17, b19);

        builder->SetInsertPoint(b17);
        llvm::Value *const len_m2 = builder->CreateSub(len_m1, builder->getInt64(1), "b17_len_m2");
        llvm::Value *const buffer_at_len_m2_ptr = builder->CreateGEP(               //
            builder->getInt8Ty(), buffer_alloca, len_m2, "b17_buffer_at_len_m2_ptr" //
        );
        llvm::Value *const buffer_at_len_m2 = IR::aligned_load(                          //
            *builder, builder->getInt8Ty(), buffer_at_len_m2_ptr, "b17_buffer_at_len_m2" //
        );
        llvm::Value *const buffer_at_len_m2_eq_cr = builder->CreateICmpEQ(         //
            buffer_at_len_m2, builder->getInt8('\r'), "b17_buffer_at_len_m2_eq_cr" //
        );
        builder->CreateCondBr(buffer_at_len_m2_eq_cr, b18, b19);

        builder->SetInsertPoint(b18);
        IR::aligned_store(*builder, builder->getInt8(0), buffer_at_len_m2_ptr);
        builder->CreateBr(b19);

        builder->SetInsertPoint(b19);
        llvm::PHINode *const real_len = builder->CreatePHI(builder->getInt64Ty(), 5, "b19_real_len");
        real_len->addIncoming(len, b14_body);
        real_len->addIncoming(len, b15);
        real_len->addIncoming(len_m1, b16);
        real_len->addIncoming(len_m1, b17);
        real_len->addIncoming(len_m2, b18);
        llvm::Value *const line_idx_value = IR::aligned_load(*builder, builder->getInt64Ty(), line_idx_alloca, "b19_line_idx_value");
        llvm::Value *const dim_lengths = builder->CreateStructGEP(str_type, lines_array, 1, "b19_dim_lengths");
        llvm::Value *const data = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, builder->getInt32(1), "b19_data");
        llvm::Value *const line = builder->CreateCall(init_str_fn, {buffer_alloca, real_len}, "b19_line");
        llvm::Value *const line_eq_null = builder->CreateICmpEQ(line, null_ptr, "b19_line_eq_null");
        builder->CreateCondBr(line_eq_null, b20, b24);

        {
            builder->SetInsertPoint(b20);
            llvm::AllocaInst *const i_alloca = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "i_alloca");
            IR::aligned_store(*builder, builder->getInt64(0), i_alloca);
            builder->CreateBr(b21_cond);

            builder->SetInsertPoint(b21_cond);
            llvm::Value *const i_value = IR::aligned_load(*builder, builder->getInt64Ty(), i_alloca, "b21_i_value");
            llvm::Value *const i_lt_line_idx = builder->CreateICmpULT(i_value, line_idx_value, "b21_i_lt_line_idx");
            builder->CreateCondBr(i_lt_line_idx, b22_body, b23_merge);

            builder->SetInsertPoint(b22_body);
            llvm::Value *const line_str_ptr = builder->CreateCall(                                               //
                access_arr_fn, {ptr_size, data, builder->getInt64(1), dim_lengths, i_alloca}, "b22_line_str_ptr" //
            );
            llvm::Value *const line_str = IR::aligned_load(*builder, PTR_TY, line_str_ptr, "b22_line_str");
            builder->CreateCall(free_fn, {line_str});
            llvm::Value *const i_value_p1 = builder->CreateAdd(i_value, builder->getInt64(1), "b22_i_value_p1");
            IR::aligned_store(*builder, i_value_p1, i_alloca);
            builder->CreateBr(b21_cond);

            builder->SetInsertPoint(b23_merge);
            builder->CreateCall(free_fn, {lines_array});
            builder->CreateCall(fclose_fn, {file_ptr});
            llvm::Value *ret_val = llvm::ConstantAggregateZero::get(function_result_type);
            llvm::Value *const err_value = IR::generate_err_value(*builder, module, ErrFS, TooLarge, TooLargeMessage);
            ret_val = builder->CreateInsertValue(ret_val, err_value, 0);
            llvm::Value *const empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "b23_empty_str");
            ret_val = builder->CreateInsertValue(ret_val, empty_str, 1, "b23_ret_val");
            builder->CreateRet(ret_val);
        }

        builder->SetInsertPoint(b24);
        llvm::Value *const elem_ptr = builder->CreateCall(                                                      //
            access_arr_fn, {ptr_size, data, builder->getInt64(1), dim_lengths, line_idx_alloca}, "b24_elem_ptr" //
        );
        IR::aligned_store(*builder, line, elem_ptr);
        llvm::Value *const line_idx_p1 = builder->CreateAdd(line_idx_value, builder->getInt64(1), "b24_line_idx_p1");
        IR::aligned_store(*builder, line_idx_p1, line_idx_alloca);
        builder->CreateBr(b13_cond);
    }

    builder->SetInsertPoint(b25_merge);
    llvm::Value *const line_idx_value = IR::aligned_load(*builder, builder->getInt64Ty(), line_idx_alloca, "b25_line_idx_value");
    llvm::Value *const line_count_value = IR::aligned_load(*builder, builder->getInt64Ty(), line_count_alloca, "b25_line_count_value");
    llvm::Value *const line_idx_lt_line_count = builder->CreateICmpULT(line_idx_value, line_count_value, "b25_line_idx_lt_line_count");
    builder->CreateCondBr(line_idx_lt_line_count, b26, b27);

    builder->SetInsertPoint(b26);
    IR::aligned_store(*builder, line_idx_value, arr_dim_lengths);
    builder->CreateBr(b27);

    builder->SetInsertPoint(b27);
    builder->CreateCall(fclose_fn, {file_ptr});
    llvm::Value *ret_val = llvm::ConstantAggregateZero::get(function_result_type);
    llvm::StructType *const err_type = type_map.at("type.flint.err");
    llvm::Value *const err_struct = IR::get_default_value_of_type(err_type);
    ret_val = builder->CreateInsertValue(ret_val, err_struct, 0);
    ret_val = builder->CreateInsertValue(ret_val, lines_array, 1);
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
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::Function *const fopen_fn = c_functions.at(FOPEN);
    llvm::Function *const fclose_fn = c_functions.at(FCLOSE);

    llvm::FunctionType *const file_exists_type = llvm::FunctionType::get( //
        llvm::Type::getInt1Ty(context),                                   // return bool
        {PTR_TY},                                                         // str* path
        false                                                             // No vaarg
    );
    llvm::Function *const file_exists_fn = llvm::Function::Create(                        //
        file_exists_type, llvm::Function::ExternalLinkage, prefix + "file_exists", module //
    );
    fs_functions["file_exists"] = file_exists_fn;
    if (only_declarations) {
        return;
    }

    // Get the path parameter
    llvm::Argument *const path_arg = file_exists_fn->arg_begin();
    path_arg->setName("path");

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", file_exists_fn);
    llvm::BasicBlock *const file_ok_block = llvm::BasicBlock::Create(context, "file_ok", file_exists_fn);
    llvm::BasicBlock *const file_fail_block = llvm::BasicBlock::Create(context, "file_fail", file_exists_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *const c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "r" string constant for fopen mode
    llvm::Value *const mode_str = IR::generate_const_string(module, "r");

    // Open file: file = fopen(c_path, "r")
    llvm::Value *const file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *const file_null = builder->CreateIsNull(file, "file_null");
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
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::Function *const fopen_fn = c_functions.at(FOPEN);
    llvm::Function *const fwrite_fn = c_functions.at(FWRITE);
    llvm::Function *const fclose_fn = c_functions.at(FCLOSE);
    llvm::Function *const create_str_fn = String::string_manip_functions.at("create_str");

    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotWritable = 3;
    const std::string NotWritableMessage(ErrIOValues.at(NotWritable).second);

    const unsigned int ErrIOCount = 5;
    const unsigned int ErrFS = hash.get_type_id_from_str("ErrFS");
    const std::vector<error_value> &ErrFSValues = std::get<2>(core_module_error_sets.at("filesystem").at(1));
    const unsigned int InvalidPath = 6;
    const std::string InvalidPathMessage(ErrFSValues.at(InvalidPath - ErrIOCount).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("str");
    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *const write_file_type = llvm::FunctionType::get( //
        function_result_type,                                            // Return type: struct with error code
        {PTR_TY, PTR_TY},                                                // Parameters: const str *path, const str *content
        false                                                            // Not variadic
    );
    llvm::Function *const write_file_fn = llvm::Function::Create(                       //
        write_file_type, llvm::Function::ExternalLinkage, prefix + "write_file", module //
    );
    fs_functions["write_file"] = write_file_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *const path_arg = write_file_fn->arg_begin();
    llvm::Argument *const content_arg = write_file_fn->arg_begin() + 1;
    path_arg->setName("path");
    content_arg->setName("content");

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", write_file_fn);
    llvm::BasicBlock *const file_fail_block = llvm::BasicBlock::Create(context, "file_fail", write_file_fn);
    llvm::BasicBlock *const file_ok_block = llvm::BasicBlock::Create(context, "file_ok", write_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *const c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "wb" string constant for fopen mode
    llvm::Value *const mode_str = IR::generate_const_string(module, "wb");

    // Open file: file = fopen(c_path, "wb")
    llvm::Value *const file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *const file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open failure, throw ErrFS.InvalidPath
    builder->SetInsertPoint(file_fail_block);
    llvm::AllocaInst *const ret_file_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_fail_alloc");
    llvm::Value *const ret_file_fail_err_ptr = builder->CreateStructGEP(      //
        function_result_type, ret_file_fail_alloc, 0, "ret_file_fail_err_ptr" //
    );
    llvm::Value *error_value = IR::generate_err_value(*builder, module, ErrFS, InvalidPath, InvalidPathMessage);
    IR::aligned_store(*builder, error_value, ret_file_fail_err_ptr);
    llvm::Value *const ret_file_fail_val = IR::aligned_load(*builder, function_result_type, ret_file_fail_alloc, "ret_file_fail_val");
    builder->CreateRet(ret_file_fail_val);

    // Write content to file
    builder->SetInsertPoint(file_ok_block);

    // Get content->len
    llvm::Value *const content_len_ptr = builder->CreateStructGEP(str_type, content_arg, 0, "content_len_ptr");
    llvm::Value *const content_len = IR::aligned_load(*builder, builder->getInt64Ty(), content_len_ptr, "content_len");

    // Get content->value
    llvm::Value *const content_value_ptr = builder->CreateStructGEP(str_type, content_arg, 1, "content_value_ptr");

    // Write to file: fwrite(content->value, 1, content->len, file)
    llvm::Value *const bytes_written = builder->CreateCall(                                      //
        fwrite_fn, {content_value_ptr, builder->getInt64(1), content_len, file}, "bytes_written" //
    );

    // Close the file
    builder->CreateCall(fclose_fn, {file});

    // Check if write was successful (bytes_written == content_len)
    llvm::Value *const write_check = builder->CreateICmpEQ(bytes_written, content_len, "write_check");

    // For write failure, return error 132
    llvm::AllocaInst *const ret_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_alloc");
    llvm::Value *const ret_err_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 0, "ret_err_ptr");
    error_value = IR::generate_err_value(*builder, module, ErrFS, NotWritable, NotWritableMessage);
    llvm::StructType *const err_type = type_map.at("type.flint.err");
    llvm::Value *const no_error_value = IR::get_default_value_of_type(err_type);
    llvm::Value *const ret_err_val = builder->CreateSelect(write_check, no_error_value, error_value);
    IR::aligned_store(*builder, ret_err_val, ret_err_ptr);

    // Return empty string in the data portion regardless of success/failure
    llvm::Value *const ret_empty_str = builder->CreateCall(create_str_fn, {builder->getInt64(0)}, "ret_empty_str");
    llvm::Value *const ret_val_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 1, "ret_val_ptr");
    IR::aligned_store(*builder, ret_empty_str, ret_val_ptr);
    llvm::Value *const ret_val = IR::aligned_load(*builder, function_result_type, ret_alloc, "ret_val");
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
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::Function *const fopen_fn = c_functions.at(FOPEN);
    llvm::Function *const fwrite_fn = c_functions.at(FWRITE);
    llvm::Function *const fclose_fn = c_functions.at(FCLOSE);

    const std::vector<error_value> &ErrIOValues = std::get<2>(core_module_error_sets.at("filesystem").at(0));
    const unsigned int NotWritable = 3;
    const std::string NotWritableMessage(ErrIOValues.at(NotWritable).second);

    const unsigned int ErrIOCount = 5;
    const unsigned int ErrFS = hash.get_type_id_from_str("ErrFS");
    const std::vector<error_value> &ErrFSValues = std::get<2>(core_module_error_sets.at("filesystem").at(1));
    const unsigned int InvalidPath = 6;
    const std::string InvalidPathMessage(ErrFSValues.at(InvalidPath - ErrIOCount).second);

    const std::shared_ptr<Type> &result_type_ptr = Type::get_primitive_type("void");
    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    llvm::FunctionType *const append_file_type = llvm::FunctionType::get( //
        function_result_type,                                             // return struct with error code
        {PTR_TY, PTR_TY},                                                 // Parameters: const str *path, const str *content
        false                                                             // No vaarg
    );
    llvm::Function *const append_file_fn = llvm::Function::Create(                        //
        append_file_type, llvm::Function::ExternalLinkage, prefix + "append_file", module //
    );
    fs_functions["append_file"] = append_file_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameters
    llvm::Argument *const path_arg = append_file_fn->arg_begin();
    llvm::Argument *const content_arg = append_file_fn->arg_begin() + 1;
    path_arg->setName("path");
    content_arg->setName("content");

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", append_file_fn);
    llvm::BasicBlock *const file_fail_block = llvm::BasicBlock::Create(context, "file_fail", append_file_fn);
    llvm::BasicBlock *const file_ok_block = llvm::BasicBlock::Create(context, "file_ok", append_file_fn);
    llvm::BasicBlock *const write_fail_block = llvm::BasicBlock::Create(context, "write_fail", append_file_fn);
    llvm::BasicBlock *const write_ok_block = llvm::BasicBlock::Create(context, "write_ok", append_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *const c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "ab" string constant for fopen mode (append binary)
    llvm::Value *const mode_str = IR::generate_const_string(module, "ab");

    // Open file: file = fopen(c_path, "ab")
    llvm::Value *const file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *const file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open failure, throw ErrFS.InvalidPath
    builder->SetInsertPoint(file_fail_block);
    llvm::AllocaInst *const ret_file_fail_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_file_fail_alloc");
    llvm::Value *const ret_file_fail_err_ptr = builder->CreateStructGEP(      //
        function_result_type, ret_file_fail_alloc, 0, "ret_file_fail_err_ptr" //
    );
    llvm::Value *error_value = IR::generate_err_value(*builder, module, ErrFS, InvalidPath, InvalidPathMessage);
    IR::aligned_store(*builder, error_value, ret_file_fail_err_ptr);
    llvm::Value *const ret_file_fail_val = IR::aligned_load(*builder, function_result_type, ret_file_fail_alloc, "ret_file_fail_val");
    builder->CreateRet(ret_file_fail_val);

    // Append content to file
    builder->SetInsertPoint(file_ok_block);

    // Get content->len
    llvm::Value *const content_len_ptr = builder->CreateStructGEP(str_type, content_arg, 0, "content_len_ptr");
    llvm::Value *const content_len = IR::aligned_load(*builder, builder->getInt64Ty(), content_len_ptr, "content_len");

    // Get content->value
    llvm::Value *const content_value_ptr = builder->CreateStructGEP(str_type, content_arg, 1, "content_value_ptr");

    // Write to file: fwrite(content->value, 1, content->len, file)
    llvm::Value *const bytes_written = builder->CreateCall(                                      //
        fwrite_fn, {content_value_ptr, builder->getInt64(1), content_len, file}, "bytes_written" //
    );

    // Close the file
    builder->CreateCall(fclose_fn, {file});

    // Check if write was successful (bytes_written == content_len)
    llvm::Value *const write_check = builder->CreateICmpEQ(bytes_written, content_len, "write_check");
    builder->CreateCondBr(write_check, write_ok_block, write_fail_block);

    // Throw ErrFS.NotWritable when the written bytes differ with the length
    builder->SetInsertPoint(write_fail_block);
    llvm::AllocaInst *const ret_alloc = builder->CreateAlloca(function_result_type, 0, nullptr, "ret_alloc");
    llvm::Value *const ret_err_ptr = builder->CreateStructGEP(function_result_type, ret_alloc, 0, "ret_err_ptr");
    error_value = IR::generate_err_value(*builder, module, ErrFS, NotWritable, NotWritableMessage);
    IR::aligned_store(*builder, error_value, ret_err_ptr);
    llvm::Value *const ret_val = IR::aligned_load(*builder, function_result_type, ret_alloc, "ret_val");
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
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::Function *const fopen_fn = c_functions.at(FOPEN);
    llvm::Function *const fread_fn = c_functions.at(FREAD);
    llvm::Function *const fseek_fn = c_functions.at(FSEEK);
    llvm::Function *const fclose_fn = c_functions.at(FCLOSE);

    llvm::FunctionType *const is_file_type = llvm::FunctionType::get( //
        llvm::Type::getInt1Ty(context),                               // return bool
        {PTR_TY},                                                     // str *path
        false                                                         // No vaarg
    );
    llvm::Function *const is_file_fn = llvm::Function::Create(                    //
        is_file_type, llvm::Function::ExternalLinkage, prefix + "is_file", module //
    );
    fs_functions["is_file"] = is_file_fn;
    if (only_declarations) {
        return;
    }

    // Get function parameter
    llvm::Argument *const path_arg = is_file_fn->arg_begin();
    path_arg->setName("path");

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", is_file_fn);
    llvm::BasicBlock *const file_fail_block = llvm::BasicBlock::Create(context, "file_fail", is_file_fn);
    llvm::BasicBlock *const file_ok_block = llvm::BasicBlock::Create(context, "file_ok", is_file_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Convert str path to C string
    llvm::Value *const c_path = builder->CreateStructGEP(str_type, path_arg, 1, "c_path");

    // Create "rb" string constant for fopen mode (read binary)
    llvm::Value *const mode_str = IR::generate_const_string(module, "rb");

    // Open file: file = fopen(c_path, "rb")
    llvm::Value *const file = builder->CreateCall(fopen_fn, {c_path, mode_str}, "file");

    // Check if file is NULL
    llvm::Value *const file_null = builder->CreateIsNull(file, "file_null");
    builder->CreateCondBr(file_null, file_fail_block, file_ok_block);

    // Handle file open failure - return false
    builder->SetInsertPoint(file_fail_block);
    builder->CreateRet(builder->getFalse());

    // File opened successfully, check if it's a real file
    builder->SetInsertPoint(file_ok_block);

    // Create a buffer to read one byte
    llvm::AllocaInst *const buffer = builder->CreateAlloca(builder->getInt8Ty(), nullptr, "buffer");

    // Try to read 1 byte: fread(buffer, 1, 1, file)
    builder->CreateCall(fread_fn, {buffer, builder->getInt64(1), builder->getInt64(1), file}, "read_result");

    // Seek back to beginning: fseek(file, 0, SEEK_SET)
    llvm::Value *const seek_set = builder->getInt32(0);
    builder->CreateCall(fseek_fn, {file, builder->getInt64(0), seek_set});

    // Close the file: fclose(file)
    builder->CreateCall(fclose_fn, {file});

    // If we got here, it's a file - return true
    builder->CreateRet(builder->getTrue());
}
