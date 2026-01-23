#include "generator/generator.hpp"
#include "parser/parser.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"

/*
 * For each type there is (for now only data types) a head will be generated. The head will have the structure
 *    // Each slot has 16 Bytes of data before the actual slot, meaining it will waste 16 bytes for each allocated value.
 *    // This is the main reason why DIMA is more memory expensive than manual memory management.
 *    typedef struct dima_slot_t {
 *        void *owner;       // A pointer to the owner of this slot
 *        uint32_t arc;      // Reference count of how many references this slot has
 *        uint16_t block_id; // ID of the block the slot is contained in
 *        uint16_t flags;    // The flags of the slot. It's a bitset:
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

llvm::GlobalVariable *Generator::Module::DIMA::get_head(const std::shared_ptr<Type> &type) {
    const DataType *data_type = type->as<DataType>();
    const std::string head_key = data_type->data_node->file_hash.to_string() + "." + data_type->data_node->name;
    llvm::GlobalVariable *data_head = Module::DIMA::dima_heads.at(head_key);
    return data_head;
}

void Generator::Module::DIMA::generate_heads(llvm::Module *module) {
    llvm::StructType *head_type = type_map.at("type.dima.head");
    const std::vector<std::shared_ptr<Type>> data_types = Parser::get_all_data_types();
    llvm::ConstantPointerNull *nullpointer = llvm::ConstantPointerNull::get(head_type->getPointerTo());
    for (const auto &data_type : data_types) {
        const DataNode *data_node = data_type->as<DataType>()->data_node;
        const std::string head_var_str = data_node->file_hash.to_string() + ".dima.head.data." + data_node->name;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
        llvm::GlobalVariable *head_variable = new llvm::GlobalVariable(                                             //
            *module, head_type->getPointerTo(), false, llvm::GlobalValue::WeakODRLinkage, nullpointer, head_var_str //
        );
#pragma GCC diagnostic pop
        const std::string heads_key = data_node->file_hash.to_string() + "." + data_node->name;
        dima_heads[heads_key] = head_variable;
    }
}

void Generator::Module::DIMA::generate_dima_functions( //
    llvm::IRBuilder<> *builder,                        //
    llvm::Module *module,                              //
    const bool is_core_generation,                     //
    const bool only_declarations                       //
) {
    generate_types();
    generate_init_heads_function(builder, module, is_core_generation || only_declarations);

    generate_get_block_capacity_function(builder, module, !is_core_generation || only_declarations);
    generate_create_block_function(builder, module, !is_core_generation || only_declarations);
    generate_allocate_in_block_function(builder, module, !is_core_generation || only_declarations);
    generate_allocate_function(builder, module, !is_core_generation || only_declarations);
    generate_retain_function(builder, module, !is_core_generation || only_declarations);
    generate_release_function(builder, module, !is_core_generation || only_declarations);
}

void Generator::Module::DIMA::generate_types() {
    if (type_map.find("type.dima.slot") == type_map.end()) {
        type_map["type.dima.slot"] = IR::create_struct_type("type.dima.slot",
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(),         // ptr owner
                llvm::Type::getInt32Ty(context),                        // u32 arc
                llvm::Type::getInt16Ty(context),                        // u16 block_id
                llvm::Type::getInt16Ty(context),                        // u16 flags
                llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // char value[]
            } //
        );
    }

    if (type_map.find("type.dima.block") == type_map.end()) {
        llvm::StructType *slot_type = type_map.at("type.dima.slot");
        type_map["type.dima.block"] = IR::create_struct_type("type.dima.block",
            {
                llvm::Type::getInt64Ty(context),   // u64 type_size
                llvm::Type::getInt64Ty(context),   // u64 capacity
                llvm::Type::getInt64Ty(context),   // u64 used
                llvm::Type::getInt64Ty(context),   // u64 pinned_count
                llvm::Type::getInt64Ty(context),   // u64 first_free_slot_id
                llvm::ArrayType::get(slot_type, 0) // dima_slot_t slots[]
            } //
        );
    }

    if (type_map.find("type.dima.head") == type_map.end()) {
        llvm::StructType *block_type = type_map.at("type.dima.block");
        type_map["type.dima.head"] = IR::create_struct_type("type.dima.head",
            {
                llvm::Type::getInt8Ty(context)->getPointerTo(),     // char* default_value
                llvm::Type::getInt64Ty(context),                    // u64 type_size
                llvm::Type::getInt64Ty(context),                    // u64 block_count
                llvm::ArrayType::get(block_type->getPointerTo(), 0) // dima_block_t* blocks[]
            } //
        );
    }
}

void Generator::Module::DIMA::generate_init_heads_function( //
    llvm::IRBuilder<> *builder,                             //
    llvm::Module *module,                                   //
    const bool only_declarations                            //
) {
    llvm::Function *malloc_fn = c_functions.at(MALLOC);
    llvm::Function *memset_fn = c_functions.at(MEMSET);

    llvm::FunctionType *init_heads_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
    llvm::Function *init_heads_fn = llvm::Function::Create( //
        init_heads_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        "flint.dima_init_heads",                            //
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

    llvm::StructType *head_type = type_map.at("type.dima.head");
    const size_t head_size = Allocation::get_type_size(module, head_type);
    const std::vector<std::shared_ptr<Type>> data_types = Parser::get_all_data_types();
    for (const auto &data_type : data_types) {
        const DataNode *data_node = data_type->as<DataType>()->data_node;
        const std::string block_name = "init_data_" + data_node->name;
        llvm::StructType *data_struct_type = IR::add_and_or_get_type(module, data_type, false);
        const size_t data_type_size = Allocation::get_type_size(module, data_struct_type);
        llvm::BasicBlock *data_block = llvm::BasicBlock::Create(context, block_name, init_heads_fn);
        builder->SetInsertPoint(last_block);
        builder->CreateBr(data_block);

        // Create the global variable for the head
        builder->SetInsertPoint(data_block);
        llvm::ConstantPointerNull *nullpointer = llvm::ConstantPointerNull::get(head_type->getPointerTo());
        const std::string head_var_str = data_node->file_hash.to_string() + ".dima.head.data." + data_node->name;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
        llvm::GlobalVariable *head_variable = new llvm::GlobalVariable(                                             //
            *module, head_type->getPointerTo(), false, llvm::GlobalValue::WeakODRLinkage, nullpointer, head_var_str //
        );
#pragma GCC diagnostic pop
        const std::string heads_key = data_node->file_hash.to_string() + "." + data_node->name;
        dima_heads[heads_key] = head_variable;

        // Allocate enough memory for the head, the default value for now is just zero-allocated which means that the whole default head
        // structure can simply be zero-initialized and still be correct
        llvm::Value *allocated_head = builder->CreateCall(malloc_fn, {builder->getInt64(head_size)}, "allocated_head_" + data_node->name);
        // Store the type size in the head
        llvm::Value *type_size_ptr = builder->CreateStructGEP(head_type, allocated_head, 1, "type_size_ptr");
        IR::aligned_store(*builder, builder->getInt64(data_type_size), type_size_ptr);
        // Allocate enough memory for the default value and set it to 0 for now
        // TODO: Once data has default values we need to set the fields of the data here too
        llvm::Value *default_value = builder->CreateCall(                                      //
            malloc_fn, {builder->getInt64(data_type_size)}, "default_value_" + data_node->name //
        );
        builder->CreateCall(memset_fn, {default_value, builder->getInt32(0), builder->getInt64(data_type_size)});
        llvm::Value *default_value_ptr = builder->CreateStructGEP(head_type, allocated_head, 0, "default_value_ptr");
        IR::aligned_store(*builder, default_value, default_value_ptr);
        IR::aligned_store(*builder, allocated_head, head_variable);
        last_block = data_block;
    }
    builder->CreateBr(merge_block);

    merge_block->insertInto(init_heads_fn);
    builder->SetInsertPoint(merge_block);
    builder->CreateRetVoid();
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
        "flint.dima_get_block_capacity",                            //
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

void Generator::Module::DIMA::generate_create_block_function( //
    llvm::IRBuilder<> *builder,                               //
    llvm::Module *module,                                     //
    const bool only_declarations                              //
) {
    /*
     * This function needs to do a few simple things with DIMA. It recieves the size of the type to create a block for as well as the number
     * of slots to allocate in the new block. Since every single block looks exactly the same this function can be rather simple, just a bit
     * of calculation and allocations.
     */
    // THE C IMPLEMENTATION:
    // dima_block_t *dima_create_block(const size_t type_size, const size_t capacity) {
    //     const size_t slot_size = sizeof(dima_slot_t) + type_size;
    //     dima_block_t *block = (dima_block_t *)malloc(sizeof(dima_block_t) + slot_size * capacity);
    //     memset(block->slots, 0, capacity * slot_size);
    //     block->type_size = type_size;
    //     block->capacity = capacity;
    //     block->used = 0;
    //     block->pinned_count = 0;
    //     block->first_free_slot_id = 0;
    //     return block;
    // }
    llvm::Function *malloc_fn = c_functions.at(MALLOC);
    llvm::Function *memset_fn = c_functions.at(MEMSET);

    llvm::StructType *dima_block_type = type_map.at("type.dima.block");
    llvm::StructType *dima_slot_type = type_map.at("type.dima.slot");

    const size_t dima_block_size = Allocation::get_type_size(module, dima_block_type);
    const size_t dima_slot_size = Allocation::get_type_size(module, dima_slot_type);

    llvm::FunctionType *create_block_type = llvm::FunctionType::get( //
        dima_block_type->getPointerTo(),                             // return dima_block_t*
        {
            llvm::Type::getInt64Ty(context), // u64 type_size
            llvm::Type::getInt64Ty(context)  // u64 capacity
        },                                   //
        false                                // No vaarg
    );
    llvm::Function *create_block_fn = llvm::Function::Create( //
        create_block_type,                                    //
        llvm::Function::ExternalLinkage,                      //
        "flint.dima_create_block",                            //
        module                                                //
    );
    dima_functions["create_block"] = create_block_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameters
    llvm::Argument *arg_type_size = create_block_fn->arg_begin();
    arg_type_size->setName("type_size");
    llvm::Argument *arg_capacity = create_block_fn->arg_begin() + 1;
    arg_capacity->setName("capacity");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", create_block_fn);
    builder->SetInsertPoint(entry_block);
    llvm::Value *slot_size = builder->CreateAdd(builder->getInt64(dima_slot_size), arg_type_size, "slot_size");
    llvm::Value *slot_allocation_size = builder->CreateMul(slot_size, arg_capacity, "slot_allocation_size");
    llvm::Value *allocation_size = builder->CreateAdd(builder->getInt64(dima_block_size), slot_allocation_size, "allocation_size");
    llvm::Value *allocated_block = builder->CreateCall(malloc_fn, {allocation_size}, "allocated_block");
    llvm::Value *block_type_size_ptr = builder->CreateStructGEP(dima_block_type, allocated_block, 0, "block_slots_ptr");
    IR::aligned_store(*builder, arg_type_size, block_type_size_ptr);
    llvm::Value *block_capacity_ptr = builder->CreateStructGEP(dima_block_type, allocated_block, 1, "block_capacity_ptr");
    IR::aligned_store(*builder, arg_capacity, block_capacity_ptr);
    llvm::Value *block_used_ptr = builder->CreateStructGEP(dima_block_type, allocated_block, 2, "block_used_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), block_used_ptr);
    llvm::Value *block_pinned_count_ptr = builder->CreateStructGEP(dima_block_type, allocated_block, 3, "block_pinned_count_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), block_pinned_count_ptr);
    llvm::Value *block_first_free_slot_ptr = builder->CreateStructGEP(dima_block_type, allocated_block, 4, "block_first_free_slot_ptr");
    IR::aligned_store(*builder, builder->getInt64(0), block_first_free_slot_ptr);
    llvm::Value *block_slots_ptr = builder->CreateStructGEP(dima_block_type, allocated_block, 5, "block_slots_ptr");
    builder->CreateCall(memset_fn, {block_slots_ptr, builder->getInt32(0), slot_allocation_size});
    builder->CreateRet(allocated_block);
}

void Generator::Module::DIMA::generate_allocate_in_block_function( //
    llvm::IRBuilder<> *builder,                                    //
    llvm::Module *module,                                          //
    const bool only_declarations                                   //
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
    llvm::StructType *dima_block_type = type_map.at("type.dima.block");
    llvm::StructType *dima_slot_type = type_map.at("type.dima.slot");

    llvm::FunctionType *allocate_in_block_type = llvm::FunctionType::get( //
        dima_slot_type->getPointerTo(),                                   // return dima_slot_t*
        {dima_block_type->getPointerTo()->getPointerTo()},                // dima_block_t* block
        false                                                             // No vaarg
    );
    llvm::Function *allocate_in_block_fn = llvm::Function::Create( //
        allocate_in_block_type,                                    //
        llvm::Function::ExternalLinkage,                           //
        "flint.dima_allocate_in_block",                            //
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
    llvm::Value *slot_flags = IR::aligned_load(*builder, builder->getInt16Ty(), slot_flags_ptr, "slot_flags");
    llvm::Value *is_empty = builder->CreateICmpEQ(slot_flags, builder->getInt16(static_cast<uint16_t>(Flags::UNUSED)), "is_empty");
    builder->CreateCondBr(is_empty, slot_unused_block, loop_merge_block);

    builder->SetInsertPoint(slot_unused_block);
    IR::aligned_store(*builder, builder->getInt16(static_cast<uint16_t>(Flags::OCCUPIED)), slot_flags_ptr);
    llvm::Value *slot_arc_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 2, "slot_arc_ptr");
    IR::aligned_store(*builder, builder->getInt32(1), slot_arc_ptr);
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

void Generator::Module::DIMA::generate_allocate_function( //
    llvm::IRBuilder<> *builder,                           //
    llvm::Module *module,                                 //
    const bool only_declarations                          //
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

    llvm::StructType *dima_head_type = type_map.at("type.dima.head");
    llvm::StructType *dima_block_type = type_map.at("type.dima.block");
    llvm::StructType *dima_slot_type = type_map.at("type.dima.slot");

    llvm::FunctionType *allocate_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),          // return void*
        {dima_head_type->getPointerTo()->getPointerTo()},        // dima_head_t** head_ref
        false                                                    // No vaarg
    );
    llvm::Function *allocate_fn = llvm::Function::Create( //
        allocate_type,                                    //
        llvm::Function::ExternalLinkage,                  //
        "flint.dima_allocate",                            //
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
            llvm::Value *slot_block_id_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 2, "slot_block_id_ptr");
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
        llvm::Value *slot_block_id_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 2, "slot_block_id_ptr");
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

