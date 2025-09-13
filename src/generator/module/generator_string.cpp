#include "error/error.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#include "lexer/builtins.hpp"

#include <llvm/IR/DerivedTypes.h>

void Generator::Module::String::generate_access_str_at_function( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Module *module,                                        //
    const bool only_declarations                                 //
) {
    // THE C IMPLEMENTATION:
    // char access_str_at(const str *string, const size_t idx) {
    //     const size_t len = string->len;
    //     if (idx >= string->len) {
    //         // Out of bounds access
    //         // Depending on the flangs `--array-...` here are different code blocks
    //     }
    //     return string->value[idx];
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;

    llvm::FunctionType *access_str_at_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context),                               // Return type: char (i8)
        {
            str_type->getPointerTo(),       // Argument const str* string
            llvm::Type::getInt64Ty(context) // Argument size_t idx
        },
        false // No vaargs
    );
    llvm::Function *access_str_at_fn = llvm::Function::Create( //
        access_str_at_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_access_str_at",                               //
        module                                                 //
    );
    string_manip_functions["access_str_at"] = access_str_at_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameters
    llvm::Argument *arg_string = access_str_at_fn->arg_begin();
    arg_string->setName("string");
    llvm::Argument *arg_idx = access_str_at_fn->arg_begin() + 1;
    arg_idx->setName("idx");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", access_str_at_fn);
    llvm::BasicBlock *out_of_bounds_block = nullptr;
    if (oob_mode != ArrayOutOfBoundsMode::UNSAFE) {
        out_of_bounds_block = llvm::BasicBlock::Create(context, "out_of_bounds", access_str_at_fn);
    }
    llvm::BasicBlock *in_bounds_block = llvm::BasicBlock::Create(context, "in_bounds", access_str_at_fn);
    builder->SetInsertPoint(entry_block);
    llvm::AllocaInst *local_idx_ptr = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "local_idx_ptr");
    IR::aligned_store(*builder, arg_idx, local_idx_ptr);

    // Get the length: string->len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, arg_string, 0, "len_ptr");
    llvm::Value *string_len = IR::aligned_load(*builder, builder->getInt64Ty(), len_ptr, "string_len");

    if (oob_mode != ArrayOutOfBoundsMode::UNSAFE) {
        // Check if idx >= string->len
        llvm::Value *out_of_bounds_cond = builder->CreateICmpUGE(arg_idx, string_len, "out_of_bounds_cond");
        builder->CreateCondBr(out_of_bounds_cond, out_of_bounds_block, in_bounds_block);

        // Out of bounds block
        builder->SetInsertPoint(out_of_bounds_block);

        if (oob_mode == ArrayOutOfBoundsMode::PRINT || oob_mode == ArrayOutOfBoundsMode::CRASH) {
            llvm::Value *format_str = IR::generate_const_string(module, "Out Of Bounds string access occured: len: %lu, idx: %lu\n");
            builder->CreateCall(c_functions.at(PRINTF), {format_str, string_len, arg_idx});
        }
        switch (oob_mode) {
            case ArrayOutOfBoundsMode::PRINT:
                [[fallthrough]];
            case ArrayOutOfBoundsMode::SILENT: {
                // Apply index clamping and update our LOCAL copy
                llvm::Value *clamped_index = builder->CreateSub(string_len, builder->getInt64(1), "clamped_index");
                IR::aligned_store(*builder, clamped_index, local_idx_ptr); // Update our local copy
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

    // In bounds block - normal access
    builder->SetInsertPoint(in_bounds_block);

    // Get pointer to string->value (the char array)
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, arg_string, 1, "value_ptr");

    // For strings, value is directly a char array, so we can access it directly
    // Calculate the address: &string->value[idx]
    llvm::Value *local_idx = IR::aligned_load(*builder, builder->getInt64Ty(), local_idx_ptr, "local_idx");
    llvm::Value *char_ptr = builder->CreateGEP(builder->getInt8Ty(), value_ptr, local_idx, "char_ptr");

    // Load and return the character
    llvm::Value *result_char = IR::aligned_load(*builder, builder->getInt8Ty(), char_ptr, "result_char");
    builder->CreateRet(result_char);
}

void Generator::Module::String::generate_create_str_function( //
    llvm::IRBuilder<> *builder,                               //
    llvm::Module *module,                                     //
    const bool only_declarations                              //
) {
    // THE C IMPLEMENTATION:
    // str *create_str(const size_t len) {
    //     str *string = (str *)malloc(sizeof(str) + len + 1);
    //     string->len = len;
    //     string->value[len] = 0;
    //     return string;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *malloc_fn = c_functions.at(MALLOC);

    llvm::FunctionType *create_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                  // Return type: str*
        {llvm::Type::getInt64Ty(context)},                         // Argument size_t len
        false                                                      // No varargs
    );
    llvm::Function *create_str_fn = llvm::Function::Create( //
        create_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_create_str",                               //
        module                                              //
    );
    string_manip_functions["create_str"] = create_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", create_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the parameter (len)
    llvm::Argument *len_arg = create_str_fn->arg_begin();
    len_arg->setName("len");

    // Calculate malloc size: sizeof(str) + len
    llvm::Value *malloc_size = builder->CreateAdd(                      //
        builder->getInt64(Allocation::get_type_size(module, str_type)), //
        len_arg,                                                        //
        "malloc_size"                                                   //
    );
    // Increment malloc size by 1 for the null terminator
    llvm::Value *actual_size = builder->CreateAdd(malloc_size, builder->getInt64(1), "actual_size");

    // Call malloc
    llvm::Value *string_ptr = builder->CreateCall(malloc_fn, {actual_size}, "string_ptr");

    // Set the len field: string->len = len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, string_ptr, 0, "len_ptr");
    IR::aligned_store(*builder, len_arg, len_ptr);

    // Set the last value in the string to be a nullbyte
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, string_ptr, 1, "value_ptr");
    llvm::Value *term_ptr = builder->CreateGEP(builder->getInt8Ty(), value_ptr, {len_arg}, "term_ptr");
    IR::aligned_store(*builder, builder->getInt8(0), term_ptr);

    // Return the string pointer
    builder->CreateRet(string_ptr);
}

