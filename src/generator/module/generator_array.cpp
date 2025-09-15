#include "generator/generator.hpp"
#include "globals.hpp"
#include "lexer/builtins.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Intrinsics.h"

void Generator::Module::Array::generate_get_arr_len_function( //
    llvm::IRBuilder<> *builder,                               //
    llvm::Module *module,                                     //
    const bool only_declarations                              //
) {
    // THE C IMPLEMENTATATION
    // size_t get_arr_len(str * arr) {
    //     const size_t dimensionality = arr->len;
    //     const size_t *lengths = (size_t *)arr->value;
    //     size_t arr_len = 1;
    //     for (size_t i = 0; i < dimensionality; i++) {
    //         arr_len *= lengths[i];
    //     }
    //     return arr_len;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;

    llvm::FunctionType *get_arr_len_type = llvm::FunctionType::get( //
        llvm::Type::getInt64Ty(context),                            // Return type: size_t
        {str_type->getPointerTo()},                                 // Argument: str* arr
        false                                                       // No vaargs
    );
    llvm::Function *get_arr_len_fn = llvm::Function::Create( //
        get_arr_len_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "__flint_get_arr_len",                               //
        module                                               //
    );
    array_manip_functions["get_arr_len"] = get_arr_len_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (arr)
    llvm::Argument *arg_arr = get_arr_len_fn->arg_begin();
    arg_arr->setName("arr");

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_arr_len_fn);
    builder->SetInsertPoint(entry_block);

    // Initialize arr_len = 1
    llvm::Value *arr_len = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "arr_len_ptr");
    IR::aligned_store(*builder, builder->getInt64(1), arr_len);

    // Get the pointer to the value field of the array, it contains the dimension lengths at it's front
    llvm::Value *lengths = builder->CreateStructGEP(str_type, arg_arr, 1, "lengths");

    // Load the dimensionality from the array
    llvm::Value *dimensionality_ptr = builder->CreateStructGEP(str_type, arg_arr, 0, "dimensionality_ptr");
    llvm::Value *dimensionality = IR::aligned_load(*builder, builder->getInt64Ty(), dimensionality_ptr, "dimensionality");

    // Create a loop to calculate arr_len by multiplying the dimension sizes
    llvm::BasicBlock *loop_entry_block = llvm::BasicBlock::Create(context, "loop_entry", get_arr_len_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", get_arr_len_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", get_arr_len_fn);

    builder->SetInsertPoint(entry_block);
    // Create loop counter
    llvm::Value *counter = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), counter);
    // Create branch to the loop entry block
    builder->CreateBr(loop_entry_block);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop_entry_block);
    llvm::Value *current_counter = IR::aligned_load(*builder, builder->getInt64Ty(), counter, "current_counter");
    llvm::Value *cond = builder->CreateICmpULT(current_counter, dimensionality, "loop_cond");
    builder->CreateCondBr(cond, loop_body_block, merge_block);

    // Loop body
    builder->SetInsertPoint(loop_body_block);

    // Load the current dimension length: lenghs[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), lengths, current_counter, "length_ptr");
    llvm::Value *current_length = IR::aligned_load(*builder, builder->getInt64Ty(), length_ptr, "current_length");

    // arg_len *= lengths[i]
    llvm::Value *current_arr_len = IR::aligned_load(*builder, builder->getInt64Ty(), arr_len, "current_arr_len");
    llvm::Value *new_arr_len = builder->CreateMul(current_arr_len, current_length, "arr_len");
    IR::aligned_store(*builder, new_arr_len, arr_len);
    // Increnment counter
    llvm::Value *next_counter = builder->CreateAdd(current_counter, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter, counter);

    // Jump back to the condition check
    builder->CreateBr(loop_entry_block);

    // The merge block
    builder->SetInsertPoint(merge_block);
    llvm::Value *total_length = IR::aligned_load(*builder, builder->getInt64Ty(), arr_len, "total_length");
    builder->CreateRet(total_length);
}

void Generator::Module::Array::generate_create_arr_function( //
    llvm::IRBuilder<> *builder,                              //
    llvm::Module *module,                                    //
    const bool only_declarations                             //
) {
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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *malloc_fn = c_functions.at(MALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *create_arr_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {
            llvm::Type::getInt64Ty(context),                // Argument size_t dimensionality
            llvm::Type::getInt64Ty(context),                // Argument size_t element_size
            llvm::Type::getInt64Ty(context)->getPointerTo() // Argument size_t* lengths
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
    IR::aligned_store(*builder, builder->getInt64(1), arr_len);

    // Calculate total_size = sizeof(str) + dimensionality * sizeof(size_t)
    llvm::DataLayout data_layout = module->getDataLayout();
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
    IR::aligned_store(*builder, builder->getInt64(0), counter);
    // Create branch to the loop entry block
    builder->CreateBr(loop_entry_block);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop_entry_block);
    llvm::Value *current_counter = IR::aligned_load(*builder, builder->getInt64Ty(), counter, "current_counter");
    llvm::Value *cond = builder->CreateICmpULT(current_counter, arg_dimensionality, "loop_cond");
    builder->CreateCondBr(cond, loop_body_block, merge_block);

    // Loop body
    builder->SetInsertPoint(loop_body_block);

    // Load the current dimension length: lenghs[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), arg_lengths, current_counter, "length_ptr");
    llvm::Value *current_length = IR::aligned_load(*builder, builder->getInt64Ty(), length_ptr, "current_length");

    // arg_len *= lengths[i]
    llvm::Value *current_arr_len = IR::aligned_load(*builder, builder->getInt64Ty(), arr_len, "current_arr_len");
    llvm::Value *new_arr_len = builder->CreateMul(current_arr_len, current_length, "arr_len");
    IR::aligned_store(*builder, new_arr_len, arr_len);
    // Increnment counter
    llvm::Value *next_counter = builder->CreateAdd(current_counter, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter, counter);

    // Jump back to the condition check
    builder->CreateBr(loop_entry_block);

    // The merge block
    builder->SetInsertPoint(merge_block);

    // Load the final arr_len value
    llvm::Value *final_arr_len = IR::aligned_load(*builder, builder->getInt64Ty(), arr_len, "final_arr_len");

    // Calculate the total allocation size: total_size + arr_len * element_size
    llvm::Value *malloc_size = builder->CreateAdd(total_size, builder->CreateMul(final_arr_len, arg_element_size), "malloc_size");

    // Allocate memory for the array
    llvm::Value *arr_ptr = builder->CreateCall(malloc_fn, {malloc_size}, "arr_ptr");
    llvm::Value *arr = builder->CreateBitCast(arr_ptr, str_type->getPointerTo(), "arr");

    // Set the dimensionality (len field): arr->len = dimensionality
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arr, 0, "len_ptr");
    llvm::StoreInst *dim_store = IR::aligned_store(*builder, arg_dimensionality, len_ptr);
    dim_store->setAlignment(llvm::Align(8));

    // Store the lengths of each dimension in the value array
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arr, 1, "value_ptr");
    builder->CreateCall(memcpy_fn, {value_ptr, arg_lengths, dimensionality_size});

    builder->CreateRet(arr);
}

void Generator::Module::Array::generate_fill_arr_inline_function( //
    llvm::IRBuilder<> *builder,                                   //
    llvm::Module *module,                                         //
    const bool only_declarations                                  //
) {
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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *fill_arr_inline_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                                 // Return type: void
        {
            str_type->getPointerTo(),                      // Argument str* arr
            llvm::Type::getInt64Ty(context),               // Argument size_t element_size
            llvm::Type::getVoidTy(context)->getPointerTo() // Argument void* value
        },
        false // No vaargs
    );
    llvm::Function *fill_arr_inline_fn = llvm::Function::Create( //
        fill_arr_inline_type,                                    //
        llvm::Function::ExternalLinkage,                         //
        "__flint_fill_arr_inline",                               //
        module                                                   //
    );
    array_manip_functions["fill_arr_inline"] = fill_arr_inline_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (dimensionality)
    llvm::Argument *arg_arr = fill_arr_inline_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (element_size)
    llvm::Argument *arg_element_size = fill_arr_inline_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");
    // Get the parameter (lengths)
    llvm::Argument *arg_value = fill_arr_inline_fn->arg_begin() + 2;
    arg_value->setName("value");

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", fill_arr_inline_fn);
    builder->SetInsertPoint(entry_block);

    // Get dimensionality = arr->len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_arr, 0, "len_ptr");
    llvm::Value *dimensionality = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "dimensionality");

    // Get dim_lengths = (size_t *)arr->value
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *dim_lengths = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "dim_lengths");

    // Initialize total_elements = 1
    llvm::Value *total_elements_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "total_elements_ptr");
    IR::aligned_store(*builder, builder->getInt64(1), total_elements_ptr);

    // Create loop for calculating total_elements
    llvm::BasicBlock *loop1_entry = llvm::BasicBlock::Create(context, "loop1_entry", fill_arr_inline_fn);
    llvm::BasicBlock *loop1_body = llvm::BasicBlock::Create(context, "loop1_body", fill_arr_inline_fn);
    llvm::BasicBlock *loop1_exit = llvm::BasicBlock::Create(context, "loop1_exit", fill_arr_inline_fn);
    llvm::BasicBlock *exp_fill_block = llvm::BasicBlock::Create(context, "exp_fill", fill_arr_inline_fn);
    llvm::BasicBlock *seq_fill_block = llvm::BasicBlock::Create(context, "seq_fill", fill_arr_inline_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", fill_arr_inline_fn);
    llvm::BasicBlock *exp_loop_entry = llvm::BasicBlock::Create(context, "exp_loop_entry", fill_arr_inline_fn);
    llvm::BasicBlock *exp_loop_body = llvm::BasicBlock::Create(context, "exp_loop_body", fill_arr_inline_fn);
    llvm::BasicBlock *seq_loop_entry = llvm::BasicBlock::Create(context, "seq_loop_entry", fill_arr_inline_fn);
    llvm::BasicBlock *seq_loop_body = llvm::BasicBlock::Create(context, "seq_loop_body", fill_arr_inline_fn);

    // Create loop counter
    llvm::Value *counter1 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), counter1);

    // Branch to loop condition check
    builder->CreateBr(loop1_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop1_entry);
    llvm::Value *current_counter1 = IR::aligned_load(*builder, builder->getInt64Ty(), counter1, "current_counter");
    llvm::Value *cond1 = builder->CreateICmpULT(current_counter1, dimensionality, "loop1_cond");
    builder->CreateCondBr(cond1, loop1_body, loop1_exit);

    // Loop body
    builder->SetInsertPoint(loop1_body);

    // Load the current dimension length: dim_lengths[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, current_counter1, "length_ptr");
    llvm::Value *current_length = IR::aligned_load(*builder, builder->getInt64Ty(), length_ptr, "current_length");

    // total_elements *= dim_lengths[i]
    llvm::Value *current_total = IR::aligned_load(*builder, builder->getInt64Ty(), total_elements_ptr, "current_total");
    llvm::Value *new_total = builder->CreateMul(current_total, current_length, "new_total");
    IR::aligned_store(*builder, new_total, total_elements_ptr);

    // Increment counter
    llvm::Value *next_counter1 = builder->CreateAdd(current_counter1, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter1, counter1);

    // Jump back to condition check
    builder->CreateBr(loop1_entry);

    // Loop1 exit
    builder->SetInsertPoint(loop1_exit);

    // Load the final total_elements value
    llvm::Value *total_elements = IR::aligned_load(*builder, builder->getInt64Ty(), total_elements_ptr, "total_elements");

    // Calculate data_start = (char *)(dim_lengths + dimensionality)
    // This means moving past the dimension lengths to get to the actual array data
    llvm::Value *dim_lengths_offset = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, dimensionality, "dim_lengths_offset");
    llvm::Value *data_start = builder->CreateBitCast(dim_lengths_offset, builder->getInt8Ty()->getPointerTo(), "data_start");

    // memcpy(data_start, value, element_size) - copy the initial value to the first element
    llvm::Value *arg_value_cast = builder->CreateBitCast(arg_value, builder->getInt8Ty()->getPointerTo());
    builder->CreateCall(memcpy_fn, {data_start, arg_value_cast, arg_element_size});

    // Create if-else for choosing exponential or sequential fill
    llvm::Value *size_cond = builder->CreateICmpULT(arg_element_size, builder->getInt64(128), "size_cond");
    builder->CreateCondBr(size_cond, exp_fill_block, seq_fill_block);

    // Exponential fill block
    builder->SetInsertPoint(exp_fill_block);

    // size_t filled = 1
    llvm::Value *filled_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "filled_ptr");
    IR::aligned_store(*builder, builder->getInt64(1), filled_ptr);
    builder->CreateBr(exp_loop_entry);

    // Exponential fill loop condition
    builder->SetInsertPoint(exp_loop_entry);
    llvm::Value *current_filled = IR::aligned_load(*builder, builder->getInt64Ty(), filled_ptr, "current_filled");
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
    IR::aligned_store(*builder, new_filled, filled_ptr);

    // Jump back to loop condition
    builder->CreateBr(exp_loop_entry);

    // Sequential fill block
    builder->SetInsertPoint(seq_fill_block);

    // Initialize i = 1
    llvm::Value *seq_counter = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "seq_i");
    IR::aligned_store(*builder, builder->getInt64(1), seq_counter);

    builder->CreateBr(seq_loop_entry);

    // Sequential fill loop condition
    builder->SetInsertPoint(seq_loop_entry);
    llvm::Value *current_seq_counter = IR::aligned_load(*builder, builder->getInt64Ty(), seq_counter, "current_seq_counter");
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
    IR::aligned_store(*builder, next_seq_counter, seq_counter);

    // Jump back to loop condition
    builder->CreateBr(seq_loop_entry);

    // Exit block
    builder->SetInsertPoint(exit_block);
    builder->CreateRetVoid();
}

