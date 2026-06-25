#pragma once

#include "expression_node.hpp"
#include "parser/ast/namespace.hpp"
#include "parser/type/fn_type.hpp"
#include "resolver/resolver.hpp"

/// @class `FunctionReferenceNode`
/// @brief Represents function references
class FunctionReferenceNode : public ExpressionNode {
  public:
    FunctionReferenceNode(                      //
        const Hash &hash,                       //
        const PosTriple &pos,                   //
        const FunctionNode *referenced_function //
        ) :
        ExpressionNode(hash, pos, true),
        referenced_function(referenced_function) {
        std::vector<std::pair<std::shared_ptr<Type>, bool>> params;
        for (const auto &[param_type, param_name, param_is_mutable] : referenced_function->parameters) {
            params.emplace_back(param_type, param_is_mutable);
        }
        std::shared_ptr<Type> fn_type = std::make_shared<FnType>(                       //
            params, referenced_function->return_types, referenced_function->error_types //
        );
        Namespace *file_namespace = Resolver::get_namespace_from_hash(file_hash);
        if (file_namespace->add_type(fn_type)) {
            this->type = fn_type;
        } else {
            // The type was already present
            this->type = file_namespace->get_type_from_str(fn_type->to_string()).value();
        }
    }

    Variation get_variation() const override {
        return Variation::FUNCTION_REFERENCE;
    }

    std::unique_ptr<ExpressionNode> clone([[maybe_unused]] const unsigned int scope_id) const override {
        return std::make_unique<FunctionReferenceNode>(file_hash, PosTriple{line, column, length}, referenced_function);
    }

    /// @var `referenced_function`
    /// @brief The function referenced by this function reference
    const FunctionNode *referenced_function;
};
