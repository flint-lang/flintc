#include "generator/generator.hpp"
#include "lexer/builtins.hpp"

static const Hash hash(std::string("time"));
static const std::string prefix = hash.to_string() + ".time.";

void Generator::Module::Time::generate_time_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_types(module);
    generate_platform_functions(module);
    generate_time_init_function(builder, module, only_declarations);
    generate_now_function(builder, module, only_declarations);
    generate_duration_function(builder, module, only_declarations);
    generate_sleep_duration_function(builder, module, only_declarations);
    generate_sleep_time_function(builder, module, only_declarations);
    generate_as_unit_function(builder, module, only_declarations);
    generate_from_function(builder, module, only_declarations);
}

void Generator::Module::Time::generate_types(llvm::Module *module) {
    // Create the data types of this module
    assert(core_module_data_types.find("time") != core_module_data_types.end());
    const auto &data_types = core_module_data_types.at("time");
    for (const data_type &type : data_types) {
        const std::string type_name(std::get<0>(type));

        // Get the global variable to the DIMA head
        const std::string head_var_str = hash.to_string() + ".dima.head.data." + type_name;
        llvm::GlobalVariable *dima_head_variable = module->getGlobalVariable(head_var_str);
        assert(time_dima_heads.find(head_var_str) == time_dima_heads.end());
        time_dima_heads[type_name] = dima_head_variable;

        // Create the struct type
        const std::vector<data_field> &fields = std::get<1>(type);
        if (time_data_types.find(type_name) != time_data_types.end()) {
            continue;
        }
        std::vector<llvm::Type *> types_vec;
        for (const auto &[field_type, field_name] : fields) {
            types_vec.emplace_back(IR::get_type(module, Type::get_primitive_type(std::string(field_type))).first);
        }
        llvm::ArrayRef<llvm::Type *> return_types_arr(types_vec);
        time_data_types[type_name] = IR::create_struct_type(prefix + "type.data." + type_name, return_types_arr);
    }

    // Generate the enum strings for this module's provided enum types
    assert(core_module_enum_types.find("time") != core_module_enum_types.end());
    const auto &enum_types = core_module_enum_types.at("time");
    for (const enum_type &type : enum_types) {
        const std::string enum_name(std::get<0>(type));
        const std::vector<std::string_view> &value_views = std::get<1>(type);
        std::vector<std::string> enum_values;
        for (const std::string_view &value : value_views) {
            enum_values.emplace_back(value);
        }
        IR::generate_enum_value_strings(module, prefix.substr(0, prefix.size() - 1), enum_name, enum_values);
    }
}

void Generator::Module::Time::generate_platform_functions(llvm::Module *module) {
#ifdef __WIN32__
    // Windows-specific functions

    // QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount)
    llvm::StructType *large_integer_type = llvm::StructType::create(context, {llvm::Type::getInt64Ty(context)}, "LARGE_INTEGER");
    time_data_types["LARGE_INTEGER"] = large_integer_type;
    llvm::FunctionType *QueryPerformanceCounter_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(context),                                        // returns BOOL (i32)
        {large_integer_type->getPointerTo()},                                   // LARGE_INTEGER*
        false                                                                   // No vaargs
    );
    llvm::Function *QueryPerformanceCounter_fn = llvm::Function::Create(                                 //
        QueryPerformanceCounter_type, llvm::Function::ExternalLinkage, "QueryPerformanceCounter", module //
    );
    time_platform_functions["QueryPerformanceCounter"] = QueryPerformanceCounter_fn;

    // QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency)
    llvm::FunctionType *QueryPerformanceFrequency_type = llvm::FunctionType::get(    //
        llvm::Type::getInt32Ty(context), {large_integer_type->getPointerTo()}, false //
    );
    llvm::Function *QueryPerformanceFrequency_fn = llvm::Function::Create(                                   //
        QueryPerformanceFrequency_type, llvm::Function::ExternalLinkage, "QueryPerformanceFrequency", module //
    );
    time_platform_functions["QueryPerformanceFrequency"] = QueryPerformanceFrequency_fn;