void Generator::Module::DIMA::generate_retain_function( //
    llvm::IRBuilder<> *builder,                         //
    llvm::Module *module,                               //
    const bool only_declarations                        //
) {
    // THE C IMPLEMENTATION:
    // void *dima_retain(void *value) {
    //     // 'container_of' is a macro to essentially offset the value pointer by sizeof(dima_slot_t) to
    //     // the left to point to the beginning of the slot
    //     dima_slot_t *slot = container_of(value, dima_slot_t, value);
    //     slot->arc++;
    //     return value;
    // }
    llvm::StructType *dima_slot_type = type_map.at("type.dima.slot");

    llvm::FunctionType *retain_type = llvm::FunctionType::get( //
        llvm::Type::getInt8Ty(context)->getPointerTo(),        // return void*
        {builder->getInt8Ty()->getPointerTo()},                // void* value
        false                                                  // No vaarg
    );
    llvm::Function *retain_fn = llvm::Function::Create( //
        retain_type,                                    //
        llvm::Function::ExternalLinkage,                //
        "flint.dima_retain",                            //
        module                                          //
    );
    dima_functions["retain"] = retain_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameter
    llvm::Argument *arg_value = retain_fn->arg_begin();
    arg_value->setName("value");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", retain_fn);

    builder->SetInsertPoint(entry_block);
    const size_t container_of_offset = -Allocation::get_type_size(module, dima_slot_type);
    llvm::Value *slot_ptr = builder->CreateGEP(builder->getInt8Ty(), arg_value, builder->getInt64(container_of_offset), "slot_ptr");
    llvm::Value *slot_arc_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 1, "slot_arc_ptr");
    llvm::Value *slot_arc = IR::aligned_load(*builder, builder->getInt32Ty(), slot_arc_ptr, "slot_arc");
    llvm::Value *slot_arc_p1 = builder->CreateAdd(slot_arc, builder->getInt32(1), "slot_arc_p1");
    IR::aligned_store(*builder, slot_arc_p1, slot_arc_ptr);
    builder->CreateRet(arg_value);
}

