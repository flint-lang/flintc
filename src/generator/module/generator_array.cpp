#include "generator/generator.hpp"
#include "globals.hpp"
#include "lexer/builtins.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Intrinsics.h"

void Generator::Array::generate_create_arr_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION
    // str *create_arr(const size_t dimensionality, const size_t element_size, const size_t *lengths) {
    //     size_t arr_len = 1;
    //     size_t total_size = sizeof(str) + dimensionality * sizeof(size_t);
    //     // Read the lengths of each dimension from the varargs
    //     for (size_t i = 0; i < dimensionality; i++) {
    //         arr_len *= lengths[i];
    //     }
    //     // Allocate memory for the str struct + its array
    //     str *arr = (str *)malloc(total_size + arr_len * element_size);
    //     // Set the dimensionality (len field)
    //     arr->len = dimensionality;
    //     // Store the lengths of each dimension in the value array
    //     memcpy(arr->value, lengths, dimensionality * sizeof(size_t));
    //     return arr;
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("str_var")).first;
    llvm::Function *malloc_fn = c_functions.at(MALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *create_arr_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {
            builder->getInt64Ty(),                // Argument size_t dimensionality
            builder->getInt64Ty(),                // Argument size_t element_size
            builder->getInt64Ty()->getPointerTo() // Argument size_t* lengths
        },
        false // No vaargs
    );
    llvm::Function *create_arr_fn = llvm::Function::Create( //
        create_arr_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_create_arr",                               //
        module                                              //
    );
    array_manip_functions["create_arr"] = create_arr_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (dimensionality)
    llvm::Argument *arg_dimensionality = create_arr_fn->arg_begin();
    arg_dimensionality->setName("dimensionality");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = create_arr_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (lengths)
    llvm::Argument *arg_lengths = create_arr_fn->arg_begin() + 2;
    arg_lengths->setName("lengths");

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", create_arr_fn);
    builder->SetInsertPoint(entry_block);

    // Initialize arr_len = 1
    llvm::Value *arr_len = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "arr_len_ptr");
    builder->CreateStore(builder->getInt64(1), arr_len);

    // Calculate total_size = sizeof(str) + dimensionality * sizeof(size_t)
    llvm::DataLayout data_layout(module);
    uint64_t str_size = data_layout.getTypeAllocSize(str_type);
    llvm::Value *dimensionality_size = builder->CreateMul(arg_dimensionality, builder->getInt64(8), "dimensionality_size");
    llvm::Value *total_size = builder->CreateAdd( //
        builder->getInt64(str_size),              //
        dimensionality_size,                      //
        "total_size"                              //
    );

    // Create a loop to calculate arr_len by multiplying the dimension sizes
    llvm::BasicBlock *loop_entry_block = llvm::BasicBlock::Create(context, "loop_entry", create_arr_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", create_arr_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", create_arr_fn);

    builder->SetInsertPoint(entry_block);
    // Create loop counter
    llvm::Value *counter = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    builder->CreateStore(builder->getInt64(0), counter);
    // Create branch to the loop entry block
    builder->CreateBr(loop_entry_block);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop_entry_block);
    llvm::Value *current_counter = builder->CreateLoad(builder->getInt64Ty(), counter, "current_counter");
    llvm::Value *cond = builder->CreateICmpULT(current_counter, arg_dimensionality, "loop_cond");
    builder->CreateCondBr(cond, loop_body_block, merge_block);

    // Loop body
    builder->SetInsertPoint(loop_body_block);

    // Load the current dimension length: lenghs[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), arg_lengths, current_counter, "length_ptr");
    llvm::Value *current_length = builder->CreateLoad(builder->getInt64Ty(), length_ptr, "current_length");

    // arg_len *= lengths[i]
    llvm::Value *current_arr_len = builder->CreateLoad(builder->getInt64Ty(), arr_len, "current_arr_len");
    llvm::Value *new_arr_len = builder->CreateMul(current_arr_len, current_length, "arr_len");
    builder->CreateStore(new_arr_len, arr_len);
    // Increnment counter
    llvm::Value *next_counter = builder->CreateAdd(current_counter, builder->getInt64(1), "next_counter");
    builder->CreateStore(next_counter, counter);

    // Jump back to the condition check
    builder->CreateBr(loop_entry_block);

    // The merge block
    builder->SetInsertPoint(merge_block);

    // Load the final arr_len value
    llvm::Value *final_arr_len = builder->CreateLoad(builder->getInt64Ty(), arr_len, "final_arr_len");

    // Calculate the total allocation size: total_size + arr_len * element_size
    llvm::Value *malloc_size = builder->CreateAdd(total_size, builder->CreateMul(final_arr_len, arg_element_size), "malloc_size");

    // Allocate memory for the array
    llvm::Value *arr_ptr = builder->CreateCall(malloc_fn, {malloc_size}, "arr_ptr");
    llvm::Value *arr = builder->CreateBitCast(arr_ptr, str_type->getPointerTo(), "arr");

    // Set the dimensionality (len field): arr->len = dimensionality
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arr, 0, "len_ptr");
    llvm::StoreInst *dim_store = builder->CreateStore(arg_dimensionality, len_ptr);
    dim_store->setAlignment(llvm::Align(8));

    // Store the lengths of each dimension in the value array
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arr, 1, "value_ptr");
    builder->CreateCall(memcpy_fn, {value_ptr, arg_lengths, dimensionality_size});

    builder->CreateRet(arr);
}

