#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

#include <sstream>
#include <vector>

/// @class `FnType`
/// @brief Represents fn types
class FnType : public Type {
  public:
    FnType(                                                                //
        const std::vector<std::pair<std::shared_ptr<Type>, bool>> &params, //
        const std::vector<std::shared_ptr<Type>> &return_types,            //
        const std::vector<std::shared_ptr<Type>> &error_types              //
        ) :
        params(params),
        return_types(return_types),
        error_types(error_types) {}

    Variation get_variation() const override {
        return Variation::FN;
    }

    bool is_freeable() const override {
        // fn variable frames are allocated on the heap so they are always freeable, like strings
        return true;
    }

    bool is_dima_managed() const override {
        return false;
    }

    Hash get_hash() const override {
        return Hash(std::string(""));
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::FN) {
            return false;
        }
        const FnType *const other_type = other->as<FnType>();
        return params == other_type->params && return_types == other_type->return_types;
    }

    std::string to_string() const override {
        std::stringstream ss;
        ss << "fn<";
        if (params.empty()) {
            ss << "()";
        } else {
            for (size_t i = 0; i < params.size(); i++) {
                if (i > 0) {
                    ss << ", ";
                }
                if (params.at(i).second) {
                    ss << "mut ";
                } else {
                    ss << "const ";
                }
                ss << params.at(i).first->to_string();
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
        ASSERT(!error_types.empty());
        ASSERT(error_types.front()->to_string() == "anyerror");
        if (error_types.size() > 1) {
            ss << " {";
            for (size_t i = 1; i < error_types.size(); i++) {
                if (i > 1) {
                    ss << ", ";
                }
                ss << error_types.at(i)->to_string();
            }
            ss << "}";
        }
        ss << ">";
        return ss.str();
    }

    std::string get_type_string(const bool is_return_type = false) const override {
        const std::string type_str = is_return_type ? ".type.ret.fn." : ".type.fn.";
        return type_str + to_string();
    }

    /// @var `params`
    /// @brief The parameter list of the referenced function, and whether each parameter is mutable or not
    std::vector<std::pair<std::shared_ptr<Type>, bool>> params;

    /// @var `return_types`
    /// @brief The return types of the referenced function
    std::vector<std::shared_ptr<Type>> return_types;

    /// @var `error_types`
    /// @brief The possible error sets the referenced function can throw
    std::vector<std::shared_ptr<Type>> error_types;
};
