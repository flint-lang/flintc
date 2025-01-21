#ifndef __ERROR_HPP__
#define __ERROR_HPP__

#include "error_type.hpp"

#include <iostream>
#include <string>

inline void throw_err(ErrorType error_type) {
    throw std::runtime_error("Custom Error: " + std::to_string(static_cast<int>(error_type)));
}

template <typename ErrorType, typename... Args> //
void throw_err(const std::string &file, int line, int column, Args &&...args) {
    ErrorType error(std::forward<Args>(args)..., file, line, column);
    std::cerr << error.to_string() << std::endl;
    std::exit(EXIT_FAILURE);
}

#endif
