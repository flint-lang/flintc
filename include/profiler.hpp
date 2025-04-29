#pragma once

/**
 * @file Profiler.hpp
 * @brief Provides tools for profiling tasks with timing functionality.
 *
 * This file defines classes and utilities for measuring the execution duration of tasks.
 * The `Profiler` class offers static methods for starting, ending, and printing task profiles,
 * while the `ScopeProfiler` class provides RAII-based scope profiling.
 * The profiling system captures a hierarchical call stack for better visualization.
 */

#include "debug.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

/// @typedef `TimePoint`
/// @brief Alias for a high-resolution time point.
using TimePoint = std::chrono::high_resolution_clock::time_point;

/// @struct ProfileNode
/// @brief Represents a node in the profiling tree.
struct ProfileNode {
    std::string name;                                   ///< Name of the task.
    TimePoint start;                                    ///< Start time of the task.
    TimePoint end;                                      ///< End time of the task.
    std::vector<std::shared_ptr<ProfileNode>> children; ///< Child profiling nodes.

    /// @brief Constructor for a profile node
    /// @param name Name of the task
    explicit ProfileNode(std::string name) :
        name(std::move(name)),
        start(std::chrono::high_resolution_clock::now()),
        end{} {}
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
    static void start_task(const std::string &task, const bool track_task = false);

    /// @brief Ends timing a named task.
    /// @param task Name of the task to end.
    static void end_task(const std::string &task);

    /// @brief Prints profiling results.
    /// @param unit Time unit for output (default: milliseconds).
    static void print_results(TimeUnit unit = TimeUnit::MILLIS);

    /// @var `profiling_durations`
    /// @brief A map contiaining the profiling names and the node containing the actual durations
    static inline std::unordered_map<std::string, const ProfileNode *const> profiling_durations;

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
        if (root_nodes.empty()) {
            std::cout << "No profiling data available.\n";
            return;
        }

        for (size_t i = 0; i < root_nodes.size(); ++i) {
            // Pass last flag for proper tree drawing
            bool is_last = (i == root_nodes.size() - 1);
            print_node<Duration>(root_nodes[i], {}, is_last, unit);
        }
    }

    /// @brief Internal function to print a node and its children recursively
    /// @tparam Duration Duration type for time measurement
    /// @param node The node to print
    /// @param prefix The prefix string for the current line
    /// @param is_last Whether this node is the last child of its parent
    /// @param unit Time unit label
    template <typename Duration>
    static void print_node(const std::shared_ptr<ProfileNode> &node, const std::vector<bool> &prefix_branches, bool is_last,
        std::string_view unit) {
        if (!node) {
            return;
        }

        // Calculate duration
        auto duration = std::chrono::duration_cast<Duration>(node->end - node->start);

        // Generate the prefix string with appropriate tree characters
        std::string line_prefix;

        // Add vertical lines for all ancestors - each is a single character plus a space
        for (bool has_next_sibling : prefix_branches) {
            if (has_next_sibling) {
                line_prefix += Debug::tree_characters.at(Debug::VERT) + "  ";
            } else {
                line_prefix += "   ";
            }
        }

        // Add the branch character for this node - single character plus a space
        if (!prefix_branches.empty()) { // Not a root node
            if (is_last) {
                line_prefix += Debug::tree_characters.at(Debug::SINGLE) + Debug::tree_characters.at(Debug::HOR) + " ";
            } else {
                line_prefix += Debug::tree_characters.at(Debug::BRANCH) + Debug::tree_characters.at(Debug::HOR) + " ";
            }
        }

        // Format the duration with color (optional) and arrow to task name
        std::string formatted_duration = format_with_separator(duration.count(), '.') + " " + std::string(unit) + " " +
            Debug::tree_characters.at(Debug::HOR) + "> " + Debug::TextFormat::BOLD_START + node->name + Debug::TextFormat::BOLD_END;

        // Print node with duration
        std::cout << line_prefix << formatted_duration << "\n";

        // Print children with updated prefix
        std::vector<bool> next_prefix = prefix_branches;
        next_prefix.push_back(!is_last);

        for (size_t i = 0; i < node->children.size(); ++i) {
            bool child_is_last = (i == node->children.size() - 1);
            print_node<Duration>(node->children[i], next_prefix, child_is_last, unit);
        }
    }

    /// @brief Access the current profile node
    /// @return Pointer to the current profile node or nullptr if no profiling in progress
    static std::shared_ptr<ProfileNode> current_node() {
        return profile_stack.empty() ? nullptr : profile_stack.top();
    }

    // Stores root profiling nodes
    static std::vector<std::shared_ptr<ProfileNode>> root_nodes;

    // Profiling stack to track nested calls
    static std::stack<std::shared_ptr<ProfileNode>> profile_stack;

    // Map for quick lookup when using manual start_task/end_task
    static std::map<std::string, std::shared_ptr<ProfileNode>> active_tasks;
};

/// @class ScopeProfiler
/// @brief RAII-based class for profiling a task's execution duration.
class ScopeProfiler {
  public:
    /// @brief Constructs a `ScopeProfiler` and starts timing a task.
    /// @param task_name Name of the task to profile.
    explicit ScopeProfiler(std::string task_name) :
        task_name(std::move(task_name)) {
        node = std::make_shared<ProfileNode>(this->task_name);

        // Add to parent if we have one
        auto current = Profiler::current_node();
        if (current) {
            current->children.push_back(node);
        } else {
            // No parent, this is a root node
            Profiler::root_nodes.push_back(node);
        }

        // Push this node onto the stack
        Profiler::profile_stack.push(node);
    }

    /// @brief Destructs the `ScopeProfiler` and ends timing the task.
    ~ScopeProfiler() {
        if (!node)
            return; // Safety check

        // Set end time
        node->end = std::chrono::high_resolution_clock::now();

        // Pop from stack only if this is the top node
        if (!Profiler::profile_stack.empty() && Profiler::profile_stack.top() == node) {
            Profiler::profile_stack.pop();
        }
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
    std::string task_name;             ///< Name of the task being profiled.
    std::shared_ptr<ProfileNode> node; ///< Node representing this profile in the tree.
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
