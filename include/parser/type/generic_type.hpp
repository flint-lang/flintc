#pragma once

#include "parser/hash.hpp"
#include "type.hpp"

class DefinitionNode;

/// @class `GenericType`
/// @brief Represents generic types
class GenericType : public Type {
  public:
    GenericType(const std::shared_ptr<Type> &base, const std::vector<std::shared_ptr<Type>> &cvl) :
        base(base),
        cvl(cvl) {}

    Variation get_variation() const override {
        return Variation::GENERIC;
    }

    bool is_freeable() const override {
        return false;
    }

    bool is_dima_managed() const override {
        return false;
    }

    bool is_default_constructible() const override {
        return false;
    }

    bool is_runtime_compatible() const override {
        return false;
    }

    std::optional<std::unique_ptr<ExpressionNode>> get_default_value( //
        const std::shared_ptr<Type> &self,                            //
        [[maybe_unused]] const Hash &hash,                            //
        [[maybe_unused]] const PosTriple &pos,                        //
        [[maybe_unused]] const unsigned int scope_id                  //
    ) const override {
        ASSERT(self.get() == static_cast<const Type *>(this));
        return std::nullopt;
    }

    Hash get_hash() const override {
        return base->get_hash();
    }

    bool equals(const std::shared_ptr<Type> &other) const override {
        if (other->get_variation() != Variation::GENERIC) {
            return false;
        }
        const GenericType *const other_type = other->as<GenericType>();
        if (cvl.size() != other_type->cvl.size()) {
            return false;
        }
        for (size_t i = 0; i < cvl.size(); i++) {
            if (!cvl.at(i)->equals(other_type->cvl.at(i))) {
                return false;
            }
        }
        return base->equals(other_type->base);
    }

    std::string to_string() const override {
        std::stringstream ss;
        ss << base->to_string();
        ss << "[";
        for (size_t i = 0; i < cvl.size(); i++) {
            if (i > 0) {
                ss << ", ";
            }
            ss << cvl.at(i)->to_string();
        }
        ss << "]";
        return ss.str();
    }

    std::string get_type_string([[maybe_unused]] const bool is_return_type = false) const override {
        ASSERT(false, "Generic types cannot be used in the generator and already should have been resolved by now");
        UNREACHABLE();
    }

    /// @var `base`
    /// @brief The type this generic type (parially) specializes
    std::shared_ptr<Type> base;

    /// @var `cvl`
    /// @brief The applied comptime values of the generic type. This is types-only for now but will be changed in the future to be
    /// value-based once compile-time evaluation is present
    std::vector<std::shared_ptr<Type>> cvl;
};
