#pragma once

#include "parser/ast/definitions/definition_node.hpp"

#include <string>
#include <vector>

/// @class `EnumNode`
/// @brief Represents enum type definitions
class EnumNode : public DefinitionNode {
  public:
    explicit EnumNode(                                                  //
        const Hash &file_hash,                                          //
        const unsigned int line,                                        //
        const unsigned int column,                                      //
        const unsigned int length,                                      //
        const std::string &name,                                        //
        const std::vector<std::pair<std::string, unsigned int>> &values //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        name(name),
        values(values) {}

    Variation get_variation() const override {
        return Variation::ENUM;
    }

    /// @var `name`
    /// @brief The name of the enum
    std::string name;

    /// @var `values`
    /// @brief The string values of the enum. The first value is the tag of the value, the second one it's integer value. Every value needs
    /// to be unique among all tags
    std::vector<std::pair<std::string, unsigned int>> values;
};
