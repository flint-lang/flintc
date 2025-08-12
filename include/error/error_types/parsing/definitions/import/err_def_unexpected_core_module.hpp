#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "lexer/builtins.hpp"

class ErrDefUnexpectedCoreModule : public BaseError {
  public:
    ErrDefUnexpectedCoreModule(             //
        const ErrorType error_type,         //
        const std::string &file,            //
        unsigned int line,                  //
        unsigned int column,                //
        const std::string &core_module_name //
        ) :
        BaseError(error_type, file, line, column, core_module_name.size()),
        core_module_name(core_module_name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ The Core module '" << YELLOW << core_module_name << DEFAULT << "' does not exist\n";
        oss << "└─ Available Core modules are\n";
        for (auto it = core_module_functions.begin(); it != core_module_functions.end(); ++it) {
            if (std::next(it) == core_module_functions.end()) {
                oss << "    └─ " << CYAN << it->first << DEFAULT;
            } else {
                oss << "    ├─ " << CYAN << it->first << DEFAULT << "\n";
            }
        }
        return oss.str();
    }

  private:
    std::string core_module_name;
};
