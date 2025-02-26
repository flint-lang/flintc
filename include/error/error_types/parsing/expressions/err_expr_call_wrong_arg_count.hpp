#ifndef __ERR_EXPR_CALL_WRONG_ARG_COUNT_HPP__
#define __ERR_EXPR_CALL_WRONG_ARG_COUNT_HPP__

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "types.hpp"

class ErrExprCallWrongArgCount : public BaseError {
  public:
    ErrExprCallWrongArgCount(               //
        const ErrorType error_type,         //
        const std::string &file,            //
        const token_list &tokens,           //
        const std::string &function_name,   //
        const unsigned int parameter_count, //
        const unsigned int arg_count        //
        ) :
        BaseError(error_type, file, tokens.at(0).line, tokens.at(0).column),
        tokens(tokens),
        function_name(function_name),
        parameter_count(parameter_count),
        arg_count(arg_count) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Function '" << YELLOW << function_name << DEFAULT << "'expected " << YELLOW
            << std::to_string(parameter_count) << DEFAULT << " parameters but only " << YELLOW << std::to_string(arg_count) << DEFAULT
            << " arguments were provided: " << YELLOW << get_token_string(tokens, {}) << DEFAULT;
        return oss.str();
    }

  private:
    token_list tokens;
    std::string function_name;
    unsigned int parameter_count;
    unsigned int arg_count;
};

#endif