#else
    // Linux/POSIX functions

    // struct timespec { time_t tv_sec; long tv_nsec; }
    llvm::StructType *timespec_type = llvm::StructType::create( //
        context,
        {
            llvm::Type::getInt64Ty(context), // tv_sec
            llvm::Type::getInt64Ty(context)  // tv_nsec
        },
        "c.struct.timespec" //
    );
    time_data_types["c.struct.timespec"] = timespec_type;

    // clock_gettime(clockid_t clock_id, struct timespec* tp)
    llvm::FunctionType *clock_gettime_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(context),                              // returns i32
        {
            llvm::Type::getInt32Ty(context), // i32 clock_id
            timespec_type->getPointerTo()    // timespec* tp
        },
        false // No vaargs
    );
    llvm::Function *clock_gettime_fn = llvm::Function::Create(clock_gettime_type, llvm::Function::ExternalLinkage, "clock_gettime", module);
    time_platform_functions["clock_gettime"] = clock_gettime_fn;

    // nanosleep(const struct timespec* req, struct timespec* rem)
    llvm::FunctionType *nanosleep_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(context),                          // returns i32
        {
            timespec_type->getPointerTo(), // requested_time
            timespec_type->getPointerTo()  // remaining
        },
        false // No vaargs
    );
    llvm::Function *nanosleep_fn = llvm::Function::Create(nanosleep_type, llvm::Function::ExternalLinkage, "nanosleep", module);
    time_platform_functions["nanosleep"] = nanosleep_fn;
#endif
}

void Generator::Module::Time::generate_time_init_function( //
    [[maybe_unused]] llvm::IRBuilder<> *builder,           //
    [[maybe_unused]] llvm::Module *module,                 //
    [[maybe_unused]] const bool only_declarations          //
) {
#ifdef __WIN32__
    // Create time_init function
    llvm::FunctionType *init_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
    llvm::Function *init_fn = llvm::Function::Create( //
        init_type,                                    //
        llvm::Function::ExternalLinkage,              //
        prefix + "time_init",                         //
        module                                        //
    );
    time_platform_functions["init_time"] = init_fn;
    if (only_declarations) {
        return;
    }

    // Create global for frequency
    llvm::GlobalVariable *freq_global = new llvm::GlobalVariable(   //
        *module,                                                    // Module
        llvm::Type::getInt64Ty(context),                            // i64 type
        false,                                                      // not constant
        llvm::GlobalValue::InternalLinkage,                         // Only used within this module
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), // Initial value of 0
        prefix + "global.frequency"                                 // Name of the global
    );

    // Create global for initialized flag
    llvm::GlobalVariable *init_global = new llvm::GlobalVariable(  //
        *module,                                                   // Module
        llvm::Type::getInt1Ty(context),                            // bool type
        false,                                                     // not constant
        llvm::GlobalValue::InternalLinkage,                        // Only used within this module
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0), // Initial value of 'false'
        prefix + "global.initialized"                              // Name of the global
    );

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", init_fn);
    llvm::BasicBlock *init_block = llvm::BasicBlock::Create(context, "init", init_fn);
    llvm::BasicBlock *exit_block = llvm::BasicBlock::Create(context, "exit", init_fn);

    builder->SetInsertPoint(entry);
    llvm::Value *is_initialized = builder->CreateLoad(llvm::Type::getInt1Ty(context), init_global);
    builder->CreateCondBr(is_initialized, exit_block, init_block);

    builder->SetInsertPoint(init_block);
    llvm::Function *qpf_fn = time_platform_functions["QueryPerformanceFrequency"];
    llvm::Value *freq_alloca = builder->CreateAlloca(llvm::Type::getInt64Ty(context));
    builder->CreateCall(qpf_fn, {freq_alloca});
    llvm::Value *freq_value = builder->CreateLoad(llvm::Type::getInt64Ty(context), freq_alloca);
    builder->CreateStore(freq_value, freq_global);
    builder->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1), init_global);
    builder->CreateBr(exit_block);

    builder->SetInsertPoint(exit_block);
    builder->CreateRetVoid();
#endif
}

void Generator::Module::Time::generate_now_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // TimeStamp* now() {
    //     TimeStamp* stamp = (TimeStamp *)malloc(sizeof(TimeStamp));
    //
    // #ifdef __WIN32__
    //     __time_init();
    //     LARGE_INTEGER counter;
    //     QueryPerformanceCounter(&counter);
    //     // Convert to nanoseconds: (counter * 1e9) / frequency
    //     // Typical frequency: 10 MHz → 100ns resolution
    //     stamp->value = (uint64_t)((counter.QuadPart * 1000000000ULL) / __time_frequency.QuadPart);
    // #else
    //     // Linux/POSIX - use CLOCK_MONOTONIC for monotonic time
    //     struct timespec ts;
    //     clock_gettime(CLOCK_MONOTONIC, &ts);
    //     stamp->value = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    // #endif
    //
    //     return stamp;
    // }
    llvm::StructType *timestamp_type = time_data_types.at("TimeStamp");

    llvm::FunctionType *now_type = llvm::FunctionType::get( //
        timestamp_type->getPointerTo(),                     // Return type: TimeStamp*
        false                                               // No arguments
    );
    llvm::Function *now_fn = llvm::Function::Create( //
        now_type,                                    //
        llvm::Function::ExternalLinkage,             //
        prefix + "now",                              //
        module                                       //
    );
    time_functions["now"] = now_fn;
    if (only_declarations) {
        return;
    }

    // Create entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", now_fn);
    builder->SetInsertPoint(entry_block);

