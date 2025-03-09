#pragma once

#include "../ast_node.hpp"

#include <string>

/// ErrorValueNode
///
class ErrorValueNode : public ASTNode {
  public:
    ErrorValueNode(std::string &type, std::string &message) :
        type(type),
        message(message) {}

  private:
    /// type
    ///     The type of the error
    std::string type;
    /// message
    ///     The message of the error
    std::string message;
};