void Generator::Module::DIMA::generate_release_function( //
    llvm::IRBuilder<> *builder,                          //
    llvm::Module *module,                                //
    const bool only_declarations                         //
) {
    // THE C IMPLEMENTATION:
    // void dima_release(dima_head_t **head_ref, void *value) {
    //     dima_slot_t *slot = container_of(value, dima_slot_t, value);
    //     assert(slot->arc > 0);
    //     slot->arc--;
    //     if (LIKELY(slot->arc > 0)) {
    //         // Do not apply all the below checks since no block is potentially freed
    //         return;
    //     }
    //     // Start at the biggest block because it has the most slots, so it is the most likely to contain the slot
    //     dima_head_t *head = *head_ref;
    //     const size_t block_id = slot->block_id;
    //     dima_block_t *block = head->blocks[block_id];
    //     // This is the block containing the freed slot
    //     assert(block->used > 0);
    //     block->used--;
    //     // Fill the slot with zeroes
    //     const size_t slot_size = sizeof(dima_slot_t) + head->type_size;
    //     memset(slot, 0, slot_size);
    //     char *start = (char *)block->slots;
    //     size_t index = ((char *)slot - start) / slot_size;
    //     if (block->first_free_slot_id > index) {
    //         block->first_free_slot_id = index;
    //     }
    //     if (LIKELY(block->used > 0)) {
    //         return;
    //     }
    //     // Remove empty block
    //     free(block);
    //     head->blocks[block_id] = NULL;
    //     // Shrink the blocks array if the last block was freed up to the first block thats not null
    //     if (LIKELY(block_id + 1 < head->block_count)) {
    //         return;
    //     }
    //
    //     // Check how many empty blocks there are to calculate the new size
    //     size_t new_size = head->block_count - 1;
    //     for (; new_size > 0; new_size--) {
    //         if (head->blocks[new_size - 1] != NULL) {
    //             break;
    //         }
    //     }
    //     // Realloc the head to the new size
    //     head = (dima_head_t *)realloc(head, sizeof(dima_head_t) + sizeof(dima_block_t *) * (new_size));
    //     *head_ref = head;
    //     head->block_count = new_size;
    // }
    llvm::Function *memset_fn = c_functions.at(MEMSET);
    llvm::Function *free_fn = c_functions.at(FREE);
    llvm::Function *realloc_fn = c_functions.at(REALLOC);

    llvm::StructType *dima_slot_type = type_map.at("type.dima.slot");
    llvm::StructType *dima_block_type = type_map.at("type.dima.block");
    llvm::StructType *dima_head_type = type_map.at("type.dima.head");

    llvm::ConstantPointerNull *block_nullptr = llvm::ConstantPointerNull::get(dima_block_type->getPointerTo());

    const size_t dima_slot_size = Allocation::get_type_size(module, dima_slot_type);
    const size_t dima_block_size = Allocation::get_type_size(module, dima_block_type);
    const size_t dima_head_size = Allocation::get_type_size(module, dima_head_type);

    llvm::FunctionType *release_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                         // return void
        {
            dima_head_type->getPointerTo()->getPointerTo(), // dima_head_t** head_ref
            builder->getInt8Ty()->getPointerTo()            // void* value
        },
        false // No vaarg
    );
    llvm::Function *release_fn = llvm::Function::Create( //
        release_type,                                    //
        llvm::Function::ExternalLinkage,                 //
        "flint.dima_release",                            //
        module                                           //
    );
    dima_functions["release"] = release_fn;
    if (only_declarations) {
        return;
    }

    // Get the parameters
    llvm::Argument *arg_head_ref = release_fn->arg_begin();
    arg_head_ref->setName("head_ref");
    llvm::Argument *arg_value = release_fn->arg_begin() + 1;
    arg_value->setName("value");

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", release_fn);
    llvm::BasicBlock *early_return_block = llvm::BasicBlock::Create(context, "early_return", release_fn);
    llvm::BasicBlock *release_slot_block = llvm::BasicBlock::Create(context, "release_slot", release_fn);
    llvm::BasicBlock *remove_empty_block_block = llvm::BasicBlock::Create(context, "remove_empty_block", release_fn);
    llvm::BasicBlock *needs_relocation_block = llvm::BasicBlock::Create(context, "needs_relocation", release_fn);
    llvm::BasicBlock *loop_condition_block = llvm::BasicBlock::Create(context, "loop_condition", release_fn);
    llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(context, "loop_body", release_fn);
    llvm::BasicBlock *realloc_block = llvm::BasicBlock::Create(context, "realloc_block", release_fn);

    builder->SetInsertPoint(entry_block);
    llvm::Value *slot_ptr = builder->CreateGEP(builder->getInt8Ty(), arg_value, builder->getInt64(-dima_slot_size), "slot_ptr");
    llvm::Value *slot_arc_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 1, "slot_arc_ptr");
    llvm::Value *slot_arc = IR::aligned_load(*builder, builder->getInt32Ty(), slot_arc_ptr, "slot_arc");
    llvm::Value *slot_arc_m1 = builder->CreateSub(slot_arc, builder->getInt32(1), "slot_arc_m1");
    IR::aligned_store(*builder, slot_arc_m1, slot_arc_ptr);
    llvm::Value *slot_arc_m1_gt_0 = builder->CreateICmpUGT(slot_arc_m1, builder->getInt32(0), "slot_arc_m1_gt_0");
    builder->CreateCondBr(slot_arc_m1_gt_0, early_return_block, release_slot_block, IR::generate_weights(100, 1));

    builder->SetInsertPoint(early_return_block);
    builder->CreateRetVoid();

    builder->SetInsertPoint(release_slot_block);
    llvm::Value *head = IR::aligned_load(*builder, dima_head_type->getPointerTo(), arg_head_ref, "head");
    llvm::Value *block_id_ptr = builder->CreateStructGEP(dima_slot_type, slot_ptr, 2, "block_id_ptr");
    llvm::Value *block_id = IR::aligned_load(*builder, builder->getInt32Ty(), block_id_ptr, "block_id");
    llvm::Value *blocks_ptr = builder->CreateStructGEP(dima_head_type, head, 3, "blocks_ptr");
    llvm::Value *block_ptr = builder->CreateGEP(dima_block_type->getPointerTo(), blocks_ptr, block_id, "block_ptr");
    llvm::Value *block = IR::aligned_load(*builder, dima_block_type->getPointerTo(), block_ptr);
    llvm::Value *block_used_ptr = builder->CreateStructGEP(dima_block_type, block, 2, "block_used_ptr");
    llvm::Value *block_used = IR::aligned_load(*builder, builder->getInt64Ty(), block_used_ptr, "block_used");
    llvm::Value *block_used_m1 = builder->CreateSub(block_used, builder->getInt64(1), "block_used_m1");
    llvm::Value *type_size_ptr = builder->CreateStructGEP(dima_block_type, block, 0, "type_size_ptr");
    llvm::Value *type_size = IR::aligned_load(*builder, builder->getInt64Ty(), type_size_ptr, "type_size");
    llvm::Value *slot_size = builder->CreateAdd(builder->getInt64(dima_slot_size), type_size, "slot_size");
    builder->CreateCall(memset_fn, {slot_ptr, builder->getInt32(0), slot_size});
    llvm::Value *block_slots_ptr = builder->CreateStructGEP(dima_block_type, block, 5, "block_slots_ptr");
    llvm::Value *block_slots_ptr_int = builder->CreatePtrToInt(block_slots_ptr, builder->getInt64Ty(), "block_slots_ptr_i64");
    llvm::Value *slot_ptr_int = builder->CreatePtrToInt(slot_ptr, builder->getInt64Ty(), "slot_ptr_i64");
    llvm::Value *slot_ptr_diff = builder->CreateSub(slot_ptr_int, block_slots_ptr_int, "slot_ptr_diff");
    llvm::Value *index = builder->CreateUDiv(slot_ptr_diff, slot_size, "index");
    llvm::Value *first_free_slot_id_ptr = builder->CreateStructGEP(dima_block_type, block, 4, "first_free_slot_id_ptr");
    llvm::Value *first_free_slot_id = IR::aligned_load(*builder, builder->getInt64Ty(), first_free_slot_id_ptr, "first_free_slot_id");
    llvm::Value *ffsid_gt_index = builder->CreateICmpUGT(first_free_slot_id, index, "ffsid_gt_index");
    llvm::Value *new_ffsid = builder->CreateSelect(ffsid_gt_index, index, first_free_slot_id, "neww_ffsid");
    IR::aligned_store(*builder, new_ffsid, first_free_slot_id_ptr);
    llvm::Value *block_used_m1_gt_0 = builder->CreateICmpUGT(block_used_m1, builder->getInt64(0), "block_used_m1_gt_0");
    builder->CreateCondBr(block_used_m1_gt_0, early_return_block, remove_empty_block_block, IR::generate_weights(100, 1));

    builder->SetInsertPoint(remove_empty_block_block);
    builder->CreateCall(free_fn, {block});
    IR::aligned_store(*builder, block_nullptr, block_ptr);
    llvm::Value *block_count_ptr = builder->CreateStructGEP(dima_head_type, head, 2, "block_count_ptr");
    llvm::Value *block_count = IR::aligned_load(*builder, builder->getInt64Ty(), block_count_ptr, "block_count");
    llvm::Value *block_count_i32 = builder->CreateTrunc(block_count, builder->getInt32Ty(), "block_count_i32");
    llvm::Value *block_id_p1 = builder->CreateAdd(block_id, builder->getInt32(1), "block_id_p1");
    llvm::Value *block_id_p1_lt_block_count = builder->CreateICmpULT(block_id_p1, block_count_i32, "block_id_p1_lt_block_count");
    builder->CreateCondBr(block_id_p1_lt_block_count, early_return_block, needs_relocation_block, IR::generate_weights(100, 1));

    builder->SetInsertPoint(needs_relocation_block);
    llvm::AllocaInst *new_size = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "new_size");
    llvm::Value *block_count_m1 = builder->CreateSub(block_count, builder->getInt64(1), "block_count_m1");
    IR::aligned_store(*builder, block_count_m1, new_size);
    builder->CreateBr(loop_condition_block);

    builder->SetInsertPoint(loop_condition_block);
    llvm::Value *new_size_value = IR::aligned_load(*builder, builder->getInt64Ty(), new_size, "new_size_value");
    llvm::Value *new_size_gt_0 = builder->CreateICmpUGT(new_size_value, builder->getInt64(0), "new_size_gt_0");
    builder->CreateCondBr(new_size_gt_0, loop_body_block, realloc_block);

    builder->SetInsertPoint(loop_body_block);
    llvm::Value *new_size_m1 = builder->CreateSub(new_size_value, builder->getInt64(1), "new_size_m1");
    llvm::Value *check_block_ptr = builder->CreateGEP(dima_block_type->getPointerTo(), blocks_ptr, new_size_m1, "check_block_ptr");
    llvm::Value *block_is_null = builder->CreateICmpEQ(check_block_ptr, block_nullptr, "block_is_null");
    builder->CreateCondBr(block_is_null, realloc_block, loop_condition_block);

    builder->SetInsertPoint(realloc_block);
    new_size_value = IR::aligned_load(*builder, builder->getInt64Ty(), new_size, "new_size_value");
    llvm::Value *block_part_size = builder->CreateMul(builder->getInt64(dima_block_size), new_size_value, "block_part_size");
    llvm::Value *realloc_size = builder->CreateAdd(builder->getInt64(dima_head_size), block_part_size, "realloc_size");
    llvm::Value *new_head = builder->CreateCall(realloc_fn, {head, realloc_size}, "new_head");
    IR::aligned_store(*builder, new_head, arg_head_ref);
    block_count_ptr = builder->CreateStructGEP(dima_head_type, new_head, 2, "block_count_ptr");
    IR::aligned_store(*builder, new_size_value, block_count_ptr);
    builder->CreateRetVoid();
}
