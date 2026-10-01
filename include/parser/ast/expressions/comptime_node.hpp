#pragma once

#include "evaluator/value/value.hpp"
#include "expression_node.hpp"

#include <memory>

/// @class `ComptimeNode`
/// @brief Represents compile-time known expressions
class ComptimeNode : public ExpressionNode {
  public:
    explicit ComptimeNode(                       //
        const Hash &hash,                        //
        const PosTriple &pos,                    //
        const std::shared_ptr<Value> &value,     //
        const std::shared_ptr<Type> &target_type //
        ) :
        ExpressionNode(hash, pos, true),
        value(value) {
        this->type = target_type;
    }

    Variation get_variation() const override {
        return Variation::COMPTIME;
    }

    std::unique_ptr<ExpressionNode> clone([[maybe_unused]] const unsigned int scope_id) const override {
        return std::make_unique<ComptimeNode>(file_hash, PosTriple{line, column, length}, value, type);
    }

    // empty constructor
    ComptimeNode() = delete;
    // deconstructor
    ~ComptimeNode() override = default;
    // copy operations - disabled due to unique_ptr memeber
    ComptimeNode(const ComptimeNode &) = delete;
    ComptimeNode &operator=(const ComptimeNode &) = delete;
    // move operations
    ComptimeNode(ComptimeNode &&) = default;
    ComptimeNode &operator=(ComptimeNode &&) = default;

    /// @var `value`
    /// @brief The value this comptime node wraps around
    std::shared_ptr<Value> value;
};
