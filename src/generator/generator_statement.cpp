#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#include "lexer/builtins.hpp"

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/type.hpp"
#include "parser/type/variant_type.hpp"
#include "llvm/IR/BasicBlock.h"

#include <algorithm>
#include <array>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <memory>
#include <string>

bool Generator::Statement::generate_statement(      //
    llvm::IRBuilder<> &builder,                     //
    GenerationContext &ctx,                         //
    const std::unique_ptr<StatementNode> &statement //
) {
    switch (statement->get_variation()) {
        case StatementNode::Variation::ARRAY_ASSIGNMENT: {
            const auto *node = statement->as<ArrayAssignmentNode>();
            return generate_array_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::ASSIGNMENT: {
            const auto *node = statement->as<AssignmentNode>();
            return generate_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::BREAK:
            builder.CreateBr(last_loop_merge_blocks.back());
            return true;
        case StatementNode::Variation::CALL: {
            const auto *node = statement->as<CallNodeStatement>();
            group_mapping gm = Expression::generate_call(builder, ctx, static_cast<const CallNodeBase *>(node));
            return gm.has_value();
        }
        case StatementNode::Variation::CATCH: {
            const auto *node = statement->as<CatchNode>();
            return generate_catch_statement(builder, ctx, node);
        }
        case StatementNode::Variation::CONTINUE:
            builder.CreateBr(last_looparound_blocks.back());
            return true;
        case StatementNode::Variation::DATA_FIELD_ASSIGNMENT: {
            const auto *node = statement->as<DataFieldAssignmentNode>();
            return generate_data_field_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::DECLARATION: {
            const auto *node = statement->as<DeclarationNode>();
            return generate_declaration(builder, ctx, node);
        }
        case StatementNode::Variation::DO_WHILE: {
            [[maybe_unused]] const auto *node = statement->as<DoWhileNode>();
            return generate_do_while_loop(builder, ctx, node);
        }
        case StatementNode::Variation::ENHANCED_FOR_LOOP: {
            const auto *node = statement->as<EnhForLoopNode>();
            return generate_enh_for_loop(builder, ctx, node);
        }
        case StatementNode::Variation::FOR_LOOP: {
            const auto *node = statement->as<ForLoopNode>();
            return generate_for_loop(builder, ctx, node);
        }
        case StatementNode::Variation::GROUP_ASSIGNMENT: {
            const auto *node = statement->as<GroupAssignmentNode>();
            return generate_group_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::GROUP_DECLARATION: {
            const auto *node = statement->as<GroupDeclarationNode>();
            return generate_group_declaration(builder, ctx, node);
        }
        case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT: {
            const auto *node = statement->as<GroupedDataFieldAssignmentNode>();
            return generate_grouped_data_field_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::IF: {
            const auto *node = statement->as<IfNode>();
            std::vector<llvm::BasicBlock *> blocks;
            return generate_if_statement(builder, ctx, blocks, 0, node);
        }
        case StatementNode::Variation::RETURN: {
            const auto *node = statement->as<ReturnNode>();
            return generate_return_statement(builder, ctx, node);
        }
        case StatementNode::Variation::STACKED_ASSIGNMENT: {
            const auto *node = statement->as<StackedAssignmentNode>();
            return generate_stacked_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::STACKED_ARRAY_ASSIGNMENT: {
            const auto *node = statement->as<StackedArrayAssignmentNode>();
            return generate_stacked_array_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::STACKED_GROUPED_ASSIGNMENT: {
            const auto *node = statement->as<StackedGroupedAssignmentNode>();
            return generate_stacked_grouped_assignment(builder, ctx, node);
        }
        case StatementNode::Variation::SWITCH: {
            const auto *node = statement->as<SwitchStatement>();
            return generate_switch_statement(builder, ctx, node);
        }
        case StatementNode::Variation::THROW: {
            const auto *node = statement->as<ThrowNode>();
            return generate_throw_statement(builder, ctx, node);
        }
        case StatementNode::Variation::UNARY_OP: {
            const auto *node = statement->as<UnaryOpStatement>();
            return generate_unary_op_statement(builder, ctx, node);
        }
        case StatementNode::Variation::WHILE: {
            const auto *node = statement->as<WhileNode>();
            return generate_while_loop(builder, ctx, node);
        }
    }
    __builtin_unreachable();
}

bool Generator::Statement::clear_garbage(                                                                        //
    llvm::IRBuilder<> &builder,                                                                                  //
    std::unordered_map<unsigned int, std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>>> &garbage //
) {
    if (garbage.empty()) {
        return true;
    }
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Garbage information:\n" << DEFAULT;
    }
    for (auto &[key, value] : garbage) {
        if (DEBUG_MODE) {
            std::cout << "-- Level " << key << " garbage:\n";
        }
        for (auto [type, llvm_val] : value) {
            if (DEBUG_MODE) {
                std::cout << "  -- Type '" << type->to_string() << "' val addr: " << llvm_val << "\n";
            }
            const auto type_variation = type->get_variation();
            if (type_variation == Type::Variation::PRIMITIVE) {
                const auto *primitive_type = type->as<PrimitiveType>();
                if (primitive_type->type_name == "str") {
                    llvm::Function *free_fn = c_functions.at(FREE);
                    llvm::CallInst *free_call = builder.CreateCall(free_fn, {llvm_val});
                    free_call->setMetadata("comment",
                        llvm::MDNode::get(context,
                            llvm::MDString ::get(context, "Clear garbage of type 'str', depth " + std::to_string(key))));
                } else {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return false;
                }
            } else if (type_variation == Type::Variation::ARRAY) {
                const auto *array_type = type->as<ArrayType>();
                // For now, we dont allow jagged arrays. If we would add jagged arrays we would need a recursive tip-to-root freeing system
                // here, but for now we keep it simple
                llvm::CallInst *free_call = builder.CreateCall(c_functions.at(FREE), {llvm_val});
                free_call->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString ::get(context,
                            "Clear garbage of type '" + array_type->to_string() + "', depth " + std::to_string(key))));
            }
        }
    }
    if (DEBUG_MODE) {
        std::cout << std::endl;
    }
    garbage.clear();
    return true;
}

bool Generator::Statement::generate_body(llvm::IRBuilder<> &builder, GenerationContext &ctx) {
    bool success = true;
    for (const auto &statement : ctx.scope->body) {
        success &= generate_statement(builder, ctx, statement);
    }
    if (!success) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Only generate end of scope if the last statement was not a return or throw statement
    const auto last_variation = ctx.scope->body.back()->get_variation();
    if (ctx.scope->parent_scope != nullptr && last_variation != StatementNode::Variation::RETURN &&
        last_variation != StatementNode::Variation::THROW) {
        success &= generate_end_of_scope(builder, ctx);
    }
    return success;
}

bool Generator::Statement::generate_end_of_scope(llvm::IRBuilder<> &builder, GenerationContext &ctx) {
    // First, get all variables of this scope that went out of scope
    llvm::BasicBlock *prev_block = builder.GetInsertBlock();
    llvm::BasicBlock *end_of_scope_block = llvm::BasicBlock::Create(               //
        context, "end_of_scope_" + std::to_string(ctx.scope->scope_id), ctx.parent //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "end_of_scope_" + std::to_string(ctx.scope->scope_id) + "_merge");
    builder.SetInsertPoint(prev_block);
    builder.CreateBr(end_of_scope_block);
    builder.SetInsertPoint(end_of_scope_block);
    auto variables = ctx.scope->get_unique_variables();
    for (const auto &[var_name, var_info] : variables) {
        // Check if the variable is a function parameter, if it is, dont free it
        // Also check if the variable is a reference to another variable (like in variant switch branches), if it is dont free it
        if (std::get<3>(var_info) || std::get<4>(var_info)) {
            continue;
        }
        // Check if the variable is returned within this scope, if it is we do not free it
        const std::vector<unsigned int> &returned_scopes = std::get<5>(var_info);
        if (std::find(returned_scopes.begin(), returned_scopes.end(), ctx.scope->scope_id) != returned_scopes.end()) {
            continue;
        }
        // Check if the variable is an alias type
        std::shared_ptr<Type> var_type = std::get<0>(var_info);
        if (var_type->get_variation() == Type::Variation::ALIAS) {
            const auto *alias_type = var_type->as<AliasType>();
            var_type = alias_type->type;
        }
        switch (var_type->get_variation()) {
            case Type::Variation::ALIAS: {
                assert(false);
            }
            case Type::Variation::ARRAY: {
                const auto *array_type = var_type->as<ArrayType>();
                const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
                llvm::Value *const alloca = ctx.allocations.at(alloca_name);
                llvm::Type *arr_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
                llvm::Value *arr_ptr = IR::aligned_load(builder, arr_type->getPointerTo(), alloca, var_name + "_cleanup");
                if (!generate_array_cleanup(builder, arr_ptr, array_type)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case Type::Variation::DATA: {
                const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
                llvm::Value *const alloca = ctx.allocations.at(alloca_name);
                llvm::Type *base_type = IR::get_type(ctx.parent->getParent(), var_type).first;
                if (!generate_data_cleanup(builder, ctx, base_type, alloca, var_type)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case Type::Variation::ENUM:
                break;
            case Type::Variation::ERROR_SET: {
                const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
                llvm::Value *const alloca = ctx.allocations.at(alloca_name);
                llvm::StructType *error_type = type_map.at("__flint_type_err");
                llvm::Value *err_message_ptr = builder.CreateStructGEP(error_type, alloca, 2, "err_message_ptr");
                llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("str")).first;
                llvm::Value *err_message = IR::aligned_load(builder, str_type, err_message_ptr, "err_message");
                llvm::CallInst *free_call = builder.CreateCall(c_functions.at(FREE), {err_message});
                free_call->setMetadata("comment",
                    llvm::MDNode::get(context, llvm::MDString ::get(context, "Clear error message from error '" + var_name + "'")));
                break;
            }
            case Type::Variation::GROUP:
                break;
            case Type::Variation::MULTI:
                break;
            case Type::Variation::OPTIONAL:
                break;
            case Type::Variation::POINTER:
                break;
            case Type::Variation::PRIMITIVE: {
                const auto *primitive_type = var_type->as<PrimitiveType>();
                if (primitive_type->type_name != "str") {
                    continue;
                }
                // Get the allocation of the variable
                const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
                llvm::Value *const alloca = ctx.allocations.at(alloca_name);
                llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
                llvm::Value *str_ptr = IR::aligned_load(builder, str_type->getPointerTo(), alloca, var_name + "_cleanup");
                builder.CreateCall(c_functions.at(FREE), {str_ptr});
                break;
            }
            case Type::Variation::RANGE:
                break;
            case Type::Variation::TUPLE:
                break;
            case Type::Variation::UNKNOWN:
                break;
            case Type::Variation::VARIANT: {
                const auto *variant_type = var_type->as<VariantType>();
                if (variant_type->is_err_variant) {
                    const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
                    llvm::Value *const alloca = ctx.allocations.at(alloca_name);
                    llvm::StructType *error_type = type_map.at("__flint_type_err");
                    llvm::Value *err_message_ptr = builder.CreateStructGEP(error_type, alloca, 2, "err_message_ptr");
                    llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("str")).first;
                    llvm::Value *err_message = IR::aligned_load(builder, str_type, err_message_ptr, "err_message");
                    llvm::CallInst *free_call = builder.CreateCall(c_functions.at(FREE), {err_message});
                    free_call->setMetadata("comment",
                        llvm::MDNode::get(context, llvm::MDString ::get(context, "Clear error message from variant '" + var_name + "'")));
                } else {
                    std::map<size_t, llvm::BasicBlock *> possible_value_blocks;
                    const auto &possible_types = variant_type->get_possible_types();
                    for (size_t i = 0; i < possible_types.size(); i++) {
                        // Check if the type is complex, if it is then we need to free it
                        const auto &possible_type = possible_types[i];
                        auto type_info = IR::get_type(ctx.parent->getParent(), possible_type.second);
                        const bool is_complex =                                                    //
                            (type_info.second.first || possible_type.second->to_string() == "str") //
                            // TODO: When DIMA works then data, entity etc types will be complex too and need to be freed as well here
                            && possible_type.second->get_variation() != Type::Variation::DATA;
                        if (is_complex) {
                            // Add a basic block in which this complex type will be freed
                            llvm::BasicBlock *free_block = llvm::BasicBlock::Create(                                                    //
                                context, "variant_" + var_name + "_free_" + std::to_string(i) + "_" + possible_type.second->to_string() //
                            );
                            possible_value_blocks[i] = free_block;
                        }
                    }
                    if (possible_value_blocks.empty()) {
                        // If there are no complex values inside the variant then we do not need to free anything
                        break;
                    }
                    // Create the merge block of the variant free and create the switch statement at the end of the current block to branch
                    // to each type.
                    llvm::BasicBlock *variant_free_merge_block = llvm::BasicBlock::Create(context, "variant_" + var_name + "_free_merge");
                    const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
                    llvm::Value *const alloca = ctx.allocations.at(alloca_name);
                    llvm::StructType *variant_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), var_type);
                    llvm::Value *variant_active_value_ptr = builder.CreateStructGEP( //
                        variant_struct_type, alloca, 0, "variant_active_value_ptr"   //
                    );
                    llvm::Value *variant_active_value = IR::aligned_load(                              //
                        builder, builder.getInt8Ty(), variant_active_value_ptr, "variant_active_value" //
                    );
                    llvm::SwitchInst *active_value_switch = builder.CreateSwitch(variant_active_value, variant_free_merge_block);

                    // Now generate the content of each block, get the value of the variant and free the value in it
                    for (auto &[value_id, value_block] : possible_value_blocks) {
                        active_value_switch->addCase(builder.getInt8(value_id), value_block);
                        value_block->insertInto(ctx.parent);
                        builder.SetInsertPoint(value_block);
                        IR::generate_debug_print( //
                            &builder, ctx.parent->getParent(),
                            "Freeing variant '" + var_name + "' with value_id of '" + std::to_string(value_id) + "'", {} //
                        );
                        llvm::Value *variant_value_ptr = builder.CreateStructGEP(variant_struct_type, alloca, 1, "variant_value_ptr");
                        auto value_type = IR::get_type(ctx.parent->getParent(), possible_types.at(value_id).second);
                        const bool is_ptr = value_type.second.first;
                        llvm::Value *variant_value = IR::aligned_load(                                                                //
                            builder, is_ptr ? value_type.first->getPointerTo() : value_type.first, variant_value_ptr, "variant_value" //
                        );
                        llvm::Function *free_fn = c_functions.at(FREE);
                        builder.CreateCall(free_fn, {variant_value});
                        builder.CreateBr(variant_free_merge_block);
                    }

                    variant_free_merge_block->insertInto(ctx.parent);
                    builder.SetInsertPoint(variant_free_merge_block);
                }
                break;
            }
        }
    }
    builder.CreateBr(merge_block);
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);
    return true;
}

bool Generator::Statement::generate_array_cleanup(llvm::IRBuilder<> &builder, llvm::Value *arr_ptr, const ArrayType *array_type) {
    // Now get the complexity of the array
    size_t complexity = 0;
    while (true) {
        if (array_type->type->get_variation() == Type::Variation::ARRAY) {
            complexity++;
            array_type = array_type->type->as<ArrayType>();
            continue;
        } else if (array_type->type->to_string() == "str") {
            complexity++;
        }
        break;
    }
    builder.CreateCall(Module::Array::array_manip_functions.at("free_arr"), {arr_ptr, builder.getInt64(complexity)});
    return true;
}

bool Generator::Statement::generate_data_cleanup( //
    llvm::IRBuilder<> &builder,                   //
    GenerationContext &ctx,                       //
    llvm::Type *base_type,                        //
    llvm::Value *const alloca,                    //
    const std::shared_ptr<Type> &data_type        //
) {
    size_t field_id = 0;
    const DataNode *data_node = data_type->as<DataType>()->data_node;
    auto type = IR::get_type(ctx.parent->getParent(), data_type);
    llvm::Value *data_ptr = IR::aligned_load(builder, type.first->getPointerTo(), alloca, "data." + data_type->to_string() + ".ptr");
    for (const auto &field : data_node->fields) {
        const std::shared_ptr<Type> &field_type = std::get<1>(field);
        const auto field_variation = field_type->get_variation();
        if (field_variation == Type::Variation::DATA) {
            llvm::Type *new_base_type = IR::get_type(ctx.parent->getParent(), field_type).first;
            llvm::Value *field_ptr = builder.CreateStructGEP(base_type, data_ptr, field_id);
            if (!generate_data_cleanup(builder, ctx, new_base_type, field_ptr, field_type)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (field_variation == Type::Variation::ARRAY) {
            const auto *array_type = field_type->as<ArrayType>();
            llvm::Type *arr_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
            llvm::Value *field_ptr = builder.CreateStructGEP(base_type, data_ptr, field_id);
            llvm::Value *arr_ptr = IR::aligned_load(builder, arr_type->getPointerTo(), field_ptr);
            if (!generate_array_cleanup(builder, arr_ptr, array_type)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
        field_id++;
    }
    builder.CreateCall(c_functions.at(FREE), {data_ptr});
    return true;
}

bool Generator::Statement::generate_return_statement(llvm::IRBuilder<> &builder, GenerationContext &ctx, const ReturnNode *return_node) {
    // Get the return type of the function
    auto *return_struct_type = llvm::cast<llvm::StructType>(ctx.parent->getReturnType());

    // Allocate space for the function's return type (should be a struct type)
    llvm::AllocaInst *return_struct = builder.CreateAlloca(return_struct_type, nullptr, "ret_struct");
    return_struct->setAlignment(llvm::Align(Allocation::calculate_type_alignment(return_struct_type)));
    return_struct->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Create ret struct '" + return_struct->getName().str() + "' of type '" + return_struct_type->getName().str() + "'")));

    // First, always store the error code (0 for no error)
    llvm::Value *error_ptr = builder.CreateStructGEP(return_struct_type, return_struct, 0, "err_ptr");
    IR::aligned_store(builder, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), error_ptr);

    // If we have a return value, store it in the struct
    if (return_node != nullptr && return_node->return_value.has_value()) {
        // Generate the expression for the return value
        Expression::garbage_type garbage;
        auto return_value = Expression::generate_expression(builder, ctx, garbage, 0, return_node->return_value.value().get());
        if (!return_value.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }

        // If the rhs is of type `str`, delete the last "garbage", as thats the _actual_ return value
        if (return_node->return_value.value()->type->to_string() == "str" && garbage.count(0) > 0) {
            garbage.at(0).clear();
        }
        if (!clear_garbage(builder, garbage)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }

        // Then, save all values of the return_value in the return struct
        for (size_t i = 0; i < return_value.value().size(); i++) {
            llvm::Value *value_ptr = builder.CreateStructGEP( //
                return_struct_type,                           //
                return_struct,                                //
                i + 1,                                        //
                "ret_val_" + std::to_string(i)                //
            );
            llvm::StoreInst *value_store = IR::aligned_store(builder, return_value.value().at(i), value_ptr);
            value_store->setMetadata("comment",
                llvm::MDNode::get(context,
                    llvm::MDString::get(context,
                        "Store result " + std::to_string(i) + " in return '" + return_struct->getName().str() + "'")));
        }
    }

    // Clean up the function's scope before returning
    if (!generate_end_of_scope(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Generate the return instruction with the evaluated value
    llvm::LoadInst *return_struct_val = IR::aligned_load(builder, return_struct_type, return_struct, "ret_val");
    return_struct_val->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context, "Load allocated ret struct of type '" + return_struct_type->getName().str() + "'")));
    builder.CreateRet(return_struct_val);
    return true;
}

bool Generator::Statement::generate_throw_statement(llvm::IRBuilder<> &builder, GenerationContext &ctx, const ThrowNode *throw_node) {
    // Get the return type of the function
    auto *throw_struct_type = llvm::cast<llvm::StructType>(ctx.parent->getReturnType());

    // Allocate the struct and set all of its values to their respective default values
    llvm::AllocaInst *throw_struct = Allocation::generate_default_struct(builder, throw_struct_type, "throw_ret", true);
    throw_struct->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context, "Create default struct of type '" + throw_struct_type->getName().str() + "' except first value")));

    // Create the pointer to the error value (the 0th index of the struct)
    llvm::Value *error_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, 0, "err_ptr");
    // Generate the expression right of the throw statement, it has to be an error set
    Expression::garbage_type garbage;
    auto expr_result = Expression::generate_expression(builder, ctx, garbage, 0, throw_node->throw_value.get());
    llvm::Value *err_value = expr_result.value().front();
    // Store the error value in the struct
    IR::aligned_store(builder, err_value, error_ptr);

    // Clean up the function's scope before throwing an error
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (!generate_end_of_scope(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Go through all of the return types and check if there is a string value under them, create an empty string for those
    std::vector<std::shared_ptr<Type>> return_types;
    const std::shared_ptr<Type> &throw_type = throw_node->throw_value->type;
    if (throw_type->get_variation() == Type::Variation::GROUP) {
        return_types = throw_type->as<GroupType>()->types;
    } else {
        return_types.emplace_back(throw_type);
    }

    // Properly "create" return values of complex types
    for (size_t i = 0; i < return_types.size(); i++) {
        if (return_types[i]->to_string() == "str") {
            llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
            llvm::Value *empty_str = builder.CreateCall(init_str_fn, {builder.getInt64(0)}, "empty_str");
            llvm::Value *value_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, i + 1, "value_" + std::to_string(i) + "_ptr");
            IR::aligned_store(builder, empty_str, value_ptr);
        }
        // TODO: Implement this for other complex types too (like data)
    }

    // Generate the throw (return) instruction with the evaluated value
    llvm::LoadInst *throw_struct_val = IR::aligned_load(builder, throw_struct_type, throw_struct, "throw_val");
    throw_struct_val->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context, "Load allocated throw struct of type '" + throw_struct_type->getName().str() + "'")));
    builder.CreateRet(throw_struct_val);
    return true;
}

