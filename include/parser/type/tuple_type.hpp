#pragma once

#include "parser/type/type.hpp"

#include <vector>

/// @class `TupleType`
/// @brief Represents tuple types
class TupleType : public Type {
  public:
    TupleType(const std::vector<std::shared_ptr<Type>> &types) :
        types(types) {}

    Variation get_variation() const override {
        return Variation::TUPLE;
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::TUPLE) {
            return false;
        }
        const TupleType *const other_type = other->as<TupleType>();
        if (types.size() != other_type->types.size()) {
            return false;
        }
        for (size_t i = 0; i < types.size(); i++) {
            if (!types.at(i)->equals(other_type->types.at(i))) {
                return false;
            }
        }
        return true;
    }

    std::string to_string() const override {
        std::string type_str = "data<";
        for (size_t i = 0; i < types.size(); i++) {
            if (i != 0) {
                type_str += ", ";
            }
            type_str += types[i]->to_string();
        }
        type_str += ">";
        return type_str;
    }

    std::string get_type_string([[maybe_unused]] const bool is_return_type = false) const override {
        // Tuples cannot be returned from functions directly
        assert(!is_return_type);
        std::string type_str = "type.tuple.";
        for (size_t i = 0; i < types.size(); i++) {
            if (i > 0) {
                type_str += "_";
            }
            type_str += types.at(i)->to_string();
        }
        return type_str;
    }

    /// @var `types`
    /// @brief The types of this group
    std::vector<std::shared_ptr<Type>> types;
};
