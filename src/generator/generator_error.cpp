#include "generator/generator.hpp"
#include "parser/parser.hpp"

#include <functional>

static const std::string prefix = "flint.error.";

/*
 * Whenever a user is throwing an error using `throw ErrType.Value("message");` or whenever an error is automatically rethrown because an
 * error-returning function is called without a catch statement, a new stack-trace entry is appended to the trace list which is stored in
 * the thread stack (the `trace_ptr` field of the `type.ts.stack` struct type).
 *
 * The trace list itself is implemented as a doubly linked list of chunks, so that only every `Chunk::ENTRY_COUNT`-th entry (16 by default)
 * needs a new allocation. The very first chunk (chunk0) is embedded directly into the thread stack struct, meaning that the list has zero
 * startup cost (and stays malloc-free until the 17th entry). When a chunk runs full, the next chunk is reused if it already exists,
 * otherwise it is allocated. The chunks which are "behind" the current tail are simply reactivated by inheriting the `used` count of the
 * tail, which makes throw-catch loops without any stack-trace printing basically allocation-free. Chunks are never freed until the end of
 * the program.
 *
 *     typedef struct trace_entry_t {
 *         const char *file_path;        // A pointer to the name of the file path, this pointer points to a string contained in
 *                                       // the global section of the built program, meaning that it does not need to be freed
 *         const char *fn_name;          // A pointer to the name of the function this trace entry was created in, like the file
 *                                       // path it points to a string contained in the global section of the built program
 *         uint32_t line;                // The line of the file this trace entry was created from
 *         uint32_t column;              // The column of the file this trace entry was created from
 *         bool is_rethrow;              // Whether this entry belongs to a rethrown error. Every `throw` statement is considered to be a
 *                                       // rethrow if the trace list is nonempty at the moment the entry is added
 *     } trace_entry_t;
 *
 *     typedef struct trace_chunk_t {
 *         struct trace_chunk_t *prev;   // The previous chunk in the list, or NULL for the embedded chunk0
 *         uint64_t used;                // The total number of entries in this chunk and all chunks before it
 *         trace_entry_t entries[16];    // The actual trace entries of this chunk
 *         struct trace_chunk_t *next;   // The next (inactive) chunk in the list, or NULL if it has not been allocated yet
 *     } trace_chunk_t;
 */

void Generator::Error::generate_error_functions(llvm::IRBuilder<> *const builder, llvm::Module *const module) {
    generate_get_type_str_function(builder, module);
    generate_get_val_str_function(builder, module);
    generate_get_str_function(builder, module);
    generate_trace_add_function(builder, module);
    generate_trace_free_function(builder, module);
    generate_trace_print_function(builder, module);
}

void Generator::Error::generate_types() {
    if (type_map.find("type.trace.entry") == type_map.end()) {
        type_map["type.trace.entry"] = IR::create_struct_type("type.trace.entry",
            {
                PTR_TY,                          // ptr file_path
                PTR_TY,                          // ptr fn_name
                llvm::Type::getInt32Ty(context), // u32 line
                llvm::Type::getInt32Ty(context), // u32 column
                llvm::Type::getInt1Ty(context),  // u8 bool is_rethrow
            } //
        );
    }
    if (type_map.find("type.trace.chunk") == type_map.end()) {
        type_map["type.trace.chunk"] = IR::create_struct_type("type.trace.chunk",
            {
                PTR_TY,                                                                           // ptr prev
                llvm::Type::getInt64Ty(context),                                                  // u64 used
                llvm::ArrayType::get(type_map.at("type.trace.entry"), Error::Chunk::ENTRY_COUNT), // trace_entry_t entries[]
                PTR_TY,                                                                           // ptr next
            } //
        );
    }
}

llvm::Value *Generator::Error::generate_load_trace_depth(llvm::IRBuilder<> &builder, llvm::Value *const ts_ptr) {
    llvm::StructType *const ts_ty = type_map.at("type.ts.stack");
    llvm::StructType *const chunk_ty = type_map.at("type.trace.chunk");
    llvm::Value *const trace_ptr_p = builder.CreateStructGEP(ts_ty, ts_ptr, Module::ThreadStack::STACK::TRACE_PTR, "trace_ptr_gep");
    llvm::Value *const tail = IR::aligned_load(builder, PTR_TY, trace_ptr_p, "trace_tail");
    llvm::Value *const used_p = builder.CreateStructGEP(chunk_ty, tail, Error::Chunk::USED, "trace_used_gep");
    return IR::aligned_load(builder, builder.getInt64Ty(), used_p, "trace_depth");
}

void Generator::Error::generate_get_type_str_function(llvm::IRBuilder<> *const builder, llvm::Module *const module) {
    llvm::FunctionType *const get_type_str_type = llvm::FunctionType::get( //
        PTR_TY,                                                            // returns char*
        {llvm::Type::getInt32Ty(context)},                                 // Takes the type of the error
        false                                                              // No vaargs
    );
    llvm::Function *const get_type_str_fn = llvm::Function::Create( //
        get_type_str_type,                                          //
        llvm::Function::ExternalLinkage,                            //
        prefix + "get_type_str",                                    //
        module                                                      //
    );
    error_functions["get_type_str"] = get_type_str_fn;

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", get_type_str_fn);
    llvm::BasicBlock *const default_block = llvm::BasicBlock::Create(context, "default", get_type_str_fn);
    llvm::BasicBlock *const zero_block = llvm::BasicBlock::Create(context, "zero_case", get_type_str_fn);

    // Get the parameter (err_type)
    llvm::Argument *const arg_err_type = get_type_str_fn->arg_begin();
    arg_err_type->setName("err_type");

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get all errors
    const std::vector<const ErrorNode *> errors = Parser::get_all_errors();

    // Create the switch instruction (we'll add cases to it)
    // Number of cases = 1 (for zero) + number of errors
    llvm::SwitchInst *const switch_inst = builder->CreateSwitch(arg_err_type, default_block, 1 + errors.size());

    // Add case for hash 0 -> return "error"
    switch_inst->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), zero_block);
    builder->SetInsertPoint(zero_block);
    llvm::Value *const error_str = IR::generate_const_string(module, "error");
    builder->CreateRet(error_str);

    // Add cases for each error type
    for (const ErrorNode *error : errors) {
        llvm::BasicBlock *const case_block = llvm::BasicBlock::Create(context, "case_" + error->name, get_type_str_fn);
        switch_inst->addCase(builder->getInt32(error->error_id), case_block);

        builder->SetInsertPoint(case_block);
        llvm::Value *const type_str = IR::generate_const_string(module, error->name);
        builder->CreateRet(type_str);
    }

    // Default case: print error message and abort
    builder->SetInsertPoint(default_block);
    llvm::Value *const unknown_err_msg = IR::generate_const_string(module, "Unknown error type hash: %u\n");
    builder->CreateCall(c_functions.at(PRINTF), {unknown_err_msg, arg_err_type});
    builder->CreateCall(c_functions.at(FFLUSH), {llvm::ConstantPointerNull::get(PTR_TY)});
    builder->CreateCall(c_functions.at(ABORT), {});
    builder->CreateUnreachable();
}

