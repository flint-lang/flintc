#ifndef __ERR_STMT_DANGLING_CATCH_HPP__
#define __ERR_STMT_DANGLING_CATCH_HPP__

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrStmtDanglingCatch : public BaseError {
  public:
    ErrStmtDanglingCatch(const ErrorType error_type, const std::string &file, const token_list &tokens) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Dangling 'catch' statemnt without preceeding function call: " << YELLOW
            << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
};

#endif