void Generator::Module::String::generate_init_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // str *init_str(const char *value, const size_t len) {
    //     str *string = create_str(len);
    //     memcpy(string->value, value, len);
    //     return string;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *init_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                // Return type str*
        {
            llvm::Type::getInt8Ty(context)->getPointerTo(), // Argument char* value
            llvm::Type::getInt64Ty(context)                 // Argument size_t len
        },                                                  //
        false                                               // No varargs
    );
    llvm::Function *init_str_fn = llvm::Function::Create( //
        init_str_type,                                    //
        llvm::Function::ExternalLinkage,                  //
        "__flint_init_str",                               //
        module                                            //
    );
    string_manip_functions["init_str"] = init_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", init_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the parameter (len)
    llvm::Argument *len_arg = init_str_fn->arg_begin() + 1;
    len_arg->setName("len");

    // Create the string by calling the create_str function
    llvm::Value *string_ptr = builder->CreateCall(create_str_fn, {len_arg}, "string");

    // Get the parameter (value)
    llvm::Argument *value_arg = init_str_fn->arg_begin();
    value_arg->setName("value");

    // Get the pointer to the string value
    llvm::Value *string_val_ptr = builder->CreateStructGEP(str_type, string_ptr, 1, "string_val_ptr");

    // Call the memcpy function
    builder->CreateCall(memcpy_fn, {string_val_ptr, value_arg, len_arg});

    // Return the created string value
    builder->CreateRet(string_ptr);
}

void Generator::Module::String::generate_compare_str_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // int32_t compare_str(const str *lhs, const str *rhs) {
    //     if (lhs->len < rhs->len) {
    //         return -1;
    //     } else if (lhs->len > rhs->len) {
    //         return 1;
    //     }
    //     return memcmp(lhs->value, rhs->value, lhs->len);
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcmp_fn = c_functions.at(MEMCMP);

    llvm::FunctionType *compare_str_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(context),                            // Return type: i32
        {
            str_type->getPointerTo(), // Argument: str* (lhs)
            str_type->getPointerTo()  // Argument: str* (rhs)
        },                            //
        false                         // No varargs
    );
    llvm::Function *compare_str_fn = llvm::Function::Create( //
        compare_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "__flint_compare_str",                               //
        module                                               //
    );
    string_manip_functions["compare_str"] = compare_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", compare_str_fn);
    llvm::BasicBlock *lhs_lt_rhs_block = llvm::BasicBlock::Create(context, "lhs_lt_rhs", compare_str_fn);
    llvm::BasicBlock *lhs_not_lt_rhs_block = llvm::BasicBlock::Create(context, "lhs_not_lt_rhs", compare_str_fn);
    llvm::BasicBlock *lhs_gt_rhs_block = llvm::BasicBlock::Create(context, "lhs_gt_rhs", compare_str_fn);
    llvm::BasicBlock *lhs_eq_rhs_block = llvm::BasicBlock::Create(context, "lhs_eq_rhs", compare_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the lhs argument
    llvm::Argument *arg_lhs = compare_str_fn->arg_begin();
    arg_lhs->setName("lhs");

    // Get the rhs argument
    llvm::Argument *arg_rhs = compare_str_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get length of lhs: lhs->len
    llvm::Value *lhs_len_ptr = builder->CreateStructGEP(str_type, arg_lhs, 0, "lhs_len_ptr");
    llvm::Value *lhs_len = IR::aligned_load(*builder, builder->getInt64Ty(), lhs_len_ptr, "lhs_len");

    // Get length of rhs: rhs->len
    llvm::Value *rhs_len_ptr = builder->CreateStructGEP(str_type, arg_rhs, 0, "rhs_len_ptr");
    llvm::Value *rhs_len = IR::aligned_load(*builder, builder->getInt64Ty(), rhs_len_ptr, "rhs_len");

    // Compare lengths: lhs->len < rhs->len
    llvm::Value *len_lt_cond = builder->CreateICmpULT(lhs_len, rhs_len, "len_lt_cond");
    builder->CreateCondBr(len_lt_cond, lhs_lt_rhs_block, lhs_not_lt_rhs_block);

    // If lhs->len < rhs->len, return -1
    builder->SetInsertPoint(lhs_lt_rhs_block);
    builder->CreateRet(builder->getInt32(-1));

    // If lhs->len >= rhs->len, check if lhs->len > rhs->len
    builder->SetInsertPoint(lhs_not_lt_rhs_block);
    llvm::Value *len_gt_cond = builder->CreateICmpUGT(lhs_len, rhs_len, "len_gt_cond");
    builder->CreateCondBr(len_gt_cond, lhs_gt_rhs_block, lhs_eq_rhs_block);

    // If lhs->len > rhs->len, return 1
    builder->SetInsertPoint(lhs_gt_rhs_block);
    builder->CreateRet(builder->getInt32(1));

    // If lengths are equal, compare the contents using memcmp
    builder->SetInsertPoint(lhs_eq_rhs_block);

    // Get lhs->value
    llvm::Value *lhs_value = builder->CreateStructGEP(str_type, arg_lhs, 1, "lhs_value_ptr");

    // Get rhs->value
    llvm::Value *rhs_value = builder->CreateStructGEP(str_type, arg_rhs, 1, "rhs_value_ptr");

    // Call memcmp
    llvm::Value *memcmp_result = builder->CreateCall(memcmp_fn, {lhs_value, rhs_value, lhs_len}, "memcmp_result");

    // Return the memcmp result
    builder->CreateRet(memcmp_result);
}