void Generator::Error::generate_get_val_str_function(llvm::IRBuilder<> *const builder, llvm::Module *const module) {
    llvm::FunctionType *const get_val_str_type = llvm::FunctionType::get( //
        PTR_TY,                                                           // returns char*
        {
            llvm::Type::getInt32Ty(context), // Takes the error type id
            llvm::Type::getInt32Ty(context)  // Takes the value id
        },
        false // No vaargs
    );
    llvm::Function *const get_val_str_fn = llvm::Function::Create( //
        get_val_str_type,                                          //
        llvm::Function::ExternalLinkage,                           //
        prefix + "get_val_str",                                    //
        module                                                     //
    );
    error_functions["get_val_str"] = get_val_str_fn;

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", get_val_str_fn);
    llvm::BasicBlock *const default_block = llvm::BasicBlock::Create(context, "default", get_val_str_fn);
    llvm::BasicBlock *const zero_block = llvm::BasicBlock::Create(context, "zero_case", get_val_str_fn);

    // Get parameters
    llvm::Argument *const arg_err_type = get_val_str_fn->arg_begin();
    arg_err_type->setName("err_type");
    llvm::Argument *const arg_err_val = get_val_str_fn->arg_begin() + 1;
    arg_err_val->setName("err_val");

    builder->SetInsertPoint(entry_block);
    const std::vector<const ErrorNode *> errors = Parser::get_all_errors();
    llvm::SwitchInst *const switch_inst = builder->CreateSwitch(arg_err_type, default_block, 1 + errors.size());

    builder->SetInsertPoint(zero_block);
    switch_inst->addCase(builder->getInt32(0), zero_block);
    llvm::Value *const error_str = IR::generate_const_string(module, "anyerror");
    builder->CreateRet(error_str);

    // Add cases for each error type
    for (const ErrorNode *error : errors) {
        llvm::BasicBlock *const case_block = llvm::BasicBlock::Create(context, "case_" + error->name, get_val_str_fn);
        switch_inst->addCase(builder->getInt32(error->error_id), case_block);

        // Now we do a small check if the error set is inherited from a different error set. If it is then we check if the error value is
        // inside the base error's range of values. If it is then we call the same function recursively with the new hash of the inherited
        // error. If the value is bigger or equal to the number of values in the base error then we switch on it. It cannot be outside the
        // range of the values, this should not be possible so we abort in that case too
        builder->SetInsertPoint(case_block);
        const std::optional<const ErrorNode *> parent_error = error->get_parent_node();
        llvm::Value *normalized_err_val = arg_err_val;
        if (parent_error.has_value()) {
            const unsigned int value_count = parent_error.value()->get_value_count();
            llvm::Value *const parent_value_count = builder->getInt32(value_count);
            llvm::Value *const is_parent_err = builder->CreateICmpULT(arg_err_val, parent_value_count);
            llvm::BasicBlock *const is_parent_error_block = llvm::BasicBlock::Create(context, "case_" + error->name + "_is_parent_error");
            llvm::BasicBlock *const is_this_error_block = llvm::BasicBlock::Create(context, "case_" + error->name + "_is_this_error");
            builder->CreateCondBr(is_parent_err, is_parent_error_block, is_this_error_block);
            is_parent_error_block->insertInto(get_val_str_fn);
            is_this_error_block->insertInto(get_val_str_fn);

            // Recrusively call itself with the error id of the parent error and the error value forwarded as-is
            builder->SetInsertPoint(is_parent_error_block);
            llvm::Value *const new_parent_id = builder->getInt32(parent_error.value()->error_id);
            llvm::Value *const value_from_parent = builder->CreateCall(           //
                get_val_str_fn, {new_parent_id, arg_err_val}, "value_from_parent" //
            );
            builder->CreateRet(value_from_parent);

            // Continue with normal flow, but we need to substract the number of values in the parent from the error value to make the
            // switch below able to work
            builder->SetInsertPoint(is_this_error_block);
            normalized_err_val = builder->CreateSub(arg_err_val, parent_value_count, "normalized_err_val");
        }

        // We switch on the normalized error value and depending on it's value we return one of the possible error values, so we need to
        // prepare a switch for all possible values
        llvm::BasicBlock *const default_value_block = llvm::BasicBlock::Create(context, "case_" + error->name + "_default");
        llvm::SwitchInst *const value_switch_inst = builder->CreateSwitch(normalized_err_val, default_value_block, error->values.size());

        for (size_t i = 0; i < error->values.size(); i++) {
            const std::string &value = error->values.at(i);
            llvm::BasicBlock *const value_case_block = llvm::BasicBlock::Create(              //
                context, "case_" + error->name + "_case_" + std::to_string(i), get_val_str_fn //
            );
            builder->SetInsertPoint(value_case_block);
            value_switch_inst->addCase(builder->getInt32(i), value_case_block);
            builder->CreateRet(IR::generate_const_string(module, value));
        }

        default_value_block->insertInto(get_val_str_fn);
        builder->SetInsertPoint(default_value_block);
        llvm::Value *const unknown_err_msg = IR::generate_const_string(module, "Unknown error value '%u' on error id '%u'\n");
        builder->CreateCall(c_functions.at(PRINTF), {unknown_err_msg, arg_err_val, arg_err_type});
        builder->CreateCall(c_functions.at(FFLUSH), {llvm::ConstantPointerNull::get(PTR_TY)});
        builder->CreateCall(c_functions.at(ABORT), {});
        builder->CreateUnreachable();
    }

    // Default case: print error message and abort
    builder->SetInsertPoint(default_block);
    llvm::Value *const unknown_err_msg = IR::generate_const_string(module, "Unknown error type hash: %u\n");
    builder->CreateCall(c_functions.at(PRINTF), {unknown_err_msg, arg_err_type});
    builder->CreateCall(c_functions.at(FFLUSH), {llvm::ConstantPointerNull::get(PTR_TY)});
    builder->CreateCall(c_functions.at(ABORT), {});
    builder->CreateUnreachable();
}

void Generator::Error::generate_get_str_function(llvm::IRBuilder<> *const builder, llvm::Module *const module) {
    llvm::Function *const strlen_fn = c_functions.at(STRLEN);
    llvm::Function *const memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *const get_type_str_fn = error_functions.at("get_type_str");
    llvm::Function *const get_val_str_fn = error_functions.at("get_val_str");
    llvm::Function *const create_str_fn = Module::String::string_manip_functions.at("create_str");

    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;
    llvm::StructType *const error_type = type_map.at("type.flint.err");
    llvm::FunctionType *const get_str_type = llvm::FunctionType::get( //
        PTR_TY,                                                       // returns str*
        {error_type},                                                 // Takes the error to create a string from
        false                                                         // No vaargs
    );
    llvm::Function *const get_str_fn = llvm::Function::Create( //
        get_str_type,                                          //
        llvm::Function::ExternalLinkage,                       //
        prefix + "get_str",                                    //
        module                                                 //
    );
    error_functions["get_str"] = get_str_fn;

    // Create basic blocks
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", get_str_fn);

    // Get the parameters
    llvm::Argument *const arg_err = get_str_fn->arg_begin();
    arg_err->setName("err");

    builder->SetInsertPoint(entry_block);
    llvm::Value *const err_type_id = builder->CreateExtractValue(arg_err, 0, "err_type_id");
    llvm::Value *const err_value_id = builder->CreateExtractValue(arg_err, 1, "err_value_id");
    llvm::Value *const err_type_str = builder->CreateCall(get_type_str_fn, {err_type_id}, "err_type_str");
    llvm::Value *const err_val_str = builder->CreateCall(get_val_str_fn, {err_type_id, err_value_id}, "err_val_str");
    llvm::Value *const err_type_str_len = builder->CreateCall(strlen_fn, {err_type_str}, "err_type_str_len");
    llvm::Value *const err_val_str_len = builder->CreateCall(strlen_fn, {err_val_str}, "err_val_str_len");
    llvm::Value *const err_str_len = builder->CreateAdd(       //
        builder->CreateAdd(err_type_str_len, err_val_str_len), //
        builder->getInt64(1),                                  //
        "err_str_len"                                          //
    );
    llvm::Value *const err_str = builder->CreateCall(create_str_fn, {err_str_len}, "err_str");
    llvm::Value *const err_str_type_ptr = builder->CreateStructGEP(str_type, err_str, 1, "err_str_type_ptr");
    builder->CreateCall(memcpy_fn, {err_str_type_ptr, err_type_str, err_type_str_len});
    llvm::Value *const dot_ptr = builder->CreateGEP(builder->getInt8Ty(), err_str_type_ptr, err_type_str_len, "dot_ptr");
    IR::aligned_store(*builder, builder->getInt8('.'), dot_ptr);
    llvm::Value *const err_str_val_ptr = builder->CreateGEP(builder->getInt8Ty(), dot_ptr, builder->getInt32(1));
    builder->CreateCall(memcpy_fn, {err_str_val_ptr, err_val_str, err_val_str_len});
    builder->CreateRet(err_str);
}

