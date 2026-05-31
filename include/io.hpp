#pragma once

#include <filesystem>
#include <chrono>
#include <thread>

static void remove_with_retry(const std::filesystem::path &path) {
    for (int i = 0; i < 5; i++) {
        try {
            std::error_code ec;
            if (std::filesystem::remove(path, ec))
                return;
        } catch (...) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}