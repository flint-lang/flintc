#include "generator/generator.hpp"
#include "lexer/builtins.hpp"

static const std::string hash = Hash(std::string("time")).to_string();

void Generator::Module::Time::generate_time_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_types(module);
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

void Generator::Module::Time::generate_now_function([[maybe_unused]] llvm::IRBuilder<> *builder, [[maybe_unused]] llvm::Module *module,
    [[maybe_unused]] const bool only_declarations) {
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
