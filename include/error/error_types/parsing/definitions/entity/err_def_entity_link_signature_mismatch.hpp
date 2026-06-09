#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"

class ErrDefEntityLinkSignatureMismatch : public BaseError {
  public:
    ErrDefEntityLinkSignatureMismatch( //
        const ErrorType error_type,    //
        const Hash &file_hash,         //
        const token_slice &tokens,     //
        const std::string from_sig,    //
        const std::string to_sig       //
        ) :
        BaseError(error_type, file_hash, tokens),
        from_sig(from_sig),
        to_sig(to_sig) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Linking functions with different explicit signatures is not allowed\n";
        oss << "├─ Left signature: " << YELLOW << from_sig << DEFAULT << "\n";
        oss << "└─ Right signature: " << YELLOW << to_sig << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Linking functions with different explicit signatures is not allowed";
        return d;
    }

  private:
    const std::string from_sig;
    const std::string to_sig;
};