void Generator::Array::generate_fill_arr_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // void fill_arr(str *arr, const size_t element_size, const void *value) {
    //     const size_t dimensionality = arr->len;
    //     size_t *dim_lengths = (size_t *)arr->value;
    //     size_t total_elements = 1;
    //     for (size_t i = 0; i < dimensionality; i++) {
    //         total_elements *= dim_lengths[i];
    //     }
    //     char *data_start = (char *)(dim_lengths + dimensionality);
    //     memcpy(data_start, value, element_size);
    //     // Use exponential approach for small elements or sequential for large elements
    //     if (element_size < 128) { // Threshold based on benchmarks
    //         // Exponential fill
    //         size_t filled = 1;
    //         while (filled < total_elements) {
    //             size_t to_copy = (filled <= total_elements - filled) ? filled : total_elements - filled;
    //             memcpy(data_start + (filled * element_size), data_start, to_copy * element_size);
    //             filled += to_copy;
    //         }
    //         return;
    //     }
    //     // Sequential fill
    //     for (size_t i = 1; i < total_elements; i++) {
    //         memcpy(data_start + (i * element_size), data_start + ((i - 1) * element_size), element_size);
    //     }
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("str_var")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *fill_arr_type = llvm::FunctionType::get( //
        builder->getVoidTy(),                                    // Return type: void
        {
            str_type->getPointerTo(),            // Argument str* arr
            builder->getInt64Ty(),               // Argument size_t element_size
            builder->getVoidTy()->getPointerTo() // Argument void* value
        },
        false // No vaargs
    );
    llvm::Function *fill_arr_fn = llvm::Function::Create( //
        fill_arr_type,                                    //
        llvm::Function::ExternalLinkage,                  //
        "__flint_fill_arr",                               //
        module                                            //
    );
    array_manip_functions["fill_arr"] = fill_arr_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (dimensionality)
    llvm::Argument *arg_arr = fill_arr_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = fill_arr_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (lengths)
    llvm::Argument *arg_value = fill_arr_fn->arg_begin() + 2;
    arg_value->setName("value");

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", fill_arr_fn);
    builder->SetInsertPoint(entry_block);

    // Get dimensionality = arr->len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_arr, 0, "len_ptr");
    llvm::Value *dimensionality = builder->CreateLoad(builder->getInt64Ty(), len_ptr, "dimensionality");

    // Get dim_lengths = (size_t *)arr->value
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *dim_lengths = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "dim_lengths");

    // Initialize total_elements = 1
    llvm::Value *total_elements_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "total_elements_ptr");
    builder->CreateStore(builder->getInt64(1), total_elements_ptr);

    // Create loop for calculating total_elements
    llvm::BasicBlock *loop1_entry = llvm::BasicBlock::Create(context, "loop1_entry", fill_arr_fn);
    llvm::BasicBlock *loop1_body = llvm::BasicBlock::Create(context, "loop1_body", fill_arr_fn);
    llvm::BasicBlock *loop1_exit = llvm::BasicBlock::Create(context, "loop1_exit", fill_arr_fn);

    // Create loop counter
    llvm::Value *counter1 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    builder->CreateStore(builder->getInt64(0), counter1);

    // Branch to loop condition check
    builder->CreateBr(loop1_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop1_entry);
    llvm::Value *current_counter1 = builder->CreateLoad(builder->getInt64Ty(), counter1, "current_counter");
    llvm::Value *cond1 = builder->CreateICmpULT(current_counter1, dimensionality, "loop1_cond");
    builder->CreateCondBr(cond1, loop1_body, loop1_exit);

    // Loop body
    builder->SetInsertPoint(loop1_body);

    // Load the current dimension length: dim_lengths[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, current_counter1, "length_ptr");
    llvm::Value *current_length = builder->CreateLoad(builder->getInt64Ty(), length_ptr, "current_length");

    // total_elements *= dim_lengths[i]
    llvm::Value *current_total = builder->CreateLoad(builder->getInt64Ty(), total_elements_ptr, "current_total");
    llvm::Value *new_total = builder->CreateMul(current_total, current_length, "new_total");
    builder->CreateStore(new_total, total_elements_ptr);

    // Increment counter
    llvm::Value *next_counter1 = builder->CreateAdd(current_counter1, builder->getInt64(1), "next_counter");
    builder->CreateStore(next_counter1, counter1);

    // Jump back to condition check
    builder->CreateBr(loop1_entry);

    // Loop1 exit
    builder->SetInsertPoint(loop1_exit);

    // Load the final total_elements value
    llvm::Value *total_elements = builder->CreateLoad(builder->getInt64Ty(), total_elements_ptr, "total_elements");

    // Calculate data_start = (char *)(dim_lengths + dimensionality)
    // This means moving past the dimension lengths to get to the actual array data
    llvm::Value *dim_lengths_offset = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, dimensionality, "dim_lengths_offset");
    llvm::Value *data_start = builder->CreateBitCast(dim_lengths_offset, builder->getInt8Ty()->getPointerTo(), "data_start");

    // memcpy(data_start, value, element_size) - copy the initial value to the first element
    llvm::Value *arg_value_cast = builder->CreateBitCast(arg_value, builder->getInt8Ty()->getPointerTo());
    builder->CreateCall(memcpy_fn, {data_start, arg_value_cast, arg_element_size});

    // Create if-else for choosing exponential or sequential fill
    llvm::Value *size_cond = builder->CreateICmpULT(arg_element_size, builder->getInt64(128), "size_cond");

    llvm::BasicBlock *exp_fill_block = llvm::BasicBlock::Create(context, "exp_fill", fill_arr_fn);
    llvm::BasicBlock *seq_fill_block = llvm::BasicBlock::Create(context, "seq_fill", fill_arr_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", fill_arr_fn);

    builder->CreateCondBr(size_cond, exp_fill_block, seq_fill_block);

    // Exponential fill block
    builder->SetInsertPoint(exp_fill_block);

    // size_t filled = 1
    llvm::Value *filled_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "filled_ptr");
    builder->CreateStore(builder->getInt64(1), filled_ptr);

    llvm::BasicBlock *exp_loop_entry = llvm::BasicBlock::Create(context, "exp_loop_entry", fill_arr_fn);
    llvm::BasicBlock *exp_loop_body = llvm::BasicBlock::Create(context, "exp_loop_body", fill_arr_fn);

    builder->CreateBr(exp_loop_entry);

    // Exponential fill loop condition
    builder->SetInsertPoint(exp_loop_entry);
    llvm::Value *current_filled = builder->CreateLoad(builder->getInt64Ty(), filled_ptr, "current_filled");
    llvm::Value *exp_cond = builder->CreateICmpULT(current_filled, total_elements, "exp_cond");
    builder->CreateCondBr(exp_cond, exp_loop_body, exit_block);

    // Exponential fill loop body
    builder->SetInsertPoint(exp_loop_body);

    // Calculate to_copy = (filled <= total_elements - filled) ? filled : total_elements - filled
    llvm::Value *remaining = builder->CreateSub(total_elements, current_filled, "remaining");
    llvm::Value *cmp_filled_remaining = builder->CreateICmpULE(current_filled, remaining, "cmp_filled_remaining");
    llvm::Value *to_copy = builder->CreateSelect(cmp_filled_remaining, current_filled, remaining, "to_copy");

    // Calculate destination: data_start + (filled * element_size)
    llvm::Value *dest_offset = builder->CreateMul(current_filled, arg_element_size, "dest_offset");
    llvm::Value *dest_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, dest_offset, "dest_ptr");

    // Calculate copy size: to_copy * element_size
    llvm::Value *copy_size = builder->CreateMul(to_copy, arg_element_size, "copy_size");

    // memcpy(data_start + (filled * element_size), data_start, to_copy * element_size)
    builder->CreateCall(memcpy_fn, {dest_ptr, data_start, copy_size});

    // filled += to_copy
    llvm::Value *new_filled = builder->CreateAdd(current_filled, to_copy, "new_filled");
    builder->CreateStore(new_filled, filled_ptr);

    // Jump back to loop condition
    builder->CreateBr(exp_loop_entry);

    // Sequential fill block
    builder->SetInsertPoint(seq_fill_block);

    // Create loop for sequential fill
    llvm::BasicBlock *seq_loop_entry = llvm::BasicBlock::Create(context, "seq_loop_entry", fill_arr_fn);
    llvm::BasicBlock *seq_loop_body = llvm::BasicBlock::Create(context, "seq_loop_body", fill_arr_fn);

    // Initialize i = 1
    llvm::Value *seq_counter = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "seq_i");
    builder->CreateStore(builder->getInt64(1), seq_counter);

    builder->CreateBr(seq_loop_entry);

    // Sequential fill loop condition
    builder->SetInsertPoint(seq_loop_entry);
    llvm::Value *current_seq_counter = builder->CreateLoad(builder->getInt64Ty(), seq_counter, "current_seq_counter");
    llvm::Value *seq_cond = builder->CreateICmpULT(current_seq_counter, total_elements, "seq_cond");
    builder->CreateCondBr(seq_cond, seq_loop_body, exit_block);

    // Sequential fill loop body
    builder->SetInsertPoint(seq_loop_body);

    // Calculate src: data_start + ((i - 1) * element_size)
    llvm::Value *prev_index = builder->CreateSub(current_seq_counter, builder->getInt64(1), "prev_index");
    llvm::Value *src_offset = builder->CreateMul(prev_index, arg_element_size, "src_offset");
    llvm::Value *src_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, src_offset, "src_ptr");

    // Calculate dest: data_start + (i * element_size)
    llvm::Value *curr_offset = builder->CreateMul(current_seq_counter, arg_element_size, "curr_offset");
    llvm::Value *curr_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, curr_offset, "curr_ptr");

    // memcpy(data_start + (i * element_size), data_start + ((i - 1) * element_size), element_size)
    builder->CreateCall(memcpy_fn, {curr_ptr, src_ptr, arg_element_size});

    // Increment counter
    llvm::Value *next_seq_counter = builder->CreateAdd(current_seq_counter, builder->getInt64(1), "next_seq_counter");
    builder->CreateStore(next_seq_counter, seq_counter);

    // Jump back to loop condition
    builder->CreateBr(seq_loop_entry);

    // Exit block
    builder->SetInsertPoint(exit_block);
    builder->CreateRetVoid();
}

