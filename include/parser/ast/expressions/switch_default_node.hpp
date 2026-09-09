#pragma once

#include "expression_node.hpp"

/// @class `SwitchDefaultNode`
/// @brief Represents default values, e.g. the 'else' branch, in switch statements/expressions
class SwitchDefaultNode : public ExpressionNode {
  public:
    SwitchDefaultNode(                    //
        const Hash &hash,                 //
        const PosTriple &pos,             //
        const std::shared_ptr<Type> &type //
        ) :
        ExpressionNode(hash, pos, true) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::SWITCH_DEFAULT;
    }

    std::unique_ptr<ExpressionNode> clone([[maybe_unused]] const unsigned int scope_id) const override {
        return std::make_unique<SwitchDefaultNode>(file_hash, PosTriple{line, column, length}, type);
    }
};
