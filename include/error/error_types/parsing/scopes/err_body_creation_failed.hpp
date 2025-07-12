#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrBodyCreationFailed : public BaseError {
  public:
    ErrBodyCreationFailed(const ErrorType error_type, const std::string &file, const std::vector<Line> &body) :
        BaseError(error_type, file, body.front().tokens.first->line, body.front().tokens.first->column),
        body(body) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Failed to parse body:\n" << YELLOW;
        // for (auto it = body.begin(); it != body.end(); ++it) {
        //     //
        // }
        oss << "TODO" << DEFAULT;
        return oss.str();
    }

  private:
    std::vector<Line> body;
};
