#pragma once

#include "lexer/token_context.hpp"

#include <cassert>
#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

using token_list = std::vector<TokenContext>;
using token_slice = std::pair<token_list::iterator, token_list::iterator>;
using uint2 = std::pair<unsigned int, unsigned int>;

struct Line {
  public:
    unsigned int indent_lvl;
    token_slice tokens;
    using CallbackHandle = std::size_t;

    Line(unsigned int indent, token_slice tokens) :
        indent_lvl(indent),
        tokens(tokens) {
        // Acquire a read lock since we're only registering a callback
        std::shared_lock read_lock(rw_mutex);
        // Acquire exclusive access to the callbacks map
        std::lock_guard<std::mutex> lock(callback_mutex);
        // Register a callback for this instance
        callback_handle = register_callback_unsafe([this](token_list &source, token_list::iterator start, unsigned int len) { //
            this->update_tokens(source, start, len);                                                                          //
        });
    }

    ~Line() {
        // Acquire a read lock since we're only unregistering a callback
        std::shared_lock read_lock(rw_mutex);
        // Acquire exclusive access to the callbacks map
        std::lock_guard<std::mutex> lock(callback_mutex);
        unregister_callback_unsafe(callback_handle);
    }

    // Copy constructor
    Line(const Line &other) :
        indent_lvl(other.indent_lvl),
        tokens(other.tokens) {
        // Acquire a read lock for the copy operation
        std::shared_lock read_lock(rw_mutex);
        // Acquire exclusive access to the callbacks map
        std::lock_guard<std::mutex> lock(callback_mutex);
        // Register a new callback for this instance
        callback_handle = register_callback_unsafe([this](token_list &source, token_list::iterator start, unsigned int len) { //
            this->update_tokens(source, start, len);
        });
    }

    // Move constructor
    Line(Line &&other) noexcept :
        indent_lvl(other.indent_lvl),
        tokens(other.tokens),
        callback_handle(other.callback_handle) {
        // Acquire a read lock for the move operation
        std::shared_lock read_lock(rw_mutex);
        // Acquire exclusive access to the callbacks map
        std::lock_guard<std::mutex> lock(callback_mutex);
        // Update the callback to point to the new location
        callbacks[callback_handle] = [this](token_list &source, token_list::iterator start, unsigned int len) {
            this->update_tokens(source, start, len);
        };
        // Prevent other from unregistering our callback
        other.callback_handle = invalid_handle;
    }

    // Copy assignment
    Line &operator=(const Line &other) {
        if (this != &other) {
            // Acquire a read lock for the copy operation
            std::shared_lock read_lock(rw_mutex);
            // Acquire exclusive access to the callbacks map
            std::lock_guard<std::mutex> lock(callback_mutex);

            unregister_callback_unsafe(callback_handle);
            indent_lvl = other.indent_lvl;
            tokens = other.tokens;
            callback_handle = register_callback_unsafe([this](token_list &source, token_list::iterator start, unsigned int len) { //
                this->update_tokens(source, start, len);
            });
        }
        return *this;
    }

    // Move assignment
    Line &operator=(Line &&other) noexcept {
        if (this != &other) {
            // Acquire a read lock for the move operation
            std::shared_lock read_lock(rw_mutex);
            // Acquire exclusive access to the callbacks map
            std::lock_guard<std::mutex> lock(callback_mutex);

            unregister_callback_unsafe(callback_handle);
            indent_lvl = other.indent_lvl;
            tokens = other.tokens;
            callback_handle = other.callback_handle;
            callbacks[callback_handle] = [this](token_list &source, token_list::iterator start, unsigned int len) {
                this->update_tokens(source, start, len);
            };
            other.callback_handle = invalid_handle;
        }
        return *this;
    }

    // Static function to delete tokens and update all Lines
    static void delete_tokens(token_list &list, token_list::iterator start_point, unsigned int length) {
        if (length == 0)
            return;

        // Acquire exclusive write lock - blocks all move/copy operations
        std::unique_lock write_lock(rw_mutex);

        // Create a safe copy of the callbacks while holding the mutex
        std::map<CallbackHandle, std::function<void(token_list &, token_list::iterator, unsigned int)>> callbacks_copy;
        {
            std::lock_guard<std::mutex> lock(callback_mutex);
            callbacks_copy = callbacks;
        }

        // First, notify all Lines about the upcoming deletion
        for (const auto &[handle, callback] : callbacks_copy) {
            if (callback) {
                callback(list, start_point, length);
            }
        }

        // Then perform the actual deletion
        // Make sure we don't delete beyond the end of the list
        unsigned int actual_length = std::min(length, static_cast<unsigned int>(std::distance(start_point, list.end())));
        if (actual_length > 0) {
            list.erase(start_point, start_point + actual_length);
        }
    }

  private:
    static inline CallbackHandle next_handle = 0;
    static inline std::map<CallbackHandle, std::function<void(token_list &, token_list::iterator, unsigned int)>> callbacks;
    CallbackHandle callback_handle;
    static constexpr CallbackHandle invalid_handle = static_cast<CallbackHandle>(-1);

    // Mutex to protect the callbacks map
    static inline std::mutex callback_mutex;
    // Reader-writer mutex for synchronizing copy/move operations with token deletion
    static inline std::shared_mutex rw_mutex;

    // Unsafe versions of register/unregister (caller must hold the appropriate locks)
    static CallbackHandle register_callback_unsafe(std::function<void(token_list &, token_list::iterator, unsigned int)> callback) {
        CallbackHandle handle = next_handle++;
        callbacks[handle] = std::move(callback);
        return handle;
    }

    static void unregister_callback_unsafe(CallbackHandle handle) {
        callbacks.erase(handle);
    }

    void update_tokens(token_list &source, token_list::iterator start_point, unsigned int length) {
        auto &begin_it = tokens.first;
        auto &end_it = tokens.second;
        // Check if this line is even inside the updated source. We need to check for this because there are multiple token_list sources,
        // one for every file, and deletions in one file should not affect ranges from another file.
        if (begin_it < source.begin() || end_it > source.end()) {
            return;
        }

        // If deletion affects our range
        if (start_point < end_it) {
            if (start_point < begin_it) {
                // Deletion starts before our range, adjust beginning
                assert(start_point + length < end_it);
                unsigned int adjust_begin = std::min(length, static_cast<unsigned int>(std::distance(start_point, begin_it)));
                begin_it = begin_it - adjust_begin;
            } else {
                // Deletion overlaps our range, adjust end
                unsigned int adjust_end = std::min(length, static_cast<unsigned int>(std::distance(start_point, end_it)));
                end_it = end_it - adjust_end;
            }
        }
    }
};
