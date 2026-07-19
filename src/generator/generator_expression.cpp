#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "globals.hpp"
#include "lexer/builtins.hpp"
#include "lexer/token.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/callable_call_node_expression.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/instance_call_node_expression.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/expressions/type_node.hpp"
#include "parser/parser.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/interface_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include "parser/type/vector_type.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"

#include <llvm/IR/Instructions.h>
#include <stack>
#include <string>
#include <variant>

Generator::group_mapping Generator::Expression::generate_expression( //
    llvm::IRBuilder<> &builder,                                      //
    GenerationContext &ctx,                                          //
    garbage_type &garbage,                                           //
    const unsigned int expr_depth,                                   //
    const ExpressionNode *expression_node,                           //
    const bool is_reference                                          //
) {
    std::vector<llvm::Value *> group_map;
    switch (expression_node->get_variation()) {
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            const auto *node = expression_node->as<ArrayAccessNode>();
            group_map.emplace_back(generate_array_access(builder, ctx, garbage, expr_depth, node, is_reference));
            return group_map;
        }
        case ExpressionNode::Variation::ARRAY_INITIALIZER: {
            const auto *node = expression_node->as<ArrayInitializerNode>();
            group_map.emplace_back(generate_array_initializer(builder, ctx, garbage, expr_depth, node));
            return group_map;
        }
        case ExpressionNode::Variation::BINARY_OP: {
            const auto *node = expression_node->as<BinaryOpNode>();
            return generate_binary_op(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::CALL: {
            const auto *node = expression_node->as<CallNodeExpression>();
            return generate_call(builder, ctx, static_cast<const CallNodeBase *>(node), is_reference);
        }
        case ExpressionNode::Variation::CALLABLE_CALL: {
            const auto *node = expression_node->as<CallableCallNodeExpression>();
            return generate_callable_call(builder, ctx, static_cast<const CallableCallNodeBase *>(node), is_reference);
        }
        case ExpressionNode::Variation::DATA_ACCESS: {
            const auto *node = expression_node->as<DataAccessNode>();
            return generate_data_access(builder, ctx, garbage, expr_depth, node, is_reference);
        }
        case ExpressionNode::Variation::DEFAULT: {
            [[maybe_unused]] const auto *node = expression_node->as<DefaultNode>();
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET); // TODO: Somehow it was unused until now?
            return std::nullopt;
        }
        case ExpressionNode::Variation::FUNCTION_REFERENCE: {
            const auto *node = expression_node->as<FunctionReferenceNode>();
            return std::vector<llvm::Value *>{generate_function_reference(builder, ctx, node)};
        }
        case ExpressionNode::Variation::GROUP_EXPRESSION: {
            const auto *node = expression_node->as<GroupExpressionNode>();
            return generate_group_expression(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::GROUPED_ARRAY_ACCESS: {
            const auto *node = expression_node->as<GroupedArrayAccessNode>();
            return generate_grouped_array_access(builder, ctx, garbage, expr_depth, node, is_reference);
        }
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
            const auto *node = expression_node->as<GroupedDataAccessNode>();
            return generate_grouped_data_access(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::INITIALIZER: {
            const auto *node = expression_node->as<InitializerNode>();
            return generate_initializer(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::INLINE_ARRAY_INITIALIZER: {
            const auto *node = expression_node->as<InlineArrayInitializerNode>();
            group_map.emplace_back(generate_inline_array_initializer(builder, ctx, garbage, expr_depth, node));
            return group_map;
        }
        case ExpressionNode::Variation::INSTANCE_CALL: {
            const auto *node = expression_node->as<InstanceCallNodeExpression>();
            return generate_instance_call(builder, ctx, garbage, expr_depth, static_cast<const InstanceCallNodeBase *>(node), is_reference);
        }
        case ExpressionNode::Variation::LITERAL: {
            const auto *node = expression_node->as<LiteralNode>();
            return generate_literal(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::OPTIONAL_CHAIN: {
            const auto *node = expression_node->as<OptionalChainNode>();
            return generate_optional_chain(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::OPTIONAL_UNWRAP: {
            const auto *node = expression_node->as<OptionalUnwrapNode>();
            return generate_optional_unwrap(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::RANGE_EXPRESSION: {
            const auto *node = expression_node->as<RangeExpressionNode>();
            return generate_range_expression(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::STRING_INTERPOLATION: {
            const auto *node = expression_node->as<StringInterpolationNode>();
            group_map.emplace_back(generate_string_interpolation(builder, ctx, garbage, expr_depth, node));
            return group_map;
        }
        case ExpressionNode::Variation::SWITCH_EXPRESSION: {
            const auto *node = expression_node->as<SwitchExpression>();
            return generate_switch_expression(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::SWITCH_MATCH: {
            [[maybe_unused]] const auto *node = expression_node->as<SwitchMatchNode>();
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET); // Somehow it was unused until now?
            return std::nullopt;
        }
        case ExpressionNode::Variation::TYPE_CAST: {
            const auto *node = expression_node->as<TypeCastNode>();
            return generate_type_cast(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::TYPE: {
            [[maybe_unused]] const auto *node = expression_node->as<TypeNode>();
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET); // Somehow it was unused until now?
            return std::nullopt;
        }
        case ExpressionNode::Variation::UNARY_OP: {
            const auto *node = expression_node->as<UnaryOpExpression>();
            return generate_unary_op_expression(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::VARIABLE: {
            const auto *node = expression_node->as<VariableNode>();
            group_map.emplace_back(generate_variable(builder, ctx, node, is_reference));
            return group_map;
        }
        case ExpressionNode::Variation::VARIANT_EXTRACTION: {
            const auto *node = expression_node->as<VariantExtractionNode>();
            return generate_variant_extraction(builder, ctx, node);
        }
        case ExpressionNode::Variation::VARIANT_UNWRAP: {
            const auto *node = expression_node->as<VariantUnwrapNode>();
            return generate_variant_unwrap(builder, ctx, node);
        }
    }
    __builtin_unreachable();
}

Generator::group_mapping Generator::Expression::generate_literal( //
    llvm::IRBuilder<> &builder,                                   ///
    GenerationContext &ctx,                                       //
    garbage_type &garbage,                                        //
    const unsigned int expr_depth,                                //
    const LiteralNode *literal_node                               //
) {
    if (std::holds_alternative<LitInt>(literal_node->value)) {
        const APInt lit_int = std::get<LitInt>(literal_node->value).value;
        const std::string lit_type = literal_node->type->to_string();
        if (lit_type == "u8") {
            const std::optional<uint8_t> lit_val = lit_int.to_uN<uint8_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an u8, maybe it's too big or negative?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), lit_val.value(), false)};
        } else if (lit_type == "u16") {
            const std::optional<uint16_t> lit_val = lit_int.to_uN<uint16_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an u16, maybe it's too big or negative?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{llvm::ConstantInt::get(llvm::Type::getInt16Ty(context), lit_val.value(), false)};
        } else if (lit_type == "u32") {
            const std::optional<uint32_t> lit_val = lit_int.to_uN<uint32_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an u32, maybe it's too big or negative?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), lit_val.value(), false)};
        } else if (lit_type == "u64") {
            const std::optional<uint64_t> lit_val = lit_int.to_uN<uint64_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an u64, maybe it's too big or negative?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), lit_val.value(), false)};
        } else if (lit_type == "i8") {
            const std::optional<int8_t> lit_val = lit_int.to_iN<int8_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an i8, maybe it's too big or too small?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), lit_val.value(), false)};
        } else if (lit_type == "i16") {
            const std::optional<int16_t> lit_val = lit_int.to_iN<int16_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an i16, maybe it's too big or too small?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{llvm::ConstantInt::get(llvm::Type::getInt16Ty(context), lit_val.value(), true)};
        } else if (lit_type == "i32") {
            const std::optional<int32_t> lit_val = lit_int.to_iN<int32_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an i32, maybe it's too big or too small?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), lit_val.value(), true)};
        } else if (lit_type == "i64") {
            const std::optional<int64_t> lit_val = lit_int.to_iN<int64_t>();
            if (!lit_val.has_value()) {
                // Could not convert the literal to an i64, maybe it's too big or too small?
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return std::vector<llvm::Value *>{
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), lit_val.value(), true) //
            };
        } else if (lit_type == "f32") {
            const APFloat lit_float = APFloat(lit_int);
            const double lit_val = lit_float.to_fN<double>();
            return std::vector<llvm::Value *>{llvm::ConstantFP::get(llvm::Type::getFloatTy(context), lit_val)};
        } else if (lit_type == "f64") {
            const APFloat lit_float = APFloat(lit_int);
            const double lit_val = lit_float.to_fN<double>();
            return std::vector<llvm::Value *>{llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), lit_val)};
        } else if (lit_type == "int") {
            // Compile-time type of literal which was not resolved to be a different type
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        } else {
            // Type that should not be possible
            UNREACHABLE();
            return std::nullopt;
        }
    }
    if (std::holds_alternative<LitFloat>(literal_node->value)) {
        const APFloat lit_float = std::get<LitFloat>(literal_node->value).value;
        const std::string lit_type = literal_node->type->to_string();
        if (lit_type == "f32") {
            const double lit_val = lit_float.to_fN<double>();
            return std::vector<llvm::Value *>{llvm::ConstantFP::get(llvm::Type::getFloatTy(context), lit_val)};
        } else if (lit_type == "f64") {
            const double lit_val = lit_float.to_fN<double>();
            return std::vector<llvm::Value *>{llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), lit_val)};
        } else if (lit_type == "u8" || lit_type == "u16" || lit_type == "u32" || lit_type == "u64" //
            || lit_type == "i8" || lit_type == "i16" || lit_type == "i32" || lit_type == "i64"     //
        ) {
            // This can only happen when the float literal is cast to a different type, so when writing 'i32(3.3)'
            // It is unclear whether this actually should be a compile-time error, since everything after the comma is stripped annyway
            // TODO: Figure out whether this should be a compile-time error (with a message like "write integer literals directly ffs") or
            // be valid Flint code
            const APInt lit_int = lit_float.to_apint();
            LitValue lit_value = LitInt{.value = lit_int};
            const ASTNode::PosTriple &lit_pos = {literal_node->line, literal_node->column, literal_node->length};
            const std::unique_ptr<LiteralNode> tmp_lit_node = std::make_unique<LiteralNode>( //
                literal_node->file_hash, lit_pos, lit_value, literal_node->type, true        //
            );
            return generate_literal(builder, ctx, garbage, expr_depth, tmp_lit_node.get());
        } else if (lit_type == "float") {
            // Compile-time type of literal which was not resolved to be a different type
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        } else {
            // Type that should not be possible
            UNREACHABLE();
            return std::nullopt;
        }
    }
    if (std::holds_alternative<LitStr>(literal_node->value)) {
        // Get the constant string value
        const std::string &str = std::get<LitStr>(literal_node->value).value;
        return std::vector<llvm::Value *>{IR::generate_const_string(ctx.parent->getParent(), str)};
    }
    if (std::holds_alternative<LitBool>(literal_node->value)) {
        return std::vector<llvm::Value *>{builder.getInt1(std::get<LitBool>(literal_node->value).value)};
    }
    if (std::holds_alternative<LitU8>(literal_node->value)) {
        return std::vector<llvm::Value *>{builder.getInt8(std::get<LitU8>(literal_node->value).value)};
    }
    if (std::holds_alternative<LitPtr>(literal_node->value)) {
        return std::vector<llvm::Value *>{llvm::ConstantPointerNull::get(PTR_TY)};
    }
    if (std::holds_alternative<LitOptional>(literal_node->value)) {
        return std::vector<llvm::Value *>{builder.getInt1(false)};
    }
    if (std::holds_alternative<LitEnum>(literal_node->value)) {
        const LitEnum &lit_enum = std::get<LitEnum>(literal_node->value);
        const EnumType *enum_type = lit_enum.enum_type->as<EnumType>();
        const auto &enum_values = enum_type->enum_node->values;
        std::vector<llvm::Value *> values;
        for (const auto &value : lit_enum.values) {
            auto value_it = enum_values.begin();
            for (; value_it != enum_values.end(); ++value_it) {
                if (value_it->first == value) {
                    break;
                }
            }
            ASSERT(value_it != enum_values.end());
            values.emplace_back(builder.getInt32(value_it->second));
        }
        return values;
    }
    if (std::holds_alternative<LitError>(literal_node->value)) {
        const LitError &lit_error = std::get<LitError>(literal_node->value);
        const ErrorSetType *error_type = lit_error.error_type->as<ErrorSetType>();
        const auto err_value_msg_pair = error_type->error_node->get_id_msg_pair_of_value(lit_error.value);
        if (!err_value_msg_pair.has_value()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        const unsigned int err_id = error_type->error_node->error_id;
        const unsigned int err_value = err_value_msg_pair.value().first;
        const std::string default_err_message = err_value_msg_pair.value().second;

        llvm::StructType *err_type = type_map.at("type.flint.err");
        llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
        llvm::Function *create_str_fn = Module::String::string_manip_functions.at("create_str");
        llvm::Value *err_struct = IR::get_default_value_of_type(err_type);
        err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_id), {0}, "insert_err_type_id");
        err_struct = builder.CreateInsertValue(err_struct, builder.getInt32(err_value), {1}, "insert_err_value");
        llvm::Value *error_message = nullptr;
        if (lit_error.message.has_value()) {
            auto msg_expr = generate_expression(builder, ctx, garbage, expr_depth, lit_error.message.value().get());
            if (garbage.find(expr_depth) != garbage.end()) {
                // Don't free the string from the error here
                garbage.at(expr_depth).clear();
            }
            if (!msg_expr.has_value()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            error_message = msg_expr.value().front();
        } else if (default_err_message.empty()) {
            error_message = builder.CreateCall(create_str_fn, {builder.getInt64(0)}, "empty_err_message");
        } else {
            llvm::Value *message_str = IR::generate_const_string(ctx.parent->getParent(), default_err_message);
            error_message = builder.CreateCall(init_str_fn, {message_str, builder.getInt64(default_err_message.size())}, "err_message");
        }
        err_struct = builder.CreateInsertValue(err_struct, error_message, {2}, "insert_err_message");
        return std::vector<llvm::Value *>{err_struct};
    }
    if (std::holds_alternative<LitVariantTag>(literal_node->value)) {
        const LitVariantTag &variant_tag = std::get<LitVariantTag>(literal_node->value);
        const VariantType *variant_type = variant_tag.variant_type->as<VariantType>();
        const std::optional<unsigned char> id = variant_type->get_idx_of_tag(variant_tag.variation_tag);
        ASSERT(id.has_value());
        return std::vector<llvm::Value *>{builder.getInt8(id.value())};
    }
    if (std::holds_alternative<LitVariant>(literal_node->value)) {
        const LitVariant &variant = std::get<LitVariant>(literal_node->value);
        const VariantType *variant_type = variant.variant_type->as<VariantType>();
        llvm::StructType *variant_ty = IR::add_and_or_get_type(ctx.parent->getParent(), variant.variant_type, false);
        const auto tag_index = variant_type->get_idx_of_tag(variant.variation_tag);
        if (!tag_index.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *tag_ptr = builder.CreateStructGEP(variant_ty, scratchspace, 0, "tag_ptr");
        llvm::Value *tag_idx = builder.getInt8(tag_index.value());
        IR::aligned_store(builder, tag_idx, tag_ptr);
        if (variant.expr.has_value()) {
            auto expr = generate_expression(builder, ctx, garbage, expr_depth + 1, variant.expr.value().get());
            if (!expr.has_value()) {
                return std::nullopt;
            }
            ASSERT(expr.value().size() == 1);
            llvm::Value *value_ptr = builder.CreateStructGEP(variant_ty, scratchspace, 1, "value_ptr");
            IR::aligned_store(builder, expr.value().front(), value_ptr);
        }
        if (variant.variant_type->is_freeable()) {
            return std::vector<llvm::Value *>{scratchspace};
        } else {
            llvm::Value *result = IR::aligned_load(builder, variant_ty, scratchspace, "result");
            return std::vector<llvm::Value *>{result};
        }
    }
    UNREACHABLE();
    return std::nullopt;
}

llvm::Value *Generator::Expression::generate_variable( //
    llvm::IRBuilder<> &builder,                        //
    GenerationContext &ctx,                            //
    const VariableNode *variable_node,                 //
    const bool is_reference                            //
) {
    if (variable_node == nullptr) {
        // Error: Null Node
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }

    // Because every variable, no matter if it's a fn parameter or a local variable, now is stored in the TS, the approach can be unified a
    // lot
    if (ctx.scope->variables.find(variable_node->name) == ctx.scope->variables.end()) {
        // Error: Undeclared Variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }
    const unsigned int variable_decl_scope = ctx.scope->variables.at(variable_node->name).scope_id;
    llvm::Value *const variable = ctx.allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);

    // If a reference is requested we return the variable allocation directly
    if (is_reference) {
        return variable;
    }

    // Get the type that the pointer points to
    auto type = IR::get_type(ctx.parent->getParent(), variable_node->type);

    // Check if the variable is complex, in that case we need to load a pointer to the value, not the value itself (the type of the load
    // inst differs)
    if (type.second.first) {
        llvm::LoadInst *load = IR::aligned_load(builder, PTR_TY, variable, variable_node->name + "_ptr");
        load->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Load ptr to '" + variable_node->name + "'")));
        return load;
    }

    // Load the variable's value from the allocation or the pointer to the memory it's located at
    llvm::LoadInst *load = IR::aligned_load(builder, type.first, variable, variable_node->name + "_val");
    load->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Load val of var '" + variable_node->name + "'")));
    return load;
}

llvm::Value *Generator::Expression::generate_string_interpolation( //
    llvm::IRBuilder<> &builder,                                    //
    GenerationContext &ctx,                                        //
    garbage_type &garbage,                                         //
    const unsigned int expr_depth,                                 //
    const StringInterpolationNode *interpol_node                   //
) {
    ASSERT(!interpol_node->string_content.empty());
    // The string interpolation works by adding all strings from the most left up to the most right one
    // First, create the string variable
    auto it = interpol_node->string_content.begin();
    llvm::Value *str_value = nullptr;
    if (std::holds_alternative<std::unique_ptr<LiteralNode>>(*it)) {
        llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
        const std::string lit_string = std::get<LitStr>(std::get<std::unique_ptr<LiteralNode>>(*it)->value).value;
        llvm::Value *lit_str = IR::generate_const_string(ctx.parent->getParent(), lit_string);
        str_value = builder.CreateCall(init_str_fn, {lit_str, builder.getInt64(lit_string.length())}, "init_str_value");
    } else {
        // Currently only the first output of a group is supported in string interpolation, as there currently is no group printing yet
        ExpressionNode *expr = std::get<std::unique_ptr<ExpressionNode>>(*it).get();
        ASSERT(expr->type->to_string() == "str");
        group_mapping res = generate_expression(builder, ctx, garbage, expr_depth, expr);
        if (!res.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        ASSERT(res.value().size() == 1);
        str_value = res.value().front();
        // Skip collecting the garbage of the string interpolation if the only content of it is a string variable, this would lead to a
        // double free bug
        if (std::next(it) == interpol_node->string_content.end() && expr->get_variation() == ExpressionNode::Variation::VARIABLE) {
            return str_value;
        }
    }
    ++it;

    llvm::Function *add_str_lit = Module::String::string_manip_functions.at("add_str_lit");
    llvm::Function *add_str_str = Module::String::string_manip_functions.at("add_str_str");
    for (; it != interpol_node->string_content.end(); ++it) {
        if (std::holds_alternative<std::unique_ptr<LiteralNode>>(*it)) {
            const std::string lit_string = std::get<LitStr>(std::get<std::unique_ptr<LiteralNode>>(*it)->value).value;
            llvm::Value *lit_str = IR::generate_const_string(ctx.parent->getParent(), lit_string);
            str_value = builder.CreateCall(add_str_lit, {str_value, lit_str, builder.getInt64(lit_string.length())});
        } else {
            ExpressionNode *expr = std::get<std::unique_ptr<ExpressionNode>>(*it).get();
            ASSERT(expr->type->to_string() == "str");
            group_mapping res = generate_expression(builder, ctx, garbage, expr_depth, expr);
            if (!res.has_value()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
            str_value = builder.CreateCall(add_str_str, {str_value, res.value().at(0)});
        }
    }
    if (garbage.count(expr_depth) == 0) {
        garbage[expr_depth].emplace_back(Type::get_primitive_type("str"), str_value);
    } else {
        garbage.at(expr_depth).emplace_back(Type::get_primitive_type("str"), str_value);
    }
    return str_value;
}

void Generator::Expression::convert_type_to_ext( //
    llvm::IRBuilder<> &builder,                  //
    GenerationContext &ctx,                      //
    const std::shared_ptr<Type> &type,           //
    llvm::Value *const value,                    //
    std::vector<llvm::Value *> &args             //
) {
    switch (type->get_variation()) {
        default:
            // No special handling needed
            args.emplace_back(value);
            return;
        case Type::Variation::TUPLE:
            [[fallthrough]];
        case Type::Variation::GROUP:
            convert_data_type_to_ext(builder, ctx, type, value, args);
            return;
        case Type::Variation::DATA: {
            llvm::Value *const data_ptr = IR::aligned_load(builder, PTR_TY, value, "loaded_data");
            convert_data_type_to_ext(builder, ctx, type, data_ptr, args);
            return;
        }
        case Type::Variation::VECTOR: {
            const auto *vector_type = type->as<VectorType>();
            // Vector types need to be passed as structs to extern functions. But the structs themselves need to be passed in 8 byte chunks
            // to the functions too. This means that the vector-type needs to be converted not into a struct but into a multiple of
            // two-component vector types.
            // A vector type of size 2 can be passed to the function directly and does not need to be converted at all
            // A vector type of size 3 is split into a two-component vector type + a scalar value
            // All bigger vector types N / 2 vector tuples (`<T x N> -> <T x 2> x (N / 2)`)
            const std::string base_type_str = vector_type->base_type->to_string();
            if (base_type_str == "f64" || base_type_str == "u64" || base_type_str == "i64") {
                for (size_t i = 0; i < vector_type->width; i++) {
                    args.emplace_back(builder.CreateExtractElement(value, builder.getInt64(i)));
                }
                return;
            } else if (vector_type->width == 2) {
                if (base_type_str == "u8" || base_type_str == "i8") {
                    // We need to pack the two u8 values as one i16
                    args.emplace_back(builder.CreateBitCast(value, builder.getInt16Ty()));
                } else if (base_type_str == "u16" || base_type_str == "i16") {
                    // We need to pack the two i16s as one i32
                    args.emplace_back(builder.CreateBitCast(value, builder.getInt32Ty()));
                } else if (base_type_str == "u32" || base_type_str == "i32") {
                    // We need to pack the two i32s as one i64
                    args.emplace_back(builder.CreateBitCast(value, builder.getInt64Ty()));
                } else {
                    args.emplace_back(value);
                }
                return;
            } else if (vector_type->width == 3) {
                if (base_type_str == "u8" || base_type_str == "i8") {
                    // We can simply cast the u8x3 to a i24 value
                    args.emplace_back(builder.CreateBitCast(value, builder.getIntNTy(24)));
                    return;
                } else if (base_type_str == "u16" || base_type_str == "i16") {
                    // We can simply cast the (u/i)16x3 to a i48 value
                    args.emplace_back(builder.CreateBitCast(value, builder.getIntNTy(48)));
                    return;
                }
                // Extract the first two elements as a vector type
                llvm::Value *first_two = builder.CreateShuffleVector(value, {0, 1});
                if (base_type_str == "u32" || base_type_str == "i32") {
                    // Pack the two i32s as one i64
                    args.emplace_back(builder.CreateBitCast(first_two, builder.getInt64Ty()));
                } else {
                    args.emplace_back(first_two);
                }
                // Extract the third value as a scalar element
                llvm::Value *third = builder.CreateExtractElement(value, builder.getInt64(2));
                args.emplace_back(third);
                return;
            }
            if (base_type_str == "u8" || base_type_str == "i8") {
                // Since all following vector-types are a multiple of 4, e.g. u8x4 or u8x8, we can just bitcast them to i64 and add them to
                // the argument list
                if (vector_type->width == 4) {
                    args.emplace_back(builder.CreateBitCast(value, builder.getInt32Ty()));
                } else if (vector_type->width == 8) {
                    args.emplace_back(builder.CreateBitCast(value, builder.getInt64Ty()));
                } else {
                    UNREACHABLE();
                }
                return;
            } else if (base_type_str == "u16" || base_type_str == "i16") {
                if (vector_type->width == 4) {
                    args.emplace_back(builder.CreateBitCast(value, builder.getInt64Ty()));
                } else if (vector_type->width == 8) {
                    for (int32_t i = 0; i < static_cast<int32_t>(vector_type->width); i += 4) {
                        llvm::Value *next_shuffle = builder.CreateShuffleVector(value, {i, i + 1, i + 2, i + 3});
                        args.emplace_back(builder.CreateBitCast(next_shuffle, builder.getInt64Ty()));
                    }
                } else {
                    UNREACHABLE();
                }
                return;
            }

            // Bigger than size 3. But there are only 2, 3, 4, 8, 16, ... vector-types in Flint, so we know all bigger than 3 are even
            // numbers
            for (int32_t i = 0; i < static_cast<int32_t>(vector_type->width); i += 2) {
                llvm::Value *next_shuffle = builder.CreateShuffleVector(value, {i, i + 1});
                if (base_type_str == "u32" || base_type_str == "i32") {
                    args.emplace_back(builder.CreateBitCast(next_shuffle, builder.getInt64Ty()));
                } else {
                    args.emplace_back(next_shuffle);
                }
            }
            return;
        }
        case Type::Variation::PRIMITIVE:
            if (type->to_string() == "str") {
                llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
                args.emplace_back(builder.CreateStructGEP(str_type, value, 1, "char_ptr"));
                return;
            }
            args.emplace_back(value);
            break;
    }
}

/// @function `expand_type_with_path`
/// @brief Recursively expands array types into scalar elements, recording the full GEP path for each.
///        Struct fields like `[3 x float]` produce 3 scalar entries, each with a GEP path
///        (e.g. `{0, 0, 0}`, `{0, 0, 1}`, `{0, 0, 2}`) so the emission phase can issue correct
///        `getelementptr` into the array rather than treating each as a separate struct field.
///
/// @param `t` The LLVM type to expand (may be an array or scalar).
/// @param `path` The GEP indices accumulated from outer arrays, including the leading `0` for struct dereference and ending with the
///               current struct field index.
/// @param `elem_types` Output: flat list of scalar element types.
/// @param `elem_gep_path` Output: `elem_gep_path[i]` is the complete GEP index list for `elem_types[i]`, usable directly with `CreateGEP`.
static void expand_type_with_path(                  //
    llvm::Type *const t,                            //
    std::vector<size_t> path,                       //
    std::vector<llvm::Type *> &elem_types,          //
    std::vector<std::vector<size_t>> &elem_gep_path //
) {
    if (llvm::ArrayType *const arr_ty = llvm::dyn_cast<llvm::ArrayType>(t)) {
        for (size_t j = 0; j < arr_ty->getNumElements(); j++) {
            auto child_path = path;
            child_path.push_back(j);
            expand_type_with_path(arr_ty->getElementType(), std::move(child_path), elem_types, elem_gep_path);
        }
    } else {
        elem_types.push_back(t);
        elem_gep_path.push_back(std::move(path));
    }
}

/// @function `create_expanded_gep`
/// @brief Emits a `getelementptr` using the pre-computed GEP path for an expanded array element.
///        This replaces a `CreateStructGEP` call when the element index refers to an array field
///        that was expanded by `expand_type_with_path`. For a `{ [3 x float] }` struct, elem 1
///        (which is `float` in the expanded view) maps to the GEP path `{0, 0, 1}` — field 0 of
///        the struct, element 1 of the array.
///
/// @param `builder` The LLVM IR Builder.
/// @param `struct_type` The LLVM struct type (e.g. `{ [3 x float] }`).
/// @param `ptr` Pointer to the struct value.
/// @param `elem_gep_path` The mapping built by `expand_type_with_path`.
/// @param `idx` Index into `elem_gep_path` (the expanded element index).
/// @return `llvm::Value *` Pointer to the scalar element at the expanded index.
static llvm::Value *create_expanded_gep(                   //
    llvm::IRBuilder<> &builder,                            //
    llvm::Type *struct_type,                               //
    llvm::Value *ptr,                                      //
    const std::vector<std::vector<size_t>> &elem_gep_path, //
    size_t idx                                             //
) {
    std::vector<llvm::Value *> indices;
    indices.reserve(elem_gep_path[idx].size());
    for (size_t i : elem_gep_path[idx]) {
        indices.push_back(builder.getInt32(i));
    }
    return builder.CreateGEP(struct_type, ptr, indices);
}

void Generator::Expression::convert_data_type_to_ext( //
    llvm::IRBuilder<> &builder,                       //
    GenerationContext &ctx,                           //
    const std::shared_ptr<Type> &type,                //
    llvm::Value *const value,                         //
    std::vector<llvm::Value *> &args                  //
) {
    // get the LLVM struct type and its elements
    llvm::Type *_struct_type = IR::get_type(ctx.parent->getParent(), type, false).first;
    ASSERT(_struct_type->isStructTy());
    size_t struct_size = Allocation::get_type_size(ctx.parent->getParent(), _struct_type);
    if (struct_size > 16) {
        // For > 16 bytes, pass pointer directly
        // The 'value' parameter should already be a pointer to the struct
        args.emplace_back(value);
        return;
    }
    llvm::BasicBlock *current_block = builder.GetInsertBlock();
    llvm::BasicBlock *convert_type_to_ext_block = llvm::BasicBlock::Create(context, "convert_type_to_ext", ctx.parent);
    llvm::BasicBlock *convert_type_to_ext_merge_block = llvm::BasicBlock::Create(context, "convert_type_to_ext_merge", ctx.parent);
    builder.SetInsertPoint(current_block);
    builder.CreateBr(convert_type_to_ext_block);
    builder.SetInsertPoint(convert_type_to_ext_block);

    // We implement it as a two-phase approach. In the first phase we go through all elements and track indexing and needed padding etc
    // and put them onto a stack with the count of total elements in the stack and each element in the stack represents the offset of
    // the element within the output 8-byte pack, so we create two stacks because for data greater than 16 bytes the 16-byte-rule
    // applies
    // Padding is handled by just different offsets of the struct elements, the total size of the first stack is also tracked for
    // sub-64-byte packed results like packing 5 u8 values into one i40.
    llvm::StructType *const struct_type = llvm::cast<llvm::StructType>(_struct_type);
    std::vector<llvm::Type *> elem_types;
    std::vector<std::vector<size_t>> elem_gep_path;
    size_t field_idx = 0;
    for (llvm::Type *const elem : struct_type->elements()) {
        expand_type_with_path(elem, {0u, field_idx}, elem_types, elem_gep_path);
        field_idx++;
    }

    std::array<std::stack<unsigned int>, 2> stacks;
    unsigned int offset = 0;
    unsigned int first_size = 0;

    size_t elem_idx = 0;
    for (; elem_idx < elem_types.size(); elem_idx++) {
        size_t elem_size = Allocation::get_type_size(ctx.parent->getParent(), elem_types.at(elem_idx));
        // We can only pack the element into the stack if there is enough space left
        if (8 - offset < elem_size) {
            break;
        }
        // If the current offset is 0 we can simply put in the element into the stack without further checks
        if (offset == 0) {
            stacks[0].push(0);
            offset += elem_size;
            first_size += elem_size;
            continue;
        }
        // Now we simply need to check where in the 8 byte structure we need to put the elements in. We can do this by calculating the
        // padding based on the type alignment, the alignment is just the elem_size in our case because we work with types <= 8 bytes in
        // size.
        // If the offset is divisible by the element size it does not need to be changed. If it is not divisible we need to clamp it to
        // the next divisible offset value, so if current offset is 2 but element size is 4 the offset needs to be clamped to 4
        uint8_t elem_offset = offset;
        if (offset % elem_size != 0) {
            elem_offset = ((elem_size + offset) / elem_size) * elem_size;
        }
        if (elem_offset == 8) {
            // This element does not fit into this stack, we need to put it into the next stack
            break;
        }
        stacks[0].push(elem_offset);
        offset = elem_offset + elem_size;
        first_size = elem_offset + elem_size;
    }
    offset = 0;
    for (; elem_idx < elem_types.size(); elem_idx++) {
        size_t elem_size = Allocation::get_type_size(ctx.parent->getParent(), elem_types.at(elem_idx));
        // For the second stack we actually can assert that enough space is left in here, since otherwise the 16-byte rule should have
        // applied
        ASSERT(8 - offset >= elem_size);
        // If the current offset is 0 we can simply put in the element into the stack without further checks
        if (offset == 0) {
            stacks[1].push(0);
            offset += elem_size;
            continue;
        }
        uint8_t elem_offset = offset;
        if (offset % elem_size != 0) {
            elem_offset = ((elem_size + offset) / elem_size) * elem_size;
        }
        // This element does not fit into this stack, this should not happen since it's the last stack
        ASSERT(elem_offset != 8);
        stacks[1].push(elem_offset);
        offset = elem_offset + elem_size;
    }
    ASSERT(elem_idx == elem_types.size());
    elem_idx = 0;

    // Now we reach the second phase, we have figured out where to put the elements in the respective chunks
    if (stacks[1].empty()) {
        // Special case for when the number of elements in the first stack is equal to the size of the first structure. This means that
        // we pack multiple u8 values into one, for them we can get even i40, i48 or i56 results
        if (stacks[0].size() == first_size) {
            llvm::Value *result = builder.getIntN(first_size * 8, 0);
            for (; elem_idx < elem_types.size(); elem_idx++) {
                llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, elem_idx);
                llvm::Value *elem = IR::aligned_load(builder, builder.getInt8Ty(), elem_ptr);
                // We now need to store the `elem` byte at the `elem_idx`th byte in the `result` integer. I think we can achieve this by
                // just extending the single byte to the integer type, then bit shifting the whole number for elem_idx elements and then
                // applying a bitwise or between the number to insert and the result itself
                // The elements are stored in the number from right to left. When we store the struct elements { u8(0), u8(1), u8(2) }
                // inside a `i24` for example we need to ensure the element 0 comes at the very right of the integer, so the first 8
                // bits. The middle is unambiguous and the second index needs to be located at the last 8 bytes. This means that the
                // amount we need to shift each value is exactly the position of the element in the struct. Element 0 does not need to
                // be shifted at all, element 1 by 1 and element 2 needs to be shifted by 2.
                llvm::Value *elem_big = builder.CreateZExt(elem, builder.getIntNTy(first_size * 8));
                if (elem_idx == 0) {
                    result = elem_big;
                    continue;
                }
                // Shift left by the element size - element index to shift it to the correct position
                llvm::Value *elem_shift = builder.CreateShl(elem_big, elem_idx * 8);
                // Bitwise and with the result to form the new result contianing the shifted element in it
                result = builder.CreateOr(result, elem_shift);
            }
            args.emplace_back(result);
            builder.CreateBr(convert_type_to_ext_merge_block);
            builder.SetInsertPoint(convert_type_to_ext_merge_block);
            return;
        }
    }
    // Because we only need to fill two 8-byte containers we can resolve a few edge-cases upfront
    elem_idx = 0;
    const size_t stacks_0_size = stacks[0].size();
    if (stacks[0].size() == 1) {
        llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, elem_idx);
        llvm::Value *elem = IR::aligned_load(builder, elem_types.at(elem_idx), elem_ptr);
        args.emplace_back(elem);
        elem_idx++;
    } else if (stacks[0].size() == 2) {
        if (elem_types.front()->isFloatTy() && elem_types.at(1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Type *vec2_type = llvm::VectorType::get(elem_types.at(elem_idx), 2, false);
            llvm::Value *result = IR::get_default_value_of_type(vec2_type);
            llvm::Value *elem_1_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, elem_idx);
            llvm::Value *elem_1 = IR::aligned_load(builder, elem_types.at(elem_idx), elem_1_ptr);
            result = builder.CreateInsertElement(result, elem_1, static_cast<size_t>(0));
            elem_idx++;
            llvm::Value *elem_2_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, elem_idx);
            llvm::Value *elem_2 = IR::aligned_load(builder, elem_types.at(elem_idx), elem_2_ptr);
            result = builder.CreateInsertElement(result, elem_2, static_cast<size_t>(1));
            args.emplace_back(result);
            elem_idx++;
        } else {
            goto stack_0_generic;
        }
    } else {
    stack_0_generic:
        llvm::Value *result = builder.getInt64(0);
        for (; elem_idx < stacks_0_size; elem_idx++) {
            const size_t actual_elem_idx = stacks_0_size - elem_idx - 1;
            llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, actual_elem_idx);
            llvm::Value *elem = IR::aligned_load(builder, elem_types.at(actual_elem_idx), elem_ptr);
            if (elem->getType()->isFloatTy()) {
                elem = builder.CreateBitCast(elem, builder.getInt32Ty());
            }
            llvm::Value *elem_big = builder.CreateZExt(elem, builder.getInt64Ty());
            if (actual_elem_idx == stacks_0_size - 1) {
                // Shift left by the amount in the stacks
                elem_big = builder.CreateShl(elem_big, stacks[0].top() * 8);
            }
            // Bitwise or with the result to form the new result contianing the shifted element in it
            result = builder.CreateOr(result, elem_big);
            stacks[0].pop();
        }
        result->setName("stack_0_result");
        args.emplace_back(result);
        ASSERT(stacks[0].empty());
    }
    if (stacks[1].size() == 1) {
        llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, elem_idx);
        llvm::Value *elem = IR::aligned_load(builder, elem_types.at(elem_idx), elem_ptr);
        args.emplace_back(elem);
        elem_idx++;
    } else if (stacks[1].size() == 2) {
        if (elem_types.at(stacks_0_size)->isFloatTy() && elem_types.at(stacks_0_size + 1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Type *vec2_type = llvm::VectorType::get(elem_types.at(elem_idx), 2, false);
            llvm::Value *result = IR::get_default_value_of_type(vec2_type);
            llvm::Value *elem_1_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, elem_idx);
            llvm::Value *elem_1 = IR::aligned_load(builder, elem_types.at(elem_idx), elem_1_ptr);
            result = builder.CreateInsertElement(result, elem_1, static_cast<size_t>(0));
            elem_idx++;
            llvm::Value *elem_2_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, elem_idx);
            llvm::Value *elem_2 = IR::aligned_load(builder, elem_types.at(elem_idx), elem_2_ptr);
            result = builder.CreateInsertElement(result, elem_2, static_cast<size_t>(1));
            args.emplace_back(result);
            elem_idx++;
        } else {
            goto stack_1_generic;
        }
    } else {
    stack_1_generic:
        llvm::Value *result = builder.getInt64(0);
        const size_t stack_size = stacks[1].size();
        for (; elem_idx < stacks_0_size + stack_size; elem_idx++) {
            const size_t actual_elem_idx = stacks_0_size * 2 + stack_size - elem_idx - 1;
            llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, value, elem_gep_path, actual_elem_idx);
            llvm::Value *elem = IR::aligned_load(builder, elem_types.at(actual_elem_idx), elem_ptr);
            if (elem->getType()->isFloatTy()) {
                elem = builder.CreateBitCast(elem, builder.getInt32Ty());
            }
            llvm::Value *elem_big = builder.CreateZExt(elem, builder.getInt64Ty());
            if (actual_elem_idx != stacks_0_size - 1) {
                // Shift left by the amount in the stacks
                elem_big = builder.CreateShl(elem_big, stacks[1].top() * 8);
            }
            // Bitwise or with the result to form the new result contianing the shifted element in it
            result = builder.CreateOr(result, elem_big);
            stacks[1].pop();
        }
        result->setName("stack_1_result");
        args.emplace_back(result);
        ASSERT(stacks[1].empty());
    }
    ASSERT(elem_idx == elem_types.size());
    builder.CreateBr(convert_type_to_ext_merge_block);
    builder.SetInsertPoint(convert_type_to_ext_merge_block);
    return;
}

void Generator::Expression::convert_type_from_ext( //
    llvm::IRBuilder<> &builder,                    //
    GenerationContext &ctx,                        //
    const std::shared_ptr<Type> &type,             //
    llvm::Value *&value                            //
) {
    switch (type->get_variation()) {
        default:
            return;
        case Type::Variation::TUPLE:
            [[fallthrough]];
        case Type::Variation::GROUP:
            [[fallthrough]];
        case Type::Variation::DATA:
            convert_data_type_from_ext(builder, ctx, type, value);
            return;
        case Type::Variation::VECTOR: {
            const auto *vector_type = type->as<VectorType>();
            llvm::Type *element_type = IR::get_type(ctx.parent->getParent(), vector_type->base_type).first;
            llvm::VectorType *target_vector_type = llvm::VectorType::get(element_type, vector_type->width, false);
            llvm::VectorType *vec2_i32 = llvm::VectorType::get(builder.getInt32Ty(), 2, false);
            const std::string base_type_str = vector_type->base_type->to_string();
            if (base_type_str == "f64" || base_type_str == "u64" || base_type_str == "i64") {
                llvm::Value *result_vec = llvm::UndefValue::get(target_vector_type);
                for (size_t i = 0; i < vector_type->width; i++) {
                    llvm::Value *elem_i = builder.CreateExtractValue(value, i);
                    result_vec = builder.CreateInsertElement(result_vec, elem_i, builder.getInt64(i));
                }
                value = result_vec;
            } else if (vector_type->width == 2) {
                if (base_type_str == "u8" || base_type_str == "i8") {
                    // Value returned as `i16` value
                    value = builder.CreateBitCast(value, target_vector_type);
                } else if (base_type_str == "u16" || base_type_str == "i16") {
                    // Value returned as `i32` value
                    value = builder.CreateBitCast(value, target_vector_type);
                } else if (base_type_str == "u32" || base_type_str == "i32") {
                    value = builder.CreateBitCast(value, vec2_i32);
                } else {
                    // vec2 is returned as <2 x T> directly for floats - no conversion needed
                    return;
                }
            } else if (vector_type->width == 3) {
                if (base_type_str == "u8" || base_type_str == "i8") {
                    // Value returned as `i24` value
                    value = builder.CreateBitCast(value, target_vector_type);
                    return;
                } else if (base_type_str == "u16" || base_type_str == "i16") {
                    // Value returned as `i48` value
                    value = builder.CreateBitCast(value, target_vector_type);
                    return;
                }
                // vec3 is returned as { <2 x T>, T } struct from extern calls
                ASSERT(value->getType()->isStructTy());

                // Extract the <2 x T> part
                llvm::Value *vec2_part = builder.CreateExtractValue(value, 0, "vec2_part");
                llvm::Value *scalar_part = builder.CreateExtractValue(value, 1, "scalar_part");

                // Reconstruct <3 x T> vector
                llvm::Value *result_vec = llvm::UndefValue::get(target_vector_type);

                // The first element of the struct is `i64` not a vector so we need to cast it first
                if (base_type_str == "u32" || base_type_str == "i32") {
                    vec2_part = builder.CreateBitCast(vec2_part, vec2_i32);
                }

                // Extract elements from vec2 and insert into result
                llvm::Value *elem0 = builder.CreateExtractElement(vec2_part, builder.getInt64(0));
                llvm::Value *elem1 = builder.CreateExtractElement(vec2_part, builder.getInt64(1));

                result_vec = builder.CreateInsertElement(result_vec, elem0, builder.getInt64(0));
                result_vec = builder.CreateInsertElement(result_vec, elem1, builder.getInt64(1));
                result_vec = builder.CreateInsertElement(result_vec, scalar_part, builder.getInt64(2));

                value = result_vec;
            } else {
                if (base_type_str == "u8" || base_type_str == "i8") {
                    // Value returned as `i32` value (u8x4 / i8x4) or `i64` value (u8x8 / i8x8)
                    value = builder.CreateBitCast(value, target_vector_type);
                    return;
                } else if (base_type_str == "u16" || base_type_str == "i16") {
                    if (vector_type->width == 4) {
                        // Value returned as `i64` value directly
                        value = builder.CreateBitCast(value, target_vector_type);
                        return;
                    } else if (vector_type->width == 8) {
                        // Value returned as a struct of two i64 values
                        llvm::Value *lhs_i64 = builder.CreateExtractValue(value, 0, "chunk_vec_left4");
                        llvm::Value *rhs_i64 = builder.CreateExtractValue(value, 1, "chunk_vec_right4");
                        llvm::VectorType *vec4_i16 = llvm::VectorType::get(element_type, 4, false);
                        auto *left4 = builder.CreateBitCast(lhs_i64, vec4_i16);
                        auto *right4 = builder.CreateBitCast(rhs_i64, vec4_i16);

                        llvm::Value *result = llvm::UndefValue::get(target_vector_type);
                        result = builder.CreateInsertVector(target_vector_type, result, left4, builder.getInt64(0));
                        result = builder.CreateInsertVector(target_vector_type, result, right4, builder.getInt64(4));
                        value = result;
                    } else {
                        UNREACHABLE();
                    }
                    return;
                }
                // vecN (N > 3) is returned as { <2 x T>, <2 x T>, ... } struct from extern calls
                ASSERT(value->getType()->isStructTy());
                llvm::Value *result_vec = llvm::UndefValue::get(target_vector_type);
                size_t element_index = 0;

                // Extract each <2 x T> chunk and rebuild the original vector
                for (size_t chunk = 0; chunk < (vector_type->width + 1) / 2; chunk++) {
                    llvm::Value *chunk_vec = builder.CreateExtractValue(value, chunk, "chunk_vec");

                    // The first element of the struct is `i64` not a vector so we need to cast it first
                    if (base_type_str == "u32" || base_type_str == "i32") {
                        chunk_vec = builder.CreateBitCast(chunk_vec, vec2_i32);
                    }

                    // Extract elements from this chunk
                    result_vec = builder.CreateInsertVector(target_vector_type, result_vec, chunk_vec, builder.getInt64(element_index));
                    element_index += 2;
                }
                value = result_vec;
            }
            break;
        }
        case Type::Variation::PRIMITIVE:
            if (type->to_string() == "str") {
                llvm::Value *str_len = builder.CreateCall(c_functions.at(STRLEN), value, "str_len");
                value = builder.CreateCall(Module::String::string_manip_functions.at("init_str"), {value, str_len}, "str");
            }
            break;
    }
}

void Generator::Expression::convert_data_type_from_ext( //
    llvm::IRBuilder<> &builder,                         //
    GenerationContext &ctx,                             //
    const std::shared_ptr<Type> &type,                  //
    llvm::Value *&value                                 //
) {
    // get the LLVM struct type and its elements
    llvm::Type *_struct_type = IR::get_type(ctx.parent->getParent(), type, false).first;
    ASSERT(_struct_type->isStructTy());
    size_t struct_size = Allocation::get_type_size(ctx.parent->getParent(), _struct_type);
    if (struct_size > 16) {
        // For > 16 bytes, the value is already allocated by caller
        // 'value' should already be the pointer to the result
        return;
    }
    llvm::BasicBlock *current_block = builder.GetInsertBlock();
    llvm::BasicBlock *convert_type_from_ext_block = llvm::BasicBlock::Create(context, "convert_type_from_ext", ctx.parent);
    llvm::BasicBlock *convert_type_from_ext_merge_block = llvm::BasicBlock::Create(context, "convert_type_from_ext_merge", ctx.parent);
    builder.SetInsertPoint(current_block);
    builder.CreateBr(convert_type_from_ext_block);
    builder.SetInsertPoint(convert_type_from_ext_block);
    llvm::Value *result_ptr = nullptr;
    if (type->is_dima_managed()) {
        llvm::Function *dima_allocate_fn = Module::DIMA::dima_functions.at("allocate");
        result_ptr = builder.CreateCall(dima_allocate_fn, {Module::DIMA::get_head(type)}, "result_ptr");
    } else {
        // TODO: Potential memory leak here, it does not seem like tuples are freed anywhere, but maybe tuples are no problem
        llvm::Function *malloc_fn = c_functions.at(MALLOC);
        result_ptr = builder.CreateCall(malloc_fn, {builder.getInt64(struct_size)}, "result_ptr");
    }

    // We implement it as a two-phase approach. In the first phase we go through all elements and track indexing and needed padding etc
    // and put them onto a stack with the count of total elements in the stack and each element in the stack represents the offset of
    // the element within the output 8-byte pack, so we create two stacks because for data greater than 16 bytes the 16-byte-rule
    // applies
    // Padding is handled by just different offsets of the struct elements, the total size of the first stack is also tracked for
    // sub-64-byte packed results like packing 5 u8 values into one i40.
    llvm::StructType *struct_type = llvm::cast<llvm::StructType>(_struct_type);
    std::vector<llvm::Type *> elem_types;
    std::vector<std::vector<size_t>> elem_gep_path;
    size_t field_idx = 0;
    for (llvm::Type *const elem : struct_type->elements()) {
        expand_type_with_path(elem, {0u, field_idx}, elem_types, elem_gep_path);
        field_idx++;
    }
    std::array<std::stack<unsigned int>, 2> stacks;
    unsigned int offset = 0;
    unsigned int first_size = 0;

    size_t elem_idx = 0;
    for (; elem_idx < elem_types.size(); elem_idx++) {
        size_t elem_size = Allocation::get_type_size(ctx.parent->getParent(), elem_types.at(elem_idx));
        // We can only pack the element into the stack if there is enough space left
        if (8 - offset < elem_size) {
            break;
        }
        // If the current offset is 0 we can simply put in the element into the stack without further checks
        if (offset == 0) {
            stacks[0].push(0);
            offset += elem_size;
            first_size += elem_size;
            continue;
        }
        // Now we simply need to check where in the 8 byte structure we need to put the elements in. We can do this by calculating the
        // padding based on the type alignment, the alignment is just the elem_size in our case because we work with types <= 8 bytes in
        // size.
        // If the offset is divisible by the element size it does not need to be changed. If it is not divisible we need to clamp it to
        // the next divisible offset value, so if current offset is 2 but element size is 4 the offset needs to be clamped to 4
        uint8_t elem_offset = offset;
        if (offset % elem_size != 0) {
            elem_offset = ((elem_size + offset) / elem_size) * elem_size;
        }
        if (elem_offset == 8) {
            // This element does not fit into this stack, we need to put it into the next stack
            break;
        }
        stacks[0].push(elem_offset);
        offset = elem_offset + elem_size;
        first_size = elem_offset + elem_size;
    }
    offset = 0;
    for (; elem_idx < elem_types.size(); elem_idx++) {
        size_t elem_size = Allocation::get_type_size(ctx.parent->getParent(), elem_types.at(elem_idx));
        // For the second stack we actually can assert that enough space is left in here, since otherwise the 16-byte rule should have
        // applied
        ASSERT(8 - offset >= elem_size);
        // If the current offset is 0 we can simply put in the element into the stack without further checks
        if (offset == 0) {
            stacks[1].push(0);
            offset += elem_size;
            continue;
        }
        uint8_t elem_offset = offset;
        if (offset % elem_size != 0) {
            elem_offset = ((elem_size + offset) / elem_size) * elem_size;
        }
        // This element does not fit into this stack, this should not happen since it's the last stack
        ASSERT(elem_offset != 8);
        stacks[1].push(elem_offset);
        offset = elem_offset + elem_size;
    }
    ASSERT(elem_idx == elem_types.size());
    elem_idx = 0;

    // Now we reach the second phase, we have figured out where to put the elements in the respective chunks
    if (stacks[1].empty()) {
        // Special case for when the number of elements in the first stack is equal to the size of the first structure. This means that
        // we pack multiple u8 values into one, for them we can get even i40, i48 or i56 results
        if (stacks[0].size() == first_size) {
            for (; elem_idx < elem_types.size(); elem_idx++) {
                // Shift right by the element index to shift it to the beginning position
                llvm::Value *elem_shift = builder.CreateLShr(value, elem_idx * 8);
                // Now we truncate the result to get the single element which we can store in the output structure
                llvm::Value *elem = builder.CreateTrunc(elem_shift, builder.getInt8Ty());
                llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, elem_idx);
                IR::aligned_store(builder, elem, elem_ptr);
            }
            value = result_ptr;
            builder.CreateBr(convert_type_from_ext_merge_block);
            builder.SetInsertPoint(convert_type_from_ext_merge_block);
            return;
        }
    }
    // Because we only need to fill two 8-byte containers we can resolve a few edge-cases upfront
    elem_idx = 0;
    const size_t stacks_0_size = stacks[0].size();
    if (stacks[0].size() == 1) {
        llvm::Value *elem = builder.CreateExtractValue(value, 0);
        llvm::Value *res_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, elem_idx);
        IR::aligned_store(builder, elem, res_ptr);
        elem_idx++;
    } else if (stacks[0].size() == 2) {
        if (elem_types.front()->isFloatTy() && elem_types.at(1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Value *vec_val = builder.CreateExtractValue(value, 0);

            llvm::Value *elem_1 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_1_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, elem_idx);
            IR::aligned_store(builder, elem_1, elem_1_ptr);
            elem_idx++;

            llvm::Value *elem_2 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_2_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, elem_idx);
            IR::aligned_store(builder, elem_2, elem_2_ptr);
            elem_idx++;
        } else {
            goto stack_0_generic;
        }
    } else {
    stack_0_generic:
        llvm::Value *res = builder.CreateExtractValue(value, 0);
        for (; elem_idx < stacks_0_size; elem_idx++) {
            const size_t actual_elem_idx = stacks_0_size - elem_idx - 1;
            // Shift right by the amount in the stack
            llvm::Value *elem_big = builder.CreateLShr(res, stacks[0].top() * 8);
            llvm::Value *elem_smol = nullptr;
            if (elem_types.at(actual_elem_idx)->isFloatTy()) {
                elem_smol = builder.CreateTrunc(elem_big, builder.getInt32Ty());
                elem_smol = builder.CreateBitCast(elem_smol, builder.getFloatTy());
            } else {
                elem_smol = builder.CreateTrunc(elem_big, elem_types.at(actual_elem_idx));
            }
            // Bitwise or with the result to form the new result contianing the shifted element in it
            llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, actual_elem_idx);
            IR::aligned_store(builder, elem_smol, elem_ptr);
            stacks[0].pop();
        }
        ASSERT(stacks[0].empty());
    }
    if (stacks[1].size() == 1) {
        llvm::Value *elem = builder.CreateExtractValue(value, 1);
        llvm::Value *res_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, elem_idx);
        IR::aligned_store(builder, elem, res_ptr);
        elem_idx++;
    } else if (stacks[1].size() == 2) {
        if (elem_types.at(stacks_0_size)->isFloatTy() && elem_types.at(stacks_0_size + 1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Value *vec_val = builder.CreateExtractValue(value, 1);

            llvm::Value *elem_1 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_1_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, elem_idx);
            IR::aligned_store(builder, elem_1, elem_1_ptr);
            elem_idx++;

            llvm::Value *elem_2 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_2_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, elem_idx);
            IR::aligned_store(builder, elem_2, elem_2_ptr);
            elem_idx++;
        } else {
            goto stack_1_generic;
        }
    } else {
    stack_1_generic:
        llvm::Value *res = builder.CreateExtractValue(value, 1);
        const size_t stack_size = stacks[1].size();
        for (; elem_idx < stacks_0_size + stack_size; elem_idx++) {
            const size_t actual_elem_idx = stacks_0_size * 2 + stack_size - elem_idx - 1;
            // Shift right by the amount in the stack
            llvm::Value *elem_big = builder.CreateLShr(res, stacks[1].top() * 8);
            llvm::Value *elem_smol = nullptr;
            if (elem_types.at(actual_elem_idx)->isFloatTy()) {
                elem_smol = builder.CreateTrunc(elem_big, builder.getInt32Ty());
                elem_smol = builder.CreateBitCast(elem_smol, builder.getFloatTy());
            } else {
                elem_smol = builder.CreateTrunc(elem_big, elem_types.at(actual_elem_idx));
            }
            // Bitwise or with the result to form the new result contianing the shifted element in it
            llvm::Value *elem_ptr = create_expanded_gep(builder, _struct_type, result_ptr, elem_gep_path, actual_elem_idx);
            IR::aligned_store(builder, elem_smol, elem_ptr);
            stacks[1].pop();
        }
        ASSERT(stacks[1].empty());
    }
    ASSERT(elem_idx == elem_types.size());
    value = result_ptr;
    builder.CreateBr(convert_type_from_ext_merge_block);
    builder.SetInsertPoint(convert_type_from_ext_merge_block);
    return;
}

Generator::group_mapping Generator::Expression::generate_extern_call( //
    llvm::IRBuilder<> &builder,                                       //
    GenerationContext &ctx,                                           //
    const CallNodeBase *call_node,                                    //
    std::vector<llvm::Value *> &args                                  //
) {
    // Check if return type > 16 bytes, if it is then we need to pass a reference to the allocation of the sret value to the function
    bool needs_sret = false;
    llvm::Value *sret_alloc = nullptr;
    size_t return_size = 0;
    if (call_node->type->to_string() != "void") {
        llvm::Type *return_type = IR::get_type(ctx.parent->getParent(), call_node->type, false).first;
        return_size = Allocation::get_type_size(ctx.parent->getParent(), return_type);

        if (return_size > 16) {
            needs_sret = true;
            // Get the pre-allocated sret space
            const std::string sret_alloca_name = "flint.sret_" + call_node->type->to_string();
            sret_alloc = ctx.allocations.at(sret_alloca_name);
        }
    }

    // Convert all argument types to types for external usage
    std::vector<llvm::Value *> converted_args;
    // If needs_sret, first argument is the sret pointer
    if (needs_sret) {
        converted_args.emplace_back(sret_alloc);
    }
    for (size_t i = 0; i < call_node->arguments.size(); i++) {
        const auto &arg = call_node->arguments[i];
        convert_type_to_ext(builder, ctx, arg.first->type, args[i], converted_args);
    }
    auto result = Function::get_function_definition(ctx.parent, call_node);
    if (call_node->type->to_string() == "void") {
        llvm::CallInst *call = builder.CreateCall(result.first.value(), converted_args);
        // Add byval attributes for > 16 byte input parameters
        for (size_t i = 0; i < call_node->arguments.size(); i++) {
            llvm::Type *arg_type = IR::get_type(ctx.parent->getParent(), call_node->arguments[i].first->type, false).first;
            size_t arg_size = Allocation::get_type_size(ctx.parent->getParent(), arg_type);
            if (arg_size > 16) {
                call->addParamAttr(i, llvm::Attribute::get(context, llvm::Attribute::ByVal, arg_type));
            }
        }
        call->setMetadata("comment",
            llvm::MDNode::get(context, llvm::MDString::get(context, "Call to extern function '" + call_node->function->name + "'")));
        return std::vector<llvm::Value *>{};
    }

    if (needs_sret) {
        // Create call with sret, the return type of the call should be void
        llvm::CallInst *call = builder.CreateCall(result.first.value(), converted_args);
        call->setMetadata("comment",
            llvm::MDNode::get(context, llvm::MDString::get(context, "Call to extern function '" + call_node->function->name + "' (sret)")));

        // Add sret attribute to first parameter (index 0)
        llvm::Type *return_type = IR::get_type(ctx.parent->getParent(), call_node->type, false).first;
        call->addParamAttr(0, llvm::Attribute::get(context, llvm::Attribute::StructRet, return_type));
        call->addParamAttr(0, llvm::Attribute::NoAlias);

        // Add byval attributes for > 16 byte input parameters
        size_t param_idx = 1;
        for (const auto &arg : call_node->arguments) {
            llvm::Type *arg_type = IR::get_type(ctx.parent->getParent(), arg.first->type, false).first;
            size_t arg_size = Allocation::get_type_size(ctx.parent->getParent(), arg_type);
            if (arg_size > 16) {
                call->addParamAttr(param_idx, llvm::Attribute::get(context, llvm::Attribute::ByVal, arg_type));
            }
            param_idx++;
        }

        // Result is in sret_alloc, need to copy to heap for the rest of Flint to understand the return value of the function
        llvm::Function *dima_allocate_fn = Module::DIMA::dima_functions.at("allocate");
        llvm::Value *result_ptr = builder.CreateCall(dima_allocate_fn, {Module::DIMA::get_head(call_node->type)}, "result_ptr");
        builder.CreateCall(c_functions.at(MEMCPY), {result_ptr, sret_alloc, builder.getInt64(return_size)});
        return std::vector<llvm::Value *>{result_ptr};
    }

    // There is no sret needed, it's a normal call
    llvm::CallInst *call = builder.CreateCall(                                   //
        result.first.value(),                                                    //
        converted_args,                                                          //
        call_node->function->name + std::to_string(call_node->call_id) + "_call" //
    );
    // Add byval attributes for > 16 byte input parameters
    for (size_t i = 0; i < call_node->arguments.size(); i++) {
        llvm::Type *arg_type = IR::get_type(ctx.parent->getParent(), call_node->arguments[i].first->type, false).first;
        size_t arg_size = Allocation::get_type_size(ctx.parent->getParent(), arg_type);
        if (arg_size > 16) {
            call->addParamAttr(i, llvm::Attribute::get(context, llvm::Attribute::ByVal, arg_type));
        }
    }
    call->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Call to extern function '" + call_node->function->name + "'")));
    std::vector<llvm::Value *> return_value;
    switch (call_node->type->get_variation()) {
        default: {
            // We have a single return value and can return that directly, but we need to check it's type still
            llvm::Value *ret_val = call;
            convert_type_from_ext(builder, ctx, call_node->type, ret_val);
            return_value.emplace_back(ret_val);
            break;
        }
        case Type::Variation::GROUP: {
            const auto *group_type = call_node->type->as<GroupType>();
            llvm::Value *ret_val = call;
            convert_type_from_ext(builder, ctx, call_node->type, ret_val);

            // Now ret_val is a pointer to the internal struct representation
            // Extract individual elements from the struct
            llvm::Type *struct_type = IR::get_type(ctx.parent->getParent(), call_node->type, false).first;
            for (unsigned int i = 0; i < group_type->types.size(); i++) {
                llvm::Value *elem_ptr = builder.CreateStructGEP(struct_type, ret_val, i);
                llvm::Value *elem = IR::aligned_load(builder, IR::get_type(ctx.parent->getParent(), group_type->types[i]).first, elem_ptr);
                return_value.emplace_back(elem);
            }
            // The Group's return value has been allocated before the extern call, now that we have extracted the results we need to free it
            // again
            llvm::Function *free_fn = c_functions.at(FREE);
            builder.CreateCall(free_fn, {ret_val});
        }
    }
    return return_value;
}

bool Generator::Expression::is_arg_reference(                    //
    const std::pair<std::unique_ptr<ExpressionNode>, bool> &arg, //
    const std::shared_ptr<Type> &param_type                      //
) {
    const Type::Variation arg_var = arg.first->type->get_variation();
    const bool is_tmp_opt = param_type->get_variation() == Type::Variation::OPTIONAL && arg_var != Type::Variation::OPTIONAL;
    bool is_temporary = is_tmp_opt;
    switch (arg.first->get_variation()) {
        default:
            break;
        case ExpressionNode::Variation::STRING_INTERPOLATION:
        case ExpressionNode::Variation::INITIALIZER:
        case ExpressionNode::Variation::ARRAY_INITIALIZER:
            is_temporary = true;
            break;
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            // Check if the array access contains a range expression
            const auto *node = arg.first->as<ArrayAccessNode>();
            for (const auto &expr : node->indexing_expressions) {
                if (expr->type->get_variation() == Type::Variation::RANGE) {
                    is_temporary = true;
                    break;
                }
            }
            break;
        }
    }
    const bool is_literal = arg.first->is_literal();
    const bool is_variant_literal = arg.first->get_variation() == ExpressionNode::Variation::LITERAL //
        && std::holds_alternative<LitVariant>(arg.first->as<LiteralNode>()->value);
    const bool is_initializer = arg.first->get_variation() == ExpressionNode::Variation::INITIALIZER;
    const bool is_opt_unwrap = arg.first->get_variation() == ExpressionNode::Variation::OPTIONAL_UNWRAP;
    const bool is_type_cast = arg.first->get_variation() == ExpressionNode::Variation::TYPE_CAST;
    const bool is_reference = arg.second && !is_temporary && (!is_literal || is_variant_literal) //
        && !is_initializer && !is_opt_unwrap && !is_type_cast;
    return is_reference;
}

Generator::group_mapping Generator::Expression::generate_call( //
    llvm::IRBuilder<> &builder,                                //
    GenerationContext &ctx,                                    //
    const CallNodeBase *call_node,                             //
    const bool is_reference                                    //
) {
    const std::string &function_name = call_node->function->name;
    const auto builtin_function = Parser::get_builtin_function(call_node->function->name, ctx.imported_core_modules);
    // Get the arguments
    std::vector<llvm::Value *> args;
    garbage_type garbage;
    std::vector<std::pair<std::shared_ptr<Type>, bool>> parameters;
    for (const auto &[param_type, param_name, param_is_mutable] : call_node->function->parameters) {
        parameters.emplace_back(param_type, param_is_mutable);
    }
    if (!generate_call_arg_prep(builder, ctx, args, garbage, call_node->arguments, parameters, builtin_function.has_value())) {
        return std::nullopt;
    }

    llvm::Function *func_decl = nullptr;
    enum class FunctionOrigin { INTERN, EXTERN, BUILTIN };
    FunctionOrigin function_origin = FunctionOrigin::INTERN;
    // First check which core modules have been imported
    if (builtin_function.has_value()) {
        return generate_builtin_call(              //
            builder,                               //
            ctx,                                   //
            garbage,                               //
            args,                                  //
            call_node,                             //
            function_name,                         //
            std::get<0>(builtin_function.value()), //
            std::get<1>(builtin_function.value())  //
        );
    } else if (call_node->function->is_extern) {
        return generate_extern_call(builder, ctx, call_node, args);
    } else {
        // Get the function definition from any module
        auto result = Function::get_function_definition(ctx.parent, call_node);
        if (!result.first.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        func_decl = result.first.value();
        function_origin = result.second ? FunctionOrigin::EXTERN : FunctionOrigin::INTERN;
    }

    // Since by now we are 100% sure that a user-defined function is called, Thread Stack rules apply. This means that we need to get the
    // current stack pointer, increment it by the current function's frame size, and then load the default frame of the to-be-called
    // function, insert our arguments into it, store it on the incremented TS, call the function and load the returned value(s)
    // The "next" stack frame already has been computed in the allocation system
    llvm::Value *const next_stack_frame = ctx.allocations.at("flint.stack.next");
    const size_t called_fn_id = call_node->function->get_id();
    llvm::StructType *const called_fn_type = Module::ThreadStack::ts_frames.at(called_fn_id);
    llvm::GlobalVariable *const called_fn_default = Module::ThreadStack::ts_defaults.at(called_fn_id);
    // Load the default frame of the to-be-called function
    llvm::Value *fn_frame = IR::aligned_load(builder, called_fn_type, called_fn_default, call_node->function->name + "_default_frame");
    // Insert the pointer to the thread stack in the function's frame, the value is loaded in the setup section
    llvm::Value *const ts_ptr = ctx.allocations.at("flint.stack.root");
    fn_frame = builder.CreateInsertValue(fn_frame, ts_ptr, {0, Module::ThreadStack::FUNCTION::THREAD_STACK});
    // Insert all arguments into the loaded default-value
    const size_t fn_ret_count = call_node->function->return_types.size();
    for (size_t i = 0; i < args.size(); i++) {
        const std::shared_ptr<Type> &param_type = std::get<0>(call_node->function->parameters.at(i));
        llvm::Value *arg_value = args[i];
        if (is_arg_reference(call_node->arguments[i], param_type)) {
            const auto param_type_pair = IR::get_type(ctx.parent->getParent(), param_type);
            llvm::Type *const param_ty = param_type_pair.second.first ? PTR_TY : param_type_pair.first;
            arg_value = IR::aligned_load(builder, param_ty, arg_value);
        }
        // arg_value->dump();
        fn_frame = builder.CreateInsertValue(                                                                                      //
            fn_frame, arg_value, i + fn_ret_count + 1, call_node->function->name + "_frame_arg_" + std::to_string(i) + "_inserted" //
        );
    }
    // Store the function frame in the next free spot in the TS
    IR::aligned_store(builder, fn_frame, next_stack_frame);

    // Call the actual function by passing the stack frame pointer to it
    llvm::CallInst *call = builder.CreateCall(                       //
        func_decl,                                                   //
        {next_stack_frame},                                          //
        function_name + std::to_string(call_node->call_id) + "_call" //
    );
    call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Call of function '" + function_name + "'")));
#ifndef __WIN32__
    call->addParamAttr(0, llvm::Attribute::InReg);
    if (OPTIMIZE_MODE != OptimizeMode::DEBUG) {
        // Add the 'tailcc' to every user-defined call
        call->setCallingConv(llvm::CallingConv::Tail);
        call->setTailCall();
    }
#endif
    last_err_values = {call, next_stack_frame};

    // Do all the common call cleanup on the arguments of the call
    if (!generate_call_arg_cleanup(                                                                           //
            builder, ctx, args, garbage, called_fn_type, next_stack_frame, call_node->function->return_types, //
            call_node->arguments, parameters, call_node->function->name)                                      //
    ) {
        return std::nullopt;
    }

    // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
    if (!call_node->has_catch) {
        generate_rethrow(builder, ctx, call_node, call_node->function->name);
    }

    // Add the call instruction to the list of unresolved functions only if it was a module-intern call
    if (function_origin == FunctionOrigin::INTERN) {
        const std::string target_name = call_node->function->file_hash.to_string() + "." + function_name //
            + (call_node->function->mangle_id.has_value() ? ("." + std::to_string(call_node->function->mangle_id.value())) : "");

        if (unresolved_functions.find(target_name) == unresolved_functions.end()) {
            unresolved_functions[target_name] = {call};
        } else {
            unresolved_functions[target_name].push_back(call);
        }
    } else if (function_origin == FunctionOrigin::EXTERN) {
        const std::string &target_name = call_node->function->file_hash.to_string() + "." + function_name;
        for (const auto &[file_name, function_list] : file_function_names) {
            if (std::find(function_list.begin(), function_list.end(), function_name) != function_list.end()) {
                // Check if any unresolved function call from a function of that file even exists, if not create the first one
                if (file_unresolved_functions.find(file_name) == file_unresolved_functions.end()) {
                    file_unresolved_functions[file_name][function_name] = {call};
                    break;
                }
                // Check if this function the call references has been referenced before. If not, create a new entry to the map
                // otherwise just add the call
                if (file_unresolved_functions.at(file_name).find(function_name) == file_unresolved_functions.at(file_name).end()) {
                    file_unresolved_functions.at(file_name)[function_name] = {call};
                } else {
                    file_unresolved_functions.at(file_name).at(function_name).push_back(call);
                }
                break;
            }
        }
    }

    // Extract all the return values from the call
    std::vector<llvm::Value *> return_value;
    for (unsigned int i = 0; i < fn_ret_count; i++) {
        llvm::Value *elem_ptr = builder.CreateStructGEP(                                                      //
            called_fn_type,                                                                                   //
            next_stack_frame,                                                                                 //
            i + 1,                                                                                            //
            function_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value_ptr" //
        );
        if (is_reference) {
            return_value.emplace_back(elem_ptr);
        } else {
            llvm::LoadInst *elem_value = IR::aligned_load(builder,                                            //
                called_fn_type->getElementType(i + 1),                                                        //
                elem_ptr,                                                                                     //
                function_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value" //
            );
            return_value.emplace_back(elem_value);
        }
    }
    return return_value;
}

bool Generator::Expression::generate_call_arg_prep(                                 //
    llvm::IRBuilder<> &builder,                                                     //
    GenerationContext &ctx,                                                         //
    std::vector<llvm::Value *> &args,                                               //
    garbage_type &garbage,                                                          //
    const std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
    const std::vector<std::pair<std::shared_ptr<Type>, bool>> &parameters,          //
    const bool is_builtin                                                           //
) {
    ASSERT(arguments.size() == parameters.size());
    args.reserve(arguments.size());

    for (size_t i = 0; i < arguments.size(); i++) {
        const auto &arg = arguments[i];
        const std::shared_ptr<Type> &param_type = parameters.at(i).first;
        // Complex arguments of function calls are always passed as references, but if the data type is complex this "reference" is a
        // pointer to the actual data of the variable, not a pointer to its allocation. So, in this case we are not allowed to pass in any
        // variable as "reference" because then a double pointer is passed to the function where a single pointer is expected This behaviour
        // should only effect array types, as data and strings are handled differently
        //
        // Optionals are always passed by-reference but because of the Thread Stack any "argument" of a function is *always* stored in the
        // passed-by thread stack frame, and it is stored inside it *by value*. This means that every argument which "is" a reference needs
        // to be loaded and stored in it's original data position after the function call *if* that parameter of the function is mutable
        //
        // This also means that the `expression` / the `args[i]` value now is a *pointer to the value* if `is_reference` is true. This means
        // that it needs to be loaded before stored in the TS. We need this behaviour to be able to "load and store" the value of the TS
        // parameter after the call is finished.
        //
        // Because builtin calls do not rely on the TS at all, the argument does not need to be a reference for builtin calls at all, we
        // just want the "raw" value to pass to the builtin function in that case. The same is true for extern calls too
        const bool is_reference = param_type->is_reference() && !is_builtin;
        group_mapping expression = generate_expression(builder, ctx, garbage, 0, arg.first.get(), is_reference);
        if (!expression.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (expression.value().empty()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        llvm::Value *expr_val = expression.value().front();

        // Check if we need to convert non-optional to optional
        const auto arg_type = arg.first->type;
        const Type::Variation arg_var = arg_type->get_variation();
        const Type::Variation param_var = param_type->get_variation();
        const bool is_tmp_opt = param_var == Type::Variation::OPTIONAL && arg_var != Type::Variation::OPTIONAL;
        const bool is_dima_managed = param_type->is_dima_managed() && arg_type->is_dima_managed();
        if (is_tmp_opt) {
            // We are passing a non-optional value to a function expecting an optional, so we build up the optional struct which is later
            // stored in the TS frame of the called function
            // If the value contained within the optional is a reference then we need to load that reference first
            if (arg_type->is_reference()) {
                expr_val = IR::aligned_load(builder, IR::get_type(ctx.parent->getParent(), arg_type).first, expr_val, "loaded_expr_val");
            }
            llvm::Type *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), param_type, false);
            llvm::Value *temp_opt = IR::get_default_value_of_type(opt_struct_type);
            temp_opt = builder.CreateInsertValue(temp_opt, builder.getInt1(true), 0);
            temp_opt = builder.CreateInsertValue(temp_opt, expr_val, 1, "temp_opt");
            expr_val = temp_opt;
        } else if (is_dima_managed) {
            // Call `dima.retain` to increment the ARC before passing the argument to the function
            // Data is always passed by reference
            llvm::Function *retain_fn = Module::DIMA::dima_functions.at("retain");
            llvm::Value *data_value = expr_val;
            const bool is_initializer = arg.first->get_variation() == ExpressionNode::Variation::INITIALIZER;
            const bool is_opt_unwrap = arg.first->get_variation() == ExpressionNode::Variation::OPTIONAL_UNWRAP;
            if (is_reference && !is_initializer && !is_opt_unwrap) {
                data_value = IR::aligned_load(builder, PTR_TY, expr_val, "data_value");
            }
            llvm::CallInst *retain_call = builder.CreateCall(retain_fn, {data_value});
            retain_call->setMetadata("comment",
                llvm::MDNode::get(context, llvm::MDString::get(context, "Calling 'retain' before passing data to function")));
        }
        args.emplace_back(expr_val);
    }
    // Now that we have generated all arguments check which need to be freed after the call, e.g. which garbage we collected
    for (size_t i = 0; i < parameters.size(); i++) {
        const auto &[param_type, is_mutable] = parameters.at(i);
        if (!param_type->is_freeable()) {
            continue;
        }
        // We only need to free garbage which is a "producer", so things like variables etc do *not* need to be freed
        const auto &arg = arguments.at(i).first;
        const ExpressionNode::Variation arg_variation = arg->get_variation();
        const std::string &arg_type_str = arg->type->to_string();
        const bool is_initializer = arg_variation == ExpressionNode::Variation::INITIALIZER;
        const bool is_variant_initializer = arg_variation == ExpressionNode::Variation::LITERAL //
            && std::holds_alternative<LitVariant>(arg->as<LiteralNode>()->value);
        const bool is_array_initializer = arg_variation == ExpressionNode::Variation::ARRAY_INITIALIZER;
        const bool is_fn_return = arg_variation == ExpressionNode::Variation::CALL;
        const bool is_string_interpolation = arg_variation == ExpressionNode::Variation::STRING_INTERPOLATION;
        bool is_slice = false;
        if (arg_variation == ExpressionNode::Variation::ARRAY_ACCESS) {
            const auto *arg_arr_access = arg->as<ArrayAccessNode>();
            for (const auto &index : arg_arr_access->indexing_expressions) {
                if (index->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                    is_slice = true;
                    break;
                }
            }
        }
        const bool is_string_literal =                                                              //
            arg_type_str == "str"                                                                   //
            && arg_variation == ExpressionNode::Variation::TYPE_CAST                                //
            && arg->as<TypeCastNode>()->expr->get_variation() == ExpressionNode::Variation::LITERAL //
            && std::holds_alternative<LitStr>(arg->as<TypeCastNode>()->expr->as<LiteralNode>()->value);
        if (is_initializer || is_variant_initializer || is_array_initializer || is_fn_return || is_string_interpolation || is_slice ||
            is_string_literal) {
            if (garbage.find(0) == garbage.end()) {
                std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>> vec{{param_type, args.at(i)}};
                garbage[0] = std::move(vec);
            } else {
                // Check if the garbage already contains the parameter value
                bool already_contains_param = false;
                for (const auto &[t, v] : garbage.at(0)) {
                    if (t->equals(arg->type) && v == args.at(i)) {
                        already_contains_param = true;
                        break;
                    }
                }
                if (!already_contains_param) {
                    garbage.at(0).emplace_back(param_type, args.at(i));
                }
            }
        }
    }
    return true;
}

bool Generator::Expression::generate_call_arg_cleanup(                              //
    llvm::IRBuilder<> &builder,                                                     //
    GenerationContext &ctx,                                                         //
    std::vector<llvm::Value *> &args,                                               //
    garbage_type &garbage,                                                          //
    const std::optional<llvm::StructType *> called_fn_type,                         //
    llvm::Value *const next_stack_frame,                                            //
    const std::vector<std::shared_ptr<Type>> fn_ret_types,                          //
    const std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> &arguments, //
    const std::vector<std::pair<std::shared_ptr<Type>, bool>> &parameters,          //
    const std::string &fn_name                                                      //
) {
    // Call 'dima.release' on the retained function arguments. It is important that we release the retained function arguments *before*
    // assigning back the potentially modified parameters of the called function (the next loop).
    for (size_t i = 0; i < arguments.size(); i++) {
        const auto &arg = arguments[i];
        if (!arg.first->type->is_dima_managed()) {
            continue;
        }
        // Call `dima.release` to decrement the ARC after the function call
        llvm::Function *release_fn = Module::DIMA::dima_functions.at("release");
        auto data_head = Module::DIMA::get_head(arg.first->type);
        llvm::Value *data_value = args.at(i);
        const bool is_initializer = arg.first->get_variation() == ExpressionNode::Variation::INITIALIZER;
        const bool is_opt_unwrap = arg.first->get_variation() == ExpressionNode::Variation::OPTIONAL_UNWRAP;
        if (!is_initializer && !is_opt_unwrap) {
            data_value = IR::aligned_load(builder, PTR_TY, data_value, "data_value");
        }
        llvm::CallInst *release_call = builder.CreateCall(release_fn, {data_head, data_value});
        release_call->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Calling 'release' on arg " + std::to_string(i) + " after calling function '" + fn_name + "'")));
    }

    // Get the pointer to where the parameter types begin (for callables only)
    llvm::Value *param_start_ptr = nullptr;
    if (!called_fn_type.has_value()) {
        // First we need to skip the `function_t` value of every function frame, which should be 32 bytes
        llvm::Module *const module = ctx.parent->getParent();
        size_t offset = Allocation::get_type_size(module, type_map.at("type.ts.function"));
        // Now the offset is at the function return values, we need to skip them as they are placed in front of the arguments
        for (const auto &return_type : fn_ret_types) {
            llvm::Type *const return_ty = IR::get_type(module, return_type).first;
            const size_t return_size = Allocation::get_type_size(module, return_ty);
            const size_t return_align = Allocation::calculate_type_alignment(return_ty);
            // Add alignment
            offset += (return_align - (offset % return_align)) % return_align;
            // Add the param size
            offset += return_size;
        }
        // Align to the first parameter's alignment so the parameter area starts on a correct boundary
        if (!parameters.empty()) {
            const size_t first_param_align = Allocation::calculate_type_alignment(IR::get_type(module, parameters.front().first).first);
            offset += (first_param_align - (offset % first_param_align)) % first_param_align;
        }
        param_start_ptr = builder.CreateGEP(builder.getInt8Ty(), next_stack_frame, builder.getInt64(offset), "param_start_ptr");
    }

    // Store back the mutable reference function arguments in their original location (within the caller's TS frame) from the callees
    // TS frame. For example if we pass mutable data to a function and re-assign that data in it the caller's variable still points to the
    // old memory, which is potentially freed now (from the re-assignment process). Same is true for strings and any other by-reference
    // values passed to functions.
    size_t offset = 0;
    for (size_t i = 0; i < arguments.size(); i++) {
        const auto &arg = arguments[i];
        const Type::Variation arg_var = arg.first->type->get_variation();
        const std::shared_ptr<Type> &param_type = parameters.at(i).first;

        const bool is_tmp_opt = param_type->get_variation() == Type::Variation::OPTIONAL && arg_var != Type::Variation::OPTIONAL;
        bool is_temporary = is_tmp_opt;
        switch (arg.first->get_variation()) {
            default:
                break;
            case ExpressionNode::Variation::STRING_INTERPOLATION:
            case ExpressionNode::Variation::INITIALIZER:
            case ExpressionNode::Variation::ARRAY_INITIALIZER:
                is_temporary = true;
                break;
            case ExpressionNode::Variation::ARRAY_ACCESS: {
                // Check if the array access contains a range expression
                const auto *node = arg.first->as<ArrayAccessNode>();
                for (const auto &expr : node->indexing_expressions) {
                    if (expr->type->get_variation() == Type::Variation::RANGE) {
                        is_temporary = true;
                        break;
                    }
                }
                break;
            }
        }
        const bool is_literal = arguments.at(i).first->is_literal();
        const bool is_opt_unwrap = arg.first->get_variation() == ExpressionNode::Variation::OPTIONAL_UNWRAP;
        const bool is_type_cast = arg.first->get_variation() == ExpressionNode::Variation::TYPE_CAST;
        const bool is_reference = arg.second && !is_literal && !is_temporary && !is_opt_unwrap && !is_type_cast;

        const bool is_mutable = parameters.at(i).second;
        if (!is_reference || !is_mutable) {
            continue;
        }

        const auto param_type_pair = IR::get_type(ctx.parent->getParent(), param_type);
        llvm::Type *const param_ty = param_type_pair.second.first ? PTR_TY : param_type_pair.first;
        llvm::Value *value_ptr = nullptr;
        if (called_fn_type.has_value()) {
            value_ptr = builder.CreateStructGEP(                                                                           //
                called_fn_type.value(), next_stack_frame, i + fn_ret_types.size() + 1, "arg_" + std::to_string(i) + "_ptr" //
            );
        } else {
            ASSERT(param_start_ptr != nullptr);
            const size_t type_size = Allocation::get_type_size(ctx.parent->getParent(), param_ty);
            const size_t type_align = Allocation::calculate_type_alignment(param_ty);
            // Make the offset aligned to the parameter
            offset += (type_align - (offset % type_align)) % type_align;
            value_ptr = builder.CreateGEP(                                                                          //
                builder.getInt8Ty(), param_start_ptr, builder.getInt64(offset), "arg_" + std::to_string(i) + "_ptr" //
            );
            // Add the type's size to the offset for the next parameter
            offset += type_size;
        }
        llvm::Value *const value = IR::aligned_load(builder, param_ty, value_ptr, "arg_" + std::to_string(i) + "_value");
        IR::aligned_store(builder, value, args[i]);
    }

    // Clear all garbage of temporary parameters
    if (!Statement::clear_garbage(builder, garbage)) {
        return false;
    }

    return true;
}

Generator::group_mapping Generator::Expression::generate_builtin_call( //
    llvm::IRBuilder<> &builder,                                        //
    GenerationContext &ctx,                                            //
    garbage_type &garbage,                                             //
    std::vector<llvm::Value *> &args,                                  //
    const CallNodeBase *call_node,                                     //
    const std::string &function_name,                                  //
    const std::string &module_name,                                    //
    const overloads &fn_overloads                                      //
) {
    std::vector<llvm::Value *> return_value;
    llvm::Function *func_decl = nullptr;
    if (module_name == "print" && function_name == "print" && call_node->arguments.size() == 1        //
        && Module::Print::print_functions.find(call_node->arguments.front().first->type->to_string()) //
            != Module::Print::print_functions.end()                                                   //
    ) {
        // Call the builtin function 'print'
        const std::string type_str = call_node->arguments.front().first->type->to_string();
        return_value.emplace_back(builder.CreateCall(Module::Print::print_functions.at(type_str), args));
        if (!Statement::clear_garbage(builder, garbage)) {
            return std::nullopt;
        }
        return return_value;
    } else if (module_name == "read" && call_node->arguments.size() == 0                          //
        && Module::Read::read_functions.find(function_name) != Module::Read::read_functions.end() //
    ) {
        if (fn_overloads.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (!std::get<2>(fn_overloads.front()).empty()) {
            // Function returns error
            func_decl = Module::Read::read_functions.at(function_name);
        } else {
            // Function does not return error
            func_decl = Module::Read::read_functions.at(function_name);
            return_value.emplace_back(builder.CreateCall(func_decl, args));
            return return_value;
        }
    } else if (module_name == "assert" && call_node->arguments.size() == 1                                //
        && Module::Assert::assert_functions.find(function_name) != Module::Assert::assert_functions.end() //
    ) {
        func_decl = Module::Assert::assert_functions.at(function_name);
    } else if (module_name == "filesystem"                                                                //
        && Module::FileSystem::fs_functions.find(function_name) != Module::FileSystem::fs_functions.end() //
    ) {
        if (fn_overloads.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (!std::get<2>(fn_overloads.front()).empty()) {
            // Function returns error
            func_decl = Module::FileSystem::fs_functions.at(function_name);
        } else {
            // Function does not return error
            func_decl = Module::FileSystem::fs_functions.at(function_name);
            return_value.emplace_back(builder.CreateCall(func_decl, args));
            return return_value;
        }
    } else if (module_name == "env"                                                           //
        && Module::Env::env_functions.find(function_name) != Module::Env::env_functions.end() //
    ) {
        if (fn_overloads.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (!std::get<2>(fn_overloads.front()).empty()) {
            // Function returns error
            func_decl = Module::Env::env_functions.at(function_name);
        } else {
            // Function does not return error
            func_decl = Module::Env::env_functions.at(function_name);
            return_value.emplace_back(builder.CreateCall(func_decl, args));
            return return_value;
        }
    } else if (module_name == "system"                                                                    //
        && Module::System::system_functions.find(function_name) != Module::System::system_functions.end() //
    ) {
        if (fn_overloads.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (!std::get<2>(fn_overloads.front()).empty()) {
            // Function returns error
            func_decl = Module::System::system_functions.at(function_name);
        } else {
            // Function does not return error
            func_decl = Module::System::system_functions.at(function_name);
            return_value.emplace_back(builder.CreateCall(func_decl, args));
            return return_value;
        }
    } else if (module_name == "math") {
        std::string fn_name = function_name;
        bool fn_found = Module::Math::math_functions.find(fn_name) != Module::System::system_functions.end();
        if (!fn_found && !call_node->arguments.empty()) {
            fn_name = fn_name + "_" + call_node->arguments.front().first->type->to_string();
            fn_found = Module::Math::math_functions.find(fn_name) != Module::System::system_functions.end();
        }
        if (fn_found) {
            size_t idx = 0;
            if (fn_overloads.size() > 1) {
                bool found = false;
                for (const auto &fn : fn_overloads) {
                    auto arg_types = std::get<0>(fn);
                    if (arg_types.size() != call_node->arguments.size()) {
                        ++idx;
                        continue;
                    }
                    bool args_match = true;
                    for (size_t i = 0; i < arg_types.size(); i++) {
                        if (call_node->arguments.at(i).first->type->to_string() != arg_types.at(i).first) {
                            args_match = false;
                            break;
                        }
                    }
                    if (!args_match) {
                        ++idx;
                        continue;
                    }
                    found = true;
                    break;
                }
                if (!found) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
            }
            if (!std::get<2>(fn_overloads.at(idx)).empty()) {
                // Function returns error
                func_decl = Module::Math::math_functions.at(fn_name);
            } else {
                // Function does not return error
                func_decl = Module::Math::math_functions.at(fn_name);
                return_value.emplace_back(builder.CreateCall(func_decl, args));
                return return_value;
            }
        }
    } else if (module_name == "parse"                                                                 //
        && Module::Parse::parse_functions.find(function_name) != Module::Parse::parse_functions.end() //
    ) {
        if (fn_overloads.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (!std::get<2>(fn_overloads.front()).empty()) {
            // Function returns error
            func_decl = Module::Parse::parse_functions.at(function_name);
        } else {
            // Function does not return error
            func_decl = Module::Parse::parse_functions.at(function_name);
            return_value.emplace_back(builder.CreateCall(func_decl, args));
            return return_value;
        }
    } else if (module_name == "time" &&
        ((function_name == "sleep" && call_node->arguments.size() >= 1)                                //
            || Module::Time::time_functions.find(function_name) != Module::Time::time_functions.end()) //
    ) {
        // Handle sleep overloads
        std::string fn_name = function_name;
        if (function_name == "sleep") {
            if (call_node->arguments.front().first->type->to_string() == "Duration") {
                fn_name = "sleep_duration";
            } else {
                fn_name = "sleep_time";
            }
        } else {
            // Other 'time' module functions do not have overloads
            if (fn_overloads.size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
        }
        // No function from the time module is able to throw
        ASSERT(std::get<2>(fn_overloads.front()).empty());
        func_decl = Module::Time::time_functions.at(fn_name);
        return_value.emplace_back(builder.CreateCall(func_decl, args));
        for (size_t i = 0; i < call_node->arguments.size(); i++) {
            const auto &arg = call_node->arguments[i];
            if (!arg.first->type->is_dima_managed()) {
                continue;
            }
            if (!call_node->arguments.at(i).first->type->is_dima_managed()) {
                continue;
            }
            // Call `dima.release` to decrement the ARC after the function call
            llvm::Function *release_fn = Module::DIMA::dima_functions.at("release");
            auto data_head = Module::DIMA::get_head(arg.first->type);
            llvm::CallInst *release_call = builder.CreateCall(release_fn, {data_head, args.at(i)});
            release_call->setMetadata("comment",
                llvm::MDNode::get(context,
                    llvm::MDString::get(context,
                        "Calling 'release' on arg " + std::to_string(i) + " after calling function '" + call_node->function->name + "'")));
        }
        return return_value;
    } else {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }

    // Create the call instruction using the original declaration
    // llvm::dbgs() << "func_decl[\"" << function_name << "\"]: ";
    // func_decl->getFunctionType()->dump();
    // for (const auto &arg : args) {
    //     arg->dump();
    // }
    llvm::CallInst *call = builder.CreateCall(                       //
        func_decl,                                                   //
        args,                                                        //
        function_name + std::to_string(call_node->call_id) + "_call" //
    );
    call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Call of function '" + function_name + "'")));

    // Call 'dima.release' on the retained function arguments
    for (size_t i = 0; i < call_node->arguments.size(); i++) {
        const auto &arg = call_node->arguments[i];
        if (!arg.first->type->is_dima_managed()) {
            continue;
        }
        if (!call_node->arguments.at(i).first->type->is_dima_managed()) {
            continue;
        }
        // Call `dima.release` to decrement the ARC after the function call
        llvm::Function *release_fn = Module::DIMA::dima_functions.at("release");
        auto data_head = Module::DIMA::get_head(arg.first->type);
        llvm::CallInst *release_call = builder.CreateCall(release_fn, {data_head, args.at(i)});
        release_call->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Calling 'release' on arg " + std::to_string(i) + " after calling function '" + call_node->function->name + "'")));
    }

    // Extract and store error value
    llvm::Value *err_val = builder.CreateExtractValue(call, 0, function_name + std::to_string(call_node->call_id) + "_err_val");
    llvm::Value *error_ptr = builder.CreateStructGEP(                                                          //
        type_map.at("type.ts.function"), ctx.allocations.at("flint.stack"), Module::ThreadStack::FUNCTION::ERR //
    );
    IR::aligned_store(builder, err_val, error_ptr);
    llvm::Value *err_id = builder.CreateExtractValue(err_val, 0, "err_id");
    llvm::Value *fn_failed = builder.CreateICmpNE(err_id, builder.getInt32(0), "fn_" + function_name + "_failed");
    last_err_values = {fn_failed, ctx.allocations.at("flint.stack")};

    // Clear all garbage of temporary parameters
    if (!Statement::clear_garbage(builder, garbage)) {
        return std::nullopt;
    }

    // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
    if (!call_node->has_catch) {
        generate_rethrow(builder, ctx, call_node, call_node->function->name);
    }

    // Extract all the return values from the call (everything except the error return)
    ASSERT(return_value.empty());
    llvm::StructType *const return_type = static_cast<llvm::StructType *>( //
        IR::add_and_or_get_type(ctx.parent->getParent(), call_node->type)  //
    );
    for (unsigned int i = 1; i < return_type->getNumElements(); i++) {
        llvm::Value *elem_value = builder.CreateExtractValue(                                                      //
            call, i, function_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value" //
        );
        return_value.emplace_back(elem_value);
    }
    return return_value;
}

Generator::group_mapping Generator::Expression::generate_callable_call( //
    llvm::IRBuilder<> &builder,                                         //
    GenerationContext &ctx,                                             //
    const CallableCallNodeBase *call_node,                              //
    const bool is_reference                                             //
) {
    // First we prepare all the arguments
    std::vector<llvm::Value *> args;
    garbage_type garbage;
    const std::string &fn_name = call_node->callable_variable;
    const Scope::Variable variable = ctx.scope->variables.at(fn_name);
    ASSERT(variable.type->get_variation() == Type::Variation::FN);
    const FnType *fn_type = variable.type->as<FnType>();
    if (!generate_call_arg_prep(builder, ctx, args, garbage, call_node->arguments, fn_type->params)) {
        return std::nullopt;
    }

    // Then we are loading the pointer to the allocated stack frame from the callable variable's allocation
    const unsigned int callable_scope_id = ctx.scope->variables.at(fn_name).scope_id;
    const std::string callable_var_str = "s" + std::to_string(callable_scope_id) + "::" + fn_name;
    llvm::Value *const callable_alloca = ctx.allocations.at(callable_var_str);
    llvm::Value *const callable_value = IR::aligned_load(builder, PTR_TY, callable_alloca, "callable_value");

    // Get the pointer to the callable frame
    llvm::Value *const callable_frame = builder.CreateGEP(PTR_TY, callable_value, builder.getInt32(1), "callable_frame");
    // Insert the pointer to the thread stack in the function's frame, the value is loaded in the setup section
    llvm::Value *const ts_ptr = ctx.allocations.at("flint.stack.root");
    IR::aligned_store(builder, ts_ptr, callable_frame);
    // Store the next frame in the TS
    llvm::Value *const next_ts_ptr = builder.CreateStructGEP(                                       //
        type_map.at("type.ts.stack"), ts_ptr, Module::ThreadStack::STACK::STACK_PTR, "ts_stack_ptr" //
    );
    IR::aligned_store(builder, ctx.allocations.at("flint.stack.next"), next_ts_ptr);
    // Store the IS_CALLABLE flag in the flags section of the TS root
    llvm::Value *const ts_flags_ptr = builder.CreateStructGEP(                                  //
        type_map.at("type.ts.stack"), ts_ptr, Module::ThreadStack::STACK::FLAGS, "ts_flags_ptr" //
    );
    IR::aligned_store(builder, builder.getInt32(Module::ThreadStack::STACK::FLAG::TS_FLAG_CALLABLE), ts_flags_ptr);

    // Store the arguments in the called function
    llvm::Module *const module = ctx.parent->getParent();
    size_t offset = Allocation::get_type_size(module, type_map.at("type.ts.function"));
    // Skip all the return values first
    for (const auto &return_type : fn_type->return_types) {
        const auto type_pair = IR::get_type(module, return_type);
        llvm::Type *const return_ty = type_pair.second.first ? PTR_TY : type_pair.first;
        const size_t return_size = Allocation::get_type_size(module, return_ty);
        const size_t return_align = Allocation::calculate_type_alignment(return_ty);
        // Add alignment
        offset += (return_align - (offset % return_align)) % return_align;
        // Add the ret type size
        offset += return_size;
    }
    // Then store the arguments
    for (size_t i = 0; i < fn_type->params.size(); i++) {
        const auto &param_type = fn_type->params.at(i).first;
        const auto type_pair = IR::get_type(module, param_type);
        llvm::Type *const param_ty = type_pair.second.first ? PTR_TY : type_pair.first;
        const size_t param_size = Allocation::get_type_size(module, param_ty);
        const size_t param_align = Allocation::calculate_type_alignment(param_ty);
        // Add alignment
        offset += (param_align - (offset % param_align)) % param_align;
        // Store arg i at offset in the callable frame
        llvm::Value *const value_ptr = builder.CreateGEP(                                                                      //
            builder.getInt8Ty(), callable_frame, builder.getInt64(offset), fn_name + "_call_arg_" + std::to_string(i) + "_ptr" //
        );

        // Check if the arg is a reference
        llvm::Value *arg_value = args[i];
        if (is_arg_reference(call_node->arguments[i], param_type)) {
            arg_value = IR::aligned_load(builder, param_ty, arg_value);
        }
        // arg_value->dump();
        IR::aligned_store(builder, arg_value, value_ptr);

        // Add the param size
        offset += param_size;
    }

    // Load the function to call
    llvm::Value *const fn_ptr = IR::aligned_load(builder, PTR_TY, callable_value, "fn_ptr");
    // Call the loaded function and pass the callable frame to it
    // We can do a small trick here. Because the function types of *all* Flint functions are exactly the same, we can just take the type of
    // the current function we are in
    const llvm::FunctionCallee fn_to_call = llvm::FunctionCallee(ctx.parent->getFunctionType(), fn_ptr);
    llvm::CallInst *const call = builder.CreateCall(fn_to_call, {callable_frame}, fn_name + std::to_string(call_node->call_id) + "_call");
    call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Call of function '" + fn_name + "'")));
#ifndef __WIN32__
    call->addParamAttr(0, llvm::Attribute::InReg);
    if (OPTIMIZE_MODE != OptimizeMode::DEBUG) {
        // Add the 'tailcc' to every user-defined call, including callable calls as well, let's see how LLVM reacts to it...
        call->setCallingConv(llvm::CallingConv::Tail);
        call->setTailCall();
    }
#endif
    last_err_values = {call, callable_frame};

    // Reset the ts flags to their original value
    IR::aligned_store(builder, ctx.allocations.at("flint.stack.flags"), ts_flags_ptr);

    // Do all the common call cleanup on the arguments of the call
    if (!generate_call_arg_cleanup(                                                           //
            builder, ctx, args, garbage, std::nullopt, callable_frame, fn_type->return_types, //
            call_node->arguments, fn_type->params, fn_name)                                   //
    ) {
        return std::nullopt;
    }

    // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
    if (!call_node->has_catch) {
        generate_rethrow(builder, ctx, call_node, fn_name);
    }

    // Extract all the return values from the call
    std::vector<llvm::Value *> return_value;
    offset = Allocation::get_type_size(module, type_map.at("type.ts.function"));
    for (unsigned int i = 0; i < fn_type->return_types.size(); i++) {
        const auto &return_type = fn_type->return_types.at(i);
        const auto &ret_type_pair = IR::get_type(module, return_type);
        llvm::Type *const ret_llvm_type = ret_type_pair.second.first ? PTR_TY : ret_type_pair.first;
        const size_t return_type_size = Allocation::get_type_size(module, ret_llvm_type);
        const size_t return_type_align = Allocation::calculate_type_alignment(ret_llvm_type);
        // Add alignment for the return type
        offset += (return_type_align - (offset % return_type_align)) % return_type_align;
        llvm::Value *const elem_ptr = builder.CreateGEP(                                                //
            builder.getInt8Ty(),                                                                        //
            callable_frame,                                                                             //
            builder.getInt64(offset),                                                                   //
            fn_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value_ptr" //
        );
        if (is_reference) {
            return_value.emplace_back(elem_ptr);
        } else {
            llvm::Type *const ret_type = ret_type_pair.second.first ? PTR_TY : ret_type_pair.first;
            llvm::LoadInst *elem_value = IR::aligned_load(builder,                                      //
                ret_type,                                                                               //
                elem_ptr,                                                                               //
                fn_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value" //
            );
            return_value.emplace_back(elem_value);
        }
        // Add the return type's size for the next return value
        offset += return_type_size;
    }
    return return_value;
}

Generator::group_mapping Generator::Expression::generate_instance_call( //
    llvm::IRBuilder<> &builder,                                         //
    GenerationContext &ctx,                                             //
    garbage_type &garbage,                                              //
    const unsigned int expr_depth,                                      //
    const InstanceCallNodeBase *call_node,                              //
    const bool is_reference                                             //
) {
    switch (call_node->instance_variable->type->get_variation()) {
        default:
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        case Type::Variation::INTERFACE: {
            auto result = generate_expression(builder, ctx, garbage, expr_depth, call_node->instance_variable.get(), true);
            if (!result.has_value()) {
                return std::nullopt;
            }
            ASSERT(result.value().size() == 1);
            llvm::Value *const interface_instance = result.value().front();

            // Prepare all the arguments we pass to the function
            std::vector<llvm::Value *> args;
            std::vector<std::pair<std::shared_ptr<Type>, bool>> parameters;
            for (const auto &[param_type, param_name, param_is_mutable] : call_node->function->parameters) {
                parameters.emplace_back(param_type, param_is_mutable);
            }
            if (!generate_call_arg_prep(builder, ctx, args, garbage, call_node->arguments, parameters)) {
                return std::nullopt;
            }

            // Set up the frame by calling the dispatch function in setup-mode
            llvm::Value *const next_stack_frame = ctx.allocations.at("flint.stack.next");
            const InterfaceType *interface_type = call_node->instance_variable->type->as<InterfaceType>();
            llvm::StructType *const interface_ty = type_map.at(interface_type->get_type_string());
            llvm::Value *const fn_id = builder.getInt64(call_node->function->get_id());
            llvm::Value *const object_ptr_ptr = builder.CreateStructGEP(interface_ty, interface_instance, 0, "object_ptr_ptr");
            llvm::Value *const object_ptr = IR::aligned_load(builder, PTR_TY, object_ptr_ptr, "object_ptr");
            llvm::Value *const dispatch_fn_ptr = builder.CreateStructGEP(interface_ty, interface_instance, 1, "dispatch_fn_ptr");
            llvm::Value *const dispatch_fn_raw = IR::aligned_load(builder, PTR_TY, dispatch_fn_ptr, "dispatch_fn");
            llvm::FunctionType *const dispatch_fn_ty = llvm::FunctionType::get(            //
                PTR_TY, {PTR_TY, PTR_TY, builder.getInt64Ty(), builder.getInt1Ty()}, false //
            );
            llvm::FunctionCallee dispatch_fn(dispatch_fn_ty, dispatch_fn_raw);
            llvm::CallInst *const setup_call = builder.CreateCall(                                     //
                dispatch_fn, {next_stack_frame, object_ptr, fn_id, builder.getInt1(true)}, "arg_start" //
            );
#ifndef __WIN32__
            setup_call->addParamAttr(0, llvm::Attribute::InReg);
            if (OPTIMIZE_MODE != OptimizeMode::DEBUG) {
                // Add the 'tailcc' to every user-defined call
                setup_call->setCallingConv(llvm::CallingConv::Tail);
                setup_call->setTailCall();
            }
#endif

            // Store the pointer to the thread stack in the function's frame, the value is loaded in the setup section
            llvm::Value *const ts_ptr = ctx.allocations.at("flint.stack.root");
            llvm::StructType *const ts_fn_ty = type_map.at("type.ts.function");
            llvm::Value *const ts_ptr_ptr = builder.CreateStructGEP(                              //
                ts_fn_ty, next_stack_frame, Module::ThreadStack::FUNCTION::THREAD_STACK, "ts_ptr" //
            );
            IR::aligned_store(builder, ts_ptr, ts_ptr_ptr);

            // Store extra function parameters starting at the returned arg pointer
            llvm::Value *arg_ptr = setup_call;
            for (size_t i = 0; i < args.size(); i++) {
                const std::shared_ptr<Type> &param_type = std::get<0>(call_node->function->parameters.at(i));
                llvm::Value *arg_value = args[i];
                if (is_arg_reference(call_node->arguments[i], param_type)) {
                    const auto param_type_pair = IR::get_type(ctx.parent->getParent(), param_type);
                    llvm::Type *const param_ty = param_type_pair.second.first ? PTR_TY : param_type_pair.first;
                    arg_value = IR::aligned_load(builder, param_ty, arg_value);
                }
                IR::aligned_store(builder, arg_value, arg_ptr);
                arg_ptr = builder.CreateGEP(arg_value->getType(), arg_ptr, builder.getInt32(1), "arg_ptr_" + std::to_string(i + 1));
            }

            // Call dispatch function in execute mode to actually call the targetted function
            llvm::CallInst *call = builder.CreateCall(                         //
                dispatch_fn,                                                   //
                {next_stack_frame, object_ptr, fn_id, builder.getInt1(false)}, //
                interface_type->interface_node->name + "_dispatch__call"       //
            );
            call->setMetadata("comment",
                llvm::MDNode::get(context, llvm::MDString::get(context, "Call of dispatch function '" + call_node->function->name + "'")));
#ifndef __WIN32__
            call->addParamAttr(0, llvm::Attribute::InReg);
            if (OPTIMIZE_MODE != OptimizeMode::DEBUG) {
                // Add the 'tailcc' to every user-defined call
                call->setCallingConv(llvm::CallingConv::Tail);
                call->setTailCall();
            }
#endif
            llvm::Value *const call_is_err = builder.CreateICmpNE(call, llvm::ConstantPointerNull::get(PTR_TY), "call_is_err");
            last_err_values = {call_is_err, next_stack_frame};

            // Do all the common call cleanup on the arguments of the call
            if (!generate_call_arg_cleanup(                                                                         //
                    builder, ctx, args, garbage, std::nullopt, next_stack_frame, call_node->function->return_types, //
                    call_node->arguments, parameters, call_node->function->name)                                    //
            ) {
                return std::nullopt;
            }

            // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
            if (!call_node->has_catch) {
                generate_rethrow(builder, ctx, call_node, call_node->function->name);
            }

            // Extract all the return values from the call
            std::vector<llvm::Value *> return_value;
            llvm::Value *ret_ptr = nullptr;
            for (unsigned int i = 0; i < call_node->function->return_types.size(); i++) {
                if (i == 0) {
                    ret_ptr = builder.CreateGEP(type_map.at("type.ts.function"), next_stack_frame, builder.getInt32(1), "ret_ptr_0");
                }
                const std::shared_ptr<Type> &ret_type = call_node->function->return_types.at(i);
                const auto ret_pair = IR::get_type(ctx.parent->getParent(), ret_type);
                llvm::Type *const ret_ty = ret_pair.second.first ? PTR_TY : ret_pair.first;
                if (is_reference) {
                    return_value.emplace_back(ret_ptr);
                } else {
                    llvm::LoadInst *ret_value = IR::aligned_load(builder,                                                         //
                        ret_ty,                                                                                                   //
                        ret_ptr,                                                                                                  //
                        call_node->function->name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value" //
                    );
                    return_value.emplace_back(ret_value);
                }
                ret_ptr = builder.CreateGEP(ret_ty, setup_call, builder.getInt32(1), "ret_ptr_" + std::to_string(i + 1));
            }
            return return_value;
        }
        case Type::Variation::FUNC:
            [[fallthrough]];
        case Type::Variation::OBJECT:
            return generate_call(builder, ctx, static_cast<const CallNodeBase *>(call_node), is_reference);
    }
}

llvm::Value *Generator::Expression::generate_function_reference( //
    llvm::IRBuilder<> &builder,                                  //
    GenerationContext &ctx,                                      //
    const FunctionReferenceNode *ref_node                        //
) {
    // A function reference allocates as much memory on the heap as needed for the function frame, then stores the default frame of the
    // referenced function in that allocated frame and simply returns the pointer to the allocated frame.
    const size_t referenced_fn_id = ref_node->referenced_function->get_id();
    llvm::StructType *const called_fn_type = Module::ThreadStack::ts_frames.at(referenced_fn_id);
    llvm::GlobalVariable *const called_fn_default = Module::ThreadStack::ts_defaults.at(referenced_fn_id);
    llvm::Value *const frame_size = builder.getInt64(Allocation::get_type_size(ctx.parent->getParent(), called_fn_type));
    // The "actual frame size" is the frame size + 8 bytes. The first 8 bytes contain the pointer to the called function
    // To that we add the number of persistent local variables, one byte each, as *after* the fn frame the "is_initialized" flags of the
    // persistent locals will follow
    const size_t frame_additional_size = 8 + ref_node->referenced_function->persistent_count;
    llvm::Value *const actual_frame_size = builder.CreateAdd(frame_size, builder.getInt64(frame_additional_size));
    llvm::Value *const callable_frame = builder.CreateCall(c_functions.at(MALLOC), {actual_frame_size}, "callable_frame");
    // Zero-initialize the whole allocated memory (including the persistence bytes at the very end)
    builder.CreateCall(c_functions.at(MEMSET), {callable_frame, builder.getInt32(0), actual_frame_size});
    // Store the pointer to the function in the first 8 bytes of the callable frame
    llvm::Function *const fn_ptr = Function::function_contexts.at(referenced_fn_id).function;
    IR::aligned_store(builder, fn_ptr, callable_frame);
    // Store the rest of the function frame in the space after that
    llvm::Value *const default_fn_frame = IR::aligned_load(                                                //
        builder, called_fn_type, called_fn_default, ref_node->referenced_function->name + "_default_frame" //
    );
    llvm::Value *const fn_frame = builder.CreateGEP(PTR_TY, callable_frame, builder.getInt32(1), "fn_frame");
    IR::aligned_store(builder, default_fn_frame, fn_frame);
    return callable_frame;
}

void Generator::Expression::generate_rethrow( //
    llvm::IRBuilder<> &builder,               //
    GenerationContext &ctx,                   //
    const CallNodeBase *call_node,            //              const std::string &function_name //
    const std::string &function_name          //
) {
    // Create basic block for the catch block
    llvm::BasicBlock *current_block = builder.GetInsertBlock();
    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(                //
        context,                                                             //
        function_name + "_" + std::to_string(call_node->call_id) + "_catch", //
        ctx.parent                                                           //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(                //
        context,                                                             //
        function_name + "_" + std::to_string(call_node->call_id) + "_merge", //
        ctx.parent                                                           //
    );
    builder.SetInsertPoint(current_block);

    // Create the branching operation
    builder.CreateCondBr(last_err_values.first, catch_block, merge_block, IR::generate_weights(1, 100))
        ->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Branch to '" + catch_block->getName().str() + "' if '" + function_name + "' returned error")));

    // Generate the body of the catch block, it only contains re-throwing the error
    builder.SetInsertPoint(catch_block);
    // Copy of the generate_throw function
    {
        // Load error value
        llvm::StructType *error_type = type_map.at("type.flint.err");
        llvm::Value *const stack_frame = last_err_values.second;
        llvm::Value *const err_var = builder.CreateStructGEP(                                           //
            type_map.at("type.ts.function"), stack_frame, Module::ThreadStack::FUNCTION::ERR, "err_var" //
        );
        llvm::LoadInst *err_val = IR::aligned_load(builder,                   //
            error_type,                                                       //
            err_var,                                                          //
            function_name + "_" + std::to_string(call_node->call_id) + "_val" //
        );
        err_val->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context, "Load err val of call '" + function_name + "::" + std::to_string(call_node->call_id) + "'")));

        // Store the error value in the error field of the current function
        llvm::Value *error_ptr = builder.CreateStructGEP(                                                          //
            type_map.at("type.ts.function"), ctx.allocations.at("flint.stack"), Module::ThreadStack::FUNCTION::ERR //
        );
        IR::aligned_store(builder, err_val, error_ptr);
        if (ctx.is_global) {
            // For now just return 1 when a call in a global initializer scope failed
            // TODO: Add proper error printing etc like it exists when calling the main function
            builder.CreateRet(builder.getInt32(1));
        } else {
            builder.CreateRet(builder.getInt1(true));
        }
    }

    // Add branch to the merge block from the catch block if it does not contain a terminator (return or throw)
    if (catch_block->getTerminator() == nullptr) {
        builder.CreateBr(merge_block);
    }

    builder.SetInsertPoint(merge_block);
}

Generator::group_mapping Generator::Expression::generate_group_expression( //
    llvm::IRBuilder<> &builder,                                            //
    GenerationContext &ctx,                                                //
    garbage_type &garbage,                                                 //
    const unsigned int expr_depth,                                         //
    const GroupExpressionNode *group_node                                  //
) {
    std::vector<llvm::Value *> group_values;
    group_values.reserve(group_node->expressions.size());
    for (const auto &expr : group_node->expressions) {
        std::vector<llvm::Value *> expr_val = generate_expression(builder, ctx, garbage, expr_depth + 1, expr.get()).value();
        ASSERT(expr_val.size() == 1, "Nested groups are not allowed");
        ASSERT(expr_val.at(0) != nullptr);
        group_values.push_back(expr_val[0]);
    }
    return group_values;
}

Generator::group_mapping Generator::Expression::generate_range_expression( //
    llvm::IRBuilder<> &builder,                                            //
    GenerationContext &ctx,                                                //
    garbage_type &garbage,                                                 //
    const unsigned int expr_depth,                                         //
    const RangeExpressionNode *range_node                                  //
) {
    // We simply generate the lower and upper bound expressions and return their respecive results as one group mapping
    group_mapping lower_bound = generate_expression(builder, ctx, garbage, expr_depth, range_node->lower_bound.get());
    if (!lower_bound.has_value()) {
        return std::nullopt;
    }
    if (lower_bound.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    group_mapping upper_bound = generate_expression(builder, ctx, garbage, expr_depth, range_node->upper_bound.get());
    if (!upper_bound.has_value()) {
        return std::nullopt;
    }
    if (upper_bound.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    return std::vector<llvm::Value *>{lower_bound.value().front(), upper_bound.value().front()};
}

Generator::group_mapping Generator::Expression::generate_initializer( //
    llvm::IRBuilder<> &builder,                                       //
    GenerationContext &ctx,                                           //
    garbage_type &garbage,                                            //
    const unsigned int expr_depth,                                    //
    const InitializerNode *initializer                                //
) {
    switch (initializer->type->get_variation()) {
        default:
            // Unsupported initializer type
            return std::nullopt;
        case Type::Variation::DATA: {
            // Allocate space for the data
            llvm::Function *dima_allocate_fn = Module::DIMA::dima_functions.at("allocate");
            llvm::GlobalVariable *data_head = Module::DIMA::get_head(initializer->type);
            llvm::Value *data_ptr = builder.CreateCall(                                             //
                dima_allocate_fn, {data_head}, "initializer.data." + initializer->type->to_string() //
            );
            llvm::Type *struct_type = IR::get_type(ctx.parent->getParent(), initializer->type).first;

            for (unsigned int i = 0; i < initializer->args.size(); i++) {
                const auto &arg_expr = initializer->args.at(i);
                const std::shared_ptr<Type> &elem_type = arg_expr->type;
                const bool is_fixed_arr_init =                                                //
                    arg_expr->get_variation() == ExpressionNode::Variation::ARRAY_INITIALIZER //
                    && elem_type->get_variation() == Type::Variation::ARRAY                   //
                    && elem_type->as<ArrayType>()->sizes.has_value();
                if (is_fixed_arr_init) {
                    llvm::Value *field_ptr = builder.CreateStructGEP(struct_type, data_ptr, i, "field_ptr_" + std::to_string(i));
                    ctx.dest = builder.CreatePointerCast(field_ptr, PTR_TY);
                }

                auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, arg_expr.get());
                if (!expr_result.has_value()) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                // For initializers, we need pure single-value types
                if (expr_result.value().size() > 1) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }

                // Skip the rest on initializers for static arrays
                if (is_fixed_arr_init) {
                    ctx.dest = nullptr;
                    continue;
                }

                llvm::Value *expr_val = expr_result.value().front();
                llvm::Value *field_ptr = builder.CreateStructGEP(struct_type, data_ptr, i, "field_ptr_" + std::to_string(i));

                const bool is_initializer = arg_expr->get_variation() == ExpressionNode::Variation::INITIALIZER;
                const bool is_opt_literal =                                              //
                    arg_expr->type->get_variation() == Type::Variation::OPTIONAL         //
                    && arg_expr->get_variation() == ExpressionNode::Variation::TYPE_CAST //
                    && arg_expr->as<TypeCastNode>()->expr->get_variation() == ExpressionNode::Variation::LITERAL;
                bool is_slice = false;
                if (arg_expr->get_variation() == ExpressionNode::Variation::ARRAY_ACCESS) {
                    const auto *arg_arr_access = arg_expr->as<ArrayAccessNode>();
                    for (const auto &index : arg_arr_access->indexing_expressions) {
                        if (index->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
                            is_slice = true;
                            break;
                        }
                    }
                }
                const bool is_opaque = arg_expr->type->get_variation() == Type::Variation::OPAQUE;
                const bool is_call = arg_expr->get_variation() == ExpressionNode::Variation::CALL;
                // Check if the element is freeable. If it is then we need to clone it before storing it in the field. We also assign it
                // directly if it's an initalizer in itself, because then it has not been stored anywhere yet
                if (!elem_type->is_freeable() || is_initializer || is_opt_literal || is_slice || is_opaque || is_call) {
                    IR::aligned_store(builder, expr_val, field_ptr);
                    continue;
                }
                // It's a complex type and needs to be cloned
                llvm::Value *type_id = builder.getInt32(elem_type->get_id());
                llvm::Function *clone_fn = Memory::memory_functions.at("clone");
                builder.CreateCall(clone_fn, {expr_val, field_ptr, type_id});
            }
            return std::vector<llvm::Value *>{data_ptr};
        }
        case Type::Variation::OBJECT: {
            // Allocate space for the object
            llvm::Function *const dima_allocate_fn = Module::DIMA::dima_functions.at("allocate");
            llvm::GlobalVariable *const object_head = Module::DIMA::get_head(initializer->type);
            llvm::Value *const object_ptr = builder.CreateCall(                                         //
                dima_allocate_fn, {object_head}, "initializer.object." + initializer->type->to_string() //
            );
            llvm::Type *const struct_type = IR::get_type(ctx.parent->getParent(), initializer->type).first;

            for (size_t i = 0; i < initializer->args.size(); i++) {
                const auto &arg = initializer->args.at(i);
                auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, arg.get());
                if (!expr_result.has_value()) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                // For initializers, we need pure single-value types
                if (expr_result.value().size() > 1) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                // Do a GEP in the initialized object to be able to call `flint.clone`
                llvm::Value *const dest_ptr = builder.CreateStructGEP(struct_type, object_ptr, i, "object_init_" + std::to_string(i));
                // Call `flint.clone` to store the data in the object
                llvm::Function *const clone_fn = Memory::memory_functions.at("clone");
                llvm::Value *const expr_val = expr_result.value().front();
                builder.CreateCall(clone_fn, {expr_val, dest_ptr, builder.getInt32(arg->type->get_id())});
            }
            return std::vector<llvm::Value *>{object_ptr};
        }
        case Type::Variation::VECTOR: {
            // Create an "empty" vector of the vector-type
            llvm::Type *const vector_type = IR::get_type(ctx.parent->getParent(), initializer->type).first;
            if (initializer->args.size() == 1) {
                const auto &arg = initializer->args[0];
                ASSERT(arg->type->get_variation() == Type::Variation::VECTOR);
                const auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, arg.get());
                if (!expr_result.has_value()) {
                    return std::nullopt;
                }
                llvm::Value *const result = generate_type_cast(builder, ctx, expr_result.value().front(), arg->type, initializer->type);
                return std::vector<llvm::Value *>{result};
            }
            llvm::Value *initialized_value = IR::get_default_value_of_type(vector_type);
            for (unsigned int i = 0; i < initializer->args.size(); i++) {
                const auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, initializer->args.at(i).get());
                if (!expr_result.has_value()) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                // For initializers, we need pure single-value types
                if (expr_result.value().size() > 1) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                llvm::Value *expr_val = expr_result.value().front();
                initialized_value = builder.CreateInsertElement(initialized_value, expr_val, i, "mutt_init_elem_" + std::to_string(i));
            }
            return std::vector<llvm::Value *>{initialized_value};
        }
    }
}

Generator::group_mapping Generator::Expression::generate_optional_switch_expression( //
    llvm::IRBuilder<> &builder,                                                      //
    GenerationContext &ctx,                                                          //
    garbage_type &garbage,                                                           //
    const unsigned int expr_depth,                                                   //
    const SwitchExpression *switch_expression,                                       //
    llvm::Value *switch_value                                                        //
) {
    if (switch_expression->switcher->get_variation() != ExpressionNode::Variation::VARIABLE) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    const auto *switcher_var_node = switch_expression->switcher->as<VariableNode>();
    const unsigned int switcher_scope_id = ctx.scope->variables.at(switcher_var_node->name).scope_id;
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_expression->switcher->type, false);
    if (switch_value->getType()->isPointerTy()) {
        switch_value = IR::aligned_load(builder, opt_struct_type, switch_value, "loaded_rhs");
    }
    llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);

    // Get the current block
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Create the basic blocks for each branch
    std::vector<llvm::BasicBlock *> branch_blocks;
    branch_blocks.reserve(switch_expression->branches.size());
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "switch_expr_merge");
    llvm::BasicBlock *default_block = nullptr;
    const std::shared_ptr<Scope> original_scope = ctx.scope;
    int value_block_idx = -1;

    std::vector<std::pair<llvm::Value *, llvm::BasicBlock *>> phi_values;
    phi_values.reserve(switch_expression->branches.size());

    // First pass: create all branch blocks and detect default case
    for (size_t i = 0; i < switch_expression->branches.size(); i++) {
        const auto &branch = switch_expression->branches[i];

        // Check if it's the default branch (represented by "else")
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            if (default_block != nullptr) {
                // Two default blocks have been defined, only one is allowed
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "switch_expr_default", ctx.parent));
            default_block = branch_blocks[i];
        } else {
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "switch_expr_branch_" + std::to_string(i), ctx.parent));
        }

        // Generate the branch expression in its block
        builder.SetInsertPoint(branch_blocks[i]);

        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::SWITCH_MATCH) {
            const auto *match_node = branch.matches.front()->as<SwitchMatchNode>();
            const std::string var_str = "s" + std::to_string(branch.scope->scope_id) + "::" + match_node->name.value();
            llvm::Value *real_value_reference = builder.CreateStructGEP(opt_struct_type, var_alloca, 1, "value_reference");
            ctx.allocations.emplace(var_str, real_value_reference);
            value_block_idx = i;
        }
        ctx.scope = branch.scope;
        group_mapping branch_expr = generate_expression(builder, ctx, garbage, expr_depth + 1, branch.expr.get());
        if (!branch_expr.has_value() || branch_expr.value().empty()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *branch_value = branch_expr.value().front();

        // Store this value for the phi node
        phi_values.emplace_back(branch_value, builder.GetInsertBlock());

        // Add branch to merge block if this block doesn't already have a terminator
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(merge_block);
        }
    }

    // Return to the original block to generate the switch instruction
    builder.SetInsertPoint(pred_block);
    ctx.scope = original_scope;

    // Because it's a switch on an optional we can have a simple conditional branch here instead of the switch
    // We just check for the "has_value" field and select our result depending on what block we come from using a phi node
    llvm::Value *has_value_ptr = builder.CreateStructGEP(opt_struct_type, var_alloca, 0, "has_value_ptr");
    llvm::Value *has_value = IR::aligned_load(builder, builder.getInt1Ty(), has_value_ptr, "has_value");
    llvm::BasicBlock *has_value_block = branch_blocks.at(value_block_idx);
    // If value block idx == 1 none block is 0, if it's 0 the none block is idx 1
    llvm::BasicBlock *none_block = branch_blocks.at(1 - value_block_idx);
    builder.CreateCondBr(has_value, has_value_block, none_block);

    // Now set the insertion point to the merge block and select the correct expression depending on from which block we come
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);

    // Create the phi node to combine results from all branches
    llvm::PHINode *phi = builder.CreatePHI(phi_values[0].first->getType(), phi_values.size(), "switch_expr_result");

    // Add all the incoming values to the phi node
    for (const auto &[value, block] : phi_values) {
        phi->addIncoming(value, block);
    }

    // Return the phi node as the result of the expression
    std::vector<llvm::Value *> result = {phi};
    return result;
}