void Generator::Module::String::generate_assign_str_function( //
    llvm::IRBuilder<> *builder,                               //
    llvm::Module *module,                                     //
    const bool only_declarations                              //
) {
    // THE C IMPLEMENTATION:
    // void assign_str(str **string, str *value) {
    //     free(*string);
    //     *string = value;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *free_fn = c_functions.at(FREE);

    llvm::FunctionType *assign_str_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                            //
        {
            str_type->getPointerTo()->getPointerTo(), // str**
            str_type->getPointerTo()                  // str*
        },                                            //
        false                                         // No varargs
    );
    llvm::Function *assign_str_fn = llvm::Function::Create( //
        assign_str_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_assign_str",                               //
        module                                              //
    );
    string_manip_functions["assign_str"] = assign_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", assign_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the string argument
    llvm::Argument *arg_string = assign_str_fn->arg_begin();
    arg_string->setName("string");

    // Get the value argument
    llvm::Argument *arg_value = assign_str_fn->arg_begin() + 1;
    arg_value->setName("value");

    // Load the current string pointer: str* old_string = *string
    llvm::Value *old_string_ptr = IR::aligned_load(*builder, str_type->getPointerTo(), arg_string, "old_str_ptr");

    // Free the old string: free(old_string)
    builder->CreateCall(free_fn, {old_string_ptr});

    // Store the new value: *string = value
    IR::aligned_store(*builder, arg_value, arg_string);

    // Return void
    builder->CreateRetVoid();
}

void Generator::Module::String::generate_assign_lit_function( //
    llvm::IRBuilder<> *builder,                               //
    llvm::Module *module,                                     //
    const bool only_declarations                              //
) {
    // THE C IMPLEMENTATION:
    // void assign_lit(str **string, const char *value, const size_t len) {
    //     str *new_string = (str *)realloc(*string, sizeof(str) + len + 1);
    //     *string = new_string;
    //     new_string->len = len;
    //     memcpy(new_string->value, value, len);
    //     new_string->value[len] = 0;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *realloc_fn = c_functions.at(REALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *assign_lit_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                            //
        {
            str_type->getPointerTo()->getPointerTo(),       // Argument: str** string
            llvm::Type::getInt8Ty(context)->getPointerTo(), // Argument: char* value
            llvm::Type::getInt64Ty(context)                 // Argument: u64 len
        },                                                  //
        false                                               // No varargs
    );
    llvm::Function *assign_lit_fn = llvm::Function::Create( //
        assign_lit_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_assign_lit",                               //
        module                                              //
    );
    string_manip_functions["assign_lit"] = assign_lit_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", assign_lit_fn);
    builder->SetInsertPoint(entry_block);

    // Get the string argument
    llvm::Argument *arg_string = assign_lit_fn->arg_begin();
    arg_string->setName("string");

    // Get the value argument
    llvm::Argument *arg_value = assign_lit_fn->arg_begin() + 1;
    arg_value->setName("value");

    // Get the len argument
    llvm::Argument *arg_len = assign_lit_fn->arg_begin() + 2;
    arg_len->setName("len");

    // Load the current string pointer: str* old_string = *string
    llvm::Value *old_string_ptr = IR::aligned_load(*builder, str_type->getPointerTo(), arg_string, "old_string_ptr");

    // Calculate new size: sizeof(str) + len
    size_t str_size = Allocation::get_type_size(module, str_type);
    llvm::Value *new_size = builder->CreateAdd(builder->getInt64(1), builder->CreateAdd(builder->getInt64(str_size), arg_len), "new_size");

    // Call realloc: str* new_string = realloc(old_string, new_size)
    llvm::Value *new_string_ptr = builder->CreateCall(realloc_fn, {old_string_ptr, new_size}, "new_string_ptr");

    // Store the new string pointer back: *string = new_string
    IR::aligned_store(*builder, new_string_ptr, arg_string);

    // Set the len field: new_string->len = len
    llvm::Value *len_ptr = builder->CreateStructGEP(str_type, new_string_ptr, 0, "len_ptr");
    IR::aligned_store(*builder, arg_len, len_ptr);

    // Get pointer to the string data area
    llvm::Value *data_ptr = builder->CreateStructGEP(str_type, new_string_ptr, 1, "data_ptr");

    // Call memcpy to copy the string content
    builder->CreateCall(memcpy_fn, {data_ptr, arg_value, arg_len}, "memcpy_result");

    // Set the last value in the string to be a nullbyte
    llvm::Value *term_ptr = builder->CreateGEP(builder->getInt8Ty(), data_ptr, {arg_len}, "term_ptr");
    IR::aligned_store(*builder, builder->getInt8(0), term_ptr);

    // Return void
    builder->CreateRetVoid();
}

