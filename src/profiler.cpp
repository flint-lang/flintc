#include "profiler.hpp"

#include "colors.hpp"
#include "globals.hpp"

std::vector<std::shared_ptr<ProfileNode>> Profiler::root_nodes;
std::stack<std::shared_ptr<ProfileNode>> Profiler::profile_stack;
std::map<std::string, std::shared_ptr<ProfileNode>> Profiler::active_tasks;

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
