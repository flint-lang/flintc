#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

inline void assert_failed(const char *file, int line, const char *expr) {
    std::fprintf(stderr, "ASSERTION FAILED [%s:%d]: %s\n", file, line, expr);
    std::abort();
}

inline void assert_failed(const char *file, int line, const char *expr, std::string_view msg) {
    std::fprintf(stderr, "ASSERTION FAILED [%s:%d]: %s: %.*s\n", file, line, expr, static_cast<int>(msg.size()), msg.data());
    std::abort();
}

#define ASSERT(expr, ...)                                                                                                                  \
    do {                                                                                                                                   \
        if (!(expr)) {                                                                                                                     \
            assert_failed(__FILE__, __LINE__, #expr __VA_OPT__(, __VA_ARGS__));                                                            \
        }                                                                                                                                  \
    } while (0)

#define UNREACHABLE()                                                                                                                      \
    do {                                                                                                                                   \
        std::fprintf(stderr, "UNREACHABLE REACHED [%s:%d]\n", __FILE__, __LINE__);                                                         \
        std::abort();                                                                                                                      \
    } while (0)
