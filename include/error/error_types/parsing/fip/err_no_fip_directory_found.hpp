#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "globals.hpp"

class ErrNoFipDirectoryFound : public BaseError {
  public:
    ErrNoFipDirectoryFound(         //
        const ErrorType error_type, //
        const Hash &file_hash,      //
        const unsigned int line,    //
        const unsigned int column,  //
        const unsigned int length   //
        ) :
        BaseError(error_type, file_hash, line, column, length) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ No '.fip' directory found\n";
        oss << "├─ To be able to interop with extern code you need the FIP set up\n";
        oss << "└─ For further information look at '" << CYAN << "https://flint-lang.github.io/v" << MAJOR << "." << MINOR << "." << PATCH
            << "-" << VERSION << "/beginners_guide/11_interop/2_defining.html" << DEFAULT << "'";
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "No '.fip' directory found in project";
        return d;
    }
};
