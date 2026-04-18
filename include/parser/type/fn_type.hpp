#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

#include <sstream>
#include <vector>

/// @class `FnType`
/// @brief Represents fn types
class FnType : public Type {
  public:
    FnType(const std::vector<std::shared_ptr<Type>> &param_types, const std::vector<std::shared_ptr<Type>> &return_types) :
        param_types(param_types),
        return_types(return_types) {}

    Variation get_variation() const override {
        return Variation::FN;
    }

    bool is_freeable() const override {
        // fn variable frames are allocated on the heap so they are always freeable, like strings
        return true;
    }

    Hash get_hash() const override {
        return Hash(std::string(""));
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::FN) {
            return false;
        }
        const FnType *const other_type = other->as<FnType>();
        return param_types == other_type->param_types && return_types == other_type->return_types;
    }

    std::string to_string() const override {
        std::stringstream ss;
        ss << "fn<";
        if (param_types.empty()) {
            ss << "()";
        } else {
            for (size_t i = 0; i < param_types.size(); i++) {
                if (i > 0) {
                    ss << ", ";
                }
                ss << param_types.at(i)->to_string();
            }
        }

        ss << " -> ";
        if (return_types.empty()) {
            ss << "void";
        } else {
            for (size_t i = 0; i < return_types.size(); i++) {
                if (i > 0) {
                    ss << ", ";
                }
                ss << return_types.at(i)->to_string();
            }
        }

        ss << ">";
        return ss.str();
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? ".type.ret.fn." : ".type.fn.";
        return type_str + to_string();
    }

    /// @var `param_types`
    /// @brief The parameter types of the referenced function
    std::vector<std::shared_ptr<Type>> param_types;

    /// @var `return_types`
    /// @brief The return types of the referenced function
    std::vector<std::shared_ptr<Type>> return_types;

    // TODO: Eventually add error sets to fn variables too?
};