void Generator::Array::generate_fill_arr_val_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // void fill_arr_val(str *arr, const size_t element_size, const size_t value) {
    //     const size_t dimensionality = arr->len;
    //     size_t *dim_lengths = (size_t *)arr->value;
    //     size_t total_elements = 1;
    //     for (size_t i = 0; i < dimensionality; i++) {
    //         total_elements *= dim_lengths[i];
    //     }
    //     char *data_start = (char *)(dim_lengths + dimensionality);
    //     memcpy(data_start, &value, element_size);
    //     // Use exponential approach for small elements or sequential for large elements
    //     if (element_size < 128) { // Threshold based on benchmarks
    //         // Exponential fill
    //         size_t filled = 1;
    //         while (filled < total_elements) {
    //             size_t to_copy = (filled <= total_elements - filled) ? filled : total_elements - filled;
    //             memcpy(data_start + (filled * element_size), data_start, to_copy * element_size);
    //             filled += to_copy;
    //         }
    //         return;
    //     }
    //     // Sequential fill
    //     for (size_t i = 1; i < total_elements; i++) {
    //         memcpy(data_start + (i * element_size), data_start + ((i - 1) * element_size), element_size);
    //     }
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("str_var")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *fill_arr_val_type = llvm::FunctionType::get( //
        builder->getVoidTy(),                                        // Return type: void
        {
            str_type->getPointerTo(), // Argument str* arr
            builder->getInt64Ty(),    // Argument size_t element_size
            builder->getInt64Ty()     // Argument size_t value
        },
        false // No vaargs
    );
    llvm::Function *fill_arr_val_fn = llvm::Function::Create( //
        fill_arr_val_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "__flint_fill_arr_val",                               //
        module                                                //
    );
    array_manip_functions["fill_arr_val"] = fill_arr_val_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (arr)
    llvm::Argument *arg_arr = fill_arr_val_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = fill_arr_val_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (value)
    llvm::Argument *arg_value = fill_arr_val_fn->arg_begin() + 2;
    arg_value->setName("value");

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", fill_arr_val_fn);
    builder->SetInsertPoint(entry_block);

    // Get dimensionality = arr->len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_arr, 0, "len_ptr");
    llvm::Value *dimensionality = builder->CreateLoad(builder->getInt64Ty(), len_ptr, "dimensionality");

    // Get dim_lengths = (size_t *)arr->value
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *dim_lengths = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "dim_lengths");

    // Initialize total_elements = 1
    llvm::Value *total_elements_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "total_elements_ptr");
    builder->CreateStore(builder->getInt64(1), total_elements_ptr);

    // Create loop for calculating total_elements
    llvm::BasicBlock *loop1_entry = llvm::BasicBlock::Create(context, "loop1_entry", fill_arr_val_fn);
    llvm::BasicBlock *loop1_body = llvm::BasicBlock::Create(context, "loop1_body", fill_arr_val_fn);
    llvm::BasicBlock *loop1_exit = llvm::BasicBlock::Create(context, "loop1_exit", fill_arr_val_fn);

    // Create loop counter
    llvm::Value *counter1 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    builder->CreateStore(builder->getInt64(0), counter1);

    // Branch to loop condition check
    builder->CreateBr(loop1_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop1_entry);
    llvm::Value *current_counter1 = builder->CreateLoad(builder->getInt64Ty(), counter1, "current_counter");
    llvm::Value *cond1 = builder->CreateICmpULT(current_counter1, dimensionality, "loop1_cond");
    builder->CreateCondBr(cond1, loop1_body, loop1_exit);

    // Loop body
    builder->SetInsertPoint(loop1_body);

    // Load the current dimension length: dim_lengths[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, current_counter1, "length_ptr");
    llvm::Value *current_length = builder->CreateLoad(builder->getInt64Ty(), length_ptr, "current_length");

    // total_elements *= dim_lengths[i]
    llvm::Value *current_total = builder->CreateLoad(builder->getInt64Ty(), total_elements_ptr, "current_total");
    llvm::Value *new_total = builder->CreateMul(current_total, current_length, "new_total");
    builder->CreateStore(new_total, total_elements_ptr);

    // Increment counter
    llvm::Value *next_counter1 = builder->CreateAdd(current_counter1, builder->getInt64(1), "next_counter");
    builder->CreateStore(next_counter1, counter1);

    // Jump back to condition check
    builder->CreateBr(loop1_entry);

    // Loop1 exit
    builder->SetInsertPoint(loop1_exit);

    // Load the final total_elements value
    llvm::Value *total_elements = builder->CreateLoad(builder->getInt64Ty(), total_elements_ptr, "total_elements");

    // Calculate data_start = (char *)(dim_lengths + dimensionality)
    // This means moving past the dimension lengths to get to the actual array data
    llvm::Value *dim_lengths_offset = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, dimensionality, "dim_lengths_offset");
    llvm::Value *data_start = builder->CreateBitCast(dim_lengths_offset, builder->getInt8Ty()->getPointerTo(), "data_start");

    // memcpy(data_start, &value, element_size) - copy the initial value to the first element
    llvm::Value *value_alloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "value_temp");
    builder->CreateStore(arg_value, value_alloca);
    builder->CreateCall(memcpy_fn, {data_start, value_alloca, arg_element_size});

    // Create if-else for choosing exponential or sequential fill
    llvm::Value *size_cond = builder->CreateICmpULT(arg_element_size, builder->getInt64(128), "size_cond");

    llvm::BasicBlock *exp_fill_block = llvm::BasicBlock::Create(context, "exp_fill", fill_arr_val_fn);
    llvm::BasicBlock *seq_fill_block = llvm::BasicBlock::Create(context, "seq_fill", fill_arr_val_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", fill_arr_val_fn);

    builder->CreateCondBr(size_cond, exp_fill_block, seq_fill_block);

    // Exponential fill block
    builder->SetInsertPoint(exp_fill_block);

    // size_t filled = 1
    llvm::Value *filled_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "filled_ptr");
    builder->CreateStore(builder->getInt64(1), filled_ptr);

    llvm::BasicBlock *exp_loop_entry = llvm::BasicBlock::Create(context, "exp_loop_entry", fill_arr_val_fn);
    llvm::BasicBlock *exp_loop_body = llvm::BasicBlock::Create(context, "exp_loop_body", fill_arr_val_fn);

    builder->CreateBr(exp_loop_entry);

    // Exponential fill loop condition
    builder->SetInsertPoint(exp_loop_entry);
    llvm::Value *current_filled = builder->CreateLoad(builder->getInt64Ty(), filled_ptr, "current_filled");
    llvm::Value *exp_cond = builder->CreateICmpULT(current_filled, total_elements, "exp_cond");
    builder->CreateCondBr(exp_cond, exp_loop_body, exit_block);

    // Exponential fill loop body
    builder->SetInsertPoint(exp_loop_body);

    // Calculate to_copy = (filled <= total_elements - filled) ? filled : total_elements - filled
    llvm::Value *remaining = builder->CreateSub(total_elements, current_filled, "remaining");
    llvm::Value *cmp_filled_remaining = builder->CreateICmpULE(current_filled, remaining, "cmp_filled_remaining");
    llvm::Value *to_copy = builder->CreateSelect(cmp_filled_remaining, current_filled, remaining, "to_copy");

    // Calculate destination: data_start + (filled * element_size)
    llvm::Value *dest_offset = builder->CreateMul(current_filled, arg_element_size, "dest_offset");
    llvm::Value *dest_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, dest_offset, "dest_ptr");

    // Calculate copy size: to_copy * element_size
    llvm::Value *copy_size = builder->CreateMul(to_copy, arg_element_size, "copy_size");

    // memcpy(data_start + (filled * element_size), data_start, to_copy * element_size)
    builder->CreateCall(memcpy_fn, {dest_ptr, data_start, copy_size});

    // filled += to_copy
    llvm::Value *new_filled = builder->CreateAdd(current_filled, to_copy, "new_filled");
    builder->CreateStore(new_filled, filled_ptr);

    // Jump back to loop condition
    builder->CreateBr(exp_loop_entry);

    // Sequential fill block
    builder->SetInsertPoint(seq_fill_block);

    // Create loop for sequential fill
    llvm::BasicBlock *seq_loop_entry = llvm::BasicBlock::Create(context, "seq_loop_entry", fill_arr_val_fn);
    llvm::BasicBlock *seq_loop_body = llvm::BasicBlock::Create(context, "seq_loop_body", fill_arr_val_fn);

    // Initialize i = 1
    llvm::Value *seq_counter = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "seq_i");
    builder->CreateStore(builder->getInt64(1), seq_counter);

    builder->CreateBr(seq_loop_entry);

    // Sequential fill loop condition
    builder->SetInsertPoint(seq_loop_entry);
    llvm::Value *current_seq_counter = builder->CreateLoad(builder->getInt64Ty(), seq_counter, "current_seq_counter");
    llvm::Value *seq_cond = builder->CreateICmpULT(current_seq_counter, total_elements, "seq_cond");
    builder->CreateCondBr(seq_cond, seq_loop_body, exit_block);

    // Sequential fill loop body
    builder->SetInsertPoint(seq_loop_body);

    // Calculate src: data_start + ((i - 1) * element_size)
    llvm::Value *prev_index = builder->CreateSub(current_seq_counter, builder->getInt64(1), "prev_index");
    llvm::Value *src_offset = builder->CreateMul(prev_index, arg_element_size, "src_offset");
    llvm::Value *src_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, src_offset, "src_ptr");

    // Calculate dest: data_start + (i * element_size)
    llvm::Value *curr_offset = builder->CreateMul(current_seq_counter, arg_element_size, "curr_offset");
    llvm::Value *curr_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, curr_offset, "curr_ptr");

    // memcpy(data_start + (i * element_size), data_start + ((i - 1) * element_size), element_size)
    builder->CreateCall(memcpy_fn, {curr_ptr, src_ptr, arg_element_size});

    // Increment counter
    llvm::Value *next_seq_counter = builder->CreateAdd(current_seq_counter, builder->getInt64(1), "next_seq_counter");
    builder->CreateStore(next_seq_counter, seq_counter);

    // Jump back to loop condition
    builder->CreateBr(seq_loop_entry);

    // Exit block
    builder->SetInsertPoint(exit_block);
    builder->CreateRetVoid();
}