void Generator::Statement::generate_if_blocks(llvm::Function *parent, std::vector<llvm::BasicBlock *> &blocks, const IfNode *if_node) {
    // Count total number of branches and create blocks
    const IfNode *current = if_node;
    unsigned int branch_count = 0;

    while (current != nullptr) {
        if (branch_count != 0) {
            // Create then condition block (for the else if blocks)
            blocks.push_back(llvm::BasicBlock::Create(      //
                context,                                    //
                "then_cond" + std::to_string(branch_count)) //
            );
        }

        // Create then block
        blocks.push_back(llvm::BasicBlock::Create( //
            context,                               //
            "then" + std::to_string(branch_count)) //
        );

        // Check for else-if or else
        if (!current->else_scope.has_value()) {
            break;
        }

        const auto &else_scope = current->else_scope.value();
        if (std::holds_alternative<std::unique_ptr<IfNode>>(else_scope)) {
            current = std::get<std::unique_ptr<IfNode>>(else_scope).get();
            ++branch_count;
        } else {
            // If there's a final else block, create it
            const auto &else_statements = std::get<std::shared_ptr<Scope>>(else_scope)->body;
            if (else_statements.empty()) {
                // No empty body allowed
                THROW_BASIC_ERR(ERR_GENERATING);
            }
            blocks.push_back(llvm::BasicBlock::Create( //
                context,                               //
                "else" + std::to_string(branch_count), //
                parent)                                //
            );
            current = nullptr;
        }
    }

    // Create merge block (shared by all branches)
    blocks.push_back(llvm::BasicBlock::Create(context, "merge"));
}