void Generator::Module::String::generate_append_str_function( //
    llvm::IRBuilder<> *builder,                               //
    llvm::Module *module,                                     //
    const bool only_declarations                              //
) {
    // THE C IMPLEMENTATION:
    // void append_str(str **dest, const str *source) {
    //     str *new_dest = (str *)realloc(*dest, sizeof(str) + (*dest)->len + source->len + 1);
    //     *dest = new_dest;
    //     memcpy(new_dest->value + new_dest->len, source->value, source->len);
    //     size_t new_len = new_dest->len + source->len;
    //     new_dest->len = new_len;
    //     new_dest->value[new_len] = 0;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *realloc_fn = c_functions.at(REALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *append_str_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                            // Return Type: void
        {
            str_type->getPointerTo()->getPointerTo(), // Argument: str** dest
            str_type->getPointerTo()                  // Argument: str* source
        },                                            //
        false                                         // No varargs
    );
    llvm::Function *append_str_fn = llvm::Function::Create(append_str_type, llvm::Function::ExternalLinkage, "__flint_append_str", module);
    string_manip_functions["append_str"] = append_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", append_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the dest argument
    llvm::Argument *arg_dest = append_str_fn->arg_begin();
    arg_dest->setName("dest");

    // Get the source argument
    llvm::Argument *arg_source = append_str_fn->arg_begin() + 1;
    arg_source->setName("source");

    // Load the destination string pointer: str* old_dest = *dest
    llvm::Value *old_dest_ptr = IR::aligned_load(*builder, str_type->getPointerTo(), arg_dest, "old_dest_ptr");

    // Load the destination string length: size_t dest_len = old_dest->len
    llvm::Value *dest_len_ptr = builder->CreateStructGEP(str_type, old_dest_ptr, 0, "dest_len_ptr");
    llvm::Value *dest_len = IR::aligned_load(*builder, builder->getInt64Ty(), dest_len_ptr, "dest_len");

    // Load the source string length: size_t source_len = source->len
    llvm::Value *source_len_ptr = builder->CreateStructGEP(str_type, arg_source, 0, "source_len_ptr");
    llvm::Value *source_len = IR::aligned_load(*builder, builder->getInt64Ty(), source_len_ptr, "source_len");

    // Calculate new size: sizeof(str) + dest_len + source_len
    size_t str_size = Allocation::get_type_size(module, str_type);
    llvm::Value *combined_len = builder->CreateAdd(dest_len, source_len, "combined_len");
    llvm::Value *new_size = builder->CreateAdd(                                                         //
        builder->getInt64(1), builder->CreateAdd(builder->getInt64(str_size), combined_len), "new_size" //
    );

    // Call realloc: str* new_dest = realloc(old_dest, new_size)
    llvm::Value *new_dest_ptr = builder->CreateCall(realloc_fn, {old_dest_ptr, new_size}, "new_dest_ptr");

    // Store the new dest pointer back: *dest = new_dest
    IR::aligned_store(*builder, new_dest_ptr, arg_dest);

    // Get pointer to the value field
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, new_dest_ptr, 1, "value_ptr");

    // Calculate the offset in the destination where to append: new_dest->value + dest_len
    llvm::Value *append_pos = builder->CreateGEP(builder->getInt8Ty(), value_ptr, dest_len, "append_pos");

    // Get the source data pointer: source->value
    llvm::Value *source_value = builder->CreateStructGEP(str_type, arg_source, 1, "source_value_ptr");

    // Call memcpy to append the source string: memcpy(new_dest->value + dest_len, source->value, source_len)
    builder->CreateCall(memcpy_fn, {append_pos, source_value, source_len}, "memcpy_result");

    // Update the length of the destination string: new_dest->len += source_len
    llvm::Value *new_len = builder->CreateAdd(dest_len, source_len, "new_len");
    llvm::Value *new_dest_len_ptr = builder->CreateStructGEP(str_type, new_dest_ptr, 0, "new_dest_len_ptr");
    IR::aligned_store(*builder, new_len, new_dest_len_ptr);

    // Set the last value in the string to be a nullbyte
    llvm::Value *term_ptr = builder->CreateGEP(builder->getInt8Ty(), value_ptr, {combined_len}, "term_ptr");
    IR::aligned_store(*builder, builder->getInt8(0), term_ptr);

    // Return void
    builder->CreateRetVoid();
}

