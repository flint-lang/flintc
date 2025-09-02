#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrExternWithoutFip : public BaseError {
  public:
    ErrExternWithoutFip(            //
        const ErrorType error_type, //
        const std::string &file,    //
        const unsigned int line,    //
        const unsigned int column,  //
        const unsigned int length   //
        ) :
        BaseError(error_type, file, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Defined '" << YELLOW << "extern" << DEFAULT
            << "' function without the FIP running and active\n";
        oss << "└─ Check your configs in '" << CYAN << ".fip/config/" << DEFAULT << "' to see if there are any problems with it";
        return oss.str();
    }
};