bool Generator::Statement::generate_if_statement( //
    llvm::IRBuilder<> &builder,                   //
    GenerationContext &ctx,                       //
    std::vector<llvm::BasicBlock *> &blocks,      //
    unsigned int nesting_level,                   //
    const IfNode *if_node                         //
) {
    if (if_node == nullptr || if_node->condition == nullptr) {
        // Invalid IfNode: missing condition
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // First call (nesting_level == 0): Create all blocks for entire if-chain
    if (nesting_level == 0) {
        llvm::BasicBlock *current_block = builder.GetInsertBlock();
        generate_if_blocks(ctx.parent, blocks, if_node);
        builder.SetInsertPoint(current_block);
    }

    // Index calculation for current blocks
    unsigned int next_idx;
    unsigned int then_idx;

    if (nesting_level == 0) {
        // The initial if statement, branch between the initial if scope and the merge block / the next condition check
        then_idx = 0;
        next_idx = 1;
    } else {
        // An else if statement, branch between the next if scope (nesting_level * 2) or the next check, if present, or the merge block
        // afterwards
        then_idx = nesting_level * 2;
        next_idx = then_idx + 1;
    }

    // Generate the condition
    const std::shared_ptr<Scope> current_scope = ctx.scope;
    ctx.scope = if_node->then_scope->parent_scope;
    Expression::garbage_type garbage;
    group_mapping condition_arr = Expression::generate_expression(builder, ctx, garbage, 0, if_node->condition.get());
    if (!condition_arr.has_value()) {
        // Failed to generate condition expression
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    assert(condition_arr.value().size() == 1); // The condition must have a bool value type
    llvm::Value *condition = condition_arr.value().at(0);

    // Clear all garbage (temporary variables)
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Create conditional branch
    llvm::BranchInst *branch = builder.CreateCondBr( //
        condition,                                   //
        blocks[then_idx],                            //
        blocks[next_idx]                             //
    );
    branch->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Branch between '" + blocks[then_idx]->getName().str() + "' and '" + blocks[next_idx]->getName().str() +
                    "' based on condition '" + condition->getName().str() + "'")));

    // Generate then branch
    blocks[then_idx]->insertInto(ctx.parent);
    builder.SetInsertPoint(blocks[then_idx]);
    ctx.scope = if_node->then_scope;
    if (!generate_body(builder, ctx)) {
        return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Branch to merge block
        builder.CreateBr(blocks.back());
    }

    // Handle else-if or else
    if (if_node->else_scope.has_value()) {
        const auto &else_scope = if_node->else_scope.value();
        if (std::holds_alternative<std::unique_ptr<IfNode>>(else_scope)) {
            // Recursive call for else-if
            blocks[next_idx]->insertInto(ctx.parent);
            builder.SetInsertPoint(blocks[next_idx]);
            if (!generate_if_statement(                                  //
                    builder,                                             //
                    ctx,                                                 //
                    blocks,                                              //
                    nesting_level + 1,                                   //
                    std::get<std::unique_ptr<IfNode>>(else_scope).get()) //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else {
            // Handle final else, if it exists
            const auto &last_else_scope = std::get<std::shared_ptr<Scope>>(else_scope);
            if (!last_else_scope->body.empty()) {
                builder.SetInsertPoint(blocks[next_idx]);
                ctx.scope = last_else_scope;
                if (!generate_body(builder, ctx)) {
                    return false;
                }
                if (builder.GetInsertBlock()->getTerminator() == nullptr) {
                    // Branch to the merge block
                    builder.CreateBr(blocks.back());
                }
            }
        }
    }

    ctx.scope = current_scope;
    // Set the insert point to the merge block
    if (nesting_level == 0) {
        // Now add the merge block to the end of the function
        blocks.back()->insertInto(ctx.parent);
        builder.SetInsertPoint(blocks.back());
    }
    return true;
}

bool Generator::Statement::generate_do_while_loop(llvm::IRBuilder<> &builder, GenerationContext &ctx, const DoWhileNode *do_while_node) {
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    std::array<llvm::BasicBlock *, 3> do_while_blocks{};

    do_while_blocks[0] = llvm::BasicBlock::Create(context, "do_while_body", ctx.parent);
    do_while_blocks[1] = llvm::BasicBlock::Create(context, "do_while_cond", ctx.parent);
    last_looparound_blocks.emplace_back(do_while_blocks[1]);
    do_while_blocks[2] = llvm::BasicBlock::Create(context, "merge");
    last_loop_merge_blocks.emplace_back(do_while_blocks[2]);

    builder.SetInsertPoint(pred_block);
    llvm::BranchInst *init_do_while_br = builder.CreateBr(do_while_blocks[0]);
    init_do_while_br->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context, "Start the do-while loop in '" + do_while_blocks[0]->getName().str() + "'")));

    builder.SetInsertPoint(do_while_blocks[0]);
    ctx.scope = do_while_node->scope;
    if (!generate_body(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        builder.CreateBr(do_while_blocks[1]);
    }

    builder.SetInsertPoint(do_while_blocks[1]);
    const std::shared_ptr<Scope> current_scope = ctx.scope;
    ctx.scope = do_while_node->scope->get_parent();
    Expression::garbage_type garbage;
    group_mapping expression_arr = Expression::generate_expression(builder, ctx, garbage, 0, do_while_node->condition.get());
    llvm::Value *expression = expression_arr.value().front();
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::BranchInst *branch = builder.CreateCondBr(expression, do_while_blocks[0], do_while_blocks[2]);
    branch->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Continue loop in '" + do_while_blocks[0]->getName().str() + "' based on cond '" + expression->getName().str() + "'")));

    do_while_blocks[2]->insertInto(ctx.parent);

    ctx.scope = current_scope;
    builder.SetInsertPoint(do_while_blocks[2]);

    last_looparound_blocks.pop_back();
    last_loop_merge_blocks.pop_back();
    return true;
}

