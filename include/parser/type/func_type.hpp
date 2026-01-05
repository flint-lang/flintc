#pragma once

#include "parser/ast/definitions/func_node.hpp"
#include "type.hpp"

/// @class `FuncType`
/// @brief Represents func types
class FuncType : public Type {
  public:
    FuncType(FuncNode *const func_node) :
        func_node(func_node) {}

    Variation get_variation() const override {
        return Variation::FUNC;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::DATA) {
            return false;
        }
        const FuncType *const other_type = other->as<FuncType>();
        return func_node == other_type->func_node;
    }

    std::string to_string() const override {
        return func_node->name;
    }

    /// @var `func_node`
    /// @brief The func node this func type points to
    FuncNode *const func_node;
};
