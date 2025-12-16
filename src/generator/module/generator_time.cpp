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
    // TimeStamp now() {
    //     TimeStamp stamp;
    //
    // #ifdef __WIN32__
    //     __time_init();
    //     LARGE_INTEGER counter;
    //     QueryPerformanceCounter(&counter);
    //     // Convert to nanoseconds: (counter * 1e9) / frequency
    //     // Typical frequency: 10 MHz → 100ns resolution
    //     stamp.value = (uint64_t)((counter.QuadPart * 1000000000ULL) / __time_frequency.QuadPart);
    //
    // #elif defined(__APPLE__)
    //     __time_init();
    //     uint64_t abs_time = mach_absolute_time();
    //     // Convert to nanoseconds
    //     stamp.value = (abs_time * __time_info.numer) / __time_info.denom;
    //
    // #else
    //     // Linux/POSIX - use CLOCK_MONOTONIC for monotonic time
    //     struct timespec ts;
    //     clock_gettime(CLOCK_MONOTONIC, &ts);
    //     stamp.value = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    // #endif
    //
    //     return stamp;
    // }
}
