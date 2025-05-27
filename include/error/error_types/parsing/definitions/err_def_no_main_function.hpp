#pragma once

#include "error/error_types/base_error.hpp"

class ErrDefNoMainFunction : public BaseError {
  public:
    ErrDefNoMainFunction(const ErrorType error_type, const std::string &file) :
        BaseError(error_type, file, 1, 1) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "no main function found anywhere";
        return oss.str();
    }
};
