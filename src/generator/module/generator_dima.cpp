#include "generator/generator.hpp"
#include "parser/parser.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"

/*
 * Okay let's think about how DIMA works. We have:
 * - Heads
 * - Blocks
 * - Slots
 * For each type there is (for now only data types) a head will be generated. The head will have the structure
 *    // Each slot has 16 Bytes of data before the actual slot, meaining it will waste 16 bytes for each allocated value.
 *    // This is the main reason why DIMA is more memory expensive than manual memory management.
 *    typedef struct dima_slot_t {
 *        void *owner;       // A pointer to the owner of this slot
 *        uint32_t block_id; // ID of the block the slot is contained in. These 4 bytes would be used for padding anyways, so we can
 *                           // just add this field to make some DIMA operations quite a lot faster
 *        uint24_t arc;      // Reference count of how many references this slot has
 *        uint8_t flags;     // The flags of the slot. It's a bitset:
 *                           //   | isOccupied | isOwned | isArrStart | isArrMember | isAsync | isOwnedByEntity | Unused | Unused |
 *                           //   | ---------- | ------- | ---------- | ----------- | ------- | --------------- | ------ | ------ |
 *        char value[];      // The actual value the dima Slot holds, directly inlined inside the slot
 *    } dima_slot_t;
 *
 *    typedef struct dima_block_t {
 *        size_t type_size;          // The size of the type stored in this block's slots
 *        size_t capacity;           // The overall capacity of this block
 *        size_t used;               // The number of used slots
 *        size_t pinned_count;       // The number of pinned slots
 *        size_t first_free_slot_id; // Used for fast-access and fast "allocations"
 *        dima_slot_t slots[];       // The slots themselves, directly added inside the allocated block as variable members
 *    } dima_block_t;
 *
 *    typedef struct dima_head_t {
 *        void *default_value;    // The default value will point to a global constant value of the type of the head
 *        size_t type_size;       // The size of the type stored in this dima tree
 *        size_t block_count;     // To keep track of the number of blocks
 *        dima_block_t *blocks[]; // Variable member pattern for the block pointers
 *    } dima_head_t;
 */

void Generator::Module::DIMA::generate_dima_functions( //
    llvm::IRBuilder<> *builder,                        //
    llvm::Module *module,                              //
    const bool is_core_generation                      //
) {
    generate_dima_types();
    generate_dima_init_heads_function(builder, module, is_core_generation);
    generate_dima_get_head_function(builder, module, is_core_generation);

    generate_get_block_capacity_function(builder, module, !is_core_generation);
    generate_dima_create_block_function(builder, module, !is_core_generation);
    generate_dima_allocate_in_block_function(builder, module, !is_core_generation);
    generate_dima_allocate_function(builder, module, !is_core_generation);
    generate_dima_allocate_slot_function(builder, module, !is_core_generation);
}

void Generator::Module::DIMA::generate_dima_types() {
    if (type_map.find("__flint_type_dima_slot") != type_map.end()) {
        return;
    }
    llvm::StructType *slot_type = llvm::StructType::create( //
        context,                                            //
        {
            llvm::Type::getInt8Ty(context)->getPointerTo(),         // ptr owner
            llvm::Type::getInt32Ty(context),                        // u32 block_id
            llvm::Type::getIntNTy(context, 24),                     // u24 arc
            llvm::Type::getInt8Ty(context),                         // u8 flags
            llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // char value[]

        },                //
        "dima.type.slot", //
        false             //
    );
    type_map["__flint_type_dima_slot"] = slot_type;

    llvm::StructType *block_type = llvm::StructType::create( //
        context,                                             //
        {
            llvm::Type::getInt64Ty(context),   // u64 type_size
            llvm::Type::getInt64Ty(context),   // u64 capacity
            llvm::Type::getInt64Ty(context),   // u64 used
            llvm::Type::getInt64Ty(context),   // u64 pinned_count
            llvm::Type::getInt64Ty(context),   // u64 first_free_slot_id
            llvm::ArrayType::get(slot_type, 0) // dima_slot_t slots[]

        },                 //
        "dima.type.block", //
        false              //
    );
    type_map["__flint_type_dima_block"] = block_type;

    llvm::StructType *head_type = llvm::StructType::create( //
        context,                                            //
        {
            llvm::Type::getInt8Ty(context)->getPointerTo(),     // char* default_value
            llvm::Type::getInt64Ty(context),                    // u64 type_size
            llvm::Type::getInt64Ty(context),                    // u64 block_count
            llvm::ArrayType::get(block_type->getPointerTo(), 0) // dima_block_t* blocks[]

        },                //
        "dima.type.head", //
        false             //
    );
    type_map["__flint_type_dima_head"] = head_type;
}