Generator::group_mapping Generator::Expression::generate_variant_switch_expression( //
    llvm::IRBuilder<> &builder,                                                     //
    GenerationContext &ctx,                                                         //
    garbage_type &garbage,                                                          //
    const unsigned int expr_depth,                                                  //
    const SwitchExpression *switch_expression,                                      //
    llvm::Value *switch_value                                                       //
) {
    // Get the current block
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Create the basic blocks for each branch
    std::vector<llvm::BasicBlock *> branch_blocks;
    branch_blocks.reserve(switch_expression->branches.size());
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "switch_expr_merge");
    llvm::BasicBlock *default_block = nullptr;
    const std::shared_ptr<Scope> original_scope = ctx.scope;

    // Values for the phi node (one from each branch)
    std::vector<std::pair<llvm::Value *, llvm::BasicBlock *>> phi_values;
    phi_values.reserve(switch_expression->branches.size());

    if (switch_expression->switcher->get_variation() != ExpressionNode::Variation::VARIABLE) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    const auto *switcher_var_node = switch_expression->switcher->as<VariableNode>();
    const unsigned int switcher_scope_id = ctx.scope->variables.at(switcher_var_node->name).scope_id;
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    llvm::StructType *variant_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_expression->switcher->type, false);
    if (switch_value->getType()->isPointerTy()) {
        switch_value = IR::aligned_load(builder, variant_struct_type, switch_value, "loaded_rhs");
    }
    switch_value = builder.CreateExtractValue(switch_value, {0}, "variant_flag");
    llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);

    // First pass: create all branch blocks and detect default case
    for (size_t i = 0; i < switch_expression->branches.size(); i++) {
        const auto &branch = switch_expression->branches[i];

        // Check if it's the default branch (represented by "else")
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            if (default_block != nullptr) {
                // Two default blocks have been defined, only one is allowed
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "switch_expr_default", ctx.parent));
            default_block = branch_blocks[i];
            continue;
        }
        branch_blocks.push_back(llvm::BasicBlock::Create(context, "switch_expr_branch_" + std::to_string(i), ctx.parent));

        // Generate the branch expression in its block
        builder.SetInsertPoint(branch_blocks[i]);
        const auto *match_node = branch.matches.front()->as<SwitchMatchNode>();

        if (match_node->name.has_value()) {
            // Add a reference to the 'value' of the variant to the block the switch expression takes place in
            const std::string var_str = "s" + std::to_string(branch.scope->scope_id) + "::" + match_node->name.value();
            llvm::Value *real_value_reference = builder.CreateStructGEP(variant_struct_type, var_alloca, 1, "value_reference");
            ctx.allocations.emplace(var_str, real_value_reference);
        }
        ctx.scope = branch.scope;

        // Create the actual expression of the branch and store it's result value in the phi_values vector
        group_mapping branch_expr = generate_expression(builder, ctx, garbage, expr_depth + 1, branch.expr.get());
        if (!branch_expr.has_value() || branch_expr.value().empty()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *branch_value = branch_expr.value().front();

        // Store this value for the phi node
        phi_values.emplace_back(branch_value, branch_blocks[i]);

        // Add branch to merge block if this block doesn't already have a terminator
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(merge_block);
        }
    }

    // Return to the original block to generate the switch instruction
    builder.SetInsertPoint(pred_block);
    ctx.scope = original_scope;

    // Create the switch instruction
    llvm::SwitchInst *switch_inst = nullptr;
    if (default_block == nullptr) {
        // If no default case, create one that produces a default value
        default_block = llvm::BasicBlock::Create(context, "switch_expr_implicit_default", ctx.parent);
        builder.SetInsertPoint(default_block);
        llvm::Value *default_value = IR::get_default_value_of_type(builder, ctx.parent->getParent(), switch_expression->type);
        phi_values.emplace_back(default_value, default_block);
        builder.CreateBr(merge_block);

        // Switch to default if no case matches
        builder.SetInsertPoint(pred_block);
        switch_inst = builder.CreateSwitch(switch_value, default_block, switch_expression->branches.size());
    } else {
        // We have an explicit default case
        switch_inst = builder.CreateSwitch(switch_value, default_block, switch_expression->branches.size() - 1);
    }

    switch_inst->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Switch expression")));

    // Add the cases to the switch instruction
    for (size_t i = 0; i < switch_expression->branches.size(); i++) {
        const auto &branch = switch_expression->branches[i];
        // Skip the default node
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            continue;
        }

        // Generate the case value
        const auto *match_node = branch.matches.front()->as<SwitchMatchNode>();
        switch_inst->addCase(builder.getInt8(match_node->id), branch_blocks[i]);
    }

    // Set insertion point to the merge block to create the phi node
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);

    // Create the phi node to combine results from all branches
    llvm::PHINode *phi = builder.CreatePHI(phi_values[0].first->getType(), phi_values.size(), "switch_expr_result");

    // Add all the incoming values to the phi node
    for (const auto &[value, block] : phi_values) {
        phi->addIncoming(value, block);
    }

    // Return the phi node as the result of the expression
    std::vector<llvm::Value *> result = {phi};
    return result;
}

