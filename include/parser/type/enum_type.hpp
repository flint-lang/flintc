#pragma once

#include "parser/ast/definitions/enum_node.hpp"
#include "type.hpp"

/// @class `EnumType`
/// @brief Represents enum types
class EnumType : public Type {
  public:
    EnumType(EnumNode *const enum_node) :
        enum_node(enum_node) {}

    std::string to_string() override {
        return enum_node->name;
    }

    /// @var `enum_node`
    /// @brief The enum node this enum type points to
    EnumNode *const enum_node;
};
