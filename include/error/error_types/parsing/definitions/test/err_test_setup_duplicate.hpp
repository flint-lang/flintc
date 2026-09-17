#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrTestSetupDuplicate : public BaseError {
  public:
    ErrTestSetupDuplicate(                //
        const ErrorType error_type,       //
        const Hash &file_hash,            //
        const unsigned int line,          //
        const unsigned int column,        //
        const std::string &annotation     //
        ) :
        BaseError(error_type, file_hash, line, column, annotation.size() + 1),
        annotation(annotation) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The '" << YELLOW << annotation << DEFAULT << "' test is already defined in this file\n";
        oss << "└─ Only one '#test_init', '#test_pre', '#test_post' and '#test_deinit' test is allowed per file";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "The '" + annotation + "' test is already defined in this file";
        return d;
    }

  private:
    std::string annotation;
};