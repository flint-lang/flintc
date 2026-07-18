#pragma once

#include "parser/ast/definitions/interface_node.hpp"
#include "type.hpp"

/// @class `InterfaceType`
/// @brief Represents interface types
class InterfaceType : public Type {
  public:
    InterfaceType(InterfaceNode *const interface_node) :
        interface_node(interface_node) {}

    Variation get_variation() const override {
        return Variation::INTERFACE;
    }

    bool is_freeable() const override {
        return true;
    }

    bool is_dima_managed() const override {
        return false;
    }

    Hash get_hash() const override {
        return interface_node->file_hash;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::INTERFACE) {
            return false;
        }
        const InterfaceType *const other_type = other->as<InterfaceType>();
        return interface_node == other_type->interface_node;
    }

    std::string to_string() const override {
        return interface_node->name;
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        return is_return_type ? "type.ret.interface" : "type.interface";
    }

    /// @var `interface_node`
    /// @brief The interface node this interface type points to
    InterfaceNode *const interface_node;
};