void Generator::Error::generate_trace_add_function(llvm::IRBuilder<> *const builder, llvm::Module *const module) {
    // THE C IMPLEMENTATION
    // void trace_add(            //
    //     thread_stack_t *ts,    //
    //     const char *file_path, //
    //     const char *fn_name,   //
    //     const uint32_t line,   //
    //     const uint32_t column  //
    // ) {
    //     trace_chunk_t *tail = ts->trace_ptr;
    //     const uint64_t used = tail->used;
    //     const bool is_rethrow = used != 0;
    //     const uint64_t base = tail->prev != NULL ? tail->prev->used : 0;
    //     uint64_t local_used = used - base;
    //     if (local_used == CHUNK_ENTRY_COUNT) {
    //         if (tail->next == NULL) {
    //             tail->next = malloc(sizeof(trace_chunk_t));
    //             tail->next->prev = tail;
    //             tail->next->next = NULL;
    //         }
    //         tail = tail->next;
    //         local_used = 0;
    //     }
    //     ts->trace_ptr = tail;
    //     tail->used = used + 1;
    //     tail->entries[local_used] = (trace_entry_t){
    //         .file_path = file_path,
    //         .fn_name = fn_name,
    //         .line = line,
    //         .column = column,
    //         .is_rethrow = is_rethrow,
    //     };
    // }
    llvm::StructType *const ts_ty = type_map.at("type.ts.stack");
    llvm::StructType *const chunk_ty = type_map.at("type.trace.chunk");
    llvm::StructType *const entry_ty = type_map.at("type.trace.entry");
    llvm::ArrayType *const entries_array_ty = llvm::ArrayType::get(entry_ty, Error::Chunk::ENTRY_COUNT);
    llvm::Type *const i64_ty = llvm::Type::getInt64Ty(context);

    llvm::FunctionType *const trace_add_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                                 // returns void
        {
            PTR_TY,                          // ptr ts
            PTR_TY,                          // ptr file_path
            PTR_TY,                          // ptr fn_name
            llvm::Type::getInt32Ty(context), // u32 line
            llvm::Type::getInt32Ty(context), // u32 column
        },
        false // No vaargs
    );
    llvm::Function *const trace_add_fn = llvm::Function::Create( //
        trace_add_type,                                          //
        llvm::Function::ExternalLinkage,                         //
        prefix + "trace_add",                                    //
        module                                                   //
    );
    error_functions["trace_add"] = trace_add_fn;

    llvm::Argument *const arg_ts = trace_add_fn->arg_begin();
    arg_ts->setName("ts");
    llvm::Argument *const arg_file_path = trace_add_fn->arg_begin() + 1;
    arg_file_path->setName("file_path");
    llvm::Argument *const arg_fn_name = trace_add_fn->arg_begin() + 2;
    arg_fn_name->setName("fn_name");
    llvm::Argument *const arg_line = trace_add_fn->arg_begin() + 3;
    arg_line->setName("line");
    llvm::Argument *const arg_column = trace_add_fn->arg_begin() + 4;
    arg_column->setName("column");

    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", trace_add_fn);
    llvm::BasicBlock *const has_prev_block = llvm::BasicBlock::Create(context, "has_prev", trace_add_fn);
    llvm::BasicBlock *const no_prev_block = llvm::BasicBlock::Create(context, "no_prev", trace_add_fn);
    llvm::BasicBlock *const base_merge_block = llvm::BasicBlock::Create(context, "base_merge", trace_add_fn);
    llvm::BasicBlock *const rollover_block = llvm::BasicBlock::Create(context, "rollover", trace_add_fn);
    llvm::BasicBlock *const alloc_block = llvm::BasicBlock::Create(context, "alloc_chunk", trace_add_fn);
    llvm::BasicBlock *const reuse_block = llvm::BasicBlock::Create(context, "reuse_chunk", trace_add_fn);
    llvm::BasicBlock *const write_block = llvm::BasicBlock::Create(context, "write", trace_add_fn);
    llvm::BasicBlock *const ret_block = llvm::BasicBlock::Create(context, "ret", trace_add_fn);

    builder->SetInsertPoint(entry_block);
    llvm::Value *const trace_ptr_gep = builder->CreateStructGEP(ts_ty, arg_ts, Module::ThreadStack::STACK::TRACE_PTR, "trace_ptr_gep");
    llvm::Value *const tail = IR::aligned_load(*builder, PTR_TY, trace_ptr_gep, "trace_tail");
    llvm::Value *const tail_used_gep = builder->CreateStructGEP(chunk_ty, tail, Error::Chunk::USED, "tail_used_gep");
    llvm::Value *const tail_used = IR::aligned_load(*builder, i64_ty, tail_used_gep, "tail_used");
    llvm::Value *const is_rethrow = builder->CreateICmpNE(tail_used, builder->getInt64(0), "is_rethrow");
    llvm::Value *const tail_prev_gep = builder->CreateStructGEP(chunk_ty, tail, Error::Chunk::PREV, "tail_prev_gep");
    llvm::Value *const tail_prev = IR::aligned_load(*builder, PTR_TY, tail_prev_gep, "tail_prev");
    llvm::Value *const has_tail_prev = builder->CreateICmpNE(tail_prev, llvm::ConstantPointerNull::get(PTR_TY), "has_tail_prev");
    builder->CreateCondBr(has_tail_prev, has_prev_block, no_prev_block);

    builder->SetInsertPoint(has_prev_block);
    llvm::Value *const tail_prev_used_gep = builder->CreateStructGEP(chunk_ty, tail_prev, Error::Chunk::USED, "tail_prev_used_gep");
    llvm::Value *const tail_prev_used = IR::aligned_load(*builder, i64_ty, tail_prev_used_gep, "tail_prev_used");
    builder->CreateBr(base_merge_block);

    builder->SetInsertPoint(no_prev_block);
    builder->CreateBr(base_merge_block);

    builder->SetInsertPoint(base_merge_block);
    llvm::PHINode *const tail_base = builder->CreatePHI(i64_ty, 2, "tail_base");
    tail_base->addIncoming(tail_prev_used, has_prev_block);
    tail_base->addIncoming(builder->getInt64(0), no_prev_block);
    llvm::Value *const local_used = builder->CreateSub(tail_used, tail_base, "local_used");
    llvm::Value *const is_full = builder->CreateICmpEQ(local_used, builder->getInt64(Error::Chunk::ENTRY_COUNT), "is_full");
    builder->CreateCondBr(is_full, rollover_block, write_block);

    builder->SetInsertPoint(rollover_block);
    llvm::Value *const tail_next_gep = builder->CreateStructGEP(chunk_ty, tail, Error::Chunk::NEXT, "tail_next_gep");
    llvm::Value *const tail_next = IR::aligned_load(*builder, PTR_TY, tail_next_gep, "tail_next");
    llvm::Value *const has_next = builder->CreateICmpNE(tail_next, llvm::ConstantPointerNull::get(PTR_TY), "has_next");
    builder->CreateCondBr(has_next, reuse_block, alloc_block);

    builder->SetInsertPoint(alloc_block);
    llvm::Value *const chunk_size = builder->getInt64(Allocation::get_type_size(module, chunk_ty));
    llvm::Value *const new_chunk = builder->CreateCall(c_functions.at(MALLOC), {chunk_size}, "new_chunk");
    llvm::Value *const new_chunk_prev_gep = builder->CreateStructGEP(chunk_ty, new_chunk, Error::Chunk::PREV, "new_chunk_prev_gep");
    IR::aligned_store(*builder, tail, new_chunk_prev_gep);
    llvm::Value *const new_chunk_next_gep = builder->CreateStructGEP(chunk_ty, new_chunk, Error::Chunk::NEXT, "new_chunk_next_gep");
    IR::aligned_store(*builder, llvm::ConstantPointerNull::get(PTR_TY), new_chunk_next_gep);
    IR::aligned_store(*builder, new_chunk, tail_next_gep);
    builder->CreateBr(write_block);

    builder->SetInsertPoint(reuse_block);
    builder->CreateBr(write_block);

    builder->SetInsertPoint(write_block);
    llvm::PHINode *const write_tail_phi = builder->CreatePHI(PTR_TY, 3, "write_tail");
    write_tail_phi->addIncoming(tail, base_merge_block);
    write_tail_phi->addIncoming(new_chunk, alloc_block);
    write_tail_phi->addIncoming(tail_next, reuse_block);
    llvm::PHINode *const write_idx_phi = builder->CreatePHI(i64_ty, 3, "write_idx");
    write_idx_phi->addIncoming(local_used, base_merge_block);
    write_idx_phi->addIncoming(builder->getInt64(0), alloc_block);
    write_idx_phi->addIncoming(builder->getInt64(0), reuse_block);
    IR::aligned_store(*builder, write_tail_phi, trace_ptr_gep);
    llvm::Value *const write_tail_used_gep = builder->CreateStructGEP(chunk_ty, write_tail_phi, Error::Chunk::USED, "write_tail_used_gep");
    llvm::Value *const new_used = builder->CreateAdd(tail_used, builder->getInt64(1), "new_used");
    IR::aligned_store(*builder, new_used, write_tail_used_gep);
    llvm::Value *const entries_gep = builder->CreateStructGEP(chunk_ty, write_tail_phi, Error::Chunk::ENTRIES, "entries_gep");
    llvm::Value *const entry_ptr = builder->CreateGEP(entries_array_ty, entries_gep, {builder->getInt32(0), write_idx_phi}, "entry_ptr");
    llvm::Value *const entry_path_gep = builder->CreateStructGEP(entry_ty, entry_ptr, Trace::FILE_PATH, "entry_path_gep");
    IR::aligned_store(*builder, arg_file_path, entry_path_gep);
    llvm::Value *const entry_fn_gep = builder->CreateStructGEP(entry_ty, entry_ptr, Trace::FN_NAME, "entry_fn_gep");
    IR::aligned_store(*builder, arg_fn_name, entry_fn_gep);
    llvm::Value *const entry_line_gep = builder->CreateStructGEP(entry_ty, entry_ptr, Trace::LINE, "entry_line_gep");
    IR::aligned_store(*builder, arg_line, entry_line_gep);
    llvm::Value *const entry_column_gep = builder->CreateStructGEP(entry_ty, entry_ptr, Trace::COLUMN, "entry_column_gep");
    IR::aligned_store(*builder, arg_column, entry_column_gep);
    llvm::Value *const entry_rethrow_gep = builder->CreateStructGEP(entry_ty, entry_ptr, Trace::IS_RETHROW, "entry_rethrow_gep");
    IR::aligned_store(*builder, is_rethrow, entry_rethrow_gep);
    builder->CreateBr(ret_block);

    builder->SetInsertPoint(ret_block);
    builder->CreateRetVoid();
}