void Generator::Module::Array::generate_fill_arr_deep_function( //
    llvm::IRBuilder<> *builder,                                 //
    llvm::Module *module,                                       //
    const bool only_declarations                                //
) {
    // THE C IMPLEMENTATION:
    // void fill_arr(str *arr, const size_t value_size, const void *value) {
    //     const size_t dimensionality = arr->len;
    //     size_t *dim_lengths = (size_t *)arr->value;
    //     size_t total_elements = 1;
    //     for (size_t i = 0; i < dimensionality; i++) {
    //         total_elements *= dim_lengths[i];
    //     }
    //     void **data_start = (void **)(dim_lengths + dimensionality);
    //     void **element_ptr;
    //     for (size_t i = 0; i < total_elements; i++) {
    //         element_ptr = data_start + i;
    //         *element_ptr = malloc(value_size);
    //         memcpy(*element_ptr, value, value_size);
    //     }
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *malloc_fn = c_functions.at(MALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *fill_arr_deep_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                               // Return type: void
        {
            str_type->getPointerTo(),                      // Argument str* arr
            llvm::Type::getInt64Ty(context),               // Argument size_t value_size
            llvm::Type::getVoidTy(context)->getPointerTo() // Argument void* value
        },
        false // No vaargs
    );
    llvm::Function *fill_arr_deep_fn = llvm::Function::Create( //
        fill_arr_deep_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_fill_arr_deep",                               //
        module                                                 //
    );
    array_manip_functions["fill_arr_deep"] = fill_arr_deep_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (dimensionality)
    llvm::Argument *arg_arr = fill_arr_deep_fn->arg_begin();
    arg_arr->setName("arr");
    // Get the parameter (value_size)
    llvm::Argument *arg_value_size = fill_arr_deep_fn->arg_begin() + 1;
    arg_value_size->setName("value_size");
    // Get the parameter (lengths)
    llvm::Argument *arg_value = fill_arr_deep_fn->arg_begin() + 2;
    arg_value->setName("value");

    // Create the basic blocks for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", fill_arr_deep_fn);
    llvm::BasicBlock *loop1_entry = llvm::BasicBlock::Create(context, "loop1_entry", fill_arr_deep_fn);
    llvm::BasicBlock *loop1_body = llvm::BasicBlock::Create(context, "loop1_body", fill_arr_deep_fn);
    llvm::BasicBlock *loop1_exit = llvm::BasicBlock::Create(context, "loop1_exit", fill_arr_deep_fn);
    llvm::BasicBlock *loop2_entry = llvm::BasicBlock::Create(context, "loop2_entry", fill_arr_deep_fn);
    llvm::BasicBlock *loop2_body = llvm::BasicBlock::Create(context, "loop2_body", fill_arr_deep_fn);
    llvm::BasicBlock *loop2_exit = llvm::BasicBlock::Create(context, "loop2_exit", fill_arr_deep_fn);
    builder->SetInsertPoint(entry_block);

    // Get dimensionality = arr->len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_arr, 0, "len_ptr");
    llvm::Value *dimensionality = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "dimensionality");

    // Get dim_lengths = (size_t *)arr->value
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *dim_lengths = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "dim_lengths");

    // Initialize total_elements = 1
    llvm::Value *total_elements_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "total_elements_ptr");
    IR::aligned_store(*builder, builder->getInt64(1), total_elements_ptr);

    // Create loop counter
    llvm::Value *counter1 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), counter1);

    // Branch to loop condition check
    builder->CreateBr(loop1_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop1_entry);
    llvm::Value *current_counter1 = IR::aligned_load(*builder, builder->getInt64Ty(), counter1, "current_counter");
    llvm::Value *cond1 = builder->CreateICmpULT(current_counter1, dimensionality, "loop1_cond");
    builder->CreateCondBr(cond1, loop1_body, loop1_exit);

    // Loop body
    builder->SetInsertPoint(loop1_body);

    // Load the current dimension length: dim_lengths[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, current_counter1, "length_ptr");
    llvm::Value *current_length = IR::aligned_load(*builder, builder->getInt64Ty(), length_ptr, "current_length");

    // total_elements *= dim_lengths[i]
    llvm::Value *current_total = IR::aligned_load(*builder, builder->getInt64Ty(), total_elements_ptr, "current_total");
    llvm::Value *new_total = builder->CreateMul(current_total, current_length, "new_total");
    IR::aligned_store(*builder, new_total, total_elements_ptr);

    // Increment counter
    llvm::Value *next_counter1 = builder->CreateAdd(current_counter1, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter1, counter1);

    // Jump back to condition check
    builder->CreateBr(loop1_entry);

    // Loop1 exit
    builder->SetInsertPoint(loop1_exit);

    // Load the final total_elements value
    llvm::Value *total_elements = IR::aligned_load(*builder, builder->getInt64Ty(), total_elements_ptr, "total_elements");

    // Calculate data_start = (void **)(dim_lengths + dimensionality)
    llvm::Value *dim_lengths_offset = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, dimensionality, "dim_lengths_offset");
    llvm::Value *data_start = builder->CreateBitCast(                                          //
        dim_lengths_offset, builder->getVoidTy()->getPointerTo()->getPointerTo(), "data_start" //
    );

    // Create loop counter
    llvm::Value *counter2 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), counter2);

    // Branch to loop condition check
    builder->CreateBr(loop2_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop2_entry);
    llvm::Value *current_counter2 = IR::aligned_load(*builder, builder->getInt64Ty(), counter2, "current_counter");
    llvm::Value *cond2 = builder->CreateICmpULT(current_counter2, total_elements, "loop2_cond");
    builder->CreateCondBr(cond2, loop2_body, loop2_exit);

    // Loop body
    builder->SetInsertPoint(loop2_body);

    // Calculate element_ptr = data_start + i
    llvm::Value *element_ptr = builder->CreateGEP(builder->getVoidTy()->getPointerTo(), data_start, current_counter2, "element_ptr");

    // Call malloc: *element_ptr = malloc(value_size)
    llvm::Value *allocated_mem = builder->CreateCall(malloc_fn, {arg_value_size}, "allocated_mem");

    // Store the allocated pointer: *element_ptr = allocated_mem
    IR::aligned_store(*builder, allocated_mem, element_ptr);

    // Call memcpy: memcpy(*element_ptr, value, value_size)
    llvm::Value *element_value = IR::aligned_load(*builder, builder->getVoidTy()->getPointerTo(), element_ptr, "element_value");
    builder->CreateCall(memcpy_fn, {element_value, arg_value, arg_value_size});

    // Increment counter
    llvm::Value *next_counter2 = builder->CreateAdd(current_counter2, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter2, counter2);

    // Jump back to condition check
    builder->CreateBr(loop2_entry);

    // Loop2 exit
    builder->SetInsertPoint(loop2_exit);
    builder->CreateRetVoid();
}

void Generator::Module::Array::generate_fill_arr_val_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *fill_arr_val_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                              // Return type: void
        {
            str_type->getPointerTo(),        // Argument str* arr
            llvm::Type::getInt64Ty(context), // Argument size_t element_size
            llvm::Type::getInt64Ty(context)  // Argument size_t value
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
    llvm::Value *dimensionality = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "dimensionality");

    // Get dim_lengths = (size_t *)arr->value
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *dim_lengths = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "dim_lengths");

    // Initialize total_elements = 1
    llvm::Value *total_elements_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "total_elements_ptr");
    IR::aligned_store(*builder, builder->getInt64(1), total_elements_ptr);

    // Create loop for calculating total_elements
    llvm::BasicBlock *loop1_entry = llvm::BasicBlock::Create(context, "loop1_entry", fill_arr_val_fn);
    llvm::BasicBlock *loop1_body = llvm::BasicBlock::Create(context, "loop1_body", fill_arr_val_fn);
    llvm::BasicBlock *loop1_exit = llvm::BasicBlock::Create(context, "loop1_exit", fill_arr_val_fn);

    // Create loop counter
    llvm::Value *counter1 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), counter1);

    // Branch to loop condition check
    builder->CreateBr(loop1_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop1_entry);
    llvm::Value *current_counter1 = IR::aligned_load(*builder, builder->getInt64Ty(), counter1, "current_counter");
    llvm::Value *cond1 = builder->CreateICmpULT(current_counter1, dimensionality, "loop1_cond");
    builder->CreateCondBr(cond1, loop1_body, loop1_exit);

    // Loop body
    builder->SetInsertPoint(loop1_body);

    // Load the current dimension length: dim_lengths[i]
    llvm::Value *length_ptr = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, current_counter1, "length_ptr");
    llvm::Value *current_length = IR::aligned_load(*builder, builder->getInt64Ty(), length_ptr, "current_length");

    // total_elements *= dim_lengths[i]
    llvm::Value *current_total = IR::aligned_load(*builder, builder->getInt64Ty(), total_elements_ptr, "current_total");
    llvm::Value *new_total = builder->CreateMul(current_total, current_length, "new_total");
    IR::aligned_store(*builder, new_total, total_elements_ptr);

    // Increment counter
    llvm::Value *next_counter1 = builder->CreateAdd(current_counter1, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter1, counter1);

    // Jump back to condition check
    builder->CreateBr(loop1_entry);

    // Loop1 exit
    builder->SetInsertPoint(loop1_exit);

    // Load the final total_elements value
    llvm::Value *total_elements = IR::aligned_load(*builder, builder->getInt64Ty(), total_elements_ptr, "total_elements");

    // Calculate data_start = (char *)(dim_lengths + dimensionality)
    // This means moving past the dimension lengths to get to the actual array data
    llvm::Value *dim_lengths_offset = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, dimensionality, "dim_lengths_offset");
    llvm::Value *data_start = builder->CreateBitCast(dim_lengths_offset, builder->getInt8Ty()->getPointerTo(), "data_start");

    // memcpy(data_start, &value, element_size) - copy the initial value to the first element
    llvm::Value *value_alloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "value_temp");
    IR::aligned_store(*builder, arg_value, value_alloca);
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
    IR::aligned_store(*builder, builder->getInt64(1), filled_ptr);

    llvm::BasicBlock *exp_loop_entry = llvm::BasicBlock::Create(context, "exp_loop_entry", fill_arr_val_fn);
    llvm::BasicBlock *exp_loop_body = llvm::BasicBlock::Create(context, "exp_loop_body", fill_arr_val_fn);

    builder->CreateBr(exp_loop_entry);

    // Exponential fill loop condition
    builder->SetInsertPoint(exp_loop_entry);
    llvm::Value *current_filled = IR::aligned_load(*builder, builder->getInt64Ty(), filled_ptr, "current_filled");
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
    IR::aligned_store(*builder, new_filled, filled_ptr);

    // Jump back to loop condition
    builder->CreateBr(exp_loop_entry);

    // Sequential fill block
    builder->SetInsertPoint(seq_fill_block);

    // Create loop for sequential fill
    llvm::BasicBlock *seq_loop_entry = llvm::BasicBlock::Create(context, "seq_loop_entry", fill_arr_val_fn);
    llvm::BasicBlock *seq_loop_body = llvm::BasicBlock::Create(context, "seq_loop_body", fill_arr_val_fn);

    // Initialize i = 1
    llvm::Value *seq_counter = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "seq_i");
    IR::aligned_store(*builder, builder->getInt64(1), seq_counter);

    builder->CreateBr(seq_loop_entry);

    // Sequential fill loop condition
    builder->SetInsertPoint(seq_loop_entry);
    llvm::Value *current_seq_counter = IR::aligned_load(*builder, builder->getInt64Ty(), seq_counter, "current_seq_counter");
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
    IR::aligned_store(*builder, next_seq_counter, seq_counter);

    // Jump back to loop condition
    builder->CreateBr(seq_loop_entry);

    // Exit block
    builder->SetInsertPoint(exit_block);
    builder->CreateRetVoid();
}

