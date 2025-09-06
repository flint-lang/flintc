#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

#include <cstdint>
#include <string>
#include <vector>

/// @class `ErrorNode`
/// @brief Represents error set definitions
class ErrorNode : public ASTNode {
  public:
    explicit ErrorNode(                                  //
        const std::string &file_name,                    //
        const unsigned int line,                         //
        const unsigned int column,                       //
        const unsigned int length,                       //
        const std::string &name,                         //
        const std::string &parent_error,                 //
        const std::vector<std::string> &values,          //
        const std::vector<std::string> &default_messages //
        ) :
        ASTNode(file_name, line, column, length),
        name(name),
        parent_error(parent_error),
        values(values),
        default_messages(default_messages) {
        error_id = Type::get_type_id_from_str(name);
    }

    /// @function `get_parent_node`
    /// @breif Returns the parent node of this node, if it has any
    ///
    /// @return `std::optional<const ErrorNode *>` The parent error node, nullopt if it doesnt have a parent
    std::optional<const ErrorNode *> get_parent_node() const;

    /// @function `get_value_count`
    /// @brief Returns the total count of error values within this set, this includes all values from it's base errors too
    ///
    /// @return `unsigned int` The total count of values within this set
    unsigned int get_value_count() const;

    /// @function `get_id_msg_pair_of_value`
    /// @brief Returns the id of a given string value, the value is either in this set or in one of it's base sets together with the default
    /// message of that value, or nullopt if the value is not part of this error set
    ///
    /// @param `value` The error value to search for
    /// @return `std::optional<std::pair<unsigned int, std::string>>` The ID of the error value with the default message, nullopt if the
    /// value is not part of this set
    std::optional<std::pair<unsigned int, std::string>> get_id_msg_pair_of_value(const std::string &value) const;

    /// @var `name`
    /// @brief The name of the new error type
    std::string name;

    /// @var `parent_error`
    /// @brief The name of the parent error set this error set extends
    std::string parent_error;

    /// @var `values`
    /// @brief The possible error values error set contains
    std::vector<std::string> values;

    /// @var `default_messages`
    /// @brief THe default messages of all the error values
    std::vector<std::string> default_messages;

    /// @var `error_id`
    /// @brief The ID of the error type, which is generated using hashing of the error type's name
    uint32_t error_id;
};
