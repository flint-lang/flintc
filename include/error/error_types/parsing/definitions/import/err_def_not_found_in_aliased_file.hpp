#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefNotFoundInAliasedFile : public BaseError {
  public:
    ErrDefNotFoundInAliasedFile(              //
        const ErrorType error_type,           //
        const Hash &file_hash,                //
        const unsigned int line,              //
        const unsigned int column,            //
        const std::string &alias,             //
        const std::string &aliased_file_name, //
        const std::string &definition         //
        ) :
        BaseError(error_type, file_hash, line, column),
        alias(alias),
        aliased_file_name(aliased_file_name),
        definition(definition) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "The definition '" << YELLOW << definition << DEFAULT
            << "' could not be found from the aliased file \"" << YELLOW << aliased_file_name << "\" as " << alias << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Unknown definition '" + definition + "' in aliased file '" + aliased_file_name + "'";
        return d;
    }

  private:
    std::string alias;
    std::string aliased_file_name;
    std::string definition;
};
