#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "globals.hpp"
#include "lexer/builtins.hpp"
#include "lexer/token.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/instance_call_node_expression.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/expressions/type_node.hpp"
#include "parser/parser.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"

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
            group_map.emplace_back(generate_array_access(builder, ctx, garbage, expr_depth, node));
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
            return generate_call(builder, ctx, node);
        }
        case ExpressionNode::Variation::DATA_ACCESS: {
            const auto *node = expression_node->as<DataAccessNode>();
            return generate_data_access(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::DEFAULT: {
            [[maybe_unused]] const auto *node = expression_node->as<DefaultNode>();
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET); // TODO: Somehow it was unused until now?
            return std::nullopt;
        }
        case ExpressionNode::Variation::GROUP_EXPRESSION: {
            const auto *node = expression_node->as<GroupExpressionNode>();
            return generate_group_expression(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
            const auto *node = expression_node->as<GroupedDataAccessNode>();
            return generate_grouped_data_access(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::INITIALIZER: {
            const auto *node = expression_node->as<InitializerNode>();
            return generate_initializer(builder, ctx, garbage, expr_depth, node);
        }
        case ExpressionNode::Variation::INSTANCE_CALL: {
            const auto *node = expression_node->as<InstanceCallNodeExpression>();
            return generate_instance_call(builder, ctx, node);
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
            return generate_type_cast(builder, ctx, garbage, expr_depth, node, is_reference);
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
            assert(false);
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
            const std::unique_ptr<LiteralNode> tmp_lit_node = std::make_unique<LiteralNode>(lit_value, literal_node->type, true);
            return generate_literal(builder, ctx, garbage, expr_depth, tmp_lit_node.get());
        } else if (lit_type == "float") {
            // Compile-time type of literal which was not resolved to be a different type
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        } else {
            // Type that should not be possible
            assert(false);
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
        return std::vector<llvm::Value *>{llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0))};
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
            assert(value_it != enum_values.end());
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
        const std::optional<unsigned char> id = variant_type->get_idx_of_type(variant_tag.variation_type);
        assert(id.has_value());
        return std::vector<llvm::Value *>{builder.getInt8(id.value())};
    }
    assert(false);
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

    // First, check if this is a function parameter
    for (auto &arg : ctx.parent->args()) {
        if (arg.getName() == variable_node->name) {
            // We return it directly if: It's a string, array, enum, immutable primitive type or data type
            if (ctx.scope->variables.find(arg.getName().str()) != ctx.scope->variables.end()) {
                auto var = ctx.scope->variables.at(arg.getName().str());
                const auto variation = variable_node->type->get_variation();
                if ((primitives.find(var.type->to_string()) != primitives.end() && !var.is_mutable) // is immutable primitive
                    || variation == Type::Variation::ARRAY                                          // is array type
                    || variable_node->type->to_string() == "str"                                    // is string type
                    || variation == Type::Variation::ENUM                                           // is enum type
                    || variation == Type::Variation::DATA                                           // is data type
                ) {
                    return &arg;
                }
            }
        }
    }

    // If not a parameter, handle as local variable
    if (ctx.scope->variables.find(variable_node->name) == ctx.scope->variables.end()) {
        // Error: Undeclared Variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }
    const unsigned int variable_decl_scope = ctx.scope->variables.at(variable_node->name).scope_id;
    llvm::Value *const variable = ctx.allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);
    // The variable is a "function parameter" when it's the context of the ehnhanced for loop, for example. It's set to a function paramter
    // in all the cases where we want to return the pointer to the allocation directly instead of loading it, but this only holds true when
    // the parameter is not a tuple. Tuples are by-value by default but when passed in to a function they are passed by reference, so we
    // still need to load them even when it's a function parameter. This also does not count when the function parameter is mutable. If we
    // have a mutable primitive typed function parameter then a local version of it is allocated in the functions scope, which means we
    // still need to load that value before returning it.
    if (ctx.scope->variables.at(variable_node->name).is_fn_param          // is function parameter
        && !ctx.scope->variables.at(variable_node->name).is_mutable       // is not mutable
        && variable_node->type->get_variation() != Type::Variation::TUPLE // Is not of type tuple
    ) {
        return variable;
    }

    // Get the type that the pointer points to
    auto type = IR::get_type(ctx.parent->getParent(), variable_node->type);

    // Check if the variable is complex, in that case we need to load the pointer first
    llvm::Value *var = variable;
    if (type.second.first) {
        llvm::LoadInst *load = IR::aligned_load(builder, type.first->getPointerTo(), variable, variable_node->name + "_ptr");
        load->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Load ptr to '" + variable_node->name + "'")));
        var = load;
    }
    // If a reference is requested we return the loaded value (this way we do not return a double pointer for complex types)
    if (is_reference) {
        return var;
    }

    // Load the variable's value from the allocation or the pointer to the memory it's located at but only if it's not a complex type, if
    // it's a complex type we return it directly without further loading
    if (type.second.first) {
        return var;
    }
    llvm::LoadInst *load = IR::aligned_load(builder, type.first, var, variable_node->name + "_val");
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
    assert(!interpol_node->string_content.empty());
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
        assert(expr->type->to_string() == "str");
        group_mapping res = generate_expression(builder, ctx, garbage, expr_depth, expr);
        if (!res.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        assert(res.value().size() == 1);
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
            assert(expr->type->to_string() == "str");
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
            [[fallthrough]];
        case Type::Variation::DATA:
            convert_data_type_to_ext(builder, ctx, type, value, args);
            return;
        case Type::Variation::MULTI: {
            const auto *multi_type = type->as<MultiType>();
            // Multi-types need to be passed as structs to extern functions. But the structs themselves need to be passed in 8 byte chunks
            // to the functions too. This means that the multi-type needs to be converted not into a struct but into a multiple of
            // two-component vector types.
            // A vector type of size 2 can be passed to the function directly and does not need to be converted at all
            // A vector type of size 3 is split into a two-component vector type + a scalar value
            // All bigger vector types N / 2 vector tuples (`<T x N> -> <T x 2> x (N / 2)`)
            llvm::Type *element_type = IR::get_type(ctx.parent->getParent(), multi_type->base_type).first;
            const std::string base_type_str = multi_type->base_type->to_string();
            if (base_type_str == "f64" || base_type_str == "i64") {
                for (size_t i = 0; i < multi_type->width; i++) {
                    args.emplace_back(builder.CreateExtractElement(value, builder.getInt64(i)));
                }
                return;
            } else if (multi_type->width == 2) {
                if (base_type_str == "i32") {
                    // We need to pack the two i32s as one i64
                    args.emplace_back(builder.CreateBitCast(value, builder.getInt64Ty()));
                } else {
                    args.emplace_back(value);
                }
                return;
            } else if (multi_type->width == 3) {
                // Extract the first two elements as a vector type
                llvm::VectorType *vec2_type = llvm::VectorType::get(element_type, 2, false);
                llvm::Value *first = builder.CreateExtractElement(value, builder.getInt64(0));
                llvm::Value *second = builder.CreateExtractElement(value, builder.getInt64(1));
                llvm::Value *vec2 = llvm::UndefValue::get(vec2_type);
                vec2 = builder.CreateInsertElement(vec2, first, builder.getInt64(0));
                vec2 = builder.CreateInsertElement(vec2, second, builder.getInt64(1));
                if (base_type_str == "i32") {
                    // Pack the two i32s as one i64
                    args.emplace_back(builder.CreateBitCast(vec2, builder.getInt64Ty()));
                } else {
                    args.emplace_back(vec2);
                }
                // Extract the third value as a scalar element
                llvm::Value *third = builder.CreateExtractElement(value, builder.getInt64(2));
                args.emplace_back(third);
                return;
            }
            // Bigger than size 3. But there are only 2, 3, 4, 8, 16, ... multi-types in Flint, so we know all bigger than 3 are even
            // numbers
            llvm::VectorType *vec2_type = llvm::VectorType::get(element_type, 2, false);
            for (size_t i = 0; i < multi_type->width; i += 2) {
                llvm::Value *first = builder.CreateExtractElement(value, builder.getInt64(i));
                llvm::Value *second = builder.CreateExtractElement(value, builder.getInt64(i + 1));
                llvm::Value *vec2 = llvm::UndefValue::get(vec2_type);
                vec2 = builder.CreateInsertElement(vec2, first, builder.getInt64(0));
                vec2 = builder.CreateInsertElement(vec2, second, builder.getInt64(1));
                if (base_type_str == "i32") {
                    args.emplace_back(builder.CreateBitCast(vec2, builder.getInt64Ty()));
                } else {
                    args.emplace_back(vec2);
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

void Generator::Expression::convert_data_type_to_ext( //
    llvm::IRBuilder<> &builder,                       //
    GenerationContext &ctx,                           //
    const std::shared_ptr<Type> &type,                //
    llvm::Value *const value,                         //
    std::vector<llvm::Value *> &args                  //
) {
    // get the LLVM struct type and its elements
    llvm::Type *_struct_type = IR::get_type(ctx.parent->getParent(), type, false).first;
    assert(_struct_type->isStructTy());
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
    llvm::StructType *struct_type = llvm::cast<llvm::StructType>(_struct_type);
    std::vector<llvm::Type *> elem_types = struct_type->elements();
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
        assert(8 - offset >= elem_size);
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
        assert(elem_offset != 8);
        stacks[1].push(elem_offset);
        offset = elem_offset + elem_size;
    }
    assert(elem_idx == elem_types.size());
    elem_idx = 0;
    // Now we reach the second phase, we have figured out where to put the elements in the respective chunks
    if (stacks[1].empty()) {
        // Special case for when the number of elements in the first stack is equal to the size of the first structure. This means that
        // we pack multiple u8 values into one, for them we can get even i40, i48 or i56 results
        if (stacks[0].size() == first_size) {
            llvm::Value *result = builder.getIntN(first_size * 8, 0);
            for (; elem_idx < elem_types.size(); elem_idx++) {
                llvm::Value *elem_ptr = builder.CreateStructGEP(_struct_type, value, elem_idx);
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
        llvm::Value *elem_ptr = builder.CreateStructGEP(_struct_type, value, elem_idx);
        llvm::Value *elem = IR::aligned_load(builder, elem_types.at(elem_idx), elem_ptr);
        args.emplace_back(elem);
        elem_idx++;
    } else if (stacks[0].size() == 2) {
        if (elem_types.front()->isFloatTy() && elem_types.at(1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Type *vec2_type = llvm::VectorType::get(elem_types.at(elem_idx), 2, false);
            llvm::Value *result = IR::get_default_value_of_type(vec2_type);
            llvm::Value *elem_1_ptr = builder.CreateStructGEP(_struct_type, value, elem_idx);
            llvm::Value *elem_1 = IR::aligned_load(builder, elem_types.at(elem_idx), elem_1_ptr);
            result = builder.CreateInsertElement(result, elem_1, static_cast<size_t>(0));
            elem_idx++;
            llvm::Value *elem_2_ptr = builder.CreateStructGEP(_struct_type, value, elem_idx);
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
            llvm::Value *elem_ptr = builder.CreateStructGEP(_struct_type, value, actual_elem_idx);
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
        assert(stacks[0].empty());
    }
    if (stacks[1].size() == 1) {
        llvm::Value *elem_ptr = builder.CreateStructGEP(_struct_type, value, elem_idx);
        llvm::Value *elem = IR::aligned_load(builder, elem_types.at(elem_idx), elem_ptr);
        args.emplace_back(elem);
        elem_idx++;
    } else if (stacks[1].size() == 2) {
        if (elem_types.at(stacks_0_size)->isFloatTy() && elem_types.at(stacks_0_size + 1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Type *vec2_type = llvm::VectorType::get(elem_types.at(elem_idx), 2, false);
            llvm::Value *result = IR::get_default_value_of_type(vec2_type);
            llvm::Value *elem_1_ptr = builder.CreateStructGEP(_struct_type, value, elem_idx);
            llvm::Value *elem_1 = IR::aligned_load(builder, elem_types.at(elem_idx), elem_1_ptr);
            result = builder.CreateInsertElement(result, elem_1, static_cast<size_t>(0));
            elem_idx++;
            llvm::Value *elem_2_ptr = builder.CreateStructGEP(_struct_type, value, elem_idx);
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
            llvm::Value *elem_ptr = builder.CreateStructGEP(_struct_type, value, actual_elem_idx);
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
        assert(stacks[1].empty());
    }
    assert(elem_idx == elem_types.size());
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
        case Type::Variation::MULTI: {
            const auto *multi_type = type->as<MultiType>();
            llvm::Type *element_type = IR::get_type(ctx.parent->getParent(), multi_type->base_type).first;
            llvm::VectorType *target_vector_type = llvm::VectorType::get(element_type, multi_type->width, false);
            llvm::VectorType *vec2_i32 = llvm::VectorType::get(builder.getInt32Ty(), 2, false);
            const std::string base_type_str = multi_type->base_type->to_string();
            if (base_type_str == "f64" || base_type_str == "i64") {
                llvm::Value *result_vec = llvm::UndefValue::get(target_vector_type);
                for (size_t i = 0; i < multi_type->width; i++) {
                    llvm::Value *elem_i = builder.CreateExtractValue(value, i);
                    result_vec = builder.CreateInsertElement(result_vec, elem_i, builder.getInt64(i));
                }
                value = result_vec;
            } else if (multi_type->width == 2) {
                if (base_type_str == "i32") {
                    value = builder.CreateBitCast(value, vec2_i32);
                } else {
                    // vec2 is returned as <2 x T> directly for floats - no conversion needed
                    return;
                }
            } else if (multi_type->width == 3) {
                // vec3 is returned as { <2 x T>, T } struct from extern calls
                assert(value->getType()->isStructTy());

                // Extract the <2 x T> part
                llvm::Value *vec2_part = builder.CreateExtractValue(value, 0, "vec2_part");
                llvm::Value *scalar_part = builder.CreateExtractValue(value, 1, "scalar_part");

                // Reconstruct <3 x T> vector
                llvm::Value *result_vec = llvm::UndefValue::get(target_vector_type);

                // The first element of the struct is `i64` not a vector so we need to cast it first
                if (base_type_str == "i32") {
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
                // vecN (N > 3) is returned as { <2 x T>, <2 x T>, ... } struct from extern calls
                assert(value->getType()->isStructTy());
                llvm::Value *result_vec = llvm::UndefValue::get(target_vector_type);
                size_t element_index = 0;

                // Extract each <2 x T> chunk and rebuild the original vector
                for (size_t chunk = 0; chunk < (multi_type->width + 1) / 2; chunk++) {
                    llvm::Value *chunk_vec = builder.CreateExtractValue(value, chunk, "chunk_vec");

                    // The first element of the struct is `i64` not a vector so we need to cast it first
                    if (base_type_str == "i32") {
                        chunk_vec = builder.CreateBitCast(chunk_vec, vec2_i32);
                    }

                    // Extract elements from this chunk
                    for (size_t i = 0; i < 2 && element_index < multi_type->width; i++, element_index++) {
                        llvm::Value *elem = builder.CreateExtractElement(chunk_vec, builder.getInt64(i));
                        result_vec = builder.CreateInsertElement(result_vec, elem, builder.getInt64(element_index));
                    }
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
    assert(_struct_type->isStructTy());
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
    const bool is_data = type->get_variation() == Type::Variation::DATA;
    llvm::Value *result_ptr = nullptr;
    if (is_data) {
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
    std::vector<llvm::Type *> elem_types = struct_type->elements();
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
        assert(8 - offset >= elem_size);
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
        assert(elem_offset != 8);
        stacks[1].push(elem_offset);
        offset = elem_offset + elem_size;
    }
    assert(elem_idx == elem_types.size());
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
                llvm::Value *elem_ptr = builder.CreateStructGEP(_struct_type, result_ptr, elem_idx);
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
        llvm::Value *res_ptr = builder.CreateStructGEP(struct_type, result_ptr, elem_idx);
        IR::aligned_store(builder, elem, res_ptr);
        elem_idx++;
    } else if (stacks[0].size() == 2) {
        if (elem_types.front()->isFloatTy() && elem_types.at(1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Value *vec_val = builder.CreateExtractValue(value, 0);

            llvm::Value *elem_1 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_1_ptr = builder.CreateStructGEP(struct_type, result_ptr, elem_idx);
            IR::aligned_store(builder, elem_1, elem_1_ptr);
            elem_idx++;

            llvm::Value *elem_2 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_2_ptr = builder.CreateStructGEP(struct_type, result_ptr, elem_idx);
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
            llvm::Value *elem_ptr = builder.CreateStructGEP(struct_type, result_ptr, actual_elem_idx);
            IR::aligned_store(builder, elem_smol, elem_ptr);
            stacks[0].pop();
        }
        assert(stacks[0].empty());
    }
    if (stacks[1].size() == 1) {
        llvm::Value *elem = builder.CreateExtractValue(value, 1);
        llvm::Value *res_ptr = builder.CreateStructGEP(struct_type, result_ptr, elem_idx);
        IR::aligned_store(builder, elem, res_ptr);
        elem_idx++;
    } else if (stacks[1].size() == 2) {
        if (elem_types.at(stacks_0_size)->isFloatTy() && elem_types.at(stacks_0_size + 1)->isFloatTy()) {
            // We can create a single `<2 x float>` vector as the argument
            llvm::Value *vec_val = builder.CreateExtractValue(value, 1);

            llvm::Value *elem_1 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_1_ptr = builder.CreateStructGEP(struct_type, result_ptr, elem_idx);
            IR::aligned_store(builder, elem_1, elem_1_ptr);
            elem_idx++;

            llvm::Value *elem_2 = builder.CreateExtractElement(vec_val, static_cast<size_t>(elem_idx));
            llvm::Value *elem_2_ptr = builder.CreateStructGEP(struct_type, result_ptr, elem_idx);
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
            llvm::Value *elem_ptr = builder.CreateStructGEP(struct_type, result_ptr, actual_elem_idx);
            IR::aligned_store(builder, elem_smol, elem_ptr);
            stacks[1].pop();
        }
        assert(stacks[1].empty());
    }
    assert(elem_idx == elem_types.size());
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
            call->addParamAttr(i, llvm::Attribute::ByVal);
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

Generator::group_mapping Generator::Expression::generate_call( //
    llvm::IRBuilder<> &builder,                                //
    GenerationContext &ctx,                                    //
    const CallNodeBase *call_node                              //
) {
    const std::string &function_name = call_node->function->name;
    // Get the arguments
    std::vector<llvm::Value *> args;
    args.reserve(call_node->arguments.size());
    garbage_type garbage;

    // Track which temporary optional allocations we've used
    std::unordered_map<std::string, unsigned int> temp_opt_usage;

    for (size_t i = 0; i < call_node->arguments.size(); i++) {
        const auto &arg = call_node->arguments[i];
        const std::shared_ptr<Type> &param_type = std::get<0>(call_node->function->parameters[i]);
        // Complex arguments of function calls are always passed as references, but if the data type is complex this "reference" is a
        // pointer to the actual data of the variable, not a pointer to its allocation. So, in this case we are not allowed to pass in any
        // variable as "reference" because then a double pointer is passed to the function where a single pointer is expected This behaviour
        // should only effect array types, as data and strings are handled differently
        const bool is_not_arr = arg.first->type->get_variation() != Type::Variation::ARRAY;
        const bool is_reference = arg.second && is_not_arr;
        group_mapping expression = generate_expression(builder, ctx, garbage, 0, arg.first.get(), is_reference);
        if (!expression.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (expression.value().empty()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *expr_val = expression.value().front();
        // Check if we need to convert non-optional to optional
        const auto arg_type = arg.first->type;
        if (param_type->get_variation() == Type::Variation::OPTIONAL && arg_type->get_variation() != Type::Variation::OPTIONAL) {
            // We're passing a non-optional value to an optional parameter
            // Use the pre-allocated temporary optional from the allocations map
            const std::string opt_type_str = param_type->to_string();
            unsigned int temp_idx = temp_opt_usage[opt_type_str]++;
            const std::string alloca_name = "temp_opt::" + opt_type_str + "::" + std::to_string(temp_idx);

            llvm::Value *temp_opt = ctx.allocations.at(alloca_name);
            llvm::Type *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), param_type, false);

            // Set has_value field
            llvm::Value *has_value_ptr = builder.CreateStructGEP(opt_struct_type, temp_opt, 0, "has_value_ptr");
            IR::aligned_store(builder, builder.getInt1(true), has_value_ptr);

            // Set the value field
            llvm::Value *value_ptr = builder.CreateStructGEP(opt_struct_type, temp_opt, 1, "value_ptr");
            IR::aligned_store(builder, expr_val, value_ptr);
            expr_val = temp_opt;
        } else if (param_type->get_variation() == Type::Variation::OPTIONAL && arg_type->to_string() == "void?") {
            // We pass a none-literal to a function expecting an optional. The `none` literal generates a zero-initializer so we simply
            // store the whole literal expression on the temp allocation and then pass that allocation to the function
            const std::string opt_type_str = param_type->to_string();
            unsigned int temp_idx = temp_opt_usage[opt_type_str]++;
            const std::string alloca_name = "temp_opt::" + opt_type_str + "::" + std::to_string(temp_idx);

            llvm::Value *temp_opt = ctx.allocations.at(alloca_name);
            IR::aligned_store(builder, expr_val, temp_opt);
            expr_val = temp_opt;
        } else if (param_type->get_variation() == Type::Variation::DATA //
            && arg_type->get_variation() == Type::Variation::DATA       //
        ) {
            // Call `dima.retain` to increment the ARC before passing the argument to the function
            llvm::Function *retain_fn = Module::DIMA::dima_functions.at("retain");
            llvm::CallInst *retain_call = builder.CreateCall(retain_fn, {expr_val});
            retain_call->setMetadata("comment",
                llvm::MDNode::get(context, llvm::MDString::get(context, "Calling 'retain' before passing data to function")));
        }
        args.emplace_back(expr_val);
    }
    // Now that we have generated all arguments check which need to be freed after the call, e.g. which garbage we collected
    for (size_t i = 0; i < call_node->function->parameters.size(); i++) {
        const auto &[param_type, param_name, is_mutable] = call_node->function->parameters.at(i);
        if (!param_type->is_freeable()) {
            continue;
        }
        // We only need to free garbage which is a "producer", so things like variables etc do *not* need to be freed
        const auto &arg = call_node->arguments.at(i).first;
        const ExpressionNode::Variation arg_variation = arg->get_variation();
        const std::string &arg_type_str = arg->type->to_string();
        const bool is_initializer = arg_variation == ExpressionNode::Variation::INITIALIZER;
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
        if (is_initializer || is_array_initializer || is_fn_return || is_string_interpolation || is_slice || is_string_literal) {
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

    llvm::Function *func_decl = nullptr;
    enum class FunctionOrigin { INTERN, EXTERN, BUILTIN };
    FunctionOrigin function_origin = FunctionOrigin::INTERN;
    // First check which core modules have been imported
    auto builtin_function = Parser::get_builtin_function(call_node->function->name, ctx.imported_core_modules);
    if (builtin_function.has_value()) {
        std::vector<llvm::Value *> return_value;
        const std::string &module_name = std::get<0>(builtin_function.value());
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
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (!std::get<2>(std::get<1>(builtin_function.value()).front()).empty()) {
                // Function returns error
                func_decl = Module::Read::read_functions.at(function_name);
                function_origin = FunctionOrigin::BUILTIN;
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
            function_origin = FunctionOrigin::BUILTIN;
        } else if (module_name == "filesystem"                                                                //
            && Module::FileSystem::fs_functions.find(function_name) != Module::FileSystem::fs_functions.end() //
        ) {
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (!std::get<2>(std::get<1>(builtin_function.value()).front()).empty()) {
                // Function returns error
                func_decl = Module::FileSystem::fs_functions.at(function_name);
                function_origin = FunctionOrigin::BUILTIN;
            } else {
                // Function does not return error
                func_decl = Module::FileSystem::fs_functions.at(function_name);
                return_value.emplace_back(builder.CreateCall(func_decl, args));
                return return_value;
            }
        } else if (module_name == "env"                                                           //
            && Module::Env::env_functions.find(function_name) != Module::Env::env_functions.end() //
        ) {
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (!std::get<2>(std::get<1>(builtin_function.value()).front()).empty()) {
                // Function returns error
                func_decl = Module::Env::env_functions.at(function_name);
                function_origin = FunctionOrigin::BUILTIN;
            } else {
                // Function does not return error
                func_decl = Module::Env::env_functions.at(function_name);
                return_value.emplace_back(builder.CreateCall(func_decl, args));
                return return_value;
            }
        } else if (module_name == "system"                                                                    //
            && Module::System::system_functions.find(function_name) != Module::System::system_functions.end() //
        ) {
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (!std::get<2>(std::get<1>(builtin_function.value()).front()).empty()) {
                // Function returns error
                func_decl = Module::System::system_functions.at(function_name);
                function_origin = FunctionOrigin::BUILTIN;
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
                if (std::get<1>(builtin_function.value()).size() > 1) {
                    bool found = false;
                    for (const auto &fn : std::get<1>(builtin_function.value())) {
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
                if (!std::get<2>(std::get<1>(builtin_function.value()).at(idx)).empty()) {
                    // Function returns error
                    func_decl = Module::Math::math_functions.at(fn_name);
                    function_origin = FunctionOrigin::BUILTIN;
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
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (!std::get<2>(std::get<1>(builtin_function.value()).front()).empty()) {
                // Function returns error
                func_decl = Module::Parse::parse_functions.at(function_name);
                function_origin = FunctionOrigin::BUILTIN;
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
                if (std::get<1>(builtin_function.value()).size() > 1) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
            }
            // No function from the time module is able to throw
            assert(std::get<2>(std::get<1>(builtin_function.value()).front()).empty());
            func_decl = Module::Time::time_functions.at(fn_name);
            return_value.emplace_back(builder.CreateCall(func_decl, args));
            for (size_t i = 0; i < call_node->arguments.size(); i++) {
                const auto &arg = call_node->arguments[i];
                if (arg.first->type->get_variation() != Type::Variation::DATA) {
                    continue;
                }
                if (call_node->arguments.at(i).first->type->get_variation() != Type::Variation::DATA) {
                    continue;
                }
                // Call `dima.release` to decrement the ARC after the function call
                llvm::Function *release_fn = Module::DIMA::dima_functions.at("release");
                auto data_head = Module::DIMA::get_head(arg.first->type);
                llvm::CallInst *release_call = builder.CreateCall(release_fn, {data_head, args.at(i)});
                release_call->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context,
                            "Calling 'release' on arg " + std::to_string(i) + " after calling function '" + call_node->function->name +
                                "'")));
            }
            return return_value;
        } else {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
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

    // Create the call instruction using the original declaration
    llvm::CallInst *call = builder.CreateCall(                       //
        func_decl,                                                   //
        args,                                                        //
        function_name + std::to_string(call_node->call_id) + "_call" //
    );
    call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Call of function '" + function_name + "'")));

    // Store results immideately after call
    const std::string call_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
    const std::string call_err_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";

    // Store function result
    llvm::Value *res_var = ctx.allocations.at(call_ret_name);
    IR::aligned_store(builder, call, res_var);

    // Call 'dima.release' on the retained function arguments
    for (size_t i = 0; i < call_node->arguments.size(); i++) {
        const auto &arg = call_node->arguments[i];
        if (arg.first->type->get_variation() != Type::Variation::DATA) {
            continue;
        }
        if (call_node->arguments.at(i).first->type->get_variation() != Type::Variation::DATA) {
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
    llvm::StructType *return_type = static_cast<llvm::StructType *>(IR::add_and_or_get_type(ctx.parent->getParent(), call_node->type));
    llvm::Value *err_ptr = builder.CreateStructGEP(                     //
        return_type,                                                    //
        res_var,                                                        //
        0,                                                              //
        function_name + std::to_string(call_node->call_id) + "_err_ptr" //
    );
    llvm::StructType *error_type = type_map.at("type.flint.err");
    llvm::Value *err_val = IR::aligned_load(builder,                    //
        error_type,                                                     //
        err_ptr,                                                        //
        function_name + std::to_string(call_node->call_id) + "_err_val" //
    );
    llvm::Value *err_var = ctx.allocations.at(call_err_name);
    IR::aligned_store(builder, err_val, err_var);

    // Clear all garbage of temporary parameters
    if (!Statement::clear_garbage(builder, garbage)) {
        return std::nullopt;
    }

    // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
    if (!call_node->has_catch) {
        generate_rethrow(builder, ctx, call_node);
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

    // Extract all the return values from the call (everything except the error return)
    std::vector<llvm::Value *> return_value;
    for (unsigned int i = 1; i < return_type->getNumElements(); i++) {
        llvm::Value *elem_ptr = builder.CreateStructGEP(                                                      //
            return_type,                                                                                      //
            res_var,                                                                                          //
            i,                                                                                                //
            function_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value_ptr" //
        );
        llvm::LoadInst *elem_value = IR::aligned_load(builder,                                            //
            return_type->getElementType(i),                                                               //
            elem_ptr,                                                                                     //
            function_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value" //
        );
        return_value.emplace_back(elem_value);
    }
    return return_value;
}

Generator::group_mapping Generator::Expression::generate_instance_call( //
    llvm::IRBuilder<> &builder,                                         //
    GenerationContext &ctx,                                             //
    const InstanceCallNodeBase *call_node                               //
) {
    switch (call_node->instance_variable->type->get_variation()) {
        default:
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        case Type::Variation::FUNC:
            [[fallthrough]];
        case Type::Variation::ENTITY:
            return generate_call(builder, ctx, static_cast<const CallNodeBase *>(call_node));
    }
}

void Generator::Expression::generate_rethrow( //
    llvm::IRBuilder<> &builder,               //
    GenerationContext &ctx,                   //
    const CallNodeBase *call_node             //
) {
    const std::string err_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    llvm::Value *const err_var = ctx.allocations.at(err_ret_name);
    const std::string function_name = call_node->function->name;

    // Load error value
    llvm::StructType *error_type = type_map.at("type.flint.err");
    llvm::LoadInst *err_val = IR::aligned_load(builder,                   //
        error_type,                                                       //
        err_var,                                                          //
        function_name + "_" + std::to_string(call_node->call_id) + "_val" //
    );
    err_val->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context, "Load err val of call '" + function_name + "::" + std::to_string(call_node->call_id) + "'")));

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

    // Create the if check and compare the err value to 0
    llvm::Value *err_type = builder.CreateExtractValue(err_val, {0}, "err_type");
    llvm::ConstantInt *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    llvm::Value *err_condition = builder.CreateICmpNE( //
        err_type,                                      //
        zero,                                          //
        "errcmp"                                       //
    );

    // Create the branching operation
    builder.CreateCondBr(err_condition, catch_block, merge_block)
        ->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Branch to '" + catch_block->getName().str() + "' if '" + function_name + "' returned error")));

    // Generate the body of the catch block, it only contains re-throwing the error
    builder.SetInsertPoint(catch_block);
    // Copy of the generate_throw function
    {
        // Get the return type of the function
        auto *throw_struct_type = llvm::cast<llvm::StructType>(ctx.parent->getReturnType());

        // Allocate the struct and set all of its values to their respective default values
        llvm::AllocaInst *throw_struct = Allocation::generate_default_struct(builder, throw_struct_type, "throw_ret", true);
        throw_struct->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Create default struct of type '" + throw_struct_type->getName().str() + "' except first value")));

        // Create the pointer to the error value (the 0th index of the struct)
        llvm::Value *error_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, 0, "err_ptr");
        // Store the error value in the struct
        IR::aligned_store(builder, err_val, error_ptr);

        // Generate the throw (return) instruction with the evaluated value
        llvm::LoadInst *throw_struct_val = IR::aligned_load(builder, throw_struct_type, throw_struct, "throw_val");
        throw_struct_val->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context, "Load allocated throw struct of type '" + throw_struct_type->getName().str() + "'")));
        builder.CreateRet(throw_struct_val);
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
        assert(expr_val.size() == 1); // Nested groups are not allowed
        assert(expr_val.at(0) != nullptr);
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
                auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, initializer->args.at(i).get());
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
                llvm::Value *field_ptr = builder.CreateStructGEP(struct_type, data_ptr, i, "field_ptr_" + std::to_string(i));

                const std::shared_ptr<Type> &elem_type = initializer->args.at(i)->type;
                const auto &arg_expr = initializer->args.at(i);
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
                // Check if the element is freeable. If it is then we need to clone it before storing it in the field. We also assign it
                // directly if it's an initalizer in itself, because then it has not been stored anywhere yet
                if (!elem_type->is_freeable() || is_initializer || is_opt_literal || is_slice || is_opaque) {
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
        case Type::Variation::ENTITY: {
            // Create an empty entity first and then simply store the pointers to the data values in it and return the constructed entity
            // structure
            llvm::Type *entity_type = IR::get_type(ctx.parent->getParent(), initializer->type).first;
            llvm::Value *initialized_entity = IR::get_default_value_of_type(entity_type);
            for (size_t i = 0; i < initializer->args.size(); i++) {
                auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, initializer->args.at(i).get());
                if (!expr_result.has_value()) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                // For initializers, we need pure single-value types
                if (expr_result.value().size() > 1) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                // The initializer is of type `i8*` and we need to cast it to `T*` before storing it
                llvm::Value *expr_val = expr_result.value().front();
                llvm::Type *data_type = IR::get_type(ctx.parent->getParent(), initializer->args.at(i)->type).first;
                llvm::Value *cast_expr = builder.CreatePointerCast(expr_val, data_type->getPointerTo());
                initialized_entity = builder.CreateInsertValue(initialized_entity, cast_expr, i, "entity_init_" + std::to_string(i));
            }
            return std::vector<llvm::Value *>{initialized_entity};
        }
        case Type::Variation::MULTI: {
            // Create an "empty" vector of the multi-type
            llvm::Type *vector_type = IR::get_type(ctx.parent->getParent(), initializer->type).first;
            llvm::Value *initialized_value = IR::get_default_value_of_type(vector_type);
            for (unsigned int i = 0; i < initializer->args.size(); i++) {
                auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, initializer->args.at(i).get());
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
            const std::string var_str = "s" + std::to_string(branch.scope->scope_id) + "::" + match_node->name;
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
        phi_values.emplace_back(branch_value, branch_blocks[i]);

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

        // Add a reference to the 'value' of the variant to the block the switch expression takes place in
        const std::string var_str = "s" + std::to_string(branch.scope->scope_id) + "::" + match_node->name;
        llvm::Value *real_value_reference = builder.CreateStructGEP(variant_struct_type, var_alloca, 1, "value_reference");
        ctx.allocations.emplace(var_str, real_value_reference);
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
                    assert(pair.has_value());
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
    llvm::Type *element_type = IR::get_type(ctx.parent->getParent(), initializer->element_type).first;
    size_t element_size_in_bytes = data_layout.getTypeAllocSize(element_type);
    llvm::CallInst *created_array = builder.CreateCall(        //
        Module::Array::array_manip_functions.at("create_arr"), //
        {
            builder.getInt64(length_expressions.size()), // dimensionality
            builder.getInt64(element_size_in_bytes),     // element_size
            length_array                                 // lengths array
        },                                               //
        "created_array"                                  //
    );
    created_array->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Create an array of type " + initializer->element_type->to_string() + "[" +
                    std::string(length_expressions.size() - 1, ',') + "]")));
    llvm::Value *initializer_expression = nullptr;
    if (initializer->initializer_value->get_variation() == ExpressionNode::Variation::DEFAULT) {
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
    if (initializer->element_type->to_string() == "str") {
        llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
        llvm::Value *str_len_ptr = builder.CreateStructGEP(str_type, initializer_expression, 0, "str_len_ptr");
        llvm::Value *str_len = IR::aligned_load(builder, builder.getInt64Ty(), str_len_ptr, "str_len");
        uint64_t str_size = data_layout.getTypeAllocSize(str_type);
        str_len = builder.CreateAdd(str_len, builder.getInt64(str_size));
        llvm::CallInst *fill_call = builder.CreateCall(               //
            Module::Array::array_manip_functions.at("fill_arr_deep"), //
            {created_array, str_len, initializer_expression}          //
        );
        fill_call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Fill the array")));
    } else if (initializer->element_type->get_variation() == Type::Variation::PRIMITIVE) {
        llvm::Type *from_type = IR::get_type(ctx.parent->getParent(), initializer->element_type).first;
        llvm::Value *value_container = IR::generate_bitwidth_change(                     //
            builder,                                                                     //
            initializer_expression,                                                      //
            from_type->getPrimitiveSizeInBits(),                                         //
            64,                                                                          //
            IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("i64")).first //
        );
        llvm::CallInst *fill_call = builder.CreateCall(                               //
            Module::Array::array_manip_functions.at("fill_arr_val"),                  //
            {created_array, builder.getInt64(element_size_in_bytes), value_container} //
        );
        fill_call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Fill the array")));
    } else if (initializer->element_type->get_variation() == Type::Variation::MULTI) {
        // TODO:
    }
    return created_array;
}

llvm::Value *Generator::Expression::generate_array_access( //
    llvm::IRBuilder<> &builder,                            //
    GenerationContext &ctx,                                //
    garbage_type &garbage,                                 //
    const unsigned int expr_depth,                         //
    const ArrayAccessNode *access                          //
) {
    std::optional<llvm::Value *> base_expr_value = std::nullopt;
    return generate_array_access(                                                                                         //
        builder, ctx, garbage, expr_depth, base_expr_value, access->type, access->base_expr, access->indexing_expressions //
    );
}

llvm::Value *Generator::Expression::generate_array_access(                   //
    llvm::IRBuilder<> &builder,                                              //
    GenerationContext &ctx,                                                  //
    garbage_type &garbage,                                                   //
    const unsigned int expr_depth,                                           //
    std::optional<llvm::Value *> base_expr_value,                            //
    const std::shared_ptr<Type> result_type,                                 //
    const std::unique_ptr<ExpressionNode> &base_expr,                        //
    const std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
) {
    const bool is_slice = result_type->get_variation() == Type::Variation::ARRAY //
        || (result_type->to_string() == "str" && base_expr->type->to_string() == "str");
    // First, generate the index expressions
    std::vector<std::array<llvm::Value *, 2>> index_expressions;
    for (auto &index_expression : indexing_expressions) {
        group_mapping index = generate_expression(builder, ctx, garbage, expr_depth, index_expression.get());
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
        group_mapping base_expression = generate_expression(builder, ctx, garbage, expr_depth, base_expr.get());
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
            llvm::Value *index_ptr = builder.CreateGEP(                                                            //
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
            llvm::Value *index_ptr = builder.CreateGEP(                                                                            //
                builder.getInt64Ty(), temp_array_indices, builder.getInt64(i * 2 + j), "idx_" + std::to_string(i * 2 + j) + "_ptr" //
            );
            llvm::StoreInst *index_store = IR::aligned_store(builder, index_expressions.at(i)[j], index_ptr);
            index_store->setMetadata("comment",                                                                               //
                llvm::MDNode::get(context, llvm::MDString::get(context, "Save the index of id " + std::to_string(i * 2 + j))) //
            );
            if (is_slice && !is_range) {
                // The slicing function expects indices of non-ranges to be '1, 1' for the index 1, and '1, 3' for the range [1, 3)
                index_ptr = builder.CreateGEP(                                                                                         //
                    builder.getInt64Ty(), temp_array_indices, builder.getInt64(i * 2 + 1), "idx_" + std::to_string(i * 2 + 1) + "_ptr" //
                );
                index_store = IR::aligned_store(builder, index_expressions.at(i)[0], index_ptr);
                index_store->setMetadata("comment",                                                                               //
                    llvm::MDNode::get(context, llvm::MDString::get(context, "Save the index of id " + std::to_string(i * 2 + 1))) //
                );
            }
        }
    }
    llvm::Type *element_type = IR::get_type(ctx.parent->getParent(), result_type).first;
    if (is_slice) {
        element_type = IR::get_type(ctx.parent->getParent(), result_type->as<ArrayType>()->type).first;
    }
    size_t element_size_in_bytes = Allocation::get_type_size(ctx.parent->getParent(), element_type);
    if (result_type->to_string() == "str") {
        // We get a 'str**' from the 'access_arr' function, so we need to dereference it first before returning it
        llvm::Value *result = builder.CreateCall(Module::Array::array_manip_functions.at("access_arr"), //
            {array_ptr, builder.getInt64(element_size_in_bytes), temp_array_indices}                    //
        );
        return IR::aligned_load(builder, element_type, result, "str_value");
    }
    switch (result_type->get_variation()) {
        default:
            // Non-supported type for array access
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        case Type::Variation::ENUM:
        case Type::Variation::PRIMITIVE: {
            llvm::Value *result = builder.CreateCall(Module::Array::array_manip_functions.at("access_arr_val"), //
                {array_ptr, builder.getInt64(element_size_in_bytes), temp_array_indices}                        //
            );
            return IR::generate_bitwidth_change(builder, result, 64, element_type->getPrimitiveSizeInBits(), element_type);
        }
        case Type::Variation::TUPLE:
        case Type::Variation::MULTI: {
            llvm::Value *elem_ptr = builder.CreateCall(Module::Array::array_manip_functions.at("access_arr"), //
                {array_ptr, builder.getInt64(element_size_in_bytes), temp_array_indices}, "access_arr_ptr"    //
            );
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
    const DataAccessNode *data_access                                 //
) {
    // First, generate the base expression to get the value of the data variable
    group_mapping base_expr = generate_expression(builder, ctx, garbage, expr_depth, data_access->base_expr.get());
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
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("type.flint.str")).first;
            llvm::Value *length_ptr = builder.CreateStructGEP(str_type, expr_val, 1, "length_ptr");
            std::vector<llvm::Value *> length_values;
            for (size_t i = 0; i < array_type->dimensionality; i++) {
                llvm::Value *actual_length_ptr = builder.CreateGEP(builder.getInt64Ty(), length_ptr, builder.getInt64(i));
                llvm::Value *length_value =
                    IR::aligned_load(builder, builder.getInt64Ty(), actual_length_ptr, "length_value_" + std::to_string(i));
                length_values.emplace_back(length_value);
            }
            return length_values;
        }
        case Type::Variation::MULTI: {
            const auto *multi_type = data_access->base_expr->type->as<MultiType>();
            std::vector<llvm::Value *> values;
            if (multi_type->base_type->to_string() == "bool") {
                // Special case for accessing an "element" on a bool8 type
                values.emplace_back(get_bool8_element_at(builder, expr_val, data_access->field_id));
            } else {
                values.emplace_back(builder.CreateExtractElement(expr_val, data_access->field_id));
            }
            return values;
        }
        case Type::Variation::ENTITY: {
            std::vector<llvm::Value *> values;
            values.emplace_back(builder.CreateExtractValue(expr_val, data_access->field_id, "entity_data_ptr"));
            return values;
        }
        case Type::Variation::ERROR_SET: {
            std::vector<llvm::Value *> values;
            values.emplace_back(builder.CreateExtractValue(expr_val, data_access->field_id, "err_field_val"));
            return values;
        }
        case Type::Variation::FUNC: {
            std::vector<llvm::Value *> values;
            values.emplace_back(builder.CreateExtractValue(expr_val, data_access->field_id, "func_data_ptr"));
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
    auto elem_type = IR::get_type(ctx.parent->getParent(), data_access->type);
    values.emplace_back(IR::aligned_load(                                                    //
        builder, elem_type.second.first ? elem_type.first->getPointerTo() : elem_type.first, //
        elem_ptr, "elem_" + std::to_string(data_access->field_id)                            //
        ));
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
        assert(expr->getType()->isStructTy());
    }
    for (size_t i = 0; i < grouped_data_access->field_names.size(); i++) {
        const unsigned int id = grouped_data_access->field_ids.at(i);
        if (is_vector_type) {
            return_values.push_back(builder.CreateExtractElement(expr, id, "group_" + grouped_data_access->field_names.at(i) + "_val"));
        } else {
            return_values.push_back(builder.CreateExtractValue(expr, id, "group_" + grouped_data_access->field_names.at(i) + "_val"));
        }
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
        llvm::Value *opt_value = generate_array_access(                                                                              //
            builder, ctx, garbage, expr_depth, base_expr_value, base_array_type->type, chain->base_expr, access.indexing_expressions //
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
    const OptionalUnwrapNode *unwrap                                      //
) {
    assert(unwrap->base_expr->type->get_variation() == Type::Variation::OPTIONAL);
    auto base_expressions = generate_expression(builder, ctx, garbage, expr_depth + 1, unwrap->base_expr.get());
    if (!base_expressions.has_value()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    // For now, we assume that the base expression is not a group type
    assert(base_expressions.value().size() == 1);
    llvm::Value *base_expr = base_expressions.value().front();
    if (base_expr->getType()->isPointerTy()) {
        llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), unwrap->base_expr->type, false);
        base_expr = IR::aligned_load(builder, opt_struct_type, base_expr, "loaded_base_expr");
    }
    if (opt_unwrap_mode == OptionalUnwrapMode::UNSAFE) {
        // Directly unwrap the value when in unsafe mode, possibly breaking stuff, but it's much faster too
        base_expr = builder.CreateExtractValue(base_expr, {1}, "opt_value_unsafe");
        return std::vector<llvm::Value *>{base_expr};
    }
    // First, check if the optional even has a value at all
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    llvm::BasicBlock *has_no_value = llvm::BasicBlock::Create(context, "opt_upwrap_no_value", ctx.parent);
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "opt_upwrap_value", ctx.parent);
    builder.SetInsertPoint(inserter);
    llvm::Value *opt_has_value = builder.CreateExtractValue(base_expr, {0}, "opt_has_value");
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
    base_expr = builder.CreateExtractValue(base_expr, {1}, "opt_value");
    return std::vector<llvm::Value *>{base_expr};
}

Generator::group_mapping Generator::Expression::generate_variant_extraction( //
    llvm::IRBuilder<> &builder,                                              //
    GenerationContext &ctx,                                                  //
    const VariantExtractionNode *extraction                                  //
) {
    const auto *var_type = extraction->base_expr->type->as<VariantType>();
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
    const std::optional<unsigned char> id = var_type->get_idx_of_type(extract_type_ptr);
    assert(id.has_value());
    llvm::Value *wanted_type = builder.getInt8(id.value());
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
    llvm::Value *value_ptr = builder.CreateBitCast(value_raw_ptr, element_type->getPointerTo(), "value_ptr");
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
    const auto *var_type = unwrap->base_expr->type->as<VariantType>();
    const auto *variable_node = unwrap->base_expr->as<VariableNode>();
    const unsigned int variable_decl_scope = ctx.scope->variables.at(variable_node->name).scope_id;
    llvm::Value *const variable = ctx.allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);
    llvm::Type *const element_type = IR::get_type(ctx.parent->getParent(), unwrap->type).first;
    llvm::Type *const variant_type = IR::get_type(ctx.parent->getParent(), unwrap->base_expr->type).first;
    if (var_unwrap_mode == VariantUnwrapMode::UNSAFE) {
        // Directly unwrap the value when in unsafe mode, possibly breaking stuff, but it's much faster too
        llvm::Value *value_ptr = builder.CreateStructGEP(variant_type, variable, 1, "var_value_ptr");
        llvm::Value *value_cast_ptr = builder.CreateBitCast(value_ptr, element_type->getPointerTo(), "value_ptr_unsafe");
        llvm::Value *value = IR::aligned_load(builder, variant_type, value_cast_ptr, "var_value_unsafe");
        return std::vector<llvm::Value *>{value};
    }

    // First, check if the variant holds a value of our wanted type
    llvm::BasicBlock *inserter = builder.GetInsertBlock();
    llvm::BasicBlock *holds_wrong_type = llvm::BasicBlock::Create(context, "var_upwrap_wrong_type", ctx.parent);
    llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "var_unwrap", ctx.parent);
    builder.SetInsertPoint(inserter);
    const std::optional<unsigned char> id = var_type->get_idx_of_type(unwrap->type);
    assert(id.has_value());
    llvm::Value *wanted_type = builder.getInt8(id.value());
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
    llvm::Value *value_ptr = builder.CreateBitCast(value_raw_ptr, element_type->getPointerTo(), "value_ptr");
    llvm::Value *value = IR::aligned_load(builder, element_type, value_ptr, "value");
    return std::vector<llvm::Value *>{value};
}

Generator::group_mapping Generator::Expression::generate_type_cast( //
    llvm::IRBuilder<> &builder,                                     //
    GenerationContext &ctx,                                         //
    garbage_type &garbage,                                          //
    const unsigned int expr_depth,                                  //
    const TypeCastNode *type_cast_node,                             //
    const bool is_reference                                         //
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
    auto expr_res = generate_expression(builder, ctx, garbage, expr_depth + 1, type_cast_node->expr.get(), is_reference);
    if (!expr_res.has_value()) {
        return std::nullopt;
    }
    std::vector<llvm::Value *> expr = expr_res.value();
    std::shared_ptr<Type> to_type;
    switch (type_cast_node->type->get_variation()) {
        default:
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
                    case Type::Variation::MULTI: {
                        const auto *multi_type = type_cast_node->expr->type->as<MultiType>();
                        assert(expr.size() == 1);
                        llvm::Value *mult_expr = expr.front();
                        expr.clear();
                        for (size_t i = 0; i < multi_type->width; i++) {
                            expr.emplace_back(builder.CreateExtractElement(mult_expr, i, "mult_group_" + std::to_string(i)));
                        }
                        return expr;
                    }
                    case Type::Variation::TUPLE: {
                        const auto *tuple_type = type_cast_node->expr->type->as<TupleType>();
                        assert(expr.size() == 1);
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
        case Type::Variation::MULTI: {
            const auto *multi_type = type_cast_node->type->as<MultiType>();
            if (type_cast_node->type->to_string() == "bool8") {
                assert(type_cast_node->expr->type->to_string() == "u8");
                assert(expr.size() == 1);
                std::vector<llvm::Value *> result;
                result.emplace_back(expr.at(0));
                return result;
            }
            // The expression now must be a group type, so the `expr` size must be the multi-type width
            if (expr.size() != multi_type->width) {
                // If the sizes dont match, the rhs must have size 1 and its type must match the element type of the multi-type
                if (expr.size() == 1 && type_cast_node->expr->type == multi_type->base_type) {
                    // We now can create a single value extension for the vector
                    expr[0] = builder.CreateVectorSplat(multi_type->width, expr[0], "vec_ext");
                    return expr;
                }
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            llvm::Type *element_type = IR::get_type(ctx.parent->getParent(), multi_type->base_type).first;
            llvm::VectorType *vector_type = llvm::VectorType::get(element_type, multi_type->width, false);

            // Create and undefined vector to insert elements into
            llvm::Value *vec = llvm::UndefValue::get(vector_type);

            // "Store" all the values inside the vector which will be stored in the alloca in the `generate_declaration` function
            for (unsigned int i = 0; i < expr.size(); i++) {
                vec = builder.CreateInsertElement(vec, expr[i], builder.getInt32(i), "vec_insert");
            }
            std::vector<llvm::Value *> result;
            result.emplace_back(vec);
            return result;
        }
        case Type::Variation::TUPLE: {
            if (type_cast_node->expr->type->get_variation() == Type::Variation::GROUP) {
                const auto *expr_group_type = type_cast_node->expr->type->as<GroupType>();
                // Type-checking should have been happened in the parser, so we can assert that the types match
                [[maybe_unused]] const auto *tuple_type = type_cast_node->type->as<TupleType>();
                assert(tuple_type->types.size() == expr_group_type->types.size());
                llvm::Type *tup_type = IR::get_type(ctx.parent->getParent(), type_cast_node->type).first;
                llvm::Value *result = IR::get_default_value_of_type(tup_type);
                for (unsigned int i = 0; i < expr_group_type->types.size(); i++) {
                    assert(expr_group_type->types[i] == tuple_type->types[i]);
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
        assert(expr.size() == 1);
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
    } else if (from_type->get_variation() == Type::Variation::MULTI) {
        if (from_type_str == "bool8") {
            if (to_type_str == "str") {
                return builder.CreateCall(Module::TypeCast::typecast_functions.at("bool8_to_str"), {expr}, "b8_to_str_val");
            } else if (to_type_str == "u8") {
                return expr;
            }
        } else if (to_type_str == "str") {
            llvm::Function *cast_fn = Module::TypeCast::typecast_functions.at(from_type_str + "_to_str");
            return builder.CreateCall(cast_fn, {expr}, from_type_str + "_to_str_res");
        }
    } else if (from_type->get_variation() == Type::Variation::ENTITY && to_type->get_variation() == Type::Variation::FUNC) {
        // We "cast" an entity to a func module by extracting the required data of the func module from the entity and storing it in the
        // func module. Whenever we extract and store a data value from the entity to the func module we call `dima.retain` on that value
        // first for proper ARC-tracking
        llvm::Value *func_value = IR::get_default_value_of_type(builder, ctx.parent->getParent(), to_type);
        const EntityNode *entity_node = from_type->as<EntityType>()->entity_node;
        const FuncNode *func_node = to_type->as<FuncType>()->func_node;
        llvm::Function *retain_fn = Module::DIMA::dima_functions.at("retain");
        for (size_t i = 0; i < func_node->required_data.size(); i++) {
            const DataNode *data_node = func_node->required_data.at(i).first->as<DataType>()->data_node;
            size_t idx = 0;
            for (; idx < entity_node->data_modules.size(); idx++) {
                const DataNode *data_ptr = entity_node->data_modules.at(idx);
                if (data_ptr == data_node) {
                    break;
                }
            }
            llvm::Value *ext_data = builder.CreateExtractValue(expr, idx, "extracted_data_" + std::to_string(idx));
            builder.CreateCall(retain_fn, {ext_data});
            func_value = builder.CreateInsertValue(func_value, ext_data, i, "func_value__insert_" + std::to_string(i));
        }
        return func_value;
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
            // "casting" the actual value of the optional and storing it in the optional struct value itself, but the storing part is done
            // in the assignment / declaration generation
            return expr;
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
        assert(to_type_str == "str");
        llvm::GlobalVariable *name_array = enum_name_arrays_map.at(from_enum_type->enum_node->name);
        llvm::Value *name_str_ptr = builder.CreateGEP(name_array->getType(), name_array, {expr}, "name_str_ptr");
        llvm::Type *i8_ptr_type = builder.getInt8Ty()->getPointerTo();
        llvm::Value *name_str = IR::aligned_load(builder, i8_ptr_type, name_str_ptr, "name_str");
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
                [[maybe_unused]] const std::shared_ptr<Type> &base_type = pointer_type->base_type;
                assert(base_type == expression->type);
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
    const bool rhs_is_reference = bin_op_node->right->type->get_variation() == Type::Variation::VARIANT;
    auto rhs_maybe = generate_expression(builder, ctx, garbage, expr_depth + 1, bin_op_node->right.get(), rhs_is_reference);
    if (!rhs_maybe.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    std::vector<llvm::Value *> rhs = rhs_maybe.value();
    if (lhs.size() != rhs.size()) {
        assert(lhs.size() == 1 || rhs.size() == 1);
        auto result = generate_binary_op_set_cmp(builder, ctx, garbage, expr_depth, bin_op_node, lhs, rhs);
        if (!result.has_value()) {
            return std::nullopt;
        }
        return std::vector<llvm::Value *>{result.value()};
    }
    std::vector<llvm::Value *> return_value;

    const bool is_lhs_mult = bin_op_node->left->type->get_variation() == Type::Variation::MULTI;
    const bool is_rhs_mult = bin_op_node->right->type->get_variation() == Type::Variation::MULTI;
    if (is_lhs_mult && is_rhs_mult) {
        const auto *lhs_mult = bin_op_node->left->type->as<MultiType>();
        const auto *rhs_mult = bin_op_node->right->type->as<MultiType>();
        if (!lhs_mult->base_type->equals(rhs_mult->base_type)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (lhs_mult->width != rhs_mult->width) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        // For multi-types we have exactly one value in each vector
        assert(lhs.size() == 1 && rhs.size() == 1);
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
        assert(group_type == nullptr || group_type->types.size() == lhs.size());
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
            llvm::Value *has_value = builder.CreateExtractValue(lhs, {0}, "has_value");
            llvm::Value *lhs_value = builder.CreateExtractValue(lhs, {1}, "value");
            return builder.CreateSelect(has_value, lhs_value, rhs, "selected_value");
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
    assert(rhs_expr->type->get_variation() == Type::Variation::VARIANT);
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
            } else {
                cmp_result = builder.CreateICmpSLT(lhs, rhs, "vec_lt");
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
            } else {
                cmp_result = builder.CreateICmpSGT(lhs, rhs, "vec_gt");
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
            } else {
                cmp_result = builder.CreateICmpSLE(lhs, rhs, "vec_le");
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
            } else {
                cmp_result = builder.CreateICmpSGE(lhs, rhs, "vec_ge");
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
