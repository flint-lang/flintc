#include "generator/generator.hpp"
#include "lexer/builtins.hpp"

static const Hash hash(std::string("read"));
static const std::string prefix = hash.to_string() + ".read.";

void Generator::Module::Read::generate_getline_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // char *getline(long *n) {
    //     const int EOF = -1;
    //     size_t cap = 128;
    //     size_t len = 0;
    //     char *buf = (char *)malloc(cap);
    //     int c;
    //     while ((c = fgetc(stdin)) != EOF) {
    //         // grow if needed
    //         if (len + 1 >= cap) {
    //             cap *= 2;
    //             buf = (char *)realloc(buf, cap);
    //         }
    //         buf[len++] = (char)c;
    //         if (c == '\n') {
    //             break;
    //         }
    //     }
    //     // if nothing read & EOF: signal end-of-input
    //     if (len == 0 && c == EOF) {
    //         free(buf);
    //         *n = 0;
    //         return NULL;
    //     }
    //     // strip trailing newline
    //     if (len > 0 && buf[len - 1] == '\n') {
    //         buf[--len] = '\0';
    //     }
    //     *n = len;
    //     return buf;
    // }
    llvm::Function *const malloc_fn = c_functions.at(MALLOC);
    llvm::Function *const fgetc_fn = c_functions.at(FGETC);
    llvm::Function *const realloc_fn = c_functions.at(REALLOC);
    llvm::Function *const free_fn = c_functions.at(FREE);

    // Create print function type
    llvm::FunctionType *const getline_type = llvm::FunctionType::get( //
        PTR_TY,                                                       // return char*
        {PTR_TY},                                                     // long* n
        false                                                         // no vaarg
    );
    // Create the print_int function
    getline_function = llvm::Function::Create( //
        getline_type,                          //
        llvm::Function::ExternalLinkage,       //
        prefix + "getline",                    //
        module                                 //
    );
    if (only_declarations) {
        return;
    }

    // Get function parameter (n pointer)
    llvm::Argument *const arg_n_ptr = getline_function->arg_begin();
    arg_n_ptr->setName("n_ptr");

    // Create basic blocks for the function
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", getline_function);
    llvm::BasicBlock *const loop_entry = llvm::BasicBlock::Create(context, "loop_entry", getline_function);
    llvm::BasicBlock *const loop_body = llvm::BasicBlock::Create(context, "loop_body", getline_function);
    llvm::BasicBlock *const do_realloc = llvm::BasicBlock::Create(context, "do_realloc", getline_function);
    llvm::BasicBlock *const after_loop = llvm::BasicBlock::Create(context, "after_loop", getline_function);
    llvm::BasicBlock *const handle_eof = llvm::BasicBlock::Create(context, "handle_eof", getline_function);
    llvm::BasicBlock *const strip_newline = llvm::BasicBlock::Create(context, "strip_newline", getline_function);
    llvm::BasicBlock *const exit_block = llvm::BasicBlock::Create(context, "exit_block", getline_function);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Initialize constants
    llvm::Value *const const_eof = builder->getInt32(-1);
    llvm::Value *const const_newline = builder->getInt32('\n');
    llvm::Value *const const_null = builder->getInt8(0);

    // Allocate stack variables
    // cap = 128
    llvm::Value *const cap_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "cap_ptr");
    IR::aligned_store(*builder, builder->getInt64(128), cap_ptr);

    // len = 0
    llvm::Value *const len_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "len_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), len_ptr);

    // c variable to hold the character read
    llvm::Value *const c_ptr = builder->CreateAlloca(builder->getInt32Ty(), nullptr, "c_ptr");

    // char *buf = (char *)malloc(cap)
    llvm::Value *const initial_cap = IR::aligned_load(*builder, builder->getInt64Ty(), cap_ptr, "initial_cap");
    llvm::Value *const buf_ptr_alloca = builder->CreateAlloca(PTR_TY, nullptr, "buf_ptr_alloca");
    llvm::Value *const buf_malloc = builder->CreateCall(malloc_fn, {initial_cap}, "buf_malloc");
    llvm::Value *const buf_init = builder->CreateBitCast(buf_malloc, PTR_TY, "buf");
    IR::aligned_store(*builder, buf_init, buf_ptr_alloca);

#ifdef __WIN32__
    // Windows: call the UCRT helper __acrt_iob_func() to get the FILE' array, then index element 0 to get stdin
    // Declare or look up __acrt_iob_func
    llvm::Type *const iobuf_ty = PTR_TY;
    llvm::FunctionCallee ac_rt_iob = module->getOrInsertFunction("__acrt_iob_func", llvm::FunctionType::get(iobuf_ty, {}, false));
    // call it
    llvm::Value *const io_array = builder->CreateCall(ac_rt_iob, {}, "io_array");
    // GEP [0] to pick stdin
    llvm::Value *const zero = builder->getInt32(0);
    llvm::Value *const stdin_ptr = builder->CreateInBoundsGEP(iobuf_ty, io_array, {zero}, "stdin_ptr");
    // Load the actual FILE*
    llvm::Value *const stdin_val = IR::aligned_load(*builder, PTR_TY, stdin_ptr, "stdin");