#ifdef __WIN32__
    // Windows implementation

    // Call time_init()
    llvm::Function *init_fn = time_platform_functions.at("init_time");
    builder->CreateCall(init_fn, {});

    // Create LARGE_INTEGER counter on stack
    llvm::StructType *large_integer_type = time_data_types.at("LARGE_INTEGER");
    llvm::Value *counter_ptr = builder->CreateAlloca(large_integer_type, nullptr, "counter_ptr");

    // Call QueryPerformanceCounter(&counter)
    llvm::Function *qpc_fn = time_platform_functions.at("QueryPerformanceCounter");
    builder->CreateCall(qpc_fn, {counter_ptr});

    // Load counter value (counter.QuadPart)
    llvm::Value *counter_field_ptr = builder->CreateStructGEP(large_integer_type, counter_ptr, 0, "counter_field_ptr");
    llvm::Value *counter_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), counter_field_ptr, "counter_value");

    // Load frequency from global
    llvm::GlobalVariable *freq_global = module->getNamedGlobal(prefix + "global.time_frequency");
    llvm::Value *freq_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), freq_global, "freq_value");

    // Calculate: (counter * 1000000000ULL) / frequency
    llvm::Value *counter_ns = builder->CreateMul(counter_value, builder->getInt64(1000000000ULL), "counter_ns");
    llvm::Value *stamp_value = builder->CreateUDiv(counter_ns, freq_value, "stamp_value");

#else
    // Linux/POSIX implementation

    // Create struct timespec on stack
    llvm::StructType *timespec_type = time_data_types.at("c.struct.timespec");
    llvm::Value *ts_ptr = builder->CreateAlloca(timespec_type, nullptr, "ts_ptr");

    // Call clock_gettime(CLOCK_MONOTONIC, &ts)
    // CLOCK_MONOTONIC = 1
    llvm::Function *clock_gettime_fn = time_platform_functions.at("clock_gettime");
    builder->CreateCall(clock_gettime_fn, {builder->getInt32(1), ts_ptr});

    // Load ts.tv_sec
    llvm::Value *tv_sec_ptr = builder->CreateStructGEP(timespec_type, ts_ptr, 0, "tv_sec_ptr");
    llvm::Value *tv_sec = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), tv_sec_ptr, "tv_sec");

    // Load ts.tv_nsec
    llvm::Value *tv_nsec_ptr = builder->CreateStructGEP(timespec_type, ts_ptr, 1, "tv_nsec_ptr");
    llvm::Value *tv_nsec = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), tv_nsec_ptr, "tv_nsec");

    // Calculate: tv_sec * 1000000000ULL + tv_nsec
    llvm::Value *tv_sec_ns = builder->CreateMul(tv_sec, builder->getInt64(1000000000ULL), "tv_sec_ns");
    llvm::Value *stamp_value = builder->CreateAdd(tv_sec_ns, tv_nsec, "stamp_value");

#endif

    // Allocate TimeStamp using dima.allocate(dima.head.TimeStamp)
    auto *timestamp_head = time_dima_heads.at("TimeStamp");
    llvm::Function *dima_allocate = DIMA::dima_functions.at("allocate");
    llvm::Value *timestamp_ptr = builder->CreateCall(dima_allocate, {timestamp_head}, "timestamp_ptr");

    // Set the value field: stamp->value = stamp_value
    llvm::Value *value_ptr = builder->CreateStructGEP(timestamp_type, timestamp_ptr, 0, "value_ptr");
    IR::aligned_store(*builder, stamp_value, value_ptr);

    // Return the pointer to the heap-allocated TimeStamp
    builder->CreateRet(timestamp_ptr);
}

