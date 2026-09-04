#pragma once

#include "expression_node.hpp"

#include <memory>
#include <vector>

/// @class `InitializerNode`
/// @brief Represents all initializer expressions
class InitializerNode : public ExpressionNode {
  public:
    /// @struct `field`
    /// @brief Represents a single field in the initializer
    struct Field {
        std::string name;
        std::unique_ptr<ExpressionNode> value;
    };

    InitializerNode(                       //
        const Hash &hash,                  //
        const PosTriple &pos,              //
        const std::shared_ptr<Type> &type, //
        std::vector<Field> &fields         //
        ) :
        ExpressionNode(hash, pos, true),
        fields(std::move(fields)) {
        this->type = type;
    }

    Variation get_variation() const override {
        return Variation::INITIALIZER;
    }

    bool is_producer() const override {
        return true;
    }

    std::unique_ptr<ExpressionNode> clone(const unsigned int scope_id) const override {
        std::vector<Field> fields_clone;
        for (auto &[name, value] : fields) {
            fields_clone.emplace_back(Field{name, value->clone(scope_id)});
        }
        return std::make_unique<InitializerNode>(file_hash, PosTriple{line, column, length}, type, fields_clone);
    }

    /// @var `fields`
    /// @brief The fields with which the initializer will be initialized
    std::vector<Field> fields;
};