#else
    // Get file* stdin - needs to access global stdin
    llvm::Value *const stdin_ptr = module->getOrInsertGlobal("stdin", PTR_TY);
    llvm::Value *const stdin_val = IR::aligned_load(*builder, PTR_TY, stdin_ptr, "stdin");
#endif

    // Branch to the loop entry
    builder->CreateBr(loop_entry);

    // Loop entry: while ((c = fgetc(stdin)) != EOF)
    builder->SetInsertPoint(loop_entry);

    // Read a character: c = fgetc(stdin)
    llvm::Value *c_val = builder->CreateCall(fgetc_fn, {stdin_val}, "c");
    IR::aligned_store(*builder, c_val, c_ptr);

    // Check if c != EOF
    llvm::Value *const cond = builder->CreateICmpNE(c_val, const_eof, "cmp_eof");
    builder->CreateCondBr(cond, loop_body, after_loop);

    // Loop body
    builder->SetInsertPoint(loop_body);

    // Check if realloc is needed: if (len + 1 >= cap)
    llvm::Value *curr_len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "curr_len");
    llvm::Value *const len_plus_one = builder->CreateAdd(curr_len, builder->getInt64(1), "len_plus_one");
    llvm::Value *const curr_cap = IR::aligned_load(*builder, builder->getInt64Ty(), cap_ptr, "curr_cap");
    llvm::Value *const need_realloc = builder->CreateICmpUGE(len_plus_one, curr_cap, "need_realloc");
    builder->CreateCondBr(need_realloc, do_realloc, loop_body->getNextNode());

    // Realloc block: cap *= 2; buf = realloc(buf, cap);
    builder->SetInsertPoint(do_realloc);
    llvm::Value *const new_cap = builder->CreateMul(curr_cap, builder->getInt64(2), "new_cap");
    IR::aligned_store(*builder, new_cap, cap_ptr);

    llvm::Value *curr_buf = IR::aligned_load(*builder, PTR_TY, buf_ptr_alloca, "curr_buf");
    llvm::Value *const new_buf_malloc = builder->CreateCall(realloc_fn, {curr_buf, new_cap}, "new_buf_malloc");
    llvm::Value *const new_buf = builder->CreateBitCast(new_buf_malloc, PTR_TY, "new_buf");
    IR::aligned_store(*builder, new_buf, buf_ptr_alloca);

    // Create a new block for storing the character after possible reallocation
    llvm::BasicBlock *const store_char = llvm::BasicBlock::Create(context, "store_char", getline_function, after_loop);
    builder->CreateBr(store_char);

    // Store character: buf[len++] = (char)c
    builder->SetInsertPoint(store_char);
    curr_buf = IR::aligned_load(*builder, PTR_TY, buf_ptr_alloca, "curr_buf");
    curr_len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "curr_len");

    // Get pointer to buf[len]
    llvm::Value *const buf_pos = builder->CreateGEP(builder->getInt8Ty(), curr_buf, curr_len, "buf_pos");

    // Get current character value
    c_val = IR::aligned_load(*builder, builder->getInt32Ty(), c_ptr, "c_val");

    // Store character
    llvm::Value *const c_as_char = builder->CreateTrunc(c_val, builder->getInt8Ty(), "c_as_char");
    IR::aligned_store(*builder, c_as_char, buf_pos);

    // Increment len
    llvm::Value *const new_len = builder->CreateAdd(curr_len, builder->getInt64(1), "new_len");
    IR::aligned_store(*builder, new_len, len_ptr);

    // Check if c == '\n'
    llvm::Value *const is_newline = builder->CreateICmpEQ(c_val, const_newline, "is_newline");
    builder->CreateCondBr(is_newline, after_loop, loop_entry);

    // After loop: handle EOF and stripping newline
    builder->SetInsertPoint(after_loop);

    // Check if len == 0 && c == EOF
    curr_len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "curr_len");
    c_val = IR::aligned_load(*builder, builder->getInt32Ty(), c_ptr, "c_val");
    llvm::Value *const len_is_zero = builder->CreateICmpEQ(curr_len, builder->getInt64(0), "len_is_zero");
    llvm::Value *const c_is_eof = builder->CreateICmpEQ(c_val, const_eof, "c_is_eof");
    llvm::Value *const eof_and_empty = builder->CreateAnd(len_is_zero, c_is_eof, "eof_and_empty");
    builder->CreateCondBr(eof_and_empty, handle_eof, strip_newline);

    // Handle EOF case: free buffer, set *n=0, return NULL
    builder->SetInsertPoint(handle_eof);
    curr_buf = IR::aligned_load(*builder, PTR_TY, buf_ptr_alloca, "curr_buf");
    builder->CreateCall(free_fn, {curr_buf});
    IR::aligned_store(*builder, builder->getInt64(0), arg_n_ptr);
    builder->CreateRet(llvm::ConstantPointerNull::get(PTR_TY));

    // Strip newline block
    builder->SetInsertPoint(strip_newline);

    // Check if we need to strip newline: if (len > 0 && buf[len-1] == '\n')
    curr_len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "curr_len");
    llvm::Value *const len_gt_zero = builder->CreateICmpUGT(curr_len, builder->getInt64(0), "len_gt_zero");

    // Create blocks for the nested check
    llvm::BasicBlock *const check_last_char = llvm::BasicBlock::Create(context, "check_last_char", getline_function, exit_block);
    llvm::BasicBlock *const do_strip = llvm::BasicBlock::Create(context, "do_strip", getline_function, exit_block);

    builder->CreateCondBr(len_gt_zero, check_last_char, exit_block);

    // Check if last char is newline
    builder->SetInsertPoint(check_last_char);
    curr_buf = IR::aligned_load(*builder, PTR_TY, buf_ptr_alloca, "curr_buf");
    llvm::Value *const last_idx = builder->CreateSub(curr_len, builder->getInt64(1), "last_idx");
    llvm::Value *const last_char_ptr = builder->CreateGEP(builder->getInt8Ty(), curr_buf, last_idx, "last_char_ptr");
    llvm::Value *const last_char = IR::aligned_load(*builder, builder->getInt8Ty(), last_char_ptr, "last_char");
    llvm::Value *const is_last_newline = builder->CreateICmpEQ(last_char, builder->getInt8('\n'), "is_last_newline");
    builder->CreateCondBr(is_last_newline, do_strip, exit_block);

    // Strip the newline: buf[--len] = '\0'
    builder->SetInsertPoint(do_strip);
    llvm::Value *const stripped_len = builder->CreateSub(curr_len, builder->getInt64(1), "stripped_len");
    IR::aligned_store(*builder, stripped_len, len_ptr);

    // Get pointer to buf[len-1]
    curr_buf = IR::aligned_load(*builder, PTR_TY, buf_ptr_alloca, "curr_buf");
    llvm::Value *const null_pos = builder->CreateGEP(builder->getInt8Ty(), curr_buf, stripped_len, "null_pos");

    // Store null terminator
    IR::aligned_store(*builder, const_null, null_pos);
    builder->CreateBr(exit_block);

    // Exit block: store len to *n and return buf
    builder->SetInsertPoint(exit_block);
    curr_len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "final_len");
    IR::aligned_store(*builder, curr_len, arg_n_ptr);
    curr_buf = IR::aligned_load(*builder, PTR_TY, buf_ptr_alloca, "final_buf");
    builder->CreateRet(curr_buf);
}