bool Generator::Statement::generate_while_loop(llvm::IRBuilder<> &builder, GenerationContext &ctx, const WhileNode *while_node) {
    // Get the current block, we need to add a branch instruction to this block to point to the while condition block
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Create the basic blocks for the condition check, the while body and the merge block
    std::array<llvm::BasicBlock *, 3> while_blocks{};
    // Create then condition block (for the else if blocks)
    while_blocks[0] = llvm::BasicBlock::Create(context, "while_cond", ctx.parent);
    last_looparound_blocks.emplace_back(while_blocks[0]);
    while_blocks[1] = llvm::BasicBlock::Create(context, "while_body", ctx.parent);
    while_blocks[2] = llvm::BasicBlock::Create(context, "merge");
    last_loop_merge_blocks.emplace_back(while_blocks[2]);

    // Create the branch instruction in the predecessor block to point to the while_cond block
    builder.SetInsertPoint(pred_block);
    llvm::BranchInst *init_while_br = builder.CreateBr(while_blocks[0]);
    init_while_br->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Start while loop in '" + while_blocks[0]->getName().str() + "'")));

    // Create the condition block's content
    builder.SetInsertPoint(while_blocks[0]);
    const std::shared_ptr<Scope> current_scope = ctx.scope;
    ctx.scope = while_node->scope->get_parent();
    Expression::garbage_type garbage;
    group_mapping expression_arr = Expression::generate_expression(builder, ctx, garbage, 0, while_node->condition.get());
    llvm::Value *expression = expression_arr.value().front();
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::BranchInst *branch = builder.CreateCondBr(expression, while_blocks[1], while_blocks[2]);
    branch->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Continue loop in '" + while_blocks[1]->getName().str() + "' based on cond '" + expression->getName().str() + "'")));

    // Create the while block's body
    builder.SetInsertPoint(while_blocks[1]);
    ctx.scope = while_node->scope;
    if (!generate_body(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Point back to the condition block to create the loop
        builder.CreateBr(while_blocks[0]);
    }

    // Now add the merge block to the end of the function
    while_blocks[2]->insertInto(ctx.parent);

    // Finally set the builder to the merge block again
    ctx.scope = current_scope;
    builder.SetInsertPoint(while_blocks[2]);
    last_looparound_blocks.pop_back();
    last_loop_merge_blocks.pop_back();
    return true;
}

bool Generator::Statement::generate_for_loop(llvm::IRBuilder<> &builder, GenerationContext &ctx, const ForLoopNode *for_node) {
    // Get the current block, we need to add a branch instruction to this block to point to the while condition block
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Generate the instructions from the definition scope (it should only contain the initializer statement, for example 'i32 i = 0')
    const std::shared_ptr<Scope> current_scope = ctx.scope;
    ctx.scope = for_node->definition_scope;
    if (!generate_body(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Create the basic blocks for the condition check, the while body and the merge block
    std::array<llvm::BasicBlock *, 4> for_blocks{};
    // Create then condition block (for the else if blocks)
    for_blocks[0] = llvm::BasicBlock::Create(context, "for_cond", ctx.parent);
    for_blocks[1] = llvm::BasicBlock::Create(context, "for_body", ctx.parent);
    for_blocks[2] = llvm::BasicBlock::Create(context, "for_looparound");
    last_looparound_blocks.emplace_back(for_blocks[2]);
    // Create the merge block but don't add it to the parent function yet
    for_blocks[3] = llvm::BasicBlock::Create(context, "merge");
    last_loop_merge_blocks.emplace_back(for_blocks[3]);

    // Create the branch instruction in the predecessor block to point to the for_cond block
    builder.SetInsertPoint(pred_block);
    ctx.scope = for_node->definition_scope;
    if (!generate_body(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::BranchInst *init_for_br = builder.CreateBr(for_blocks[0]);
    init_for_br->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Start for loop in '" + for_blocks[0]->getName().str() + "'")));

    // Create the condition block's content
    builder.SetInsertPoint(for_blocks[0]);
    ctx.scope = for_node->definition_scope;
    Expression::garbage_type garbage;
    group_mapping expression_arr = Expression::generate_expression(builder, ctx, garbage, 0, for_node->condition.get());
    llvm::Value *expression = expression_arr.value().at(0); // Conditions only are allowed to have one type, bool
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::BranchInst *branch = builder.CreateCondBr(expression, for_blocks[1], for_blocks[3]);
    branch->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Continue loop in '" + for_blocks[1]->getName().str() + "' based on cond '" + expression->getName().str() + "'")));

    // Create the for loop's body
    builder.SetInsertPoint(for_blocks[1]);
    ctx.scope = for_node->body;
    if (!generate_body(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Point to the looparound block to create the loop
        builder.CreateBr(for_blocks[2]);
    }

    // Now add the looparound block to the end of the function
    for_blocks[2]->insertInto(ctx.parent);
    builder.SetInsertPoint(for_blocks[2]);
    if (!generate_statement(builder, ctx, for_node->looparound)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    // Branch back to the loop's condition to finish the loop
    builder.CreateBr(for_blocks[0]);

    // Now add the merge block to the end of the function
    for_blocks[3]->insertInto(ctx.parent);

    // Finally set the builder to the merge block again
    ctx.scope = current_scope;
    last_looparound_blocks.pop_back();
    last_loop_merge_blocks.pop_back();
    builder.SetInsertPoint(for_blocks[3]);
    return true;
}

bool Generator::Statement::generate_enh_for_loop(llvm::IRBuilder<> &builder, GenerationContext &ctx, const EnhForLoopNode *for_node) {
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Create the basic blocks for the condition check, the loop body and the merge block
    std::array<llvm::BasicBlock *, 4> for_blocks{};
    // Create then condition block and the body of the loop
    for_blocks[0] = llvm::BasicBlock::Create(context, "for_cond", ctx.parent);
    for_blocks[1] = llvm::BasicBlock::Create(context, "for_body", ctx.parent);
    for_blocks[2] = llvm::BasicBlock::Create(context, "looparound");
    last_looparound_blocks.emplace_back(for_blocks[2]);
    // Create the merge block but don't add it to the parent function yet
    for_blocks[3] = llvm::BasicBlock::Create(context, "merge");
    last_loop_merge_blocks.emplace_back(for_blocks[3]);

    // Generate the iterable expression
    builder.SetInsertPoint(pred_block);
    Expression::garbage_type garbage{};
    group_mapping iterable = Expression::generate_expression(builder, ctx, garbage, 0, for_node->iterable.get());
    if (!iterable.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (iterable.value().size() > 1 && for_node->iterable->type->get_variation() != Type::Variation::RANGE) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *iterable_expr = iterable.value().front();
    llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Value *length = nullptr;
    llvm::Value *value_ptr = nullptr;
    llvm::Type *element_type = nullptr;
    llvm::Value *lower_bound = nullptr;
    llvm::Value *upper_bound = nullptr;
    const bool is_range = for_node->iterable->type->get_variation() == Type::Variation::RANGE;
    if (for_node->iterable->type->get_variation() == Type::Variation::ARRAY) {
        const auto *array_type = for_node->iterable->type->as<ArrayType>();
        llvm::Value *dim_ptr = builder.CreateStructGEP(str_type, iterable_expr, 0, "dim_ptr");
        llvm::Value *dimensionality = IR::aligned_load(builder, builder.getInt64Ty(), dim_ptr, "dimensionality");
        llvm::AllocaInst *length_alloca = builder.CreateAlloca(builder.getInt64Ty(), 0, nullptr, "length_alloca");
        IR::aligned_store(builder, builder.getInt64(1), length_alloca);
        llvm::Value *len_ptr = builder.CreateStructGEP(str_type, iterable_expr, 1, "len_ptr");
        for (size_t i = 0; i < array_type->dimensionality; i++) {
            llvm::Value *single_len_ptr = builder.CreateGEP(builder.getInt64Ty(), len_ptr, builder.getInt64(i));
            llvm::Value *single_len = IR::aligned_load(builder, builder.getInt64Ty(), single_len_ptr, "len_" + std::to_string(i));
            llvm::Value *len_val = IR::aligned_load(builder, builder.getInt64Ty(), length_alloca);
            len_val = builder.CreateMul(len_val, single_len);
            IR::aligned_store(builder, len_val, length_alloca);
        }
        length = IR::aligned_load(builder, builder.getInt64Ty(), length_alloca, "length");
        // The values start right after the lengths
        value_ptr = builder.CreateGEP(builder.getInt64Ty(), len_ptr, dimensionality);
        element_type = IR::get_type(ctx.parent->getParent(), array_type->type).first;
    } else if (is_range) {
        assert(iterable.value().size() == 2);
        lower_bound = iterable.value().front();
        upper_bound = iterable.value().back();
        llvm::Value *calculated_length = builder.CreateSub(upper_bound, lower_bound, "range_length");
        // Ensure length is positive
        llvm::Value *is_positive = builder.CreateICmpSGT(calculated_length, builder.getInt64(0), "is_positive");
        llvm::BasicBlock *range_error_block = llvm::BasicBlock::Create(context, "range_error", ctx.parent);
        llvm::BasicBlock *range_continue_block = llvm::BasicBlock::Create(context, "range_continue", ctx.parent);
        builder.CreateCondBr(is_positive, range_continue_block, range_error_block);

        builder.SetInsertPoint(range_error_block);
        // For simplicity, set length to 0 and continue
        // TODO: Print error that range is the wrong way around
        llvm::Value *error_length = builder.getInt64(0);
        llvm::Function *printf_function = c_functions.at(PRINTF);
        llvm::Value *err_format = IR::generate_const_string(ctx.parent->getParent(), "ERROR: Incorrect range used in for loop: %zu..%zu\n");
        builder.CreateCall(printf_function, {err_format, lower_bound, upper_bound});
        builder.CreateBr(range_continue_block);

        builder.SetInsertPoint(range_continue_block);
        llvm::PHINode *length_phi = builder.CreatePHI(builder.getInt64Ty(), 2, "length_phi");
        length_phi->addIncoming(calculated_length, pred_block);
        length_phi->addIncoming(error_length, range_error_block);
        length = length_phi;
        element_type = builder.getInt64Ty();
    } else {
        // Is a 'str' type
        llvm::Value *len_ptr = builder.CreateStructGEP(str_type, iterable_expr, 0, "len_ptr");
        length = IR::aligned_load(builder, builder.getInt64Ty(), len_ptr, "length");
        value_ptr = builder.CreateStructGEP(str_type, iterable_expr, 1, "value_ptr");
        element_type = builder.getInt8Ty();
    }

    llvm::Value *tuple_alloca = nullptr;
    llvm::Type *tuple_type = nullptr;
    llvm::Value *index_alloca = nullptr;
    if (std::holds_alternative<std::string>(for_node->iterators)) {
        const unsigned int scope_id = for_node->definition_scope->scope_id;
        const std::string tuple_alloca_name = "s" + std::to_string(scope_id) + "::" + std::get<std::string>(for_node->iterators);
        tuple_alloca = ctx.allocations.at(tuple_alloca_name);
        const auto tuple_var = for_node->definition_scope->variables.at(std::get<std::string>(for_node->iterators));
        tuple_type = IR::get_type(ctx.parent->getParent(), std::get<0>(tuple_var)).first;
        llvm::Value *idx_ptr = builder.CreateStructGEP(tuple_type, tuple_alloca, 0, "idx_ptr");
        IR::aligned_store(builder, builder.getInt64(0), idx_ptr);
    } else {
        const auto iterators = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(for_node->iterators);
        const unsigned int scope_id = for_node->definition_scope->scope_id;
        if (iterators.first.has_value()) {
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::" + iterators.first.value();
            index_alloca = ctx.allocations.at(index_alloca_name);
            IR::aligned_store(builder, builder.getInt64(0), index_alloca);
        } else {
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::IDX";
            index_alloca = ctx.allocations.at(index_alloca_name);
            IR::aligned_store(builder, builder.getInt64(0), index_alloca);
        }
        // The second element will be handled later
    }
    builder.CreateBr(for_blocks[0]);

    // Create the condition
    builder.SetInsertPoint(for_blocks[0]);
    // Check if the current index is smaller than the length to iterate through
    // First, get the current index
    llvm::Value *current_index = nullptr;
    llvm::Value *idx_ptr = nullptr;
    if (std::holds_alternative<std::string>(for_node->iterators)) {
        idx_ptr = builder.CreateStructGEP(tuple_type, tuple_alloca, 0, "idx_ptr");
        current_index = IR::aligned_load(builder, builder.getInt64Ty(), idx_ptr, "current_index");
    } else {
        current_index = IR::aligned_load(builder, builder.getInt64Ty(), index_alloca, "current_index");
    }
    // Then check if the index is still smaller than the length and branch accordingly
    llvm::Value *in_range = builder.CreateICmpULT(current_index, length, "in_range");
    builder.CreateCondBr(in_range, for_blocks[1], for_blocks[3]);

    // Now to the body itself. First we need to store the current element in its respective alloca / inside the tuple before we generate the
    // body
    builder.SetInsertPoint(for_blocks[1]);
    // Load the current element from the iterable
    llvm::Value *current_element_ptr = nullptr;
    llvm::Value *current_element = nullptr;
    if (is_range) {
        // For ranges, compute element = lower_bound + current_index
        current_element = builder.CreateAdd(lower_bound, current_index, "range_element");
        // If elem is stored in alloca, we'll store this value there below
    } else {
        current_element_ptr = builder.CreateGEP(element_type, value_ptr, current_index, "current_element_ptr");
        current_element = IR::aligned_load(builder, element_type, current_element_ptr, "current_element");
    }
    // We need to store the element in the tuple / in the element alloca
    if (std::holds_alternative<std::string>(for_node->iterators)) {
        llvm::Value *elem_ptr = builder.CreateStructGEP(tuple_type, tuple_alloca, 1, "elem_ptr");
        IR::aligned_store(builder, current_element, elem_ptr);
    } else {
        // If we have a elem variable the elem variable is actually just the iterable element itself
        const auto iterators = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(for_node->iterators);
        if (iterators.second.has_value()) {
            const unsigned int scope_id = for_node->definition_scope->scope_id;
            const std::string element_alloca_name = "s" + std::to_string(scope_id) + "::" + iterators.second.value();
            llvm::Value *const element_alloca = ctx.allocations.at(element_alloca_name);
            if (is_range) {
                IR::aligned_store(builder, current_element, element_alloca);
            } else {
                // For non-range, replace the old nullptr alloca with the new alloca
                assert(element_alloca == nullptr);
                ctx.allocations.erase(element_alloca_name);
                ctx.allocations.emplace(element_alloca_name, current_element_ptr);
            }
        }
    }
    // Then we generate the body itself
    auto old_scope = ctx.scope;
    ctx.scope = for_node->body;
    if (!generate_body(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    ctx.scope = old_scope;
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Point to the looparound block to create the loop
        builder.CreateBr(for_blocks[2]);
    }

    // At the looparound block we increment the index and branch back to the condition
    for_blocks[2]->insertInto(ctx.parent);
    builder.SetInsertPoint(for_blocks[2]);
    llvm::Value *new_index = builder.CreateAdd(current_index, builder.getInt64(1), "new_index");
    if (std::holds_alternative<std::string>(for_node->iterators)) {
        IR::aligned_store(builder, new_index, idx_ptr);
    } else {
        IR::aligned_store(builder, new_index, index_alloca);
    }
    // Branch back to the loop's condition to finish the loop
    builder.CreateBr(for_blocks[0]);

    // Finally set the insert point to the merge block and return
    for_blocks[3]->insertInto(ctx.parent);
    builder.SetInsertPoint(for_blocks[3]);
    last_looparound_blocks.pop_back();
    last_loop_merge_blocks.pop_back();
    return true;
}

bool Generator::Statement::generate_optional_switch_statement( //
    llvm::IRBuilder<> &builder,                                //
    GenerationContext &ctx,                                    //
    const SwitchStatement *switch_statement,                   //
    llvm::Value *switch_value                                  //
) {
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();
    const std::shared_ptr<Scope> original_scope = ctx.scope;
    std::vector<llvm::BasicBlock *> branch_blocks;
    branch_blocks.reserve(switch_statement->branches.size());
    llvm::BasicBlock *default_block = nullptr;
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge");

    int value_block_idx = -1;
    for (size_t i = 0; i < switch_statement->branches.size(); i++) {
        const auto &branch = switch_statement->branches[i];
        // Check if it's the default branch
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            if (default_block != nullptr) {
                // Two default blocks have been defined, only one is allowed
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "default", ctx.parent));
            default_block = branch_blocks[i];
        } else {
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "branch_" + std::to_string(i), ctx.parent));
        }
        builder.SetInsertPoint(branch_blocks[i]);
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::SWITCH_MATCH) {
            const auto *match_node = branch.matches.front()->as<SwitchMatchNode>();
            if (switch_statement->switcher->get_variation() != ExpressionNode::Variation::VARIABLE) {
                // Switching on non-variables is not supported yet
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return false;
            }
            const auto *switcher_var_node = switch_statement->switcher->as<VariableNode>();
            const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
            const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
            llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_statement->switcher->type, false);
            if (switch_value->getType()->isPointerTy()) {
                switch_value = IR::aligned_load(builder, opt_struct_type, switch_value, "loaded_rhs");
            }
            llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);
            const std::string var_str = "s" + std::to_string(branch.body->parent_scope->scope_id) + "::" + match_node->name;
            llvm::Value *real_value_reference = builder.CreateStructGEP(opt_struct_type, var_alloca, 1, "value_reference");
            ctx.allocations.emplace(var_str, real_value_reference);
            value_block_idx = i;
        }
        ctx.scope = branch.body;
        if (!generate_body(builder, ctx)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            // Point to the merge block if this case branch has no terminator
            builder.CreateBr(merge_block);
        }
    }
    // Now set the insert point to the pred block to actually generate the switch itself
    builder.SetInsertPoint(pred_block);

    // Because it's a switch on an optional we can have a simple conditional branch here instead of the switch
    if (switch_statement->switcher->get_variation() != ExpressionNode::Variation::VARIABLE) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return false;
    }
    const auto *switcher_var_node = switch_statement->switcher->as<VariableNode>();
    const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_statement->switcher->type, false);
    llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);
    // We just check for the "has_value" field and branch to our blocks depending on that field's value
    llvm::Value *has_value_ptr = builder.CreateStructGEP(opt_struct_type, var_alloca, 0, "has_value_ptr");
    llvm::Value *has_value = IR::aligned_load(builder, builder.getInt1Ty(), has_value_ptr, "has_value");
    llvm::BasicBlock *has_value_block = branch_blocks.at(value_block_idx);
    // If value block idx == 1 none block is 0, if it's 0 the none block is idx 1
    llvm::BasicBlock *none_block = branch_blocks.at(1 - value_block_idx);
    builder.CreateCondBr(has_value, has_value_block, none_block);

    // Set the insert point back to the merge block
    ctx.scope = original_scope;
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);
    return true;
}