void Generator::Module::DIMA::generate_dima_init_heads_function( //
    llvm::IRBuilder<> *builder,                                  //
    llvm::Module *module,                                        //
    const bool only_declarations                                 //
) {
    llvm::Function *malloc_fn = c_functions.at(MALLOC);

    llvm::FunctionType *init_heads_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
    llvm::Function *init_heads_fn = llvm::Function::Create( //
        init_heads_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "__flint_dima_init_heads",                          //
        module                                              //
    );
    dima_functions["init_heads"] = init_heads_fn;
    if (only_declarations) {
        return;
    }

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", init_heads_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge");
    builder->SetInsertPoint(entry_block);
    llvm::BasicBlock *last_block = entry_block;

    llvm::StructType *head_type = type_map.at("__flint_type_dima_head");
    const size_t head_size = Allocation::get_type_size(module, head_type);
    const std::vector<std::shared_ptr<Type>> data_types = Parser::get_all_data_types();
    for (const auto &data_type : data_types) {
        const DataNode *data_node = data_type->as<DataType>()->data_node;
        const std::string block_name = "init_data_" + data_node->name;
        llvm::BasicBlock *data_block = llvm::BasicBlock::Create(context, block_name, init_heads_fn);
        builder->SetInsertPoint(last_block);
        builder->CreateBr(data_block);

        // Create the global variable for the head
        builder->SetInsertPoint(data_block);
        llvm::ConstantPointerNull *nullpointer = llvm::ConstantPointerNull::get(head_type->getPointerTo());
        const std::string head_var_str = data_node->file_hash.to_string() + ".dima.head.data." + data_node->name;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
        llvm::GlobalVariable *head_variable = new llvm::GlobalVariable(                                              //
            *module, head_type->getPointerTo(), false, llvm::GlobalValue::ExternalLinkage, nullpointer, head_var_str //
        );
#pragma GCC diagnostic pop
        const std::string heads_key = data_node->file_hash.to_string() + "." + data_node->name;
        dima_heads[heads_key] = head_variable;

        // Allocate enough memory for the head, the default value for now is just zero-allocated which means that the whole default head
        // structure can simply be zero-initialized and still be correct
        llvm::Value *allocated_head = builder->CreateCall(malloc_fn, {builder->getInt64(head_size)}, "allocated_head_" + data_node->name);
        IR::aligned_store(*builder, allocated_head, head_variable);
        last_block = data_block;
    }
    builder->CreateBr(merge_block);

    merge_block->insertInto(init_heads_fn);
    builder->SetInsertPoint(merge_block);
    builder->CreateRetVoid();
}

void Generator::Module::DIMA::generate_dima_get_head_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // dima_head_t **get_head(uint32 type_id) {
    //     switch (type_id) {
    //         default:
    //             ERROR;
    //             abort();
    //         case 69:
    //             return &awesome_head;
    //         // Other cases..
    //     }
    // }
    llvm::Function *abort_fn = c_functions.at(ABORT);

    llvm::StructType *dima_head_type = type_map.at("__flint_type_dima_head");
    llvm::FunctionType *get_head_type = llvm::FunctionType::get( //
        dima_head_type->getPointerTo()->getPointerTo(),          // return dima_head_t**
        {llvm::Type::getInt32Ty(context)},                       // u32 type_id
        false                                                    // No vaarg
    );
    llvm::Function *get_head_fn = llvm::Function::Create( //
        get_head_type,                                    //
        llvm::Function::ExternalLinkage,                  //
        "__flint_dima_get_head",                          //
        module                                            //
    );
    dima_functions["get_head"] = get_head_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter
    llvm::Argument *arg_type_id = get_head_fn->arg_begin();
    arg_type_id->setName("type_id");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_head_fn);
    llvm::BasicBlock *incorrect_id_block = llvm::BasicBlock::Create(context, "incorrect_id", get_head_fn);

    builder->SetInsertPoint(entry_block);
    const std::vector<std::shared_ptr<Type>> data_types = Parser::get_all_data_types();
    llvm::SwitchInst *head_switch = builder->CreateSwitch(arg_type_id, incorrect_id_block, data_types.size());

    builder->SetInsertPoint(incorrect_id_block);
    IR::generate_debug_print(builder, module, "__dima_allocate_slot: Unknown type ID: %u", {arg_type_id});
    builder->CreateCall(abort_fn, {});
    builder->CreateUnreachable();

    builder->SetInsertPoint(entry_block);
    for (const auto &data_type : data_types) {
        const DataNode *data_node = data_type->as<DataType>()->data_node;
        const size_t type_id = data_node->file_hash.get_type_id_from_str(data_node->name);
        llvm::BasicBlock *type_block = llvm::BasicBlock::Create(context, "case_" + data_node->name, get_head_fn);
        head_switch->addCase(builder->getInt32(type_id), type_block);

        builder->SetInsertPoint(type_block);
        llvm::GlobalVariable *head_variable = dima_heads.at(data_node->file_hash.to_string() + "." + data_node->name);
        builder->CreateRet(head_variable);
    }
}