void Generator::Array::generate_access_arr_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // char *access_arr(str *arr, const size_t element_size, const size_t *indices) {
    //     const size_t dimensionality = arr->len;
    //     size_t *dim_lengths = (size_t *)arr->value;
    //     // Calculate the offset
    //     size_t offset = 0;
    //     size_t stride = 1; // Stride for each dimension
    //     for (size_t i = 0; i < dimensionality; i++) {
    //         size_t index = indices[i];
    //         if (index >= dim_lengths[i]) {
    //             // Out of bounds access
    //             // Depending on the flangs `--array-...` here are different code blocks
    //         }
    //         offset += index * stride;
    //         // Update stride for the next dimension
    //         stride *= dim_lengths[i];
    //     }
    //     // Calculate the pointer to the desired element
    //     char *data_start = (char *)(dim_lengths + dimensionality); // Skip the dimension lengths
    //     return data_start + offset * element_size;
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("str_var")).first;

    llvm::FunctionType *access_arr_type = llvm::FunctionType::get( //
        builder->getInt8Ty()->getPointerTo(),                      // Return type: char*
        {
            str_type->getPointerTo(),             // Argument str* arr
            builder->getInt64Ty(),                // Argument size_t element_size
            builder->getInt64Ty()->getPointerTo() // Argument size_t* indices
        },
        false // No vaargs
    );
    llvm::Function *access_arr_fn = llvm::Function::Create( //
        access_arr_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_access_arr",                               //
        module                                              //
    );
    array_manip_functions["access_arr"] = access_arr_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (arr)
    llvm::Argument *arg_arr = access_arr_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = access_arr_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (indices)
    llvm::Argument *arg_indices = access_arr_fn->arg_begin() + 2;
    arg_indices->setName("indices");

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", access_arr_fn);
    llvm::BasicBlock *loop_block = llvm::BasicBlock::Create(context, "loop", access_arr_fn);
    llvm::BasicBlock *bounds_check_block = nullptr;
    llvm::BasicBlock *out_of_bounds_block = nullptr;
    if (oob_mode != ArrayOutOfBoundsMode::UNSAFE) {
        bounds_check_block = llvm::BasicBlock::Create(context, "bounds_check", access_arr_fn);
        out_of_bounds_block = llvm::BasicBlock::Create(context, "out_of_bounds", access_arr_fn);
    }
    llvm::BasicBlock *in_bounds_block = llvm::BasicBlock::Create(context, "in_bounds", access_arr_fn);
    llvm::BasicBlock *continue_block = llvm::BasicBlock::Create(context, "continue", access_arr_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", access_arr_fn);
    builder->SetInsertPoint(entry_block);

    // const size_t dimensionality = arr->len;
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_arr, 0, "len_ptr");
    llvm::Value *dimensionality = builder->CreateLoad(builder->getInt64Ty(), len_ptr, "dimensionality");

    // size_t *dim_lengths = (size_t *)arr->value;
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *dim_lengths = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "dim_lengths");

    // Initialize offset = 0
    llvm::Value *offset_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "offset_ptr");
    builder->CreateStore(builder->getInt64(0), offset_ptr);

    // Initialize stride = 1
    llvm::Value *stride_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "stride_ptr");
    builder->CreateStore(builder->getInt64(1), stride_ptr);

    // Initialize counter i = 0
    llvm::Value *counter_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i_ptr");
    builder->CreateStore(builder->getInt64(0), counter_ptr);

    // Branch to loop condition
    builder->CreateBr(loop_block);

    // Loop condition: i < dimensionality
    builder->SetInsertPoint(loop_block);
    llvm::Value *current_counter = builder->CreateLoad(builder->getInt64Ty(), counter_ptr, "i");
    llvm::Value *loop_cond = builder->CreateICmpULT(current_counter, dimensionality, "loop_cond");

    // size_t index = indices[i] - Now use our local copy
    llvm::Value *index_ptr = builder->CreateGEP(builder->getInt64Ty(), arg_indices, current_counter, "index_ptr");
    llvm::Value *current_index = builder->CreateLoad(builder->getInt64Ty(), index_ptr, "index");
    llvm::Value *dim_length_ptr = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, current_counter, "dim_length_ptr");
    llvm::Value *current_dim_length = builder->CreateLoad(builder->getInt64Ty(), dim_length_ptr, "dim_length");

    if (oob_mode == ArrayOutOfBoundsMode::UNSAFE) {
        // Just jump to the "in bounds" block as we don't include a bounds check
        builder->CreateCondBr(loop_cond, in_bounds_block, exit_block);
    } else {
        builder->CreateCondBr(loop_cond, bounds_check_block, exit_block);

        // Get current index and check bounds
        builder->SetInsertPoint(bounds_check_block);

        // if (index >= dim_lengths[i]) return NULL;
        llvm::Value *bounds_cond = builder->CreateICmpUGE(current_index, current_dim_length, "bounds_cond");
        builder->CreateCondBr(bounds_cond, out_of_bounds_block, in_bounds_block);

        // Out of bounds
        builder->SetInsertPoint(out_of_bounds_block);
        // Print to the console that an OOB happened
        if (oob_mode == ArrayOutOfBoundsMode::PRINT || oob_mode == ArrayOutOfBoundsMode::CRASH) {
            llvm::Value *format_str = IR::generate_const_string(*builder, "Out Of Bounds access occured: Arr Len: %lu, Index: %lu\n");
            builder->CreateCall(builtins.at(PRINT), {format_str, current_dim_length, current_index});
        }
        switch (oob_mode) {
            case ArrayOutOfBoundsMode::PRINT:
                [[fallthrough]];
            case ArrayOutOfBoundsMode::SILENT: {
                // Apply index clamping and update our LOCAL copy
                llvm::Value *clamped_index = builder->CreateSub(current_dim_length, builder->getInt64(1), "clamped_index");
                builder->CreateStore(clamped_index, index_ptr); // Update our local copy
                builder->CreateBr(in_bounds_block);
                break;
            }
            case ArrayOutOfBoundsMode::CRASH:
                builder->CreateCall(c_functions.at(ABORT));
                builder->CreateUnreachable();
                break;
            case ArrayOutOfBoundsMode::UNSAFE:
                assert(false); // This should never be happening
                break;
        }
    }

    // In bounds - update offset and stride
    builder->SetInsertPoint(in_bounds_block);

    // Reload the potentially updated index from our local copy
    llvm::Value *index_to_use = builder->CreateLoad(builder->getInt64Ty(), index_ptr, "index_after_bounds_check");

    // load stride
    llvm::Value *current_stride = builder->CreateLoad(builder->getInt64Ty(), stride_ptr, "stride");

    // offset += index * stride;
    llvm::Value *current_offset = builder->CreateLoad(builder->getInt64Ty(), offset_ptr, "offset");
    llvm::Value *index_times_stride = builder->CreateMul(index_to_use, current_stride, "index_times_stride");
    llvm::Value *new_offset = builder->CreateAdd(current_offset, index_times_stride, "new_offset");
    builder->CreateStore(new_offset, offset_ptr);

    // stride *= dim_lengths[i];
    llvm::Value *new_stride = builder->CreateMul(current_stride, current_dim_length, "new_stride");
    builder->CreateStore(new_stride, stride_ptr);

    builder->CreateBr(continue_block);

    // Continue loop - increment counter
    builder->SetInsertPoint(continue_block);
    llvm::Value *next_counter = builder->CreateAdd(current_counter, builder->getInt64(1), "next_counter");
    builder->CreateStore(next_counter, counter_ptr);
    builder->CreateBr(loop_block);

    // Exit block - calculate final pointer
    builder->SetInsertPoint(exit_block);

    // Calculate the pointer to the data start
    // char *data_start = (char *)(dim_lengths + dimensionality);
    llvm::Value *dim_lengths_offset = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, dimensionality, "dim_lengths_offset");
    llvm::Value *data_start = builder->CreateBitCast(dim_lengths_offset, builder->getInt8Ty()->getPointerTo(), "data_start");

    // Calculate the final offset: data_start + offset * element_size
    llvm::Value *final_offset = builder->CreateLoad(builder->getInt64Ty(), offset_ptr, "final_offset");
    llvm::Value *byte_offset = builder->CreateMul(final_offset, arg_element_size, "byte_offset");
    llvm::Value *result_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, byte_offset, "result_ptr");

    // Return the pointer
    builder->CreateRet(result_ptr);
}