bool Generator::Statement::generate_variant_switch_statement( //
    llvm::IRBuilder<> &builder,                               //
    GenerationContext &ctx,                                   //
    const SwitchStatement *switch_statement,                  //
    llvm::Value *switch_value                                 //
) {
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();
    const std::shared_ptr<Scope> original_scope = ctx.scope;
    std::vector<llvm::BasicBlock *> branch_blocks;
    branch_blocks.reserve(switch_statement->branches.size());
    llvm::BasicBlock *default_block = nullptr;
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge");

    if (switch_statement->switcher->get_variation() != ExpressionNode::Variation::VARIABLE) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return false;
    }
    const auto *switcher_var_node = switch_statement->switcher->as<VariableNode>();
    const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    // The switcher variable must be a variant type
    const auto *variant_type = switch_statement->switcher->type->as<VariantType>();
    llvm::StructType *variant_struct_type;
    if (variant_type->is_err_variant) {
        variant_struct_type = type_map.at("__flint_type_err");
    } else {
        variant_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_statement->switcher->type, false);
    }
    if (switch_value->getType()->isPointerTy()) {
        switch_value = IR::aligned_load(builder, variant_struct_type, switch_value, "loaded_rhs");
    }
    llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);

    for (size_t i = 0; i < switch_statement->branches.size(); i++) {
        const auto &branch = switch_statement->branches[i];
        // Check if it's the default branch, if it is this is the last branch to generate
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            if (default_block != nullptr) {
                // Two default blocks have been defined, only one is allowed
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "default", ctx.parent));
            default_block = branch_blocks[i];
            break;
        } else {
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "branch_" + std::to_string(i), ctx.parent));
        }
        builder.SetInsertPoint(branch_blocks[i]);

        const auto *match_node = branch.matches.front()->as<SwitchMatchNode>();
        const std::string var_str = "s" + std::to_string(branch.body->parent_scope->scope_id) + "::" + match_node->name;
        llvm::Value *real_value_reference = nullptr;
        if (variant_type->is_err_variant) {
            real_value_reference = var_alloca;
            if (match_node->type->to_string() == "anyerror") {
                default_block = branch_blocks[i];
            }
        } else {
            real_value_reference = builder.CreateStructGEP(variant_struct_type, var_alloca, 1, "value_reference");
        }
        ctx.allocations.emplace(var_str, real_value_reference);

        ctx.scope = branch.body;
        if (!generate_body(builder, ctx)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            // Point to the merge block if this case branch has no terminator
            builder.CreateBr(merge_block);
        }
    }
    // Now set the insert point to the pred block to actually generate the switch itself
    builder.SetInsertPoint(pred_block);

    // Create the switch instruction. Branch to the default block, if one exists, when no default block exists we jump to the merge
    // block
    llvm::SwitchInst *switch_inst = nullptr;
    switch_value = builder.CreateExtractValue(switch_value, {0}, "variant_flag");
    if (default_block == nullptr) {
        switch_inst = builder.CreateSwitch(switch_value, merge_block, switch_statement->branches.size());
    } else {
        switch_inst = builder.CreateSwitch(switch_value, default_block, switch_statement->branches.size() - 1);
    }

    // Add the cases to the switch instruction
    for (size_t i = 0; i < switch_statement->branches.size(); i++) {
        const auto &branch = switch_statement->branches[i];
        // Skip the default node, this block is not targetted directly by any switch expression
        if (branch_blocks[i] == default_block) {
            continue;
        }
        const auto *match_node = branch.matches.front()->as<SwitchMatchNode>();
        if (variant_type->is_err_variant) {
            const auto *err_set_type = match_node->type->as<ErrorSetType>();
            switch_inst->addCase(builder.getInt32(err_set_type->error_node->error_id), branch_blocks[i]);
        } else {
            switch_inst->addCase(builder.getInt8(match_node->id), branch_blocks[i]);
        }
    }

    // Set the insert point back to the merge block
    ctx.scope = original_scope;
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);
    return true;
}

bool Generator::Statement::generate_switch_statement( //
    llvm::IRBuilder<> &builder,                       //
    GenerationContext &ctx,                           //
    const SwitchStatement *switch_statement           //
) {
    // Generate the switch expression
    Expression::garbage_type garbage;
    group_mapping expr_result = Expression::generate_expression(builder, ctx, garbage, 0, switch_statement->switcher.get());
    llvm::Value *switch_value = expr_result.value().front();
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Generate the switch branches specially if we switch on various types
    const auto switcher_type_variation = switch_statement->switcher->type->get_variation();
    if (switcher_type_variation == Type::Variation::OPTIONAL) {
        return generate_optional_switch_statement(builder, ctx, switch_statement, switch_value);
    }
    if (switcher_type_variation == Type::Variation::VARIANT) {
        return generate_variant_switch_statement(builder, ctx, switch_statement, switch_value);
    }

    // Create the basic blocks for the switch branches and fill those basic blocks at the same time, e.g. generate the body of the switch
    // branches right here as well
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();
    std::vector<llvm::BasicBlock *> branch_blocks;
    branch_blocks.reserve(switch_statement->branches.size());
    const std::shared_ptr<Scope> original_scope = ctx.scope;
    llvm::BasicBlock *default_block = nullptr;
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(context, "merge");

    // It's a "normal" switch
    for (size_t i = 0; i < switch_statement->branches.size(); i++) {
        const auto &branch = switch_statement->branches[i];
        // Check if it's the default branch
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            if (default_block != nullptr) {
                // Two default blocks have been defined, only one is allowed
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "default", ctx.parent));
            default_block = branch_blocks[i];
        } else {
            branch_blocks.push_back(llvm::BasicBlock::Create(context, "branch_" + std::to_string(i), ctx.parent));
        }
        builder.SetInsertPoint(branch_blocks[i]);
        ctx.scope = branch.body;
        if (!generate_body(builder, ctx)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (builder.GetInsertBlock()->getTerminator() == nullptr) {
            // Point to the merge block if this case branch has no terminator
            builder.CreateBr(merge_block);
        }
    }
    // Now set the insert point to the pred block to actually generate the switch itself
    builder.SetInsertPoint(pred_block);

    // Create the switch instruction. Branch to the default block, if one exists, when no default block exists we jump to the merge
    // block
    if (switch_statement->switcher->type->get_variation() == Type::Variation::ERROR_SET) {
        switch_value = builder.CreateExtractValue(switch_value, {1}, "error_value");
    }
    llvm::SwitchInst *switch_inst = nullptr;
    if (default_block == nullptr) {
        switch_inst = builder.CreateSwitch(switch_value, merge_block, switch_statement->branches.size());
    } else {
        switch_inst = builder.CreateSwitch(switch_value, default_block, switch_statement->branches.size() - 1);
    }

    // Add the cases to the switch instruction
    for (size_t i = 0; i < switch_statement->branches.size(); i++) {
        const auto &branch = switch_statement->branches[i];
        // Skip the default node, this block is not targetted directly by any switch expression
        if (branch.matches.front()->get_variation() == ExpressionNode::Variation::DEFAULT) {
            continue;
        }

        // Generate the case values
        for (auto &match : branch.matches) {
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
            Expression::garbage_type case_garbage;
            group_mapping case_expr = Expression::generate_expression(builder, ctx, case_garbage, 0, match.get());
            llvm::Value *case_value = case_expr.value().front();
            if (!clear_garbage(builder, case_garbage)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }

            // Add the case to the switch
            llvm::ConstantInt *const_case = llvm::dyn_cast<llvm::ConstantInt>(case_value);
            if (!const_case) {
                // Switch case value must be a constant integer
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }

            switch_inst->addCase(const_case, branch_blocks[i]);
        }
    }

    // Set the insert point back to the merge block
    ctx.scope = original_scope;
    merge_block->insertInto(ctx.parent);
    builder.SetInsertPoint(merge_block);
    return true;
}

bool Generator::Statement::generate_catch_statement(llvm::IRBuilder<> &builder, GenerationContext &ctx, const CatchNode *catch_node) {
    // The catch statement is basically just an if check if the err value of the function return is != 0 or not
    const CallNodeBase *call_node = catch_node->call_node;
    const std::string err_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    llvm::Value *const err_var = ctx.allocations.at(err_ret_name);

    // Load the error value
    llvm::Type *const error_type = type_map.at("__flint_type_err");
    llvm::Value *err_val_ptr = builder.CreateStructGEP(error_type, err_var, 0, "err_val_ptr");
    llvm::LoadInst *err_val = IR::aligned_load(builder,                               //
        llvm::Type::getInt32Ty(context),                                              //
        err_val_ptr,                                                                  //
        call_node->function->name + "_" + std::to_string(call_node->call_id) + "_err" //
    );
    err_val->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Load err val of call '" + call_node->function->name + "::" + std::to_string(call_node->call_id) + "'")));

    llvm::BasicBlock *last_block = &ctx.parent->back();
    llvm::BasicBlock *first_block = &ctx.parent->front();
    // Create basic block for the catch block
    llvm::BasicBlock *current_block = builder.GetInsertBlock();

    // Check if the current block is the last block, if it is not, insert right after the current block
    bool will_insert_after = current_block == last_block || current_block != first_block;
    llvm::BasicBlock *insert_before = will_insert_after ? (current_block->getNextNode()) : current_block;

    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(                            //
        context,                                                                         //
        call_node->function->name + "_" + std::to_string(call_node->call_id) + "_catch", //
        ctx.parent,                                                                      //
        insert_before                                                                    //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(                            //
        context,                                                                         //
        call_node->function->name + "_" + std::to_string(call_node->call_id) + "_merge", //
        nullptr,                                                                         //
        insert_before                                                                    //
    );
    builder.SetInsertPoint(current_block);

    // Create the if check and compare the err value to 0
    llvm::ConstantInt *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    llvm::Value *err_condition = builder.CreateICmpNE( //
        err_val,                                       //
        zero,                                          //
        "errcmp"                                       //
    );

    // Create the branching operation
    builder.CreateCondBr(err_condition, catch_block, merge_block)
        ->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Branch to '" + catch_block->getName().str() + "' if '" + call_node->function->name + "' returned error")));

    const std::shared_ptr<Scope> current_scope = ctx.scope;
    ctx.scope = catch_node->scope;
    builder.SetInsertPoint(catch_block);

    std::string err_alloca_name;
    if (catch_node->var_name.has_value()) {
        // Add the error variable to the list of allocations (temporarily)
        err_alloca_name = "s" + std::to_string(catch_node->scope->scope_id) + "::" + catch_node->var_name.value();
        ctx.allocations.insert({err_alloca_name, ctx.allocations.at(err_ret_name)});
        // Generate the body of the catch block
        if (!generate_body(builder, ctx)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    } else {
        // Generate the implicit switch on the error value
        assert(catch_node->scope->body.size() == 1);
        const auto *switch_statement = catch_node->scope->body.front()->as<SwitchStatement>();
        // Add the error variable to the list of allocations (temporarily)
        err_alloca_name = "s" + std::to_string(catch_node->scope->scope_id) + "::__flint_value_err";
        ctx.allocations.insert({err_alloca_name, ctx.allocations.at(err_ret_name)});
        if (!generate_variant_switch_statement(builder, ctx, switch_statement, err_var)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    // Remove the error variable from the list of allocations
    ctx.allocations.erase(ctx.allocations.find(err_alloca_name));

    // Add branch to the merge block from the catch block if it does not contain a terminator (return or throw)
    // If the catch block has its own blocks, we actually dont need to check the catch block but the second last block in the function
    // (the last one is the merge block)
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        builder.CreateBr(merge_block);
    }

    // Now add the merge block to the end of the function
    merge_block->insertInto(ctx.parent);

    // Set the insert block to the merge block again
    ctx.scope = current_scope;
    builder.SetInsertPoint(merge_block);
    return true;
}

bool Generator::Statement::generate_group_declaration( //
    llvm::IRBuilder<> &builder,                        //
    GenerationContext &ctx,                            //
    const GroupDeclarationNode *declaration_node       //
) {
    Expression::garbage_type garbage;
    auto expression = Expression::generate_expression(builder, ctx, garbage, 0, declaration_node->initializer.get());
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    assert(declaration_node->variables.size() == expression.value().size());

    // Delete all level-0 garbage, as thats the "garbage" thats saved on the variables
    if (garbage.count(0) > 0) {
        garbage.at(0).clear();
    }
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    unsigned int elem_idx = 0;
    for (const auto &variable : declaration_node->variables) {
        const std::string variable_name = "s" + std::to_string(ctx.scope->scope_id) + "::" + variable.second;
        llvm::Value *const variable_alloca = ctx.allocations.at(variable_name);
        llvm::Value *elem_value = expression.value().at(elem_idx);
        IR::aligned_store(builder, elem_value, variable_alloca);
        elem_idx++;
    }
    return true;
}

bool Generator::Statement::generate_declaration( //
    llvm::IRBuilder<> &builder,                  //
    GenerationContext &ctx,                      //
    const DeclarationNode *declaration_node      //
) {
    const unsigned int scope_id = std::get<1>(ctx.scope->variables.at(declaration_node->name));
    const std::string var_name = "s" + std::to_string(scope_id) + "::" + declaration_node->name;
    llvm::Value *const alloca = ctx.allocations.at(var_name);

    llvm::Value *expression;
    if (declaration_node->initializer.has_value()) {
        Expression::garbage_type garbage;
        const bool is_reference = declaration_node->type->get_variation() == Type::Variation::OPTIONAL;
        auto expr_val = Expression::generate_expression(                                        //
            builder, ctx, garbage, 0, declaration_node->initializer.value().get(), is_reference //
        );
        if (!expr_val.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        // Delete all level-0 garbage, as thats the "garbage" thats saved on the variables
        if (garbage.count(0) > 0) {
            garbage.at(0).clear();
        }
        if (!clear_garbage(builder, garbage)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        switch (declaration_node->type->get_variation()) {
            default:
                break;
            case Type::Variation::TUPLE: {
                assert(expr_val.value().size() == 1);
                IR::aligned_store(builder, expr_val.value().front(), alloca);
                return true;
            }
            case Type::Variation::OPTIONAL: {
                if (declaration_node->initializer.value()->get_variation() == ExpressionNode::Variation::TYPE_CAST) {
                    const auto *typecast_node = declaration_node->initializer.value()->as<TypeCastNode>();
                    if (typecast_node->expr->type->to_string() == "void?") {
                        break;
                    }
                }
                // We do not execute this branch if the rhs is a 'none' literal, as this would cause problems (zero-initializer of T? being
                // stored on the 'value' property of the optional struct, leading to the byte next to the struct being overwritten, e.g. UB)
                // Furthermore, if the RHS already is the correct optional type we also do not execute this branch as this would also lead
                // to a double-store of the optional value. Luckily, we can detect whether the RHS is already a complete optional by just
                // checking whether the LLVM type of the expression's type matches our expected optional type
                llvm::StructType *var_type = IR::add_and_or_get_type(ctx.parent->getParent(), declaration_node->type, false);
                const bool types_match = expr_val.value().front()->getType() == var_type;
                if (types_match) {
                    break;
                }
                // Get the pointer to the i1 element of the optional variable and set it to 1
                llvm::Value *var_has_value_ptr = builder.CreateStructGEP(var_type, alloca, 0, declaration_node->name + "_has_value_ptr");
                llvm::StoreInst *store = IR::aligned_store(builder, builder.getInt1(1), var_has_value_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context, "Set 'has_value' property of optional '" + declaration_node->name + "' to 1")));
                llvm::Value *var_value_ptr = builder.CreateStructGEP(var_type, alloca, 1, declaration_node->name + "_value_ptr");
                store = IR::aligned_store(builder, expr_val.value().front(), var_value_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context, "Store result of expr in var '" + declaration_node->name + "'")));
                return true;
            }
            case Type::Variation::VARIANT: {
                const auto *var_type = declaration_node->type->as<VariantType>();
                // We first check of which type the rhs really is. If it's a typecast, then we know it's one of the "inner" variations of
                // the variant, if it's a variant directly then we can store the variant in the variable as is. This means we dont need to
                // do anything if the typecast is a nullptr
                if (declaration_node->initializer.value()->get_variation() != ExpressionNode::Variation::TYPE_CAST) {
                    break;
                }
                const auto *typecast_node = declaration_node->initializer.value()->as<TypeCastNode>();
                // First, we need to get the ID of the type within the variant
                std::optional<unsigned int> index = var_type->get_idx_of_type(typecast_node->expr->type);
                if (!index.has_value()) {
                    // Rhs has wrong type
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                llvm::StructType *variant_type = IR::add_and_or_get_type(ctx.parent->getParent(), declaration_node->type, false);
                llvm::Value *flag_ptr = builder.CreateStructGEP(variant_type, alloca, 0, declaration_node->name + "_flag_ptr");
                llvm::StoreInst *store = IR::aligned_store(builder, builder.getInt8(index.value()), flag_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context,
                            "Set 'flag' property of variant '" + declaration_node->name + "' to '" + std::to_string(index.value()) +
                                "' for type '" + typecast_node->expr->type->to_string() + "'")));
                llvm::Value *value_ptr = builder.CreateStructGEP(variant_type, alloca, 1, declaration_node->name + "_value_ptr");
                store = IR::aligned_store(builder, expr_val.value().front(), value_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context, "Store actual variant value in var '" + declaration_node->name + "'")));
                return true;
            }
        }
        expression = expr_val.value().front();
    } else {
        expression = IR::get_default_value_of_type(builder, ctx.parent->getParent(), declaration_node->type);
    }

    if (declaration_node->type->to_string() == "str") {
        std::optional<const ExpressionNode *> initializer;
        if (declaration_node->initializer.has_value()) {
            initializer = declaration_node->initializer.value().get();
        } else {
            initializer = std::nullopt;
        }
        expression = Module::String::generate_string_declaration(builder, expression, initializer);
    }
    llvm::StoreInst *store = IR::aligned_store(builder, expression, alloca);
    store->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Store the actual val of '" + declaration_node->name + "'")));
    return true;
}

