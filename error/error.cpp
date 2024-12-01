#include "error.hpp"
#include "error_type.hpp"

#include <stdexcept>

//#include <string>

void throw_err(ErrorType error_type) {
    throw std::runtime_error("Custom Error: " + std::to_string(static_cast<int>(error_type)));
}
