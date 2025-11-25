#pragma once

#include "parser/ast/ast_node.hpp"
#include "parser/type/type.hpp"

#include <cassert>

/// @class `ExpressionNode`
/// @brief Base class for all expressions
class ExpressionNode : public ASTNode {
  protected:
    // constructor
    ExpressionNode() = default;

    explicit ExpressionNode(const Hash &hash) :
        ASTNode(hash, 0, 0, 0) {}

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
