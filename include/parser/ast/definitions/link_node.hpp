#pragma once

#include "../ast_node.hpp"

#include <string>
#include <utility>
#include <vector>

/// LinkNode
///     Represents links within entities
class LinkNode : public ASTNode {
  public:
    explicit LinkNode(std::vector<std::string> &from, std::vector<std::string> &to) :
        from(std::move(from)),
        to(std::move(to)) {}

  private:
    /// from
    ///     The function reference of the function that gets shadowed
    std::vector<std::string> from;
    // to
    //      The function reference of the function that gets referenced
    std::vector<std::string> to;
};