void Generator::Module::Time::generate_duration_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // Duration *duration(TimeStamp *t1, TimeStamp *t2) {
    //     Duration *d = (Duration *)dima_allocate(dima.head.Duration);
    //     // Handle both forward and backward time differences
    //     if (t2->value >= t1->value) {
    //         d->value = t2->value - t1->value;
    //     } else {
    //         d->value = t1->value - t2->value;
    //     }
    //     return d;
    // }
    llvm::StructType *timestamp_type = time_data_types.at("TimeStamp");
    llvm::StructType *duration_type = time_data_types.at("Duration");

    llvm::FunctionType *duration_fn_type = llvm::FunctionType::get( //
        duration_type->getPointerTo(),                              // Return type: Duration*
        {
            timestamp_type->getPointerTo(), // TimeStamp* t1
            timestamp_type->getPointerTo()  // TimeStamp* t2
        },
        false // No vaargs
    );
    llvm::Function *duration_fn = llvm::Function::Create( //
        duration_fn_type,                                 //
        llvm::Function::ExternalLinkage,                  //
        prefix + "duration",                              //
        module                                            //
    );
    time_functions["duration"] = duration_fn;
    if (only_declarations) {
        return;
    }

    // Get the arguments
    llvm::Argument *arg_t1 = duration_fn->arg_begin();
    arg_t1->setName("t1");
    llvm::Argument *arg_t2 = arg_t1 + 1;
    arg_t2->setName("t2");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", duration_fn);
    llvm::BasicBlock *forward_block = llvm::BasicBlock::Create(context, "forward", duration_fn);
    llvm::BasicBlock *backward_block = llvm::BasicBlock::Create(context, "backward", duration_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", duration_fn);

    // Entry block: load t1->value and t2->value
    builder->SetInsertPoint(entry_block);

    // Load t1->value
    llvm::Value *t1_value_ptr = builder->CreateStructGEP(timestamp_type, arg_t1, 0, "t1_value_ptr");
    llvm::Value *t1_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), t1_value_ptr, "t1_value");

    // Load t2->value
    llvm::Value *t2_value_ptr = builder->CreateStructGEP(timestamp_type, arg_t2, 0, "t2_value_ptr");
    llvm::Value *t2_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), t2_value_ptr, "t2_value");

    // Compare: t2->value >= t1->value
    llvm::Value *t2_gte_t1 = builder->CreateICmpUGE(t2_value, t1_value, "t2_gte_t1");
    builder->CreateCondBr(t2_gte_t1, forward_block, backward_block);

    // Forward block: d->value = t2->value - t1->value
    builder->SetInsertPoint(forward_block);
    llvm::Value *forward_diff = builder->CreateSub(t2_value, t1_value, "forward_diff");
    builder->CreateBr(merge_block);

    // Backward block: d->value = t1->value - t2->value
    builder->SetInsertPoint(backward_block);
    llvm::Value *backward_diff = builder->CreateSub(t1_value, t2_value, "backward_diff");
    builder->CreateBr(merge_block);

    // Merge block: allocate Duration and set value
    builder->SetInsertPoint(merge_block);
    llvm::PHINode *diff_value = builder->CreatePHI(llvm::Type::getInt64Ty(context), 2, "diff_value");
    diff_value->addIncoming(forward_diff, forward_block);
    diff_value->addIncoming(backward_diff, backward_block);

    // Allocate Duration using dima.allocate(dima.head.Duration)
    auto *duration_head = time_dima_heads.at("Duration");
    llvm::Function *dima_allocate = DIMA::dima_functions.at("allocate");
    llvm::Value *duration_ptr = builder->CreateCall(dima_allocate, {duration_head}, "duration_ptr");

    // Set the value field: d->value = diff_value
    llvm::Value *duration_value_ptr = builder->CreateStructGEP(duration_type, duration_ptr, 0, "duration_value_ptr");
    IR::aligned_store(*builder, diff_value, duration_value_ptr);

    // Return the pointer to the heap-allocated Duration
    builder->CreateRet(duration_ptr);
}

void Generator::Module::Time::generate_sleep_duration_function( //
    llvm::IRBuilder<> *builder,                                 //
    llvm::Module *module,                                       //
    const bool only_declarations                                //
) {
    // THE C IMPLEMENTATION:
    // void sleep_duration(Duration *d) {
    // #ifdef __WIN32__
    //     // Windows Sleep takes milliseconds
    //     uint64_t ms = d->value / 1000000ULL;
    //     if (ms == 0 && d->value > 0) {
    //         ms = 1; // Minimum 1ms on Windows
    //     }
    //     Sleep((DWORD)ms);
    // #else
    //     // POSIX nanosleep
    //     struct timespec ts;
    //     ts.tv_sec = (time_t)(d->value / 1000000000ULL);
    //     ts.tv_nsec = (long)(d->value % 1000000000ULL);
    //     nanosleep(&ts, NULL);
    // #endif
    // }
    llvm::StructType *duration_type = time_data_types.at("Duration");

    llvm::FunctionType *sleep_duration_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                                // Return type: void
        {duration_type->getPointerTo()},                               // Duration* d
        false                                                          // No vaargs
    );
    llvm::Function *sleep_duration_fn = llvm::Function::Create( //
        sleep_duration_type,                                    //
        llvm::Function::ExternalLinkage,                        //
        prefix + "sleep_duration",                              //
        module                                                  //
    );
    time_functions["sleep_duration"] = sleep_duration_fn;
    if (only_declarations) {
        return;
    }

    // Get the argument
    llvm::Argument *arg_d = sleep_duration_fn->arg_begin();
    arg_d->setName("d");

    // Create entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", sleep_duration_fn);
    builder->SetInsertPoint(entry_block);

    // Load d->value
    llvm::Value *d_value_ptr = builder->CreateStructGEP(duration_type, arg_d, 0, "d_value_ptr");
    llvm::Value *d_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), d_value_ptr, "d_value");

