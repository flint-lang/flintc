#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

/// @brief A thread pool implementation that manages a collection of worker threads
/// @details This class provides a simple interface for parallel task execution by maintaining
///          a pool of worker threads that process tasks from a shared queue. Tasks can be
///          submitted from any thread and their results retrieved asynchronously.
class ThreadPool {
  public:
    /// @brief Constructs a ThreadPool with the specified number of threads
    /// @param num_threads The number of worker threads to create (defaults to hardware thread count)
    explicit ThreadPool(unsigned int num_threads = std::thread::hardware_concurrency()) {
        start(num_threads);
    }

    /// @brief Destructor that ensures all threads are properly stopped and joined
    ~ThreadPool() {
        stop();
    }

    /// @function `enqueue`
    /// @brief Enqueues a task to be executed by the thread pool
    ///
    /// @tparam F The type of the function to execute
    /// @tparam Args The types of the arguments to pass to the function
    ///
    /// @param f The function to execute
    /// @param args The arguments to pass to the function
    /// @return std::future containing the result of the function execution
    ///
    /// @throws std::bad_alloc If task allocation fails
    /// @note This method is thread-safe and can be called concurrently from multiple threads
    template <typename F, typename... Args> auto enqueue(F &&f, Args &&...args) {
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.emplace([task]() { (*task)(); });
        }
        _condition.notify_one();
        return result;
    }

  private:
    std::vector<std::thread> _workers;        ///< Collection of worker threads
    std::queue<std::function<void()>> _tasks; ///< Queue of pending tasks
    std::mutex _mutex;                        ///< Mutex for thread synchronization
    std::condition_variable _condition;       ///< Condition variable for thread synchronization
    std::atomic<bool> _stop{false};           ///< Flag indicating whether the pool should stop

    /// @function `start`
    /// @brief Starts the thread pool with the specified number of threads
    /// @param num_threads The number of worker threads to create
    void start(unsigned int num_threads) {
        for (unsigned int i = 0; i < num_threads; ++i) {
            _workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _condition.wait(lock, [this] { return _stop || !_tasks.empty(); });
                        if (_stop && _tasks.empty()) {
                            return;
                        }
                        task = std::move(_tasks.front());
                        _tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    /// @function `stop`
    /// @brief Stops all worker threads and joins them
    void stop() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stop = true;
        }
        _condition.notify_all();
        for (std::thread &worker : _workers) {
            worker.join();
        }
    }
};
