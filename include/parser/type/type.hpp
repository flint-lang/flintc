#pragma once

#include <cassert>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

/// @class `Type`
/// @brief This is the base class of all types, but it cannot be initialized directly. Instead, its just a base type from which all explicit
/// types extend from.
class Type {
  protected:
    Type() = default;

  public:
    virtual ~Type() = default;

    /// @var `types`
    /// @brief A global type register map to track all currently active types
    static inline std::unordered_map<std::string, std::shared_ptr<Type>> types;

    /// @var `types_mutex`
    /// @brief A mutex for thread-safe access on the `types` and `unknown_types` map
    static inline std::shared_mutex types_mutex;

    /// @enum `Variation`
    /// @brief A enum describing which type variations exist
    enum class Variation {
        ALIAS,
        ARRAY,
        DATA,
        ENUM,
        ERROR_SET,
        GROUP,
        MULTI,
        OPTIONAL,
        POINTER,
        PRIMITIVE,
        RANGE,
        TUPLE,
        UNKNOWN,
        VARIANT,
    };

    /// @function `get_variation`
    /// @brief Function to return which variation this type is
    ///
    /// @return `Variation` The variation of this type
    virtual Variation get_variation() const = 0;

    /// @function `equals`
    /// @brief Function to check whether this type is equal to a different type
    ///
    /// @param `other` The other type to compare this type against
    /// @return `bool` Whether the types are equal
    virtual bool equals(const std::shared_ptr<Type> &other) const = 0;

    /// @function `as`
    /// @brief Casts this type to the requested type, but the requested type must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<Type, T> && !std::is_same_v<Type, T>, const T *> inline as() const {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(const_cast<Type *>(this));
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<const T *>(this);
#endif
    }

    /// @function `as`
    /// @brief Casts this type to the requested type, but the requested type must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<Type, T> && !std::is_same_v<Type, T>, T *> inline as() {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(this);
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<T *>(this);
#endif
    }

    /// @function `to_string`
    /// @brief Returns the string representation of the type
    ///
    /// @return `std::strint` The string representation of this type
    virtual std::string to_string() const = 0;

    /// @function `init_types`
    /// @brief Initializes all primitive types to be ready to be used
    /// @note This function only needs to be called once at the start of the parsing phase
    static void init_types();

    /// @function `clear_types`
    /// @brief Clears all the types from the type list
    /// @note This function is only allowed to be called at the end of the program, calling it while the parser or codegen are active is
    /// actually...not good
    static void clear_types();

    /// @function `add_type`
    /// @brief Adds the given type to the type list. Returns whether the type was already present
    ///
    /// @param `type_to_add` The type to add to the types map
    /// @return `bool` Whether the type was newly added (true) or already present (false)
    static bool add_type(const std::shared_ptr<Type> &type_to_add);

    /// @function get_primitive_type`
    /// @brief Returns the primitive type of a given string type, adds the type if it doesnt exist yet
    ///
    /// @param `type_str` The type string
    /// @retrun `std::shared_ptr<Type>` The added type
    static std::shared_ptr<Type> get_primitive_type(const std::string &type_str);

    /// @function `get_type_from_str`
    /// @brief Returns the type from a given string. If the types map does not contain this type, nullopt is retunred
    ///
    /// @param `type_str` The string representation of the full type
    /// @return `std::optional<std::shared_ptr<Type>>` The type from the types map, nullopt if the given type does not exist
    static std::optional<std::shared_ptr<Type>> get_type_from_str(const std::string &type_str);
};
