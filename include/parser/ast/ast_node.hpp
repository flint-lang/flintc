#pragma once

#include <string>

/// @class `ASTNode`
/// @brief Base class for all AST nodes.
class ASTNode {
  protected:
    // private constructor
    ASTNode() = default;
    ASTNode(const std::string &file_name, const unsigned int line, const unsigned int column, const unsigned int length) :
        file_name(file_name),
        line(line),
        column(column),
        length(length) {}

  public:
    // destructor
    virtual ~ASTNode() = default;
    // copy operators
    ASTNode(const ASTNode &) = default;
    ASTNode &operator=(const ASTNode &) = default;
    // move operators
    ASTNode(ASTNode &&) = default;
    ASTNode &operator=(ASTNode &&) = default;

    /// @var `file_name`
    /// @brief The name of the file this AST Node is defined in
    std::string file_name;

    /// @var `line`
    /// @brief The line in the file this AST node starts at
    unsigned int line;

    /// @var `column`
    /// @brief The column in the file this AST node starts at
    unsigned int column;

    /// @var `length`
    /// @brief The length of the AST node
    unsigned int length;
};
