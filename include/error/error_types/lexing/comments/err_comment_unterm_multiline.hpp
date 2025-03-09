#pragma once

#include "error/error_types/base_error.hpp"

class ErrCommentUnterminatedMultiline : public BaseError {
  public:
    ErrCommentUnterminatedMultiline(const ErrorType error_type, const std::string &file, int line, int column) :
        BaseError(error_type, file, line, column) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Multiline comment not terminated!";
        return oss.str();
    }
};
