#include "generator/generator.hpp"

static const std::string prefix = "flint.ts.";

/*
 * These are all the structures the thread stack itself "provides":
 *     struct thread_stack_t;
 *     typedef struct function_t {
 *         struct thread_stack_t *thread_stack; // A pointer back to the thread stack itself to be able to tell
 *                                              // which thread a function is part of (future stuff)
 *         uint64_t fn_id;                      // The ID of the called (current) function. IDK yet why it's there but we will see
 *         err_t err;                           // The error return value, since *every* user-defined and thus TS-managed function
 *                                              // can throw an error
 *         char data[];                         // The data contains:
 *                                              //     - The returned values
 *                                              //     - The function parameters
 *                                              //     - The local variables
 *                                              //     - The persistent local variables (future stuff)
 *     } function_t;
 *
 *     typedef enum ts_flags_e : uint32_t {
 *         TS_FLAG_FREE = 0,        // This Thread Stack is "free" and can be occupied by an 'spawn' statement (future stuff)
 *         TS_FLAG_AVAILABLE = 1,   // This Thread Stack is "available" and can be occupied by an 'async' expression or other
 *                                  // parallel work from other threads while it is waiting on a 'sync' point (future stuff)
 *         TS_FLAG_USED = 2,        // This Thread Stack is currently used by a normal execution, for example the main thread
 *         TS_FLAG_CALLABLE = 3,    // This Thread Stack is currently operating on an callable function, this is needed for
 *                                  // future persitent local support, and also to correctly load the next TS frame
 *         TS_FLAG_ASYNC = 4,       // This Thread Stack is currently operating on an async function, we need to keep track of
 *                                  // this information to emit every following 'async' call as a direct call
 *     } ts_flags_e;
 *
 *     typedef struct thread_stack_t {
 *         uint64_t capacity;           // The remaining capacity of the Thread Stack before it's "frame buffer" is full
 *         function_t *stack_ptr;       // The pointer to the next free memory to put the next call frame at
 *         uint32_t thread_id;          // The ID of this thread
 *         uint32_t flags;              // The flags described above (ts_flags_e)
 *         char stack_data[];           // The actual stack data of the thread stack
 *     } thread_stack_t;
 *
 * The Thread Stack is not a global thread_local variable, instead it is allocated at startup of the program. For now, only one single
 * thread and thus only one Thread Stack exist. How to do all the multi-threading stuff is a problem of future me, for now I just focus on
 * single-threaded execution and correctness. The stack size of the thread stack will be 2MiB for now, so '2 << 20' (2**21) bytes.
 */

void Generator::Module::ThreadStack::generate_types() {
    if (type_map.find("type.ts.stack") == type_map.end()) {
        type_map["type.ts.stack"] = IR::create_struct_type("type.ts.stack",
            {
                llvm::Type::getInt64Ty(context),                        // u64 capacity
                llvm::Type::getInt32Ty(context),                        // u32 thread_id
                llvm::Type::getInt32Ty(context),                        // u32 flags
                PTR_TY,                                                 // ptr stack_ptr
                llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // char stack_data[]
            } //
        );
    }
    if (type_map.find("type.ts.function") == type_map.end()) {
        type_map["type.ts.function"] = IR::create_struct_type("type.ts.function",
            {
                PTR_TY,                          // ptr thread_stack
                llvm::Type::getInt64Ty(context), // u64 fn_id
                type_map.at("type.flint.err"),   // err_t err
            } //
        );
    }
}

void Generator::Module::ThreadStack::generate_capacity_check( //
    llvm::IRBuilder<> &builder,                               //
    llvm::Function *const function,                           //
    llvm::Value *const remaining,                             //
    llvm::Type *const frame_type                              //
) {
    llvm::Value *const callee_frame_size = builder.getInt64(Allocation::get_type_size(function->getParent(), frame_type));
    llvm::BasicBlock *const current_block = builder.GetInsertBlock();
    llvm::Value *const has_space = builder.CreateICmpUGE(remaining, callee_frame_size, "has_stack_space");

    llvm::BasicBlock *const overflow_block = llvm::BasicBlock::Create(context, "stack_overflow", function);
    llvm::BasicBlock *const ok_block = llvm::BasicBlock::Create(context, "stack_ok", function);
    builder.SetInsertPoint(current_block);
    builder.CreateCondBr(has_space, ok_block, overflow_block, IR::generate_weights(100, 1));

    builder.SetInsertPoint(overflow_block);
    llvm::Value *msg = IR::generate_const_string(function->getParent(), "Stack overflow detected\n");
    builder.CreateCall(c_functions.at(PRINTF), {msg});
    builder.CreateCall(c_functions.at(ABORT), {});
    builder.CreateUnreachable();

    builder.SetInsertPoint(ok_block);
}
