#include "generator/generator.hpp"

void Generator::Module::Time::generate_time_functions([[maybe_unused]] llvm::IRBuilder<> *builder, [[maybe_unused]] llvm::Module *module,
    [[maybe_unused]] const bool only_declarations) {
    // TODO:
}

void Generator::Module::Time::generate_now_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    // static TimeStamp now(void) {
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
