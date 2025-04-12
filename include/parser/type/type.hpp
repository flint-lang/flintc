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
    /// @function `add_and_or_get_type`
    /// @brief Returns the type of a given token list, adds the type if it doesnt exist yet
    ///
    /// @param `tokens` The list of all tokens that represent the type
    /// @return `std::optional<std::shared_ptr<Type>>` The shared pointer to the Type, nullopt if the creation of the type faied
    static std::optional<std::shared_ptr<Type>> add_and_or_get_type(token_list &tokens);

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
    /// @return `std::optional<Type>` The created type, nullopt if creation failed
    static std::optional<Type> create_type(token_list &tokens);
};
