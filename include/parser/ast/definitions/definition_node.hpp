#pragma once

#include "parser/ast/ast_node.hpp"

#include <cassert>

/// @class `DefinitionNode`
/// @brief Base class for all top-level definitions
class DefinitionNode : public ASTNode {
  protected:
    // constructor
    DefinitionNode(                //
        const Hash &file_hash,     //
        const unsigned int line,   //
        const unsigned int column, //
        const unsigned int length  //
        ) :
        ASTNode(file_hash, line, column, length) {}

    // DefinitionNode() :
    // ASTNode(Hash(std::string("")), 0, 0, 0) {}

  public:
    // destructor
    ~DefinitionNode() override = default;
    // copy operations - disabled by default
    DefinitionNode(const DefinitionNode &) = delete;
    DefinitionNode &operator=(const DefinitionNode &) = delete;
    // move operations
    DefinitionNode(DefinitionNode &&) = default;
    DefinitionNode &operator=(DefinitionNode &&) = default;

    /// @enum `Variation`
    /// @brief A enum describing which definition variations exist
    enum class Variation {
        DATA,
        ENTITY,
        ENUM,
        ERROR,
        FUNC,
        FUNCTION,
        IMPORT,
        LINK,
        TEST,
        VARIANT,
    };

    /// @function `get_variation`
    /// @brief Function to return which variation this definition node is
    ///
    /// @return `Variation` The variation of this definition node
    virtual Variation get_variation() const = 0;

    /// @function `as`
    /// @brief Casts this definition node to the requested type, but the requested type must be a child type of this class
    template <typename T>
    std::enable_if_t<std::is_base_of_v<DefinitionNode, T> && !std::is_same_v<DefinitionNode, T>, const T *> inline as() const {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(const_cast<DefinitionNode *>(this));
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<const T *>(this);
#endif
    }

    /// @function `as`
    /// @brief Casts this definition node to the requested type, but the requested type must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<DefinitionNode, T> && !std::is_same_v<DefinitionNode, T>, T *> inline as() {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(this);
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<T *>(this);
#endif
    }
};
