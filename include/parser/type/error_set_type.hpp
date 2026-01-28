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

    bool is_freeable() const override {
        return true;
    }

    Hash get_hash() const override {
        return error_node->file_hash;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::ERROR_SET) {
            return false;
        }
        const ErrorSetType *const other_type = other->as<ErrorSetType>();
        return error_node == other_type->error_node;
    }

    std::string to_string() const override {
        return error_node->name;
    }

    std::string get_type_string([[maybe_unused]] const bool is_return_type = false) const override {
        assert(!is_return_type);
        return "type.flint.err";
    }

    /// @var `error_node`
    /// @brief The error node this error type points to
    ErrorNode *const error_node;
};
