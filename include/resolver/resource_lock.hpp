#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/// @class `ResourceLock`
/// @brief This class enables RAII-based resource-locking, where the locked "resource" is a string. It ensures that many threads can
/// concurrently work on different resources at a time, but they are not allowed to work on the same resource at the same time
class ResourceLock {
  public:
    // The constructor locks a given resource
    explicit ResourceLock(const std::string &resource) :
        resource_name(resource) {
        std::unique_lock<std::mutex> lock(map_mutex);
        // Get or create mutex for the resource
        if (mutex_map.find(resource) == mutex_map.end()) {
            mutex_map[resource] = std::make_pair(0, std::make_shared<std::mutex>());
        }
        auto &mtx = mutex_map.at(resource);
        mtx.first++;   // Increase the reference count by 1 for every lock
        lock.unlock(); // Unlock the map while we lock the resource
        mtx.second->lock();
    }

    // The destructor unlocks the resource of this ResourceLock again, making it usable for other threads
    ~ResourceLock() {
        std::unique_lock<std::mutex> lock(map_mutex);
        auto it = mutex_map.find(resource_name);
        if (it != mutex_map.end()) {
            // Unlock the mutex
            it->second.second->unlock();
            // Decrement the counter
            it->second.first--;
            // Only erase the entry when the counter has reached 0
            if (it->second.first == 0) {
                mutex_map.erase(it);
            }
        }
    }

  private:
    /// @var `resource_name`
    /// @brief The name of the resource that is locked
    std::string resource_name;

    /// @var `mutex_map`
    /// @brief The map of all mutexes of all resources as well as a counter how many threads have locked this resource
    static inline std::unordered_map<std::string, std::pair<unsigned int, std::shared_ptr<std::mutex>>> mutex_map;

    /// @var `map_mutex`
    /// @brief The mutex of the map itself, to enable thread-safe accessed to the `mutex_map`
    static inline std::mutex map_mutex;
};
