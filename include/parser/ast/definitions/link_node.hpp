#pragma once

#include "parser/ast/definitions/definition_node.hpp"

#include <string>
#include <utility>
#include <vector>

/// LinkNode
///     Represents links within entities
class LinkNode : public DefinitionNode {
  public:
    explicit LinkNode(                  //
        const Hash &file_hash,          //
        const unsigned int line,        //
        const unsigned int column,      //
        const unsigned int length,      //
        std::vector<std::string> &from, //
        std::vector<std::string> &to    //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        from(std::move(from)),
        to(std::move(to)) {}

    Variation get_variation() const override {
        return Variation::LINK;
    }

  private:
    /// from
    ///     The function reference of the function that gets shadowed
    std::vector<std::string> from;
    // to
    //      The function reference of the function that gets referenced
    std::vector<std::string> to;
};