void Generator::Module::String::generate_append_lit_function( //
    llvm::IRBuilder<> *builder,                               //
    llvm::Module *module,                                     //
    const bool only_declarations                              //
) {
    // THE C IMPLEMENTATION:
    // void append_lit(str **dest, const char *source, const size_t source_len) {
    //     str *new_dest = (str *)realloc(*dest, sizeof(str) + (*dest)->len + source_len + 1);
    //     *dest = new_dest;
    //     memcpy(new_dest->value + new_dest->len, source, source_len);
    //     size_t new_len = new_dest->len + source_len;
    //     new_dest->len = new_len;
    //     new_dest->value[new_len] = 0;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *realloc_fn = c_functions.at(REALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::FunctionType *append_lit_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                            // Return Type: void
        {
            str_type->getPointerTo()->getPointerTo(),       // Argument: str** dest
            llvm::Type::getInt8Ty(context)->getPointerTo(), // Argument: char* source
            llvm::Type::getInt64Ty(context)                 // Argument: size_t source_len
        },                                                  //
        false                                               // No varargs
    );
    llvm::Function *append_lit_fn = llvm::Function::Create(append_lit_type, llvm::Function::ExternalLinkage, "__flint_append_lit", module);
    string_manip_functions["append_lit"] = append_lit_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", append_lit_fn);
    builder->SetInsertPoint(entry_block);

    // Get the arguments
    llvm::Argument *arg_dest = append_lit_fn->arg_begin();
    arg_dest->setName("dest");

    llvm::Argument *arg_source = append_lit_fn->arg_begin() + 1;
    arg_source->setName("source");

    llvm::Argument *arg_source_len = append_lit_fn->arg_begin() + 2;
    arg_source_len->setName("source_len");

    // Load the destination string pointer: str* old_dest = *dest
    llvm::Value *old_dest_ptr = IR::aligned_load(*builder, str_type->getPointerTo(), arg_dest, "old_dest_ptr");

    // Load the destination string length: size_t dest_len = old_dest->len
    llvm::Value *dest_len_ptr = builder->CreateStructGEP(str_type, old_dest_ptr, 0, "dest_len_ptr");
    llvm::Value *dest_len = IR::aligned_load(*builder, builder->getInt64Ty(), dest_len_ptr, "dest_len");

    // Calculate new size: sizeof(str) + dest_len + source_len
    size_t str_size = Allocation::get_type_size(module, str_type);
    llvm::Value *combined_len = builder->CreateAdd(dest_len, arg_source_len, "combined_len");
    llvm::Value *new_size = builder->CreateAdd(                                                         //
        builder->getInt64(1), builder->CreateAdd(builder->getInt64(str_size), combined_len), "new_size" //
    );

    // Call realloc: str* new_dest = realloc(old_dest, new_size)
    llvm::Value *new_dest_ptr = builder->CreateCall(realloc_fn, {old_dest_ptr, new_size}, "new_dest_ptr");

    // Store the new dest pointer back: *dest = new_dest
    IR::aligned_store(*builder, new_dest_ptr, arg_dest);

    // Get pointer to the value field: new_dest->value
    llvm::Value *value_ptr = builder->CreateStructGEP(str_type, new_dest_ptr, 1, "value_ptr");

    // Calculate the offset in the destination where to append: new_dest->value + dest_len
    llvm::Value *append_pos = builder->CreateGEP(builder->getInt8Ty(), value_ptr, dest_len, "append_pos");

    // Call memcpy to append the source string: memcpy(new_dest->value + dest_len, source, source_len)
    builder->CreateCall(memcpy_fn, {append_pos, arg_source, arg_source_len}, "memcpy_result");

    // Update the length of the destination string: new_dest->len += source_len
    llvm::Value *new_len = builder->CreateAdd(dest_len, arg_source_len, "new_len");
    llvm::Value *new_dest_len_ptr = builder->CreateStructGEP(str_type, new_dest_ptr, 0, "new_dest_len_ptr");
    IR::aligned_store(*builder, new_len, new_dest_len_ptr);

    // Set the last value in the string to be a nullbyte
    llvm::Value *term_ptr = builder->CreateGEP(builder->getInt8Ty(), value_ptr, {combined_len}, "term_ptr");
    IR::aligned_store(*builder, builder->getInt8(0), term_ptr);

    // Return void
    builder->CreateRetVoid();
}

void Generator::Module::String::generate_add_str_str_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // str *add_str_str(const str *lhs, const str *rhs) {
    //     str *result = create_str(lhs->len + rhs->len);
    //     memcpy(result->value, lhs->value, lhs->len);
    //     memcpy(result->value + lhs->len, rhs->value, rhs->len);
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");

    llvm::FunctionType *add_str_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return Type: str*
        {
            str_type->getPointerTo(), // Argument: str* lhs
            str_type->getPointerTo()  // Argument: str* rhs
        },                            //
        false                         // No varargs
    );
    llvm::Function *add_str_str_fn = llvm::Function::Create( //
        add_str_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "__flint_add_str_str",                               //
        module                                               //
    );
    string_manip_functions["add_str_str"] = add_str_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", add_str_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the lhs argument
    llvm::Argument *arg_lhs = add_str_str_fn->arg_begin();
    arg_lhs->setName("lhs");

    // Get the rhs argument
    llvm::Argument *arg_rhs = add_str_str_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get lhs->len
    llvm::Value *lhs_len_ptr = builder->CreateStructGEP(str_type, arg_lhs, 0, "lhs_len_ptr");
    llvm::Value *lhs_len = IR::aligned_load(*builder, builder->getInt64Ty(), lhs_len_ptr, "lhs_len");

    // Get rhs->len
    llvm::Value *rhs_len_ptr = builder->CreateStructGEP(str_type, arg_rhs, 0, "rhs_len_ptr");
    llvm::Value *rhs_len = IR::aligned_load(*builder, builder->getInt64Ty(), rhs_len_ptr, "rhs_len");

    // Calculate total length: lhs->len + rhs->len
    llvm::Value *total_len = builder->CreateAdd(lhs_len, rhs_len, "total_len");

    // Create result string: str *result = create_str(total_len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {total_len}, "result");

    // Get lhs->value
    llvm::Value *lhs_value_ptr = builder->CreateStructGEP(str_type, arg_lhs, 1, "lhs_value_ptr");

    // Get result->value
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");

    // Copy first string: memcpy(result->value, lhs->value, lhs->len)
    builder->CreateCall(memcpy_fn, {result_value_ptr, lhs_value_ptr, lhs_len}, "memcpy1_result");

    // Calculate position for second string: result->value + lhs->len
    llvm::Value *second_pos = builder->CreateGEP(builder->getInt8Ty(), result_value_ptr, lhs_len, "second_pos");

    // Get rhs->value
    llvm::Value *rhs_value_ptr = builder->CreateStructGEP(str_type, arg_rhs, 1, "rhs_value_ptr");

    // Copy second string: memcpy(result->value + lhs->len, rhs->value, rhs->len)
    builder->CreateCall(memcpy_fn, {second_pos, rhs_value_ptr, rhs_len}, "memcpy2_result");

    // Return the result
    builder->CreateRet(result);
}