#ifdef __WIN32__
    // Windows implementation using Sleep(ms)

    // Convert nanoseconds to milliseconds: ms = d->value / 1000000ULL
    llvm::Value *ms = builder->CreateUDiv(d_value, builder->getInt64(1000000ULL), "ms");

    // Check if ms == 0 && d->value > 0
    llvm::Value *ms_is_zero = builder->CreateICmpEQ(ms, builder->getInt64(0), "ms_is_zero");
    llvm::Value *d_value_gt_zero = builder->CreateICmpUGT(d_value, builder->getInt64(0), "d_value_gt_zero");
    llvm::Value *needs_min_sleep = builder->CreateAnd(ms_is_zero, d_value_gt_zero, "needs_min_sleep");

    // If true, set ms = 1 (minimum 1ms on Windows)
    llvm::Value *final_ms = builder->CreateSelect(needs_min_sleep, builder->getInt64(1), ms, "final_ms");

    // Truncate to i32 for Sleep(DWORD)
    llvm::Value *ms_i32 = builder->CreateTrunc(final_ms, llvm::Type::getInt32Ty(context), "ms_i32");

    // Declare/get Sleep function
    llvm::FunctionType *sleep_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                       // void return
        {llvm::Type::getInt32Ty(context)},                    // DWORD dwMilliseconds
        false                                                 // No vaargs
    );
    llvm::Function *sleep_fn = llvm::cast<llvm::Function>(module->getOrInsertFunction("Sleep", sleep_type).getCallee());

    // Call Sleep(ms)
    builder->CreateCall(sleep_fn, {ms_i32});
#else
    // Linux/POSIX implementation using nanosleep

    llvm::StructType *timespec_type = time_data_types.at("c.struct.timespec");
    llvm::Function *nanosleep_fn = time_platform_functions.at("nanosleep");

    // Create struct timespec on stack
    llvm::Value *ts_ptr = builder->CreateAlloca(timespec_type, nullptr, "ts_ptr");

    // ts.tv_sec = d->value / 1000000000ULL
    llvm::Value *tv_sec = builder->CreateUDiv(d_value, builder->getInt64(1000000000ULL), "tv_sec");
    llvm::Value *tv_sec_ptr = builder->CreateStructGEP(timespec_type, ts_ptr, 0, "tv_sec_ptr");
    IR::aligned_store(*builder, tv_sec, tv_sec_ptr);

    // ts.tv_nsec = d->value % 1000000000ULL
    llvm::Value *tv_nsec = builder->CreateURem(d_value, builder->getInt64(1000000000ULL), "tv_nsec");
    llvm::Value *tv_nsec_ptr = builder->CreateStructGEP(timespec_type, ts_ptr, 1, "tv_nsec_ptr");
    IR::aligned_store(*builder, tv_nsec, tv_nsec_ptr);

    // Call nanosleep(&ts, NULL)
    builder->CreateCall(nanosleep_fn, {ts_ptr, llvm::ConstantPointerNull::get(timespec_type->getPointerTo())});
#endif
    builder->CreateRetVoid();
}