Generator::group_mapping Generator::Expression::generate_switch_expression( //
    llvm::IRBuilder<> &builder,                                             //
    GenerationContext &ctx,                                                 //
    garbage_type &garbage,                                                  //
    const unsigned int expr_depth,                                          //
    const SwitchExpression *switch_expression                               //
) {
    // Generate the switch value (the value being switched on)
    group_mapping switch_value_mapping = generate_expression(builder, ctx, garbage, expr_depth + 1, switch_expression->switcher.get());
    if (!switch_value_mapping.has_value() || switch_value_mapping.value().empty()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    llvm::Value *switch_value = switch_value_mapping.value().front();

    switch (switch_expression->switcher->type->get_variation()) {
        case Type::Variation::OPTIONAL:
            return generate_optional_switch_expression(builder, ctx, garbage, expr_depth, switch_expression, switch_value);
        case Type::Variation::VARIANT:
            return generate_variant_switch_expression(builder, ctx, garbage, expr_depth, switch_expression, switch_value);
        default:
            break;
    }

    // Get the current block
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Create the basic blocks for each branch
    std::vector<llvm::BasicBlock *> branch_blocks;
    branch_blocks.reserve(switch_expression->branches.size());
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "switch_expr_merge");
    llvm::BasicBlock *default_block = nullptr;
    const std::shared_ptr<Scope> original_scope = ctx.scope;

    // Values for the phi node (one from each branch)
    std::vector<std::pair<llvm::Value *, llvm::BasicBlock *>> phi_values;
    phi_values.reserve(switch_expression->branches.size());

    // First pass: create all branch blocks and detect default case
    for (size_t i = 0; i < switch_expression->branches.size(); i++) {
        const auto &branch = switch_expression->branches[i];

        // Check if it's the default branch (represented by "_")
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            if (default_block != nullptr) {
                // Two default blocks have been defined, only one is allowed
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "switch_expr_default", ctx.parent));
            default_block = branch_blocks[i];
        } else {
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "switch_expr_branch_" + std::to_string(i), ctx.parent));
        }

        // Generate the branch expression in its block
        builder.SetInsertPoint(branch_blocks[i]);

        ctx.scope = branch.scope;
        group_mapping branch_expr = generate_expression(builder, ctx, garbage, expr_depth + 1, branch.expr.get());
        if (!branch_expr.has_value() || branch_expr.value().empty()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *branch_value = branch_expr.value().front();

        // Store this value for the phi node
        phi_values.emplace_back(branch_value, builder.GetInsertBlock());

        // Add branch to merge block if this block doesn't already have a terminator
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            builder.CreateBr(merge_block);
        }
    }

    // Return to the original block to generate the switch instruction
    builder.SetInsertPoint(pred_block);
    ctx.scope = original_scope;

    // Create the switch instruction
    llvm::SwitchInst *switch_inst = nullptr;
    if (switch_expression->switcher->type->get_variation() == Type::Variation::ERROR_SET) {
        switch_value = builder.CreateExtractValue(switch_value, {1}, "error_value");
    }
    if (default_block == nullptr) {
        // If no default case, create one that produces a default value
        default_block = llvm::BasicBlock::Create(context, "switch_expr_implicit_default", ctx.parent);
        builder.SetInsertPoint(default_block);
        llvm::Value *default_value = IR::get_default_value_of_type(builder, ctx.parent->getParent(), switch_expression->type);
        phi_values.emplace_back(default_value, default_block);
        builder.CreateBr(merge_block);

        // Switch to default if no case matches
        builder.SetInsertPoint(pred_block);
        switch_inst = builder.CreateSwitch(switch_value, default_block, switch_expression->branches.size());
    } else {
        // We have an explicit default case
        switch_inst = builder.CreateSwitch(switch_value, default_block, switch_expression->branches.size() - 1);
    }

    switch_inst->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Switch expression")));

    // Add the cases to the switch instruction
    for (size_t i = 0; i < switch_expression->branches.size(); i++) {
        const auto &branch = switch_expression->branches[i];
        // Skip the default node
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            continue;
        }

        // Generate the case value
        for (const auto &match : branch.matches) {
            if (match->get_variation() == ExpressionNode::Variation::LITERAL) {
                const auto *literal_node = match->as<LiteralNode>();
                if (std::holds_alternative<LitError>(literal_node->value)) {
                    const LitError &lit_err = std::get<LitError>(literal_node->value);
                    const auto *error_type = lit_err.error_type->as<ErrorSetType>();
                    const auto pair = error_type->error_node->get_id_msg_pair_of_value(lit_err.value);
                    ASSERT(pair.has_value());
                    switch_inst->addCase(builder.getInt32(pair.value().first), branch_blocks[i]);
                    continue;
                }
            }
            group_mapping case_expr = generate_expression(builder, ctx, garbage, expr_depth + 1, match.get());
            if (!case_expr.has_value() || case_expr.value().empty()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            llvm::Value *case_value = case_expr.value().front();

            // Add the case to the switch
            llvm::ConstantInt *const_case = llvm::dyn_cast<llvm::ConstantInt>(case_value);
            if (!const_case) {
                // Switch case value must be a constant integer
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }

            switch_inst->addCase(const_case, branch_blocks[i]);
        }
    }

    // Set insertion point to the merge block to create the phi node
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);

    // Create the phi node to combine results from all branches
    llvm::PHINode *phi = builder.CreatePHI(phi_values[0].first->getType(), phi_values.size(), "switch_expr_result");

    // Add all the incoming values to the phi node
    for (const auto &[value, block] : phi_values) {
        phi->addIncoming(value, block);
    }

    // Return the phi node as the result of the expression
    std::vector<llvm::Value *> result = {phi};
    return result;
}

