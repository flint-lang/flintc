#include "generator/generator.hpp"
#include "lexer/builtins.hpp"

static const std::string hash = Hash(std::string("time")).to_string();

void Generator::Module::Time::generate_time_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_types(module);
    generate_platform_functions(module);
    generate_time_init_function(builder, module, only_declarations);
    generate_now_function(builder, module, only_declarations);
}

void Generator::Module::Time::generate_types(llvm::Module *module) {
    // Create the data types of this module
    assert(core_module_data_types.find("time") != core_module_data_types.end());
    const auto &data_types = core_module_data_types.at("time");
    for (const data_type &type : data_types) {
        const std::string type_name(std::get<0>(type));
        const std::vector<data_field> &fields = std::get<1>(type);
        assert(time_data_types.find(type_name) == time_data_types.end());
        std::vector<llvm::Type *> types_vec;
        for (const auto &[field_type, field_name] : fields) {
            types_vec.emplace_back(IR::get_type(module, Type::get_primitive_type(std::string(field_type))).first);
        }
        llvm::ArrayRef<llvm::Type *> return_types_arr(types_vec);
        time_data_types[type_name] = llvm::StructType::create( //
            context,                                           //
            return_types_arr,                                  //
            hash + ".data." + type_name,                       //
            false                                              //
        );
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
        IR::generate_enum_value_strings(module, hash, enum_name, enum_values);
    }
}

void Generator::Module::Time::generate_platform_functions(llvm::Module *module) {
#ifdef __WIN32__
    // Windows-specific functions

    // QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount)
    llvm::StructType *large_integer_type = llvm::StructType::create(context, {llvm::Type::getInt64Ty(context)}, "LARGE_INTEGER");
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
    // Create __time_init function
    llvm::FunctionType *init_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
    llvm::Function *init_fn = llvm::Function::Create( //
        init_type,                                    //
        llvm::Function::InternalLinkage,              //
        hash + ".time_init",                          //
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
        hash + ".global.time_frequency"                             // Name of the global
    );

    // Create global for initialized flag
    llvm::GlobalVariable *init_global = new llvm::GlobalVariable(  //
        *module,                                                   // Module
        llvm::Type::getInt1Ty(context),                            // bool type
        false,                                                     // not constant
        llvm::GlobalValue::InternalLinkage,                        // Only used within this module
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0), // Initial value of 'false'
        hash + ".global.time_initialized"                          //
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
    llvm::Function *malloc_fn = c_functions.at(MALLOC);

    llvm::FunctionType *now_type = llvm::FunctionType::get( //
        timestamp_type->getPointerTo(),                     // Return type: TimeStamp*
        false                                               // No arguments
    );
    llvm::Function *now_fn = llvm::Function::Create( //
        now_type,                                    //
        llvm::Function::ExternalLinkage,             //
        hash + ".now",                               //
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
    llvm::StructType *large_integer_type = llvm::cast<llvm::StructType>(module->getTypeByName("LARGE_INTEGER"));
    llvm::Value *counter_ptr = builder->CreateAlloca(large_integer_type, nullptr, "counter_ptr");

    // Call QueryPerformanceCounter(&counter)
    llvm::Function *qpc_fn = time_platform_functions.at("QueryPerformanceCounter");
    builder->CreateCall(qpc_fn, {counter_ptr});

    // Load counter value (counter.QuadPart)
    llvm::Value *counter_field_ptr = builder->CreateStructGEP(large_integer_type, counter_ptr, 0, "counter_field_ptr");
    llvm::Value *counter_value = IR::aligned_load(*builder, llvm::Type::getInt64Ty(context), counter_field_ptr, "counter_value");

    // Load frequency from global
    llvm::GlobalVariable *freq_global = module->getNamedGlobal(hash + ".global.time_frequency");
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

    // Allocate TimeStamp on heap: malloc(sizeof(TimeStamp))
    llvm::Value *malloc_size = builder->getInt64(Allocation::get_type_size(module, timestamp_type));
    llvm::Value *timestamp_ptr = builder->CreateCall(malloc_fn, {malloc_size}, "timestamp_ptr");

    // Set the value field: stamp->value = stamp_value
    llvm::Value *value_ptr = builder->CreateStructGEP(timestamp_type, timestamp_ptr, 0, "value_ptr");
    IR::aligned_store(*builder, stamp_value, value_ptr);

    // Return the pointer to the heap-allocated TimeStamp
    builder->CreateRet(timestamp_ptr);
}
