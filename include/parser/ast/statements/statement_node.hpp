#pragma once

#include "parser/ast/annotation_node.hpp"
#include "parser/ast/ast_node.hpp"

#include <cassert>

/// @class `StatementNode`
/// @brief Base class for all statements
class StatementNode : public ASTNode {
  protected:
    explicit StatementNode(const std::vector<AnnotationNode> &annotations) :
        annotations(annotations) {}

    // constructor
    StatementNode() = default;

  public:
    // destructor
    ~StatementNode() override = default;
    // copy operations - disabled by default
    StatementNode(const StatementNode &) = delete;
    StatementNode &operator=(const StatementNode &) = delete;
    // move operations
    StatementNode(StatementNode &&) = default;
    StatementNode &operator=(StatementNode &&) = default;

    /// @var `annotations`
    /// @brief The annotations defined for this statement
    std::vector<AnnotationNode> annotations;

    /// @enum `Variation`
    /// @brief A enum describing which statement variations exist
    enum class Variation {
        ARRAY_ASSIGNMENT,
        ASSIGNMENT,
        BREAK,
        CALL,
        CATCH,
        CONTINUE,
        DATA_FIELD_ASSIGNMENT,
        DECLARATION,
        DO_WHILE,
        ENHANCED_FOR_LOOP,
        FOR_LOOP,
        GROUP_ASSIGNMENT,
        GROUP_DECLARATION,
        GROUPED_DATA_FIELD_ASSIGNMENT,
        IF,
        INSTANCE_CALL,
        RETURN,
        STACKED_ASSIGNMENT,
        STACKED_ARRAY_ASSIGNMENT,
        STACKED_GROUPED_ASSIGNMENT,
        SWITCH,
        THROW,
        UNARY_OP,
        WHILE,
    };

    /// @function `get_variation`
    /// @brief Function to return which variation this statement node is
    ///
    /// @return `Variation` The variation of this statement node
    virtual Variation get_variation() const = 0;

    /// @function `as`
    /// @brief Casts this statement node to the requested type, but the requested type must be a child type of this class
    template <typename T>
    std::enable_if_t<std::is_base_of_v<StatementNode, T> && !std::is_same_v<StatementNode, T>, const T *> inline as() const {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(const_cast<StatementNode *>(this));
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<const T *>(this);
#endif
    }

    /// @function `as`
    /// @brief Casts this statement node to the requested type, but the requested type must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<StatementNode, T> && !std::is_same_v<StatementNode, T>, T *> inline as() {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(this);
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<T *>(this);
#endif
    }
};
