#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefUnexpectedCoreModule : public BaseError {
  public:
    ErrDefUnexpectedCoreModule(const ErrorType error_type, const std::string &file, unsigned int line, unsigned int column,
        const std::string &core_module_name) :
        BaseError(error_type, file, line, column),
        core_module_name(core_module_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The Core module '" << YELLOW << core_module_name << DEFAULT
            << "' does not exist. Available Core modules are '" << YELLOW << "assert" << DEFAULT << "', '" << YELLOW << "env" << DEFAULT
            << "', '" << YELLOW << "filesystem" << DEFAULT << "', '" << YELLOW << "print" << DEFAULT << "' or '" << YELLOW << "read"
            << DEFAULT << "'";
        return oss.str();
    }

  private:
    std::string core_module_name;
};