void Generator::Module::DIMA::generate_get_block_capacity_function( //
    llvm::IRBuilder<> *builder,                                     //
    llvm::Module *module,                                           //
    const bool only_declarations                                    //
) {
    // THE C IMPLEMENTATION:
    // size_t dima_get_block_capacity(size_t index) {
    //     size_t cap = DIMA_BASE_CAPACITY;
    //     for (size_t j = 0; j < index; j++) {
    //         // Integer mul/div with ceil rounding to approximate float growth
    //         cap = (cap * DIMA_GROWTH_FACTOR + 9) / 10;
    //     }
    //     return cap;
    // }
    llvm::FunctionType *get_block_capacity_type = llvm::FunctionType::get( //
        llvm::Type::getInt64Ty(context),                                   //
        {llvm::Type::getInt64Ty(context)},                                 //
        false                                                              //
    );
    llvm::Function *get_block_capacity_fn = llvm::Function::Create( //
        get_block_capacity_type,                                    //
        llvm::Function::ExternalLinkage,                            //
        "__flint_get_block_capacity",                               //
        module                                                      //
    );
    dima_functions["get_block_capacity"] = get_block_capacity_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter (index)
    llvm::Argument *arg_index = get_block_capacity_fn->arg_begin();
    arg_index->setName("index");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", get_block_capacity_fn);
    llvm::BasicBlock *loop_cond_block = llvm::BasicBlock::Create(context, "loop_cond", get_block_capacity_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", get_block_capacity_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", get_block_capacity_fn);

    builder->SetInsertPoint(entry_block);
    llvm::AllocaInst *capacity = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "capacity");
    IR::aligned_store(*builder, builder->getInt64(BASE_CAPACITY), capacity);
    llvm::AllocaInst *i = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), i);
    builder->CreateBr(loop_cond_block);

    builder->SetInsertPoint(loop_cond_block);
    llvm::Value *i_value = IR::aligned_load(*builder, builder->getInt64Ty(), i, "i_value");
    llvm::Value *i_lt_index = builder->CreateICmpULT(i_value, arg_index, "i_lt_index");
    builder->CreateCondBr(i_lt_index, loop_body_block, merge_block);

    builder->SetInsertPoint(loop_body_block);
    llvm::Value *current_capacity = IR::aligned_load(*builder, builder->getInt64Ty(), capacity, "current_capacity");
    llvm::Value *cap_times_gf = builder->CreateMul(current_capacity, builder->getInt64(GROWTH_FACTOR), "cap_times_gf");
    llvm::Value *ctg_plus_9 = builder->CreateAdd(cap_times_gf, builder->getInt64(9), "ctg_plus_9");
    llvm::Value *new_capacity = builder->CreateUDiv(ctg_plus_9, builder->getInt64(10), "new_capacity");
    IR::aligned_store(*builder, new_capacity, capacity);
    builder->CreateBr(loop_cond_block);

    builder->SetInsertPoint(merge_block);
    llvm::Value *loaded_capacity = IR::aligned_load(*builder, builder->getInt64Ty(), capacity, "loaded_capacity");
    builder->CreateRet(loaded_capacity);
}

void Generator::Module::DIMA::generate_dima_create_block_function( //
    llvm::IRBuilder<> *builder,                                    //
    llvm::Module *module,                                          //
    const bool only_declarations                                   //
) {
    /*
     * This function needs to do a few simple things with DIMA. It recieves the size of the type to create a block for as well as the number
     * of slots to allocate in the new block. Since every single block looks exactly the same this function can be rather simple, just a bit
     * of calculation and allocations.
     */
    // THE C IMPLEMENTATION:
    // void *dima_create_block(size_t type_size, size_t slot_count) {
    //     size_t slot_size = sizeof(DimaSlot) + type_size;
    //     size_t block_size = sizeof(DimaBlock);
    //     size_t allocation_size = block_size + slot_size * slot_count;
    //     return malloc(allocation_size);
    // }
    llvm::Function *malloc_fn = c_functions.at(MALLOC);

    llvm::FunctionType *create_block_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),              // return void*
        {
            llvm::Type::getInt64Ty(context), // u64 type_size
            llvm::Type::getInt64Ty(context)  // u64 slot_coount
        },                                   //
        false                                // No vaarg
    );
    llvm::Function *create_block_fn = llvm::Function::Create( //
        create_block_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "__flint_dima_create_block",                          //
        module                                                //
    );
    dima_functions["create_block"] = create_block_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameters
    llvm::Argument *arg_type_size = create_block_fn->arg_begin();
    arg_type_size->setName("type_size");
    llvm::Argument *arg_slot_count = create_block_fn->arg_begin() + 1;
    arg_slot_count->setName("slot_count");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", create_block_fn);
    builder->SetInsertPoint(entry_block);
    llvm::Value *slot_struct_size = builder->getInt64(16);
    llvm::Value *slot_size = builder->CreateAdd(slot_struct_size, arg_type_size, "slot_size");
    llvm::Value *block_struct_size = builder->getInt64(32);
    llvm::Value *slot_allocation_size = builder->CreateMul(slot_size, arg_slot_count, "slot_allocation_size");
    llvm::Value *allocation_size = builder->CreateAdd(block_struct_size, slot_allocation_size, "allocation_size");
    llvm::Value *allocated_block = builder->CreateCall(malloc_fn, {allocation_size}, "allocated_block");
    builder->CreateRet(allocated_block);
}