void Generator::Error::generate_trace_free_function(llvm::IRBuilder<> *const builder, llvm::Module *const module) {
    // THE C IMPLEMENTATION:
    // void trace_free(thread_stack_t *ts, const uint64_t depth) {
    //     trace_chunk_t *chunk = ts->trace_ptr;
    //     while (chunk->prev != NULL && depth < chunk->prev->used) {
    //         chunk = chunk->prev;
    //     }
    //     chunk->used = depth;
    //     ts->trace_ptr = chunk;
    // }
    llvm::StructType *const ts_ty = type_map.at("type.ts.stack");
    llvm::StructType *const chunk_ty = type_map.at("type.trace.chunk");
    llvm::Type *const i64 = llvm::Type::getInt64Ty(context);

    llvm::FunctionType *const trace_free_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                                  // returns void
        {PTR_TY, i64},                                                   // ts, depth
        false                                                            // No vaargs
    );
    llvm::Function *const fn = llvm::Function::Create( //
        trace_free_type,                               //
        llvm::Function::ExternalLinkage,               //
        prefix + "trace_free",                         //
        module                                         //
    );
    error_functions["trace_free"] = fn;

    llvm::Argument *const arg_ts = fn->arg_begin();
    arg_ts->setName("ts");
    llvm::Argument *const arg_depth = fn->arg_begin() + 1;
    arg_depth->setName("depth");

    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", fn);
    llvm::BasicBlock *const loop_block = llvm::BasicBlock::Create(context, "loop", fn);
    llvm::BasicBlock *const below_prev_block = llvm::BasicBlock::Create(context, "below_prev", fn);
    llvm::BasicBlock *const done_block = llvm::BasicBlock::Create(context, "done", fn);

    builder->SetInsertPoint(entry_block);
    llvm::Value *const ts_trace_ptr = builder->CreateStructGEP(ts_ty, arg_ts, Module::ThreadStack::STACK::TRACE_PTR, "ts_trace_ptr");
    llvm::Value *const trace_tail = IR::aligned_load(*builder, PTR_TY, ts_trace_ptr, "trace_tail");
    builder->CreateBr(loop_block);

    builder->SetInsertPoint(loop_block);
    llvm::PHINode *const chunk = builder->CreatePHI(PTR_TY, 2, "chunk");
    chunk->addIncoming(trace_tail, entry_block);
    llvm::Value *const prev_gep = builder->CreateStructGEP(chunk_ty, chunk, Error::Chunk::PREV, "prev_gep");
    llvm::Value *const prev_chunk = IR::aligned_load(*builder, PTR_TY, prev_gep, "prev_chunk");
    llvm::Value *const has_prev = builder->CreateICmpNE(prev_chunk, llvm::ConstantPointerNull::get(PTR_TY), "has_prev");
    builder->CreateCondBr(has_prev, below_prev_block, done_block);

    builder->SetInsertPoint(below_prev_block);
    llvm::Value *const prev_used_ptr = builder->CreateStructGEP(chunk_ty, prev_chunk, Error::Chunk::USED, "prev_used_ptr");
    llvm::Value *const prev_used = IR::aligned_load(*builder, i64, prev_used_ptr, "prev_used");
    llvm::Value *const is_below_prev = builder->CreateICmpULT(arg_depth, prev_used, "is_below_prev");
    builder->CreateCondBr(is_below_prev, loop_block, done_block);
    chunk->addIncoming(prev_chunk, below_prev_block);

    builder->SetInsertPoint(done_block);
    llvm::Value *const done_used_gep = builder->CreateStructGEP(chunk_ty, chunk, Error::Chunk::USED, "done_used_gep");
    IR::aligned_store(*builder, arg_depth, done_used_gep);
    IR::aligned_store(*builder, chunk, ts_trace_ptr);
    builder->CreateRetVoid();
}

