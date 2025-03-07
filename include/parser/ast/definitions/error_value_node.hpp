#ifndef __ERROR_VALUE_NODE_HPP__
#define __ERROR_VALUE_NODE_HPP__

#include "../ast_node.hpp"

#include <string>

/// ErrorValueNode
///
class ErrorValueNode : public ASTNode {
  public:
    ErrorValueNode(std::string &type, std::string &message)
        : type(type),
          message(message) {}

  private:
    /// type
    ///     The type of the error
    std::string type;
    /// message
    ///     The message of the error
    std::string message;
};

#endif
