#include "profiler.hpp"

#include "colors.hpp"
#include "globals.hpp"

#include <algorithm>
#include <iomanip>

std::vector<std::shared_ptr<ProfileNode>> Profiler::root_nodes;
std::stack<std::shared_ptr<ProfileNode>> Profiler::profile_stack;
std::map<std::string, std::shared_ptr<ProfileNode>> Profiler::active_tasks;
thread_local std::stack<CumulativeProfiler *> CumulativeProfiler::cumulative_stack;

ScopeProfiler Profiler::start_scope(const std::string &task_name) {
    ScopeProfiler scope_profiler(task_name);
    return scope_profiler;
}

void Profiler::start_task(const std::string &task, const bool special_task) {
    // Ensure the task was not already started
    if (active_tasks.find(task) != active_tasks.end()) {
        std::cerr << "Error: Task \"" << task << "\" was already started.\n";
        return;
    }

    // Create a new profile node
    auto node = std::make_shared<ProfileNode>(task);
    active_tasks[task] = node;

    if (special_task) {
        profiling_durations.emplace(task, node.get());
    }

    // Add to parent if we have one
    auto current = current_node();
    if (current) {
        current->children.push_back(node);
    } else {
        // No parent, this is a root node
        root_nodes.push_back(node);
    }

    // Push this node onto the stack
    profile_stack.push(node);
}

void Profiler::end_task(const std::string &task) {
    // Ensure the task was started
    if (active_tasks.find(task) == active_tasks.end()) {
        std::cerr << "Error: Task \"" << task << "\" was not started.\n";
        return;
    }

    // Get the node and record end time
    auto node = active_tasks[task];
    node->end = std::chrono::high_resolution_clock::now();

    // Pop from stack only if this is the top node
    if (!profile_stack.empty() && profile_stack.top() == node) {
        profile_stack.pop();
    }

    // Remove from active tasks
    active_tasks.erase(task);
}

void Profiler::print_results(TimeUnit unit) {
    if (!DEBUG_MODE) {
        return;
    }
    std::cout << YELLOW << "[Debug Info] Printing results of the profiler" << DEFAULT << std::endl;
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

uint64_t Profiler::calibrate_profiler_overhead(size_t iterations) {
    if (calibrating) {
        return 0; // Prevent recursion
    }
    calibrating = true;

    // Warm-up phase to stabilize CPU caches and branch predictor
    for (size_t i = 0; i < 100; ++i) {
        CumulativeProfiler calibration_profiler("__calibration__", true);
    }

    // Actual measurement phase - measure total time and divide by iterations
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        CumulativeProfiler calibration_profiler("__calibration__", true);
    }
    auto end = std::chrono::high_resolution_clock::now();

    calibrating = false;

    // Calculate average overhead per profiler instance
    uint64_t total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    uint64_t average = total_ns / iterations;

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Profiler overhead calibrated: " << average << " ns (average of " << iterations
                  << " iterations)" << DEFAULT << std::endl;
    }

    return average;
}

uint64_t Profiler::get_profiler_overhead() {
    if (profiler_overhead_ns == 0 && !calibrating) {
        profiler_overhead_ns = calibrate_profiler_overhead();
    }
    return profiler_overhead_ns;
}

void Profiler::record_cumulative(const std::string &key, uint64_t exclusive_ns, uint64_t inclusive_ns) {
    // Subtract profiler overhead (but don't go negative)
    uint64_t overhead = get_profiler_overhead();
    uint64_t corrected_exclusive = exclusive_ns > overhead ? exclusive_ns - overhead : 0;
    uint64_t corrected_inclusive = inclusive_ns > overhead ? inclusive_ns - overhead : 0;

    auto &stats = cumulative_stats[key];
    stats.name = key;
    stats.call_count++;
    stats.exclusive_time_ns += corrected_exclusive;
    stats.inclusive_time_ns += corrected_inclusive;
}

void Profiler::print_cumulative_stats(const std::string &sort_by) {
    if (!DEBUG_MODE) {
        return;
    }

    if (cumulative_stats.empty()) {
        std::cout << "No cumulative profiling data available.\n";
        return;
    }

    std::cout << "\n" << YELLOW << "[Debug Info] Cumulative Profiling Statistics" << DEFAULT << std::endl;

    // Convert to vector for sorting
    std::vector<CumulativeStats> stats_vec;
    stats_vec.reserve(cumulative_stats.size());
    for (const auto &[_, stats] : cumulative_stats) {
        stats_vec.push_back(stats);
    }

    // Sort based on criteria
    if (sort_by == "calls") {
        std::sort(stats_vec.begin(), stats_vec.end(), [](const auto &a, const auto &b) { return a.call_count > b.call_count; });
    } else if (sort_by == "average") {
        std::sort(stats_vec.begin(), stats_vec.end(),
            [](const auto &a, const auto &b) { return a.average_exclusive_ns() > b.average_exclusive_ns(); });
    } else { // "total" or default
        std::sort(stats_vec.begin(), stats_vec.end(),
            [](const auto &a, const auto &b) { return a.exclusive_time_ns > b.exclusive_time_ns; });
    }

    // Print header
    std::cout << std::left << std::setw(40) << "Name" << std::right << std::setw(12) << "Calls" << std::setw(18) << "Exclusive (µs)"
              << std::setw(18) << "Inclusive (µs)" << std::setw(16) << "Avg Excl (µs)" << std::setw(16) << "Avg Incl (µs)" << std::setw(12)
              << "% of Total"
              << "\n";
    std::cout << std::string(132, '-') << "\n";

    // Calculate total exclusive time for percentages
    uint64_t grand_total = 0;
    for (const auto &stat : stats_vec) {
        grand_total += stat.exclusive_time_ns;
    }

    // Print each stat
    for (const auto &stat : stats_vec) {
        double exclusive_us = stat.exclusive_time_ns / 1000.0;
        double inclusive_us = stat.inclusive_time_ns / 1000.0;
        double avg_excl_us = stat.average_exclusive_ns() / 1000.0;
        double avg_incl_us = stat.average_inclusive_ns() / 1000.0;
        double percentage = grand_total > 0 ? (100.0 * stat.exclusive_time_ns / grand_total) : 0.0;

        std::cout << std::left << std::setw(40) << stat.name << std::right << std::setw(12) << format_with_separator(stat.call_count)
                  << std::setw(18) << std::fixed << std::setprecision(2) << exclusive_us << std::setw(18) << std::fixed
                  << std::setprecision(2) << inclusive_us << std::setw(16) << std::fixed << std::setprecision(2) << avg_excl_us
                  << std::setw(16) << std::fixed << std::setprecision(2) << avg_incl_us << std::setw(11) << std::fixed
                  << std::setprecision(1) << percentage << "%"
                  << "\n";
    }

    std::cout << std::string(132, '-') << "\n";
    std::cout << "Total Exclusive: " << format_with_separator(grand_total / 1000) << " µs across "
              << format_with_separator(stats_vec.size()) << " unique keys\n";
    std::cout << "Note: Profiler overhead of " << profiler_overhead_ns << " ns has been subtracted from all measurements\n\n";
}