void Generator::Module::Time::generate_sleep_time_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // void sleep_time(uint64_t t, TimeUnit u) {
    //     uint64_t ns;
    //     switch (u) {
    //         case TIME_UNIT_NS:
    //             ns = t;
    //             break;
    //         case TIME_UNIT_US:
    //             ns = t * 1000ULL;
    //             break;
    //         case TIME_UNIT_MS:
    //             ns = t * 1000000ULL;
    //             break;
    //         case TIME_UNIT_S:
    //             ns = t * 1000000000ULL;
    //             break;
    //         default:
    //             ns = 0;
    //             break;
    //     }
    //     Duration d = {ns};
    //     sleep_duration(&d);
    // }
    llvm::StructType *duration_type = time_data_types.at("Duration");
    llvm::Function *sleep_duration_fn = time_functions.at("sleep_duration");

    llvm::FunctionType *sleep_time_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                            // Return type: void
        {
            llvm::Type::getInt64Ty(context), // u64 t
            llvm::Type::getInt32Ty(context)  // i32 u (TimeUnit enum)
        },
        false // No vaargs
    );
    llvm::Function *sleep_time_fn = llvm::Function::Create( //
        sleep_time_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        prefix + "sleep_time",                              //
        module                                              //
    );
    time_functions["sleep_time"] = sleep_time_fn;
    if (only_declarations) {
        return;
    }

    // Get the arguments
    llvm::Argument *arg_t = sleep_time_fn->arg_begin();
    arg_t->setName("t");
    llvm::Argument *arg_u = arg_t + 1;
    arg_u->setName("u");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", sleep_time_fn);
    llvm::BasicBlock *case_ns_block = llvm::BasicBlock::Create(context, "case_ns", sleep_time_fn);
    llvm::BasicBlock *case_us_block = llvm::BasicBlock::Create(context, "case_us", sleep_time_fn);
    llvm::BasicBlock *case_ms_block = llvm::BasicBlock::Create(context, "case_ms", sleep_time_fn);
    llvm::BasicBlock *case_s_block = llvm::BasicBlock::Create(context, "case_s", sleep_time_fn);
    llvm::BasicBlock *default_block = llvm::BasicBlock::Create(context, "default", sleep_time_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", sleep_time_fn);

    // Entry block: create switch
    builder->SetInsertPoint(entry_block);
    llvm::SwitchInst *switch_inst = builder->CreateSwitch(arg_u, default_block, 4);
    switch_inst->addCase(builder->getInt32(0), case_ns_block); // TIME_UNIT_NS = 0
    switch_inst->addCase(builder->getInt32(1), case_us_block); // TIME_UNIT_US = 1
    switch_inst->addCase(builder->getInt32(2), case_ms_block); // TIME_UNIT_MS = 2
    switch_inst->addCase(builder->getInt32(3), case_s_block);  // TIME_UNIT_S = 3

    // Case NS: ns = t
    builder->SetInsertPoint(case_ns_block);
    llvm::Value *ns_ns = arg_t;
    builder->CreateBr(merge_block);

    // Case US: ns = t * 1000ULL
    builder->SetInsertPoint(case_us_block);
    llvm::Value *ns_us = builder->CreateMul(arg_t, builder->getInt64(1000ULL), "ns_us");
    builder->CreateBr(merge_block);

    // Case MS: ns = t * 1000000ULL
    builder->SetInsertPoint(case_ms_block);
    llvm::Value *ns_ms = builder->CreateMul(arg_t, builder->getInt64(1000000ULL), "ns_ms");
    builder->CreateBr(merge_block);

    // Case S: ns = t * 1000000000ULL
    builder->SetInsertPoint(case_s_block);
    llvm::Value *ns_s = builder->CreateMul(arg_t, builder->getInt64(1000000000ULL), "ns_s");
    builder->CreateBr(merge_block);

    // Default: ns = 0
    builder->SetInsertPoint(default_block);
    llvm::Value *ns_default = builder->getInt64(0);
    builder->CreateBr(merge_block);

    // Merge block: create Duration and call sleep_duration
    builder->SetInsertPoint(merge_block);
    llvm::PHINode *ns_value = builder->CreatePHI(llvm::Type::getInt64Ty(context), 5, "ns_value");
    ns_value->addIncoming(ns_ns, case_ns_block);
    ns_value->addIncoming(ns_us, case_us_block);
    ns_value->addIncoming(ns_ms, case_ms_block);
    ns_value->addIncoming(ns_s, case_s_block);
    ns_value->addIncoming(ns_default, default_block);

    // Allocate Duration using dima.allocate(dima.head.Duration)
    auto *duration_head = time_dima_heads.at("Duration");
    llvm::Function *dima_allocate_fn = DIMA::dima_functions.at("allocate");
    llvm::Value *duration_ptr = builder->CreateCall(dima_allocate_fn, {duration_head}, "duration_ptr");

    // Set d->value = ns_value
    llvm::Value *duration_value_ptr = builder->CreateStructGEP(duration_type, duration_ptr, 0, "duration_value_ptr");
    IR::aligned_store(*builder, ns_value, duration_value_ptr);

    // Call sleep_duration(&d)
    builder->CreateCall(sleep_duration_fn, {duration_ptr});

    // Free the duration
    llvm::Function *dima_release_fn = DIMA::dima_functions.at("release");
    builder->CreateCall(dima_release_fn, {duration_head, duration_ptr});

    builder->CreateRetVoid();
}