void Generator::Array::generate_access_arr_val_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // size_t access_arr_val(str *arr, const size_t element_size, const size_t *indices) {
    //     char *element = access_arr(arr, element_size, indices);
    //     size_t value;
    //     memcpy(&value, element, element_size);
    //     return value;
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("str_var")).first;
    llvm::Function *access_arr_fn = array_manip_functions.at("access_arr");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *access_arr_val_type = llvm::FunctionType::get( //
        builder->getInt64Ty(),                                         // Return type: size_t
        {
            str_type->getPointerTo(),             // Argument str* arr
            builder->getInt64Ty(),                // Argument size_t element_size
            builder->getInt64Ty()->getPointerTo() // Argument size_t* indices
        },
        false // No vaargs
    );
    llvm::Function *access_arr_val_fn = llvm::Function::Create( //
        access_arr_val_type,                                    //
        llvm::Function::ExternalLinkage,                        //
        "__flint_access_arr_val",                               //
        module                                                  //
    );
    array_manip_functions["access_arr_val"] = access_arr_val_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (arr)
    llvm::Argument *arg_arr = access_arr_val_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = access_arr_val_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (indices)
    llvm::Argument *arg_indices = access_arr_val_fn->arg_begin() + 2;
    arg_indices->setName("indices");

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", access_arr_val_fn);
    builder->SetInsertPoint(entry);

    llvm::Value *access_result = builder->CreateCall(access_arr_fn, {arg_arr, arg_element_size, arg_indices}, "element");
    llvm::Value *value = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "value_buffer");
    builder->CreateCall(memcpy_fn, {value, access_result, arg_element_size});
    llvm::Value *loaded_value = builder->CreateLoad(builder->getInt64Ty(), value, "value");
    builder->CreateRet(loaded_value);
}