void Generator::Module::String::generate_add_str_lit_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // str *add_str_lit(const str *lhs, const char *rhs, const size_t rhs_len) {
    //     str *result = create_str(lhs->len + rhs_len);
    //     memcpy(result->value, lhs->value, lhs->len);
    //     memcpy(result->value + lhs->len, rhs, rhs_len);
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");

    llvm::FunctionType *add_str_lit_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return Type: str*
        {
            str_type->getPointerTo(),                       // Argument: str* lhs
            llvm::Type::getInt8Ty(context)->getPointerTo(), // Argument: char* rhs
            llvm::Type::getInt64Ty(context)                 // Argument: u64 rhs_len
        },                                                  //
        false                                               // No varargs
    );
    llvm::Function *add_str_lit_fn = llvm::Function::Create( //
        add_str_lit_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "__flint_add_str_lit",                               //
        module                                               //
    );
    string_manip_functions["add_str_lit"] = add_str_lit_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", add_str_lit_fn);
    builder->SetInsertPoint(entry_block);

    // Get the lhs argument
    llvm::Argument *arg_lhs = add_str_lit_fn->arg_begin();
    arg_lhs->setName("lhs");

    // Get the rhs argument
    llvm::Argument *arg_rhs = add_str_lit_fn->arg_begin() + 1;
    arg_rhs->setName("rhs");

    // Get the rhs_len argument
    llvm::Argument *arg_rhs_len = add_str_lit_fn->arg_begin() + 2;
    arg_rhs_len->setName("rhs_len");

    // Get lhs->len
    llvm::Value *lhs_len_ptr = builder->CreateStructGEP(str_type, arg_lhs, 0, "lhs_len_ptr");
    llvm::Value *lhs_len = IR::aligned_load(*builder, builder->getInt64Ty(), lhs_len_ptr, "lhs_len");

    // Calculate total length: lhs->len + rhs_len
    llvm::Value *total_len = builder->CreateAdd(lhs_len, arg_rhs_len, "total_len");

    // Create result string: str *result = create_str(total_len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {total_len}, "result");

    // Get lhs->value
    llvm::Value *lhs_value_ptr = builder->CreateStructGEP(str_type, arg_lhs, 1, "lhs_value_ptr");

    // Get result->value
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");

    // Copy first string: memcpy(result->value, lhs->value, lhs->len)
    builder->CreateCall(memcpy_fn, {result_value_ptr, lhs_value_ptr, lhs_len});

    // Calculate position for second string: result->value + lhs->len
    llvm::Value *second_pos = builder->CreateGEP(builder->getInt8Ty(), result_value_ptr, lhs_len, "second_pos");

    // Copy second string: memcpy(result->value + lhs->len, rhs, rhs_len)
    builder->CreateCall(memcpy_fn, {second_pos, arg_rhs, arg_rhs_len});

    // Return the result
    builder->CreateRet(result);
}

void Generator::Module::String::generate_add_lit_str_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // str *add_lit_str(const char *lhs, const size_t lhs_len, const str *rhs) {
    //     str *result = create_str(lhs_len + rhs->len);
    //     memcpy(result->value, lhs, lhs_len);
    //     memcpy(result->value + lhs_len, rhs->value, rhs->len);
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");

    llvm::FunctionType *add_lit_str_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                   // Return Type: str*
        {
            llvm::Type::getInt8Ty(context)->getPointerTo(), // Argument: char* lhs
            llvm::Type::getInt64Ty(context),                // Argument: u64 lhs_len
            str_type->getPointerTo()                        // Argument: str* rhs
        },                                                  //
        false                                               // No varargs
    );
    llvm::Function *add_lit_str_fn = llvm::Function::Create( //
        add_lit_str_type,                                    //
        llvm::Function::ExternalLinkage,                     //
        "__flint_add_lit_str",                               //
        module                                               //
    );
    string_manip_functions["add_lit_str"] = add_lit_str_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", add_lit_str_fn);
    builder->SetInsertPoint(entry_block);

    // Get the lhs argument
    llvm::Argument *arg_lhs = add_lit_str_fn->arg_begin();
    arg_lhs->setName("lhs");

    // Get the lhs_len argument
    llvm::Argument *arg_lhs_len = add_lit_str_fn->arg_begin() + 1;
    arg_lhs_len->setName("lhs_len");

    // Get the rhs argument
    llvm::Argument *arg_rhs = add_lit_str_fn->arg_begin() + 2;
    arg_rhs->setName("rhs");

    // Get rhs->len
    llvm::Value *rhs_len_ptr = builder->CreateStructGEP(str_type, arg_rhs, 0, "rhs_len_ptr");
    llvm::Value *rhs_len = IR::aligned_load(*builder, builder->getInt64Ty(), rhs_len_ptr, "rhs_len");

    // Calculate total length: lhs_len + rhs->len
    llvm::Value *total_len = builder->CreateAdd(arg_lhs_len, rhs_len, "total_len");

    // Create result string: str *result = create_str(total_len)
    llvm::Value *result = builder->CreateCall(create_str_fn, {total_len}, "result");

    // Get result->value
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");

    // Copy first string: memcpy(result->value, lhs, lhs_len)
    builder->CreateCall(memcpy_fn, {result_value_ptr, arg_lhs, arg_lhs_len}, "memcpy1_result");

    // Calculate position for second string: result->value + lhs_len
    llvm::Value *second_pos = builder->CreateGEP(builder->getInt8Ty(), result_value_ptr, arg_lhs_len, "second_pos");

    // Get rhs->value
    llvm::Value *rhs_value_ptr = builder->CreateStructGEP(str_type, arg_rhs, 1, "rhs_value_ptr");

    // Copy second string: memcpy(result->value + lhs_len, rhs->value, rhs->len)
    builder->CreateCall(memcpy_fn, {second_pos, rhs_value_ptr, rhs_len}, "memcpy2_result");

    // Return the result
    builder->CreateRet(result);
}