void Generator::Module::DIMA::generate_dima_allocate_in_block_function( //
    llvm::IRBuilder<> *builder,                                         //
    llvm::Module *module,                                               //
    const bool only_declarations                                        //
) {
    // THE C IMPLEMENTATION:
    // dima_slot_t *dima_allocate_in_block(dima_block_t *block) {
    //     const size_t slot_size = sizeof(dima_slot_t) + block->type_size;
    //     for (size_t i = block->first_free_slot_id; i < block->capacity; i++) {
    //         dima_slot_t *slot = (dima_slot_t *)((char *)block->slots + slot_size * i);
    //         if (slot->flags == DIMA_UNUSED) {
    //             slot->flags = DIMA_OCCUPIED;
    //             slot->arc = 1;
    //             block->used++;
    //             block->first_free_slot_id = i + 1 >= block->capacity ? 0 : i + 1;
    //             return slot;
    //         }
    //     }
    //     return NULL;
    // }
    llvm::StructType *dima_block_type = type_map.at("__flint_type_dima_block");
    llvm::StructType *dima_slot_type = type_map.at("__flint_type_dima_slot");

    llvm::FunctionType *allocate_in_block_type = llvm::FunctionType::get( //
        dima_slot_type->getPointerTo(),                                   // return dima_slot_t*
        {dima_block_type->getPointerTo()->getPointerTo()},                // dima_block_t* block
        false                                                             // No vaarg
    );
    llvm::Function *allocate_in_block_fn = llvm::Function::Create( //
        allocate_in_block_type,                                    //
        llvm::Function::ExternalLinkage,                           //
        "__flint_dima_allocate_in_block",                          //
        module                                                     //
    );
    dima_functions["allocate_in_block"] = allocate_in_block_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter
    llvm::Argument *arg_block = allocate_in_block_fn->arg_begin();
    arg_block->setName("block");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", allocate_in_block_fn);
    llvm::BasicBlock *loop_cond_block = llvm::BasicBlock::Create(context, "loop_cond", allocate_in_block_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", allocate_in_block_fn);
    llvm::BasicBlock *slot_unused_block = llvm::BasicBlock::Create(context, "slot_unused", allocate_in_block_fn);
    llvm::BasicBlock *loop_merge_block = llvm::BasicBlock::Create(context, "loop_merge", allocate_in_block_fn);

    llvm::ConstantPointerNull *slot_nullptr = llvm::ConstantPointerNull::get(dima_slot_type->getPointerTo());
    const size_t dima_slot_size = Allocation::get_type_size(module, dima_slot_type);

    builder->SetInsertPoint(entry_block);
    llvm::AllocaInst *i = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "i");
    llvm::Value *type_size_ptr = builder->CreateStructGEP(dima_block_type, arg_block, 0, "type_size_ptr");
    llvm::Value *type_size = IR::aligned_load(*builder, builder->getInt64Ty(), type_size_ptr, "type_size");
    llvm::Value *slot_size = builder->CreateAdd(builder->getInt64(dima_slot_size), type_size, "slot_size");
    llvm::Value *capacity_ptr = builder->CreateStructGEP(dima_block_type, arg_block, 1, "capacity_ptr");
    llvm::Value *capacity = IR::aligned_load(*builder, builder->getInt64Ty(), capacity_ptr, "capacity");
    llvm::Value *first_free_slot_ptr = builder->CreateStructGEP(dima_block_type, arg_block, 4, "first_free_slot_ptr");
    llvm::Value *first_free_slot = IR::aligned_load(*builder, builder->getInt64Ty(), first_free_slot_ptr, "first_free_slot");
    IR::aligned_store(*builder, first_free_slot, i);
    llvm::Value *block_slots_ptr = builder->CreateStructGEP(dima_block_type, arg_block, 5, "block_slots_ptr");
    builder->CreateBr(loop_cond_block);

    builder->SetInsertPoint(loop_cond_block);
    llvm::Value *i_value = IR::aligned_load(*builder, builder->getInt64Ty(), i, "i_value");
    llvm::Value *i_lt_capacity = builder->CreateICmpULT(i_value, capacity, "i_lt_capacity");
    builder->CreateCondBr(i_lt_capacity, loop_body_block, loop_merge_block);

    builder->SetInsertPoint(loop_body_block);
    llvm::Value *slot_offset_in_bytes = builder->CreateMul(slot_size, i_value, "slot_offset_in_bytes");
    llvm::Value *slot_ptr = builder->CreateGEP(builder->getInt8Ty(), block_slots_ptr, slot_offset_in_bytes, "slot_ptr");
    llvm::Value *slot_flags_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 3, "slot_flags_ptr");
    llvm::Value *slot_flags = IR::aligned_load(*builder, builder->getInt8Ty(), slot_flags_ptr, "slot_flags");
    llvm::Value *is_empty = builder->CreateICmpEQ(slot_flags, builder->getInt8(static_cast<uint8_t>(Flags::UNUSED)), "is_empty");
    builder->CreateCondBr(is_empty, slot_unused_block, loop_merge_block);

    builder->SetInsertPoint(slot_unused_block);
    IR::aligned_store(*builder, builder->getInt8(static_cast<uint8_t>(Flags::OCCUPIED)), slot_flags_ptr);
    llvm::Value *slot_arc_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 2, "slot_arc_ptr");
    IR::aligned_store(*builder, builder->getIntN(24, 1), slot_arc_ptr);
    llvm::Value *block_used_ptr = builder->CreateStructGEP(dima_block_type, arg_block, 2, "block_used_ptr");
    llvm::Value *block_used = IR::aligned_load(*builder, builder->getInt64Ty(), block_used_ptr, "block_used");
    llvm::Value *block_used_p1 = builder->CreateAdd(block_used, builder->getInt64(1), "block_used_p1");
    IR::aligned_store(*builder, block_used_p1, block_used_ptr);
    llvm::Value *i_p1 = builder->CreateAdd(i_value, builder->getInt64(1), "i_p1");
    llvm::Value *i_p1_ge_cap = builder->CreateICmpUGE(i_p1, capacity, "i_p1_ge_cap");
    llvm::Value *new_first_free_slot = builder->CreateSelect(i_p1_ge_cap, builder->getInt64(0), i_p1, "new_first_free_slot");
    IR::aligned_store(*builder, new_first_free_slot, first_free_slot_ptr);
    builder->CreateRet(slot_ptr);

    builder->SetInsertPoint(loop_merge_block);
    builder->CreateRet(slot_nullptr);
}

