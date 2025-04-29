#pragma once

#include "types.hpp"

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

    /// @function `to_string`
    /// @brief Returns the string representation of the type
    ///
    /// @return `std::strint` The string representation of this type
    virtual std::string to_string() = 0;

    /// @function `init_types`
    /// @brief Initializes all primitive types to be ready to be used
    /// @note This function only needs to be called once at the start of the parsing phase
    static void init_types();

    /// @function `add_type`
    /// @brief Adds the given type to the type list. Returns whether the type was already present
    ///
    /// @param `type_to_add` The type to add to the types map
    /// @return `bool` Whether the type was newly added (true) or already present (false)
    static bool add_type(const std::shared_ptr<Type> &type_to_add);

    /// @function `get_type`
    /// @brief Returns the type of a given token list, adds the type if it doesnt exist yet
    ///
    /// @param `tokens` The list of all tokens that represent the type
    /// @param `mutex_already_locked` For recursive calls of `get_type` to prevent deadlocks
    /// @return `std::optional<std::shared_ptr<Type>>` The shared pointer to the Type, nullopt if the creation of the type faied
    static std::optional<std::shared_ptr<Type>> get_type(const token_slice &tokens, const bool mutex_already_locked = false);

    /// @function get_simple_type`
    /// @brief Returns the simple type of a given string type, adds the type if it doesnt exist yet
    ///
    /// @param `type_str` The type string
    /// @retrun `std::shared_ptr<Type>` The added type
    static std::shared_ptr<Type> get_simple_type(const std::string &type_str);

    /// @function `get_type_from_str`
    /// @brief Returns the type from a given string. If the types map does not contain this type, nullopt is retunred
    ///
    /// @param `type_str` The string representation of the full type
    /// @return `std::optional<std::shared_ptr<Type>>` The type from the types map, nullopt if the given type does not exist
    static std::optional<std::shared_ptr<Type>> get_type_from_str(const std::string &type_str);

    /// @function `str_to_type`
    /// @brief Converts a string to the type. Its basically a wrapper around `get_type_from_str` that panics if the type cannot be converted
    ///
    /// @param `str` The string to convert
    /// @return `std::shared_ptr<Type>` The converted type
    static std::shared_ptr<Type> str_to_type(const std::string_view &str);

  private:
    /// @var `types`
    /// @brief A global type register map to track all currently active types
    static inline std::unordered_map<std::string, std::shared_ptr<Type>> types;

    /// @var `types_mutex`
    /// @brief A mutex for access on the types map
    static inline std::shared_mutex types_mutex;

    /// @function `create_type`
    /// @brief Creates a type from a given list of tokens
    ///
    /// @param `tokens` The list of tokens to create the type from
    /// @param `mutex_already_locked` Whether the types mutex is already locked
    /// @return `std::optional<Type>` The created type, nullopt if creation failed
    static std::optional<std::shared_ptr<Type>> create_type(const token_slice &tokens, const bool mutex_already_locked = false);
};