void Generator::Module::String::generate_get_str_slice_function( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Module *module,                                        //
    const bool only_declarations                                 //
) {
    // THE C IMPLEMENTATION:
    // str *get_str_slice(const str *src, const size_t from, const size_t to) {
    //     const size_t real_to = to == 0 ? src->len : to;
    //     const size_t len = real_to - from;
    //     str *result = create_str(len);
    //     memcpy(result->value, src->value + from, len);
    //     return result;
    // }
    llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);
    llvm::Function *create_str_fn = string_manip_functions.at("create_str");

    llvm::FunctionType *get_str_slice_type = llvm::FunctionType::get( //
        str_type->getPointerTo(),                                     // Return Type: str*
        {
            str_type->getPointerTo(),        // Argument: str* src
            llvm::Type::getInt64Ty(context), // Argument: u64 from
            llvm::Type::getInt64Ty(context)  // Argument: u64 to
        },                                   //
        false                                // No varargs
    );
    llvm::Function *get_str_slice_fn = llvm::Function::Create( //
        get_str_slice_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_get_str_slice",                               //
        module                                                 //
    );
    string_manip_functions["get_str_slice"] = get_str_slice_fn;
    if (only_declarations) {
        return;
    }

    // Create a basic block for the function
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_str_slice_fn);
    builder->SetInsertPoint(entry_block);

    // Get the src argument
    llvm::Argument *arg_src = get_str_slice_fn->arg_begin();
    arg_src->setName("src");

    // Get the from argument
    llvm::Argument *arg_from = get_str_slice_fn->arg_begin() + 1;
    arg_from->setName("from");

    // Get the to argument
    llvm::Argument *arg_to = get_str_slice_fn->arg_begin() + 2;
    arg_to->setName("to");

    builder->SetInsertPoint(entry_block);
    llvm::Value *to_eq_0 = builder->CreateICmpEQ(arg_to, builder->getInt64(0), "to_eq_0");
    llvm::Value *src_len_ptr = builder->CreateStructGEP(str_type, arg_src, 0, "src_len_ptr");
    llvm::Value *src_len = IR::aligned_load(*builder, builder->getInt64Ty(), src_len_ptr, "src_len");
    llvm::Value *real_to = builder->CreateSelect(to_eq_0, src_len, arg_to, "real_to");
    llvm::Value *len = builder->CreateSub(real_to, arg_from, "len");
    llvm::Value *result = builder->CreateCall(create_str_fn, {len}, "result");
    llvm::Value *raw_src_value_ptr = builder->CreateStructGEP(str_type, arg_src, 1, "raw_src_value_ptr");
    llvm::Value *src_value_ptr = builder->CreateGEP(builder->getInt8Ty(), raw_src_value_ptr, {arg_from}, "src_value_ptr");
    llvm::Value *result_value_ptr = builder->CreateStructGEP(str_type, result, 1, "result_value_ptr");
    builder->CreateCall(memcpy_fn, {result_value_ptr, src_value_ptr, len});
    builder->CreateRet(result);
}

void Generator::Module::String::generate_string_manip_functions( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Module *module,                                        //
    const bool only_declarations                                 //
) {
    generate_access_str_at_function(builder, module, only_declarations);
    generate_create_str_function(builder, module, only_declarations);
    generate_init_str_function(builder, module, only_declarations);
    generate_compare_str_function(builder, module, only_declarations);
    generate_assign_str_function(builder, module, only_declarations);
    generate_assign_lit_function(builder, module, only_declarations);
    generate_append_str_function(builder, module, only_declarations);
    generate_append_lit_function(builder, module, only_declarations);
    generate_add_str_str_function(builder, module, only_declarations);
    generate_add_str_lit_function(builder, module, only_declarations);
    generate_add_lit_str_function(builder, module, only_declarations);
    generate_get_str_slice_function(builder, module, only_declarations);
}

llvm::Value *Generator::Module::String::generate_string_declaration( //
    llvm::IRBuilder<> &builder,                                      //
    llvm::Value *rhs,                                                //
    std::optional<const ExpressionNode *> rhs_expr                   //
) {
    // If there is no initalizer we need to create a new str struct with length 0 and put absolutely nothing into the value field of it
    if (!rhs_expr.has_value()) {
        llvm::Function *create_str_fn = string_manip_functions.at("create_str");
        llvm::Value *zero = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
        return builder.CreateCall(create_str_fn, {zero}, "empty_str");
    }

    // If there is a rhs, we first check if its a literal node or not. If its a literal string the declaration looks differently than if
    // its a variable
    if (const LiteralNode *literal = dynamic_cast<const LiteralNode *>(rhs_expr.value())) {
        // Get the `init_str` function
        llvm::Function *init_str_fn = string_manip_functions.at("init_str");

        // Get the length of the literal
        const size_t len = std::get<LitStr>(literal->value).value.length();
        llvm::Value *len_val = llvm::ConstantInt::get(builder.getInt64Ty(), len);

        // Call the `init_str` function
        return builder.CreateCall(init_str_fn, {rhs, len_val}, "str_init");
    } else {
        // Just return the rhs, as this is a declaration the lhs definitely doesnt have a value saved on it
        return rhs;
    }
}

