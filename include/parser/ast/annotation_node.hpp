#pragma once

#include "ast_node.hpp"
#include <unordered_set>

/// @enum `AnnotationKind`
/// @brief An enum of all possible annotation kinds there are
enum class AnnotationKind {
    TEST_SHOULD_FAIL, // #test_should_fail usable on `TestNode`s
    TEST_PERFORMANCE, // #test_performance usable on `TestNode`s
};

/// @var `annotation_map`
/// @brief A map mapping strings to the correnct annotaiton kind enum
static const inline std::unordered_map<std::string_view, AnnotationKind> annotation_map = {
    {"test_should_fail", AnnotationKind::TEST_SHOULD_FAIL},
    {"test_performance", AnnotationKind::TEST_PERFORMANCE},
};

/// @var `annotation_map_rev`
/// @brief The reverse of the `annotation_map`, mapping the annotation kind enums to the strings views
static const inline std::unordered_map<AnnotationKind, std::string_view> annotation_map_rev = {
    {AnnotationKind::TEST_SHOULD_FAIL, "test_should_fail"},
    {AnnotationKind::TEST_PERFORMANCE, "test_performance"},
};

/// @class `AnnotationNode`
/// @brief The ast node representing any annotations
class AnnotationNode : public ASTNode {
  public:
    AnnotationNode() = default;

    explicit AnnotationNode(const AnnotationKind kind, const std::vector<std::string> &arguments) :
        kind(kind),
        arguments(arguments) {}

    /// @function `extract_consumable`
    /// @brief Extracts all consumable annotations from the given list of annotations
    ///
    /// @param `annotations` The list to extract all consumable annotations from
    /// @param `consumable_annotations` The set containing all consumable annotations
    /// @return `std::vector<AnnotationNode>` The new list containing all consumable annotations from the `annotations` list.
    ///
    /// @note This removes all consumable annotations from the `annotations` list, thus modifying it
    static std::vector<AnnotationNode> extract_consumable(               //
        std::vector<AnnotationNode> &annotations,                        //
        const std::unordered_set<AnnotationKind> &consumable_annotations //
    ) {
        std::vector<AnnotationNode> extracted;
        for (auto it = annotations.begin(); it != annotations.end();) {
            if (consumable_annotations.find(it->kind) != consumable_annotations.end()) {
                extracted.emplace_back(std::move(*it));
                annotations.erase(it);
                continue;
            }
            ++it;
        }
        return extracted;
    }

    /// @var `kind`
    /// @brief The kind of the annotation
    AnnotationKind kind;

    /// @var `arguments`
    /// @brief Potential arguments of the annotation, could be empty
    std::vector<std::string> arguments;
};
