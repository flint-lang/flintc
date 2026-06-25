#pragma once

#include "expression_node.hpp"

/// @class `DefaultNode`
/// @brief Represents default values (the '_' symbol). A default node literally has only a type and nothing else
class DefaultNode : public ExpressionNode {
  public:
    DefaultNode(                          //
        const Hash &hash,                 //
        const PosTriple &pos,             //
        const std::shared_ptr<Type> &type //
        ) :
        ExpressionNode(hash, pos, true) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::DEFAULT;
    }

    std::unique_ptr<ExpressionNode> clone([[maybe_unused]] const unsigned int scope_id) const override {
        return std::make_unique<DefaultNode>(file_hash, PosTriple{line, column, length}, type);
    }
};