void Generator::Module::Array::generate_access_arr_function( //
    llvm::IRBuilder<> *builder,                              //
    llvm::Module *module,                                    //
    const bool only_declarations                             //
) {
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
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;

    llvm::FunctionType *access_arr_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),            // Return type: char*
        {
            str_type->getPointerTo(),                       // Argument str* arr
            llvm::Type::getInt64Ty(context),                // Argument size_t element_size
            llvm::Type::getInt64Ty(context)->getPointerTo() // Argument size_t* indices
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
    llvm::Value *dimensionality = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "dimensionality");

    // size_t *dim_lengths = (size_t *)arr->value;
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *dim_lengths = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "dim_lengths");

    // Initialize offset = 0
    llvm::Value *offset_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "offset_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), offset_ptr);

    // Initialize stride = 1
    llvm::Value *stride_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "stride_ptr");
    IR::aligned_store(*builder, builder->getInt64(1), stride_ptr);

    // Initialize counter i = 0
    llvm::Value *counter_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), counter_ptr);

    // Branch to loop condition
    builder->CreateBr(loop_block);

    // Loop condition: i < dimensionality
    builder->SetInsertPoint(loop_block);
    llvm::Value *current_counter = IR::aligned_load(*builder, builder->getInt64Ty(), counter_ptr, "i");
    llvm::Value *loop_cond = builder->CreateICmpULT(current_counter, dimensionality, "loop_cond");

    // size_t index = indices[i] - Now use our local copy
    llvm::Value *index_ptr = builder->CreateGEP(builder->getInt64Ty(), arg_indices, current_counter, "index_ptr");
    llvm::Value *current_index = IR::aligned_load(*builder, builder->getInt64Ty(), index_ptr, "index");
    llvm::Value *dim_length_ptr = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, current_counter, "dim_length_ptr");
    llvm::Value *current_dim_length = IR::aligned_load(*builder, builder->getInt64Ty(), dim_length_ptr, "dim_length");

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
            llvm::Value *format_str = IR::generate_const_string(module, "Out Of Bounds access occured: Arr Len: %lu, Index: %lu\n");
            builder->CreateCall(c_functions.at(PRINTF), {format_str, current_dim_length, current_index});
        }
        switch (oob_mode) {
            case ArrayOutOfBoundsMode::PRINT:
                [[fallthrough]];
            case ArrayOutOfBoundsMode::SILENT: {
                // Apply index clamping and update our LOCAL copy
                llvm::Value *clamped_index = builder->CreateSub(current_dim_length, builder->getInt64(1), "clamped_index");
                IR::aligned_store(*builder, clamped_index, index_ptr); // Update our local copy
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
    llvm::Value *index_to_use = IR::aligned_load(*builder, builder->getInt64Ty(), index_ptr, "index_after_bounds_check");

    // load stride
    llvm::Value *current_stride = IR::aligned_load(*builder, builder->getInt64Ty(), stride_ptr, "stride");

    // offset += index * stride;
    llvm::Value *current_offset = IR::aligned_load(*builder, builder->getInt64Ty(), offset_ptr, "offset");
    llvm::Value *index_times_stride = builder->CreateMul(index_to_use, current_stride, "index_times_stride");
    llvm::Value *new_offset = builder->CreateAdd(current_offset, index_times_stride, "new_offset");
    IR::aligned_store(*builder, new_offset, offset_ptr);

    // stride *= dim_lengths[i];
    llvm::Value *new_stride = builder->CreateMul(current_stride, current_dim_length, "new_stride");
    IR::aligned_store(*builder, new_stride, stride_ptr);

    builder->CreateBr(continue_block);

    // Continue loop - increment counter
    builder->SetInsertPoint(continue_block);
    llvm::Value *next_counter = builder->CreateAdd(current_counter, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter, counter_ptr);
    builder->CreateBr(loop_block);

    // Exit block - calculate final pointer
    builder->SetInsertPoint(exit_block);

    // Calculate the pointer to the data start
    // char *data_start = (char *)(dim_lengths + dimensionality);
    llvm::Value *dim_lengths_offset = builder->CreateGEP(builder->getInt64Ty(), dim_lengths, dimensionality, "dim_lengths_offset");
    llvm::Value *data_start = builder->CreateBitCast(dim_lengths_offset, builder->getInt8Ty()->getPointerTo(), "data_start");

    // Calculate the final offset: data_start + offset * element_size
    llvm::Value *final_offset = IR::aligned_load(*builder, builder->getInt64Ty(), offset_ptr, "final_offset");
    llvm::Value *byte_offset = builder->CreateMul(final_offset, arg_element_size, "byte_offset");
    llvm::Value *result_ptr = builder->CreateGEP(builder->getInt8Ty(), data_start, byte_offset, "result_ptr");

    // Return the pointer
    builder->CreateRet(result_ptr);
}

void Generator::Module::Array::generate_access_arr_val_function( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Module *module,                                        //
    const bool only_declarations                                 //
) {
    // THE C IMPLEMENTATION:
    // size_t access_arr_val(str *arr, const size_t element_size, const size_t *indices) {
    //     char *element = access_arr(arr, element_size, indices);
    //     size_t value;
    //     memcpy(&value, element, element_size);
    //     return value;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *access_arr_fn = array_manip_functions.at("access_arr");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *access_arr_val_type = llvm::FunctionType::get( //
        llvm::Type::getInt64Ty(context),                               // Return type: size_t
        {
            str_type->getPointerTo(),                       // Argument str* arr
            llvm::Type::getInt64Ty(context),                // Argument size_t element_size
            llvm::Type::getInt64Ty(context)->getPointerTo() // Argument size_t* indices
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
    llvm::Value *loaded_value = IR::aligned_load(*builder, builder->getInt64Ty(), value, "value");
    builder->CreateRet(loaded_value);
}

void Generator::Module::Array::generate_assign_arr_at_function( //
    llvm::IRBuilder<> *builder,                                 //
    llvm::Module *module,                                       //
    const bool only_declarations                                //
) {
    // THE C IMPLEMENTATION:
    // void assign_arr_at(str *arr, const size_t element_size, const size_t *indices, const void *value) {
    //     char *element = access_arr(arr, element_size, indices);
    //     memcpy(element, value, element_size);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *access_arr_fn = array_manip_functions.at("access_arr");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *assign_arr_at_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                               // Return type: void
        {
            str_type->getPointerTo(),                        // Argument str* arr
            llvm::Type::getInt64Ty(context),                 // Argument size_t element_size
            llvm::Type::getInt64Ty(context)->getPointerTo(), // Argument size_t* indices
            llvm::Type::getVoidTy(context)->getPointerTo()   // Argument: void* value
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

void Generator::Module::Array::generate_assign_arr_val_at_function( //
    llvm::IRBuilder<> *builder,                                     //
    llvm::Module *module,                                           //
    const bool only_declarations                                    //
) {
    // THE C IMPLEMENTATION:
    // void assign_arr_val_at(str *arr, const size_t element_size, const size_t *indices, const size_t value) {
    //     char *element = access_arr(arr, element_size, indices);
    //     memcpy(element, &value, element_size);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *access_arr_fn = array_manip_functions.at("access_arr");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *assign_arr_val_at_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                                   // Return type: void
        {
            str_type->getPointerTo(),                        // Argument str* arr
            llvm::Type::getInt64Ty(context),                 // Argument size_t element_size
            llvm::Type::getInt64Ty(context)->getPointerTo(), // Argument size_t* indices
            llvm::Type::getInt64Ty(context)                  // Argument: size_t value
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
    IR::aligned_store(*builder, arg_value, val);
    builder->CreateCall(memcpy_fn, {access_result, val, arg_element_size});

    builder->CreateRetVoid();
}

void Generator::Module::Array::generate_free_arr_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION: void free_arr(str * arr, size_t complexity) {
    //     if (complexity == 0) {
    //         free(arr);
    //         return;
    //     }
    //     size_t dimensionality = arr->len;
    //     size_t *lens = (size_t *)arr->value;
    //     size_t length = 1;
    //     for (size_t i = 0; i < dimensionality; i++) {
    //         length *= lens[i];
    //     }
    //     str **fields = (str **)(lens + dimensionality);
    //     size_t next_complexity = complexity - 1;
    //     for (size_t i = 0; i < length; i++) {
    //         free_arr(fields[i], next_complexity);
    //     }
    //     free(arr);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *free_fn = c_functions.at(FREE);

    llvm::FunctionType *free_arr_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                          // Return type: void
        {
            str_type->getPointerTo(),       // Argument str* arr
            llvm::Type::getInt64Ty(context) // Argument size_t complexity
        },
        false // No vaargs
    );
    llvm::Function *free_arr_fn = llvm::Function::Create(free_arr_type, llvm::Function::ExternalLinkage, "__flint_free_arr", module);
    array_manip_functions["free_arr"] = free_arr_fn;
    if (only_declarations) {
        return;
    }

    // Get the function arguments
    llvm::Argument *arg_arr = free_arr_fn->arg_begin();
    arg_arr->setName("arr");

    llvm::Argument *arg_complexity = free_arr_fn->arg_begin() + 1;
    arg_complexity->setName("complexity");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", free_arr_fn);
    llvm::BasicBlock *complexity_zero_block = llvm::BasicBlock::Create(context, "complexity_zero", free_arr_fn);
    llvm::BasicBlock *complexity_nonzero_block = llvm::BasicBlock::Create(context, "complexity_nonzero", free_arr_fn);
    llvm::BasicBlock *loop1_entry = llvm::BasicBlock::Create(context, "loop1_entry", free_arr_fn);
    llvm::BasicBlock *loop1_body = llvm::BasicBlock::Create(context, "loop1_body", free_arr_fn);
    llvm::BasicBlock *loop1_exit = llvm::BasicBlock::Create(context, "loop1_exit", free_arr_fn);
    llvm::BasicBlock *loop2_entry = llvm::BasicBlock::Create(context, "loop2_entry", free_arr_fn);
    llvm::BasicBlock *loop2_body = llvm::BasicBlock::Create(context, "loop2_body", free_arr_fn);
    llvm::BasicBlock *loop2_exit = llvm::BasicBlock::Create(context, "loop2_exit", free_arr_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", free_arr_fn);

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Check if complexity == 0
    llvm::Value *is_zero_complexity = builder->CreateICmpEQ(arg_complexity, builder->getInt64(0), "is_zero_complexity");
    builder->CreateCondBr(is_zero_complexity, complexity_zero_block, complexity_nonzero_block);

    // Complexity zero block - just free the array and return
    builder->SetInsertPoint(complexity_zero_block);
    // Cast str* to void* for free()
    llvm::Value *arr_void_ptr = builder->CreateBitCast(arg_arr, builder->getInt8Ty()->getPointerTo(), "arr_void_ptr");
    builder->CreateCall(free_fn, {arr_void_ptr});
    builder->CreateBr(exit_block);

    // Complexity nonzero block - recursive freeing of nested arrays
    builder->SetInsertPoint(complexity_nonzero_block);

    // Get dimensionality = arr->len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_arr, 0, "len_ptr");
    llvm::Value *dimensionality = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "dimensionality");

    // Get lens = (size_t *)arr->value
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_arr, 1, "value_ptr");
    llvm::Value *lens_size_t = builder->CreateBitCast(value_ptr, builder->getInt64Ty()->getPointerTo(), "lens_size_t");

    // Calculate total length
    llvm::Value *length_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "length_ptr");
    IR::aligned_store(*builder, builder->getInt64(1), length_ptr);

    // Create loop counter
    llvm::Value *counter1 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), counter1);

    // Branch to loop condition check
    builder->CreateBr(loop1_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop1_entry);
    llvm::Value *current_counter1 = IR::aligned_load(*builder, builder->getInt64Ty(), counter1, "current_counter");
    llvm::Value *cond1 = builder->CreateICmpULT(current_counter1, dimensionality, "loop1_cond");
    builder->CreateCondBr(cond1, loop1_body, loop1_exit);

    // Loop body
    builder->SetInsertPoint(loop1_body);

    // Load the current dimension length: lens[i]
    llvm::Value *dim_length_ptr = builder->CreateGEP(builder->getInt64Ty(), lens_size_t, current_counter1, "dim_length_ptr");
    llvm::Value *current_length = IR::aligned_load(*builder, builder->getInt64Ty(), dim_length_ptr, "current_length");

    // length *= lens[i]
    llvm::Value *current_total = IR::aligned_load(*builder, builder->getInt64Ty(), length_ptr, "current_total");
    llvm::Value *new_total = builder->CreateMul(current_total, current_length, "new_total");
    IR::aligned_store(*builder, new_total, length_ptr);

    // Increment counter
    llvm::Value *next_counter1 = builder->CreateAdd(current_counter1, builder->getInt64(1), "next_counter");
    IR::aligned_store(*builder, next_counter1, counter1);

    // Jump back to condition check
    builder->CreateBr(loop1_entry);

    // After calculating total length
    builder->SetInsertPoint(loop1_exit);
    llvm::Value *total_length = IR::aligned_load(*builder, builder->getInt64Ty(), length_ptr, "total_length");

    // Calculate fields = (str **)(lens + dimensionality)
    // This gives us a pointer to the array of str pointers
    llvm::Value *lens_offset = builder->CreateGEP(builder->getInt64Ty(), lens_size_t, dimensionality, "lens_offset");
    llvm::Value *fields = builder->CreateBitCast(lens_offset, str_type->getPointerTo()->getPointerTo(), "fields");

    // Calculate next_complexity = complexity - 1
    llvm::Value *next_complexity = builder->CreateSub(arg_complexity, builder->getInt64(1), "next_complexity");

    // Create loop counter
    llvm::Value *counter2 = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "j");
    IR::aligned_store(*builder, builder->getInt64(0), counter2);

    // Branch to loop condition check
    builder->CreateBr(loop2_entry);

    // Loop entry (condition check)
    builder->SetInsertPoint(loop2_entry);
    llvm::Value *current_counter2 = IR::aligned_load(*builder, builder->getInt64Ty(), counter2, "current_counter2");
    llvm::Value *cond2 = builder->CreateICmpULT(current_counter2, total_length, "loop2_cond");
    builder->CreateCondBr(cond2, loop2_body, loop2_exit);

    // Loop body
    builder->SetInsertPoint(loop2_body);

    // Get current field: fields[i]
    llvm::Value *field_ptr = builder->CreateGEP(str_type->getPointerTo(), fields, current_counter2, "field_ptr");
    llvm::Value *field = IR::aligned_load(*builder, str_type->getPointerTo(), field_ptr, "field");

    // Call free_arr recursively: free_arr(fields[i], next_complexity)
    builder->CreateCall(free_arr_fn, {field, next_complexity});

    // Increment counter
    llvm::Value *next_counter2 = builder->CreateAdd(current_counter2, builder->getInt64(1), "next_counter2");
    IR::aligned_store(*builder, next_counter2, counter2);

    // Jump back to condition check
    builder->CreateBr(loop2_entry);

    // After freeing all nested arrays
    builder->SetInsertPoint(loop2_exit);

    // Finally, free the array itself
    llvm::Value *arr_to_free = builder->CreateBitCast(arg_arr, builder->getInt8Ty()->getPointerTo(), "arr_to_free");
    builder->CreateCall(free_fn, {arr_to_free});
    builder->CreateBr(exit_block);

    // Exit block - return void
    builder->SetInsertPoint(exit_block);
    builder->CreateRetVoid();
}