llvm::Value *Generator::Expression::generate_inline_array_initializer( //
    llvm::IRBuilder<> &builder,                                        //
    GenerationContext &ctx,                                            //
    garbage_type &garbage,                                             //
    const unsigned int expr_depth,                                     //
    const InlineArrayInitializerNode *initializer                      //
) {
    // Generate all length expressions and store them into arr::idx::N
    std::vector<llvm::Value *> length_expressions;
    for (auto &expr : initializer->length_expressions) {
        group_mapping result = generate_expression(builder, ctx, garbage, expr_depth, expr.get());
        if (!result.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (result.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        llvm::Value *index_i64 = generate_type_cast(builder, ctx, result.value().front(), expr->type, Type::get_primitive_type("u64"));
        length_expressions.emplace_back(index_i64);
    }
    llvm::Value *const length_array = ctx.allocations.at("arr::idx::" + std::to_string(length_expressions.size()));
    for (size_t i = 0; i < length_expressions.size(); i++) {
        llvm::Value *array_element_ptr = builder.CreateGEP(builder.getInt64Ty(), length_array, builder.getInt64(i));
        IR::aligned_store(builder, length_expressions.at(i), array_element_ptr);
    }

    // Get element type info
    const llvm::DataLayout &data_layout = ctx.parent->getParent()->getDataLayout();
    const auto &elem_type_pair = IR::get_type(ctx.parent->getParent(), initializer->element_type);
    llvm::Type *llvm_element_type = initializer->element_type->is_dima_managed() ? PTR_TY : elem_type_pair.first;
    size_t element_size_in_bytes = data_layout.getTypeAllocSize(llvm_element_type);

    // Generate and cast each element value
    std::vector<llvm::Value *> element_values;
    for (auto &expr : initializer->initializer_values) {
        group_mapping result = generate_expression(builder, ctx, garbage, expr_depth, expr.get());
        if (!result.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (result.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        llvm::Value *value = result.value().front();
        if (!expr->type->equals(initializer->element_type)) {
            value = generate_type_cast(builder, ctx, value, expr->type, initializer->element_type);
        }
        element_values.emplace_back(value);
    }

    // Const/local array: ctx.dest is the stack alloca of the array LLVM type
    if (ctx.dest != nullptr) {
        // Bitcast to a flat element pointer so we can GEP with a single flat index
        for (size_t i = 0; i < element_values.size(); i++) {
            llvm::Value *element_ptr = builder.CreateGEP(                                                 //
                llvm_element_type, ctx.dest, builder.getInt64(i), "inline_init_elem_" + std::to_string(i) //
            );
            IR::aligned_store(builder, element_values[i], element_ptr);
        }
        return ctx.dest;
    }

    // Dynamic array: heap-allocate via create_arr
    llvm::Value *dim_val = builder.getInt64(length_expressions.size());
    llvm::CallInst *created_array = builder.CreateCall(        //
        Module::Array::array_manip_functions.at("create_arr"), //
        {
            dim_val,                                 // dimensionality
            builder.getInt64(element_size_in_bytes), // element_size
            length_array                             // lengths array
        },                                           //
        "created_array"                              //
    );
    created_array->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Create an array of type " + initializer->element_type->to_string() + "[" +
                    std::string(length_expressions.size() - 1, ',') + "]")));

    // Extract the data pointer from the str*: data_start = (char*)(str->value + str->len * sizeof(size_t))
    llvm::Type *const str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
    llvm::Value *const dim_lengths = builder.CreateStructGEP(str_type, created_array, 1, "arr_dim_lengths");
    llvm::Value *const loaded_dim = IR::aligned_load(                                                               //
        builder, builder.getInt64Ty(), builder.CreateStructGEP(str_type, created_array, 0, "len_ptr"), "loaded_dim" //
    );
    llvm::Value *const data_start = builder.CreateBitCast(                                     //
        builder.CreateGEP(builder.getInt64Ty(), dim_lengths, loaded_dim, "data_start"), PTR_TY //
    );

    // Store each element at the correct byte offset into the flat data buffer
    for (size_t i = 0; i < element_values.size(); i++) {
        llvm::Value *offset = builder.getInt64(i * element_size_in_bytes);
        llvm::Value *element_ptr = builder.CreateGEP(builder.getInt8Ty(), data_start, offset, "elem_" + std::to_string(i));
        IR::aligned_store(builder, element_values[i], element_ptr);
    }

    return created_array;
}

llvm::Value *Generator::Expression::generate_array_initializer( //
    llvm::IRBuilder<> &builder,                                 //
    GenerationContext &ctx,                                     //
    garbage_type &garbage,                                      //
    const unsigned int expr_depth,                              //
    const ArrayInitializerNode *initializer                     //
) {
    // First, generate all initializer expressions
    std::vector<llvm::Value *> length_expressions;
    for (auto &expr : initializer->length_expressions) {
        group_mapping result = generate_expression(builder, ctx, garbage, expr_depth, expr.get());
        if (!result.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (result.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        llvm::Value *index_i64 = generate_type_cast(builder, ctx, result.value().front(), expr->type, Type::get_primitive_type("u64"));
        length_expressions.emplace_back(index_i64);
    }
    llvm::Value *const length_array = ctx.allocations.at("arr::idx::" + std::to_string(length_expressions.size()));
    for (size_t i = 0; i < length_expressions.size(); i++) {
        llvm::Value *array_element_ptr = builder.CreateGEP(builder.getInt64Ty(), length_array, builder.getInt64(i));
        IR::aligned_store(builder, length_expressions.at(i), array_element_ptr);
    }
    const llvm::DataLayout &data_layout = ctx.parent->getParent()->getDataLayout();
    const auto &elem_type_pair = IR::get_type(ctx.parent->getParent(), initializer->element_type);
    llvm::Type *element_type = initializer->element_type->is_dima_managed() ? PTR_TY : elem_type_pair.first;
    size_t element_size_in_bytes = data_layout.getTypeAllocSize(element_type);

    // Generate the initializer expression (shared between const and dynamic paths)
    llvm::Value *initializer_expression = nullptr;
    bool is_default_init = initializer->initializer_value->get_variation() == ExpressionNode::Variation::DEFAULT;

    // For complex types (strings, data/structs), we require an explicit initializer
    // because default values would lead to uninitialized memory (e.g., invalid string pointers)
    if (is_default_init) {
        if (initializer->element_type->is_freeable()) {
            // Complex type requires explicit initializer
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        initializer_expression = IR::get_default_value_of_type(builder, ctx.parent->getParent(), initializer->element_type);
    } else {
        group_mapping initializer_mapping = generate_expression(builder, ctx, garbage, expr_depth, initializer->initializer_value.get());
        if (!initializer_mapping.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (initializer_mapping.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        initializer_expression = initializer_mapping.value().front();
        std::shared_ptr<Type> init_expr_type = initializer->initializer_value->type;
        if (!init_expr_type->equals(initializer->element_type)) {
            initializer_expression = generate_type_cast(builder, ctx, initializer_expression, init_expr_type, initializer->element_type);
        }
    }
    llvm::Value *type_id = initializer->element_type->is_freeable() //
        ? builder.getInt32(initializer->element_type->get_id())     //
        : builder.getInt32(0);
    llvm::Value *initializer_ptr = initializer_expression;
    if (!initializer_expression->getType()->isPointerTy()) {
        IR::aligned_store(builder, initializer_expression, scratchspace);
        initializer_ptr = scratchspace;
    }

    llvm::Function *fill_arr_fn = Module::Array::array_manip_functions.at("fill_arr");
    llvm::Value *dim_val = builder.getInt64(length_expressions.size());

    if (ctx.dest != nullptr) {
        // Const array: fill directly into destination memory using fill_arr
        llvm::CallInst *fill_call = builder.CreateCall(                            //
            fill_arr_fn,                                                           //
            {ctx.dest, dim_val, length_array,                                      //
                builder.getInt64(element_size_in_bytes), initializer_ptr, type_id} //
        );
        fill_call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Fill the fixed array")));
        return ctx.dest;
    }

    llvm::CallInst *created_array = builder.CreateCall(        //
        Module::Array::array_manip_functions.at("create_arr"), //
        {
            dim_val,                                 // dimensionality
            builder.getInt64(element_size_in_bytes), // element_size
            length_array                             // lengths array
        },                                           //
        "created_array"                              //
    );
    created_array->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Create an array of type " + initializer->element_type->to_string() + "[" +
                    std::string(length_expressions.size() - 1, ',') + "]")));

    // Extract the data pointer from the str*: data_start = (char*)(str->value + str->len * sizeof(size_t))
    llvm::Type *const str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
    llvm::Value *const dim_lengths = builder.CreateStructGEP(str_type, created_array, 1, "arr_dim_lengths");
    llvm::Value *const loaded_dim =
        IR::aligned_load(builder, builder.getInt64Ty(), builder.CreateStructGEP(str_type, created_array, 0, "len_ptr"), "loaded_dim");
    llvm::Value *const data_start =
        builder.CreateBitCast(builder.CreateGEP(builder.getInt64Ty(), dim_lengths, loaded_dim, "data_start"), PTR_TY);

    llvm::CallInst *fill_call = builder.CreateCall(                                                              //
        fill_arr_fn,                                                                                             //
        {data_start, loaded_dim, dim_lengths, builder.getInt64(element_size_in_bytes), initializer_ptr, type_id} //
    );
    fill_call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Fill the array")));
    return created_array;
}

llvm::Value *Generator::Expression::generate_array_access( //
    llvm::IRBuilder<> &builder,                            //
    GenerationContext &ctx,                                //
    garbage_type &garbage,                                 //
    const unsigned int expr_depth,                         //
    const ArrayAccessNode *access,                         //
    const bool is_reference                                //
) {
    std::optional<llvm::Value *> base_expr_value = std::nullopt;
    std::vector<const ExpressionNode *> indexing_exprs;
    for (const auto &expr : access->indexing_expressions) {
        indexing_exprs.emplace_back(expr.get());
    }
    return generate_array_access(                                                                                         //
        builder, ctx, garbage, expr_depth, base_expr_value, access->type, access->base_expr, indexing_exprs, is_reference //
    );
}

Generator::group_mapping Generator::Expression::generate_grouped_array_access( //
    llvm::IRBuilder<> &builder,                                                //
    GenerationContext &ctx,                                                    //
    garbage_type &garbage,                                                     //
    const unsigned int expr_depth,                                             //
    const GroupedArrayAccessNode *access,                                      //
    const bool is_reference                                                    //
) {
    // To generate a grouped array access we simply go through all "indexing expressions" and generate a regular array access at every
    // index, and then collect all the results in a simple list of values
    std::vector<llvm::Value *> results;
    const auto &base_expr_ty = access->base_expr->type;
    [[maybe_unused]] uint32_t dimensionality = 1;
    std::shared_ptr<Type> base_type{nullptr};
    if (base_expr_ty->to_string() == "str") {
        base_type = Type::get_primitive_type("u8");
    } else {
        const auto *arr_ty = base_expr_ty->as<ArrayType>();
        dimensionality = arr_ty->dimensionality;
        base_type = arr_ty->type;
    }
    for (const auto &expr : access->indexing_expressions) {
        std::vector<const ExpressionNode *> indexing_exprs;
        switch (expr->type->get_variation()) {
            default:
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            case Type::Variation::GROUP:
                ASSERT(expr->get_variation() == ExpressionNode::Variation::GROUP_EXPRESSION);
                for (const auto &ie : expr->as<GroupExpressionNode>()->expressions) {
                    indexing_exprs.emplace_back(ie.get());
                }
                break;
            case Type::Variation::PRIMITIVE:
                indexing_exprs.emplace_back(expr.get());
                break;
        }
        llvm::Value *const result = generate_array_access(                                                              //
            builder, ctx, garbage, expr_depth, std::nullopt, base_type, access->base_expr, indexing_exprs, is_reference //
        );
        results.emplace_back(result);
    }
    return results;
}

llvm::Value *Generator::Expression::generate_array_access(           //
    llvm::IRBuilder<> &builder,                                      //
    GenerationContext &ctx,                                          //
    garbage_type &garbage,                                           //
    const unsigned int expr_depth,                                   //
    std::optional<llvm::Value *> base_expr_value,                    //
    const std::shared_ptr<Type> result_type,                         //
    const std::unique_ptr<ExpressionNode> &base_expr,                //
    const std::vector<const ExpressionNode *> &indexing_expressions, //
    const bool is_reference                                          //
) {
    const bool is_slice = result_type->get_variation() == Type::Variation::ARRAY //
        || (result_type->to_string() == "str" && base_expr->type->to_string() == "str");
    // First, generate the index expressions
    std::vector<std::array<llvm::Value *, 2>> index_expressions;
    for (auto &index_expression : indexing_expressions) {
        group_mapping index = generate_expression(builder, ctx, garbage, expr_depth, index_expression);
        if (!index.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (index.value().size() > 2) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (index_expression->type->get_variation() == Type::Variation::GROUP) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        std::array<llvm::Value *, 2> index_expr;
        std::shared_ptr<Type> from_type = index_expression->type;
        std::shared_ptr<Type> to_type = Type::get_primitive_type("u64");
        if (index_expression->type->get_variation() == Type::Variation::RANGE) {
            const auto *range_type = index_expression->type->as<RangeType>();
            from_type = range_type->bound_type;
            index_expr = {index.value().front(), index.value().at(1)};
        } else {
            index_expr = {index.value().front(), nullptr};
        }
        if (!from_type->equals(to_type)) {
            index_expr[0] = generate_type_cast(builder, ctx, index_expr[0], from_type, to_type);
            if (index_expr[1] != nullptr) {
                index_expr[1] = generate_type_cast(builder, ctx, index_expr[1], from_type, to_type);
            }
        }
        index_expressions.emplace_back(index_expr);
    }
    // Then, generate the base expression
    llvm::Value *array_ptr = nullptr;
    if (base_expr_value.has_value()) {
        array_ptr = base_expr_value.value();
    } else {
        const bool base_is_static =                                    //
            base_expr->type->get_variation() == Type::Variation::ARRAY //
            && base_expr->type->as<ArrayType>()->sizes.has_value();
        group_mapping base_expression = generate_expression(builder, ctx, garbage, expr_depth, base_expr.get(), base_is_static);
        if (!base_expression.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (base_expression.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        array_ptr = base_expression.value().front();
    }
    if (base_expr->type->to_string() == "str") {
        // "Array" accesses on strings dont need all the things below, they are much simpler to handle
        if (index_expressions.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (is_slice) {
            llvm::Function *get_str_slice_fn = Module::String::string_manip_functions.at("get_str_slice");
            return builder.CreateCall(get_str_slice_fn, {array_ptr, index_expressions.front()[0], index_expressions.front()[1]});
        } else {
            llvm::Function *access_str_at_fn = Module::String::string_manip_functions.at("access_str_at");
            return builder.CreateCall(access_str_at_fn, {array_ptr, index_expressions.front()[0]});
        }
    }
    const size_t idx_size = indexing_expressions.size() * (static_cast<size_t>(is_slice) + 1);
    llvm::Value *const temp_array_indices = ctx.allocations.at("arr::idx::" + std::to_string(idx_size));
    // Save all the indices in the temp array
    for (size_t i = 0; i < index_expressions.size(); i++) {
        if (!is_slice) {
            llvm::Value *index_ptr = builder.CreateInBoundsGEP(                                                    //
                builder.getInt64Ty(), temp_array_indices, builder.getInt64(i), "idx_" + std::to_string(i) + "_ptr" //
            );
            llvm::StoreInst *index_store = IR::aligned_store(builder, index_expressions.at(i)[0], index_ptr);
            index_store->setMetadata("comment",                                                                       //
                llvm::MDNode::get(context, llvm::MDString::get(context, "Save the index of id " + std::to_string(i))) //
            );
            continue;
        }
        const bool is_range = indexing_expressions.at(i)->type->get_variation() == Type::Variation::RANGE;
        for (size_t j = 0; j < 1 + static_cast<size_t>(is_range); j++) {
            llvm::Value *index_ptr = builder.CreateInBoundsGEP(                                                                    //
                builder.getInt64Ty(), temp_array_indices, builder.getInt64(i * 2 + j), "idx_" + std::to_string(i * 2 + j) + "_ptr" //
            );
            llvm::StoreInst *index_store = IR::aligned_store(builder, index_expressions.at(i)[j], index_ptr);
            index_store->setMetadata("comment",                                                                               //
                llvm::MDNode::get(context, llvm::MDString::get(context, "Save the index of id " + std::to_string(i * 2 + j))) //
            );
            if (is_slice && !is_range) {
                // The slicing function expects indices of non-ranges to be '1, 1' for the index 1, and '1, 3' for the range [1, 3)
                index_ptr = builder.CreateInBoundsGEP(                                                                                 //
                    builder.getInt64Ty(), temp_array_indices, builder.getInt64(i * 2 + 1), "idx_" + std::to_string(i * 2 + 1) + "_ptr" //
                );
                index_store = IR::aligned_store(builder, index_expressions.at(i)[0], index_ptr);
                index_store->setMetadata("comment",                                                                               //
                    llvm::MDNode::get(context, llvm::MDString::get(context, "Save the index of id " + std::to_string(i * 2 + 1))) //
                );
            }
        }
    }
    const auto elem_type_pair = IR::get_type(ctx.parent->getParent(), result_type);
    llvm::Type *element_type = elem_type_pair.second.first ? PTR_TY : elem_type_pair.first;
    if (is_slice) {
        element_type = IR::get_type(ctx.parent->getParent(), result_type->as<ArrayType>()->type).first;
    }
    const size_t element_size_in_bytes = Allocation::get_type_size(ctx.parent->getParent(), element_type);
    switch (result_type->get_variation()) {
        default:
            // Non-supported type for array access
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        case Type::Variation::DATA:
        case Type::Variation::ENUM:
        case Type::Variation::PRIMITIVE:
        case Type::Variation::TUPLE:
        case Type::Variation::VECTOR: {
            ASSERT(base_expr->type->get_variation() == Type::Variation::ARRAY);
            const ArrayType *base_arr_type = base_expr->type->as<ArrayType>();
            llvm::Function *const access_arr_fn = Module::Array::array_manip_functions.at("access_arr");
            llvm::Value *const arr_element_size = builder.getInt64(element_size_in_bytes);
            llvm::Value *arr_dim = nullptr;
            llvm::Value *arr_dim_lengths = nullptr;
            llvm::Value *arr_data = nullptr;
            if (base_arr_type->sizes.has_value()) {
                // It's a static array
                arr_dim = builder.getInt64(base_arr_type->sizes.value().size());
                for (size_t i = 0; i < base_arr_type->sizes.value().size(); i++) {
                    llvm::Value *const arr_len_i_ptr = builder.CreateGEP(                                                //
                        builder.getInt64Ty(), scratchspace, builder.getInt64(i), "arr_len_" + std::to_string(i) + "_ptr" //
                    );
                    IR::aligned_store(builder, builder.getInt64(base_arr_type->sizes.value().at(i)), arr_len_i_ptr);
                }
                arr_dim_lengths = scratchspace;
                arr_data = builder.CreateBitCast(array_ptr, PTR_TY, "arr_data");
            } else {
                // It's a dynamic array
                llvm::Type *const str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
                llvm::Value *const len_ptr = builder.CreateStructGEP(str_type, array_ptr, 0, "len_ptr");
                arr_dim = IR::aligned_load(builder, builder.getInt64Ty(), len_ptr, "arr_dim");
                arr_dim_lengths = builder.CreateStructGEP(str_type, array_ptr, 1, "arr_dim_lengths");
                arr_data = builder.CreateGEP(builder.getInt64Ty(), arr_dim_lengths, arr_dim, "arr_data");
            }
            llvm::Value *const elem_ptr = builder.CreateCall(                                                               //
                access_arr_fn, {arr_element_size, arr_data, arr_dim, arr_dim_lengths, temp_array_indices}, "access_arr_ptr" //
            );
            if (is_reference && result_type->get_variation() != Type::Variation::DATA) {
                // Always load data as it's stored as pointers within the array
                return elem_ptr;
            }
            return IR::aligned_load(builder, element_type, elem_ptr, "access_arr_value");
        }
        case Type::Variation::ARRAY: {
            // This is a slicing operation
            llvm::Value *result = builder.CreateCall(Module::Array::array_manip_functions.at("get_arr_slice"), //
                {array_ptr, builder.getInt64(element_size_in_bytes), temp_array_indices}                       //
            );
            return result;
        }
    }
}

llvm::Value *Generator::Expression::get_bool8_element_at(llvm::IRBuilder<> &builder, llvm::Value *b8_val, unsigned int elem_idx) {
    // Shift right by i bits and AND with 1 to extract the i-th bit
    llvm::Value *bit_i = builder.CreateAnd(builder.CreateLShr(b8_val, builder.getInt8(elem_idx)), builder.getInt8(1), "extract_bit");
    // Convert the result (i8 with value 0 or 1) to i1 (boolean)
    llvm::Value *bool_bit = builder.CreateTrunc(bit_i, builder.getInt1Ty(), "to_bool");
    return bool_bit;
}

llvm::Value *Generator::Expression::set_bool8_element_at( //
    llvm::IRBuilder<> &builder,                           //
    llvm::Value *b8_val,                                  //
    llvm::Value *bit_value,                               //
    unsigned int elem_idx                                 //
) {
    // Create a bit mask for the specific position (this is a compile-time constant)
    llvm::Value *bit_mask = builder.getInt8(1 << elem_idx);
    // Create the inverse mask (also compile-time)
    llvm::Value *inverse_mask = builder.getInt8(~(1 << elem_idx));
    // Check if the bit value is true
    llvm::Value *is_true = builder.CreateICmpNE(bit_value, builder.getFalse(), "is_true");
    // If bit_value is true, OR with mask to set the bit
    llvm::Value *set_value = builder.CreateOr(b8_val, bit_mask, "set_bit");
    // If bit_value is false, AND with inverse mask to clear the bit
    llvm::Value *clear_value = builder.CreateAnd(b8_val, inverse_mask, "clear_bit");
    // Select the appropriate value based on whether bit_value is true
    return builder.CreateSelect(is_true, set_value, clear_value, "new_value");
}

Generator::group_mapping Generator::Expression::generate_data_access( //
    llvm::IRBuilder<> &builder,                                       //
    GenerationContext &ctx,                                           //
    garbage_type &garbage,                                            //
    const unsigned int expr_depth,                                    //
    const DataAccessNode *data_access,                                //
    const bool is_reference                                           //
) {
    // First, generate the base expression to get the value of the data variable
    const bool base_is_ref = is_reference && data_access->base_expr->type->get_variation() == Type::Variation::FUNC;
    group_mapping base_expr = generate_expression(builder, ctx, garbage, expr_depth, data_access->base_expr.get(), base_is_ref);
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    if (base_expr.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    llvm::Value *expr_val = base_expr.value().front();
    // Get the type of the data variable to access
    if (data_access->base_expr->type->to_string() == "str") {
        if (data_access->field_name != "length" && data_access->field_name != "len") {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
        llvm::Value *length_ptr = builder.CreateStructGEP(str_type, expr_val, 0, "length_ptr");
        llvm::Value *length = IR::aligned_load(builder, builder.getInt64Ty(), length_ptr, "length");
        return std::vector<llvm::Value *>{length};
    }
    if (data_access->base_expr->type->to_string() == "anyerror") {
        expr_val = builder.CreateExtractValue(expr_val, data_access->field_id, "anyerror_id_" + std::to_string(data_access->field_id));
        return std::vector<llvm::Value *>{expr_val};
    }

    switch (data_access->base_expr->type->get_variation()) {
        default:
            break;
        case Type::Variation::ARRAY: {
            const auto *array_type = data_access->base_expr->type->as<ArrayType>();
            if (data_access->field_name != "length" && data_access->field_name != "len") {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            ASSERT(!is_reference);
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
            llvm::Value *length_ptr = builder.CreateStructGEP(str_type, expr_val, 1, "length_ptr");
            std::vector<llvm::Value *> length_values;
            for (size_t i = 0; i < array_type->dimensionality; i++) {
                llvm::Value *actual_length_ptr = builder.CreateGEP(builder.getInt64Ty(), length_ptr, builder.getInt64(i));
                llvm::Value *length_value = IR::aligned_load(                                             //
                    builder, builder.getInt64Ty(), actual_length_ptr, "length_value_" + std::to_string(i) //
                );
                length_values.emplace_back(length_value);
            }
            return length_values;
        }
        case Type::Variation::VECTOR: {
            const auto *vector_type = data_access->base_expr->type->as<VectorType>();
            ASSERT(!is_reference);
            std::vector<llvm::Value *> values;
            if (vector_type->base_type->to_string() == "bool") {
                // Special case for accessing an "element" on a bool8 type
                values.emplace_back(get_bool8_element_at(builder, expr_val, data_access->field_id));
            } else {
                values.emplace_back(builder.CreateExtractElement(expr_val, data_access->field_id));
            }
            return values;
        }
        case Type::Variation::OBJECT: {
            llvm::Type *object_type = IR::get_type(ctx.parent->getParent(), data_access->base_expr->type).first;
            llvm::Value *data_ptr = builder.CreateStructGEP(object_type, expr_val, data_access->field_id, "object_data_ptr_gep");
            if (!is_reference) {
                data_ptr = IR::aligned_load(builder, PTR_TY, data_ptr, "object_data_ptr");
            }
            std::vector<llvm::Value *> values = {data_ptr};
            return values;
        }
        case Type::Variation::ERROR_SET: {
            std::vector<llvm::Value *> values;
            values.emplace_back(builder.CreateExtractValue(expr_val, data_access->field_id, "err_field_val"));
            return values;
        }
        case Type::Variation::FUNC: {
            std::vector<llvm::Value *> values;
            if (is_reference) {
                llvm::Type *func_type = IR::get_type(ctx.parent->getParent(), data_access->base_expr->type).first;
                values.emplace_back(builder.CreateStructGEP(func_type, expr_val, data_access->field_id, "func_data_ptr_ref"));
            } else {
                values.emplace_back(builder.CreateExtractValue(expr_val, data_access->field_id, "func_data_ptr"));
            }
            return values;
        }
        case Type::Variation::TUPLE: {
            std::vector<llvm::Value *> values;
            values.emplace_back(builder.CreateExtractValue(expr_val, data_access->field_id, "tuple_field_val"));
            return values;
        }
        case Type::Variation::VARIANT: {
            std::vector<llvm::Value *> values;
            values.emplace_back(builder.CreateExtractValue(expr_val, data_access->field_id, "variant_field_val"));
            return values;
        }
    }
    if (!expr_val) {
        std::cerr << "ERROR: expr_val is null" << std::endl;
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    std::vector<llvm::Value *> values;
    auto type = IR::get_type(ctx.parent->getParent(), data_access->base_expr->type);
    llvm::Value *elem_ptr = builder.CreateStructGEP(                                                     //
        type.first, expr_val, data_access->field_id, "elem_ptr_" + std::to_string(data_access->field_id) //
    );
    // If this expression is requested as a reference then we return the pointer to the element if the element is not complex in of itself
    // (like data, strings etc) as these values themselves point somewhere
    auto elem_type = IR::get_type(ctx.parent->getParent(), data_access->type);
    if (is_reference) {
        values.emplace_back(elem_ptr);
    } else {
        values.emplace_back(IR::aligned_load(                           //
            builder, elem_type.second.first ? PTR_TY : elem_type.first, //
            elem_ptr, "elem_" + std::to_string(data_access->field_id)   //
            ));
    }
    return values;
}

Generator::group_mapping Generator::Expression::generate_grouped_data_access( //
    llvm::IRBuilder<> &builder,                                               //
    GenerationContext &ctx,                                                   //
    garbage_type &garbage,                                                    //
    const unsigned int expr_depth,                                            //
    const GroupedDataAccessNode *grouped_data_access                          //
) {
    // First, generate the base expression
    group_mapping base_expr = generate_expression(builder, ctx, garbage, expr_depth, grouped_data_access->base_expr.get());
    if (!base_expr.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    if (base_expr.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    llvm::Value *expr = base_expr.value().front();
    const auto *group_type = grouped_data_access->type->as<GroupType>();

    std::vector<llvm::Value *> return_values;
    return_values.reserve(group_type->types.size());
    // Its a grouped access on a bool8 variable, we need to handle this specially
    if (grouped_data_access->base_expr->type->to_string() == "bool8") {
        for (auto it = grouped_data_access->field_ids.begin(); it != grouped_data_access->field_ids.end(); ++it) {
            return_values.push_back(get_bool8_element_at(builder, expr, *it));
        }
        return return_values;
    }
    // Check if expr_val is a pointer type
    if (expr && expr->getType()->isPointerTy()) {
        llvm::Type *struct_value_type = IR::get_type(ctx.parent->getParent(), grouped_data_access->base_expr->type).first;
        expr = IR::aligned_load(builder, struct_value_type, expr);
    }
    // Its a normal grouped data access
    const bool is_vector_type = expr->getType()->isVectorTy();
    if (!is_vector_type) {
        ASSERT(expr->getType()->isStructTy());
    }
    for (size_t i = 0; i < grouped_data_access->field_names.size(); i++) {
        const unsigned int id = grouped_data_access->field_ids.at(i);
        llvm::Value *field_val = nullptr;
        if (is_vector_type) {
            field_val = builder.CreateExtractElement(expr, id, "group_" + grouped_data_access->field_names.at(i) + "_val");
        } else {
            field_val = builder.CreateExtractValue(expr, id, "group_" + grouped_data_access->field_names.at(i) + "_val");
        }
        if (group_type->types.at(i)->is_freeable()) {
            llvm::Function *clone_fn = Memory::memory_functions.at("clone");
            llvm::Value *type_id = builder.getInt32(group_type->types.at(i)->get_id());
            builder.CreateCall(clone_fn, {field_val, scratchspace, type_id});
            field_val = IR::aligned_load(builder, PTR_TY, scratchspace, "cloned_" + grouped_data_access->field_names.at(i));
        }
        return_values.push_back(field_val);
    }
    return return_values;
}

Generator::group_mapping Generator::Expression::generate_optional_chain( //
    llvm::IRBuilder<> &builder,                                          //
    GenerationContext &ctx,                                              //
    garbage_type &garbage,                                               //
    const unsigned int expr_depth,                                       //
    const OptionalChainNode *chain                                       //
) {
    // First we need to create all the basic blocks the optional chain needs. This includes one "happy path" block for when the chain
    // has a value and one single "bad path" block for when the chain short-circuits.
    const bool is_toplevel_chain = !ctx.short_circuit_block.has_value();
    llvm::Type *result_type = IR::get_type(ctx.parent->getParent(), chain->type).first;
    if (is_toplevel_chain) {
        // We add the short_circuit_block at the very end of the generation function to the body of this generation function
        ctx.short_circuit_block = llvm::BasicBlock::Create(context, "short_circuit");
    }

    // Now let's run the base expression and then do the operation on it (access, array access)
    group_mapping expr_result = generate_expression(builder, ctx, garbage, expr_depth, chain->base_expr.get());
    if (!expr_result.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    // Because there does not exist such thing as a 'optional group' in Flint, the base expression must have a size of 1 element
    if (expr_result.value().size() != 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    llvm::Value *base_expr = expr_result.value().front();

    // We need to check whether the base expression even *has* a value stored in it and then we need to branch depending on whether it
    // has a value
    llvm::Value *has_value = builder.CreateExtractValue(base_expr, {0}, "base_expr_has_value");
    // Now we branch depending on whether the base expression has a value
    llvm::BasicBlock *happy_path = llvm::BasicBlock::Create(context, "happy_path", ctx.parent);
    builder.CreateCondBr(has_value, happy_path, ctx.short_circuit_block.value());

    // Do the field or array access
    builder.SetInsertPoint(happy_path);
    llvm::Value *base_expr_value = builder.CreateExtractValue(base_expr, {1}, "base_expr_value");
    llvm::Value *result_value = IR::get_default_value_of_type(result_type);
    result_value = builder.CreateInsertValue(result_value, builder.getInt1(true), {0});
    if (std::holds_alternative<ChainFieldAccess>(chain->operation)) {
        const ChainFieldAccess &access = std::get<ChainFieldAccess>(chain->operation);
        const auto *base_expr_type = chain->base_expr->type->as<OptionalType>();
        llvm::Type *data_type = IR::get_type(ctx.parent->getParent(), base_expr_type->base_type).first;
        llvm::Type *field_type = nullptr;
        if (chain->is_toplevel_chain_node) {
            const auto *optional_result_type = chain->type->as<OptionalType>();
            field_type = IR::get_type(ctx.parent->getParent(), optional_result_type->base_type).first;
        } else {
            field_type = IR::get_type(ctx.parent->getParent(), chain->type).first;
        }

        llvm::Value *opt_value_ptr = builder.CreateStructGEP(data_type, base_expr_value, access.field_id, "opt_value_ptr");
        llvm::Value *opt_value = IR::aligned_load(builder, field_type, opt_value_ptr, "opt_value");
        result_value = builder.CreateInsertValue(result_value, opt_value, {1}, "filled_result");
    } else if (std::holds_alternative<ChainArrayAccess>(chain->operation)) {
        const ChainArrayAccess &access = std::get<ChainArrayAccess>(chain->operation);
        const auto *base_expr_type = chain->base_expr->type->as<OptionalType>();
        const auto *base_array_type = base_expr_type->base_type->as<ArrayType>();
        std::vector<const ExpressionNode *> indexing_exprs;
        for (const auto &expr : access.indexing_expressions) {
            indexing_exprs.emplace_back(expr.get());
        }
        llvm::Value *opt_value = generate_array_access(                                                                 //
            builder, ctx, garbage, expr_depth, base_expr_value, base_array_type->type, chain->base_expr, indexing_exprs //
        );
        result_value = builder.CreateInsertValue(result_value, opt_value, {1}, "filled_result");
    }

    if (!is_toplevel_chain) {
        // Return our accessed value directly now, as we do not do any further "return-evaluation" if we are not the toplevel
        return std::vector<llvm::Value *>{result_value};
    }

    // It's definitely the top-level chain element
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge_block");
    builder.CreateBr(merge_block);

    // Now we can create the short-circuit block which only contains a branch to the merge block, because the default-value is a simple
    // zeroinitializer
    ctx.short_circuit_block.value()->insertInto(ctx.parent);
    builder.SetInsertPoint(ctx.short_circuit_block.value());
    llvm::Value *sc_value = IR::get_default_value_of_type(result_type);
    builder.CreateBr(merge_block);

    // Now we need to select our value depending on which block we come from
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);
    llvm::PHINode *selected_value = builder.CreatePHI(result_type, 2, "selected_value");
    selected_value->addIncoming(result_value, happy_path);
    selected_value->addIncoming(sc_value, ctx.short_circuit_block.value());

    // Lastly reset the short-circuit block
    ctx.short_circuit_block = std::nullopt;
    return std::vector<llvm::Value *>{selected_value};
}

Generator::group_mapping Generator::Expression::generate_optional_unwrap( //
    llvm::IRBuilder<> &builder,                                           //
    GenerationContext &ctx,                                               //
    garbage_type &garbage,                                                //
    const unsigned int expr_depth,                                        //
    const OptionalUnwrapNode *unwrap,                                     //
    const bool is_reference                                               //
) {
    ASSERT(unwrap->base_expr->type->get_variation() == Type::Variation::OPTIONAL);
    auto base_expressions = generate_expression(builder, ctx, garbage, expr_depth + 1, unwrap->base_expr.get(), is_reference);
    if (!base_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // For now, we assume that the base expression is not a group type
    ASSERT(base_expressions.value().size() == 1);
    llvm::Value *base_expr = base_expressions.value().front();
    if (base_expr->getType()->isPointerTy()) {
        llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), unwrap->base_expr->type, false);
        base_expr = IR::aligned_load(builder, opt_struct_type, base_expr, "loaded_base_expr");
    }
    if (opt_unwrap_mode == OptionalUnwrapMode::UNSAFE) {
        // Directly unwrap the value when in unsafe mode, possibly breaking stuff, but it's much faster too
        if (is_reference) {
            base_expr = builder.CreateStructGEP(base_expr->getType(), base_expr, 1, "opt_value_unsafe");
        } else {
            base_expr = builder.CreateExtractValue(base_expr, {1}, "opt_value_unsafe");
        }
        return std::vector<llvm::Value *>{base_expr};
    }
    // First, check if the optional even has a value at all
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    llvm::BasicBlock *has_no_value = llvm::BasicBlock::Create(context, "opt_upwrap_no_value", ctx.parent);
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "opt_upwrap_value", ctx.parent);
    builder.SetInsertPoint(inserter);
    llvm::Value *opt_has_value = nullptr;
    if (is_reference) {
        llvm::Value *opt_has_value_ptr = builder.CreateStructGEP(base_expr->getType(), base_expr, 0, "opt_has_value_ptr");
        opt_has_value = IR::aligned_load(builder, builder.getInt1Ty(), opt_has_value_ptr, "opt_has_value");
    } else {
        opt_has_value = builder.CreateExtractValue(base_expr, {0}, "opt_has_value");
    }
    llvm::BranchInst *branch = builder.CreateCondBr(opt_has_value, merge, has_no_value, IR::generate_weights(100, 1));
    branch->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Check if the 'has_value' property is true")));

    // The crash block, in the case of a bad optional access
    builder.SetInsertPoint(has_no_value);
    llvm::Value *err_msg = IR::generate_const_string(ctx.parent->getParent(), "Bad optional access occurred\n");
    builder.CreateCall(c_functions.at(PRINTF), {err_msg});
    builder.CreateCall(c_functions.at(ABORT), {});
    builder.CreateUnreachable();

    // The merge block, when the optional access was okay
    builder.SetInsertPoint(merge);
    if (is_reference) {
        base_expr = builder.CreateStructGEP(base_expr->getType(), base_expr, 1, "opt_value");
    } else {
        base_expr = builder.CreateExtractValue(base_expr, {1}, "opt_value");
    }
    return std::vector<llvm::Value *>{base_expr};
}

Generator::group_mapping Generator::Expression::generate_variant_extraction( //
    llvm::IRBuilder<> &builder,                                              //
    GenerationContext &ctx,                                                  //
    const VariantExtractionNode *extraction                                  //
) {
    const auto *variable_node = extraction->base_expr->as<VariableNode>();
    const unsigned int variable_decl_scope = ctx.scope->variables.at(variable_node->name).scope_id;
    llvm::Value *const variable = ctx.allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);
    const auto *result_type_ptr = extraction->type->as<OptionalType>();
    const std::shared_ptr<Type> &extract_type_ptr = result_type_ptr->base_type;
    llvm::Type *const element_type = IR::get_type(ctx.parent->getParent(), extract_type_ptr).first;
    llvm::Type *const variant_type = IR::get_type(ctx.parent->getParent(), extraction->base_expr->type).first;

    // First, check if the variant holds a value of our wanted type
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    llvm::BasicBlock *holds_wrong_type = llvm::BasicBlock::Create(context, "var_extract_wrong_type", ctx.parent);
    llvm::BasicBlock *holds_correct_type = llvm::BasicBlock::Create(context, "var_extract_correct_type", ctx.parent);
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "var_extract_merge", ctx.parent);
    builder.SetInsertPoint(inserter);
    const uint8_t id = extraction->extracted_id;
    llvm::Value *wanted_type = builder.getInt8(id);
    llvm::Value *current_type_ptr = builder.CreateStructGEP(variant_type, variable, 0, "var_type_ptr");
    llvm::Value *current_type = IR::aligned_load(builder, builder.getInt8Ty(), current_type_ptr, "var_type");
    llvm::Value *holds_type = builder.CreateICmpEQ(current_type, wanted_type, "holds_type");
    llvm::BranchInst *branch = builder.CreateCondBr(holds_type, holds_correct_type, holds_wrong_type);
    branch->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Check if the variant holds the correct type")));

    llvm::Type *opt_type = IR::get_type(ctx.parent->getParent(), extraction->type).first;

    // The none block, in the case the variant does not hold the requested type
    builder.SetInsertPoint(holds_wrong_type);
    llvm::Value *value_nok = IR::get_default_value_of_type(opt_type);
    builder.CreateBr(merge);

    // The OK block, in the case the variant does contain the correct type, the value of the variant needs to be stored in an optional
    // wrapper
    builder.SetInsertPoint(holds_correct_type);
    llvm::Value *value_raw_ptr = builder.CreateStructGEP(variant_type, variable, 1, "value_raw_ptr");
    llvm::Value *value_ptr = builder.CreateBitCast(value_raw_ptr, PTR_TY, "value_ptr");
    llvm::Value *value = IR::aligned_load(builder, element_type, value_ptr, "value");
    llvm::Value *value_ok = IR::get_default_value_of_type(opt_type);
    value_ok = builder.CreateInsertValue(value_ok, builder.getInt1(true), {0}, "value_ok_fill_has_value");
    value_ok = builder.CreateInsertValue(value_ok, value, {1}, "value_ok_fill_value");
    builder.CreateBr(merge);

    // The merge block, where we select the correct value depending on the accesses success
    builder.SetInsertPoint(merge);
    llvm::PHINode *phi_node = builder.CreatePHI(opt_type, 2, "selected_value");
    phi_node->addIncoming(value_nok, holds_wrong_type);
    phi_node->addIncoming(value_ok, holds_correct_type);
    return std::vector<llvm::Value *>{phi_node};
}

Generator::group_mapping Generator::Expression::generate_variant_unwrap( //
    llvm::IRBuilder<> &builder,                                          //
    GenerationContext &ctx,                                              //
    const VariantUnwrapNode *unwrap                                      //
) {
    const auto *variable_node = unwrap->base_expr->as<VariableNode>();
    const unsigned int variable_decl_scope = ctx.scope->variables.at(variable_node->name).scope_id;
    llvm::Value *const variable = ctx.allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);
    llvm::Type *const element_type = IR::get_type(ctx.parent->getParent(), unwrap->type).first;
    llvm::Type *const variant_type = IR::get_type(ctx.parent->getParent(), unwrap->base_expr->type).first;
    if (var_unwrap_mode == VariantUnwrapMode::UNSAFE) {
        // Directly unwrap the value when in unsafe mode, possibly breaking stuff, but it's much faster too
        llvm::Value *value_ptr = builder.CreateStructGEP(variant_type, variable, 1, "var_value_ptr");
        llvm::Value *value_cast_ptr = builder.CreateBitCast(value_ptr, PTR_TY, "value_ptr_unsafe");
        llvm::Value *value = IR::aligned_load(builder, element_type, value_cast_ptr, "var_value_unsafe");
        return std::vector<llvm::Value *>{value};
    }

    // First, check if the variant holds a value of our wanted type
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    llvm::BasicBlock *holds_wrong_type = llvm::BasicBlock::Create(context, "var_upwrap_wrong_type", ctx.parent);
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "var_unwrap", ctx.parent);
    builder.SetInsertPoint(inserter);
    const unsigned char id = unwrap->unwrap_id;
    llvm::Value *wanted_type = builder.getInt8(id);
    llvm::Value *current_type_ptr = builder.CreateStructGEP(variant_type, variable, 0, "var_type_ptr");
    llvm::Value *current_type = IR::aligned_load(builder, builder.getInt8Ty(), current_type_ptr, "var_type");
    llvm::Value *holds_type = builder.CreateICmpEQ(current_type, wanted_type, "holds_type");
    llvm::BranchInst *branch = builder.CreateCondBr(holds_type, merge, holds_wrong_type, IR::generate_weights(100, 1));
    branch->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Check if the variant holds the correct type")));

    // The crash block, in the case of a bad variant unwrap
    builder.SetInsertPoint(holds_wrong_type);
    llvm::Value *err_msg = IR::generate_const_string(ctx.parent->getParent(), "Bad variant unwrap occurred\n");
    builder.CreateCall(c_functions.at(PRINTF), {err_msg});
    builder.CreateCall(c_functions.at(ABORT), {});
    builder.CreateUnreachable();

    // The merge block, when the variant access is okay
    builder.SetInsertPoint(merge);
    llvm::Value *value_raw_ptr = builder.CreateStructGEP(variant_type, variable, 1, "value_raw_ptr");
    llvm::Value *value_ptr = builder.CreateBitCast(value_raw_ptr, PTR_TY, "value_ptr");
    llvm::Value *value = IR::aligned_load(builder, element_type, value_ptr, "value");
    return std::vector<llvm::Value *>{value};
}

Generator::group_mapping Generator::Expression::generate_type_cast( //
    llvm::IRBuilder<> &builder,                                     //
    GenerationContext &ctx,                                         //
    garbage_type &garbage,                                          //
    const unsigned int expr_depth,                                  //
    const TypeCastNode *type_cast_node                              //
) {
    // If the base expression is a `TypeNode` and the type cast result is a variant the actual "value" of the type cast is dependant on the
    // variant type, this means that this is a special case which needs special handling
    const VariantType *variant_type = dynamic_cast<const VariantType *>(type_cast_node->type.get());
    const bool is_type_node = type_cast_node->expr->get_variation() == ExpressionNode::Variation::TYPE;
    if (variant_type != nullptr && is_type_node) {
        const std::shared_ptr<Type> &type = type_cast_node->expr->type;
        const auto id = variant_type->get_idx_of_type(type);
        if (!id.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        return std::vector<llvm::Value *>{builder.getInt8(id.value())};
    }
    // First, generate the expression
    auto expr_res = generate_expression(builder, ctx, garbage, expr_depth + 1, type_cast_node->expr.get());
    if (!expr_res.has_value()) {
        return std::nullopt;
    }
    std::vector<llvm::Value *> expr = expr_res.value();
    std::shared_ptr<Type> to_type;
    switch (type_cast_node->type->get_variation()) {
        default:
            to_type = type_cast_node->type;
            break;
        case Type::Variation::PRIMITIVE:
            if (type_cast_node->type->to_string() == "str" && type_cast_node->expr->type->get_variation() == Type::Variation::GROUP) {
                const GroupType *group_type = type_cast_node->expr->type->as<GroupType>();
                std::vector<llvm::Value *> str_values;
                std::vector<size_t> temporaries;
                for (size_t i = 0; i < group_type->types.size(); i++) {
                    const auto &elem_type = group_type->types.at(i);
                    if (elem_type->to_string() == "str") {
                        str_values.emplace_back(expr.at(i));
                        continue;
                    }
                    llvm::Value *str_val = generate_type_cast(builder, ctx, expr.at(i), elem_type, type_cast_node->type);
                    str_values.emplace_back(str_val);
                    temporaries.emplace_back(i);
                }
                llvm::Function *const init_str_fn = Module::String::string_manip_functions.at("init_str");
                llvm::Function *const append_str_fn = Module::String::string_manip_functions.at("append_str");
                llvm::Function *const append_lit_fn = Module::String::string_manip_functions.at("append_lit");
                llvm::Function *const free_fn = c_functions.at(FREE);
                llvm::Value *const open_paren = IR::generate_const_string(ctx.parent->getParent(), "(");
                llvm::Value *const close_paren = IR::generate_const_string(ctx.parent->getParent(), ")");
                llvm::Value *const comma_sep = IR::generate_const_string(ctx.parent->getParent(), ", ");
                llvm::Value *result = builder.CreateCall(init_str_fn, {open_paren, builder.getInt64(1)}, "group_str");
                IR::aligned_store(builder, result, scratchspace);
                for (size_t i = 0; i < str_values.size(); i++) {
                    if (i > 0) {
                        builder.CreateCall(append_lit_fn, {scratchspace, comma_sep, builder.getInt64(2)});
                    }
                    builder.CreateCall(append_str_fn, {scratchspace, str_values[i]});
                }
                builder.CreateCall(append_lit_fn, {scratchspace, close_paren, builder.getInt64(1)});
                for (const auto &tmp_idx : temporaries) {
                    builder.CreateCall(free_fn, {str_values[tmp_idx]});
                }
                result = IR::aligned_load(builder, PTR_TY, scratchspace, "group_str_result");
                return std::vector<llvm::Value *>{result};
            }
            to_type = type_cast_node->type;
            break;
        case Type::Variation::GROUP: {
            const auto *group_type = type_cast_node->type->as<GroupType>();
            const std::vector<std::shared_ptr<Type>> &types = group_type->types;
            if (types.size() > 1) {
                switch (type_cast_node->expr->type->get_variation()) {
                    default:
                        THROW_BASIC_ERR(ERR_GENERATING);
                        return std::nullopt;
                    case Type::Variation::VECTOR: {
                        const auto *vector_type = type_cast_node->expr->type->as<VectorType>();
                        ASSERT(expr.size() == 1);
                        llvm::Value *mult_expr = expr.front();
                        expr.clear();
                        for (size_t i = 0; i < vector_type->width; i++) {
                            expr.emplace_back(builder.CreateExtractElement(mult_expr, i, "mult_group_" + std::to_string(i)));
                        }
                        return expr;
                    }
                    case Type::Variation::TUPLE: {
                        const auto *tuple_type = type_cast_node->expr->type->as<TupleType>();
                        ASSERT(expr.size() == 1);
                        llvm::Value *tuple_expr = expr.front();
                        expr.clear();
                        for (size_t i = 0; i < tuple_type->types.size(); i++) {
                            expr.emplace_back(builder.CreateExtractValue(tuple_expr, i, "tuple_group_" + std::to_string(i)));
                        }
                        return expr;
                    }
                }
            }
            to_type = types.front();
            break;
        }
        case Type::Variation::VECTOR: {
            const auto *vector_type = type_cast_node->type->as<VectorType>();
            if (type_cast_node->type->to_string() == "bool8") {
                ASSERT(type_cast_node->expr->type->to_string() == "u8");
                ASSERT(expr.size() == 1);
                std::vector<llvm::Value *> result;
                result.emplace_back(expr.at(0));
                return result;
            }
            // The expression now must be a group type, so the `expr` size must be the vector-type width
            if (expr.size() != vector_type->width) {
                // If the sizes dont match, the rhs must have size 1 and its type must match the element type of the vector-type
                if (expr.size() == 1 && type_cast_node->expr->type == vector_type->base_type) {
                    // We now can create a single value extension for the vector
                    expr[0] = builder.CreateVectorSplat(vector_type->width, expr[0], "vec_ext");
                    return expr;
                }
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            llvm::Type *const element_type = IR::get_type(ctx.parent->getParent(), vector_type->base_type).first;
            llvm::VectorType *const vector_ty = llvm::VectorType::get(element_type, vector_type->width, false);

            // Create and undefined vector to insert elements into
            llvm::Value *vec = llvm::UndefValue::get(vector_ty);

            // "Store" all the values inside the vector which will be stored in the alloca in the `generate_declaration` function
            for (unsigned int i = 0; i < expr.size(); i++) {
                vec = builder.CreateInsertElement(vec, expr[i], builder.getInt32(i), "vec_insert");
            }
            return std::vector<llvm::Value *>{vec};
        }
        case Type::Variation::TUPLE: {
            if (type_cast_node->expr->type->get_variation() == Type::Variation::GROUP) {
                const auto *expr_group_type = type_cast_node->expr->type->as<GroupType>();
                // Type-checking should have been happened in the parser, so we can assert that the types match
                [[maybe_unused]] const auto *tuple_type = type_cast_node->type->as<TupleType>();
                ASSERT(tuple_type->types.size() == expr_group_type->types.size());
                llvm::Type *tup_type = IR::get_type(ctx.parent->getParent(), type_cast_node->type).first;
                llvm::Value *result = IR::get_default_value_of_type(tup_type);
                for (unsigned int i = 0; i < expr_group_type->types.size(); i++) {
                    ASSERT(expr_group_type->types[i] == tuple_type->types[i]);
                    result = builder.CreateInsertValue(result, expr[i], {i});
                }
                return std::vector<llvm::Value *>{result};
            } else {
                // Only groups can be cast to tuples
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
        }
    }
    if (to_type->to_string() == "str" && type_cast_node->expr->type->to_string() == "type.flint.str.lit") {
        ASSERT(expr.size() == 1);
        expr[0] = Module::String::generate_string_declaration(builder, expr[0], type_cast_node->expr.get());
        return expr;
    }
    // Lastly, check if the expression is castable, and if it is generate the type cast
    for (size_t i = 0; i < expr.size(); i++) {
        expr.at(i) = generate_type_cast(builder, ctx, expr.at(i), type_cast_node->expr->type, to_type);
    }
    return expr;
}

llvm::Value *Generator::Expression::generate_type_cast( //
    llvm::IRBuilder<> &builder,                         //
    GenerationContext &ctx,                             //
    llvm::Value *expr,                                  //
    const std::shared_ptr<Type> &from_type,             //
    const std::shared_ptr<Type> &to_type                //
) {
    const std::string from_type_str = from_type->to_string();
    const std::string to_type_str = to_type->to_string();
    if (from_type->equals(to_type)) {
        return expr;
    } else if (from_type_str == "type.flint.str.lit" && to_type_str == "str") {
        llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
        // Get the length of the literal
        llvm::Value *str_len = builder.CreateCall(c_functions.at(STRLEN), {expr}, "lit_len");
        // Call the `init_str` function
        return builder.CreateCall(init_str_fn, {expr, str_len}, "str_init");
    } else if (from_type->get_variation() == Type::Variation::VECTOR) {
        if (from_type_str == "bool8") {
            if (to_type_str == "str") {
                return builder.CreateCall(Module::TypeCast::typecast_functions.at("bool8_to_str"), {expr}, "b8_to_str_val");
            } else if (to_type_str == "u8") {
                return expr;
            }
        } else if (to_type_str == "str") {
            llvm::Function *cast_fn = Module::TypeCast::typecast_functions.at(from_type_str + "_to_str");
            return builder.CreateCall(cast_fn, {expr}, from_type_str + "_to_str_res");
        } else if (to_type->get_variation() == Type::Variation::VECTOR) {
            const VectorType *from_mult = from_type->as<VectorType>();
            const VectorType *to_mult = to_type->as<VectorType>();
            ASSERT(from_mult->width == to_mult->width);
            llvm::Type *const dest_el_type = IR::get_type(ctx.parent->getParent(), to_mult->base_type).first;
            llvm::VectorType *const dest_vec_type = llvm::VectorType::get(dest_el_type, from_mult->width, false);
            llvm::Value *cast_vector = llvm::UndefValue::get(dest_vec_type);
            for (unsigned int i = 0; i < from_mult->width; i++) {
                llvm::Value *const elem = builder.CreateExtractElement(expr, builder.getInt64(i), "src_el_" + std::to_string(i));
                llvm::Value *const cast_elem = generate_type_cast(builder, ctx, elem, from_mult->base_type, to_mult->base_type);
                cast_vector = builder.CreateInsertElement(                                      //
                    cast_vector, cast_elem, builder.getInt64(i), "cast_el_" + std::to_string(i) //
                );
            }
            return cast_vector;
        }
    } else if (from_type->get_variation() == Type::Variation::OBJECT && to_type->get_variation() == Type::Variation::FUNC) {
        // We "cast" an object to a func module by extracting the required data of the func module from the object and storing it in the
        // func module. Whenever we extract and store a data value from the entity to the func module we call `dima.retain` on that value
        // first for proper ARC-tracking
        llvm::Value *func_value = IR::get_default_value_of_type(builder, ctx.parent->getParent(), to_type);
        llvm::Type *const object_ty = IR::get_type(ctx.parent->getParent(), from_type).first;
        const ObjectNode *object_node = from_type->as<ObjectType>()->object_node;
        const FuncNode *func_node = to_type->as<FuncType>()->func_node;
        llvm::Function *const retain_fn = Module::DIMA::dima_functions.at("retain");
        for (size_t i = 0; i < func_node->required_data.size(); i++) {
            const DataNode *data_node = func_node->required_data.at(i).type->as<DataType>()->data_node;
            size_t idx = 0;
            for (; idx < object_node->data_modules.size(); idx++) {
                const DataNode *data_ptr = object_node->data_modules.at(idx).first;
                if (data_ptr == data_node) {
                    break;
                }
            }
            llvm::Value *const ext_data_ptr = builder.CreateStructGEP(object_ty, expr, idx, "extracted_data_ptr_" + std::to_string(idx));
            llvm::Value *const ext_data = IR::aligned_load(builder, PTR_TY, ext_data_ptr, "extracted_data_" + std::to_string(idx));
            builder.CreateCall(retain_fn, {ext_data});
            func_value = builder.CreateInsertValue(func_value, ext_data, i, "func_value__insert_" + std::to_string(i));
        }
        return func_value;
    } else if (from_type->get_variation() == Type::Variation::OBJECT && to_type->get_variation() == Type::Variation::INTERFACE) {
        // We "cast" an object to an interface by storing the pointer to the object alongside the pointer to the dispatch function and the
        // ID of the object in a new structure.
        const ObjectType *object_type = from_type->as<ObjectType>();
        llvm::Value *interface_value = IR::get_default_value_of_type(builder, ctx.parent->getParent(), to_type);
        const std::string &cast_name = object_type->to_string() + "_to_" + to_type->to_string();
        llvm::Function *const object_dispatch_fn = object_dispatch_functions.at(object_type->get_id());
        const std::string head_key = object_type->object_node->file_hash.to_string() + "." + object_type->object_node->name;
        llvm::Value *const object_dima_head = Module::DIMA::dima_heads.at(head_key);
        llvm::Function *const dima_retain_fn = Module::DIMA::dima_functions.at("retain");
        builder.CreateCall(dima_retain_fn, {expr});
        interface_value = builder.CreateInsertValue(interface_value, expr, 0);
        interface_value = builder.CreateInsertValue(interface_value, object_dispatch_fn, 1);
        interface_value = builder.CreateInsertValue(interface_value, object_dima_head, 2, cast_name);
        return interface_value;
    } else if (from_type->get_variation() == Type::Variation::ARRAY && to_type->get_variation() == Type::Variation::ARRAY) {
        [[maybe_unused]] const ArrayType *from_array = from_type->as<ArrayType>();
        [[maybe_unused]] const ArrayType *to_array = to_type->as<ArrayType>();
        ASSERT(from_array->type->equals(to_array->type));
        ASSERT(from_array->dimensionality == to_array->dimensionality);
        ASSERT(from_array->sizes.has_value());
        ASSERT(!to_array->sizes.has_value());
        // TODO: Implement proper "fixed array" to "dynamic array" casting in the future, for now they are the exact same thing at runtime
        return expr;
    } else if (from_type_str == "u8") {
        if (to_type_str == "str") {
            // Create a new string of size 1 and set its first byte to the char
            llvm::Value *str_value = builder.CreateCall(                                                   //
                Module::String::string_manip_functions.at("create_str"), {builder.getInt64(1)}, "char_val" //
            );
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
            llvm::Value *val_ptr = builder.CreateStructGEP(str_type, str_value, 1);
            IR::aligned_store(builder, expr, val_ptr);
            return str_value;
        } else if (to_type_str == "i8") {
            return Module::TypeCast::uN_to_iN_same(builder, expr);
        } else if (to_type_str == "u16") {
            return builder.CreateZExt(expr, builder.getInt16Ty());
        } else if (to_type_str == "i16") {
            return builder.CreateSExt(expr, builder.getInt16Ty());
        } else if (to_type_str == "u32") {
            return builder.CreateZExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "i32") {
            return builder.CreateSExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "u64") {
            return builder.CreateZExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "f32") {
            return Module::TypeCast::uN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::uN_to_f64(builder, expr);
        } else if (to_type_str == "bool8") {
            return expr;
        }
    } else if (from_type_str == "i8") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("i8_to_str"), {expr}, "i8_to_str_res");
        } else if (to_type_str == "i8") {
            return Module::TypeCast::iN_to_uN_same(builder, expr);
        } else if (to_type_str == "u16") {
            return Module::TypeCast::iN_to_uN_ext(builder, expr, 16);
        } else if (to_type_str == "i16") {
            return builder.CreateSExt(expr, builder.getInt16Ty());
        } else if (to_type_str == "u32") {
            return Module::TypeCast::iN_to_uN_ext(builder, expr, 32);
        } else if (to_type_str == "i32") {
            return builder.CreateSExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "u64") {
            return Module::TypeCast::iN_to_uN_ext(builder, expr, 64);
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "f32") {
            return Module::TypeCast::iN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::iN_to_f64(builder, expr);
        } else if (to_type_str == "bool8") {
            return expr;
        }
    } else if (from_type_str == "u16") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("u16_to_str"), {expr}, "u16_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::uN_to_uN_trunc(builder, expr, 8);
        } else if (to_type_str == "i8") {
            return Module::TypeCast::uN_to_iN_trunc(builder, expr, 8);
        } else if (to_type_str == "i16") {
            return Module::TypeCast::uN_to_iN_same(builder, expr);
        } else if (to_type_str == "u32") {
            return builder.CreateZExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "i32") {
            return builder.CreateSExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "u64") {
            return builder.CreateZExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "f32") {
            return Module::TypeCast::uN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::uN_to_f64(builder, expr);
        } else if (to_type_str == "bool8") {
            return expr;
        }
    } else if (from_type_str == "i16") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("i16_to_str"), {expr}, "i16_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::iN_to_uN_trunc(builder, expr, 8);
        } else if (to_type_str == "i8") {
            return Module::TypeCast::iN_to_iN_trunc(builder, expr, 8);
        } else if (to_type_str == "u16") {
            return Module::TypeCast::iN_to_uN_same(builder, expr);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::iN_to_uN_ext(builder, expr, 32);
        } else if (to_type_str == "i32") {
            return builder.CreateSExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "u64") {
            return Module::TypeCast::iN_to_uN_ext(builder, expr, 64);
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "f32") {
            return Module::TypeCast::iN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::iN_to_f64(builder, expr);
        } else if (to_type_str == "bool8") {
            return expr;
        }
    } else if (from_type_str == "u32") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("u32_to_str"), {expr}, "u32_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::uN_to_uN_trunc(builder, expr, 8);
        } else if (to_type_str == "i8") {
            return Module::TypeCast::uN_to_iN_trunc(builder, expr, 8);
        } else if (to_type_str == "u16") {
            return Module::TypeCast::uN_to_uN_trunc(builder, expr, 16);
        } else if (to_type_str == "i16") {
            return Module::TypeCast::uN_to_iN_trunc(builder, expr, 16);
        } else if (to_type_str == "i32") {
            return Module::TypeCast::uN_to_iN_same(builder, expr);
        } else if (to_type_str == "u64") {
            return builder.CreateZExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "f32") {
            return Module::TypeCast::uN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::uN_to_f64(builder, expr);
        }
    } else if (from_type_str == "i32") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("i32_to_str"), {expr}, "i32_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::iN_to_uN_trunc(builder, expr, 8);
        } else if (to_type_str == "i8") {
            return Module::TypeCast::iN_to_iN_trunc(builder, expr, 8);
        } else if (to_type_str == "u16") {
            return Module::TypeCast::iN_to_uN_trunc(builder, expr, 16);
        } else if (to_type_str == "i16") {
            return Module::TypeCast::iN_to_iN_trunc(builder, expr, 16);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::iN_to_uN_same(builder, expr);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::iN_to_uN_ext(builder, expr, 64);
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "f32") {
            return Module::TypeCast::iN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::iN_to_f64(builder, expr);
        }
    } else if (from_type_str == "u64") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("u64_to_str"), {expr}, "u64_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::uN_to_uN_trunc(builder, expr, 8);
        } else if (to_type_str == "i8") {
            return Module::TypeCast::uN_to_iN_trunc(builder, expr, 8);
        } else if (to_type_str == "u16") {
            return Module::TypeCast::uN_to_uN_trunc(builder, expr, 16);
        } else if (to_type_str == "i16") {
            return Module::TypeCast::uN_to_iN_trunc(builder, expr, 16);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::uN_to_uN_trunc(builder, expr, 32);
        } else if (to_type_str == "i32") {
            return Module::TypeCast::uN_to_iN_trunc(builder, expr, 32);
        } else if (to_type_str == "i64") {
            return Module::TypeCast::uN_to_iN_same(builder, expr);
        } else if (to_type_str == "f32") {
            return Module::TypeCast::uN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::uN_to_f64(builder, expr);
        }
    } else if (from_type_str == "i64") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("i64_to_str"), {expr}, "i64_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::iN_to_uN_trunc(builder, expr, 8);
        } else if (to_type_str == "i8") {
            return Module::TypeCast::iN_to_iN_trunc(builder, expr, 8);
        } else if (to_type_str == "u16") {
            return Module::TypeCast::iN_to_uN_trunc(builder, expr, 16);
        } else if (to_type_str == "i16") {
            return Module::TypeCast::iN_to_iN_trunc(builder, expr, 16);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::iN_to_uN_trunc(builder, expr, 32);
        } else if (to_type_str == "i32") {
            return Module::TypeCast::iN_to_iN_trunc(builder, expr, 32);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::iN_to_uN_same(builder, expr);
        } else if (to_type_str == "f32") {
            return Module::TypeCast::iN_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::iN_to_f64(builder, expr);
        }
    } else if (from_type_str == "f32" || from_type_str == "f64") {
        if (to_type_str == "str") {
            if (from_type_str == "f32") {
                return builder.CreateCall(Module::TypeCast::typecast_functions.at("f32_to_str"), {expr}, "f32_to_str_res");
            } else {
                return builder.CreateCall(Module::TypeCast::typecast_functions.at("f64_to_str"), {expr}, "f64_to_str_res");
            }
        } else if (to_type_str == "u8") {
            return Module::TypeCast::fN_to_uN(builder, expr, 8);
        } else if (to_type_str == "i8") {
            return Module::TypeCast::fN_to_iN(builder, expr, 8);
        } else if (to_type_str == "u16") {
            return Module::TypeCast::fN_to_uN(builder, expr, 16);
        } else if (to_type_str == "i16") {
            return Module::TypeCast::fN_to_iN(builder, expr, 16);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::fN_to_uN(builder, expr, 32);
        } else if (to_type_str == "i32") {
            return Module::TypeCast::fN_to_iN(builder, expr, 32);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::fN_to_uN(builder, expr, 64);
        } else if (to_type_str == "i64") {
            return Module::TypeCast::fN_to_iN(builder, expr, 64);
        } else if (from_type_str == "f32" && to_type_str == "f64") {
            return Module::TypeCast::f32_to_f64(builder, expr);
        } else if (from_type_str == "f64" && to_type_str == "f32") {
            return Module::TypeCast::f64_to_f32(builder, expr);
        }
    } else if (from_type_str == "bool") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("bool_to_str"), {expr}, "bool_to_str_res");
        } else if (to_type_str == "u8") {
            return builder.CreateSelect(expr, builder.getInt8(1), builder.getInt8(0), "bool_to_u8");
        } else if (to_type_str == "i8") {
            return builder.CreateSelect(expr, builder.getInt8(1), builder.getInt8(0), "bool_to_i8");
        } else if (to_type_str == "u16") {
            return builder.CreateSelect(expr, builder.getInt16(1), builder.getInt16(0), "bool_to_u16");
        } else if (to_type_str == "i16") {
            return builder.CreateSelect(expr, builder.getInt16(1), builder.getInt16(0), "bool_to_i16");
        } else if (to_type_str == "u32") {
            return builder.CreateSelect(expr, builder.getInt32(1), builder.getInt32(0), "bool_to_u32");
        } else if (to_type_str == "i32") {
            return builder.CreateSelect(expr, builder.getInt32(1), builder.getInt32(0), "bool_to_i32");
        } else if (to_type_str == "u64") {
            return builder.CreateSelect(expr, builder.getInt64(1), builder.getInt64(0), "bool_to_u64");
        } else if (to_type_str == "i64") {
            return builder.CreateSelect(expr, builder.getInt64(1), builder.getInt64(0), "bool_to_i64");
        } else if (to_type_str == "f32") {
            return builder.CreateSelect(                          //
                expr,                                             //
                llvm::ConstantFP::get(builder.getFloatTy(), 1.0), //
                llvm::ConstantFP::get(builder.getFloatTy(), 0.0), //
                "bool_to_f32"                                     //
            );
        } else if (to_type_str == "f64") {
            return builder.CreateSelect(                           //
                expr,                                              //
                llvm::ConstantFP::get(builder.getDoubleTy(), 1.0), //
                llvm::ConstantFP::get(builder.getDoubleTy(), 0.0), //
                "bool_to_f64"                                      //
            );
        }
    } else if (from_type_str == "void?") {
        // The 'none' literal
        return IR::get_default_value_of_type(builder, ctx.parent->getParent(), to_type);
    } else if (from_type_str == "void*") {
        // The 'null' literal
        return IR::get_default_value_of_type(builder, ctx.parent->getParent(), to_type);
    } else if (from_type->get_variation() == Type::Variation::OPAQUE && to_type_str == "str") {
        // Casting an opaque value to a string
        return builder.CreateCall(Module::TypeCast::typecast_functions.at("opaque_to_str"), {expr}, "opaque_to_str_res");
    }
    switch (to_type->get_variation()) {
        default:
            // Jump to the error prinitng of below for non-castable types
            break;
        case Type::Variation::OPTIONAL: {
            const auto *to_opt_type = to_type->as<OptionalType>();
            if (!from_type->equals(to_opt_type->base_type)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
            llvm::Value *cast_optional = IR::get_default_value_of_type(builder, ctx.parent->getParent(), to_type);
            cast_optional = builder.CreateInsertValue(cast_optional, builder.getInt1(true), 0);
            return builder.CreateInsertValue(cast_optional, expr, 1, "cast_optional");
        }
        case Type::Variation::VARIANT: {
            const auto *to_var_type = to_type->as<VariantType>();
            if (to_var_type->get_idx_of_type(from_type).has_value()) {
                // It's allowed to "cast" the type to the variant
                return expr;
            }
            break;
        }
        case Type::Variation::ERROR_SET: {
            const auto *to_error_type = to_type->as<ErrorSetType>();
            if (to_type_str == "anyerror") {
                // Every error is an anyerror and we don't need to change the `type_id` value of the error
                return expr;
            }
            // We simply need to change the `type_id` value field of the error
            unsigned int new_type_id = to_error_type->error_node->file_hash.get_type_id_from_str(to_error_type->error_node->name);
            expr = builder.CreateInsertValue(expr, builder.getInt32(new_type_id), {0}, "cast_type_id");
            return expr;
        }
    }
    if (from_type->get_variation() == Type::Variation::ENUM) {
        const auto *from_enum_type = from_type->as<EnumType>();
        // Enum types are only allowed to be cast to strings or to integers
        if (to_type_str == "i32" || to_type_str == "u32") {
            // Enums are i32 values internally annyway, so we actually do not need any casting in this case
            return expr;
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty(), "enum_cast_i64");
        } else if (to_type_str == "u64") {
            return builder.CreateZExt(expr, builder.getInt64Ty(), "enum_cast_u64");
        } else if (to_type_str == "u8") {
            return builder.CreateTrunc(expr, builder.getInt8Ty(), "num_cast_u8");
        }
        ASSERT(to_type_str == "str");
        // TODO: Create a global `flint.to_string` function with a type id parameter and pointer to the value being printed to reduce
        // code-duplication of casting enums to strings, as this is very messy for large enums
        const EnumNode *enum_node = from_enum_type->enum_node;
        const std::string enum_name_arrays_map_key = enum_node->file_hash.to_string() + "." + enum_node->name;
        llvm::GlobalVariable *name_array = enum_name_arrays_map.at(enum_name_arrays_map_key);

        // Create basic blocks for switch
        llvm::BasicBlock *const previous_block = builder.GetInsertBlock();
        llvm::BasicBlock *const default_block = llvm::BasicBlock::Create(context, "default", previous_block->getParent());
        llvm::BasicBlock *const merge_block = llvm::BasicBlock::Create(context, "merge");

        // Create switch on expr (the enum value)
        builder.SetInsertPoint(previous_block);
        llvm::SwitchInst *sw = builder.CreateSwitch(expr, default_block);

        // Add cases for each enum member
        std::vector<llvm::BasicBlock *> case_blocks;
        for (size_t i = 0; i < enum_node->values.size(); i++) {
            const auto &[name, value] = enum_node->values.at(i);
            case_blocks.emplace_back(llvm::BasicBlock::Create(context, "case_" + name, previous_block->getParent()));
            sw->addCase(llvm::ConstantInt::get(builder.getInt32Ty(), value), case_blocks.back());

            builder.SetInsertPoint(case_blocks.back());
            builder.CreateBr(merge_block);
        }

        // Default case (invalid enum value): unreachable for now
        builder.SetInsertPoint(default_block);
        builder.CreateUnreachable();

        // Merge block: load mapped index
        merge_block->insertInto(previous_block->getParent());
        builder.SetInsertPoint(merge_block);
        llvm::PHINode *mapped_index = builder.CreatePHI(builder.getInt32Ty(), enum_node->values.size(), "mapped_index");
        for (size_t i = 0; i < enum_node->values.size(); i++) {
            mapped_index->addIncoming(builder.getInt32(i), case_blocks.at(i));
        }

        // Use mapped_index for GEP
        llvm::Value *name_str_ptr = builder.CreateGEP(name_array->getType(), name_array, {mapped_index}, "name_str_ptr");
        llvm::Value *name_str = IR::aligned_load(builder, PTR_TY, name_str_ptr, "name_str");
        llvm::Value *name_len = builder.CreateCall(c_functions.at(STRLEN), {name_str}, "name_len");
        llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
        llvm::Value *cast_value = builder.CreateCall(init_str_fn, {name_str, name_len}, "cast_enum");
        return cast_value;
    }
    if (from_type->get_variation() == Type::Variation::ERROR_SET || from_type->to_string() == "anyerror") {
        if (to_type_str == "str") {
            llvm::Function *get_err_str_fn = Error::error_functions.at("get_err_str");
            return builder.CreateCall(get_err_str_fn, {expr}, "err_to_str");
        }
    }
    if (from_type->get_variation() == Type::Variation::POINTER && to_type->get_variation() == Type::Variation::OPAQUE) {
        // Opaque pointers still are pointers, no conversion needed
        return expr;
    }
    if (from_type_str == "u8[]" && to_type_str == "str") {
        // one-dimensional u8 arrays are always castable to a string since a string more or less "is" a one-dimensional u8 array
        llvm::StructType *const str_type = type_map.at("type.str");
        llvm::Value *arr_value_ptr = builder.CreateStructGEP(str_type, expr, 1, "arr_value_ptr");
        llvm::Value *arr_len = IR::aligned_load(builder, builder.getInt64Ty(), arr_value_ptr, "arr_len");
        // IR::generate_debug_print(&builder, ctx.parent->getParent(), "u8[]->str: arr_len=%lu", {arr_len});
        llvm::Value *str_type_size = builder.getInt64(Allocation::get_type_size(ctx.parent->getParent(), str_type) + 1);
        llvm::Value *cast_str_size = builder.CreateAdd(str_type_size, arr_len, "cast_str_size");
        llvm::Value *cast_str = builder.CreateCall(c_functions.at(MALLOC), {cast_str_size}, "cast_str");
        llvm::Value *str_len_ptr = builder.CreateStructGEP(str_type, cast_str, 0, "str_len_ptr");
        IR::aligned_store(builder, arr_len, str_len_ptr);
        llvm::Value *str_value_ptr = builder.CreateStructGEP(str_type, cast_str, 1, "str_value_ptr");
        llvm::Value *arr_value_start = builder.CreateGEP(builder.getInt64Ty(), arr_value_ptr, builder.getInt64(1), "arr_value_start");
        builder.CreateCall(c_functions.at(MEMCPY), {str_value_ptr, arr_value_start, arr_len});
        llvm::Value *str_last_char = builder.CreateGEP(builder.getInt8Ty(), str_value_ptr, arr_len, "str_last_char");
        IR::aligned_store(builder, builder.getInt8(0), str_last_char);
        return cast_str;
    }
    std::cout << "FROM_TYPE: " << from_type_str << ", TO_TYPE: " << to_type_str << std::endl;
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}

