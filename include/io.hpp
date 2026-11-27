#pragma once

#include "profiler.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <thread>

/// @class `IO`
/// @brief This class contains IO-related functions. It contains only static functions and is not instantiable
class IO {
  public:
    IO() = delete;

    /// @function `remove_with_retry`
    /// @brief Tries to remove a given file 5 times with a 10ms delay between tries. This is to prevent crashes on windows where the
    ///        compiler tries to delete the `main.obj` file after compilation and it's a bit too fast for Windows, as it still holds a lock
    ///        on the file for some reason
    ///
    /// @param `path` The path to the file to remove with retries
    static void remove_with_retry(const std::filesystem::path &path) {
        PROFILE_CUMULATIVE("IO::remove_with_retry");
        for (int i = 0; i < 5; i++) {
            try {
                std::error_code ec;
                if (std::filesystem::remove(path, ec)) {
                    return;
                }
            } catch (...) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    /// @function `file_exists_and_is_readable`
    /// @brief Checks if the given file does exist and is readable
    ///
    /// @param `file_path` The file path to check
    /// @return `bool` Whether the file exists and is readable
    [[nodiscard]] static bool file_exists_and_is_readable(const std::filesystem::path &file_path) {
        PROFILE_CUMULATIVE("IO::file_exists_and_is_readable");
        std::ifstream file(file_path.string());
        return file.is_open() && !file.fail();
    }

    /// @function `load_file`
    /// @brief Loads a given file from a file path and returns the files content
    ///
    /// @param `path` The path to the file
    /// @return `std::string` The loaded file
    [[nodiscard]] static std::string load_file(const std::filesystem::path &path) {
        PROFILE_CUMULATIVE("IO::load_file");
        std::ifstream file(path.string());
        if (!file) {
            throw std::runtime_error("Failed to load file " + path.string());
        }
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }

    /// @function `is_parent_of`
    /// @brief Checks if a given `parent` path is the parent of another `path`
    ///
    /// @param `parent` The potential parent path
    /// @param `path` The path to check
    /// @return `bool` Whether `path` is equal to or contained within `parent`
    [[nodiscard]] static bool is_parent_of(const std::filesystem::path &parent, const std::filesystem::path &path) {
        PROFILE_CUMULATIVE("IO::is_parent_of");
        if (parent.empty() || path.empty()) {
            return false;
        }
        std::error_code ec;
        const std::filesystem::path c_parent = std::filesystem::weakly_canonical(parent, ec);
        if (ec) {
            return false;
        }
        const std::filesystem::path c_path = std::filesystem::weakly_canonical(path, ec);
        if (ec) {
            return false;
        }

        auto parent_iter = c_parent.begin();
        auto path_iter = c_path.begin();
        for (; parent_iter != c_parent.end(); ++parent_iter, ++path_iter) {
            if (path_iter == c_path.end() || *parent_iter != *path_iter) {
                return false;
            }
        }
        return true;
    }
};
