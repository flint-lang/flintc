#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/annotation_node.hpp"
#include "parser/type/type.hpp"

class ErrAnnoUnknown : public BaseError {
  public:
    ErrAnnoUnknown(                   //
        const ErrorType error_type,   //
        const Hash &file_hash,        //
        const token_slice &tokens,    //
        const std::string &annotation //
        ) :
        BaseError(error_type, file_hash, tokens),
        annotation(annotation) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Unknown annotation '" << YELLOW << annotation << DEFAULT << "'\n";
        oss << "└─ Known annotations are:\n";
        size_t i = 0;
        for (const auto &[anno, kind] : annotation_map) {
            const bool is_last = i + 1 == annotation_map.size();
            if (is_last) {
                oss << "    └─ ";
            } else {
                oss << "    ├─ ";
            }
            oss << "#" << anno;
            if (!is_last) {
                oss << "\n";
            }
            i++;
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Unknown annotation '" + annotation + "'";
        return d;
    }

  private:
    std::string annotation;
};