void Generator::Module::Time::generate_as_unit_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // double as_unit(Duration *d, TimeUnit u) {
    //     switch (u) {
    //         case TIME_UNIT_NS:
    //             return (double)d->value;
    //         case TIME_UNIT_US:
    //             return (double)d->value / 1000.0;
    //         case TIME_UNIT_MS:
    //             return (double)d->value / 1000000.0;
    //         case TIME_UNIT_S:
    //             return (double)d->value / 1000000000.0;
    //     }
    // }
    llvm::StructType *duration_type = time_data_types.at("Duration");

    llvm::FunctionType *as_unit_type = llvm::FunctionType::get( //
        llvm::Type::getDoubleTy(context),                       // Return type: f64
        {
            duration_type->getPointerTo(),  // Duration* d
            llvm::Type::getInt32Ty(context) // i32 u (TimeUnit enum)
        },
        false // No vaargs
    );
    llvm::Function *as_unit_fn = llvm::Function::Create( //
        as_unit_type,                                    //
        llvm::Function::ExternalLinkage,                 //
        prefix + "as_unit",                              //
        module                                           //
    );
    time_functions["as_unit"] = as_unit_fn;
    if (only_declarations) {
        return;
    }

    // Get the arguments
    llvm::Argument *arg_d = as_unit_fn->arg_begin();
    arg_d->setName("d");
    llvm::Argument *arg_u = arg_d + 1;
    arg_u->setName("u");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", as_unit_fn);
    llvm::BasicBlock *case_ns_block = llvm::BasicBlock::Create(context, "case_ns", as_unit_fn);
    llvm::BasicBlock *case_us_block = llvm::BasicBlock::Create(context, "case_us", as_unit_fn);
    llvm::BasicBlock *case_ms_block = llvm::BasicBlock::Create(context, "case_ms", as_unit_fn);
    llvm::BasicBlock *case_s_block = llvm::BasicBlock::Create(context, "case_s", as_unit_fn);
    llvm::BasicBlock *default_block = llvm::BasicBlock::Create(context, "default", as_unit_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", as_unit_fn);

    // Entry block: load d->value and create switch
    builder->SetInsertPoint(entry_block);
    llvm::Value *d_value_ptr = builder->CreateStructGEP(duration_type, arg_d, 0, "d_value_ptr");
    llvm::Value *d_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), d_value_ptr, "d_value");

    llvm::SwitchInst *switch_inst = builder->CreateSwitch(arg_u, default_block, 4);
    switch_inst->addCase(builder->getInt32(0), case_ns_block); // TIME_UNIT_NS = 0
    switch_inst->addCase(builder->getInt32(1), case_us_block); // TIME_UNIT_US = 1
    switch_inst->addCase(builder->getInt32(2), case_ms_block); // TIME_UNIT_MS = 2
    switch_inst->addCase(builder->getInt32(3), case_s_block);  // TIME_UNIT_S = 3

    // Case NS: return (double)d->value
    builder->SetInsertPoint(case_ns_block);
    llvm::Value *result_ns = builder->CreateUIToFP(d_value, llvm::Type::getDoubleTy(context), "result_ns");
    builder->CreateBr(merge_block);

    // Case US: return (double)d->value / 1000.0
    builder->SetInsertPoint(case_us_block);
    llvm::Value *d_value_f64_us = builder->CreateUIToFP(d_value, llvm::Type::getDoubleTy(context), "d_value_f64_us");
    llvm::Value *result_us =
        builder->CreateFDiv(d_value_f64_us, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 1000.0), "result_us");
    builder->CreateBr(merge_block);

    // Case MS: return (double)d->value / 1000000.0
    builder->SetInsertPoint(case_ms_block);
    llvm::Value *d_value_f64_ms = builder->CreateUIToFP(d_value, llvm::Type::getDoubleTy(context), "d_value_f64_ms");
    llvm::Value *result_ms =
        builder->CreateFDiv(d_value_f64_ms, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 1000000.0), "result_ms");
    builder->CreateBr(merge_block);

    // Case S: return (double)d->value / 1000000000.0
    builder->SetInsertPoint(case_s_block);
    llvm::Value *d_value_f64_s = builder->CreateUIToFP(d_value, llvm::Type::getDoubleTy(context), "d_value_f64_s");
    llvm::Value *result_s =
        builder->CreateFDiv(d_value_f64_s, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 1000000000.0), "result_s");
    builder->CreateBr(merge_block);

    // Default: return 0.0 (invalid unit)
    builder->SetInsertPoint(default_block);
    llvm::Value *default_value = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 0.0);
    builder->CreateBr(merge_block);

    // Merge: select the correct value depending on which block we come from, release the input argument and return the value
    builder->SetInsertPoint(merge_block);
    llvm::PHINode *return_value = builder->CreatePHI(builder->getDoubleTy(), 5);
    return_value->addIncoming(result_ns, case_ns_block);
    return_value->addIncoming(result_us, case_us_block);
    return_value->addIncoming(result_ms, case_ms_block);
    return_value->addIncoming(result_s, case_s_block);
    return_value->addIncoming(default_value, default_block);
    builder->CreateRet(return_value);
}