void Generator::Array::generate_assign_arr_at_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // void assign_arr_at(str *arr, const size_t element_size, const size_t *indices, const void *value) {
    //     char *element = access_arr(arr, element_size, indices);
    //     memcpy(element, value, element_size);
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("str_var")).first;
    llvm::Function *access_arr_fn = array_manip_functions.at("access_arr");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *assign_arr_at_type = llvm::FunctionType::get( //
        builder->getVoidTy(),                                         // Return type: void
        {
            str_type->getPointerTo(),              // Argument str* arr
            builder->getInt64Ty(),                 // Argument size_t element_size
            builder->getInt64Ty()->getPointerTo(), // Argument size_t* indices
            builder->getVoidTy()->getPointerTo()   // Argument: void* value
        },
        false // No vaargs
    );
    llvm::Function *assign_arr_at_fn = llvm::Function::Create( //
        assign_arr_at_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_assign_arr_at",                               //
        module                                                 //
    );
    array_manip_functions["assign_arr_at"] = assign_arr_at_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (arr)
    llvm::Argument *arg_arr = assign_arr_at_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = assign_arr_at_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (indices)
    llvm::Argument *arg_indices = assign_arr_at_fn->arg_begin() + 2;
    arg_indices->setName("indices");
    // Get the parameter (value)
    llvm::Argument *arg_value = assign_arr_at_fn->arg_begin() + 3;
    arg_value->setName("value");

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", assign_arr_at_fn);
    builder->SetInsertPoint(entry);

    llvm::Value *access_result = builder->CreateCall(access_arr_fn, {arg_arr, arg_element_size, arg_indices}, "element");
    builder->CreateCall(memcpy_fn, {access_result, arg_value, arg_element_size});

    builder->CreateRetVoid();
}

