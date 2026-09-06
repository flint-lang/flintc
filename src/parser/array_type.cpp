#include "parser/type/array_type.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/inline_array_initializer_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"

std::optional<std::unique_ptr<ExpressionNode>> ArrayType::get_default_value( //
    const std::shared_ptr<Type> &self,                                       //
    const Hash &hash,                                                        //
    const PosTriple &pos,                                                    //
    const unsigned int scope_id                                              //
) const {
    ASSERT(self.get() == static_cast<const Type *>(this));
    if (!is_default_constructible()) {
        return std::nullopt;
    }
    std::vector<std::unique_ptr<ExpressionNode>> length_expressions;
    if (sizes.has_value()) {
        for (const size_t size : sizes.value()) {
            LitValue s = LitInt(APInt(std::to_string(size)));
            length_expressions.emplace_back(std::make_unique<LiteralNode>(hash, pos, s, Type::get_primitive_type("u64"), false));
        }
        std::unique_ptr<ExpressionNode> initializer_expression = type->get_default_value(type, hash, pos, scope_id).value();
        return std::make_unique<ArrayInitializerNode>(hash, pos, self, length_expressions, initializer_expression);
    } else {
        LitValue zero = LitInt(APInt("0"));
        length_expressions.emplace_back(std::make_unique<LiteralNode>(hash, pos, zero, Type::get_primitive_type("u64"), false));
        std::vector<std::unique_ptr<ExpressionNode>> initializer_values{};
        return std::make_unique<InlineArrayInitializerNode>(hash, pos, self, length_expressions, initializer_values);
    }
}