Generator::group_mapping Generator::Expression::generate_unary_op_expression( //
    llvm::IRBuilder<> &builder,                                               //
    GenerationContext &ctx,                                                   //
    garbage_type &garbage,                                                    //
    const unsigned int expr_depth,                                            //
    const UnaryOpExpression *unary_op                                         //
) {
    const ExpressionNode *expression = unary_op->operand.get();
    const bool is_pointer = unary_op->type->get_variation() == Type::Variation::POINTER;
    std::vector<llvm::Value *> operand = generate_expression(builder, ctx, garbage, expr_depth + 1, expression, is_pointer).value();
    for (size_t i = 0; i < operand.size(); i++) {
        const std::string &expression_type = expression->type->to_string();
        switch (unary_op->operator_token) {
            default:
                // Unknown unary operator
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            case TOK_NOT:
                // Not is only allowed to be placed at the left of the expression
                if (!unary_op->is_left) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                operand.at(i) = Logical::generate_not(builder, operand.at(i));
                break;
            case TOK_INCREMENT:
                if (expression_type == "i32") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateAdd(operand.at(i), one, "add_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("i32_safe_add"), {operand.at(i), one}, "safe_add_res" //
                          );
                } else if (expression_type == "i64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateAdd(operand.at(i), one, "add_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("i64_safe_add"), {operand.at(i), one}, "safe_add_res" //
                          );
                } else if (expression_type == "u32") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateAdd(operand.at(i), one, "add_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("u32_safe_add"), {operand.at(0), one}, "safe_add_res" //
                          );
                } else if (expression_type == "u64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateAdd(operand.at(i), one, "add_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("u64_safe_add"), {operand.at(0), one}, "safe_add_res" //
                          );
                } else if (expression_type == "f32" || expression_type == "f64") {
                    llvm::Value *one = llvm::ConstantFP::get(operand.at(i)->getType(), 1.0);
                    operand.at(i) = builder.CreateFAdd(operand.at(i), one);
                }
                break;
            case TOK_DECREMENT:
                if (expression_type == "i32") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateSub(operand.at(i), one, "sub_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("i32_safe_sub"), {operand.at(i), one}, "safe_sub_res" //
                          );
                } else if (expression_type == "i64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateSub(operand.at(i), one, "sub_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("i64_safe_sub"), {operand.at(i), one}, "safe_sub_res" //
                          );
                } else if (expression_type == "u32") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateSub(operand.at(i), one, "sub_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("u32_safe_sub"), {operand.at(0), one}, "safe_sub_res" //
                          );
                } else if (expression_type == "u64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateSub(operand.at(i), one, "sub_res")
                        : builder.CreateCall(                                                                                   //
                              Module::Arithmetic::arithmetic_functions.at("u64_safe_sub"), {operand.at(0), one}, "safe_sub_res" //
                          );
                } else if (expression_type == "f32" || expression_type == "f64") {
                    llvm::Value *one = llvm::ConstantFP::get(operand.at(i)->getType(), 1.0);
                    operand.at(i) = builder.CreateFSub(operand.at(i), one);
                }
                break;
            case TOK_MINUS:
                if (!unary_op->is_left) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                if (expression_type == "u32" || expression_type == "u64") {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                } else if (expression_type == "i32" || expression_type == "i64") {
                    llvm::Constant *zero = llvm::ConstantInt::get(operand.at(i)->getType(), 0);
                    operand.at(i) = builder.CreateSub(zero, operand.at(i), "neg");
                } else if (expression_type == "f32" || expression_type == "f64") {
                    operand.at(i) = builder.CreateFNeg(operand.at(i), "fneg");
                }
                break;
            case TOK_BIT_AND: {
                if (!unary_op->is_left) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                if (unary_op->type->get_variation() != Type::Variation::POINTER) {
                    // Reference of operator (`&var`) not allowed on non-pointer types
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                const auto *pointer_type = unary_op->type->as<PointerType>();
                const std::shared_ptr<Type> &base_type = pointer_type->base_type;
                const bool is_dynamic_array = expression->type->get_variation() == Type::Variation::ARRAY //
                    && !expression->type->as<ArrayType>()->sizes.has_value();
                ASSERT(base_type == expression->type || (is_dynamic_array && base_type == expression->type->as<ArrayType>()->type));
                ASSERT(operand.size() == 1);
                if (base_type->is_dima_managed()) {
                    // We need to load the pointer since the operand points to the allocation in which the pointer to the allocated slot
                    operand.front() = IR::aligned_load(builder, PTR_TY, operand.front(), "loaded_data_ptr");
                } else if (is_dynamic_array) {
                    // & on a dynamic array T[] produces T* (pointer to the element data section).
                    // The array struct has layout { i64 dim, i64 lengths[], data[] }.
                    // Load the array struct pointer, then GEP past the metadata to get the data pointer.
                    operand.front() = IR::aligned_load(builder, PTR_TY, operand.front(), "arr_struct_ptr");
                    llvm::Type *const str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
                    llvm::Value *const len_ptr = builder.CreateStructGEP(str_type, operand.front(), 0, "len_ptr");
                    llvm::Value *const dim = IR::aligned_load(builder, builder.getInt64Ty(), len_ptr, "arr_dim");
                    llvm::Value *const dim_lengths = builder.CreateStructGEP(str_type, operand.front(), 1, "dim_lengths");
                    operand.front() = builder.CreateGEP(builder.getInt64Ty(), dim_lengths, dim, "arr_data");
                }
                // Check if the base type is a complex type and whether it needs to be passed by value or by reference
                // If it needs to be passed by value normally, we first need to get the allocation of it, for now only VariableNodes are
                // supported for non-complex types (like i32) but complex types (like data etc, which are passed by reference normally) can
                // be used from any context here
                //
                // If it's a complex type, is allocated on the heap and a pointer is stored in the alloca, we need to load that pointer
                // first, but this is already handled by the `generate_expression` function above, it loads the pointer and returns it
                // as the operand. This means that we literally do not need to change the operands *at all* here lol
                //
                // If it's not a complex type, it's allocated directly on the stack and we simply need to pass in a pointer to the
                // allocation here
                //
                // This means we do not need to check whether the base type is complex or not etc, the compiler internals help us here
                // tremendously, in the `generate_expression` function we just pass in the `is_reference` argument to true when it's a
                // pointer and it already handles everything we would ever want to be handled...nice
                break;
            }
        }
    }
    return operand;
}