void Generator::Array::generate_assign_arr_val_at_function( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations                            //
) {
    // THE C IMPLEMENTATION:
    // void assign_arr_val_at(str *arr, const size_t element_size, const size_t *indices, const size_t value) {
    //     char *element = access_arr(arr, element_size, indices);
    //     memcpy(element, &value, element_size);
    // }
    llvm::Type *str_type = IR::get_type(Type::get_primitive_type("str_var")).first;
    llvm::Function *access_arr_fn = array_manip_functions.at("access_arr");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *assign_arr_val_at_type = llvm::FunctionType::get( //
        builder->getVoidTy(),                                             // Return type: void
        {
            str_type->getPointerTo(),              // Argument str* arr
            builder->getInt64Ty(),                 // Argument size_t element_size
            builder->getInt64Ty()->getPointerTo(), // Argument size_t* indices
            builder->getInt64Ty()                  // Argument: size_t value
        },
        false // No vaargs
    );
    llvm::Function *assign_arr_val_at_fn = llvm::Function::Create( //
        assign_arr_val_at_type,                                    //
        llvm::Function::ExternalLinkage,                           //
        "__flint_assign_val_arr_at",                               //
        module                                                     //
    );
    array_manip_functions["assign_arr_val_at"] = assign_arr_val_at_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (arr)
    llvm::Argument *arg_arr = assign_arr_val_at_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = assign_arr_val_at_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (indices)
    llvm::Argument *arg_indices = assign_arr_val_at_fn->arg_begin() + 2;
    arg_indices->setName("indices");
    // Get the parameter (value)
    llvm::Argument *arg_value = assign_arr_val_at_fn->arg_begin() + 3;
    arg_value->setName("value");

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", assign_arr_val_at_fn);
    builder->SetInsertPoint(entry);

    llvm::Value *access_result = builder->CreateCall(access_arr_fn, {arg_arr, arg_element_size, arg_indices}, "element");
    llvm::Value *val = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "val");
    builder->CreateStore(arg_value, val);
    builder->CreateCall(memcpy_fn, {access_result, val, arg_element_size});

    builder->CreateRetVoid();
}

void Generator::Array::generate_array_manip_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declaration) {
    generate_create_arr_function(builder, module, only_declaration);
    generate_fill_arr_function(builder, module, only_declaration);
    generate_fill_arr_val_function(builder, module, only_declaration);
    generate_access_arr_function(builder, module, only_declaration);
    generate_access_arr_val_function(builder, module, only_declaration);
    generate_assign_arr_at_function(builder, module, only_declaration);
    generate_assign_arr_val_at_function(builder, module, only_declaration);
}
