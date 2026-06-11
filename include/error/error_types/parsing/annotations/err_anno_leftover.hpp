#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/annotation_node.hpp"
#include "parser/type/type.hpp"

class ErrAnnoLeftover : public BaseError {
  public:
    ErrAnnoLeftover(                                       //
        const ErrorType error_type,                        //
        const Hash &file_hash,                             //
        const unsigned int line,                           //
        const unsigned int column,                         //
        const unsigned int length,                         //
        const std::vector<AnnotationNode> annotation_queue //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        annotation_queue(annotation_queue) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string();
        if (annotation_queue.size() == 1) {
            oss << "└─ The annotation '" << YELLOW << annotation_map_rev.at(annotation_queue.front().kind) << DEFAULT
                << "' not usable for this definition";
            return oss.str();
        }
        oss << "└─ These annotations not usable for this definition:\n";
        for (size_t i = 0; i < annotation_queue.size(); i++) {
            const bool is_last = i + 1 == annotation_queue.size();
            if (is_last) {
                oss << "    └─ ";
            } else {
                oss << "    ├─ ";
            }
            oss << YELLOW << "#" << annotation_map_rev.at(annotation_queue.at(i).kind) << DEFAULT;
            if (!is_last) {
                oss << "\n";
            }
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message =
            "Annotation '#" + std::string(annotation_map_rev.at(annotation_queue.front().kind)) + "' not usable for this definition";
        return d;
    }

  private:
    std::vector<AnnotationNode> annotation_queue;
};
