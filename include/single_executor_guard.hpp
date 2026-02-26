#include <atomic>
#include <cassert>
#include <thread>

class SingleExecutorGuard {
  private:
    std::atomic<std::thread::id> &owner_;
    std::atomic<int> &counter_;

  public:
    SingleExecutorGuard(std::atomic<std::thread::id> &owner, std::atomic<int> &counter) :
        owner_(owner),
        counter_(counter) {
        std::thread::id this_id = std::this_thread::get_id();
        // Increment counter atomically
        int old_count = counter_.fetch_add(1, std::memory_order_acquire);
        if (old_count == 0) {
            // First entry: set owner
            owner_.store(this_id, std::memory_order_release);
        } else {
            // Check if same thread (allow recursion)
            if (owner_.load(std::memory_order_acquire) != this_id) {
                assert(false && "Concurrent access from different threads detected!");
            }
        }
    }

    ~SingleExecutorGuard() {
        // Decrement counter
        int new_count = counter_.fetch_sub(1, std::memory_order_release);
        if (new_count == 1) {
            // Last exit: unset owner
            owner_.store(std::thread::id(), std::memory_order_release);
        }
    }
};

// Macro for convenience (per-use unique names via __LINE__)
#ifdef DEBUG_MODE
#define ASSERT_ST                                                                                                                          \
    static std::atomic<std::thread::id> owner_##__LINE__{std::thread::id()};                                                               \
    static std::atomic<int> counter_##__LINE__{0};                                                                                         \
    SingleExecutorGuard guard_##__LINE__(owner_##__LINE__, counter_##__LINE__);
#else
#define ASSERT_ST // No-op in release
#endif