void Generator::Error::generate_trace_print_function(llvm::IRBuilder<> *const builder, llvm::Module *const module) {
    // THE C IMPLEMENTATION:
    // inline uint64_t digit_count(const uint32_t value) {
    //     uint64_t count = 1;
    //     for (uint64_t threshold = 10; threshold <= 1000000000ULL; threshold *= 10) {
    //         if (value >= threshold) {
    //             ++count;
    //         }
    //     }
    //     return count;
    // }
    //
    // inline void print_dashes(const uint64_t count) {
    //     for (uint64_t i = 0; i < count; ++i) {
    //         printf("─");
    //     }
    // }
    //
    // void trace_print(thread_stack_t *ts, const uint32_t type_id, const uint32_t value_id, const str *msg) {
    //     printf("Runtime Error: %s.%s\n", get_type_str(type_id), get_val_str(type_id, value_id));
    //     printf("  │ » %s\n", msg->value);
    //     trace_chunk_t *tail = ts->trace_ptr;
    //     const uint64_t used = tail->used;
    //     if (used == 0) {
    //         fflush(stdout);
    //         return;
    //     }
    //
    //     // Measure the widest `fn_name` and `path:line:column` strings
    //     uint64_t name_max = 0
    //     uint64_t loc_max = 0;
    //     trace_chunk_t *chunk = tail;
    //     while (chunk->prev != NULL) {
    //         chunk = chunk->prev;
    //     }
    //     for (; ; chunk = chunk->next) {
    //         const uint64_t local = (chunk == tail)
    //             ? used - (tail->prev != NULL ? tail->prev->used : 0)
    //             : CHUNK_ENTRY_COUNT;
    //         for (uint64_t i = 0; i < local; ++i) {
    //             const trace_entry_t *entry = &chunk->entries[i];
    //             const uint64_t fn_len = strlen(entry->fn_name);
    //             if (fn_len > name_max) {
    //                 name_max = fn_len;
    //             }
    //             const uint64_t loc_len = strlen(entry->file_path) + digit_count(entry->line) + 1 + digit_count(entry->column) + 1;
    //             if (loc_len > loc_max) {
    //                 loc_max = loc_len;
    //             }
    //         }
    //         if (chunk == tail) {
    //             break;
    //         }
    //     }
    //     // Column widths: at least 8/12 and rounded up to an even number, so the header dash runs are integers
    //     uint64_t name_width = name_max > 8 ? name_max : 8;
    //     if (name_width & 1) {
    //         ++name_width;
    //     }
    //     uint64_t loc_width = loc_max > 12 ? loc_max : 12;
    //     if (loc_width & 1) {
    //         ++loc_width;
    //     }
    //     // ' Name ' is 6 characters
    //     const uint64_t name_dashes = (name_width - 6) / 2;
    //     // ' Location ' is 10 characters
    //     const uint64_t loc_dashes = (loc_width - 10) / 2;
    //     printf("  └─┬─");
    //     print_dashes(name_dashes);
    //     printf(" Name ");
    //     print_dashes(name_dashes);
    //     printf("─┬─");
    //     print_dashes(loc_dashes);
    //     printf(" Location ");
    //     print_dashes(loc_dashes);
    //     printf("─┬─ Info ──┐\n");
    //
    //     // Second walk: print every entry, the first is the origin, every other a rethrow and the last closes the box
    //     chunk = tail;
    //     while (chunk->prev != NULL) {
    //         chunk = chunk->prev;
    //     }
    //     for (; ; chunk = chunk->next) {
    //         const uint64_t local = (chunk == tail)
    //             ? used - (tail->prev != NULL ? tail->prev->used : 0)
    //             : CHUNK_ENTRY_COUNT;
    //         for (uint64_t i = 0; i < local; ++i) {
    //             const trace_entry_t *entry = &chunk->entries[i];
    //             const bool is_last = chunk == tail && i + 1 == local;
    //             const uint64_t loc_len = strlen(entry->file_path) + digit_count(entry->line) + 1 + digit_count(entry->column) + 1;
    //             const uint64_t pad = loc_width - loc_len;
    //             const char *prefix = is_last ? "└ " : "├ ";
    //             const char *info = is_last ? "" : (entry->is_rethrow ? "rethrow" : "origin");
    //             const char *right_bar = is_last ? "┘" : "┤";
    //             printf("    %s%-*s   %s:%u:%u%*s   %-*s%s\n", //
    //                 prefix,                                   //
    //                 (int)name_width,                          //
    //                 entry->fn_name,                           //
    //                 entry->file_path,                         //
    //                 entry->line,                              //
    //                 entry->column,                            //
    //                 (int)pad,                                 //
    //                 "",                                       //
    //                 8,                                        //
    //                 info,                                     //
    //                 right_bar                                 //
    //             );
    //         }
    //         if (chunk == tail) break;
    //     }
    //     fflush(stdout);
    // }
    llvm::Function *const strlen_fn = c_functions.at(STRLEN);
    llvm::Function *const printf_fn = c_functions.at(PRINTF);
    llvm::Function *const fflush_fn = c_functions.at(FFLUSH);
    llvm::Function *const get_type_str_fn = error_functions.at("get_type_str");
    llvm::Function *const get_val_str_fn = error_functions.at("get_val_str");
    llvm::Type *const i64 = llvm::Type::getInt64Ty(context);
    llvm::Type *const i32 = llvm::Type::getInt32Ty(context);
    llvm::StructType *const ts_ty = type_map.at("type.ts.stack");
    llvm::StructType *const chunk_ty = type_map.at("type.trace.chunk");
    llvm::StructType *const entry_ty = type_map.at("type.trace.entry");
    llvm::ArrayType *const entries_array_ty = llvm::ArrayType::get(entry_ty, Error::Chunk::ENTRY_COUNT);
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).type;

    llvm::Value *const header_fmt = IR::generate_const_string(module, "Runtime Error: %s.%s\n");
    llvm::Value *const message_fmt = IR::generate_const_string(module, "  │ » %s\n");
    llvm::Value *const box_top_fmt = IR::generate_const_string(module, "  └─┬─");
    llvm::Value *const name_label_fmt = IR::generate_const_string(module, " Name ");
    llvm::Value *const box_mid_fmt = IR::generate_const_string(module, "─┬─");
    llvm::Value *const location_label_fmt = IR::generate_const_string(module, " Location ");
    llvm::Value *const box_tail_fmt = IR::generate_const_string(module, " Info ──┐\n");
    llvm::Value *const dash_fmt = IR::generate_const_string(module, "─");
    llvm::Value *const entry_fmt = IR::generate_const_string(module, "    %s%-*s   %s:%u:%u%*s   %-*s%s\n");
    llvm::Value *const prefix_last = IR::generate_const_string(module, "└ ");
    llvm::Value *const prefix_normal = IR::generate_const_string(module, "├ ");
    llvm::Value *const info_origin = IR::generate_const_string(module, "origin");
    llvm::Value *const info_rethrow = IR::generate_const_string(module, "rethrow");
    llvm::Value *const empty_str = IR::generate_const_string(module, "");
    llvm::Value *const box_right_mid = IR::generate_const_string(module, "┤");
    llvm::Value *const box_right_last = IR::generate_const_string(module, "┘");

    llvm::FunctionType *const trace_print_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                                   // returns void
        {PTR_TY, i32, i32, PTR_TY},                                       // ts, type_id, value_id, msg_str
        false                                                             // No vaargs
    );
    llvm::Function *const trace_print_fn = llvm::Function::Create( //
        trace_print_type,                                          //
        llvm::Function::ExternalLinkage,                           //
        prefix + "trace_print",                                    //
        module                                                     //
    );
    error_functions["trace_print"] = trace_print_fn;

    llvm::Argument *const arg_ts = trace_print_fn->arg_begin();
    arg_ts->setName("ts");
    llvm::Argument *const arg_type_id = trace_print_fn->arg_begin() + 1;
    arg_type_id->setName("type_id");
    llvm::Argument *const arg_value_id = trace_print_fn->arg_begin() + 2;
    arg_value_id->setName("value_id");
    llvm::Argument *const arg_msg_str = trace_print_fn->arg_begin() + 3;
    arg_msg_str->setName("msg_str");

    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", trace_print_fn);
    llvm::BasicBlock *const width_block = llvm::BasicBlock::Create(context, "width", trace_print_fn);
    llvm::BasicBlock *const done_block = llvm::BasicBlock::Create(context, "done", trace_print_fn);

    builder->SetInsertPoint(entry_block);
    llvm::Value *const type_str = builder->CreateCall(get_type_str_fn, {arg_type_id}, "type_str");
    llvm::Value *const val_str = builder->CreateCall(get_val_str_fn, {arg_type_id, arg_value_id}, "val_str");
    builder->CreateCall(printf_fn, {header_fmt, type_str, val_str});
    llvm::Value *const msg_data = builder->CreateStructGEP(str_type, arg_msg_str, 1, "msg_data");
    builder->CreateCall(printf_fn, {message_fmt, msg_data});
    llvm::Value *const trace_ptr_gep = builder->CreateStructGEP(ts_ty, arg_ts, Module::ThreadStack::STACK::TRACE_PTR, "trace_ptr_gep");
    llvm::Value *const tail = IR::aligned_load(*builder, PTR_TY, trace_ptr_gep, "trace_tail");
    llvm::Value *const tail_used_gep = builder->CreateStructGEP(chunk_ty, tail, Error::Chunk::USED, "tail_used_gep");
    llvm::Value *const used = IR::aligned_load(*builder, i64, tail_used_gep, "trace_used");
    llvm::Value *const is_empty = builder->CreateICmpEQ(used, builder->getInt64(0), "is_empty");
    llvm::Value *const name_max_alloca = builder->CreateAlloca(i64, nullptr, "name_max");
    IR::aligned_store(*builder, builder->getInt64(0), name_max_alloca);
    llvm::Value *const loc_max_alloca = builder->CreateAlloca(i64, nullptr, "loc_max");
    IR::aligned_store(*builder, builder->getInt64(0), loc_max_alloca);

    const auto digit_count = [&](llvm::Value *const value) -> llvm::Value * {
        llvm::Value *count = builder->getInt64(1);
        uint32_t threshold = 10;
        for (unsigned digits = 2; digits <= 10; ++digits, threshold *= 10) {
            llvm::Value *const is_ge = builder->CreateICmpUGE(value, builder->getInt32(threshold), "is_digit_ge");
            count = builder->CreateSelect(is_ge, builder->getInt64(digits), count, "digits");
        }
        return count;
    };

    const auto emit_dash_loop = [&](llvm::Value *const count, const llvm::Twine &stage) -> void {
        llvm::BasicBlock *const dash_init_block = llvm::BasicBlock::Create(context, stage + "init", trace_print_fn);
        llvm::BasicBlock *const dash_loop_block = llvm::BasicBlock::Create(context, stage + "loop", trace_print_fn);
        llvm::BasicBlock *const dash_body_block = llvm::BasicBlock::Create(context, stage + "body", trace_print_fn);
        llvm::BasicBlock *const dash_done_block = llvm::BasicBlock::Create(context, stage + "done", trace_print_fn);
        builder->CreateBr(dash_init_block);
        builder->SetInsertPoint(dash_init_block);
        builder->CreateBr(dash_loop_block);
        builder->SetInsertPoint(dash_loop_block);
        llvm::PHINode *const dash_index = builder->CreatePHI(i64, 2, stage + "dash_index");
        dash_index->addIncoming(builder->getInt64(0), dash_init_block);
        llvm::Value *const dash_more = builder->CreateICmpULT(dash_index, count, "dash_more");
        builder->CreateCondBr(dash_more, dash_body_block, dash_done_block);
        builder->SetInsertPoint(dash_body_block);
        builder->CreateCall(printf_fn, {dash_fmt});
        dash_index->addIncoming(builder->CreateAdd(dash_index, builder->getInt64(1), "next_dash"), dash_body_block);
        builder->CreateBr(dash_loop_block);
        builder->SetInsertPoint(dash_done_block);
    };

    // Everything a walk body needs to know about the entry it is invoked for
    struct TraceEntryContext {
        llvm::Value *chunk;         // the chunk the entry is contained in
        llvm::Value *local_count;   // the number of entries of that chunk
        llvm::Value *is_tail_chunk; // whether that chunk is the tail chunk
        llvm::Value *entry_ptr;     // pointer to the trace entry itself
        llvm::Value *next_index;    // the entry index plus one
    };

    // Emits the whole chunk walk machinery, invoking `body` for every used entry of every chunk, walking from chunk0 to the tail.
    // All labels are prefixed with `stage` so that the two passes of this function result in distinct block names. The walk is
    // entered through the returned block and terminates by branching to `after_block`
    const auto emit_trace_walk = [&](const std::string &stage, llvm::BasicBlock *const after_block,
                                     const std::function<void(const TraceEntryContext &)> &body) -> llvm::BasicBlock * {
        llvm::BasicBlock *const walk_block = llvm::BasicBlock::Create(context, stage + "walk", trace_print_fn);
        llvm::BasicBlock *const walk_loop_block = llvm::BasicBlock::Create(context, stage + "walk_loop", trace_print_fn);
        llvm::BasicBlock *const walk_prev_block = llvm::BasicBlock::Create(context, stage + "walk_prev", trace_print_fn);
        llvm::BasicBlock *const outer_loop_block = llvm::BasicBlock::Create(context, stage + "outer_loop", trace_print_fn);
        llvm::BasicBlock *const tail_base_block = llvm::BasicBlock::Create(context, stage + "tail_base", trace_print_fn);
        llvm::BasicBlock *const tail_prev_base_block = llvm::BasicBlock::Create(context, stage + "tail_prev_base", trace_print_fn);
        llvm::BasicBlock *const tail_zero_base_block = llvm::BasicBlock::Create(context, stage + "tail_zero_base", trace_print_fn);
        llvm::BasicBlock *const local_merge_block = llvm::BasicBlock::Create(context, stage + "local_merge", trace_print_fn);
        llvm::BasicBlock *const full_local_block = llvm::BasicBlock::Create(context, stage + "full_local", trace_print_fn);
        llvm::BasicBlock *const local_block = llvm::BasicBlock::Create(context, stage + "local", trace_print_fn);
        llvm::BasicBlock *const inner_loop_init_block = llvm::BasicBlock::Create(context, stage + "inner_loop_init", trace_print_fn);
        llvm::BasicBlock *const inner_loop_block = llvm::BasicBlock::Create(context, stage + "inner_loop", trace_print_fn);
        llvm::BasicBlock *const inner_body_block = llvm::BasicBlock::Create(context, stage + "inner_body", trace_print_fn);
        llvm::BasicBlock *const inner_done_block = llvm::BasicBlock::Create(context, stage + "inner_done", trace_print_fn);
        llvm::BasicBlock *const outer_continue_block = llvm::BasicBlock::Create(context, stage + "outer_continue", trace_print_fn);

        builder->SetInsertPoint(walk_block);
        builder->CreateBr(walk_loop_block);

        builder->SetInsertPoint(walk_loop_block);
        llvm::PHINode *const chunk_walk_phi = builder->CreatePHI(PTR_TY, 2, stage + "walk_chunk");
        chunk_walk_phi->addIncoming(tail, walk_block);
        llvm::Value *const walk_prev_gep = builder->CreateStructGEP(chunk_ty, chunk_walk_phi, Error::Chunk::PREV, "walk_prev_gep");
        llvm::Value *const walk_prev = IR::aligned_load(*builder, PTR_TY, walk_prev_gep, "walk_prev");
        llvm::Value *const has_walk_prev = builder->CreateICmpNE(walk_prev, llvm::ConstantPointerNull::get(PTR_TY), "has_walk_prev");
        builder->CreateCondBr(has_walk_prev, walk_prev_block, outer_loop_block);
        chunk_walk_phi->addIncoming(walk_prev, walk_prev_block);

        builder->SetInsertPoint(walk_prev_block);
        builder->CreateBr(walk_loop_block);

        builder->SetInsertPoint(outer_loop_block);
        llvm::PHINode *const chunk_outer_phi = builder->CreatePHI(PTR_TY, 2, stage + "outer_chunk");
        chunk_outer_phi->addIncoming(chunk_walk_phi, walk_loop_block);
        llvm::Value *const is_tail_chunk = builder->CreateICmpEQ(chunk_outer_phi, tail, "is_tail_chunk");
        builder->CreateCondBr(is_tail_chunk, tail_base_block, full_local_block);

        builder->SetInsertPoint(tail_base_block);
        llvm::Value *const tail_prev_gep = builder->CreateStructGEP(chunk_ty, chunk_outer_phi, Error::Chunk::PREV, "tail_prev_gep");
        llvm::Value *const tail_prev = IR::aligned_load(*builder, PTR_TY, tail_prev_gep, "tail_prev");
        llvm::Value *const has_tail_prev = builder->CreateICmpNE(tail_prev, llvm::ConstantPointerNull::get(PTR_TY), "has_tail_prev");
        builder->CreateCondBr(has_tail_prev, tail_prev_base_block, tail_zero_base_block);

        builder->SetInsertPoint(tail_prev_base_block);
        llvm::Value *const tail_prev_used_gep = builder->CreateStructGEP(chunk_ty, tail_prev, Error::Chunk::USED, "tail_prev_used_gep");
        llvm::Value *const tail_prev_used = IR::aligned_load(*builder, i64, tail_prev_used_gep, "tail_prev_used");
        builder->CreateBr(local_merge_block);

        builder->SetInsertPoint(tail_zero_base_block);
        builder->CreateBr(local_merge_block);

        builder->SetInsertPoint(local_merge_block);
        llvm::PHINode *const base_phi = builder->CreatePHI(i64, 2, stage + "tail_base");
        base_phi->addIncoming(tail_prev_used, tail_prev_base_block);
        base_phi->addIncoming(builder->getInt64(0), tail_zero_base_block);
        llvm::Value *const local_count = builder->CreateSub(used, base_phi, stage + "local_count");
        builder->CreateBr(local_block);

        builder->SetInsertPoint(full_local_block);
        builder->CreateBr(local_block);

        builder->SetInsertPoint(local_block);
        llvm::PHINode *const local_phi = builder->CreatePHI(i64, 2, stage + "local");
        local_phi->addIncoming(local_count, local_merge_block);
        local_phi->addIncoming(builder->getInt64(Error::Chunk::ENTRY_COUNT), full_local_block);
        builder->CreateBr(inner_loop_init_block);

        builder->SetInsertPoint(inner_loop_init_block);
        builder->CreateBr(inner_loop_block);

        builder->SetInsertPoint(inner_loop_block);
        llvm::PHINode *const entry_index_phi = builder->CreatePHI(i64, 2, stage + "entry_index");
        entry_index_phi->addIncoming(builder->getInt64(0), inner_loop_init_block);
        llvm::Value *const has_more = builder->CreateICmpULT(entry_index_phi, local_phi, "has_more");
        builder->CreateCondBr(has_more, inner_body_block, inner_done_block);

        builder->SetInsertPoint(inner_body_block);
        llvm::Value *const entries_gep = builder->CreateStructGEP(chunk_ty, chunk_outer_phi, Error::Chunk::ENTRIES, "entries_gep");
        llvm::Value *const entry_ptr = builder->CreateGEP(                                              //
            entries_array_ty, entries_gep, {builder->getInt32(0), entry_index_phi}, stage + "entry_ptr" //
        );
        llvm::Value *const next_index = builder->CreateAdd(entry_index_phi, builder->getInt64(1), "next_index");
        body(TraceEntryContext{chunk_outer_phi, local_phi, is_tail_chunk, entry_ptr, next_index});
        entry_index_phi->addIncoming(next_index, inner_body_block);
        builder->CreateBr(inner_loop_block);

        builder->SetInsertPoint(inner_done_block);
        llvm::Value *const is_last_chunk = builder->CreateICmpEQ(chunk_outer_phi, tail, "is_last_chunk");
        builder->CreateCondBr(is_last_chunk, after_block, outer_continue_block);

        builder->SetInsertPoint(outer_continue_block);
        llvm::Value *const next_gep = builder->CreateStructGEP(chunk_ty, chunk_outer_phi, Error::Chunk::NEXT, "outer_next_gep");
        llvm::Value *const next_chunk = IR::aligned_load(*builder, PTR_TY, next_gep, "outer_next");
        builder->CreateBr(outer_loop_block);
        chunk_outer_phi->addIncoming(next_chunk, outer_continue_block);

        return walk_block;
    };

    const auto measure_body = [&](const TraceEntryContext &ctx) -> void {
        llvm::Value *const entry_fn_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::FN_NAME, "entry_fn_gep");
        llvm::Value *const fn_name = IR::aligned_load(*builder, PTR_TY, entry_fn_gep, "fn_name");
        llvm::Value *const fn_len = builder->CreateCall(strlen_fn, {fn_name}, "fn_len");
        llvm::Value *const name_max = IR::aligned_load(*builder, i64, name_max_alloca, "name_max");
        llvm::Value *const new_name_max = builder->CreateSelect(                                    //
            builder->CreateICmpUGT(fn_len, name_max, "fn_longer"), fn_len, name_max, "new_name_max" //
        );
        IR::aligned_store(*builder, new_name_max, name_max_alloca);

        llvm::Value *const entry_path_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::FILE_PATH, "entry_path_gep");
        llvm::Value *const file_path = IR::aligned_load(*builder, PTR_TY, entry_path_gep, "file_path");
        llvm::Value *const entry_line_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::LINE, "entry_line_gep");
        llvm::Value *const line = IR::aligned_load(*builder, i32, entry_line_gep, "line");
        llvm::Value *const entry_column_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::COLUMN, "entry_column_gep");
        llvm::Value *const column = IR::aligned_load(*builder, i32, entry_column_gep, "column");
        llvm::Value *loc_len = builder->CreateCall(strlen_fn, {file_path}, "path_len");
        loc_len = builder->CreateAdd(loc_len, digit_count(line), "loc_len");
        loc_len = builder->CreateAdd(loc_len, builder->getInt64(1), "loc_len");
        loc_len = builder->CreateAdd(loc_len, digit_count(column), "loc_len");
        loc_len = builder->CreateAdd(loc_len, builder->getInt64(1), "loc_len");
        llvm::Value *const loc_max = IR::aligned_load(*builder, i64, loc_max_alloca, "loc_max");
        llvm::Value *const new_loc_max = builder->CreateSelect(                                     //
            builder->CreateICmpUGT(loc_len, loc_max, "loc_longer"), loc_len, loc_max, "new_loc_max" //
        );
        IR::aligned_store(*builder, new_loc_max, loc_max_alloca);
    };
    llvm::BasicBlock *const measure_walk = emit_trace_walk("measure_", width_block, measure_body);

    builder->SetInsertPoint(width_block);
    llvm::Value *const name_max = IR::aligned_load(*builder, i64, name_max_alloca, "name_max");
    llvm::Value *const name_max_gt_8 = builder->CreateICmpUGT(name_max, builder->getInt64(8), "name_max_gt_8");
    llvm::Value *const name_raw = builder->CreateSelect(name_max_gt_8, name_max, builder->getInt64(8), "name_raw");
    llvm::Value *const name_box_width = builder->CreateAdd(name_raw, builder->CreateAnd(name_raw, builder->getInt64(1)), "name_box_width");
    llvm::Value *const loc_max = IR::aligned_load(*builder, i64, loc_max_alloca, "loc_max");
    llvm::Value *const loc_max_gt_12 = builder->CreateICmpUGT(loc_max, builder->getInt64(12), "loc_max_gt_12");
    llvm::Value *const loc_raw = builder->CreateSelect(loc_max_gt_12, loc_max, builder->getInt64(12), "loc_raw");
    llvm::Value *const loc_box_width = builder->CreateAdd(loc_raw, builder->CreateAnd(loc_raw, builder->getInt64(1)), "loc_box_width");
    llvm::Value *const name_box_width_m6 = builder->CreateSub(name_box_width, builder->getInt64(6), "name_box_width_m6");
    llvm::Value *const name_dashes = builder->CreateSDiv(name_box_width_m6, builder->getInt64(2), "name_dashes");
    llvm::Value *const loc_box_width_m10 = builder->CreateSub(loc_box_width, builder->getInt64(10), "loc_box_width_m10");
    llvm::Value *const loc_dashes = builder->CreateSDiv(loc_box_width_m10, builder->getInt64(2), "loc_dashes");
    builder->CreateCall(printf_fn, {box_top_fmt});
    emit_dash_loop(name_dashes, "header_name_a_");
    builder->CreateCall(printf_fn, {name_label_fmt});
    emit_dash_loop(name_dashes, "header_name_b_");
    builder->CreateCall(printf_fn, {box_mid_fmt});
    emit_dash_loop(loc_dashes, "header_loc_a_");
    builder->CreateCall(printf_fn, {location_label_fmt});
    emit_dash_loop(loc_dashes, "header_loc_b_");
    builder->CreateCall(printf_fn, {box_mid_fmt});
    builder->CreateCall(printf_fn, {box_tail_fmt});
    llvm::BasicBlock *const header_done_block = builder->GetInsertBlock();

    const auto print_body = [&](const TraceEntryContext &ctx) -> void {
        llvm::Value *const entry_fn_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::FN_NAME, "entry_fn_gep");
        llvm::Value *const fn_name = IR::aligned_load(*builder, PTR_TY, entry_fn_gep, "fn_name");
        llvm::Value *const entry_path_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::FILE_PATH, "entry_path_gep");
        llvm::Value *const file_path = IR::aligned_load(*builder, PTR_TY, entry_path_gep, "file_path");
        llvm::Value *const entry_line_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::LINE, "entry_line_gep");
        llvm::Value *const line = IR::aligned_load(*builder, i32, entry_line_gep, "line");
        llvm::Value *const entry_column_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::COLUMN, "entry_column_gep");
        llvm::Value *const column = IR::aligned_load(*builder, i32, entry_column_gep, "column");
        llvm::Value *const entry_rethrow_gep = builder->CreateStructGEP(entry_ty, ctx.entry_ptr, Trace::IS_RETHROW, "entry_rethrow_gep");
        llvm::Value *const is_rethrow = IR::aligned_load(*builder, builder->getInt1Ty(), entry_rethrow_gep, "is_rethrow");
        llvm::Value *loc_len = builder->CreateCall(strlen_fn, {file_path}, "path_len");
        loc_len = builder->CreateAdd(loc_len, digit_count(line), "loc_len");
        loc_len = builder->CreateAdd(loc_len, builder->getInt64(1), "loc_len");
        loc_len = builder->CreateAdd(loc_len, digit_count(column), "loc_len");
        loc_len = builder->CreateAdd(loc_len, builder->getInt64(1), "loc_len");
        llvm::Value *const pad = builder->CreateSub(loc_box_width, loc_len, "pad");
        llvm::Value *const is_last_entry = builder->CreateAnd(                                         //
            ctx.is_tail_chunk, builder->CreateICmpEQ(ctx.next_index, ctx.local_count), "is_last_entry" //
        );
        llvm::Value *const entry_prefix = builder->CreateSelect(is_last_entry, prefix_last, prefix_normal, "prefix");
        llvm::Value *const info_sel = builder->CreateSelect(is_rethrow, info_rethrow, info_origin, "info_sel");
        llvm::Value *const info = builder->CreateSelect(is_last_entry, empty_str, info_sel, "info");
        llvm::Value *const right_bar = builder->CreateSelect(is_last_entry, box_right_last, box_right_mid, "right_bar");
        builder->CreateCall(printf_fn,
            {
                entry_fmt,
                entry_prefix,
                builder->CreateIntCast(name_box_width, i32, false, "name_width"),
                fn_name,
                file_path,
                line,
                column,
                builder->CreateIntCast(pad, i32, false, "pad_width"),
                empty_str,
                builder->getInt32(8),
                info,
                right_bar,
            } //
        );
    };
    llvm::BasicBlock *const print_walk = emit_trace_walk("print_", done_block, print_body);

    builder->SetInsertPoint(header_done_block);
    builder->CreateBr(print_walk);

    builder->SetInsertPoint(done_block);
    builder->CreateCall(fflush_fn, {llvm::ConstantPointerNull::get(PTR_TY)});
    builder->CreateRetVoid();

    builder->SetInsertPoint(entry_block);
    builder->CreateCondBr(is_empty, done_block, measure_walk);
}