bool Generator::Statement::generate_assignment(llvm::IRBuilder<> &builder, GenerationContext &ctx, const AssignmentNode *assignment_node) {
    Expression::garbage_type garbage;
    const bool is_reference = assignment_node->type->get_variation() == Type::Variation::OPTIONAL;
    auto expr = Expression::generate_expression(builder, ctx, garbage, 0, assignment_node->expression.get(), is_reference);
    if (!expr.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    // If the rhs is of type `str`, delete the last "garbage", as thats the _actual_ value
    if (assignment_node->expression->type->to_string() == "str" && garbage.count(0) > 0) {
        garbage.at(0).clear();
    }
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Check if the variable is declared
    if (ctx.scope->variables.find(assignment_node->name) == ctx.scope->variables.end()) {
        // Error: Undeclared Variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    // Get the allocation of the lhs
    const std::shared_ptr<Type> variable_type = std::get<0>(ctx.scope->variables.at(assignment_node->name));
    const unsigned int variable_decl_scope = std::get<1>(ctx.scope->variables.at(assignment_node->name));
    llvm::Value *const lhs = ctx.allocations.at("s" + std::to_string(variable_decl_scope) + "::" + assignment_node->name);

    // If its a group type we have to handle it differently than when its a single value
    if (assignment_node->expression->type->get_variation() == Type::Variation::GROUP) {
        const auto *group_type = assignment_node->expression->type->as<GroupType>();
        if (assignment_node->type->get_variation() != Type::Variation::TUPLE) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        const auto *tuple_type = assignment_node->type->as<TupleType>();
        if (group_type->types != tuple_type->types) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        llvm::StructType *tuple_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), assignment_node->type, false);
        for (size_t i = 0; i < tuple_type->types.size(); i++) {
            llvm::Value *element_ptr = builder.CreateStructGEP(tuple_struct_type, lhs, i, "tuple_elem_" + std::to_string(i));
            IR::aligned_store(builder, expr.value()[i], element_ptr);
        }
        return true;
    } else if (variable_type->get_variation() == Type::Variation::OPTIONAL) {
        const auto *optional_type = variable_type->as<OptionalType>();
        llvm::StructType *var_type = IR::add_and_or_get_type(ctx.parent->getParent(), variable_type, false);
        if (optional_type->base_type->to_string() == "str") {
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("str")).first;
            llvm::Value *var_value_ptr = builder.CreateStructGEP(var_type, lhs, 1, assignment_node->name + "value_ptr");
            llvm::Value *actual_str_ptr = IR::aligned_load(builder, str_type->getPointerTo(), var_value_ptr, "actual_str_ptr");
            builder.CreateCall(c_functions.at(FREE), {actual_str_ptr});
        }
        // We do not execute this branch if the rhs is a 'none' literal, as this would cause problems (zero-initializer of T? being
        // stored on the 'value' property of the optional struct, leading to the byte next to the struct being overwritten, e.g. UB)
        // Furthermore, if the RHS already is the correct optional type we also do not execute this branch as this would also lead to a
        // double-store of the optional value. Luckily, we can detect whether the RHS is already a complete optional by just checking
        // whether the LLVM type of the expression's type matches our expected optional type
        const bool types_match = expr.value().front()->getType() == var_type;
        const TypeCastNode *rhs_cast = dynamic_cast<const TypeCastNode *>(assignment_node->expression.get());
        if (!types_match && (rhs_cast == nullptr || rhs_cast->expr->type->to_string() != "void?")) {
            // Get the pointer to the i1 element of the optional variable and set it to 1
            llvm::Value *var_has_value_ptr = builder.CreateStructGEP(var_type, lhs, 0, assignment_node->name + "_has_value_ptr");
            llvm::StoreInst *store = IR::aligned_store(builder, builder.getInt1(1), var_has_value_ptr);
            store->setMetadata("comment",
                llvm::MDNode::get(context,
                    llvm::MDString::get(context, "Set 'has_value' property of optional '" + assignment_node->name + "' to 1")));

            // Check if the base type is complex
            auto base_type_info = IR::get_type(ctx.parent->getParent(), optional_type->base_type);
            llvm::Type *base_type = base_type_info.first;
            bool is_complex = base_type_info.second.first;
            llvm::Value *var_value_ptr = builder.CreateStructGEP(var_type, lhs, 1, assignment_node->name + "value_ptr");
            if (is_complex) {
                // For complex types, allocate memory and store a pointer
                llvm::Value *type_size = builder.getInt64(Allocation::get_type_size(ctx.parent->getParent(), base_type));
                llvm::Value *allocated_memory = builder.CreateCall(                               //
                    c_functions.at(MALLOC), {type_size}, assignment_node->name + "allocated_data" //
                );
                IR::aligned_store(builder, expr.value().front(), allocated_memory);
                store = IR::aligned_store(builder, allocated_memory, var_value_ptr);
            } else {
                // For simple types, store the value directly
                store = IR::aligned_store(builder, expr.value().front(), var_value_ptr);
            }
            store->setMetadata("comment",
                llvm::MDNode::get(context, llvm::MDString::get(context, "Store result of expr in var '" + assignment_node->name + "'")));
            return true;
        }
    } else if (assignment_node->type->get_variation() == Type::Variation::VARIANT) {
        const auto *var_type = assignment_node->type->as<VariantType>();
        // We first check of which type the rhs really is. If it's a typecast, then we know it's one of the "inner" variations of the
        // variant, if it's a variant directly then we can store the variant in the variable as is. This means we dont need to do
        // anything if the typecast is a nullptr
        if (assignment_node->expression->get_variation() == ExpressionNode::Variation::TYPE_CAST) {
            const auto *typecast_node = assignment_node->expression->as<TypeCastNode>();
            // First, we need to get the ID of the type within the variant
            std::optional<unsigned int> index = var_type->get_idx_of_type(typecast_node->expr->type);
            if (!index.has_value()) {
                // Rhs has wrong type
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            llvm::StructType *variant_type = IR::add_and_or_get_type(ctx.parent->getParent(), assignment_node->type, false);
            llvm::Value *flag_ptr = builder.CreateStructGEP(variant_type, lhs, 0, assignment_node->name + "_flag_ptr");
            llvm::StoreInst *store = IR::aligned_store(builder, builder.getInt8(index.value()), flag_ptr);
            store->setMetadata("comment",
                llvm::MDNode::get(context,
                    llvm::MDString::get(context,
                        "Set 'flag' property of variant '" + assignment_node->name + "' to '" + std::to_string(index.value()) +
                            "' for type '" + typecast_node->expr->type->to_string() + "'")));
            llvm::Value *value_ptr = builder.CreateStructGEP(variant_type, lhs, 1, assignment_node->name + "_value_ptr");
            store = IR::aligned_store(builder, expr.value().front(), value_ptr);
            store->setMetadata("comment",
                llvm::MDNode::get(context,
                    llvm::MDString::get(context, "Store actual variant value in var '" + assignment_node->name + "'")));
            return true;
        }
    }
    // Its definitely a single value
    llvm::Value *expression = expr.value().front();
    if (assignment_node->type->to_string() == "str") {
        // Only generate the string assignment if its not a shorthand
        if (!assignment_node->is_shorthand) {
            Module::String::generate_string_assignment(builder, lhs, assignment_node->expression.get(), expression);
        }
        return true;
    } else {
        llvm::StoreInst *store = IR::aligned_store(builder, expression, lhs);
        store->setMetadata("comment",
            llvm::MDNode::get(context, llvm::MDString::get(context, "Store result of expr in var '" + assignment_node->name + "'")));
    }
    return true;
}

bool Generator::Statement::generate_group_assignment( //
    llvm::IRBuilder<> &builder,                       //
    GenerationContext &ctx,                           //
    const GroupAssignmentNode *group_assignment       //
) {
    Expression::garbage_type garbage;
    auto expression = Expression::generate_expression(builder, ctx, garbage, 0, group_assignment->expression.get());
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Delete all level-0 garbage, as thats the "garbage" thats saved on the variables
    if (garbage.count(0) > 0) {
        garbage.at(0).clear();
    }
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    unsigned int elem_idx = 0;
    for (const auto &assign : group_assignment->assignees) {
        const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(assign.second));
        const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + assign.second;
        llvm::Value *const alloca = ctx.allocations.at(var_name);
        llvm::Value *elem_value = expression.value().at(elem_idx);
        IR::aligned_store(builder, elem_value, alloca);
        elem_idx++;
    }
    return true;
}

