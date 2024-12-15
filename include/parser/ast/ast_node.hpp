#ifndef __BASE_AST_HPP__
#define __BASE_AST_HPP__

/// ASTNode
///     Base class for all AST nodes.
class ASTNode {
  protected:
    // private constructor
    ASTNode() = default;

  public:
    // destructor
    virtual ~ASTNode() = default;
    // copy operators
    ASTNode(const ASTNode &) = default;
    ASTNode &operator=(const ASTNode &) = default;
    // move operators
    ASTNode(ASTNode &&) = default;
    ASTNode &operator=(ASTNode &&) = default;
};

#endif
