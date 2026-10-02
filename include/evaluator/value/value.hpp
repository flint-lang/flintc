#pragma once

#include "parser/type/type.hpp"

/// @class `Value`
/// @brief This is the base class of all compile-time values, but it cannot be initialized directly. Instead, its just a base type from
/// which all explicit values extend from.
class Value {
  protected:
    explicit Value(const std::shared_ptr<Type> &type) :
        type(type) {}

  public:
    virtual ~Value() = default;

    /// @enum `Variation`
    /// @brief A enum describing which value variations exist
    enum class Variation {
        ARRAY,
        BOOL,
        CHAR,
        FLOAT,
        INT,
        STR,
    };

    /// @function `get_variation`
    /// @brief Function to return which variation this value is
    ///
    /// @return `Variation` The variation of this value
    virtual Variation get_variation() const = 0;

    /// @function `as`
    /// @brief Casts this value to the requested value, but the requested value must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<Value, T> && !std::is_same_v<Value, T>, const T *> inline as() const {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(const_cast<Value *>(this));
        ASSERT(result, "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<const T *>(this);
#endif
    }

    /// @function `as`
    /// @brief Casts this value to the requested type, but the requested type must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<Value, T> && !std::is_same_v<Value, T>, T *> inline as() {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(this);
        ASSERT(result, "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<T *>(this);
#endif
    }

    /// @var `type`
    /// @brief The type of this compile-time value
    std::shared_ptr<Type> type;
};
