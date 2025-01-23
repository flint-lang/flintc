#include "profiler.hpp"

std::map<std::string, ProfileData> Profiler::profiles;

ScopeProfiler Profiler::start_scope(const std::string &task_name) {
    ScopeProfiler scope_profiler(task_name);
    return scope_profiler;
}

void Profiler::start_task(const std::string &task) {
    // Ensure the task was not already started
    if (profiles.find(task) != profiles.end()) {
        std::cerr << "Error: Task \"" << task << "\" was already started.\n";
        return;
    }
    // Record the start time
    profiles[task].start = std::chrono::high_resolution_clock::now();
}

void Profiler::end_task(const std::string &task) {
    // Ensure the task was started
    if (profiles.find(task) == profiles.end()) {
        std::cerr << "Error: Task \"" << task << "\" was not started.\n";
        return;
    }
    // Record the end time
    profiles[task].end = std::chrono::high_resolution_clock::now();
}

void Profiler::print_results(TimeUnit unit) {
    switch (unit) {
        case TimeUnit::NS:
            print_results<std::chrono::nanoseconds>("ns");
            break;
        case TimeUnit::MICS:
            print_results<std::chrono::microseconds>("µs");
            break;
        case TimeUnit::MILLIS:
            print_results<std::chrono::milliseconds>("ms");
            break;
        case TimeUnit::SEC:
            print_results<std::chrono::seconds>("s");
            break;
    }
}