void Generator::Module::Time::generate_from_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // THE C IMPLEMENTATION:
    // Duration *from(uint64_t t, TimeUnit u) {
    //     Duration *d = (Duration *)malloc(sizeof(Duration));
    //     switch (u) {
    //         case TIME_UNIT_NS:
    //             d->value = t;
    //             break;
    //         case TIME_UNIT_US:
    //             d->value = t * 1000ULL;
    //             break;
    //         case TIME_UNIT_MS:
    //             d->value = t * 1000000ULL;
    //             break;
    //         case TIME_UNIT_S:
    //             d->value = t * 1000000000ULL;
    //             break;
    //         default:
    //             d->value = 0;
    //             break;
    //     }
    //     return d;
    // }
    llvm::StructType *duration_type = time_data_types.at("Duration");

    llvm::FunctionType *from_type = llvm::FunctionType::get( //
        duration_type->getPointerTo(),                       // Return type: Duration*
        {
            llvm::Type::getInt64Ty(context), // u64 t
            llvm::Type::getInt32Ty(context)  // i32 u (TimeUnit enum)
        },
        false // No vaargs
    );
    llvm::Function *from_fn = llvm::Function::Create( //
        from_type,                                    //
        llvm::Function::ExternalLinkage,              //
        prefix + "from",                              //
        module                                        //
    );
    time_functions["from"] = from_fn;
    if (only_declarations) {
        return;
    }

    // Get the arguments
    llvm::Argument *arg_t = from_fn->arg_begin();
    arg_t->setName("t");
    llvm::Argument *arg_u = arg_t + 1;
    arg_u->setName("u");

    // Create basic blocks
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", from_fn);
    llvm::BasicBlock *case_ns_block = llvm::BasicBlock::Create(context, "case_ns", from_fn);
    llvm::BasicBlock *case_us_block = llvm::BasicBlock::Create(context, "case_us", from_fn);
    llvm::BasicBlock *case_ms_block = llvm::BasicBlock::Create(context, "case_ms", from_fn);
    llvm::BasicBlock *case_s_block = llvm::BasicBlock::Create(context, "case_s", from_fn);
    llvm::BasicBlock *default_block = llvm::BasicBlock::Create(context, "default", from_fn);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", from_fn);

    // Entry block: allocate Duration and create switch
    builder->SetInsertPoint(entry_block);

    // Allocate Duration using dima.allocate(dima.head.Duration)
    auto *duration_head = time_dima_heads.at("Duration");
    llvm::Function *dima_allocate_fn = DIMA::dima_functions.at("allocate");
    llvm::Value *duration_ptr = builder->CreateCall(dima_allocate_fn, {duration_head}, "duration_ptr");

    llvm::SwitchInst *switch_inst = builder->CreateSwitch(arg_u, default_block, 4);
    switch_inst->addCase(builder->getInt32(0), case_ns_block); // TIME_UNIT_NS = 0
    switch_inst->addCase(builder->getInt32(1), case_us_block); // TIME_UNIT_US = 1
    switch_inst->addCase(builder->getInt32(2), case_ms_block); // TIME_UNIT_MS = 2
    switch_inst->addCase(builder->getInt32(3), case_s_block);  // TIME_UNIT_S = 3

    // Case NS: d->value = t
    builder->SetInsertPoint(case_ns_block);
    llvm::Value *value_ns = arg_t;
    builder->CreateBr(merge_block);

    // Case US: d->value = t * 1000ULL
    builder->SetInsertPoint(case_us_block);
    llvm::Value *value_us = builder->CreateMul(arg_t, builder->getInt64(1000ULL), "value_us");
    builder->CreateBr(merge_block);

    // Case MS: d->value = t * 1000000ULL
    builder->SetInsertPoint(case_ms_block);
    llvm::Value *value_ms = builder->CreateMul(arg_t, builder->getInt64(1000000ULL), "value_ms");
    builder->CreateBr(merge_block);

    // Case S: d->value = t * 1000000000ULL
    builder->SetInsertPoint(case_s_block);
    llvm::Value *value_s = builder->CreateMul(arg_t, builder->getInt64(1000000000ULL), "value_s");
    builder->CreateBr(merge_block);

    // Default: d->value = 0
    builder->SetInsertPoint(default_block);
    llvm::Value *value_default = builder->getInt64(0);
    builder->CreateBr(merge_block);

    // Merge block: set d->value and return
    builder->SetInsertPoint(merge_block);
    llvm::PHINode *duration_value = builder->CreatePHI(llvm::Type::getInt64Ty(context), 5, "duration_value");
    duration_value->addIncoming(value_ns, case_ns_block);
    duration_value->addIncoming(value_us, case_us_block);
    duration_value->addIncoming(value_ms, case_ms_block);
    duration_value->addIncoming(value_s, case_s_block);
    duration_value->addIncoming(value_default, default_block);

    // Set d->value
    llvm::Value *duration_value_ptr = builder->CreateStructGEP(duration_type, duration_ptr, 0, "duration_value_ptr");
    IR::aligned_store(*builder, duration_value, duration_value_ptr);

    // Return the pointer to the heap-allocated Duration
    builder->CreateRet(duration_ptr);
}