void Generator::Module::DIMA::generate_dima_allocate_function( //
    llvm::IRBuilder<> *builder,                                //
    llvm::Module *module,                                      //
    const bool only_declarations                               //
) {
    // THE C IMPLEMENTATION:
    // void *dima_allocate(dima_head_t **head_ref) {
    //     dima_head_t *head = *head_ref;
    //     dima_slot_t *slot_ptr = NULL;
    //     if (UNLIKELY(head->block_count == 0)) {
    //         // Create the first block
    //         head = (dima_head_t *)realloc(head, sizeof(dima_head_t) + sizeof(dima_block_t *));
    //         *head_ref = head;
    //         head->block_count = 1;
    //         head->blocks[0] = dima_create_block(head->type_size, DIMA_BASE_CAPACITY);
    //         slot_ptr = dima_allocate_in_block(head->blocks[0]);
    //     } else {
    //         // Try to find free slot
    //         for (size_t i = head->block_count; i > 0; i--) {
    //             dima_block_t *block = head->blocks[i - 1];
    //             if (UNLIKELY(block == NULL)) {
    //                 continue;
    //             }
    //             if (LIKELY(block->used == block->capacity)) {
    //                 continue;
    //             }
    //             // If came here, there definitely is a free slot, so allocation wont fail
    //             slot_ptr = dima_allocate_in_block(block);
    //             break;
    //         }
    //         if (UNLIKELY(slot_ptr == NULL)) {
    //             // Check if an a block can be created within the blocks array
    //             for (size_t i = head->block_count; i > 0; i--) {
    //                 if (UNLIKELY(head->blocks[i - 1] == NULL)) {
    //                     head->blocks[i - 1] = dima_create_block(head->type_size, dima_get_block_capacity(i - 1));
    //                     slot_ptr = dima_allocate_in_block(head->blocks[i - 1]);
    //                     break;
    //                 }
    //             }
    //         }
    //         if (UNLIKELY(slot_ptr == NULL)) {
    //             // No free slot, allocate new block by reallocating the head
    //             head = (dima_head_t *)realloc(head, sizeof(dima_head_t) + sizeof(dima_block_t *) * (head->block_count + 1));
    //             *head_ref = head;
    //             head->blocks[head->block_count] = dima_create_block(head->type_size, dima_get_block_capacity(head->block_count));
    //             head->block_count++;
    //             // There definitely will be a free slot now
    //             slot_ptr = dima_allocate_in_block(head->blocks[head->block_count - 1]);
    //         }
    //     }
    //     // Copy the default value into the slot
    //     memcpy(slot_ptr->value, head->default_value, head->type_size);
    //     return slot_ptr->value;
    // }
    llvm::Function *realloc_fn = c_functions.at(REALLOC);
    llvm::Function *memcpy_fn = c_functions.at(MEMCPY);

    llvm::Function *create_block_fn = dima_functions.at("create_block");
    llvm::Function *allocate_in_block_fn = dima_functions.at("allocate_in_block");
    llvm::Function *get_block_capacity_fn = dima_functions.at("get_block_capacity");

    llvm::StructType *dima_head_type = type_map.at("__flint_type_dima_head");
    llvm::StructType *dima_block_type = type_map.at("__flint_type_dima_block");
    llvm::StructType *dima_slot_type = type_map.at("__flint_type_dima_slot");

    llvm::FunctionType *allocate_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),          // return void*
        {dima_head_type->getPointerTo()->getPointerTo()},        // dima_head_t** head_ref
        false                                                    // No vaarg
    );
    llvm::Function *allocate_fn = llvm::Function::Create( //
        allocate_type,                                    //
        llvm::Function::ExternalLinkage,                  //
        "__flint_dima_allocate",                          //
        module                                            //
    );
    dima_functions["allocate"] = allocate_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter
    llvm::Argument *arg_head_ref = allocate_fn->arg_begin();
    arg_head_ref->setName("head_ref");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", allocate_fn);

    llvm::BasicBlock *no_heads_block = llvm::BasicBlock::Create(context, "no_heads", allocate_fn);

    llvm::BasicBlock *heads_present_block = llvm::BasicBlock::Create(context, "heads_present", allocate_fn);
    llvm::BasicBlock *loop_condition_block = llvm::BasicBlock::Create(context, "loop_condition", allocate_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", allocate_fn);
    llvm::BasicBlock *loop_body_block_not_null_block = llvm::BasicBlock::Create(context, "loop_body_block_not_null", allocate_fn);
    llvm::BasicBlock *loop_body_block_not_full_block = llvm::BasicBlock::Create(context, "loop_body_block_full", allocate_fn);
    llvm::BasicBlock *loop_merge_block = llvm::BasicBlock::Create(context, "loop_merge", allocate_fn);

    llvm::BasicBlock *create_block_inline_block = llvm::BasicBlock::Create(context, "create_block_inline", allocate_fn);
    llvm::BasicBlock *search_free_loop_condition_block = llvm::BasicBlock::Create(context, "search_free_loop_condition", allocate_fn);
    llvm::BasicBlock *search_free_loop_body_block = llvm::BasicBlock::Create(context, "search_free_loop_body", allocate_fn);
    llvm::BasicBlock *search_free_loop_empty_found_block = llvm::BasicBlock::Create(context, "search_free_loop_empty_found", allocate_fn);
    llvm::BasicBlock *create_block_inline_merge_block = llvm::BasicBlock::Create(context, "create_block_inline_merge", allocate_fn);

    llvm::BasicBlock *create_new_block_block = llvm::BasicBlock::Create(context, "create_new_block", allocate_fn);

    llvm::BasicBlock *copy_block = llvm::BasicBlock::Create(context, "copy", allocate_fn);

    // llvm::ConstantPointerNull *head_nullptr = llvm::ConstantPointerNull::get(dima_head_type->getPointerTo());
    llvm::ConstantPointerNull *block_nullptr = llvm::ConstantPointerNull::get(dima_block_type->getPointerTo());
    llvm::ConstantPointerNull *slot_nullptr = llvm::ConstantPointerNull::get(dima_slot_type->getPointerTo());
    const size_t head_size = Allocation::get_type_size(module, dima_head_type);
    const size_t block_ptr_size = Allocation::get_type_size(module, dima_block_type->getPointerTo());

    builder->SetInsertPoint(entry_block);
    llvm::AllocaInst *slot_alloca = builder->CreateAlloca(dima_slot_type->getPointerTo(), 0, nullptr, "slot");
    IR::aligned_store(*builder, slot_nullptr, slot_alloca);
    llvm::AllocaInst *i = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "i");
    IR::aligned_store(*builder, builder->getInt64(0), i);
    llvm::Value *head_value = IR::aligned_load(*builder, dima_head_type->getPointerTo(), arg_head_ref, "head_value");
    llvm::Value *type_size_ptr = builder->CreateStructGEP(dima_head_type, head_value, 1, "type_size_ptr");
    llvm::Value *type_size = IR::aligned_load(*builder, builder->getInt64Ty(), type_size_ptr, "type_size");
    llvm::Value *head_block_count_ptr = builder->CreateStructGEP(dima_head_type, head_value, 2, "head_block_count_ptr");
    llvm::Value *head_block_count = IR::aligned_load(*builder, builder->getInt64Ty(), head_block_count_ptr, "head_block_count");
    llvm::Value *is_head_empty = builder->CreateICmpEQ(head_block_count, builder->getInt64(0), "is_head_empty");
    builder->CreateCondBr(is_head_empty, no_heads_block, heads_present_block, IR::generate_weights(1, 100));

    { // if (head->block_count == 0) {
        builder->SetInsertPoint(no_heads_block);
        llvm::Value *new_head_value = builder->CreateCall(                                            //
            realloc_fn, {head_value, builder->getInt64(head_size + block_ptr_size)}, "new_head_value" //
        );
        IR::aligned_store(*builder, new_head_value, arg_head_ref);
        llvm::Value *new_head_block_count = builder->CreateStructGEP(dima_head_type, new_head_value, 2);
        IR::aligned_store(*builder, builder->getInt64(1), new_head_block_count);
        llvm::Value *new_block = builder->CreateCall(create_block_fn, {type_size, builder->getInt64(BASE_CAPACITY)}, "new_block");
        llvm::Value *blocks_ptr = builder->CreateStructGEP(dima_head_type, new_head_value, 3, "blocks_ptr");
        IR::aligned_store(*builder, new_block, blocks_ptr);
        llvm::Value *slot_value = builder->CreateCall(allocate_in_block_fn, {new_block}, "slot_value");
        IR::aligned_store(*builder, slot_value, slot_alloca);
        builder->CreateBr(copy_block);
    }

    { // } else {
        builder->SetInsertPoint(heads_present_block);
        head_value = IR::aligned_load(*builder, dima_head_type->getPointerTo(), arg_head_ref);
        llvm::Value *block_count_ptr = builder->CreateStructGEP(dima_head_type, head_value, 2, "block_count_ptr");
        llvm::Value *block_count = IR::aligned_load(*builder, builder->getInt64Ty(), block_count_ptr, "block_count");
        IR::aligned_store(*builder, block_count, i);
        builder->CreateBr(loop_condition_block);

        builder->SetInsertPoint(loop_condition_block);
        llvm::Value *i_value = IR::aligned_load(*builder, builder->getInt64Ty(), i, "i_value");
        llvm::Value *i_gt_0 = builder->CreateICmpUGT(i_value, builder->getInt64(0), "i_gt_0");
        builder->CreateCondBr(i_gt_0, loop_body_block, loop_merge_block);

        { // for (size_t i = head->block_count; i > 0; i--) {
            builder->SetInsertPoint(loop_body_block);
            llvm::Value *block_idx = builder->CreateSub(i_value, builder->getInt64(1), "block_idx");
            llvm::Value *blocks_ptr = builder->CreateStructGEP(dima_head_type, head_value, 3, "blocks_ptr");
            llvm::Value *block_ptr = builder->CreateGEP(dima_block_type->getPointerTo(), blocks_ptr, {block_idx}, "block_ptr");
            llvm::Value *block = IR::aligned_load(*builder, dima_block_type->getPointerTo(), block_ptr, "block");
            llvm::Value *block_null = builder->CreateICmpEQ(block, block_nullptr, "block_null");
            builder->CreateCondBr(block_null, loop_condition_block, loop_body_block_not_null_block, IR::generate_weights(1, 100));

            builder->SetInsertPoint(loop_body_block_not_null_block);
            llvm::Value *block_used_ptr = builder->CreateStructGEP(dima_block_type, block, 2, "block_used_ptr");
            llvm::Value *block_used = IR::aligned_load(*builder, builder->getInt64Ty(), block_used_ptr, "block_used");
            llvm::Value *block_capacity_ptr = builder->CreateStructGEP(dima_block_type, block, 1, "block_capacity_ptr");
            llvm::Value *block_capacity = IR::aligned_load(*builder, builder->getInt64Ty(), block_capacity_ptr, "block_capacity");
            llvm::Value *is_block_full = builder->CreateICmpEQ(block_used, block_capacity, "is_block_full");
            builder->CreateCondBr(is_block_full, loop_condition_block, loop_body_block_not_full_block, IR::generate_weights(1, 100));

            builder->SetInsertPoint(loop_body_block_not_full_block);
            llvm::Value *slot_ptr_value = builder->CreateCall(allocate_in_block_fn, {block}, "slot_ptr_value");
            IR::aligned_store(*builder, slot_ptr_value, slot_alloca);
            llvm::Value *slot_ptr_block_id_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr_value, 1, "slot_ptr_block_id_ptr");
            IR::aligned_store(*builder, block_idx, slot_ptr_block_id_ptr);
            builder->CreateBr(loop_merge_block);
        }
    }
    builder->SetInsertPoint(loop_merge_block);
    llvm::Value *slot_value = IR::aligned_load(*builder, dima_slot_type->getPointerTo(), slot_alloca, "slot_value");
    llvm::Value *is_slot_null = builder->CreateICmpEQ(slot_value, slot_nullptr, "is_slot_null");
    builder->CreateCondBr(is_slot_null, create_block_inline_block, create_block_inline_merge_block, IR::generate_weights(1, 100));

    { // if (UNLIKELY(slot_ptr == NULL)) {
      // Check if a block can be created within the blocks array
        builder->SetInsertPoint(create_block_inline_block);
        IR::aligned_store(*builder, head_block_count, i);
        llvm::Value *blocks = builder->CreateStructGEP(dima_head_type, head_value, 3, "blocks");
        builder->CreateBr(search_free_loop_condition_block);

        builder->SetInsertPoint(search_free_loop_condition_block);
        llvm::Value *i_value = IR::aligned_load(*builder, builder->getInt64Ty(), i, "i_value");
        llvm::Value *i_gt_0 = builder->CreateICmpUGT(i_value, builder->getInt64(0), "i_gt_0");
        builder->CreateCondBr(i_gt_0, search_free_loop_body_block, create_block_inline_merge_block);

        { // loop body
            builder->SetInsertPoint(search_free_loop_body_block);
            llvm::Value *block_idx = builder->CreateSub(i_value, builder->getInt64(1), "block_idx");
            llvm::Value *block_ptr = builder->CreateGEP(dima_block_type->getPointerTo(), blocks, block_idx, "block_ptr");
            llvm::Value *block = IR::aligned_load(*builder, dima_block_type->getPointerTo(), block_ptr, "block");
            llvm::Value *block_is_null = builder->CreateICmpEQ(block, block_nullptr, "block_is_null");
            builder->CreateCondBr(                                                                                         //
                block_is_null, search_free_loop_body_block, search_free_loop_condition_block, IR::generate_weights(1, 100) //
            );

            builder->SetInsertPoint(search_free_loop_empty_found_block);
            llvm::Value *block_capacity = builder->CreateCall(get_block_capacity_fn, {block_idx}, "block_capacity");
            llvm::Value *created_block = builder->CreateCall(create_block_fn, {type_size, block_capacity}, "created_block");
            IR::aligned_store(*builder, created_block, block_ptr);
            llvm::Value *slot_ptr = builder->CreateCall(allocate_in_block_fn, {block}, "slot_ptr");
            IR::aligned_store(*builder, slot_ptr, slot_alloca);
            llvm::Value *slot_block_id_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 1, "slot_block_id_ptr");
            IR::aligned_store(*builder, block_idx, slot_block_id_ptr);
            builder->CreateBr(create_block_inline_merge_block);
        }
    }

    builder->SetInsertPoint(create_block_inline_merge_block);
    slot_value = IR::aligned_load(*builder, dima_slot_type->getPointerTo(), slot_alloca, "slot_value");
    is_slot_null = builder->CreateICmpEQ(slot_value, slot_nullptr, "is_slot_null");
    builder->CreateCondBr(is_slot_null, create_new_block_block, copy_block, IR::generate_weights(1, 100));

    { // if (UNLIKELY(slot_ptr == NULL)) {
      // No free slot, allocate new block by reallocating the head
        builder->SetInsertPoint(create_new_block_block);
        llvm::Value *block_count_p1 = builder->CreateAdd(head_block_count, builder->getInt64(1), "block_count_p1");
        llvm::Value *blocks_size = builder->CreateMul(builder->getInt64(block_ptr_size), block_count_p1, "blocks_size");
        llvm::Value *new_head_size = builder->CreateAdd(builder->getInt64(head_size), blocks_size, "new_head_size");
        llvm::Value *new_head = builder->CreateCall(realloc_fn, {head_value, new_head_size}, "new_head");
        IR::aligned_store(*builder, new_head, arg_head_ref);
        llvm::Value *block_capacity = builder->CreateCall(get_block_capacity_fn, {head_block_count}, "block_capacity");
        llvm::Value *new_block = builder->CreateCall(create_block_fn, {type_size, block_capacity}, "new_block");
        llvm::Value *new_head_block_count_ptr = builder->CreateStructGEP(dima_head_type, new_head, 2, "new_head_block_count_ptr");
        IR::aligned_store(*builder, block_count_p1, new_head_block_count_ptr);
        llvm::Value *slot_ptr = builder->CreateCall(allocate_in_block_fn, {new_block}, "slot_ptr");
        IR::aligned_store(*builder, slot_ptr, slot_alloca);
        llvm::Value *slot_block_id_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 1, "slot_block_id_ptr");
        IR::aligned_store(*builder, head_block_count, slot_block_id_ptr);
        builder->CreateBr(copy_block);
    }

    builder->SetInsertPoint(copy_block);
    slot_value = IR::aligned_load(*builder, dima_slot_type->getPointerTo(), slot_alloca, "slot_value");
    llvm::Value *slot_value_ptr = builder->CreateStructGEP(dima_slot_type, slot_value, 4, "slot_value_ptr");
    head_value = IR::aligned_load(*builder, dima_head_type->getPointerTo(), arg_head_ref);
    llvm::Value *head_default_value_ptr = builder->CreateStructGEP(dima_head_type, head_value, 0, "head_default_value_ptr");
    llvm::Value *head_default_value = IR::aligned_load(                                              //
        *builder, builder->getInt8Ty()->getPointerTo(), head_default_value_ptr, "head_default_value" //
    );
    builder->CreateCall(memcpy_fn, {slot_value_ptr, head_default_value, type_size});
    builder->CreateRet(slot_value_ptr);
}

