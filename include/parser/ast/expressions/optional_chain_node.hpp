#pragma once

#include "expression_node.hpp"
#include "parser/type/optional_type.hpp"

#include <variant>

/// @struct `ChainFieldAccess`
/// @brief The data used for a optional-chained field access
struct ChainFieldAccess {
    /// @var `field_name`
    /// @brief The name of the accessed field, if the accessed field has no name it means its accessed via `.$N` instead, for tuples or
    /// multi-types
    std::optional<std::string> field_name;

    /// @var `field_id`
    /// @brief The index of the field to access
    unsigned int field_id;
};

/// @struct `ChainArrayAccess`
/// @brief The data used for a optional-chained array access
struct ChainArrayAccess {
    /// @var `indexing_expressions`
    /// @brief The expressions of all the dimensions indices
    std::vector<std::unique_ptr<ExpressionNode>> indexing_expressions;
};

using ChainOperation = std::variant<ChainFieldAccess, ChainArrayAccess>;

/// @class `OptionalChainNode`
/// @brief Represents optional chains, possibly forced
class OptionalChainNode : public ExpressionNode {
  public:
    OptionalChainNode(                              //
        std::unique_ptr<ExpressionNode> &base_expr, //
        const bool is_toplevel_chain_node,          //
        ChainOperation &operation,                  //
        const std::shared_ptr<Type> &result_type    //
        ) :
        base_expr(std::move(base_expr)),
        is_toplevel_chain_node(is_toplevel_chain_node),
        operation(std::move(operation)) {
        if (is_toplevel_chain_node) {
            this->type = std::make_shared<OptionalType>(result_type);
            if (!Type::add_type(this->type)) {
                this->type = Type::get_type_from_str(this->type->to_string()).value();
            }
        } else {
            this->type = result_type;
        }
    }

    Variation get_variation() const override {
        return Variation::OPTIONAL_CHAIN;
    }

    /// @var `base_expr`
    /// @brief The base expression which is accessed in the optional chain
    std::unique_ptr<ExpressionNode> base_expr;

    /// @var `is_toplevel_chain_node`
    /// @brief Whether this chain node is the top level chain node
    bool is_toplevel_chain_node;

    /// @var `operation`
    /// @brief The operation to do for this chain node
    ChainOperation operation;
};
