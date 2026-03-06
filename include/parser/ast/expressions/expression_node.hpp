#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

#include <cassert>

/// @class `ExpressionNode`
/// @brief Base class for all expressions
class ExpressionNode : public ASTNode {
  protected:
    // constructor
    ExpressionNode(const bool is_const) :
        is_const(is_const) {};

    explicit ExpressionNode(const Hash &hash, const bool is_const) :
        ASTNode(hash, 0, 0, 0),
        is_const(is_const) {}

  public:
    // deconstructor
    virtual ~ExpressionNode() override = default;
    // copy operations - deleted because of unique_ptr member
    ExpressionNode(const ExpressionNode &) = delete;
    ExpressionNode &operator=(const ExpressionNode &) = delete;
    // move operations
    ExpressionNode(ExpressionNode &&) = default;
    ExpressionNode &operator=(ExpressionNode &&) = default;

    /// @var `type`
    /// @brief The type of the expression
    std::shared_ptr<Type> type;

    /// @var `is_const`
    /// @brief Whether the expression is constant, like a variable expression or any operation on a variable that is constant. Constness
    /// propagates through all expressions, so `d.x` is a const expression if the `d` variable itself is const.
    bool is_const;

    /// @enum `Variation`
    /// @brief A enum describing which expression variations exist
    enum class Variation {
        ARRAY_ACCESS,
        ARRAY_INITIALIZER,
        BINARY_OP,
        CALL,
        DATA_ACCESS,
        DEFAULT,
        GROUP_EXPRESSION,
        GROUPED_DATA_ACCESS,
        INITIALIZER,
        INSTANCE_CALL,
        LITERAL,
        OPTIONAL_CHAIN,
        OPTIONAL_UNWRAP,
        RANGE_EXPRESSION,
        STRING_INTERPOLATION,
        SWITCH_EXPRESSION,
        SWITCH_MATCH,
        TYPE_CAST,
        TYPE,
        UNARY_OP,
        VARIABLE,
        VARIANT_EXTRACTION,
        VARIANT_UNWRAP,
    };

    /// @function `get_variation`
    /// @brief Function to return which variation this expression node is
    ///
    /// @return `Variation` The variation of this expression node
    virtual Variation get_variation() const = 0;

    /// @function `clone`
    /// @brief Clones this expression by creating a new expression node from this one
    ///
    /// @brief `scope_id` The ID of the scope in which we clone the expression into
    /// @return `std::unique_ptr<ExpressionNode>` The cloned expression
    virtual std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const = 0;

    /// @function `as`
    /// @brief Casts this expression node to the requested type, but the requested type must be a child type of this class
    template <typename T>
    std::enable_if_t<std::is_base_of_v<ExpressionNode, T> && !std::is_same_v<ExpressionNode, T>, const T *> inline as() const {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(const_cast<ExpressionNode *>(this));
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<const T *>(this);
#endif
    }

    /// @function `as`
    /// @brief Casts this expression node to the requested type, but the requested type must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<ExpressionNode, T> && !std::is_same_v<ExpressionNode, T>, T *> inline as() {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(this);
        assert(result && "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<T *>(this);
#endif
    }
};