void Generator::Module::Read::generate_read_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // str *read_str() {
    //     long len = 0;
    //     char *buffer = flint.getline(&len);
    //     if (buffer == NULL) {
    //         printf("Something went wrong\n");
    //         abort();
    //     }
    //     // Reallocate the buffer to match the size of the string
    //     size_t header = sizeof(str);
    //     buffer = (char *)realloc(buffer, header + len);
    //     memmove(buffer + header, buffer, len);
    //     str *result = (str *)buffer;
    //     result->len = len;
    //     return result;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::Function *const printf_fn = c_functions.at(PRINTF);
    llvm::Function *const fflush_fn = c_functions.at(FFLUSH);
    llvm::Function *const abort_fn = c_functions.at(ABORT);
    llvm::Function *const realloc_fn = c_functions.at(REALLOC);
    llvm::Function *const memmove_fn = c_functions.at(MEMMOVE);

    llvm::FunctionType *const read_str_type = llvm::FunctionType::get(PTR_TY, false);
    llvm::Function *const read_str_fn = llvm::Function::Create( //
        read_str_type,                                          //
        llvm::Function::ExternalLinkage,                        //
        prefix + "read_str",                                    //
        module                                                  //
    );
    read_functions["read_str"] = read_str_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", read_str_fn);
    llvm::BasicBlock *const error_block = llvm::BasicBlock::Create(context, "error", read_str_fn);
    llvm::BasicBlock *const continue_block = llvm::BasicBlock::Create(context, "continue", read_str_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Create len variable: long len = 0
    llvm::Value *const len_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "len_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), len_ptr);

    // Call getline: char *buffer = flint.getline(&len)
    llvm::Value *const buffer = builder->CreateCall(getline_function, {len_ptr}, "buffer");

    // Check if buffer is NULL
    llvm::Value *const is_null = builder->CreateICmpEQ(buffer, llvm::ConstantPointerNull::get(PTR_TY), "is_null");
    builder->CreateCondBr(is_null, error_block, continue_block);

    // Error block: print error and abort
    builder->SetInsertPoint(error_block);

    // In a real implementation, we'd add a printf call here
    // For simplicity, we'll just abort directly since the error message
    // is mostly informational
    llvm::Value *const format_str = IR::generate_const_string(module, "Got a NULL from flint.getline function call\n");
    builder->CreateCall(printf_fn, {format_str});
    builder->CreateCall(fflush_fn, {llvm::ConstantPointerNull::get(PTR_TY)});
    builder->CreateCall(abort_fn, {});
    builder->CreateUnreachable();

    // Continue with normal execution
    builder->SetInsertPoint(continue_block);

    // Get the length value
    llvm::Value *const len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Calculate header size: size_t header = sizeof(str)
    llvm::Value *const header_size = builder->getInt64(Allocation::get_type_size(module, str_type));

    // Calculate new buffer size: header + len
    llvm::Value *const new_size = builder->CreateAdd(header_size, len, "new_size");

    // Reallocate buffer: buffer = (char *)realloc(buffer, header + len)
    llvm::Value *const new_buffer = builder->CreateCall(realloc_fn, {buffer, new_size}, "new_buffer");

    // Calculate destination pointer: buffer + header
    llvm::Value *const dest_ptr = builder->CreateGEP(builder->getInt8Ty(), new_buffer, header_size, "dest_ptr");

    // Move the string content: memmove(buffer + header, buffer, len)
    builder->CreateCall(memmove_fn, {dest_ptr, new_buffer, len});

    // Cast buffer to str*: str *result = (str *)buffer
    llvm::Value *const result = builder->CreateBitCast(new_buffer, PTR_TY, "result");

    // Set the length: result->len = len
    llvm::Value *const len_field_ptr = builder->CreateStructGEP(str_type, result, 0, "len_field_ptr");
    IR::aligned_store(*builder, len, len_field_ptr);

    // Return the str pointer
    builder->CreateRet(result);
}