void Generator::Module::String::generate_string_assignment( //
    llvm::IRBuilder<> &builder,                             //
    llvm::Value *const lhs,                                 //
    const ExpressionNode *expression_node,                  //
    llvm::Value *expression                                 //
) {
    // If the rhs is a string literal we need to do different code than if it is a string variable. The expression contains a pointer to
    // the str in memory if its a variable, otherwise it will contain a pointer to the chars (char*).
    if (const LiteralNode *lit = dynamic_cast<const LiteralNode *>(expression_node)) {
        // Get the `assign_lit` function
        llvm::Function *assign_lit_fn = string_manip_functions.at("assign_lit");

        // Get the size of the string literal
        const size_t len = std::get<LitStr>(lit->value).value.length();
        llvm::Value *len_val = llvm::ConstantInt::get(builder.getInt64Ty(), len);

        // Call the `assign_lit` function
        builder.CreateCall(assign_lit_fn, {lhs, expression, len_val});
    } else {
        // Get the `assign_str` function
        llvm::Function *assign_str_fn = string_manip_functions.at("assign_str");

        // Call the `assign_str` function
        builder.CreateCall(assign_str_fn, {lhs, expression});
    }
}

llvm::Value *Generator::Module::String::generate_string_addition(                                                 //
    llvm::IRBuilder<> &builder,                                                                                   //
    const std::shared_ptr<Scope> scope,                                                                           //
    const std::unordered_map<std::string, llvm::Value *const> &allocations,                                       //
    std::unordered_map<unsigned int, std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                                //
    llvm::Value *lhs,                                                                                             //
    const ExpressionNode *lhs_expr,                                                                               //
    llvm::Value *rhs,                                                                                             //
    const ExpressionNode *rhs_expr,                                                                               //
    const bool is_append                                                                                          //
) {
    // It highly depends on whether the lhs and / or the rhs are string literals or variables
    const LiteralNode *lhs_lit = dynamic_cast<const LiteralNode *>(lhs_expr);
    const LiteralNode *rhs_lit = dynamic_cast<const LiteralNode *>(rhs_expr);
    if (lhs_lit == nullptr && rhs_lit == nullptr) {
        // Both sides are variables
        if (is_append) {
            llvm::Function *append_str_fn = string_manip_functions.at("append_str");
            const VariableNode *str_var = dynamic_cast<const VariableNode *>(lhs_expr);
            if (str_var == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
            const unsigned int variable_decl_scope = std::get<1>(scope->variables.at(str_var->name));
            llvm::Value *const variable_alloca = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + str_var->name);
            builder.CreateCall(append_str_fn, {variable_alloca, rhs});
            return lhs;
        } else {
            llvm::Function *add_str_str_fn = string_manip_functions.at("add_str_str");
            llvm::Value *addition_result = builder.CreateCall(add_str_str_fn, {lhs, rhs}, "add_str_str_res");
            const VariableNode *lhs_var = dynamic_cast<const VariableNode *>(lhs_expr);
            const VariableNode *rhs_var = dynamic_cast<const VariableNode *>(rhs_expr);
            if (garbage.count(expr_depth) == 0) {
                if (lhs_var == nullptr) {
                    garbage[expr_depth].emplace_back(Type::get_primitive_type("str"), lhs);
                }
                if (rhs_var == nullptr) {
                    garbage[expr_depth].emplace_back(Type::get_primitive_type("str"), rhs);
                }
            } else {
                if (lhs_var == nullptr) {
                    garbage.at(expr_depth).emplace_back(Type::get_primitive_type("str"), lhs);
                }
                if (rhs_var == nullptr) {
                    garbage.at(expr_depth).emplace_back(Type::get_primitive_type("str"), rhs);
                }
            }
            return addition_result;
        }
    } else if (lhs_lit == nullptr && rhs_lit != nullptr) {
        // Only rhs is literal
        const VariableNode *lhs_var = dynamic_cast<const VariableNode *>(lhs_expr);
        if (is_append) {
            llvm::Function *append_lit_fn = string_manip_functions.at("append_lit");
            if (lhs_var == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
            const unsigned int variable_decl_scope = std::get<1>(scope->variables.at(lhs_var->name));
            llvm::Value *const variable_alloca = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + lhs_var->name);
            builder.CreateCall(append_lit_fn, {variable_alloca, rhs, builder.getInt64(std::get<LitStr>(rhs_lit->value).value.length())});
            return lhs;
        } else {
            llvm::Function *add_str_lit_fn = string_manip_functions.at("add_str_lit");
            llvm::Value *rhs_len = llvm::ConstantInt::get(      //
                llvm::Type::getInt64Ty(context),                //
                std::get<LitStr>(rhs_lit->value).value.length() //
            );
            llvm::Value *addition_result = builder.CreateCall(add_str_lit_fn, {lhs, rhs, rhs_len}, "add_str_lit_res");
            if (lhs_var == nullptr) {
                if (garbage.count(expr_depth) == 0) {
                    garbage[expr_depth].emplace_back(Type::get_primitive_type("str"), lhs);
                } else {
                    garbage.at(expr_depth).emplace_back(Type::get_primitive_type("str"), lhs);
                }
            }
            return addition_result;
        }
    } else if (lhs_lit != nullptr && rhs_lit == nullptr) {
        // Only lhs is literal
        llvm::Function *add_lit_str_fn = string_manip_functions.at("add_lit_str");
        llvm::Value *lhs_len = llvm::ConstantInt::get(      //
            llvm::Type::getInt64Ty(context),                //
            std::get<LitStr>(lhs_lit->value).value.length() //
        );
        llvm::Value *addition_result = builder.CreateCall(add_lit_str_fn, {lhs, lhs_len, rhs}, "add_lit_str_res");
        if (dynamic_cast<const VariableNode *>(rhs_expr) == nullptr) {
            if (garbage.count(expr_depth) == 0) {
                garbage[expr_depth].emplace_back(Type::get_primitive_type("str"), rhs);
            } else {
                garbage.at(expr_depth).emplace_back(Type::get_primitive_type("str"), rhs);
            }
        }
        return addition_result;
    }

    // Both sides are literals, this shouldnt be possible, as they should have been folded already
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}