Generator::group_mapping Generator::Expression::generate_binary_op( //
    llvm::IRBuilder<> &builder,                                     //
    GenerationContext &ctx,                                         //
    garbage_type &garbage,                                          //
    const unsigned int expr_depth,                                  //
    const BinaryOpNode *bin_op_node                                 //
) {
    // Handle short-circuit evaluation for 'and' and 'or' operators
    if (bin_op_node->operator_token == TOK_AND || bin_op_node->operator_token == TOK_OR) {
        const bool is_and = bin_op_node->operator_token == TOK_AND;

        // Evaluate LHS first
        const bool lhs_is_reference = bin_op_node->left->type->get_variation() == Type::Variation::VARIANT;
        auto lhs_maybe = generate_expression(builder, ctx, garbage, expr_depth + 1, bin_op_node->left.get(), lhs_is_reference);
        if (!lhs_maybe.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        std::vector<llvm::Value *> lhs = lhs_maybe.value();
        if (lhs.size() != 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *lhs_val = lhs[0];

        // Save the LHS basic block before branching
        llvm::BasicBlock *lhs_block = builder.GetInsertBlock();

        // Create basic blocks for short-circuit evaluation
        llvm::Function *parent_func = lhs_block->getParent();
        llvm::BasicBlock *rhs_block = llvm::BasicBlock::Create(builder.getContext(), is_and ? "and.rhs" : "or.rhs", parent_func);
        llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(builder.getContext(), is_and ? "and.merge" : "or.merge", parent_func);

        // For 'and': if LHS is false, skip RHS and result is false
        // For 'or': if LHS is true, skip RHS and result is true
        if (is_and) {
            builder.CreateCondBr(lhs_val, rhs_block, merge_block);
        } else {
            builder.CreateCondBr(lhs_val, merge_block, rhs_block);
        }

        // Evaluate RHS in rhs_block
        builder.SetInsertPoint(rhs_block);
        const bool rhs_is_reference = bin_op_node->right->type->get_variation() == Type::Variation::VARIANT;
        auto rhs_maybe = generate_expression(builder, ctx, garbage, expr_depth + 1, bin_op_node->right.get(), rhs_is_reference);
        if (!rhs_maybe.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        std::vector<llvm::Value *> rhs = rhs_maybe.value();
        if (rhs.size() != 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *rhs_val = rhs[0];

        llvm::BasicBlock *rhs_end_block = builder.GetInsertBlock();
        // Clear all garbage so far before branching to the merge block
        if (!Statement::clear_garbage(builder, garbage, expr_depth)) {
            return std::nullopt;
        }
        builder.CreateBr(merge_block);

        // Create PHI node in merge block
        builder.SetInsertPoint(merge_block);
        llvm::PHINode *phi = builder.CreatePHI(builder.getInt1Ty(), 2, is_and ? "and.result" : "or.result");
        phi->addIncoming(is_and ? builder.getFalse() : builder.getTrue(), lhs_block);
        phi->addIncoming(rhs_val, rhs_end_block);
        return std::vector<llvm::Value *>{phi};
    }

    // For all other operators, evaluate both sides
    const bool lhs_is_reference = bin_op_node->left->type->get_variation() == Type::Variation::VARIANT;
    auto lhs_maybe = generate_expression(builder, ctx, garbage, expr_depth + 1, bin_op_node->left.get(), lhs_is_reference);
    if (!lhs_maybe.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    std::vector<llvm::Value *> lhs = lhs_maybe.value();
    std::vector<llvm::Value *> rhs;
    if (bin_op_node->operator_token != TOK_CATCH) {
        const bool rhs_is_reference = bin_op_node->right->type->get_variation() == Type::Variation::VARIANT;
        auto rhs_maybe = generate_expression(builder, ctx, garbage, expr_depth + 1, bin_op_node->right.get(), rhs_is_reference);
        if (!rhs_maybe.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        rhs = rhs_maybe.value();
    } else {
        // Create as many rhs "values" as present in the lhs
        rhs = std::vector<llvm::Value *>(lhs.size(), nullptr);
    }
    if (lhs.size() != rhs.size()) {
        ASSERT(lhs.size() == 1 || rhs.size() == 1);
        auto result = generate_binary_op_set_cmp(builder, ctx, garbage, expr_depth, bin_op_node, lhs, rhs);
        if (!result.has_value()) {
            return std::nullopt;
        }
        return std::vector<llvm::Value *>{result.value()};
    }
    std::vector<llvm::Value *> return_value;

    const bool is_lhs_mult = bin_op_node->left->type->get_variation() == Type::Variation::VECTOR;
    const bool is_rhs_mult = bin_op_node->right->type->get_variation() == Type::Variation::VECTOR;
    if (is_lhs_mult && is_rhs_mult) {
        const auto *lhs_mult = bin_op_node->left->type->as<VectorType>();
        const auto *rhs_mult = bin_op_node->right->type->as<VectorType>();
        if (!lhs_mult->base_type->equals(rhs_mult->base_type)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (lhs_mult->width != rhs_mult->width) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        // For vector-types we have exactly one value in each vector
        ASSERT(lhs.size() == 1 && rhs.size() == 1);
        const std::string type_str = bin_op_node->type->to_string();
        std::optional<llvm::Value *> result = generate_binary_op_vector(builder, bin_op_node, type_str, lhs[0], rhs[0]);
        if (!result.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        return_value.emplace_back(result.value());
        return return_value;
    }

    for (size_t i = 0; i < lhs.size(); i++) {
        const std::shared_ptr<Type> type = bin_op_node->left->type;
        const GroupType *group_type = dynamic_cast<const GroupType *>(type.get());
        ASSERT(group_type == nullptr || group_type->types.size() == lhs.size());
        const std::string type_str = group_type == nullptr ? type->to_string() : group_type->types[i]->to_string();
        std::optional<llvm::Value *> result = generate_binary_op_scalar(             //
            builder, ctx, garbage, expr_depth, bin_op_node, type_str, lhs[i], rhs[i] //
        );
        if (!result.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        return_value.emplace_back(result.value());
    }
    if (return_value.size() > 1) {
        // It's a group containing multiple values, we then need to check if the operation is a comparison operation which should result in
        // a boolean value, if it is then we must combine the results together to form the single value to return
        switch (bin_op_node->operator_token) {
            default:
                break;
            case TOK_NOT_EQUAL: {
                // All return values need to be combined together with ors. Only one value can differ for the whole condition to become
                // 'false'
                llvm::Value *combined = return_value.front();
                for (size_t i = 1; i < return_value.size(); i++) {
                    combined = builder.CreateOr(combined, return_value.at(i));
                }
                combined->setName("combined_group_cond_neq");
                return_value.clear();
                return_value.emplace_back(combined);
                break;
            }
            case TOK_EQUAL_EQUAL:
            case TOK_LESS:
            case TOK_LESS_EQUAL:
            case TOK_GREATER:
            case TOK_GREATER_EQUAL: {
                // All return values need to be combined together with ands.
                llvm::Value *combined = return_value.front();
                for (size_t i = 1; i < return_value.size(); i++) {
                    combined = builder.CreateAnd(combined, return_value.at(i));
                }
                return_value.clear();
                return_value.emplace_back(combined);
                break;
            }
        }
    }
    return return_value;
}

std::optional<llvm::Value *> Generator::Expression::generate_binary_op_set_cmp( //
    llvm::IRBuilder<> &builder,                                                 //
    GenerationContext &ctx,                                                     //
    garbage_type &garbage,                                                      //
    const unsigned int expr_depth,                                              //
    const BinaryOpNode *bin_op_node,                                            //
    std::vector<llvm::Value *> lhs,                                             //
    std::vector<llvm::Value *> rhs                                              //
) {
    llvm::Value *scalar_value;
    std::vector<llvm::Value *> group_value;
    ExpressionNode const *lhs_expr = bin_op_node->left.get();
    ExpressionNode const *rhs_expr = bin_op_node->right.get();
    if (lhs.size() == 1) {
        scalar_value = lhs.front();
        group_value = rhs;
        rhs_expr = lhs_expr;
    } else {
        scalar_value = rhs.front();
        group_value = lhs;
        lhs_expr = rhs_expr;
    }
    const FakeBinaryOpNode bin_op = {
        bin_op_node->operator_token, //
        lhs_expr,                    //
        rhs_expr,                    //
        bin_op_node->type,           //
        bin_op_node->is_shorthand    //
    };

    // Let's check all the cases and how they will compile down for the scalar—group operation
    // `x == (1, 2)` -> `x == 1 or x == 2`
    // `x != (1, 2)` -> `x != 1 and x != 2`
    // `x <= (1, 2)` -> `x <= 1 and x <= 2`
    // `x >= (1, 2)` -> `x >= 1 and x >= 2`
    // `x < (1, 2)` -> `x < 1 and x < 2`
    // `x > (1, 2)` -> `x > 1 and x > 2`
    // As we can see, the `==` case is the *only* case where we combine the comparisons with an `or`, all other cases use the `and` to
    // connect the expressions
    const bool is_or_cmp = bin_op_node->operator_token == TOK_EQUAL_EQUAL;
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    // We need to create N blocks here because we need N - 1 for all cases + 1 for the merge block
    std::vector<llvm::BasicBlock *> blocks;
    for (size_t i = 1; i < group_value.size(); i++) {
        blocks.push_back(llvm::BasicBlock::Create(context, "set_cmp_" + std::to_string(i), ctx.parent));
    }
    blocks.push_back(llvm::BasicBlock::Create(context, std::string(is_or_cmp ? "true" : "false") + "_block", ctx.parent));
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "merge", ctx.parent);

    // Now we can create all the checks and branch accordingly
    builder.SetInsertPoint(inserter);
    auto result_maybe = generate_binary_op_scalar(                                                                        //
        builder, ctx, garbage, expr_depth, bin_op_node, bin_op.left->type->to_string(), scalar_value, group_value.front() //
    );
    if (!result_maybe.has_value()) {
        return std::nullopt;
    }
    if (is_or_cmp) {
        // Branch to the merge block if result is true, otherwise to the first other block
        builder.CreateCondBr(result_maybe.value(), blocks.back(), blocks.front());
    } else {
        // Branch to the first other block if result is true, otherwise to the merge block
        builder.CreateCondBr(result_maybe.value(), blocks.front(), blocks.back());
    }
    for (size_t i = 0; i < blocks.size() - 1; i++) {
        builder.SetInsertPoint(blocks[i]);
        result_maybe = generate_binary_op_scalar(                                                                            //
            builder, ctx, garbage, expr_depth, bin_op_node, bin_op.left->type->to_string(), scalar_value, group_value[i + 1] //
        );
        if (!result_maybe.has_value()) {
            return std::nullopt;
        }
        // Now we simply branch according to the result, either to the next block or to the merge block. To which block we branch, again,
        // depends on the 'or' or 'and' operation. The 'and' operation just means that we have gone through all blocks successfully
        if (is_or_cmp) {
            builder.CreateCondBr(result_maybe.value(), blocks.back(), (i + 2 == blocks.size()) ? merge : blocks[i + 1]);
        } else {
            builder.CreateCondBr(result_maybe.value(), (i + 2 == blocks.size()) ? merge : blocks[i + 1], blocks.back());
        }
    }

    // Just branch to the merge block in the false block, this way we can tell whether we
    builder.SetInsertPoint(blocks.back());
    builder.CreateBr(merge);

    // The result is either true or false, depending from which block we come from
    builder.SetInsertPoint(merge);
    llvm::PHINode *phi_node = builder.CreatePHI(builder.getInt1Ty(), group_value.size(), "set_cmp_result");
    phi_node->addIncoming(builder.getInt1(!is_or_cmp), blocks[blocks.size() - 2]);
    phi_node->addIncoming(builder.getInt1(is_or_cmp), blocks.back());
    return phi_node;
}

std::optional<llvm::Value *> Generator::Expression::generate_binary_op_scalar( //
    llvm::IRBuilder<> &builder,                                                //
    GenerationContext &ctx,                                                    //
    garbage_type &garbage,                                                     //
    const unsigned int expr_depth,                                             //
    const BinaryOpNode *bin_op_node,                                           //
    const std::string &type_str,                                               //
    llvm::Value *lhs,                                                          //
    llvm::Value *rhs                                                           //
) {
    const FakeBinaryOpNode bin_op = {
        bin_op_node->operator_token, //
        bin_op_node->left.get(),     //
        bin_op_node->right.get(),    //
        bin_op_node->type,           //
        bin_op_node->is_shorthand    //
    };
    return generate_binary_op_scalar(builder, ctx, garbage, expr_depth, &bin_op, type_str, lhs, rhs);
}

std::optional<llvm::Value *> Generator::Expression::generate_binary_op_scalar( //
    llvm::IRBuilder<> &builder,                                                //
    GenerationContext &ctx,                                                    //
    garbage_type &garbage,                                                     //
    const unsigned int expr_depth,                                             //
    const FakeBinaryOpNode *bin_op_node,                                       //
    const std::string &type_str,                                               //
    llvm::Value *lhs,                                                          //
    llvm::Value *rhs                                                           //
) {
    const auto &check_for_string_lit_garbage = [lhs, rhs, bin_op_node, &garbage, expr_depth]() {
        if (bin_op_node->left->type->to_string() == "str") {
            const bool left_is_literal =                                                       //
                bin_op_node->left->get_variation() == ExpressionNode::Variation::LITERAL       //
                || (bin_op_node->left->get_variation() == ExpressionNode::Variation::TYPE_CAST //
                       && bin_op_node->right->as<TypeCastNode>()->expr->get_variation() == ExpressionNode::Variation::LITERAL);
            if (left_is_literal) {
                if (garbage.find(expr_depth) == garbage.end()) {
                    std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>> vec{{bin_op_node->left->type, lhs}};
                    garbage[expr_depth] = std::move(vec);
                } else {
                    garbage.at(expr_depth).emplace_back(bin_op_node->left->type, lhs);
                }
            }
        }
        if (bin_op_node->right->type->to_string() == "str") {
            const bool right_is_literal =                                                       //
                bin_op_node->right->get_variation() == ExpressionNode::Variation::LITERAL       //
                || (bin_op_node->right->get_variation() == ExpressionNode::Variation::TYPE_CAST //
                       && bin_op_node->right->as<TypeCastNode>()->expr->get_variation() == ExpressionNode::Variation::LITERAL);
            if (right_is_literal) {
                if (garbage.find(expr_depth) == garbage.end()) {
                    std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>> vec{{bin_op_node->right->type, rhs}};
                    garbage[expr_depth] = std::move(vec);
                } else {
                    garbage.at(expr_depth).emplace_back(bin_op_node->right->type, rhs);
                }
            }
        }
    };
    switch (bin_op_node->operator_token) {
        default:
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        case TOK_PLUS:
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE && lhs->getType()->isIntegerTy()) {
                return builder.CreateAdd(lhs, rhs, "add_res");
            }
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64"    //
                || type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64" //
            ) {
                return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_add"), {lhs, rhs}, "safe_add_res");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFAdd(lhs, rhs, "faddtmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                check_for_string_lit_garbage();
                return Module::String::generate_string_addition(builder, //
                    ctx.scope, ctx.allocations,                          //
                    garbage, expr_depth + 1,                             //
                    lhs, bin_op_node->left,                              //
                    rhs, bin_op_node->right,                             //
                    bin_op_node->is_shorthand                            //
                );
            }
            break;
        case TOK_MINUS:
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE && lhs->getType()->isIntegerTy()) {
                return builder.CreateSub(lhs, rhs, "sub_res");
            }
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64"    //
                || type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64" //
            ) {
                return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_sub"), {lhs, rhs}, "safe_sub_res");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFSub(lhs, rhs, "fsubtmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            break;
        case TOK_MULT:
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE && lhs->getType()->isIntegerTy()) {
                return builder.CreateMul(lhs, rhs, "mul_res");
            }
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64"    //
                || type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64" //
            ) {
                return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_mul"), {lhs, rhs}, "safe_mul_res");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFMul(lhs, rhs, "fmultmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            break;
        case TOK_DIV:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateUDiv(lhs, rhs, "udiv_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_div"), {lhs, rhs}, "safe_udiv_res");
            } else if (type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateSDiv(lhs, rhs, "sdiv_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_div"), {lhs, rhs}, "safe_sdiv_res");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFDiv(lhs, rhs, "fdivtmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            break;
        case TOK_POW:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64"    //
                || type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64" //
            ) {
                return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_pow"), {lhs, rhs}, "pow_res");
            } else if (type_str == "f32") {
                return builder.CreateCall(c_functions.at(POWF), {lhs, rhs}, "powf_res");
            } else if (type_str == "f64") {
                return builder.CreateCall(c_functions.at(POW), {lhs, rhs}, "pow_res");
            }
            break;
        case TOK_MOD:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateURem(lhs, rhs, "urem_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_mod"), {lhs, rhs}, "safe_umod_res");
            } else if (type_str == "i8" || type_str == "u16" || type_str == "i32" || type_str == "i64") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateSRem(lhs, rhs, "srem_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_mod"), {lhs, rhs}, "safe_smod_res");
            }
            break;
        case TOK_LESS:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return builder.CreateICmpULT(lhs, rhs, "ucmptmp");
            } else if (type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSLT(lhs, rhs, "icmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOLT(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                check_for_string_lit_garbage();
                return Logical::generate_string_cmp_lt(builder, lhs, bin_op_node->left, rhs, bin_op_node->right);
            }
            break;
        case TOK_GREATER:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return builder.CreateICmpUGT(lhs, rhs, "ucmptmp");
            } else if (type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSGT(lhs, rhs, "icmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOGT(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                check_for_string_lit_garbage();
                return Logical::generate_string_cmp_gt(builder, lhs, bin_op_node->left, rhs, bin_op_node->right);
            }
            break;
        case TOK_LESS_EQUAL:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return builder.CreateICmpULE(lhs, rhs, "ucmptmp");
            } else if (type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSLE(lhs, rhs, "icmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOLE(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                check_for_string_lit_garbage();
                return Logical::generate_string_cmp_le(builder, lhs, bin_op_node->left, rhs, bin_op_node->right);
            }
            break;
        case TOK_GREATER_EQUAL:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return builder.CreateICmpUGE(lhs, rhs, "ucmptmp");
            } else if (type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSGE(lhs, rhs, "icmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOGE(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                check_for_string_lit_garbage();
                return Logical::generate_string_cmp_ge(builder, lhs, bin_op_node->left, rhs, bin_op_node->right);
            }
            break;
        case TOK_EQUAL_EQUAL:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return builder.CreateICmpEQ(lhs, rhs, "ucmptmp");
            } else if (type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpEQ(lhs, rhs, "icmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOEQ(lhs, rhs, "fcmptmp");
            } else if (type_str == "bool") {
                return builder.CreateICmpEQ(lhs, rhs, "bcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                check_for_string_lit_garbage();
                return Logical::generate_string_cmp_eq(builder, lhs, bin_op_node->left, rhs, bin_op_node->right);
            }
            switch (bin_op_node->left->type->get_variation()) {
                default:
                    break;
                case Type::Variation::ENUM: {
                    if (bin_op_node->right->type->get_variation() == Type::Variation::ENUM) {
                        return builder.CreateICmpEQ(lhs, rhs, "enumeq");
                    }
                    break;
                }
                case Type::Variation::OPAQUE: {
                    if (bin_op_node->right->type->get_variation() == Type::Variation::OPAQUE) {
                        return builder.CreateICmpEQ(lhs, rhs, "ptr_cmp");
                    }
                    break;
                }
                case Type::Variation::OPTIONAL: {
                    if (bin_op_node->right->type->get_variation() == Type::Variation::OPTIONAL) {
                        return generate_optional_cmp(builder, ctx, garbage, expr_depth, lhs, bin_op_node->left, rhs, bin_op_node->right,
                            true);
                    }
                    break;
                }
                case Type::Variation::VARIANT: {
                    if (bin_op_node->right->type->get_variation() != Type::Variation::VARIANT) {
                        break;
                    }
                    llvm::Type *variant_type = IR::get_type(ctx.parent->getParent(), bin_op_node->left->type).first;
                    const auto left_variation = bin_op_node->left->get_variation();
                    const auto right_variation = bin_op_node->right->get_variation();
                    if (left_variation == ExpressionNode::Variation::TYPE_CAST) {
                        const auto *lhs_cast = bin_op_node->left->as<TypeCastNode>();
                        if (lhs_cast->expr->get_variation() == ExpressionNode::Variation::TYPE) {
                            // The lhs is the "type" value of the comparison and the rhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, rhs, 0, "active_type_ptr");
                            rhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpEQ(lhs, rhs, "var_holds_type");
                        }
                    } else if (right_variation == ExpressionNode::Variation::TYPE_CAST) {
                        const auto *rhs_cast = bin_op_node->right->as<TypeCastNode>();
                        if (rhs_cast->expr->get_variation() == ExpressionNode::Variation::TYPE) {
                            // The rhs is the "type" value of the comparison and the lhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, lhs, 0, "active_type_ptr");
                            lhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpEQ(lhs, rhs, "var_holds_type");
                        }
                    } else if (left_variation == ExpressionNode::Variation::LITERAL) {
                        const auto *lhs_lit = bin_op_node->left->as<LiteralNode>();
                        if (std::holds_alternative<LitVariantTag>(lhs_lit->value)) {
                            // The lhs is the "type" value of the comparison and the rhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, rhs, 0, "active_type_ptr");
                            rhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpEQ(lhs, rhs, "var_holds_type");
                        }
                    } else if (right_variation == ExpressionNode::Variation::LITERAL) {
                        const auto *rhs_lit = bin_op_node->right->as<LiteralNode>();
                        if (std::holds_alternative<LitVariantTag>(rhs_lit->value)) {
                            // The rhs is the "type" value of the comparison and the lhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, lhs, 0, "active_type_ptr");
                            lhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpEQ(lhs, rhs, "var_holds_type");
                        }
                    }
                    // Both sides are "real" variant types
                    return generate_variant_cmp(builder, ctx, lhs, bin_op_node->left, rhs, bin_op_node->right, true);
                }
            }
            break;
        case TOK_NOT_EQUAL:
            if (type_str == "u8" || type_str == "u16" || type_str == "u32" || type_str == "u64") {
                return builder.CreateICmpNE(lhs, rhs, "ucmptmp");
            } else if (type_str == "i8" || type_str == "i16" || type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpNE(lhs, rhs, "icmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpONE(lhs, rhs, "fcmptmp");
            } else if (type_str == "bool") {
                return builder.CreateICmpNE(lhs, rhs, "bcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Logical::generate_string_cmp_neq(builder, lhs, bin_op_node->left, rhs, bin_op_node->right);
            }
            switch (bin_op_node->left->type->get_variation()) {
                default:
                    break;
                case Type::Variation::ENUM: {
                    if (bin_op_node->right->type->get_variation() != Type::Variation::ENUM) {
                        break;
                    }
                    return builder.CreateICmpNE(lhs, rhs, "enumneq");
                }
                case Type::Variation::OPAQUE: {
                    if (bin_op_node->right->type->get_variation() == Type::Variation::OPAQUE) {
                        return builder.CreateICmpNE(lhs, rhs, "ptr_cmp");
                    }
                    break;
                }
                case Type::Variation::OPTIONAL: {
                    if (bin_op_node->right->type->get_variation() != Type::Variation::OPTIONAL) {
                        break;
                    }
                    return generate_optional_cmp(builder, ctx, garbage, expr_depth, lhs, bin_op_node->left, rhs, bin_op_node->right, false);
                }
                case Type::Variation::VARIANT: {
                    if (bin_op_node->right->type->get_variation() != Type::Variation::VARIANT) {
                        break;
                    }
                    llvm::Type *variant_type = IR::get_type(ctx.parent->getParent(), bin_op_node->left->type).first;
                    const auto left_variation = bin_op_node->left->get_variation();
                    const auto right_variation = bin_op_node->right->get_variation();
                    if (left_variation == ExpressionNode::Variation::TYPE_CAST) {
                        const auto *lhs_cast = bin_op_node->left->as<TypeCastNode>();
                        if (lhs_cast->expr->get_variation() == ExpressionNode::Variation::TYPE) {
                            // The lhs is the "type" value of the comparison and the rhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, rhs, 0, "active_type_ptr");
                            rhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpNE(lhs, rhs, "var_holds_not_type");
                        }
                    } else if (right_variation == ExpressionNode::Variation::TYPE_CAST) {
                        const auto *rhs_cast = bin_op_node->right->as<TypeCastNode>();
                        if (rhs_cast->expr->get_variation() == ExpressionNode::Variation::TYPE) {
                            // The rhs is the "type" value of the comparison and the lhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, lhs, 0, "active_type_ptr");
                            lhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpNE(lhs, rhs, "var_holds_not_type");
                        }
                    } else if (left_variation == ExpressionNode::Variation::LITERAL) {
                        const auto *lhs_lit = bin_op_node->left->as<LiteralNode>();
                        if (std::holds_alternative<LitVariantTag>(lhs_lit->value)) {
                            // The lhs is the "type" value of the comparison and the rhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, rhs, 0, "active_type_ptr");
                            rhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpNE(lhs, rhs, "var_holds_not_type");
                        }
                    } else if (right_variation == ExpressionNode::Variation::LITERAL) {
                        const auto *rhs_lit = bin_op_node->right->as<LiteralNode>();
                        if (std::holds_alternative<LitVariantTag>(rhs_lit->value)) {
                            // The rhs is the "type" value of the comparison and the lhs is the actual variant
                            llvm::Value *active_type_ptr = builder.CreateStructGEP(variant_type, lhs, 0, "active_type_ptr");
                            lhs = IR::aligned_load(builder, builder.getInt8Ty(), active_type_ptr, "active_type");
                            return builder.CreateICmpNE(lhs, rhs, "var_holds_not_type");
                        }
                    }
                    // Both sides are "real" variant types
                    return generate_variant_cmp(builder, ctx, lhs, bin_op_node->left, rhs, bin_op_node->right, false);
                }
            }
            break;
        case TOK_OPT_DEFAULT: {
            // Both the lhs and rhs expressions have already been parsed, this means we can do a simple select based on whether the
            // lhs optional has a value stored in it
            if (lhs->getType()->isPointerTy()) {
                llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), bin_op_node->left->type, false);
                lhs = IR::aligned_load(builder, opt_struct_type, lhs, "loaded_lhs");
            }
            llvm::Value *const has_value = builder.CreateExtractValue(lhs, {0}, "has_value");
            llvm::Value *const lhs_value = builder.CreateExtractValue(lhs, {1}, "value");
            if (!bin_op_node->type->is_freeable()) {
                return builder.CreateSelect(has_value, lhs_value, rhs, "selected_value");
            }
            // lhs_value is a borrowed pointer into the optional's storage, rhs is already owned.
            // Only clone the lhs to avoid leaking rhs.
            llvm::Function *const clone_fn = Memory::memory_functions.at("clone");
            llvm::Value *const type_id = builder.getInt32(bin_op_node->type->get_id());
            llvm::BasicBlock *const current_block = builder.GetInsertBlock();
            llvm::BasicBlock *const has_value_block = llvm::BasicBlock::Create(context, "opt_default_has_value", ctx.parent);
            llvm::BasicBlock *const merge_block = llvm::BasicBlock::Create(context, "opt_default_merge", ctx.parent);

            builder.SetInsertPoint(current_block);
            builder.CreateCondBr(has_value, has_value_block, merge_block);

            builder.SetInsertPoint(has_value_block);
            builder.CreateCall(clone_fn, {lhs_value, scratchspace, type_id});
            llvm::Value *const cloned_lhs = IR::aligned_load(builder, PTR_TY, scratchspace, "cloned_value");
            builder.CreateBr(merge_block);

            builder.SetInsertPoint(merge_block);
            llvm::PHINode *const phi = builder.CreatePHI(PTR_TY, 2, "opt_default_result");
            phi->addIncoming(cloned_lhs, has_value_block);
            phi->addIncoming(rhs, current_block);
            return phi;
        }
        case TOK_CATCH: {
            llvm::BasicBlock *const current_block = builder.GetInsertBlock();
            llvm::BasicBlock *const catch_expr_block = llvm::BasicBlock::Create(context, "catch_expr", ctx.parent);
            llvm::BasicBlock *const catch_expr_merge_block = llvm::BasicBlock::Create(context, "catch_expr_merge", ctx.parent);

            builder.SetInsertPoint(current_block);
            llvm::Value *const call_had_error = last_err_values.first;
            builder.CreateCondBr(call_had_error, catch_expr_block, catch_expr_merge_block);

            builder.SetInsertPoint(catch_expr_block);
            const group_mapping rhs_value = generate_expression(builder, ctx, garbage, expr_depth, bin_op_node->right);
            if (!rhs_value.has_value()) {
                return std::nullopt;
            }
            if (rhs_value.value().size() > 1) {
                // Linearization of function calls returning groups is not possible yet
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            rhs = rhs_value.value().front();
            builder.CreateBr(catch_expr_merge_block);

            builder.SetInsertPoint(catch_expr_merge_block);
            llvm::PHINode *expr_result = builder.CreatePHI(lhs->getType(), 2, "catch_expr_result");
            expr_result->addIncoming(lhs, current_block);
            expr_result->addIncoming(rhs, catch_expr_block);
            return expr_result;
        }
        case TOK_AND:
        case TOK_OR:
            // These should now be handled in generate_binary_op with proper short-circuit evaluation
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<llvm::Value *> Generator::Expression::generate_optional_cmp( //
    llvm::IRBuilder<> &builder,                                            //
    GenerationContext &ctx,                                                //
    garbage_type &garbage,                                                 //
    const unsigned int expr_depth,                                         //
    llvm::Value *lhs,                                                      //
    const ExpressionNode *lhs_expr,                                        //
    llvm::Value *rhs,                                                      //
    const ExpressionNode *rhs_expr,                                        //
    const bool eq                                                          //
) {
    // If both sides are the 'none' literal, we can return a constant as the result directly
    if (lhs_expr->type->to_string() == "void?" && rhs_expr->type->to_string() == "void?") {
        return builder.getInt1(eq ? 1 : 0);
    }
    // First, we check if one of the sides is a TypeCast Node, and if one side is a TypeCast we can check if the base type
    // is of type `void?`, indicating that we check if one side is the 'none' literal.
    if (lhs_expr->get_variation() == ExpressionNode::Variation::TYPE_CAST) {
        const auto *lhs_type_cast = lhs_expr->as<TypeCastNode>();
        if (lhs_type_cast->expr->type->to_string() == "void?") {
            // We can just extract the first bit of the rhs and return it's (negated) value directly
            if (lhs->getType()->isPointerTy()) {
                // The optional is a function argument
                llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), rhs_expr->type, false);
                rhs = IR::aligned_load(builder, opt_struct_type, rhs, "loaded_rhs");
            }
            llvm::Value *has_value = builder.CreateExtractValue(rhs, {0}, "has_value");
            if (eq) {
                return builder.CreateNot(has_value, "has_no_value");
            } else {
                return has_value;
            }
        }
    }
    if (rhs_expr->get_variation() == ExpressionNode::Variation::TYPE_CAST) {
        const auto *rhs_type_cast = rhs_expr->as<TypeCastNode>();
        if (rhs_type_cast->expr->type->to_string() == "void?") {
            // We can just extract the first bit of the rhs and return it's (negated) value directly
            if (lhs->getType()->isPointerTy()) {
                // The optional is a function argument
                llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), lhs_expr->type, false);
                lhs = IR::aligned_load(builder, opt_struct_type, lhs, "loaded_lhs");
            }
            llvm::Value *has_value = builder.CreateExtractValue(lhs, {0}, "has_value");
            if (eq) {
                return builder.CreateNot(has_value, "has_no_value");
            } else {
                return has_value;
            }
        }
    }
    // If both sides are "real" optionals, we first compare if their 'has_value' fields match, and only if the 'has_value'
    // fields of both optional variables are 1 we continue to compare the actual values of the optional.

    // Create the basic blocks needed for the comparison
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    llvm::BasicBlock *one_no_value_block = llvm::BasicBlock::Create(context, "one_no_value", ctx.parent);
    llvm::BasicBlock *both_value_block = llvm::BasicBlock::Create(context, "both_value", ctx.parent);
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge", ctx.parent);

    // Branch to the blocks accordingly
    builder.SetInsertPoint(inserter);
    llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), lhs_expr->type, false);
    if (lhs->getType()->isPointerTy()) {
        lhs = IR::aligned_load(builder, opt_struct_type, lhs, "loaded_lhs");
    }
    if (rhs->getType()->isPointerTy()) {
        rhs = IR::aligned_load(builder, opt_struct_type, rhs, "loaded_rhs");
    }
    llvm::Value *lhs_has_value = builder.CreateExtractValue(lhs, {0}, "lhs_has_value");
    llvm::Value *rhs_has_value = builder.CreateExtractValue(rhs, {0}, "rhs_has_value");
    llvm::Value *both_have_value = builder.CreateAnd(lhs_has_value, rhs_has_value, "both_have_value");
    builder.CreateCondBr(both_have_value, both_value_block, one_no_value_block);

    // The optionals are still equal if both have no value stored in them
    builder.SetInsertPoint(one_no_value_block);
    llvm::Value *both_empty;
    if (eq) {
        both_empty = builder.CreateICmpEQ(lhs_has_value, rhs_has_value, "both_empty");
    } else {
        both_empty = builder.CreateICmpNE(lhs_has_value, rhs_has_value, "both_differ");
    }
    builder.CreateBr(merge_block);

    // The optionals are equal if their values match
    builder.SetInsertPoint(both_value_block);
    llvm::Value *lhs_value = builder.CreateExtractValue(lhs, {1}, "lhs_value");
    llvm::Value *rhs_value = builder.CreateExtractValue(rhs, {1}, "rhs_value");
    const auto *lhs_opt_type = lhs_expr->type->as<OptionalType>();
    const std::string base_type_str = lhs_opt_type->base_type->to_string();
    const FakeBinaryOpNode bin_op = {
        eq ? TOK_EQUAL_EQUAL : TOK_NOT_EQUAL, // The operation
        lhs_expr,                             // unused
        rhs_expr,                             // unused
        lhs_expr->type,                       // unused
        false                                 // unused
    };
    // The result of this call is the result of the comparison of the two element types, so it will be 1 if they are equal
    std::optional<llvm::Value *> result_value = generate_binary_op_scalar(              //
        builder, ctx, garbage, expr_depth, &bin_op, base_type_str, lhs_value, rhs_value //
    );
    if (!result_value.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    builder.CreateBr(merge_block);

    // In the merge block we check from which block we come and choose the boolean value to use accordingly through phi nodes
    builder.SetInsertPoint(merge_block);
    llvm::PHINode *selected_value = builder.CreatePHI(builder.getInt1Ty(), 2, "result");
    selected_value->addIncoming(both_empty, one_no_value_block);
    selected_value->addIncoming(result_value.value(), both_value_block);
    return selected_value;
}

