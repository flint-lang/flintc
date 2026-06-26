#pragma once

#include "parser/hash.hpp"

/// @class `ASTNode`
/// @brief Base class for all AST nodes.
class ASTNode {
  protected:
    ASTNode() = default;
    ASTNode(const Hash &file_hash, const unsigned int line, const unsigned int column, const unsigned int length) :
        file_hash(file_hash),
        line(line),
        column(column),
        length(length),
        end_line(line) {}

  public:
    // destructor
    virtual ~ASTNode() = default;
    // copy operators
    ASTNode(const ASTNode &) = default;
    ASTNode &operator=(const ASTNode &) = default;
    // move operators
    ASTNode(ASTNode &&) = default;
    ASTNode &operator=(ASTNode &&) = default;

    struct PosTriple {
        unsigned int line;
        unsigned int column;
        unsigned int length;
    };

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

    /// @var `end_line`
    /// @brief The last line this AST node spans (inclusive)
    unsigned int end_line;

    /// @function `contains_pos`
    /// @brief Checks whether this AST Node contains the given line & column position
    ///
    /// @param `l` The line to check whether this ASTNode contains it
    /// @param `c` The column to chek whether this ASTNode contains it
    /// @return `bool` Whether this ASTNode contains the given position
    bool contains_pos(unsigned int l, unsigned int c) const {
        return line == l                            //
            ? (c >= column && c <= column + length) //
            : (line < l && l <= end_line);
    }
};