void Generator::Module::Read::generate_read_int_function( //
    llvm::IRBuilder<> *builder,                           //
    llvm::Module *module,                                 //
    const bool only_declarations,                         //
    const std::shared_ptr<Type> &result_type_ptr          //
) {
    // THE C IMPLEMENTATION:
    // int32_t read_i32() {
    //     long len = 0;
    //     char *buffer = flint.getline(&len);
    //     if (buffer == NULL) {
    //         printf("Something went wrong\n");
    //         abort();
    //     }
    //     char *endptr = NULL;
    //     long value = strtol(buffer, &endptr, 10);
    //     // The whole string should have been parsed
    //     if (endptr < buffer + len) {
    //         printf("Not whole buffer read!\n");
    //         abort();
    //     }
    //     return (int32_t)value;
    // }
    llvm::Function *const strtol_fn = c_functions.at(STRTOL);

    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrRead = hash.get_type_id_from_str("ErrRead");
    const std::vector<error_value> &ErrReadValues = std::get<2>(core_module_error_sets.at("read").at(0));
    const unsigned int ReadLines = 0;
    const unsigned int ParseInt = 1;
    const std::string ReadLinesMessage(ErrReadValues.at(ReadLines).second);
    const std::string ParseIntMessage(ErrReadValues.at(ParseInt).second);
    llvm::Type *const result_type = IR::get_type(module, result_type_ptr).type;
    llvm::FunctionType *const read_int_type = llvm::FunctionType::get(function_result_type, false);
    llvm::Function *const read_int_fn = llvm::Function::Create(                //
        read_int_type,                                                         //
        llvm::Function::ExternalLinkage,                                       //
        prefix + "read_i" + std::to_string(result_type->getIntegerBitWidth()), //
        module                                                                 //
    );
    read_functions["read_i" + std::to_string(result_type->getIntegerBitWidth())] = read_int_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", read_int_fn);
    llvm::BasicBlock *const error_block = llvm::BasicBlock::Create(context, "error", read_int_fn);
    llvm::BasicBlock *const continue_block = llvm::BasicBlock::Create(context, "continue", read_int_fn);
    llvm::BasicBlock *const parse_error_block = llvm::BasicBlock::Create(context, "parse_error", read_int_fn);
    llvm::BasicBlock *const exit_block = llvm::BasicBlock::Create(context, "exit", read_int_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Create len variable: long len = 0
    llvm::Value *const len_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "len_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), len_ptr);

    // Call getline: char *buffer = flint.getline(&len)
    llvm::Value *const buffer = builder->CreateCall(getline_function, {len_ptr}, "buffer");

    // Check if buffer is NULL
    llvm::Value *const is_null = builder->CreateICmpEQ(buffer, llvm::ConstantPointerNull::get(PTR_TY), "is_null");
    builder->CreateCondBr(is_null, error_block, continue_block);

    // Error block: throw error ErrRead.ReadLines
    builder->SetInsertPoint(error_block);
    llvm::AllocaInst *const creation_ret_alloca = Allocation::generate_default_struct( //
        *builder, function_result_type, "creation_ret_alloca", true                    //
    );
    llvm::Value *const creation_err_ptr = builder->CreateStructGEP(function_result_type, creation_ret_alloca, 0, "creation_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrRead, ReadLines, ReadLinesMessage);
    IR::aligned_store(*builder, err_value, creation_err_ptr);
    llvm::Value *const creation_ret_val = IR::aligned_load(*builder, function_result_type, creation_ret_alloca, "creation_ret_val");
    builder->CreateRet(creation_ret_val);

    // Continue with normal execution
    builder->SetInsertPoint(continue_block);

    // Get the length value
    llvm::Value *const len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Create endptr variable: char *endptr = NULL
    llvm::Value *const endptr_ptr = builder->CreateAlloca(PTR_TY, nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(PTR_TY), endptr_ptr);

    // Call strtol: long value = strtol(buffer, &endptr, 10)
    llvm::Value *const base = builder->getInt32(10); // base 10
    llvm::Value *const value = builder->CreateCall(strtol_fn, {buffer, endptr_ptr, base}, "value");

    // Load the endptr value after strtol call
    llvm::Value *const endptr = IR::aligned_load(*builder, PTR_TY, endptr_ptr, "endptr");

    // Calculate buffer + len (end of the buffer)
    llvm::Value *const buffer_end = builder->CreateGEP(builder->getInt8Ty(), buffer, len, "buffer_end");

    // Check if endptr < buffer + len
    llvm::Value *const endptr_lt_end = builder->CreateICmpULT(endptr, buffer_end, "endptr_lt_end");
    builder->CreateCondBr(endptr_lt_end, parse_error_block, exit_block);

    // Parse error block: throw error ErrRead.ParseInt
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *const parse_ret_alloca =
        Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    llvm::Value *const parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrRead, ParseInt, ParseIntMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *const parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Create exit block for the final return
    builder->SetInsertPoint(exit_block);

    // Convert long to the required integer type: return (int32_t)value
    llvm::Value *result_value;
    if (result_type->getIntegerBitWidth() < 64) {
        // Truncate if result type is smaller than long (64-bit)
        result_value = builder->CreateTrunc(value, result_type, "result_value");
    } else if (result_type->getIntegerBitWidth() > 64) {
        // Sign-extend if result type is larger than long (64-bit)
        result_value = builder->CreateSExt(value, result_type, "result_value");
    } else {
        // Same bit width, no conversion needed
        result_value = value;
    }

    // Return the converted value
    llvm::AllocaInst *const ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *const val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    IR::aligned_store(*builder, result_value, val_ptr);
    llvm::Value *const ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::Read::generate_read_uint_function( //
    llvm::IRBuilder<> *builder,                            //
    llvm::Module *module,                                  //
    const bool only_declarations,                          //
    const std::shared_ptr<Type> &result_type_ptr           //
) {
    // THE C IMPLEMENTATION:
    // uint32_t read_u32() {
    //     long len = 0;
    //     char *buffer = flint.getline(&len);
    //     if (buffer == NULL) {
    //         printf("Something went wrong\n");
    //         abort();
    //     }
    //     if (len > 0 && buffer[0] == '-') {
    //         printf("Negative input not allowed for unsigned types!\n");
    //         abort();
    //     }
    //     char *endptr = NULL;
    //     unsigned long value = strtoul(buffer, &endptr, 10);
    //     // The whole string should have been parsed
    //     if (endptr < buffer + len) {
    //         printf("Not whole buffer read!\n");
    //         abort();
    //     }
    //     return (uint32_t)value;
    // }
    llvm::Function *const strtoul_fn = c_functions.at(STRTOUL);

    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrRead = hash.get_type_id_from_str("ErrRead");
    const std::vector<error_value> &ErrReadValues = std::get<2>(core_module_error_sets.at("read").at(0));
    const unsigned int ReadLines = 0;
    const unsigned int ParseInt = 1;
    const unsigned int NegativeUint = 2;
    const std::string ReadLinesMessage(ErrReadValues.at(ReadLines).second);
    const std::string ParseIntMessage(ErrReadValues.at(ParseInt).second);
    const std::string NegativeUintMessage(ErrReadValues.at(NegativeUint).second);
    llvm::Type *const result_type = IR::get_type(module, result_type_ptr).type;
    llvm::FunctionType *const read_uint_type = llvm::FunctionType::get(function_result_type, false);
    llvm::Function *const read_uint_fn = llvm::Function::Create(               //
        read_uint_type,                                                        //
        llvm::Function::ExternalLinkage,                                       //
        prefix + "read_u" + std::to_string(result_type->getIntegerBitWidth()), //
        module                                                                 //
    );
    read_functions["read_u" + std::to_string(result_type->getIntegerBitWidth())] = read_uint_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", read_uint_fn);
    llvm::BasicBlock *const error_block = llvm::BasicBlock::Create(context, "error", read_uint_fn);
    llvm::BasicBlock *const continue_block = llvm::BasicBlock::Create(context, "continue", read_uint_fn);
    llvm::BasicBlock *const check_negative_block = llvm::BasicBlock::Create(context, "check_negative", read_uint_fn);
    llvm::BasicBlock *const negative_error_block = llvm::BasicBlock::Create(context, "negative_error", read_uint_fn);
    llvm::BasicBlock *const parse_block = llvm::BasicBlock::Create(context, "parse", read_uint_fn);
    llvm::BasicBlock *const parse_error_block = llvm::BasicBlock::Create(context, "parse_error", read_uint_fn);
    llvm::BasicBlock *const exit_block = llvm::BasicBlock::Create(context, "exit", read_uint_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Create len variable: long len = 0
    llvm::Value *const len_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "len_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), len_ptr);

    // Call getline: char *buffer = flint.getline(&len)
    llvm::Value *const buffer = builder->CreateCall(getline_function, {len_ptr}, "buffer");

    // Check if buffer is NULL
    llvm::Value *const is_null = builder->CreateICmpEQ(buffer, llvm::ConstantPointerNull::get(PTR_TY), "is_null");
    builder->CreateCondBr(is_null, error_block, continue_block);

    // Error block: throw ErrRead.ReadLines
    builder->SetInsertPoint(error_block);
    llvm::AllocaInst *const create_ret_alloca = Allocation::generate_default_struct( //
        *builder, function_result_type, "create_ret_alloca", true                    //
    );
    llvm::Value *const create_err_ptr = builder->CreateStructGEP(function_result_type, create_ret_alloca, 0, "create_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrRead, ReadLines, ReadLinesMessage);
    IR::aligned_store(*builder, err_value, create_err_ptr);
    llvm::Value *const create_ret_val = IR::aligned_load(*builder, function_result_type, create_ret_alloca, "create_ret_val");
    builder->CreateRet(create_ret_val);

    // Continue with normal execution
    builder->SetInsertPoint(continue_block);

    // Get the length value
    llvm::Value *const len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Check if the length is greater than zero
    llvm::Value *const len_gt_zero = builder->CreateICmpUGT(len, builder->getInt64(0), "len_gt_zero");
    builder->CreateCondBr(len_gt_zero, check_negative_block, parse_block);

    // Check if the first character is a negative sign
    builder->SetInsertPoint(check_negative_block);

    // Load the first character: buffer[0]
    llvm::Value *const first_char_ptr = builder->CreateGEP(builder->getInt8Ty(), buffer, builder->getInt64(0), "first_char_ptr");
    llvm::Value *const first_char = IR::aligned_load(*builder, builder->getInt8Ty(), first_char_ptr, "first_char");

    // Check if first character is '-'
    llvm::Value *const is_negative = builder->CreateICmpEQ(first_char, builder->getInt8('-'), "is_negative");
    builder->CreateCondBr(is_negative, negative_error_block, parse_block);

    // Negative error block: throw ErrRead.NegativeUint
    builder->SetInsertPoint(negative_error_block);
    llvm::AllocaInst *const neg_ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "neg_ret_alloca", true);
    llvm::Value *const neg_err_ptr = builder->CreateStructGEP(function_result_type, neg_ret_alloca, 0, "neg_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrRead, NegativeUint, NegativeUintMessage);
    IR::aligned_store(*builder, err_value, neg_err_ptr);
    llvm::Value *const neg_ret_val = IR::aligned_load(*builder, function_result_type, neg_ret_alloca, "neg_ret_val");
    builder->CreateRet(neg_ret_val);

    // Parse block: parse the string with strtoul
    builder->SetInsertPoint(parse_block);

    // Create endptr variable: char *endptr = NULL
    llvm::Value *const endptr_ptr = builder->CreateAlloca(PTR_TY, nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(PTR_TY), endptr_ptr);

    // Call strtoul: unsigned long value = strtoul(buffer, &endptr, 10)
    llvm::Value *const base = builder->getInt32(10); // base 10
    llvm::Value *const value = builder->CreateCall(strtoul_fn, {buffer, endptr_ptr, base}, "value");

    // Load the endptr value after strtoul call
    llvm::Value *const endptr = IR::aligned_load(*builder, PTR_TY, endptr_ptr, "endptr");

    // Calculate buffer + len (end of the buffer)
    llvm::Value *const buffer_end = builder->CreateGEP(builder->getInt8Ty(), buffer, len, "buffer_end");

    // Check if endptr < buffer + len
    llvm::Value *const endptr_lt_end = builder->CreateICmpULT(endptr, buffer_end, "endptr_lt_end");
    builder->CreateCondBr(endptr_lt_end, parse_error_block, exit_block);

    // Parse error block: throw ErrRead.ParseInt
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *const parse_ret_alloca =
        Allocation::generate_default_struct(*builder, function_result_type, "parse_ret_alloca", true);
    llvm::Value *const parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrRead, ParseInt, ParseIntMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *const parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Create exit block for the final return
    builder->SetInsertPoint(exit_block);

    // Convert unsigned long to the required integer type: return (uint32_t)value
    llvm::Value *result_value;
    if (result_type->getIntegerBitWidth() < 64) {
        // Truncate if result type is smaller than unsigned long (64-bit)
        result_value = builder->CreateTrunc(value, result_type, "result_value");
    } else if (result_type->getIntegerBitWidth() > 64) {
        // Zero-extend if result type is larger than unsigned long (64-bit)
        result_value = builder->CreateZExt(value, result_type, "result_value");
    } else {
        // Same bit width, no conversion needed
        result_value = value;
    }

    // Return the converted value
    llvm::AllocaInst *const ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *const val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    IR::aligned_store(*builder, result_value, val_ptr);
    llvm::Value *const ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::Read::generate_read_f32_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // float read_f32() {
    //     long len = 0;
    //     char *buffer = flint.getline(&len);
    //     if (buffer == NULL) {
    //         printf("Something went wrong\n");
    //         abort();
    //     }
    //     char *endptr = NULL;
    //     float value = strtof(buffer, &endptr);
    //     // The whole string should have been parsed
    //     if (endptr < buffer + len) {
    //         printf("Not whole buffer read!\n");
    //         abort();
    //     }
    //     return value;
    // }
    llvm::Function *const strtof_fn = c_functions.at(STRTOF);

    const std::shared_ptr<Type> result_type_ptr = Type::get_primitive_type("f32");
    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrRead = hash.get_type_id_from_str("ErrRead");
    const std::vector<error_value> &ErrReadValues = std::get<2>(core_module_error_sets.at("read").at(0));
    const unsigned int ReadLines = 0;
    const unsigned int ParseFloat = 3;
    const std::string ReadLinesMessage(ErrReadValues.at(ReadLines).second);
    const std::string ParseFloatMessage(ErrReadValues.at(ParseFloat).second);
    llvm::FunctionType *const read_f32_type = llvm::FunctionType::get(function_result_type, false);
    llvm::Function *const read_f32_fn = llvm::Function::Create( //
        read_f32_type,                                          //
        llvm::Function::ExternalLinkage,                        //
        prefix + "read_f32",                                    //
        module                                                  //
    );
    read_functions["read_f32"] = read_f32_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", read_f32_fn);
    llvm::BasicBlock *const error_block = llvm::BasicBlock::Create(context, "error", read_f32_fn);
    llvm::BasicBlock *const continue_block = llvm::BasicBlock::Create(context, "continue", read_f32_fn);
    llvm::BasicBlock *const parse_error_block = llvm::BasicBlock::Create(context, "parse_error", read_f32_fn);
    llvm::BasicBlock *const exit_block = llvm::BasicBlock::Create(context, "exit", read_f32_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Create len variable: long len = 0
    llvm::Value *const len_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "len_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), len_ptr);

    // Call getline: char *buffer = flint.getline(&len)
    llvm::Value *const buffer = builder->CreateCall(getline_function, {len_ptr}, "buffer");

    // Check if buffer is NULL
    llvm::Value *const is_null = builder->CreateICmpEQ(buffer, llvm::ConstantPointerNull::get(PTR_TY), "is_null");
    builder->CreateCondBr(is_null, error_block, continue_block);

    // Error block: throw ErrRead.ReadLines
    builder->SetInsertPoint(error_block);
    llvm::AllocaInst *const create_ret_alloca = Allocation::generate_default_struct( //
        *builder, function_result_type, "create_ret_alloca", true                    //
    );
    llvm::Value *const create_err_ptr = builder->CreateStructGEP(function_result_type, create_ret_alloca, 0, "create_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrRead, ReadLines, ReadLinesMessage);
    IR::aligned_store(*builder, err_value, create_err_ptr);
    llvm::Value *const create_ret_val = IR::aligned_load(*builder, function_result_type, create_ret_alloca, "create_ret_val");
    builder->CreateRet(create_ret_val);

    // Continue with normal execution
    builder->SetInsertPoint(continue_block);

    // Get the length value
    llvm::Value *const len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Create endptr variable: char *endptr = NULL
    llvm::Value *const endptr_ptr = builder->CreateAlloca(PTR_TY, nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(PTR_TY), endptr_ptr);

    // Call strtof: float value = strtof(buffer, &endptr)
    llvm::Value *const value = builder->CreateCall(strtof_fn, {buffer, endptr_ptr}, "value");

    // Load the endptr value after strtof call
    llvm::Value *const endptr = IR::aligned_load(*builder, PTR_TY, endptr_ptr, "endptr");

    // Calculate buffer + len (end of the buffer)
    llvm::Value *const buffer_end = builder->CreateGEP(builder->getInt8Ty(), buffer, len, "buffer_end");

    // Check if endptr < buffer + len (not all input was parsed)
    llvm::Value *const endptr_lt_end = builder->CreateICmpULT(endptr, buffer_end, "endptr_lt_end");

    // Branch if an parse error occured
    builder->CreateCondBr(endptr_lt_end, parse_error_block, exit_block);

    // Parse warning: throw ErrRead.ParseFloat
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *const parse_ret_alloca = Allocation::generate_default_struct( //
        *builder, function_result_type, "parse_ret_alloca", true                    //
    );
    llvm::Value *const parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrRead, ParseFloat, ParseFloatMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *const parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Exit block: return the float value
    builder->SetInsertPoint(exit_block);
    llvm::AllocaInst *const ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *const val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    IR::aligned_store(*builder, value, val_ptr);
    llvm::Value *const ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::Read::generate_read_f64_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // double read_f64() {
    //     long len = 0;
    //     char *buffer = flint.getline(&len);
    //     if (buffer == NULL) {
    //         printf("Something went wrong\n");
    //         abort();
    //     }
    //     char *endptr = NULL;
    //     double value = strtod(buffer, &endptr);
    //     // The whole string should have been parsed
    //     if (endptr < buffer + len) {
    //         printf("Not whole buffer read!\n");
    //         abort();
    //     }
    //     return value;
    // }
    llvm::Function *const strtod_fn = c_functions.at(STRTOD);

    const std::shared_ptr<Type> result_type_ptr = Type::get_primitive_type("f64");
    llvm::StructType *const function_result_type = IR::add_and_or_get_type(module, result_type_ptr, true);
    const unsigned int ErrRead = hash.get_type_id_from_str("ErrRead");
    const std::vector<error_value> &ErrReadValues = std::get<2>(core_module_error_sets.at("read").at(0));
    const unsigned int ReadLines = 0;
    const unsigned int ParseFloat = 3;
    const std::string ReadLinesMessage(ErrReadValues.at(ReadLines).second);
    const std::string ParseFloatMessage(ErrReadValues.at(ParseFloat).second);
    llvm::FunctionType *const read_f64_type = llvm::FunctionType::get(function_result_type, false);
    llvm::Function *const read_f64_fn = llvm::Function::Create( //
        read_f64_type,                                          //
        llvm::Function::ExternalLinkage,                        //
        prefix + "read_f64",                                    //
        module                                                  //
    );
    read_functions["read_f64"] = read_f64_fn;
    if (only_declarations) {
        return;
    }

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", read_f64_fn);
    llvm::BasicBlock *const error_block = llvm::BasicBlock::Create(context, "error", read_f64_fn);
    llvm::BasicBlock *const continue_block = llvm::BasicBlock::Create(context, "continue", read_f64_fn);
    llvm::BasicBlock *const parse_error_block = llvm::BasicBlock::Create(context, "parse_error", read_f64_fn);
    llvm::BasicBlock *const exit_block = llvm::BasicBlock::Create(context, "exit", read_f64_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Create len variable: long len = 0
    llvm::Value *const len_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "len_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), len_ptr);

    // Call getline: char *buffer = flint.getline(&len)
    llvm::Value *const buffer = builder->CreateCall(getline_function, {len_ptr}, "buffer");

    // Check if buffer is NULL
    llvm::Value *const is_null = builder->CreateICmpEQ(buffer, llvm::ConstantPointerNull::get(PTR_TY), "is_null");
    builder->CreateCondBr(is_null, error_block, continue_block);

    // Error block: throw ErrRead.ReadLines
    builder->SetInsertPoint(error_block);
    llvm::AllocaInst *const create_ret_alloca = Allocation::generate_default_struct( //
        *builder, function_result_type, "create_ret_alloca", true                    //
    );
    llvm::Value *const create_err_ptr = builder->CreateStructGEP(function_result_type, create_ret_alloca, 0, "create_err_ptr");
    llvm::Value *err_value = IR::generate_err_value(*builder, module, ErrRead, ReadLines, ReadLinesMessage);
    IR::aligned_store(*builder, err_value, create_err_ptr);
    llvm::Value *const create_ret_val = IR::aligned_load(*builder, function_result_type, create_ret_alloca, "create_ret_val");
    builder->CreateRet(create_ret_val);

    // Continue with normal execution
    builder->SetInsertPoint(continue_block);

    // Get the length value
    llvm::Value *const len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "len");

    // Create endptr variable: char *endptr = NULL
    llvm::Value *const endptr_ptr = builder->CreateAlloca(PTR_TY, nullptr, "endptr_ptr");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(PTR_TY), endptr_ptr);

    // Call strtod: double value = strtod(buffer, &endptr)
    llvm::Value *const value = builder->CreateCall(strtod_fn, {buffer, endptr_ptr}, "value");

    // Load the endptr value after strtod call
    llvm::Value *const endptr = IR::aligned_load(*builder, PTR_TY, endptr_ptr, "endptr");

    // Calculate buffer + len (end of the buffer)
    llvm::Value *const buffer_end = builder->CreateGEP(builder->getInt8Ty(), buffer, len, "buffer_end");

    // Check if endptr < buffer + len (not all input was parsed)
    llvm::Value *const endptr_lt_end = builder->CreateICmpULT(endptr, buffer_end, "endptr_lt_end");

    // Branch if an parse error occured
    builder->CreateCondBr(endptr_lt_end, parse_error_block, exit_block);

    // Parse warning block: throw ErrRead.ParseFloat
    builder->SetInsertPoint(parse_error_block);
    llvm::AllocaInst *const parse_ret_alloca = Allocation::generate_default_struct( //
        *builder, function_result_type, "parse_ret_alloca", true                    //
    );
    llvm::Value *const parse_err_ptr = builder->CreateStructGEP(function_result_type, parse_ret_alloca, 0, "parse_err_ptr");
    err_value = IR::generate_err_value(*builder, module, ErrRead, ParseFloat, ParseFloatMessage);
    IR::aligned_store(*builder, err_value, parse_err_ptr);
    llvm::Value *const parse_ret_val = IR::aligned_load(*builder, function_result_type, parse_ret_alloca, "parse_ret_val");
    builder->CreateRet(parse_ret_val);

    // Exit block: return the double value
    builder->SetInsertPoint(exit_block);
    llvm::AllocaInst *const ret_alloca = Allocation::generate_default_struct(*builder, function_result_type, "ret_alloca", false);
    llvm::Value *const val_ptr = builder->CreateStructGEP(function_result_type, ret_alloca, 1, "ret_value_ptr");
    IR::aligned_store(*builder, value, val_ptr);
    llvm::Value *const ret_val = IR::aligned_load(*builder, function_result_type, ret_alloca, "ret_val");
    builder->CreateRet(ret_val);
}

void Generator::Module::Read::generate_read_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_getline_function(builder, module, only_declarations);
    generate_read_str_function(builder, module, only_declarations);
    generate_read_uint_function(builder, module, only_declarations, Type::get_primitive_type("u32"));
    generate_read_int_function(builder, module, only_declarations, Type::get_primitive_type("i32"));
    generate_read_uint_function(builder, module, only_declarations, Type::get_primitive_type("u64"));
    generate_read_int_function(builder, module, only_declarations, Type::get_primitive_type("i64"));
    generate_read_f32_function(builder, module, only_declarations);
    generate_read_f64_function(builder, module, only_declarations);
}
