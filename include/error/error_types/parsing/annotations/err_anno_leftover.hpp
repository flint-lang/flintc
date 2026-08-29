#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/annotation_node.hpp"
#include "parser/type/type.hpp"

class ErrAnnoLeftover : public BaseError {
  public:
    ErrAnnoLeftover(                                                    //
        const ErrorType error_type,                                     //
        const Hash &file_hash,                                          //
        const unsigned int line,                                        //
        const unsigned int column,                                      //
        const unsigned int length,                                      //
        const std::vector<AnnotationNode> annotation_queue,             //
        const std::unordered_set<AnnotationKind> consumable_annotations //
        ) :
        BaseError(error_type, file_hash, line, column, length),
        annotation_queue(annotation_queue),
        consumable_annotations(consumable_annotations) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string();
        ASSERT(annotation_queue.size() >= 1);
        oss << "├─ Annotation '" << YELLOW << "#" << annotation_map_rev.at(annotation_queue.front().kind) << DEFAULT
            << "' not usable for this definition\n";
        if (consumable_annotations.empty()) {
            oss << "└─ There are no annotations available for this definition";
            return oss.str();
        }
        oss << "└─ These are the possible annotations usable for this definition:\n";
        size_t i = 0;
        for (const auto &anno : consumable_annotations) {
            const bool is_last = ++i == consumable_annotations.size();
            if (is_last) {
                oss << "    └─ ";
            } else {
                oss << "    ├─ ";
            }
            oss << YELLOW << "#" << annotation_map_rev.at(anno) << DEFAULT;
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
    std::unordered_set<AnnotationKind> consumable_annotations;
};
