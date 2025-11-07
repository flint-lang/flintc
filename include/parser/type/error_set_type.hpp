#pragma once

#include "parser/ast/definitions/error_node.hpp"
#include "type.hpp"

/// @class `ErrorSetType`
/// @brief Represents error set types
class ErrorSetType : public Type {
  public:
    ErrorSetType(ErrorNode *const error_node) :
        error_node(error_node) {}

    Variation get_variation() const override {
        return Variation::ERROR_SET;
    }

    std::string to_string() const override {
        return error_node->name;
    }

    /// @var `error_node`
    /// @brief The error node this error type points to
    ErrorNode *const error_node;
};