void Generator::Module::DIMA::generate_dima_allocate_slot_function( //
    llvm::IRBuilder<> *builder,                                     //
    llvm::Module *module,                                           //
    const bool only_declarations                                    //
) {
    /*
     * This function allocates a slot for the given `type_id`. It gets the head from the given type id and allocates a new slot
     * in / for a specific type. It then returns an opaque pointer to the slot of the type. The slot itself always has the same
     * structure. The type id is a 32 bit unsigned integer. This can be implemented pretty easily with a select or even better with a
     * phi node in llvm. We only need to get the correct head and then we can call the `dima_allocate` function which handles the actual
     * allocation of the slot within the given head for us.
     */
    // THE C IMPLEMENTATION:
    // void *dima_allocate_slot(uint32_t type_id) {
    //     dima_head_t **head_ref = __flint_dima_get_head(type_id);
    //     return __flint_dima_allocate(head_ref);
    // }
    llvm::Function *dima_get_head_fn = dima_functions.at("get_head");
    llvm::Function *dima_allocate_fn = dima_functions.at("allocate");

    llvm::FunctionType *allocate_slot_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),               // return void*
        {llvm::Type::getInt32Ty(context)},                            // u32 type_id
        false                                                         // No vaarg
    );
    llvm::Function *allocate_slot_fn = llvm::Function::Create( //
        allocate_slot_type,                                    //
        llvm::Function::ExternalLinkage,                       //
        "__flint_dima_allocate_slot",                          //
        module                                                 //
    );
    dima_functions["allocate_slot"] = allocate_slot_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter
    llvm::Argument *arg_type_id = allocate_slot_fn->arg_begin();
    arg_type_id->setName("type_id");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", allocate_slot_fn);
    builder->SetInsertPoint(entry_block);
    llvm::Value *head = builder->CreateCall(dima_get_head_fn, {arg_type_id}, "head");
    llvm::Value *allocated_slot = builder->CreateCall(dima_allocate_fn, {head}, "allocated_slot");
    builder->CreateRet(allocated_slot);
}
