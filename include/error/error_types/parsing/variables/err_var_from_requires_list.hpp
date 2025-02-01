#ifndef __ERR_VAR_FROM_REQUIRES_LIST_HPP__
#define __ERR_VAR_FROM_REQUIRES_LIST_HPP__

#include "error/error_types/base_error.hpp"

class ErrVarFromRequiresList : public BaseError {
  public:
    ErrVarFromRequiresList(const ErrorType error_type, const std::string &file, int line, int column, const std::string &var_name) :
        BaseError(error_type, file, line, column),
        var_name(var_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "Variable '" + var_name + "' was defined in the 'requires' statement of the func module";
        return oss.str();
    }

  private:
    std::string var_name;
};

#endif