std::optional<llvm::Value *> Generator::Expression::generate_variant_cmp( //
    llvm::IRBuilder<> &builder,                                           //
    GenerationContext &ctx,                                               //
    llvm::Value *lhs,                                                     //
    const ExpressionNode *lhs_expr,                                       //
    llvm::Value *rhs,                                                     //
    [[maybe_unused]] const ExpressionNode *rhs_expr,                      //
    const bool eq                                                         //
) {
    // Ge the variant type of the comparison
    const auto *variant_type = lhs_expr->type->as<VariantType>();
    ASSERT(rhs_expr->type->get_variation() == Type::Variation::VARIANT);
    const auto &possible_types = variant_type->get_possible_types();
    // First we create all the basic blocks we need
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    llvm::BasicBlock *same_type_check = llvm::BasicBlock::Create(context, "same_type_check", ctx.parent);
    llvm::BasicBlock *switch_on_type = llvm::BasicBlock::Create(context, "switch_on_type", ctx.parent);
    std::vector<llvm::BasicBlock *> type_switch_blocks;
    type_switch_blocks.reserve(possible_types.size());
    for (auto type_it = possible_types.begin(); type_it != possible_types.end(); ++type_it) {
        const unsigned int type_id = 1 + std::distance(possible_types.begin(), type_it);
        const std::string block_name = "switch_type_" + std::to_string(type_id);
        type_switch_blocks.push_back(llvm::BasicBlock::Create(context, block_name, ctx.parent));
    }
    llvm::BasicBlock *switch_merge = llvm::BasicBlock::Create(context, "switch_merge", ctx.parent);
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "merge", ctx.parent);

    builder.SetInsertPoint(inserter);
    builder.CreateBr(same_type_check);

    builder.SetInsertPoint(same_type_check);
    llvm::Type *var_type = IR::get_type(ctx.parent->getParent(), lhs_expr->type).first;
    llvm::Value *lhs_type_ptr = builder.CreateStructGEP(var_type, lhs, 0, "lhs_type_ptr");
    llvm::Value *lhs_type = IR::aligned_load(builder, builder.getInt8Ty(), lhs_type_ptr, "lhs_type");
    llvm::Value *rhs_type_ptr = builder.CreateStructGEP(var_type, rhs, 0, "rhs_type_ptr");
    llvm::Value *rhs_type = IR::aligned_load(builder, builder.getInt8Ty(), rhs_type_ptr, "rhs_type");
    llvm::Value *types_match = builder.CreateICmpEQ(lhs_type, rhs_type, "types_match");
    builder.CreateCondBr(types_match, switch_on_type, merge);

    builder.SetInsertPoint(switch_on_type);
    llvm::SwitchInst *type_switch = builder.CreateSwitch(lhs_type, merge, possible_types.size());
    for (auto type_it = possible_types.begin(); type_it != possible_types.end(); ++type_it) {
        const unsigned int type_id = 1 + std::distance(possible_types.begin(), type_it);
        type_switch->addCase(builder.getInt8(type_id), type_switch_blocks.at(type_id - 1));
    }

    std::vector<llvm::Value *> switch_values;
    switch_values.reserve(possible_types.size());
    for (auto type_it = possible_types.begin(); type_it != possible_types.end(); ++type_it) {
        const unsigned int idx = std::distance(possible_types.begin(), type_it);
        builder.SetInsertPoint(type_switch_blocks.at(idx));
        const auto &pair = IR::get_type(ctx.parent->getParent(), type_it->second);
        const unsigned int type_size = pair.second.second ? 8 : Allocation::get_type_size(ctx.parent->getParent(), pair.first);
        switch_values.push_back(builder.getInt64(type_size));
        builder.CreateBr(switch_merge);
    }

    builder.SetInsertPoint(switch_merge);
    llvm::PHINode *size_select = builder.CreatePHI(builder.getInt64Ty(), possible_types.size(), "size_select");
    for (size_t i = 0; i < possible_types.size(); i++) {
        size_select->addIncoming(switch_values.at(i), type_switch_blocks.at(i));
    }
    // Now call memcmp to compare the first `size_select` bytes of the value field of both variants
    llvm::Function *memcmp_fn = c_functions.at(MEMCMP);
    llvm::Value *lhs_val_ptr = builder.CreateStructGEP(var_type, lhs, 1, "lhs_val_ptr");
    llvm::Value *rhs_val_ptr = builder.CreateStructGEP(var_type, rhs, 1, "rhs_val_ptr");
    llvm::Value *memcmp_result = builder.CreateCall(memcmp_fn, {lhs_val_ptr, rhs_val_ptr, size_select}, "memcmp_result");
    llvm::Value *values_equal = builder.CreateICmpEQ(memcmp_result, builder.getInt32(0), "values_equal");
    llvm::Value *value_cmp = eq ? values_equal : builder.CreateNot(values_equal, "values_not_equal");
    builder.CreateBr(merge);

    builder.SetInsertPoint(merge);
    llvm::PHINode *result_select = builder.CreatePHI(builder.getInt1Ty(), 2, "result_select");
    result_select->addIncoming(builder.getInt1(!eq), same_type_check);
    result_select->addIncoming(builder.getInt1(!eq), switch_on_type);
    result_select->addIncoming(value_cmp, switch_merge);
    return result_select;
}