void Generator::Module::Array::generate_get_arr_slice_1d_function( //
    llvm::IRBuilder<> *builder,                                    //
    llvm::Module *module,                                          //
    const bool only_declarations                                   //
) {
    // THE C IMPLEMENTATION:
    // str *get_arr_slice_1d(const str *src, const size_t element_size, const size_t from, const size_t to) {
    //     const size_t src_len = *(size_t *)src->value;
    //     size_t real_to = to == 0 ? src_len : to;
    //     if (real_to > src_len) {
    //         // Print error to tell a oob-slicing attempt was done and clamp to the src len
    //         // Because slicing is technically an array operation, the array OOB options apply here
    //         // So this could be a hard crash, silent, unsafe or verbose
    //         real_to = src_len;
    //     }
    //     if (from == real_to) {
    //         // If real_to is equal to from, this block hard crashes no matter the setting, as we
    //         // would now handle with an explicit 'x..x' range, which is not allowed, since the range does not contain a single element
    //         abort();
    //     }
    //     size_t real_from = from;
    //     if (from > real_to) {
    //         if (real_to == 0) {
    //             // This is a hard crash scenario, as 'from' would potentially be < 0 which is undefined for indexing
    //             abort();
    //         }
    //         // The lower bound of the range was above or equal to the upper bound of the range
    //         // The from value will get clamped to the value (real_to - 1)
    //         // As above, the OOB options apply to whether a print, crash etc is done in here
    //         real_from = real_to - 1;
    //     }
    //     const size_t len = real_to - real_from;
    //     str *slice = create_arr(1, element_size, &len);
    //     char *dest_ptr = (char *)(((size_t *)slice->value) + 1);
    //     char *src_ptr = (char *)(((size_t *)src->value) + 1);
    //     memcpy(dest_ptr, src_ptr + (real_from * element_size), len * element_size);
    //     return slice;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *printf_fn = c_functions.at(PRINTF);
    llvm::Function *abort_fn = c_functions.at(ABORT);
    llvm::Function *create_arr_fn = array_manip_functions.at("create_arr");

    llvm::FunctionType *get_arr_slice_1d_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                        // Return Type: str*
        {
            str_type->getPointerTo(),        // Argument: str* src
            llvm::Type::getInt64Ty(context), // Argument: u64 element_size
            llvm::Type::getInt64Ty(context), // Argument: u64 from
            llvm::Type::getInt64Ty(context)  // Argument: u64 to
        },                                   //
        false                                // No varargs
    );
    llvm::Function *get_arr_slice_1d_fn = llvm::Function::Create( //
        get_arr_slice_1d_type,                                    //
        llvm::Function::ExternalLinkage,                          //
        "__flint_get_arr_slice_1d",                               //
        module                                                    //
    );
    array_manip_functions["get_arr_slice_1d"] = get_arr_slice_1d_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_arr_slice_1d_fn);
    llvm::BasicBlock *end_oob_block = nullptr;
    llvm::BasicBlock *end_oob_merge_block = nullptr;
    if (oob_mode != ArrayOutOfBoundsMode::UNSAFE) {
        end_oob_block = llvm::BasicBlock::Create(context, "end_oob", get_arr_slice_1d_fn);
        end_oob_merge_block = llvm::BasicBlock::Create(context, "end_oob_merge", get_arr_slice_1d_fn);
    }
    llvm::BasicBlock *range_empty_block = llvm::BasicBlock::Create(context, "range_empty", get_arr_slice_1d_fn);
    llvm::BasicBlock *range_empty_merge_block = llvm::BasicBlock::Create(context, "range_empty_merge", get_arr_slice_1d_fn);
    llvm::BasicBlock *from_gt_to_block = nullptr;
    llvm::BasicBlock *real_to_eq_0_block = nullptr;
    llvm::BasicBlock *real_to_eq_0_merge_block = nullptr;
    llvm::BasicBlock *from_gt_to_merge_block = nullptr;
    if (oob_mode != ArrayOutOfBoundsMode::UNSAFE) {
        from_gt_to_block = llvm::BasicBlock::Create(context, "from_gt_to", get_arr_slice_1d_fn);
        if (oob_mode != ArrayOutOfBoundsMode::CRASH) {
            real_to_eq_0_block = llvm::BasicBlock::Create(context, "real_to_eq_0", get_arr_slice_1d_fn);
            real_to_eq_0_merge_block = llvm::BasicBlock::Create(context, "real_to_eq_0_merge", get_arr_slice_1d_fn);
        }
        from_gt_to_merge_block = llvm::BasicBlock::Create(context, "from_gt_to_merge", get_arr_slice_1d_fn);
    }
    builder->SetInsertPoint(entry_block);

    // Get the src argument
    llvm::Argument *arg_src = get_arr_slice_1d_fn->arg_begin();
    arg_src->setName("src");

    // Get the element_size argument
    llvm::Argument *arg_element_size = get_arr_slice_1d_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");

    // Get the from argument
    llvm::Argument *arg_from = get_arr_slice_1d_fn->arg_begin() + 2;
    arg_from->setName("from");

    // Get the to argument
    llvm::Argument *arg_to = get_arr_slice_1d_fn->arg_begin() + 3;
    arg_to->setName("to");

    builder->SetInsertPoint(entry_block);
    llvm::Value *to_eq_0 = builder->CreateICmpEQ(arg_to, builder->getInt64(0), "to_eq_0");
    // For arrays, the length is stored as the first element in the value array: *(size_t *)src->value
    llvm::Value *src_value_ptr = builder->CreateStructGEP(str_type, arg_src, 1, "src_value_ptr");
    llvm::Value *src_len_ptr = builder->CreateBitCast(src_value_ptr, builder->getInt64Ty()->getPointerTo(), "src_len_ptr");
    llvm::Value *src_len = IR::aligned_load(*builder, builder->getInt64Ty(), src_len_ptr, "src_len");
    llvm::Value *real_to = builder->CreateSelect(to_eq_0, src_len, arg_to, "real_to");

    // if (real_to > src_len) { ... }
    if (oob_mode != ArrayOutOfBoundsMode::UNSAFE) {
        llvm::Value *real_to_gt_src_len = builder->CreateICmpUGT(real_to, src_len, "real_to_gt_src_len");
        builder->CreateCondBr(real_to_gt_src_len, end_oob_block, end_oob_merge_block, IR::generate_weights(1, 100));
        builder->SetInsertPoint(end_oob_block);
        if (oob_mode != ArrayOutOfBoundsMode::SILENT) {
            llvm::Value *msg = IR::generate_const_string(module, "OOB ranged array access: len=%lu, upper_bound=%lu\n");
            builder->CreateCall(printf_fn, {msg, src_len, real_to});
        }
        if (oob_mode == ArrayOutOfBoundsMode::CRASH) {
            builder->CreateCall(abort_fn);
            builder->CreateUnreachable();
        } else {
            builder->CreateBr(end_oob_merge_block);
        }
        builder->SetInsertPoint(end_oob_merge_block);
        if (oob_mode != ArrayOutOfBoundsMode::CRASH) {
            // We only need a phi node if we do not crash here
            llvm::PHINode *to_selection = builder->CreatePHI(builder->getInt64Ty(), 2, "real_to_phi");
            to_selection->addIncoming(real_to, entry_block);
            to_selection->addIncoming(src_len, end_oob_block);
            real_to = to_selection;
        }
    }

    // if (from == real_to) { ... }
    {
        llvm::Value *is_range_empty = builder->CreateICmpEQ(arg_from, real_to, "is_range_empty");
        builder->CreateCondBr(is_range_empty, range_empty_block, range_empty_merge_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(range_empty_block);
        llvm::Value *msg = IR::generate_const_string(module, "Cannot get empty slice %lu..%lu from array\n");
        builder->CreateCall(printf_fn, {msg, arg_from, real_to});
        builder->CreateCall(abort_fn);
        builder->CreateUnreachable();

        builder->SetInsertPoint(range_empty_merge_block);
    }

    // if (from > real_to) { ... }
    llvm::Value *real_from = arg_from;
    if (oob_mode != ArrayOutOfBoundsMode::UNSAFE) {
        llvm::Value *from_gt_to = builder->CreateICmpUGT(arg_from, real_to, "from_gt_to");
        builder->CreateCondBr(from_gt_to, from_gt_to_block, from_gt_to_merge_block, IR::generate_weights(1, 100));

        builder->SetInsertPoint(from_gt_to_block);
        if (oob_mode != ArrayOutOfBoundsMode::SILENT) {
            llvm::Value *msg = IR::generate_const_string(module, "Array slice lower bound greater than upper bound\n");
            builder->CreateCall(printf_fn, {msg});
        }
        if (oob_mode == ArrayOutOfBoundsMode::CRASH) {
            builder->CreateCall(abort_fn);
            builder->CreateUnreachable();
        } else {
            // if (real_to == 0) { ... }
            {
                llvm::Value *real_to_eq_0 = builder->CreateICmpEQ(real_to, builder->getInt64(0), "real_to_eq_0");
                builder->CreateCondBr(real_to_eq_0, real_to_eq_0_block, real_to_eq_0_merge_block, IR::generate_weights(1, 100));

                builder->SetInsertPoint(real_to_eq_0_block);
                if (oob_mode != ArrayOutOfBoundsMode::SILENT) {
                    llvm::Value *msg = IR::generate_const_string(module, "Upper bound is 0, lower bound cannot be lowered any further\n");
                    builder->CreateCall(printf_fn, {msg});
                }
                builder->CreateCall(abort_fn);
                builder->CreateUnreachable();

                builder->SetInsertPoint(real_to_eq_0_merge_block);
                if (oob_mode != ArrayOutOfBoundsMode::SILENT) {
                    llvm::Value *msg = IR::generate_const_string(module, "Clamping lower bound to be (to - 1)\n");
                    builder->CreateCall(printf_fn, {msg});
                }
                real_from = builder->CreateSub(real_to, builder->getInt64(1), "real_from");
                builder->CreateBr(from_gt_to_merge_block);
            }
        }

        builder->SetInsertPoint(from_gt_to_merge_block);
        if (oob_mode != ArrayOutOfBoundsMode::CRASH) {
            llvm::PHINode *from_select = builder->CreatePHI(builder->getInt64Ty(), 2, "from_select");
            from_select->addIncoming(arg_from, range_empty_merge_block);
            from_select->addIncoming(real_from, real_to_eq_0_merge_block);
            real_from = from_select;
        }
    }

    // The actual slicing
    llvm::Value *len = builder->CreateSub(real_to, real_from, "len");
    // create_arr(1, element_size, &len) - we need to pass len by reference
    llvm::Value *len_ptr = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "len_ptr");
    IR::aligned_store(*builder, len, len_ptr);
    llvm::Value *dimensionality = builder->getInt64(1);
    llvm::Value *result = builder->CreateCall(create_arr_fn, {dimensionality, arg_element_size, len_ptr}, "result");

    // char *dest_ptr = (char *)(((size_t *)slice->value) + 1);
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");
    llvm::Value *result_size_ptr = builder->CreateBitCast(result_value_ptr, builder->getInt64Ty()->getPointerTo(), "result_size_ptr");
    llvm::Value *result_data_ptr = builder->CreateGEP(builder->getInt64Ty(), result_size_ptr, {builder->getInt64(1)}, "result_data_ptr");
    llvm::Value *dest_ptr = builder->CreateBitCast(result_data_ptr, builder->getInt8Ty()->getPointerTo(), "dest_ptr");

    // char *src_ptr = (char *)(((size_t *)src->value) + 1);
    llvm::Value *src_size_ptr = builder->CreateBitCast(src_value_ptr, builder->getInt64Ty()->getPointerTo(), "src_size_ptr");
    llvm::Value *src_data_ptr = builder->CreateGEP(builder->getInt64Ty(), src_size_ptr, {builder->getInt64(1)}, "src_data_ptr");
    llvm::Value *src_ptr = builder->CreateBitCast(src_data_ptr, builder->getInt8Ty()->getPointerTo(), "src_ptr");

    // src_ptr + (real_from * element_size)
    llvm::Value *offset_bytes = builder->CreateMul(real_from, arg_element_size, "offset_bytes");
    llvm::Value *src_offset_ptr = builder->CreateGEP(builder->getInt8Ty(), src_ptr, {offset_bytes}, "src_offset_ptr");

    // len * element_size
    llvm::Value *copy_bytes = builder->CreateMul(len, arg_element_size, "copy_bytes");

    builder->CreateCall(memcpy_fn, {dest_ptr, src_offset_ptr, copy_bytes});
    builder->CreateRet(result);
}

