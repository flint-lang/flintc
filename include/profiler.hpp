#pragma once

/**
 * @file Profiler.hpp
 * @brief Provides tools for profiling tasks with timing functionality.
 *
 * This file defines classes and utilities for measuring the execution duration of tasks.
 * The `Profiler` class offers static methods for starting, ending, and printing task profiles,
 * while the `ScopeProfiler` class provides RAII-based scope profiling.
 */

#include <chrono>
#include <iomanip> // For formatting
#include <iostream>
#include <map>
#include <string>

/// @typedef `TimePoint`
/// @brief Alias for a high-resolution time point.
using TimePoint = std::chrono::high_resolution_clock::time_point;

/// @struct ProfileData
/// @brief Stores timing information for a task.
struct ProfileData {
    TimePoint start; ///< Start time of the task.
    TimePoint end;   ///< End time of the task.
};

class ScopeProfiler; ///< Forward declaration.

/// @class Profiler
/// @brief Provides static methods for task profiling.
class Profiler {
  public:
    /// @enum TimeUnit
    /// @brief Units of time used for profiling output.
    enum class TimeUnit {
        NS,     ///< Nanoseconds
        MICS,   ///< Microseconds
        MILLIS, ///< Milliseconds
        SEC     ///< Seconds
    };

    /// @brief Deleted default constructor to prevent instantiation.
    Profiler() = delete;

    /// @brief Starts a scope-based profiler for a task.
    /// @param task_name Name of the task to profile.
    /// @return A `ScopeProfiler` instance managing the task.
    static ScopeProfiler start_scope(const std::string &task_name);

    /// @brief Starts timing a named task.
    /// @param task Name of the task to start.
    static void start_task(const std::string &task);

    /// @brief Ends timing a named task.
    /// @param task Name of the task to end.
    static void end_task(const std::string &task);

    /// @brief Prints profiling results.
    /// @param unit Time unit for output (default: milliseconds).
    static void print_results(TimeUnit unit = TimeUnit::MILLIS);

    /// @brief Formats a numeric value with a separator for readability.
    /// @tparam T Type of the value (e.g., integer, floating-point).
    /// @param value The numeric value to format.
    /// @param separator Character to use as a separator.
    /// @return A formatted string.
    template <typename T> static std::string format_with_separator(T value, char separator = '_') {
        std::string str = std::to_string(value);
        int pos = str.length() - 3;
        while (pos > 0) {
            str.insert(pos, 1, separator);
            pos -= 3;
        }
        return str;
    }

    /// @brief Prints profiling results with a custom duration type.
    /// @tparam Duration Type of the duration (default: milliseconds).
    /// @param unit Label for the time unit.
    template <typename Duration = std::chrono::milliseconds> static void print_results(std::string_view unit = "ms") {
        std::cout << std::setw(30) << std::left << "Task Name" << std::setw(15) << ("Duration (" + std::string(unit) + ")") << "\n";
        std::cout << std::string(45, '-') << "\n";

        for (const auto &[task, data] : profiles) {
            if (data.end == TimePoint{}) {
                std::cerr << "Warning: Task \"" << task << "\" was not ended properly.\n";
                continue;
            }
            auto duration = std::chrono::duration_cast<Duration>(data.end - data.start);
            std::cout << std::setw(30) << std::left << task << std::setw(15) << format_with_separator(duration.count(), '.') << "\n";
        }
    }

    /// @brief Stores profiling data for all tasks.
    static std::map<std::string, ProfileData> profiles;
};

/// @class ScopeProfiler
/// @brief RAII-based class for profiling a task's execution duration.
class ScopeProfiler {
  public:
    /// @brief Constructs a `ScopeProfiler` and starts timing a task.
    /// @param task_name Name of the task to profile.
    explicit ScopeProfiler(std::string task_name) :
        task_name(std::move(task_name)) {
        profile_data = ProfileData{std::chrono::high_resolution_clock::now(), {}};
    }

    /// @brief Destructs the `ScopeProfiler` and ends timing the task.
    ~ScopeProfiler() {
        profile_data.end = std::chrono::high_resolution_clock::now();
        if (Profiler::profiles.find(task_name) != Profiler::profiles.end()) {
            std::cerr << "Error: Task \"" << task_name << "\" was already started.\n";
        }
        Profiler::profiles[task_name] = profile_data;
    }

    /// @brief Default copy constructor.
    ScopeProfiler(const ScopeProfiler &) = default;

    /// @brief Default copy assignment operator.
    ScopeProfiler &operator=(const ScopeProfiler &) = default;

    /// @brief Default move constructor.
    ScopeProfiler(ScopeProfiler &&) = default;

    /// @brief Default move assignment operator.
    ScopeProfiler &operator=(ScopeProfiler &&) = delete;

  protected:
    std::string task_name;    ///< Name of the task being profiled.
    ProfileData profile_data; ///< Profiling data for the task.
};

/// @def PROFILE_SCOPE(name)
/// @brief Macro for creating a `ScopeProfiler` instance with a unique name.
/// @param name Name of the task to profile.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#ifdef DEBUG_BUILD
#define PROFILE_SCOPE(name) ScopeProfiler CONCAT(sp_, __LINE__)(name)
#else
#define PROFILE_SCOPE(name) ((void)0)
#endif

/// @def CONCAT(a, b)
/// @brief Macro for concatenating two tokens.
/// @param a First token.
/// @param b Second token.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CONCAT(a, b) CONCAT_INNER(a, b)

/// @def CONCAT_INNER(a, b)
/// @brief Inner macro for token concatenation.
/// @param a First token.
/// @param b Second token.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CONCAT_INNER(a, b) a##b