std::optional<llvm::Value *> Generator::Expression::generate_binary_op_vector( //
    llvm::IRBuilder<> &builder,                                                //
    const BinaryOpNode *bin_op_node,                                           //
    const std::string &type_str,                                               //
    llvm::Value *lhs,                                                          //
    llvm::Value *rhs                                                           //
) {
    const bool is_float = lhs->getType()->getScalarType()->isFloatTy();
    const bool is_signed = bin_op_node->type->to_string()[0] == 'i';
    switch (bin_op_node->operator_token) {
        default:
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        case TOK_PLUS:
            if (is_float) {
                return builder.CreateFAdd(lhs, rhs, "vec_add");
            }
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
                return builder.CreateAdd(lhs, rhs, "vec_add");
            }
            if (Module::Arithmetic::arithmetic_functions.find(type_str + "_safe_add") == Module::Arithmetic::arithmetic_functions.end()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_add"), {lhs, rhs}, "safe_add_res");
            break;
        case TOK_MINUS:
            if (is_float) {
                return builder.CreateFSub(lhs, rhs, "vec_sub");
            }
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
                return builder.CreateSub(lhs, rhs, "vec_sub");
            }
            if (Module::Arithmetic::arithmetic_functions.find(type_str + "_safe_sub") == Module::Arithmetic::arithmetic_functions.end()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_sub"), {lhs, rhs}, "safe_sub_res");
            break;
        case TOK_MULT:
            if (is_float) {
                return builder.CreateFMul(lhs, rhs, "vec_mul");
            }
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
                return builder.CreateMul(lhs, rhs, "vec_mul");
            }
            if (Module::Arithmetic::arithmetic_functions.find(type_str + "_safe_mul") == Module::Arithmetic::arithmetic_functions.end()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_mul"), {lhs, rhs}, "safe_mul_res");
            break;
        case TOK_DIV:
            if (is_float) {
                return builder.CreateFDiv(lhs, rhs, "vec_div");
            }
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE) {
                return builder.CreateSDiv(lhs, rhs, "vec_div");
            }
            if (Module::Arithmetic::arithmetic_functions.find(type_str + "_safe_div") == Module::Arithmetic::arithmetic_functions.end()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_div"), {lhs, rhs}, "safe_div_res");
            break;
        case TOK_LESS: {
            llvm::Value *cmp_result;
            if (is_float) {
                cmp_result = builder.CreateFCmpOLT(lhs, rhs, "vec_lt");
            } else if (is_signed) {
                cmp_result = builder.CreateICmpSLT(lhs, rhs, "vec_lt");
            } else {
                cmp_result = builder.CreateICmpULT(lhs, rhs, "vec_lt");
            }
            // Create the intrinsic declaration for vector_reduce_and
            llvm::Type *cmp_type = cmp_result->getType();
            llvm::Function *reduce_and_fn = llvm::Intrinsic::getDeclaration(                          //
                builder.GetInsertBlock()->getModule(), llvm::Intrinsic::vector_reduce_and, {cmp_type} //
            );
            // Call the intrinsic to reduce the vector to a single boolean
            return builder.CreateCall(reduce_and_fn, {cmp_result}, "reduce_lt");
        }
        case TOK_GREATER: {
            llvm::Value *cmp_result;
            if (is_float) {
                cmp_result = builder.CreateFCmpOGT(lhs, rhs, "vec_gt");
            } else if (is_signed) {
                cmp_result = builder.CreateICmpSGT(lhs, rhs, "vec_gt");
            } else {
                cmp_result = builder.CreateICmpUGT(lhs, rhs, "vec_gt");
            }
            llvm::Type *cmp_type = cmp_result->getType();
            llvm::Function *reduce_and_fn = llvm::Intrinsic::getDeclaration(                          //
                builder.GetInsertBlock()->getModule(), llvm::Intrinsic::vector_reduce_and, {cmp_type} //
            );
            return builder.CreateCall(reduce_and_fn, {cmp_result}, "reduce_gt");
        }
        case TOK_LESS_EQUAL: {
            llvm::Value *cmp_result;
            if (is_float) {
                cmp_result = builder.CreateFCmpOLE(lhs, rhs, "vec_le");
            } else if (is_signed) {
                cmp_result = builder.CreateICmpSLE(lhs, rhs, "vec_le");
            } else {
                cmp_result = builder.CreateICmpULE(lhs, rhs, "vec_le");
            }
            llvm::Type *cmp_type = cmp_result->getType();
            llvm::Function *reduce_and_fn = llvm::Intrinsic::getDeclaration(                          //
                builder.GetInsertBlock()->getModule(), llvm::Intrinsic::vector_reduce_and, {cmp_type} //
            );
            return builder.CreateCall(reduce_and_fn, {cmp_result}, "reduce_le");
        }
        case TOK_GREATER_EQUAL: {
            llvm::Value *cmp_result;
            if (is_float) {
                cmp_result = builder.CreateFCmpOGE(lhs, rhs, "vec_ge");
            } else if (is_signed) {
                cmp_result = builder.CreateICmpSGE(lhs, rhs, "vec_ge");
            } else {
                cmp_result = builder.CreateICmpUGE(lhs, rhs, "vec_ge");
            }
            llvm::Type *cmp_type = cmp_result->getType();
            llvm::Function *reduce_and_fn = llvm::Intrinsic::getDeclaration(                          //
                builder.GetInsertBlock()->getModule(), llvm::Intrinsic::vector_reduce_and, {cmp_type} //
            );
            return builder.CreateCall(reduce_and_fn, {cmp_result}, "reduce_ge");
        }
        case TOK_EQUAL_EQUAL: {
            llvm::Value *cmp_result;
            if (is_float) {
                cmp_result = builder.CreateFCmpOEQ(lhs, rhs, "vec_eq");
            } else {
                cmp_result = builder.CreateICmpEQ(lhs, rhs, "vec_eq");
            }
            llvm::Type *cmp_type = cmp_result->getType();
            llvm::Function *reduce_and_fn = llvm::Intrinsic::getDeclaration(                          //
                builder.GetInsertBlock()->getModule(), llvm::Intrinsic::vector_reduce_and, {cmp_type} //
            );
            return builder.CreateCall(reduce_and_fn, {cmp_result}, "reduce_eq");
        }
        case TOK_NOT_EQUAL: {
            llvm::Value *cmp_result;
            if (is_float) {
                cmp_result = builder.CreateFCmpONE(lhs, rhs, "vec_ne");
            } else {
                cmp_result = builder.CreateICmpNE(lhs, rhs, "vec_ne");
            }
            llvm::Type *cmp_type = cmp_result->getType();
            llvm::Function *reduce_and_fn = llvm::Intrinsic::getDeclaration(                          //
                builder.GetInsertBlock()->getModule(), llvm::Intrinsic::vector_reduce_and, {cmp_type} //
            );
            return builder.CreateCall(reduce_and_fn, {cmp_result}, "reduce_ne");
        }
        case TOK_AND:
            if (type_str != "bool8") {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateAnd(lhs, rhs, "vec_i8_and");
            break;
        case TOK_OR:
            if (type_str != "bool8") {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateOr(lhs, rhs, "vec_i8_or");
            break;
    }
    return std::nullopt;
}