bool Generator::Statement::generate_data_field_assignment( //
    llvm::IRBuilder<> &builder,                            //
    GenerationContext &ctx,                                //
    const DataFieldAssignmentNode *data_field_assignment   //
) {
    // Just save the result of the expression in the field of the data
    Expression::garbage_type garbage;
    const bool is_reference = data_field_assignment->field_type->get_variation() == Type::Variation::OPTIONAL;
    group_mapping expression = Expression::generate_expression(                         //
        builder, ctx, garbage, 0, data_field_assignment->expression.get(), is_reference //
    );
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    // Delete all level-0 garbage, as thats the "garbage" thats saved on the variables
    if (garbage.count(0) > 0) {
        garbage.at(0).clear();
    }
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *expr_val = expression.value().front();
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(data_field_assignment->var_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + data_field_assignment->var_name;
    llvm::Value *const var_alloca = ctx.allocations.at(var_name);

    if (data_field_assignment->data_type->to_string() == "bool8") {
        // The 'field access' is actually the bit at the given field index
        // Load the current value of the bool8 (i8)
        llvm::Value *current_value = IR::aligned_load(builder, builder.getInt8Ty(), var_alloca, var_name + "_val");
        // Get the boolean value from the expression
        llvm::Value *bool_value = expression.value().front();
        // Set or clear the specific bit based on the bool value
        unsigned int bit_index = data_field_assignment->field_id;
        // Get the new value of the bool8 value
        llvm::Value *new_value = Expression::set_bool8_element_at(builder, current_value, bool_value, bit_index);
        // Store the new value back
        llvm::StoreInst *store = IR::aligned_store(builder, new_value, var_alloca);
        store->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Store result of expr in field '" + data_field_assignment->var_name + ".$" +
                        std::to_string(data_field_assignment->field_id) + "'")));
        return true;
    }

    auto data_type = IR::get_type(ctx.parent->getParent(), data_field_assignment->data_type);
    llvm::Value *field_ptr = var_alloca;
    bool is_fn_param = false;
    for (auto &arg : ctx.parent->args()) {
        if (arg.getName() == data_field_assignment->var_name) {
            is_fn_param = true;
            break;
        }
    }
    if (data_type.second.first && !is_fn_param) {
        field_ptr = IR::aligned_load(builder, data_type.first->getPointerTo(), var_alloca, data_field_assignment->var_name + "_ptr");
    }
    field_ptr = builder.CreateStructGEP(data_type.first, field_ptr, data_field_assignment->field_id);

    // Check if the field is a complex type and create an allocation before storing
    // Check if the field is an optional type and check whether to need an allocation for the optional value

    // Get the type of the field we're assigning to
    const DataType *struct_data_type = dynamic_cast<const DataType *>(data_field_assignment->data_type.get());
    if (struct_data_type != nullptr && data_field_assignment->field_id < struct_data_type->data_node->fields.size()) {
        // Get the field type from the struct definition
        auto field_it = struct_data_type->data_node->fields.begin();
        std::advance(field_it, data_field_assignment->field_id);
        const std::shared_ptr<Type> &field_type = field_it->second;

        // Check if the field is an optional type
        if (field_type->get_variation() == Type::Variation::OPTIONAL) {
            const auto *optional_type = field_type->as<OptionalType>();
            const TypeCastNode *rhs_cast = data_field_assignment->expression->as<TypeCastNode>();
            llvm::StructType *field_optional_type = IR::add_and_or_get_type(ctx.parent->getParent(), field_type, false);

            // Handle special cases (like str cleanup)
            if (optional_type->base_type->to_string() == "str") {
                llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("str")).first;
                llvm::Value *field_value_ptr = builder.CreateStructGEP(field_optional_type, field_ptr, 1, "field_value_ptr");
                llvm::Value *actual_str_ptr = IR::aligned_load(builder, str_type->getPointerTo(), field_value_ptr, "actual_str_ptr");
                builder.CreateCall(c_functions.at(FREE), {actual_str_ptr});
            }

            // Check if we need to handle optional conversion
            const bool types_match = expr_val->getType() == field_optional_type;
            if (!types_match && (rhs_cast == nullptr || rhs_cast->expr->type->to_string() != "void?")) {
                // Set has_value to true
                llvm::Value *field_has_value_ptr = builder.CreateStructGEP(field_optional_type, field_ptr, 0, "field_has_value_ptr");
                llvm::StoreInst *store = IR::aligned_store(builder, builder.getInt1(1), field_has_value_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context, llvm::MDString::get(context, "Set 'has_value' property of optional field to 1")));

                // Store the value in the optional
                llvm::Value *field_value_ptr = builder.CreateStructGEP(field_optional_type, field_ptr, 1, "field_value_ptr");
                store = IR::aligned_store(builder, expr_val, field_value_ptr);

                if (data_field_assignment->field_name.has_value()) {
                    store->setMetadata("comment",
                        llvm::MDNode::get(context,
                            llvm::MDString::get(context,
                                "Store result of expr in optional field '" + data_field_assignment->var_name + "." +
                                    data_field_assignment->field_name.value() + "'")));
                } else {
                    store->setMetadata("comment",
                        llvm::MDNode::get(context,
                            llvm::MDString::get(context,
                                "Store result of expr in optional field '" + data_field_assignment->var_name + ".$" +
                                    std::to_string(data_field_assignment->field_id) + "'")));
                }
                return true;
            }
        }
    }

    llvm::StoreInst *store = IR::aligned_store(builder, expr_val, field_ptr);
    if (data_field_assignment->field_name.has_value()) {
        store->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Store result of expr in field '" + data_field_assignment->var_name + "." + data_field_assignment->field_name.value() +
                        "'")));
    } else {
        store->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Store result of expr in field '" + data_field_assignment->var_name + ".$" +
                        std::to_string(data_field_assignment->field_id) + "'")));
    }
    return true;
}

bool Generator::Statement::generate_grouped_data_field_assignment( //
    llvm::IRBuilder<> &builder,                                    //
    GenerationContext &ctx,                                        //
    const GroupedDataFieldAssignmentNode *grouped_field_assignment //
) {
    // Just save the result of the expression in the field of the data
    Expression::garbage_type garbage;
    group_mapping expression = Expression::generate_expression(builder, ctx, garbage, 0, grouped_field_assignment->expression.get());
    if (!expression.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    // Delete all level-0 garbage, as thats the "garbage" thats saved on the variables
    if (garbage.count(0) > 0) {
        garbage.at(0).clear();
    }
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(grouped_field_assignment->var_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + grouped_field_assignment->var_name;
    llvm::Value *const var_alloca = ctx.allocations.at(var_name);

    if (grouped_field_assignment->data_type->to_string() == "bool8") {
        // Load the current value of the bool8 (i8)
        llvm::Value *current_value = IR::aligned_load(builder, builder.getInt8Ty(), var_alloca, var_name + "_val");
        llvm::Value *new_value = current_value;

        // Process each field in the grouped assignment
        for (size_t i = 0; i < grouped_field_assignment->field_ids.size(); i++) {
            unsigned int bit_index = grouped_field_assignment->field_ids.at(i);
            llvm::Value *bool_value = expression.value().at(i);
            new_value = Expression::set_bool8_element_at(builder, new_value, bool_value, bit_index);
        }

        // Store the final value back
        llvm::StoreInst *store = IR::aligned_store(builder, new_value, var_alloca);

        // Add metadata comment
        std::string fields_str;
        for (size_t i = 0; i < grouped_field_assignment->field_ids.size(); i++) {
            if (i > 0)
                fields_str += ", ";
            fields_str += "$" + std::to_string(grouped_field_assignment->field_ids.at(i));
        }
        store->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Store result of expr in fields '" + grouped_field_assignment->var_name + ".(" + fields_str + ")'")));
        return true;
    }

    auto data_type = IR::get_type(ctx.parent->getParent(), grouped_field_assignment->data_type);
    llvm::Value *alloca = var_alloca;
    bool is_fn_param = false;
    for (auto &arg : ctx.parent->args()) {
        if (arg.getName() == grouped_field_assignment->var_name) {
            is_fn_param = true;
            break;
        }
    }
    if (data_type.second.first && !is_fn_param) {
        alloca = IR::aligned_load(builder, data_type.first->getPointerTo(), var_alloca, grouped_field_assignment->var_name + "_ptr");
    }
    for (size_t i = 0; i < expression.value().size(); i++) {
        llvm::Value *field_ptr = builder.CreateStructGEP(data_type.first, alloca, grouped_field_assignment->field_ids.at(i));
        llvm::StoreInst *store = IR::aligned_store(builder, expression.value().at(i), field_ptr);
        store->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Store result of expr in field '" + grouped_field_assignment->var_name + "." +
                        grouped_field_assignment->field_names.at(i) + "'")));
    }
    return true;
}

