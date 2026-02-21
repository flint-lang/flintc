#pragma once

#include "expression_node.hpp"
#include "parser/type/range_type.hpp"
#include "resolver/resolver.hpp"

#include <memory>

/// @class `RangeExpressionNode`
/// @brief Represents range expressions
class RangeExpressionNode : public ExpressionNode {
  public:
    explicit RangeExpressionNode(                     //
        const Hash &hash,                             //
        std::unique_ptr<ExpressionNode> &lower_bound, //
        std::unique_ptr<ExpressionNode> &upper_bound  //
        ) :
        ExpressionNode(hash),
        lower_bound(std::move(lower_bound)),
        upper_bound(std::move(upper_bound)) {
        assert(this->lower_bound->type == this->upper_bound->type);
        std::shared_ptr<Type> range_type = std::make_shared<RangeType>(this->lower_bound->type);
        Namespace *file_namespace = Resolver::get_namespace_from_hash(file_hash);
        if (file_namespace->add_type(range_type)) {
            this->type = range_type;
        } else {
            // The type was already present
            this->type = file_namespace->get_type_from_str(range_type->to_string()).value();
        }
    }

    Variation get_variation() const override {
        return Variation::RANGE_EXPRESSION;
    }

    std::unique_ptr<ExpressionNode> clone() const override {
        std::unique_ptr<ExpressionNode> lower_bound_clone = lower_bound->clone();
        std::unique_ptr<ExpressionNode> upper_bound_clone = upper_bound->clone();
        return std::make_unique<RangeExpressionNode>(file_hash, lower_bound_clone, upper_bound_clone);
    }

    /// @var `lower_bound`
    /// @brief The lower bound of the range
    std::unique_ptr<ExpressionNode> lower_bound;

    /// @var `upper_bound`
    /// @brief The upper bound of the range
    std::unique_ptr<ExpressionNode> upper_bound;
};