void Generator::Module::Array::generate_get_arr_slice_function( //
    llvm::IRBuilder<> *builder,                                 //
    llvm::Module *module,                                       //
    const bool only_declarations                                //
) {
    // THE C IMPLEMENTATION:
    // str *get_arr_slice(const str *src, const size_t element_size, const size_t *ranges) {
    //     const size_t src_dimensionality = src->len;
    //     size_t *src_dim_lengths = (size_t *)src->value;
    //
    //     // First, validate ranges and count new dimensionality
    //     size_t new_dimensionality = 0;
    //     for (size_t i = 0; i < src_dimensionality; i++) {
    //         const size_t from = ranges[i * 2];
    //         const size_t to = ranges[i * 2 + 1];
    //         if (from != to) {
    //             // Validate range bounds
    //             if (to > src_dim_lengths[i]) {
    //                 // Out of bounds range
    //                 return NULL;
    //             } else if (to - from < 2) {
    //                 // "Range" is less than 2, so it's actually a single value, which means that the dimensionality would decrase. But in
    //                 // that case not a range but a single value should have been provided instead
    //                 return NULL;
    //             }
    //             new_dimensionality++;
    //         } else {
    //             // Validate single index
    //             if (from >= src_dim_lengths[i]) {
    //                 // Index out of bounds
    //                 return NULL;
    //             }
    //         }
    //     }
    //     assert(new_dimensionality > 0);
    //     const bool is_first_range = ranges[0] != ranges[1];
    //     if (src_dimensionality == 1 && new_dimensionality == 1) {
    //         assert(is_first_range);
    //         return get_arr_slice_1d(src, element_size, ranges[0], ranges[1]);
    //     }
    //
    //     // Calculate new dimension lengths for ranges only
    //     size_t *new_dim_lengths = (size_t *)malloc(new_dimensionality * sizeof(size_t));
    //     size_t new_dim_index = 0;
    //     for (size_t i = 0; i < src_dimensionality; i++) {
    //         const size_t from = ranges[i * 2];
    //         const size_t to = ranges[i * 2 + 1];
    //         if (from != to) {
    //             new_dim_lengths[new_dim_index] = to - from;
    //             new_dim_index++;
    //         }
    //     }
    //
    //     // Create the new sliced array
    //     str *result = create_arr(new_dimensionality, element_size, new_dim_lengths);
    //     char *src_data = (char *)(src_dim_lengths + src_dimensionality);
    //     char *dest_data = (char *)((size_t *)result->value + new_dimensionality);
    //
    //     // Calculate strides for each dimension in source array
    //     size_t *src_strides = (size_t *)malloc(src_dimensionality * sizeof(size_t));
    //     src_strides[0] = 1;
    //     for (size_t i = 1; i < src_dimensionality; i++) {
    //         src_strides[i] = src_strides[i - 1] * src_dim_lengths[i - 1];
    //     }
    //
    //     // Calculate total elements in the result
    //     size_t total_result_elements = 1;
    //     for (size_t i = 0; i < new_dimensionality; i++) {
    //         total_result_elements *= new_dim_lengths[i];
    //     }
    //
    //     // Copy elements from source to destination
    //     size_t dest_index = 0;
    //
    //     // We need to iterate through all combinations of the ranges
    //     // This is a recursive-like problem, but we'll do it iteratively
    //     size_t *current_indices = (size_t *)malloc(src_dimensionality * sizeof(size_t));
    //
    //     // Initialize indices to the start of each range/index
    //     for (size_t i = 0; i < src_dimensionality; i++) {
    //         current_indices[i] = ranges[i * 2];
    //     }
    //
    //     // Check whether the first indexing element is a range
    //     size_t chunk_size = 1;
    //     if (is_first_range) {
    //         chunk_size = ranges[1] - ranges[0];
    //     }
    //
    //     // Calculate how many chunks we need (total elements divided by chunk size)
    //     assert(total_result_elements % chunk_size == 0);
    //     size_t num_chunks = total_result_elements / chunk_size;
    //     for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    //         // Calculate source offset
    //         size_t src_offset = 0;
    //         for (size_t i = 0; i < src_dimensionality; i++) {
    //             src_offset += current_indices[i] * src_strides[i];
    //         }
    //
    //         // Copy the chunk (either 1 element or chunk_size elements)
    //         memcpy(dest_data + dest_index * element_size, src_data + src_offset * element_size, chunk_size * element_size);
    //         dest_index += chunk_size;
    //
    //         // Increment indices - skip the first dimension if it's a range since we copied the whole chunk
    //         bool carry = true;
    //         size_t start_dim = (size_t)is_first_range;
    //
    //         for (size_t i = src_dimensionality - 1; i >= start_dim && carry; i--) {
    //             const size_t from = ranges[i * 2];
    //             const size_t to = ranges[i * 2 + 1];
    //             if (from != to) {
    //                 current_indices[i]++;
    //                 if (current_indices[i] < to) {
    //                     carry = false;
    //                 } else {
    //                     current_indices[i] = from;
    //                 }
    //             }
    //         }
    //     }
    //
    //     // Clean up
    //     free(new_dim_lengths);
    //     free(src_strides);
    //     free(current_indices);
    //
    //     return result;
    // }
    llvm::Type *const str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Type *const i64_ty = llvm::Type::getInt64Ty(context);
    llvm::Function *malloc_fn = c_functions.at(MALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *printf_fn = c_functions.at(PRINTF);
    llvm::Function *abort_fn = c_functions.at(ABORT);
    llvm::Function *create_arr_fn = array_manip_functions.at("create_arr");
    llvm::Function *get_arr_slice_1d_fn = array_manip_functions.at("get_arr_slice_1d");

    llvm::FunctionType *get_arr_slice_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                     // Return Type: str*
        {
            str_type->getPointerTo(), // Argument: str* src
            i64_ty,                   // Argument: u64 element_size
            i64_ty->getPointerTo()    // Argument: u64* ranges
        },                            //
        false                         // No varargs
    );
    llvm::Function *get_arr_slice_fn = llvm::Function::Create( //
        get_arr_slice_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_get_arr_slice",                               //
        module                                                 //
    );
    array_manip_functions["get_arr_slice"] = get_arr_slice_fn;
    if (only_declarations) {
        return;
    }

    // Create the basic blocks for the function
    llvm::BasicBlock *const entry_block = llvm::BasicBlock::Create(context, "entry", get_arr_slice_fn);
    // for (size_t i = 0; i < src_dimensionality; i++) {
    llvm::BasicBlock *const get_dim_loop_cond_block = llvm::BasicBlock::Create(context, "get_dim_loop_cond", get_arr_slice_fn);
    llvm::BasicBlock *const get_dim_loop_body_block = llvm::BasicBlock::Create(context, "get_dim_loop_body", get_arr_slice_fn);
    //     if (from != to) {
    llvm::BasicBlock *const get_dim_loop_is_range_block = llvm::BasicBlock::Create(context, "get_dim_loop_is_range", get_arr_slice_fn);
    //         if (to > src_dim_lengths[i]) {
    llvm::BasicBlock *const get_dim_loop_is_range_is_oob_block =
        llvm::BasicBlock::Create(context, "get_dim_loop_is_range_is_oob", get_arr_slice_fn);
    llvm::BasicBlock *const get_dim_loop_is_range_is_not_oob_block =
        llvm::BasicBlock::Create(context, "get_dim_loop_is_range_is_not_oob", get_arr_slice_fn);
    //         } else if (to - from < 2) {
    llvm::BasicBlock *const get_dim_loop_is_range_is_empty_block =
        llvm::BasicBlock::Create(context, "get_dim_loop_is_range_is_empty", get_arr_slice_fn);
    //         }
    llvm::BasicBlock *const get_dim_loop_is_range_merge_block =
        llvm::BasicBlock::Create(context, "get_dim_loop_is_range_merge", get_arr_slice_fn);
    //     } else {
    llvm::BasicBlock *const get_dim_loop_is_no_range_block =
        llvm::BasicBlock::Create(context, "get_dim_loop_is_no_range", get_arr_slice_fn);
    llvm::BasicBlock *const get_dim_loop_is_no_range_is_oob_block =
        llvm::BasicBlock::Create(context, "get_dim_loop_is_no_range_is_oob", get_arr_slice_fn);
    //     }
    llvm::BasicBlock *const get_dim_loop_action_block = llvm::BasicBlock::Create(context, "get_dim_loop_action", get_arr_slice_fn);
    // }
    llvm::BasicBlock *const get_dim_loop_merge_block = llvm::BasicBlock::Create(context, "get_dim_loop_merge", get_arr_slice_fn);
    // if (src_dimensionality == 1 && new_dimensionality == 1) {
    llvm::BasicBlock *const is_1d_slice_block = llvm::BasicBlock::Create(context, "is_1d_slice", get_arr_slice_fn);
    // }
    llvm::BasicBlock *const is_1d_slice_merge_block = llvm::BasicBlock::Create(context, "is_1d_slice_merge", get_arr_slice_fn);
    // for (size_t i = 0; i < src_dimensionality; i++) {
    llvm::BasicBlock *const new_dim_loop_cond_block = llvm::BasicBlock::Create(context, "new_dim_loop_cond", get_arr_slice_fn);
    llvm::BasicBlock *const new_dim_loop_body_block = llvm::BasicBlock::Create(context, "new_dim_loop_body", get_arr_slice_fn);
    //     if (from != to) {
    llvm::BasicBlock *const new_dim_loop_is_range_block = llvm::BasicBlock::Create(context, "new_dim_loop_is_range", get_arr_slice_fn);
    //     }
    llvm::BasicBlock *const new_dim_loop_action_block = llvm::BasicBlock::Create(context, "new_dim_loop_action", get_arr_slice_fn);
    // }
    llvm::BasicBlock *const new_dim_loop_merge_block = llvm::BasicBlock::Create(context, "new_dim_loop_merge_block", get_arr_slice_fn);
    // for (size_t i = 1; i < src_dimensionality; i++) {
    llvm::BasicBlock *const strides_loop_cond_block = llvm::BasicBlock::Create(context, "strides_loop_cond", get_arr_slice_fn);
    llvm::BasicBlock *const strides_loop_body_block = llvm::BasicBlock::Create(context, "strides_loop_body", get_arr_slice_fn);
    llvm::BasicBlock *const strides_loop_action_block = llvm::BasicBlock::Create(context, "strides_loop_action", get_arr_slice_fn);
    // }
    llvm::BasicBlock *const strides_loop_merge_block = llvm::BasicBlock::Create(context, "strides_loop_merge", get_arr_slice_fn);
    // for (size_t i = 0; i < new_dimensionality; i++) {
    llvm::BasicBlock *const tre_loop_cond_block = llvm::BasicBlock::Create(context, "tre_loop_cond", get_arr_slice_fn);
    llvm::BasicBlock *const tre_loop_body_block = llvm::BasicBlock::Create(context, "tre_loop_body", get_arr_slice_fn);
    llvm::BasicBlock *const tre_loop_action_block = llvm::BasicBlock::Create(context, "tre_loop_action", get_arr_slice_fn);
    // }
    llvm::BasicBlock *const tre_loop_merge_block = llvm::BasicBlock::Create(context, "tre_loop_merge", get_arr_slice_fn);
    // for (size_t i = 0; i < src_dimensionality; i++) {
    llvm::BasicBlock *const idx_init_loop_cond_block = llvm::BasicBlock::Create(context, "idx_init_loop_cond", get_arr_slice_fn);
    llvm::BasicBlock *const idx_init_loop_body_block = llvm::BasicBlock::Create(context, "idx_init_loop_body", get_arr_slice_fn);
    llvm::BasicBlock *const idx_init_loop_action_block = llvm::BasicBlock::Create(context, "idx_init_loop_action", get_arr_slice_fn);
    // }
    llvm::BasicBlock *const idx_init_loop_merge_block = llvm::BasicBlock::Create(context, "idx_init_loop_merge", get_arr_slice_fn);
    // for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    llvm::BasicBlock *const chunk_loop_cond_block = llvm::BasicBlock::Create(context, "chunk_loop_cond_block", get_arr_slice_fn);
    llvm::BasicBlock *const chunk_loop_body_block = llvm::BasicBlock::Create(context, "chunk_loop_body", get_arr_slice_fn);
    //     for (size_t i = 0; i < src_dimensionality; i++) {
    llvm::BasicBlock *const chunk_loop_offset_loop_cond_block =
        llvm::BasicBlock::Create(context, "chunk_loop_offset_loop_cond", get_arr_slice_fn);
    llvm::BasicBlock *const chunk_loop_offset_loop_body_block =
        llvm::BasicBlock::Create(context, "chunk_loop_offset_loop_body", get_arr_slice_fn);
    llvm::BasicBlock *const chunk_loop_offset_loop_action_block =
        llvm::BasicBlock::Create(context, "chunk_loop_offset_loop_action", get_arr_slice_fn);
    //     }
    llvm::BasicBlock *const chunk_loop_offset_loop_merge_block =
        llvm::BasicBlock::Create(context, "chunk_loop_offset_loop_merge", get_arr_slice_fn);
    //     for (size_t i = src_dimensionality - 1; i >= start_dim; i--) {
    llvm::BasicBlock *const chunk_loop_index_loop_cond_block =
        llvm::BasicBlock::Create(context, "chunk_loop_index_loop_cond", get_arr_slice_fn);
    llvm::BasicBlock *const chunk_loop_index_loop_body_block =
        llvm::BasicBlock::Create(context, "chunk_loop_index_loop_body", get_arr_slice_fn);
    //         if (from != to) {
    llvm::BasicBlock *const chunk_loop_index_loop_is_range_block =
        llvm::BasicBlock::Create(context, "chunk_loop_index_loop_is_range", get_arr_slice_fn);
    //             if (current_indices[i] < to) {
    llvm::BasicBlock *const chunk_loop_index_loop_is_range_is_past =
        llvm::BasicBlock::Create(context, "chunk_loop_index_loop_is_range_is_past", get_arr_slice_fn);
    //             } else {
    llvm::BasicBlock *const chunk_loop_index_loop_is_range_is_not_past =
        llvm::BasicBlock::Create(context, "chunk_loop_index_loop_is_range_is_not_past", get_arr_slice_fn);
    //             }
    //         }
    llvm::BasicBlock *const chunk_loop_index_loop_action_block =
        llvm::BasicBlock::Create(context, "chunk_loop_index_loop_action", get_arr_slice_fn);
    //     }
    llvm::BasicBlock *const chunk_loop_index_loop_merge_block =
        llvm::BasicBlock::Create(context, "chunk_loop_index_loop_merge", get_arr_slice_fn);
    llvm::BasicBlock *const chunk_loop_action_block = llvm::BasicBlock::Create(context, "chunk_loop_action", get_arr_slice_fn);
    // }
    llvm::BasicBlock *const chunk_loop_merge_block = llvm::BasicBlock::Create(context, "chunk_loop_merge", get_arr_slice_fn);

    // Get the src argument
    llvm::Argument *const arg_src = get_arr_slice_fn->arg_begin();
    arg_src->setName("src");

    // Get the element_size argument
    llvm::Argument *const arg_element_size = get_arr_slice_fn->arg_begin() + 1;
    arg_element_size->setName("element_size");

    // Get the from argument
    llvm::Argument *const arg_ranges = get_arr_slice_fn->arg_begin() + 2;
    arg_ranges->setName("ranges");

    builder->SetInsertPoint(entry_block);
    llvm::AllocaInst *const new_dimensionality = builder->CreateAlloca(i64_ty, 0, nullptr, "new_dimensionality");
    llvm::AllocaInst *const i = builder->CreateAlloca(i64_ty, 0, nullptr, "i");
    llvm::AllocaInst *const new_dim_index = builder->CreateAlloca(i64_ty, 0, nullptr, "new_dim_index");
    llvm::AllocaInst *const total_result_elements = builder->CreateAlloca(i64_ty, 0, nullptr, "total_result_elements");
    llvm::AllocaInst *const dest_index = builder->CreateAlloca(i64_ty, 0, nullptr, "dest_index");
    llvm::AllocaInst *const chunk = builder->CreateAlloca(i64_ty, 0, nullptr, "chunk");
    llvm::AllocaInst *const src_offset = builder->CreateAlloca(i64_ty, 0, nullptr, "src_offset");

    // const size_t src_dimensionality = src->len;
    llvm::Value *const src_dimensionality_ptr = builder->CreateStructGEP(str_type, arg_src, 0, "src_dimensionality_ptr");
    llvm::Value *const src_dimensionality = IR::aligned_load(*builder, i64_ty, src_dimensionality_ptr, "src_dimensionality");

    // const size_t *src_dim_lengths = (size_t *)src->value;
    llvm::Value *const src_dim_lengths_ptr = builder->CreateStructGEP(str_type, arg_src, 1, "src_dim_lengths_ptr");
    llvm::Value *const src_dim_lengths = builder->CreateBitCast(       //
        src_dim_lengths_ptr, i64_ty->getPointerTo(), "src_dim_legnths" //
    );

    IR::aligned_store(*builder, builder->getInt64(0), new_dimensionality);
    IR::aligned_store(*builder, builder->getInt64(0), i);
    builder->CreateBr(get_dim_loop_cond_block);
    // for (size_t i = 0; i < src_dimensionality; i++) { ... }
    {
        // CONDITION Block
        builder->SetInsertPoint(get_dim_loop_cond_block);
        llvm::Value *i_val = IR::aligned_load(*builder, i64_ty, i, "i_val");
        llvm::Value *i_lt_src_dimensionality = builder->CreateICmpULT(i_val, src_dimensionality, "i_lt_src_dimensionality");
        builder->CreateCondBr(i_lt_src_dimensionality, get_dim_loop_body_block, get_dim_loop_merge_block);

        // BODY Block
        builder->SetInsertPoint(get_dim_loop_body_block);
        llvm::Value *from_offset = builder->CreateMul(i_val, builder->getInt64(2), "from_offset");
        llvm::Value *from_ptr = builder->CreateGEP(i64_ty, arg_ranges, from_offset, "from_ptr");
        llvm::Value *from = IR::aligned_load(*builder, i64_ty, from_ptr, "from");
        llvm::Value *to_ptr = builder->CreateGEP(i64_ty, from_ptr, builder->getInt64(1), "to_offset");
        llvm::Value *to = IR::aligned_load(*builder, i64_ty, to_ptr, "to");
        llvm::Value *src_dim_lengths_i_ptr = builder->CreateGEP(i64_ty, src_dim_lengths, i_val, "src_dim_lengths_i_ptr");
        llvm::Value *src_dim_lengths_i = IR::aligned_load(*builder, i64_ty, src_dim_lengths_i_ptr, "src_dim_lengths_i");
        llvm::Value *is_range = builder->CreateICmpNE(from, to, "is_range");
        builder->CreateCondBr(is_range, get_dim_loop_is_range_block, get_dim_loop_is_no_range_block);

        // if (from != to) {
        {
            builder->SetInsertPoint(get_dim_loop_is_range_block);
            llvm::Value *to_gt_lengths_i = builder->CreateICmpUGT(to, src_dim_lengths_i, "to_gt_lengths_i");
            builder->CreateCondBr(                                                                                                        //
                to_gt_lengths_i, get_dim_loop_is_range_is_oob_block, get_dim_loop_is_range_is_not_oob_block, IR::generate_weights(1, 100) //
            );

            // if (to > src_dim_lengths[i]) {
            {
                builder->SetInsertPoint(get_dim_loop_is_range_is_oob_block);
                llvm::Value *msg = IR::generate_const_string(module, "OOB ranged array access: len=%lu, upper_bound=%lu\n");
                builder->CreateCall(printf_fn, {msg, src_dim_lengths_i, to});
                builder->CreateCall(abort_fn);
                builder->CreateUnreachable();
            }

            builder->SetInsertPoint(get_dim_loop_is_range_is_not_oob_block);
            llvm::Value *to_m_from = builder->CreateSub(to, from, "to_m_from");
            llvm::Value *to_m_from_lt_2 = builder->CreateICmpULT(to_m_from, builder->getInt64(2), "to_m_from_lt_2");
            builder->CreateCondBr(                                                                                                    //
                to_m_from_lt_2, get_dim_loop_is_range_is_empty_block, get_dim_loop_is_range_merge_block, IR::generate_weights(1, 100) //
            );

            // } else if (to - from < 2) {
            {
                builder->SetInsertPoint(get_dim_loop_is_range_is_empty_block);
                llvm::Value *msg = IR::generate_const_string(module, "Empty ranged array access\n");
                builder->CreateCall(printf_fn, {msg});
                builder->CreateCall(abort_fn);
                builder->CreateUnreachable();
            }

            builder->SetInsertPoint(get_dim_loop_is_range_merge_block);
            llvm::Value *nd_val = IR::aligned_load(*builder, i64_ty, new_dimensionality, "nd_val");
            llvm::Value *nd_val_p1 = builder->CreateAdd(nd_val, builder->getInt64(1), "nd_val_p1");
            IR::aligned_store(*builder, nd_val_p1, new_dimensionality);
            builder->CreateBr(get_dim_loop_action_block);
        }
        // } else {
        {
            builder->SetInsertPoint(get_dim_loop_is_no_range_block);
            llvm::Value *from_ge_lengths_i = builder->CreateICmpUGE(from, src_dim_lengths_i, "from_ge_lengths_i");
            builder->CreateCondBr(                                                                                                //
                from_ge_lengths_i, get_dim_loop_is_no_range_is_oob_block, get_dim_loop_action_block, IR::generate_weights(1, 100) //
            );

            // if (from >= src_dim_lengths[i]) {
            {
                builder->SetInsertPoint(get_dim_loop_is_no_range_is_oob_block);
                llvm::Value *msg = IR::generate_const_string(module, "OOB array access: len=%lu, upper_bound=%lu\n");
                builder->CreateCall(printf_fn, {msg, src_dim_lengths_i, from});
                builder->CreateCall(abort_fn);
                builder->CreateUnreachable();
            }
        }

        // ACTION Block
        builder->SetInsertPoint(get_dim_loop_action_block);
        llvm::Value *next_i_val = builder->CreateAdd(i_val, builder->getInt64(1), "next_i_val");
        IR::aligned_store(*builder, next_i_val, i);
        builder->CreateBr(get_dim_loop_cond_block);
    }

    builder->SetInsertPoint(get_dim_loop_merge_block);
    llvm::Value *const ranges_0 = IR::aligned_load(*builder, i64_ty, arg_ranges, "ranges_0");
    llvm::Value *const ranges_1_ptr = builder->CreateGEP(i64_ty, arg_ranges, builder->getInt64(1), "ranges_1_ptr");
    llvm::Value *const ranges_1 = IR::aligned_load(*builder, i64_ty, ranges_1_ptr, "ranges_1");
    llvm::Value *const is_first_range = builder->CreateICmpNE(ranges_0, ranges_1, "is_first_range");
    llvm::Value *src_dim_eq_1 = builder->CreateICmpEQ(src_dimensionality, builder->getInt64(1), "src_dim_eq_1");
    llvm::Value *new_dim_val = IR::aligned_load(*builder, i64_ty, new_dimensionality, "new_dim_val");
    llvm::Value *new_dim_eq_1 = builder->CreateICmpEQ(new_dim_val, builder->getInt64(1), "new_dim_eq_1");
    llvm::Value *both_dim_eq_1 = builder->CreateAnd(src_dim_eq_1, new_dim_eq_1, "both_dim_eq_1");
    builder->CreateCondBr(both_dim_eq_1, is_1d_slice_block, is_1d_slice_merge_block);

    // if (src_dimensionality == 1 && new_dimensionality == 1) {
    {
        builder->SetInsertPoint(is_1d_slice_block);
        llvm::Value *result = builder->CreateCall(get_arr_slice_1d_fn, {arg_src, arg_element_size, ranges_0, ranges_1}, "result");
        builder->CreateRet(result);
    }

    builder->SetInsertPoint(is_1d_slice_merge_block);
    llvm::Value *const new_dim_lengths_size = builder->CreateMul(new_dim_val, builder->getInt64(8), "new_dim_lengths_size");
    llvm::Value *const new_dim_lengths_ptr = builder->CreateCall(malloc_fn, {new_dim_lengths_size}, "new_dim_lengths_ptr");
    llvm::Value *const new_dim_lengths = builder->CreateBitCast(       //
        new_dim_lengths_ptr, i64_ty->getPointerTo(), "new_dim_lengths" //
    );

    IR::aligned_store(*builder, builder->getInt64(0), new_dim_index);
    IR::aligned_store(*builder, builder->getInt64(0), i);
    builder->CreateBr(new_dim_loop_cond_block);
    // for (size_t i = 0; i < src_dimensionality; i++) {
    {
        // CONDITION Block
        builder->SetInsertPoint(new_dim_loop_cond_block);
        llvm::Value *i_val = IR::aligned_load(*builder, i64_ty, i, "i_val");
        llvm::Value *i_lt_src_dimensionality = builder->CreateICmpULT(i_val, src_dimensionality, "i_lt_src_dimensionality");
        builder->CreateCondBr(i_lt_src_dimensionality, new_dim_loop_body_block, new_dim_loop_merge_block);

        // BODY Block
        builder->SetInsertPoint(new_dim_loop_body_block);
        llvm::Value *from_offset = builder->CreateMul(i_val, builder->getInt64(2), "from_offset");
        llvm::Value *from_ptr = builder->CreateGEP(i64_ty, arg_ranges, from_offset, "from_ptr");
        llvm::Value *from = IR::aligned_load(*builder, i64_ty, from_ptr, "from");
        llvm::Value *to_ptr = builder->CreateGEP(i64_ty, from_ptr, builder->getInt64(1), "to_offset");
        llvm::Value *to = IR::aligned_load(*builder, i64_ty, to_ptr, "to");
        llvm::Value *is_range = builder->CreateICmpNE(from, to, "is_range");
        builder->CreateCondBr(is_range, new_dim_loop_is_range_block, new_dim_loop_action_block);

        // if (from != to) {
        {
            builder->SetInsertPoint(new_dim_loop_is_range_block);
            llvm::Value *new_dim_index_val = IR::aligned_load(*builder, i64_ty, new_dim_index, "new_dim_index_val");
            llvm::Value *new_dim_lengths_idx_ptr = builder->CreateGEP(            //
                i64_ty, new_dim_lengths, new_dim_index_val, "new_dim_lenghts_ptr" //
            );
            llvm::Value *to_m_from = builder->CreateSub(to, from, "to_m_from");
            IR::aligned_store(*builder, to_m_from, new_dim_lengths_idx_ptr);
            llvm::Value *new_dim_index_p1 = builder->CreateAdd(new_dim_index_val, builder->getInt64(1), "new_dim_index_p1");
            IR::aligned_store(*builder, new_dim_index_p1, new_dim_index);
            builder->CreateBr(new_dim_loop_action_block);
        }

        // ACTION Block
        builder->SetInsertPoint(new_dim_loop_action_block);
        llvm::Value *next_i_val = builder->CreateAdd(i_val, builder->getInt64(1), "next_i_val");
        IR::aligned_store(*builder, next_i_val, i);
        builder->CreateBr(new_dim_loop_cond_block);
    }
    builder->SetInsertPoint(new_dim_loop_merge_block);

    // Create the new sliced array
    llvm::Value *const new_dimensionality_val = IR::aligned_load(*builder, i64_ty, new_dimensionality, "new_dimensionality_val");
    llvm::Value *const result = builder->CreateCall(create_arr_fn, {new_dimensionality_val, arg_element_size, new_dim_lengths}, "result");
    llvm::Value *const src_data_ptr = builder->CreateGEP(i64_ty, src_dim_lengths, src_dimensionality);
    llvm::Value *const src_data = builder->CreateBitCast(src_data_ptr, builder->getInt8Ty()->getPointerTo(), "src_data");
    llvm::Value *const result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");
    llvm::Value *const result_value_cast = builder->CreateBitCast(result_value_ptr, i64_ty->getPointerTo(), "result_value_cast");
    llvm::Value *const dest_data_ptr = builder->CreateGEP(i64_ty, result_value_cast, new_dimensionality_val, "dest_data_ptr");
    llvm::Value *const dest_data = builder->CreateBitCast(dest_data_ptr, builder->getInt8Ty()->getPointerTo(), "dest_data");

    // Calculate strides for each dimension in source array
    llvm::Value *const src_strides_size = builder->CreateMul(src_dimensionality, builder->getInt64(8), "src_strides_size");
    llvm::Value *const src_strides_ptr = builder->CreateCall(malloc_fn, {src_strides_size}, "src_strides_ptr");
    llvm::Value *const src_strides = builder->CreateBitCast(src_strides_ptr, i64_ty->getPointerTo(), "src_strides");

    IR::aligned_store(*builder, builder->getInt64(1), src_strides);
    IR::aligned_store(*builder, builder->getInt64(1), i);
    builder->CreateBr(strides_loop_cond_block);
    // for (size_t i = 1; i < src_dimensionality; i++) {
    {
        // CONDITION Block
        builder->SetInsertPoint(strides_loop_cond_block);
        llvm::Value *i_val = IR::aligned_load(*builder, i64_ty, i, "i_val");
        llvm::Value *i_lt_src_dimensionality = builder->CreateICmpULT(i_val, src_dimensionality, "i_lt_src_dimensionality");
        builder->CreateCondBr(i_lt_src_dimensionality, strides_loop_body_block, strides_loop_merge_block);

        // BODY Block
        builder->SetInsertPoint(strides_loop_body_block);
        llvm::Value *i_m1 = builder->CreateSub(i_val, builder->getInt64(1), "i_m1");
        llvm::Value *src_strides_i_m1_ptr = builder->CreateGEP(i64_ty, src_strides, i_m1, "src_strides_i_m1_ptr");
        llvm::Value *src_strides_i_m1 = IR::aligned_load(*builder, i64_ty, src_strides_i_m1_ptr, "src_strides_i_m1");
        llvm::Value *src_dim_lengths_i_m1_ptr = builder->CreateGEP(i64_ty, src_dim_lengths, i_m1, "src_dim_lengths_i_m1_ptr");
        llvm::Value *src_dim_lengths_i_m1 = IR::aligned_load(*builder, i64_ty, src_dim_lengths_i_m1_ptr, "src_dim_lengths_i_m1");
        llvm::Value *src_stride_i_val = builder->CreateMul(src_strides_i_m1, src_dim_lengths_i_m1, "src_stride_i_val");
        llvm::Value *src_strides_i_ptr = builder->CreateGEP(i64_ty, src_strides, i_val, "src_strides_i_ptr");
        IR::aligned_store(*builder, src_stride_i_val, src_strides_i_ptr);
        builder->CreateBr(strides_loop_action_block);

        // ACTION Block
        builder->SetInsertPoint(strides_loop_action_block);
        llvm::Value *next_i_val = builder->CreateAdd(i_val, builder->getInt64(1), "next_i_val");
        IR::aligned_store(*builder, next_i_val, i);
        builder->CreateBr(strides_loop_cond_block);
    }
    builder->SetInsertPoint(strides_loop_merge_block);

    IR::aligned_store(*builder, builder->getInt64(0), i);
    IR::aligned_store(*builder, builder->getInt64(1), total_result_elements);
    builder->CreateBr(tre_loop_cond_block);
    // for (size_t i = 0; i < new_dimensionality; i++) {
    {
        // CONDITION Block
        builder->SetInsertPoint(tre_loop_cond_block);
        llvm::Value *i_val = IR::aligned_load(*builder, i64_ty, i, "i_val");
        llvm::Value *i_lt_new_dimensionality = builder->CreateICmpULT(i_val, new_dimensionality_val, "i_lt_new_dimensionality");
        builder->CreateCondBr(i_lt_new_dimensionality, tre_loop_body_block, tre_loop_merge_block);

        // BODY Block
        builder->SetInsertPoint(tre_loop_body_block);
        llvm::Value *total_result_elements_val = IR::aligned_load(*builder, i64_ty, total_result_elements, "total_result_elements_val");
        llvm::Value *new_dim_lengths_i_ptr = builder->CreateGEP(i64_ty, new_dim_lengths, i_val, "new_dim_lengths_i_ptr");
        llvm::Value *new_dim_lengths_i = IR::aligned_load(*builder, i64_ty, new_dim_lengths_i_ptr, "new_dim_lengths_i");
        llvm::Value *new_total_result = builder->CreateMul(total_result_elements_val, new_dim_lengths_i, "new_total_result");
        IR::aligned_store(*builder, new_total_result, total_result_elements);
        builder->CreateBr(tre_loop_action_block);

        // ACTION Block
        builder->SetInsertPoint(tre_loop_action_block);
        llvm::Value *next_i_val = builder->CreateAdd(i_val, builder->getInt64(1), "next_i_val");
        IR::aligned_store(*builder, next_i_val, i);
        builder->CreateBr(tre_loop_cond_block);
    }

    builder->SetInsertPoint(tre_loop_merge_block);
    // Initialize indices to the start of each range/index
    llvm::Value *const current_indices_ptr = builder->CreateCall(malloc_fn, src_strides_size, "current_indices_ptr");
    llvm::Value *const current_indices = builder->CreateBitCast(current_indices_ptr, i64_ty->getPointerTo(), "current_indices");

    IR::aligned_store(*builder, builder->getInt64(0), i);
    builder->CreateBr(idx_init_loop_cond_block);
    // for (size_t i = 1; i < src_dimensionality; i++) {
    {
        // CONDITION Block
        builder->SetInsertPoint(idx_init_loop_cond_block);
        llvm::Value *i_val = IR::aligned_load(*builder, i64_ty, i, "i_val");
        llvm::Value *i_lt_src_dimensionality = builder->CreateICmpULT(i_val, src_dimensionality, "i_lt_src_dimensionality");
        builder->CreateCondBr(i_lt_src_dimensionality, idx_init_loop_body_block, idx_init_loop_merge_block);

        // BODY Block
        builder->SetInsertPoint(idx_init_loop_body_block);
        llvm::Value *ix2 = builder->CreateMul(i_val, builder->getInt64(2), "ix2");
        llvm::Value *ranges_ix2_ptr = builder->CreateGEP(i64_ty, arg_ranges, ix2, "ranges_ix2_ptr");
        llvm::Value *ranges_ix2 = IR::aligned_load(*builder, i64_ty, ranges_ix2_ptr, "ranges_ix2");
        llvm::Value *current_indices_i_ptr = builder->CreateGEP(i64_ty, current_indices, i_val, "current_indices_i_ptr");
        IR::aligned_store(*builder, ranges_ix2, current_indices_i_ptr);
        builder->CreateBr(idx_init_loop_action_block);

        // ACTION Block
        builder->SetInsertPoint(idx_init_loop_action_block);
        llvm::Value *next_i_val = builder->CreateAdd(i_val, builder->getInt64(1), "next_i_val");
        IR::aligned_store(*builder, next_i_val, i);
        builder->CreateBr(idx_init_loop_cond_block);
    }
    builder->SetInsertPoint(idx_init_loop_merge_block);

    // Get the chunk size of the extraction
    llvm::Value *const ranges_1_m_r0 = builder->CreateSub(ranges_1, ranges_0, "ranges_1_m_r0");
    llvm::Value *const chunk_size = builder->CreateSelect(is_first_range, ranges_1_m_r0, builder->getInt64(1), "chunk_size");
    llvm::Value *const total_result_elements_val = IR::aligned_load(*builder, i64_ty, total_result_elements, "tre_val");
    llvm::Value *const num_chunks = builder->CreateUDiv(total_result_elements_val, chunk_size, "num_chunks");

    IR::aligned_store(*builder, builder->getInt64(0), dest_index);
    IR::aligned_store(*builder, builder->getInt64(0), chunk);
    builder->CreateBr(chunk_loop_cond_block);
    // for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    {
        // CONDITION Block
        builder->SetInsertPoint(chunk_loop_cond_block);
        llvm::Value *chunk_val = IR::aligned_load(*builder, i64_ty, chunk, "chunk_val");
        llvm::Value *chunk_lt_num_chunks = builder->CreateICmpULT(chunk_val, num_chunks, "chunk_lt_num_chunks");
        builder->CreateCondBr(chunk_lt_num_chunks, chunk_loop_body_block, chunk_loop_merge_block);

        // BODY Block
        builder->SetInsertPoint(chunk_loop_body_block);

        IR::aligned_store(*builder, builder->getInt64(0), i);
        IR::aligned_store(*builder, builder->getInt64(0), src_offset);
        builder->CreateBr(chunk_loop_offset_loop_cond_block);
        // for (size_t i = 0; i < src_dimensionality; i++) {
        {
            // CONDITION Block
            builder->SetInsertPoint(chunk_loop_offset_loop_cond_block);
            llvm::Value *i_val = IR::aligned_load(*builder, i64_ty, i, "i_val");
            llvm::Value *i_lt_src_dimensionality = builder->CreateICmpULT(i_val, src_dimensionality, "i_lt_src_dimensionality");
            builder->CreateCondBr(i_lt_src_dimensionality, chunk_loop_offset_loop_body_block, chunk_loop_offset_loop_merge_block);

            // BODY Block
            builder->SetInsertPoint(chunk_loop_offset_loop_body_block);
            llvm::Value *current_indices_i_ptr = builder->CreateGEP(i64_ty, current_indices, i_val, "current_indices_i_ptr");
            llvm::Value *current_indices_i = IR::aligned_load(*builder, i64_ty, current_indices_i_ptr, "current_indices_i");
            llvm::Value *src_strides_i_ptr = builder->CreateGEP(i64_ty, src_strides, i_val, "src_strides_i_ptr");
            llvm::Value *src_strides_i = IR::aligned_load(*builder, i64_ty, src_strides_i_ptr, "src_strides_i");
            llvm::Value *curr_idx_mult_src_strides = builder->CreateMul(current_indices_i, src_strides_i, "curr_idx_mult_src_strides");
            llvm::Value *src_offset_val = IR::aligned_load(*builder, i64_ty, src_offset, "src_offest_val");
            llvm::Value *new_src_offset_val = builder->CreateAdd(src_offset_val, curr_idx_mult_src_strides, "new_src_offset_val");
            IR::aligned_store(*builder, new_src_offset_val, src_offset);
            builder->CreateBr(chunk_loop_offset_loop_action_block);

            // ACTION Block
            builder->SetInsertPoint(chunk_loop_offset_loop_action_block);
            llvm::Value *next_i_val = builder->CreateAdd(i_val, builder->getInt64(1), "next_i_val");
            IR::aligned_store(*builder, next_i_val, i);
            builder->CreateBr(chunk_loop_offset_loop_cond_block);
        }
        builder->SetInsertPoint(chunk_loop_offset_loop_merge_block);

        // Copy the chunk (either 1 element or chunk_size elements)
        llvm::Value *dest_index_val = IR::aligned_load(*builder, i64_ty, dest_index, "dest_index_val");
        llvm::Value *dest_idx_mul_elem_size = builder->CreateMul(dest_index_val, arg_element_size, "dest_idx_mul_elem_size");
        llvm::Value *cpy_dest_data_ptr = builder->CreateGEP(builder->getInt8Ty(), dest_data, dest_idx_mul_elem_size, "cpy_dest_data_ptr");
        llvm::Value *src_offset_val = IR::aligned_load(*builder, i64_ty, src_offset, "src_offset_val");
        llvm::Value *src_offset_mul_elem_size = builder->CreateMul(src_offset_val, arg_element_size, "src_offset_mul_elem_size");
        llvm::Value *cpy_src_data_ptr = builder->CreateGEP(builder->getInt8Ty(), src_data, src_offset_mul_elem_size, "cpy_src_data_ptr");
        llvm::Value *cpy_amount = builder->CreateMul(chunk_size, arg_element_size, "cpy_amount");
        builder->CreateCall(memcpy_fn, {cpy_dest_data_ptr, cpy_src_data_ptr, cpy_amount});
        // Increment dest index
        llvm::Value *new_dest_index = builder->CreateAdd(dest_index_val, chunk_size, "new_dest_index");
        IR::aligned_store(*builder, new_dest_index, dest_index);
        // Increment indices - skip the first dimension if it's a range since we copied the whole chunk
        llvm::Value *start_dim = builder->CreateZExt(is_first_range, i64_ty, "start_dim");
        llvm::Value *src_dimensionality_m1 = builder->CreateSub(src_dimensionality, builder->getInt64(1), "src_dimensionality_m1");

        IR::aligned_store(*builder, src_dimensionality_m1, i);
        builder->CreateBr(chunk_loop_index_loop_cond_block);
        // for (size_t i = src_dimensionality - 1; i >= start_dim; i--) {
        {
            // CONDITION Block
            builder->SetInsertPoint(chunk_loop_index_loop_cond_block);
            llvm::Value *i_val = IR::aligned_load(*builder, i64_ty, i, "i_val");
            llvm::Value *i_ge_start_dim = builder->CreateICmpUGE(i_val, start_dim, "i_ge_start_dim");
            builder->CreateCondBr(i_ge_start_dim, chunk_loop_index_loop_body_block, chunk_loop_action_block);

            // BODY Block
            builder->SetInsertPoint(chunk_loop_index_loop_body_block);
            llvm::Value *from_offset = builder->CreateMul(i_val, builder->getInt64(2), "from_offset");
            llvm::Value *from_ptr = builder->CreateGEP(i64_ty, arg_ranges, from_offset, "from_ptr");
            llvm::Value *from = IR::aligned_load(*builder, i64_ty, from_ptr, "from");
            llvm::Value *to_ptr = builder->CreateGEP(i64_ty, from_ptr, builder->getInt64(1), "to_offset");
            llvm::Value *to = IR::aligned_load(*builder, i64_ty, to_ptr, "to");
            llvm::Value *is_range = builder->CreateICmpNE(from, to, "is_range");
            builder->CreateCondBr(is_range, chunk_loop_index_loop_is_range_block, chunk_loop_index_loop_action_block);

            // if (from != to) {
            {
                builder->SetInsertPoint(chunk_loop_index_loop_is_range_block);
                llvm::Value *current_indices_i_ptr = builder->CreateGEP(i64_ty, current_indices, i_val, "current_indices_i_ptr");
                llvm::Value *current_indices_i = IR::aligned_load(*builder, i64_ty, current_indices_i_ptr, "current_indices_i");
                llvm::Value *new_cii_val = builder->CreateAdd(current_indices_i, builder->getInt64(1), "new_cii_val");
                IR::aligned_store(*builder, new_cii_val, current_indices_i_ptr);
                llvm::Value *is_past = builder->CreateICmpULT(new_cii_val, to, "is_past");
                builder->CreateCondBr(is_past, chunk_loop_index_loop_is_range_is_past, chunk_loop_index_loop_is_range_is_not_past);

                // if (current_indices[i] < to) {
                {
                    builder->SetInsertPoint(chunk_loop_index_loop_is_range_is_past);
                    builder->CreateBr(chunk_loop_action_block);
                }

                // } else {
                {
                    builder->SetInsertPoint(chunk_loop_index_loop_is_range_is_not_past);
                    IR::aligned_store(*builder, from, current_indices_i_ptr);
                    builder->CreateBr(chunk_loop_index_loop_action_block);
                }
            }

            // ACTION Block
            builder->SetInsertPoint(chunk_loop_index_loop_action_block);
            llvm::Value *next_i_val = builder->CreateSub(i_val, builder->getInt64(1), "next_i_val");
            IR::aligned_store(*builder, next_i_val, i);
            builder->CreateBr(chunk_loop_index_loop_cond_block);
        }
        builder->SetInsertPoint(chunk_loop_index_loop_merge_block);
        builder->CreateBr(chunk_loop_action_block);

        // ACTION Block
        builder->SetInsertPoint(chunk_loop_action_block);
        llvm::Value *next_chunk_val = builder->CreateAdd(chunk_val, builder->getInt64(1), "next_chunk_val");
        IR::aligned_store(*builder, next_chunk_val, chunk);
        builder->CreateBr(chunk_loop_cond_block);
    }
    builder->SetInsertPoint(chunk_loop_merge_block);

    // Clean up and return
    builder->CreateCall(free_fn, new_dim_lengths);
    builder->CreateCall(free_fn, src_strides);
    builder->CreateCall(free_fn, current_indices);
    builder->CreateRet(result);
}

void Generator::Module::Array::generate_array_manip_functions( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declaration                                //
) {
    generate_get_arr_len_function(builder, module, only_declaration);
    generate_create_arr_function(builder, module, only_declaration);
    generate_fill_arr_inline_function(builder, module, only_declaration);
    generate_fill_arr_deep_function(builder, module, only_declaration);
    generate_fill_arr_val_function(builder, module, only_declaration);
    generate_access_arr_function(builder, module, only_declaration);
    generate_access_arr_val_function(builder, module, only_declaration);
    generate_assign_arr_at_function(builder, module, only_declaration);
    generate_assign_arr_val_at_function(builder, module, only_declaration);
    generate_free_arr_function(builder, module, only_declaration);
    generate_get_arr_slice_1d_function(builder, module, only_declaration);
    generate_get_arr_slice_function(builder, module, only_declaration);
}