bool Generator::Statement::generate_array_assignment( //
    llvm::IRBuilder<> &builder,                       //
    GenerationContext &ctx,                           //
    const ArrayAssignmentNode *array_assignment       //
) {
    // Generate the main expression
    Expression::garbage_type garbage;
    group_mapping expression_result = Expression::generate_expression(builder, ctx, garbage, 0, array_assignment->expression.get());
    if (!expression_result.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (expression_result.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *expression = expression_result.value().front();
    if (!array_assignment->expression->type->equals(array_assignment->value_type)) {
        expression =
            Expression::generate_type_cast(builder, ctx, expression, array_assignment->expression->type, array_assignment->value_type);
    }
    // Generate all the indexing expressions
    std::vector<llvm::Value *> idx_expressions;
    for (auto &idx_expression : array_assignment->indexing_expressions) {
        group_mapping idx_expr = Expression::generate_expression(builder, ctx, garbage, 0, idx_expression.get());
        if (!idx_expr.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (idx_expr.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        idx_expressions.emplace_back(idx_expr.value().at(0));
    }
    // Store all the results of the index expressions in the indices array
    llvm::Value *const indices = ctx.allocations.at("arr::idx::" + std::to_string(array_assignment->indexing_expressions.size()));
    for (size_t i = 0; i < idx_expressions.size(); i++) {
        llvm::Value *idx_ptr = builder.CreateGEP(builder.getInt64Ty(), indices, builder.getInt64(i), "idx_ptr_" + std::to_string(i));
        IR::aligned_store(builder, idx_expressions[i], idx_ptr);
    }
    // Get the array value
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(array_assignment->variable_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + array_assignment->variable_name;
    llvm::Value *const array_alloca = ctx.allocations.at(var_name);
    llvm::Type *arr_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first->getPointerTo();
    // Check if this is a function parameter - if so, use it directly without loading
    llvm::Value *array_ptr;
    if (std::get<3>(ctx.scope->variables.at(array_assignment->variable_name))) {
        // It's a function parameter (or enhanced for loop variable), use the alloca directly
        array_ptr = array_alloca;
    } else {
        // It's a local variable, load the pointer from the alloca
        array_ptr = IR::aligned_load(builder, arr_type, array_alloca, "array_ptr");
    }
    if (array_assignment->expression->type->to_string() == "str") {
        // This call returns a 'str**'
        llvm::Value *element_ptr = builder.CreateCall(             //
            Module::Array::array_manip_functions.at("access_arr"), //
            {array_ptr, builder.getInt64(8), indices}              //
        );
        // The string assignment will call the 'assign_str' function, which takes in a 'str**' argument for its dest, so this is correct
        Module::String::generate_string_assignment(builder, element_ptr, array_assignment->expression.get(), expression);
        return true;
    }

    // For types larger than 8 bytes (like structs/tuples), use direct store via access_arr
    const unsigned int element_size_bytes = Allocation::get_type_size(ctx.parent->getParent(), expression->getType());
    if (element_size_bytes > 8) {
        // Get pointer to the array element
        llvm::Value *element_ptr = builder.CreateCall(                 //
            Module::Array::array_manip_functions.at("access_arr"),     //
            {array_ptr, builder.getInt64(element_size_bytes), indices} //
        );
        // Cast the char* to the correct pointer type
        llvm::Value *typed_element_ptr = builder.CreateBitCast(element_ptr, expression->getType()->getPointerTo(), "typed_element_ptr");
        // Store the value directly
        IR::aligned_store(builder, expression, typed_element_ptr);
        return true;
    }

    // For primitives <= 8 bytes, use the `assign_arr_val_at` function instead
    llvm::Type *to_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("i64")).first;
    const unsigned int expr_bitwidth = expression->getType()->getPrimitiveSizeInBits();
    expression = IR::generate_bitwidth_change(builder, expression, expr_bitwidth, 64, to_type);
    // Call the `assign_at_val` function
    builder.CreateCall(                                                                     //
        Module::Array::array_manip_functions.at("assign_arr_val_at"),                       //
        {array_ptr, builder.getInt64(std::max(1U, expr_bitwidth / 8)), indices, expression} //
    );
    return true;
}

bool Generator::Statement::generate_stacked_assignment( //
    llvm::IRBuilder<> &builder,                         //
    GenerationContext &ctx,                             //
    const StackedAssignmentNode *stacked_assignment     //
) {
    // Generate the main expression
    Expression::garbage_type garbage;
    group_mapping expression_result = Expression::generate_expression(builder, ctx, garbage, 0, stacked_assignment->expression.get());
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (!expression_result.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (expression_result.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *expression = expression_result.value().front();
    if (!stacked_assignment->expression->type->equals(stacked_assignment->field_type)) {
        expression = Expression::generate_type_cast(                                                       //
            builder, ctx, expression, stacked_assignment->expression->type, stacked_assignment->field_type //
        );
    }
    // Now we can create the "base expression" which then gets accessed
    group_mapping base_expr_res = Expression::generate_expression(builder, ctx, garbage, 0, stacked_assignment->base_expression.get());
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (!base_expr_res.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (base_expr_res.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *base_expr = base_expr_res.value().front();
    if (stacked_assignment->base_expression->type->to_string() == "bool8") {
        // TODO: Find a way how to store the return value of the `set_bool8_element_at` function back at the value the stacked
        // expression came from (we need a pointer to the data field, if the bool8 variable is stored in another data, for example). We
        // currently only get the actual loaded value of bool8, and there is no way to get a pointer to where it came from. This
        // definitely needs to be done, otherwise stacked assignments for the bool8 type will not work. It still works for tuple types
        // and other multi-types, so this is a bool8-specific issue
        //
        // Expression::set_bool8_element_at(builder, base_expr, expression, stacked_assignment->field_id);
        return false;
    }
    // Now we can access the element of the data of the lhs and assign the rhs expression result to it
    // TOOD: Stacked assignments do not work for any multi-types yet, as the vector type is loaded as a "normal" value still.
    llvm::Type *base_type = IR::get_type(ctx.parent->getParent(), stacked_assignment->base_expression->type).first;
    llvm::Value *field_ptr = builder.CreateStructGEP(base_type, base_expr, stacked_assignment->field_id, "field_ptr");
    IR::aligned_store(builder, expression, field_ptr);
    return true;
}

bool Generator::Statement::generate_stacked_array_assignment( //
    llvm::IRBuilder<> &builder,                               //
    GenerationContext &ctx,                                   //
    const StackedArrayAssignmentNode *stacked_assignment      //
) {
    // Generate the main expression
    Expression::garbage_type garbage;
    group_mapping expression_result = Expression::generate_expression(builder, ctx, garbage, 0, stacked_assignment->expression.get());
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (!expression_result.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (expression_result.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *expression = expression_result.value().front();

    // Now we can create the "base expression" which then gets accessed
    group_mapping base_expr_res = Expression::generate_expression(builder, ctx, garbage, 0, stacked_assignment->base_expression.get());
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (!base_expr_res.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (base_expr_res.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *base_expr = base_expr_res.value().front();

    // Generate all the indexing expressions
    std::vector<llvm::Value *> idx_expressions;
    for (auto &idx_expression : stacked_assignment->indexing_expressions) {
        group_mapping idx_expr = Expression::generate_expression(builder, ctx, garbage, 0, idx_expression.get());
        if (!idx_expr.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (idx_expr.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        idx_expressions.emplace_back(idx_expr.value().at(0));
    }
    // We need to make a special case if the "array" is a string
    if (stacked_assignment->base_expression->type->to_string() == "str") {
        // We do a normal string assignment at a given position
        assert(stacked_assignment->expression->type->to_string() == "u8");
        assert(idx_expressions.size() == 1);
        llvm::Function *assign_str_at_fn = Module::String::string_manip_functions.at("assign_str_at");
        builder.CreateCall(assign_str_at_fn, {base_expr, idx_expressions.front(), expression});
        return true;
    }

    // Store all the results of the index expressions in the indices array
    llvm::Value *const indices = ctx.allocations.at("arr::idx::" + std::to_string(stacked_assignment->indexing_expressions.size()));
    for (size_t i = 0; i < idx_expressions.size(); i++) {
        llvm::Value *idx_ptr = builder.CreateGEP(builder.getInt64Ty(), indices, builder.getInt64(i), "idx_ptr_" + std::to_string(i));
        IR::aligned_store(builder, idx_expressions[i], idx_ptr);
    }
    // The base expression should return the pointer to the array directly
    llvm::Value *array_ptr = base_expr;
    if (stacked_assignment->expression->type->to_string() == "str") {
        // This call returns a 'str**'
        llvm::Value *element_ptr = builder.CreateCall(             //
            Module::Array::array_manip_functions.at("access_arr"), //
            {array_ptr, builder.getInt64(8), indices}              //
        );
        // The string assignment will call the 'assign_str' function, which takes in a 'str**' argument for its dest, so this is correct
        Module::String::generate_string_assignment(builder, element_ptr, stacked_assignment->expression.get(), expression);
        return true;
    }

    // For types larger than 8 bytes (like structs/tuples), use direct store via access_arr
    const unsigned int element_size_bytes = Allocation::get_type_size(ctx.parent->getParent(), expression->getType());
    if (element_size_bytes > 8) {
        // Get pointer to the array element
        llvm::Value *element_ptr = builder.CreateCall(                 //
            Module::Array::array_manip_functions.at("access_arr"),     //
            {array_ptr, builder.getInt64(element_size_bytes), indices} //
        );
        // Cast the char* to the correct pointer type
        llvm::Value *typed_element_ptr = builder.CreateBitCast(element_ptr, expression->getType()->getPointerTo(), "typed_element_ptr");
        // Store the value directly
        IR::aligned_store(builder, expression, typed_element_ptr);
        return true;
    }

    // For primitives <= 8 bytes, use the `assign_arr_val_at` function instead
    llvm::Type *to_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("i64")).first;
    const unsigned int expr_bitwidth = expression->getType()->getPrimitiveSizeInBits();
    expression = IR::generate_bitwidth_change(builder, expression, expr_bitwidth, 64, to_type);
    // Call the `assign_at_val` function
    builder.CreateCall(                                                                     //
        Module::Array::array_manip_functions.at("assign_arr_val_at"),                       //
        {array_ptr, builder.getInt64(std::max(1U, expr_bitwidth / 8)), indices, expression} //
    );
    return true;
}

bool Generator::Statement::generate_stacked_grouped_assignment( //
    llvm::IRBuilder<> &builder,                                 //
    GenerationContext &ctx,                                     //
    const StackedGroupedAssignmentNode *stacked_assignment      //
) {
    // Generate the rhs expression
    Expression::garbage_type garbage;
    group_mapping expression_result = Expression::generate_expression(builder, ctx, garbage, 0, stacked_assignment->expression.get());
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (!expression_result.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (expression_result.value().size() != stacked_assignment->field_names.size()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (stacked_assignment->expression->type->get_variation() != Type::Variation::GROUP) {
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    const auto *expr_group_type = stacked_assignment->expression->type->as<GroupType>();
    // Now we can create the "base expression" which then gets accessed
    group_mapping base_expr_res = Expression::generate_expression(builder, ctx, garbage, 0, stacked_assignment->base_expression.get());
    if (!clear_garbage(builder, garbage)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (!base_expr_res.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (base_expr_res.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *base_expr = base_expr_res.value().front();
    llvm::Type *base_type = IR::get_type(ctx.parent->getParent(), stacked_assignment->base_expression->type).first;
    for (size_t i = 0; i < expression_result.value().size(); i++) {
        llvm::Value *expression = expression_result.value().at(i);

        if (expr_group_type->types.at(i) != stacked_assignment->field_types.at(i)) {
            expression = Expression::generate_type_cast(                                                      //
                builder, ctx, expression, expr_group_type->types.at(i), stacked_assignment->field_types.at(i) //
            );
        }
        if (stacked_assignment->field_types.at(i)->to_string() == "bool8") {
            // TODO: Find a way how to store the return value of the `set_bool8_element_at` function back at the value the stacked
            // expression came from (we need a pointer to the data field, if the bool8 variable is stored in another data, for example).
            // We currently only get the actual loaded value of bool8, and there is no way to get a pointer to where it came from. This
            // definitely needs to be done, otherwise stacked assignments for the bool8 type will not work. It still works for tuple
            // types and other multi-types, so this is a bool8-specific issue
            //
            // Expression::set_bool8_element_at(builder, base_expr, expression, stacked_assignment->field_id);
            return false;
        }
        // Now we can access the element of the data of the lhs and assign the rhs expression result to it
        // TOOD: Stacked assignments do not work for any multi-types yet, as the vector type is loaded as a "normal" value still.
        llvm::Value *field_ptr = builder.CreateStructGEP(base_type, base_expr, stacked_assignment->field_ids.at(i), "field_ptr");
        IR::aligned_store(builder, expression, field_ptr);
    }
    return true;
}

bool Generator::Statement::generate_unary_op_statement( //
    llvm::IRBuilder<> &builder,                         //
    GenerationContext &ctx,                             //
    const UnaryOpStatement *unary_op                    //
) {
    if (unary_op->operand->get_variation() != ExpressionNode::Variation::VARIABLE) {
        // Expression is not a variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    const auto *var_node = unary_op->operand->as<VariableNode>();
    const unsigned int scope_id = std::get<1>(ctx.scope->variables.at(var_node->name));
    const std::string var_name = "s" + std::to_string(scope_id) + "::" + var_node->name;
    llvm::Value *const alloca = ctx.allocations.at(var_name);

    llvm::LoadInst *var_value = IR::aligned_load(builder,            //
        IR::get_type(ctx.parent->getParent(), var_node->type).first, //
        alloca,                                                      //
        var_node->name + "_val"                                      //
    );
    var_value->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Load val of var '" + var_node->name + "'")));
    llvm::Value *operation_result = nullptr;

    if (var_node->type->get_variation() == Type::Variation::GROUP) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    const std::string var_type = var_node->type->to_string();
    switch (unary_op->operator_token) {
        default:
            // Unknown unary operator
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        case TOK_INCREMENT:
            if (var_type == "i32") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateAdd(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("i32_safe_add"), {var_value, one}, "safe_add_res" //
                      );
            } else if (var_type == "i64") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateAdd(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("i64_safe_add"), {var_value, one}, "safe_add_res" //
                      );
            } else if (var_type == "u8") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt8Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateAdd(var_value, one)
                    : builder.CreateCall(                                                                              //
                          Module::Arithmetic::arithmetic_functions.at("u8_safe_add"), {var_value, one}, "safe_add_res" //
                      );
            } else if (var_type == "u32") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateAdd(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("u32_safe_add"), {var_value, one}, "safe_add_res" //
                      );
            } else if (var_type == "u64") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateAdd(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("u64_safe_add"), {var_value, one}, "safe_add_res" //
                      );
            } else if (var_type == "f32" || var_type == "f64") {
                llvm::Value *one = llvm::ConstantFP::get(var_value->getType(), 1.0);
                operation_result = builder.CreateFAdd(var_value, one);
            } else {
                // Type not allowed for increment operator
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        case TOK_DECREMENT:
            if (var_type == "i32") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateSub(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("i32_safe_sub"), {var_value, one}, "safe_sub_res" //
                      );
            } else if (var_type == "i64") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateSub(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("i64_safe_sub"), {var_value, one}, "safe_sub_res" //
                      );
            } else if (var_type == "u8") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt8Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateSub(var_value, one)
                    : builder.CreateCall(                                                                              //
                          Module::Arithmetic::arithmetic_functions.at("u8_safe_sub"), {var_value, one}, "safe_sub_res" //
                      );
            } else if (var_type == "u32") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateSub(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("u32_safe_sub"), {var_value, one}, "safe_sub_res" //
                      );
            } else if (var_type == "u64") {
                llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                operation_result = overflow_mode == ArithmeticOverflowMode::UNSAFE
                    ? builder.CreateSub(var_value, one)
                    : builder.CreateCall(                                                                               //
                          Module::Arithmetic::arithmetic_functions.at("u64_safe_sub"), {var_value, one}, "safe_sub_res" //
                      );
            } else if (var_type == "f32" || var_type == "f64") {
                llvm::Value *one = llvm::ConstantFP::get(var_value->getType(), 1.0);
                operation_result = builder.CreateFSub(var_value, one);
            } else {
                // Type not allowed for decrement operator
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
    }
    llvm::StoreInst *operation_store = IR::aligned_store(builder, operation_result, alloca);
    operation_store->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Store result of unary operation on '" + var_node->name + "'")));
    return true;
}
