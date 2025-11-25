#pragma once

#include "parser/hash.hpp"

/// @class `ASTNode`
/// @brief Base class for all AST nodes.
class ASTNode {
  protected:
    // private constructor
    ASTNode() = default;
    ASTNode(const Hash &file_hash, const unsigned int line, const unsigned int column, const unsigned int length) :
        file_hash(file_hash),
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

    /// @var `file_hash`
    /// @brief The hash of the file this AST Node is defined in
    Hash file_hash;

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
