#include "generator/generator.hpp"
#include "parser/parser.hpp"
#include "llvm/IR/Constants.h"

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
 *         TS_FLAG_ASYNC = 2,       // This Thread Stack is currently operating on an async function, we need to keep track of
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
    llvm::PointerType *const ptr_ty = llvm::PointerType::get(context, 0);
    if (type_map.find("type.ts.stack") == type_map.end()) {
        type_map["type.ts.stack"] = IR::create_struct_type("type.ts.stack",
            {
                llvm::Type::getInt64Ty(context),                        // u64 capacity
                ptr_ty,                                                 // ptr stack_ptr
                llvm::Type::getInt32Ty(context),                        // u32 thread_id
                llvm::Type::getInt32Ty(context),                        // u32 flags
                llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // char stack_data[]
            } //
        );
    }
    if (type_map.find("type.ts.function") == type_map.end()) {
        type_map["type.ts.function"] = IR::create_struct_type("type.ts.function",
            {
                ptr_ty,                          // ptr thread_stack
                llvm::Type::getInt64Ty(context), // u64 fn_id
                type_map.at("type.flint.err"),   // err_t err
                // llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0) // char data[]
            } //
        );
    }
}

void Generator::Module::ThreadStack::generate_ts_frames(llvm::IRBuilder<> *builder, llvm::Module *module) {
    generate_types();
    llvm::StructType *const ts_fn_ty = type_map.at("type.ts.function");
    // We start by getting all functions to generate their respective frame types and default values
    const auto functions = Parser::get_all_functions();
    for (const auto *function : functions) {
        std::vector<llvm::Type *> frame_types = {ts_fn_ty};
        // Append all return types in-order first
        for (const auto &type : function->return_types) {
            if (type->to_string() == "void") {
                continue;
            }
            const auto t = IR::get_type(module, type);
            if (t.second.first) {
                frame_types.emplace_back(t.first->getPointerTo());
            } else {
                frame_types.emplace_back(t.first);
            }
        }
        // Append all argument types in-order
        for (const auto &[type, name, is_mut] : function->parameters) {
            const auto t = IR::get_type(module, type);
            if (t.second.first) {
                frame_types.emplace_back(t.first->getPointerTo());
            } else {
                frame_types.emplace_back(t.first);
            }
        }
        // Append all local variables in-order
        const auto local_variables = function->scope.value()->get_all_variables();
        for (const auto &[var_name, variable] : local_variables) {
            if (variable.is_pseudo_variable || variable.is_fn_param) {
                continue;
            }
            const auto t = IR::get_type(module, variable.type);
            if (t.second.first) {
                frame_types.emplace_back(t.first->getPointerTo());
            } else {
                frame_types.emplace_back(t.first);
            }
        }

        // Create the struct type of the frame and the global default-initializer variable as well
        std::string frame_type_name = function->file_hash.to_string() + ".type.ts." + function->name;
        for (size_t i = 0; i < function->parameters.size(); i++) {
            if (i == 0) {
                frame_type_name += ".";
            }
            if (i > 0) {
                frame_type_name += "_";
            }
            frame_type_name += std::get<0>(function->parameters.at(i))->to_string();
        }
        llvm::StructType *frame_type = IR::create_struct_type(frame_type_name, frame_types);
        const size_t id = function->get_id();
        assert(ts_defaults.find(id) == ts_defaults.end());
        llvm::Constant *ts_fn_default = llvm::ConstantStruct::get(ts_fn_ty,
            {
                llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
                builder->getInt64(id),
                llvm::Constant::getNullValue(type_map.at("type.flint.err")),
            });
        std::vector<llvm::Constant *> frame_elems = {ts_fn_default};
        for (size_t i = 1; i < frame_types.size(); i++) {
            frame_elems.push_back(IR::get_default_value_of_type(frame_types[i]));
        }
        llvm::Constant *frame_default = llvm::ConstantStruct::get(frame_type, frame_elems);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
        ts_defaults[id] = new llvm::GlobalVariable(                                       //
            *module, frame_type, false, llvm::GlobalValue::WeakODRLinkage, frame_default, //
            function->file_hash.to_string() + ".default.ts." + function->name             //
        );
#pragma GCC diagnostic pop
    }
}
