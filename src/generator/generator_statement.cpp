#include "error/error.hpp"
#include "generator/generator.hpp"
#include "globals.hpp"
#include "lexer/builtins.hpp"

#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/statements/break_node.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/continue_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <string>

bool Generator::Statement::generate_statement(      //
    llvm::IRBuilder<> &builder,                     //
    GenerationContext &ctx,                         //
    const std::unique_ptr<StatementNode> &statement //
) {
    if (const auto *call_node = dynamic_cast<const CallNodeStatement *>(statement.get())) {
        group_mapping gm = Expression::generate_call(builder, ctx, dynamic_cast<const CallNodeBase *>(call_node));
        return gm.has_value();
    } else if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement.get())) {
        return generate_return_statement(builder, ctx, return_node);
    } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement.get())) {
        std::vector<llvm::BasicBlock *> blocks;
        return generate_if_statement(builder, ctx, blocks, 0, if_node);
    } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement.get())) {
        return generate_while_loop(builder, ctx, while_node);
    } else if (const auto *for_node = dynamic_cast<const ForLoopNode *>(statement.get())) {
        return generate_for_loop(builder, ctx, for_node);
    } else if (const auto *enh_for_node = dynamic_cast<const EnhForLoopNode *>(statement.get())) {
        return generate_enh_for_loop(builder, ctx, enh_for_node);
    } else if (const auto *group_assignment_node = dynamic_cast<const GroupAssignmentNode *>(statement.get())) {
        return generate_group_assignment(builder, ctx, group_assignment_node);
    } else if (const auto *assignment_node = dynamic_cast<const AssignmentNode *>(statement.get())) {
        return generate_assignment(builder, ctx, assignment_node);
    } else if (const auto *group_declaration_node = dynamic_cast<const GroupDeclarationNode *>(statement.get())) {
        return generate_group_declaration(builder, ctx, group_declaration_node);
    } else if (const auto *declaration_node = dynamic_cast<const DeclarationNode *>(statement.get())) {
        return generate_declaration(builder, ctx, declaration_node);
    } else if (const auto *throw_node = dynamic_cast<const ThrowNode *>(statement.get())) {
        return generate_throw_statement(builder, ctx, throw_node);
    } else if (const auto *catch_node = dynamic_cast<const CatchNode *>(statement.get())) {
        return generate_catch_statement(builder, ctx, catch_node);
    } else if (const auto *unary_node = dynamic_cast<const UnaryOpStatement *>(statement.get())) {
        return generate_unary_op_statement(builder, ctx, unary_node);
    } else if (const auto *field_assignment_node = dynamic_cast<const DataFieldAssignmentNode *>(statement.get())) {
        return generate_data_field_assignment(builder, ctx, field_assignment_node);
    } else if (const auto *grouped_assignment_node = dynamic_cast<const GroupedDataFieldAssignmentNode *>(statement.get())) {
        return generate_grouped_data_field_assignment(builder, ctx, grouped_assignment_node);
    } else if (const auto *array_assignment_node = dynamic_cast<const ArrayAssignmentNode *>(statement.get())) {
        return generate_array_assignment(builder, ctx, array_assignment_node);
    } else if (const auto *stacked_assignment_node = dynamic_cast<const StackedAssignmentNode *>(statement.get())) {
        return generate_stacked_assignment(builder, ctx, stacked_assignment_node);
    } else if (const auto *stacked_grouped_assignment_node = dynamic_cast<const StackedGroupedAssignmentNode *>(statement.get())) {
        return generate_stacked_grouped_assignment(builder, ctx, stacked_grouped_assignment_node);
    } else if (const auto *switch_statement = dynamic_cast<const SwitchStatement *>(statement.get())) {
        return generate_switch_statement(builder, ctx, switch_statement);
    } else if (dynamic_cast<const BreakNode *>(statement.get())) {
        builder.CreateBr(last_loop_merge_blocks.back());
        return true;
    } else if (dynamic_cast<const ContinueNode *>(statement.get())) {
        builder.CreateBr(last_looparound_blocks.back());
        return true;
    } else {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
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
            if (const auto primitive_type = dynamic_cast<const PrimitiveType *>(type.get())) {
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
            } else if (const auto array_type = dynamic_cast<const ArrayType *>(type.get())) {
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

    // Only generate end of scope if the last statement was not a return statement
    if (ctx.scope->parent_scope != nullptr && !dynamic_cast<const ReturnNode *>(ctx.scope->body.back().get()) &&
        !dynamic_cast<const ThrowNode *>(ctx.scope->body.back().get())) {
        success &= generate_end_of_scope(builder, ctx);
    }
    return success;
}

bool Generator::Statement::generate_end_of_scope(llvm::IRBuilder<> &builder, GenerationContext &ctx) {
    // First, get all variables of this scope that went out of scope
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
        // Check if the variable is of type str
        std::shared_ptr<Type> var_type = std::get<0>(var_info);
        if (const auto primitive_type = dynamic_cast<const PrimitiveType *>(var_type.get())) {
            if (primitive_type->type_name != "str") {
                continue;
            }
            // Get the allocation of the variable
            const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
            llvm::Value *const alloca = ctx.allocations.at(alloca_name);
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
            llvm::Value *str_ptr = builder.CreateLoad(str_type->getPointerTo(), alloca, var_name + "_cleanup");
            builder.CreateCall(c_functions.at(FREE), {str_ptr});
        } else if (ArrayType *array_type = dynamic_cast<ArrayType *>(var_type.get())) {
            const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
            llvm::Value *const alloca = ctx.allocations.at(alloca_name);
            llvm::Type *arr_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
            llvm::Value *arr_ptr = builder.CreateLoad(arr_type->getPointerTo(), alloca, var_name + "_cleanup");
            if (!generate_array_cleanup(builder, arr_ptr, array_type)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (DataType *data_type = dynamic_cast<DataType *>(var_type.get())) {
            const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
            llvm::Value *const alloca = ctx.allocations.at(alloca_name);
            llvm::Type *base_type = IR::get_type(ctx.parent->getParent(), var_type).first;
            if (!generate_data_cleanup(builder, ctx, base_type, alloca, data_type->data_node)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto variant_type = dynamic_cast<const VariantType *>(var_type.get())) {
            if (variant_type->is_err_variant) {
                const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
                llvm::Value *const alloca = ctx.allocations.at(alloca_name);
                llvm::StructType *error_type = type_map.at("__flint_type_err");
                llvm::Value *err_message_ptr = builder.CreateStructGEP(error_type, alloca, 2, "err_message_ptr");
                llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("str")).first;
                llvm::Value *err_message = builder.CreateLoad(str_type, err_message_ptr, "err_message");
                llvm::CallInst *free_call = builder.CreateCall(c_functions.at(FREE), {err_message});
                free_call->setMetadata("comment",
                    llvm::MDNode::get(context, llvm::MDString ::get(context, "Clear error message from variant '" + var_name + "'")));
            }
        } else if (dynamic_cast<const ErrorSetType *>(var_type.get())) {
            const std::string alloca_name = "s" + std::to_string(std::get<1>(var_info)) + "::" + var_name;
            llvm::Value *const alloca = ctx.allocations.at(alloca_name);
            llvm::StructType *error_type = type_map.at("__flint_type_err");
            llvm::Value *err_message_ptr = builder.CreateStructGEP(error_type, alloca, 2, "err_message_ptr");
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("str")).first;
            llvm::Value *err_message = builder.CreateLoad(str_type, err_message_ptr, "err_message");
            llvm::CallInst *free_call = builder.CreateCall(c_functions.at(FREE), {err_message});
            free_call->setMetadata("comment",
                llvm::MDNode::get(context, llvm::MDString ::get(context, "Clear error message from error '" + var_name + "'")));
        }
    }
    return true;
}

bool Generator::Statement::generate_array_cleanup(llvm::IRBuilder<> &builder, llvm::Value *arr_ptr, const ArrayType *array_type) {
    // Now get the complexity of the array
    size_t complexity = 0;
    while (true) {
        if (ArrayType *element_array_type = dynamic_cast<ArrayType *>(array_type->type.get())) {
            complexity++;
            array_type = element_array_type;
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
    const DataNode *data_node,                    //
    const size_t cleanup_depth                    //
) {
    size_t field_id = 0;
    for (const auto &field : data_node->fields) {
        if (const DataType *data_type = dynamic_cast<const DataType *>(std::get<1>(field).get())) {
            llvm::Type *new_base_type = IR::get_type(ctx.parent->getParent(), std::get<1>(field)).first;
            llvm::Value *field_ptr = builder.CreateStructGEP(base_type, alloca, field_id);
            llvm::Value *field_alloca = builder.CreateLoad(new_base_type->getPointerTo(), field_ptr);
            if (!generate_data_cleanup(builder, ctx, new_base_type, field_alloca, data_type->data_node, cleanup_depth + 1)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const ArrayType *array_type = dynamic_cast<const ArrayType *>(std::get<1>(field).get())) {
            llvm::Type *arr_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
            llvm::Value *field_ptr = builder.CreateStructGEP(base_type, alloca, field_id);
            llvm::Value *arr_ptr = builder.CreateLoad(arr_type->getPointerTo(), field_ptr);
            if (!generate_array_cleanup(builder, arr_ptr, array_type)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
        field_id++;
    }
    if (cleanup_depth != 0) {
        builder.CreateCall(c_functions.at(FREE), {alloca});
    }
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
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), error_ptr);

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
            llvm::StoreInst *value_store = builder.CreateStore(return_value.value().at(i), value_ptr);
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
    llvm::LoadInst *return_struct_val = builder.CreateLoad(return_struct_type, return_struct, "ret_val");
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
    builder.CreateStore(err_value, error_ptr);

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
    if (const GroupType *group_type = dynamic_cast<const GroupType *>(throw_node->throw_value->type.get())) {
        return_types = group_type->types;
    } else {
        return_types.emplace_back(throw_node->throw_value->type);
    }

    // Properly "create" return values of complex types
    for (size_t i = 0; i < return_types.size(); i++) {
        if (return_types[i]->to_string() == "str") {
            llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
            llvm::Value *empty_str = builder.CreateCall(init_str_fn, {builder.getInt64(0)}, "empty_str");
            llvm::Value *value_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, i + 1, "value_" + std::to_string(i) + "_ptr");
            builder.CreateStore(empty_str, value_ptr);
        }
        // TODO: Implement this for other complex types too (like data)
    }

    // Generate the throw (return) instruction with the evaluated value
    llvm::LoadInst *throw_struct_val = builder.CreateLoad(throw_struct_type, throw_struct, "throw_val");
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
    llvm::Value *expression = expression_arr.value().at(0);
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
    if (iterable.value().size() > 1) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    llvm::Value *iterable_expr = iterable.value().front();
    const ArrayType *array_type = dynamic_cast<const ArrayType *>(for_node->iterable->type.get());
    const bool is_array = array_type != nullptr;
    llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
    llvm::Value *length = nullptr;
    llvm::Value *value_ptr = nullptr;
    llvm::Type *element_type = nullptr;
    if (is_array) {
        llvm::Value *dim_ptr = builder.CreateStructGEP(str_type, iterable_expr, 0, "dim_ptr");
        llvm::Value *dimensionality = builder.CreateLoad(builder.getInt64Ty(), dim_ptr, "dimensionality");
        llvm::AllocaInst *length_alloca = builder.CreateAlloca(builder.getInt64Ty(), 0, nullptr, "length_alloca");
        builder.CreateStore(builder.getInt64(1), length_alloca);
        llvm::Value *len_ptr = builder.CreateStructGEP(str_type, iterable_expr, 1, "len_ptr");
        for (size_t i = 0; i < array_type->dimensionality; i++) {
            llvm::Value *single_len_ptr = builder.CreateGEP(builder.getInt64Ty(), len_ptr, builder.getInt64(i));
            llvm::Value *single_len = builder.CreateLoad(builder.getInt64Ty(), single_len_ptr, "len_" + std::to_string(i));
            llvm::Value *len_val = builder.CreateLoad(builder.getInt64Ty(), length_alloca);
            len_val = builder.CreateMul(len_val, single_len);
            builder.CreateStore(len_val, length_alloca);
        }
        length = builder.CreateLoad(builder.getInt64Ty(), length_alloca, "length");
        // The values start right after the lengths
        value_ptr = builder.CreateGEP(builder.getInt64Ty(), len_ptr, dimensionality);
        element_type = IR::get_type(ctx.parent->getParent(), array_type->type).first;
    } else {
        // Is 'str' type
        llvm::Value *len_ptr = builder.CreateStructGEP(str_type, iterable_expr, 0, "len_ptr");
        length = builder.CreateLoad(builder.getInt64Ty(), len_ptr, "length");
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
        builder.CreateStore(builder.getInt64(0), idx_ptr);
    } else {
        const auto iterators = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(for_node->iterators);
        const unsigned int scope_id = for_node->definition_scope->scope_id;
        if (iterators.first.has_value()) {
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::" + iterators.first.value();
            index_alloca = ctx.allocations.at(index_alloca_name);
            builder.CreateStore(builder.getInt64(0), index_alloca);
        } else {
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::IDX";
            index_alloca = ctx.allocations.at(index_alloca_name);
            builder.CreateStore(builder.getInt64(0), index_alloca);
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
        current_index = builder.CreateLoad(builder.getInt64Ty(), idx_ptr, "current_index");
    } else {
        current_index = builder.CreateLoad(builder.getInt64Ty(), index_alloca, "current_index");
    }
    // Then check if the index is still smaller than the length and branch accordingly
    llvm::Value *in_range = builder.CreateICmpULT(current_index, length, "in_range");
    builder.CreateCondBr(in_range, for_blocks[1], for_blocks[3]);

    // Now to the body itself. First we need to store the current element in its respective alloca / inside the tuple before we generate the
    // body
    builder.SetInsertPoint(for_blocks[1]);
    // Load the current element from the iterable
    llvm::Value *current_element_ptr = builder.CreateGEP(element_type, value_ptr, current_index, "current_element_ptr");
    // We need to store the element in the tuple / in the element alloca
    if (std::holds_alternative<std::string>(for_node->iterators)) {
        llvm::Value *elem_ptr = builder.CreateStructGEP(tuple_type, tuple_alloca, 1, "elem_ptr");
        llvm::Value *current_element = builder.CreateLoad(element_type, current_element_ptr, "current_element");
        builder.CreateStore(current_element, elem_ptr);
    } else {
        // If we have a elem variable the elem variable is actually just the iterable element itself
        const auto iterators = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(for_node->iterators);
        if (iterators.second.has_value()) {
            const unsigned int scope_id = for_node->definition_scope->scope_id;
            const std::string element_alloca_name = "s" + std::to_string(scope_id) + "::" + iterators.second.value();
            // Replace the old nullptr alloca with the new alloca
            assert(ctx.allocations.at(element_alloca_name) == nullptr);
            ctx.allocations.erase(element_alloca_name);
            ctx.allocations.emplace(element_alloca_name, current_element_ptr);
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
        builder.CreateStore(new_index, idx_ptr);
    } else {
        builder.CreateStore(new_index, index_alloca);
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
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
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
        const SwitchMatchNode *match_node = dynamic_cast<const SwitchMatchNode *>(branch.matches.front().get());
        if (match_node != nullptr) {
            auto switcher_var_node = dynamic_cast<const VariableNode *>(switch_statement->switcher.get());
            if (switcher_var_node == nullptr) {
                // Switching on non-variables is not supported yet
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return false;
            }
            const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
            const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
            llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_statement->switcher->type, false);
            if (switch_value->getType()->isPointerTy()) {
                switch_value = builder.CreateLoad(opt_struct_type, switch_value, "loaded_rhs");
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
        if (!generate_end_of_scope(builder, ctx)) {
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
    auto switcher_var_node = dynamic_cast<const VariableNode *>(switch_statement->switcher.get());
    if (switcher_var_node == nullptr) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return false;
    }
    const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_statement->switcher->type, false);
    llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);
    // We just check for the "has_value" field and branch to our blocks depending on that field's value
    llvm::Value *has_value_ptr = builder.CreateStructGEP(opt_struct_type, var_alloca, 0, "has_value_ptr");
    llvm::Value *has_value = builder.CreateLoad(builder.getInt1Ty(), has_value_ptr, "has_value");
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

    auto switcher_var_node = dynamic_cast<const VariableNode *>(switch_statement->switcher.get());
    if (switcher_var_node == nullptr) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return false;
    }
    const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    // The switcher variable must be a variant type
    const VariantType *variant_type = dynamic_cast<const VariantType *>(switch_statement->switcher->type.get());
    assert(variant_type != nullptr);
    llvm::StructType *variant_struct_type;
    if (variant_type->is_err_variant) {
        variant_struct_type = type_map.at("__flint_type_err");
    } else {
        variant_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_statement->switcher->type, false);
    }
    if (switch_value->getType()->isPointerTy()) {
        switch_value = builder.CreateLoad(variant_struct_type, switch_value, "loaded_rhs");
    }
    llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);

    for (size_t i = 0; i < switch_statement->branches.size(); i++) {
        const auto &branch = switch_statement->branches[i];
        // Check if it's the default branch, if it is this is the last branch to generate
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
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

        const SwitchMatchNode *match_node = dynamic_cast<const SwitchMatchNode *>(branch.matches.front().get());
        assert(match_node != nullptr);

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
        if (!generate_end_of_scope(builder, ctx)) {
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
        const SwitchMatchNode *match_node = dynamic_cast<const SwitchMatchNode *>(branch.matches.front().get());
        assert(match_node != nullptr);
        if (variant_type->is_err_variant) {
            const ErrorSetType *err_set_type = dynamic_cast<const ErrorSetType *>(match_node->type.get());
            assert(err_set_type != nullptr);
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
    if (dynamic_cast<const OptionalType *>(switch_statement->switcher->type.get())) {
        return generate_optional_switch_statement(builder, ctx, switch_statement, switch_value);
    }
    if (dynamic_cast<const VariantType *>(switch_statement->switcher->type.get())) {
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
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
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
        if (!generate_end_of_scope(builder, ctx)) {
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
    if (dynamic_cast<const ErrorSetType *>(switch_statement->switcher->type.get())) {
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
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
            continue;
        }

        // Generate the case values
        for (auto &match : branch.matches) {
            if (const LiteralNode *literal_node = dynamic_cast<const LiteralNode *>(match.get())) {
                if (std::holds_alternative<LitError>(literal_node->value)) {
                    const LitError &lit_err = std::get<LitError>(literal_node->value);
                    const ErrorSetType *error_type = dynamic_cast<const ErrorSetType *>(lit_err.error_type.get());
                    assert(error_type != nullptr);
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
    llvm::LoadInst *err_val = builder.CreateLoad(                                    //
        llvm::Type::getInt32Ty(context),                                             //
        err_var,                                                                     //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_err" //
    );
    err_val->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Load err val of call '" + call_node->function_name + "::" + std::to_string(call_node->call_id) + "'")));

    llvm::BasicBlock *last_block = &ctx.parent->back();
    llvm::BasicBlock *first_block = &ctx.parent->front();
    // Create basic block for the catch block
    llvm::BasicBlock *current_block = builder.GetInsertBlock();

    // Check if the current block is the last block, if it is not, insert right after the current block
    bool will_insert_after = current_block == last_block || current_block != first_block;
    llvm::BasicBlock *insert_before = will_insert_after ? (current_block->getNextNode()) : current_block;

    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(                           //
        context,                                                                        //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_catch", //
        ctx.parent,                                                                     //
        insert_before                                                                   //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(                           //
        context,                                                                        //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_merge", //
        nullptr,                                                                        //
        insert_before                                                                   //
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
                    "Branch to '" + catch_block->getName().str() + "' if '" + call_node->function_name + "' returned error")));

    // Add the error variable to the list of allocations (temporarily)
    // TODO: Add support for catch blocks which do not define a catch variable name
    const std::string err_alloca_name = "s" + std::to_string(catch_node->scope->scope_id) + "::" + catch_node->var_name.value();
    ctx.allocations.insert({err_alloca_name, ctx.allocations.at(err_ret_name)});

    // Generate the body of the catch block
    builder.SetInsertPoint(catch_block);
    const std::shared_ptr<Scope> current_scope = ctx.scope;
    ctx.scope = catch_node->scope;
    if (!generate_body(builder, ctx)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
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
        builder.CreateStore(elem_value, variable_alloca);
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
        auto expr_val = Expression::generate_expression(builder, ctx, garbage, 0, declaration_node->initializer.value().get());
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
        const auto *typecast_node = dynamic_cast<const TypeCastNode *>(declaration_node->initializer.value().get());
        const auto *tuple_type = dynamic_cast<const TupleType *>(declaration_node->type.get());
        if (tuple_type != nullptr) {
            // If the rhs is a InitializerNode, it returns all element values from the initializer expression
            // First, get the struct type of the data
            llvm::Type *data_type = IR::get_type(ctx.parent->getParent(), declaration_node->type).first;
            std::vector<std::shared_ptr<Type>> types = tuple_type->types;
            for (size_t i = 0; i < expr_val.value().size(); i++) {
                llvm::Value *elem_ptr = builder.CreateStructGEP(              //
                    data_type,                                                //
                    alloca,                                                   //
                    static_cast<unsigned int>(i),                             //
                    declaration_node->name + "_" + std::to_string(i) + "_ptr" //
                );
                // If the data field is a complex field we need to allocate memory for it, then store the "real" result in the allocated
                // memory region and then store the pointer to the allocated memory region in the 'elem_ptr' GEP
                const std::shared_ptr<Type> &elem_type = types[i];
                const DataType *field_data_type = dynamic_cast<const DataType *>(elem_type.get());
                const ArrayType *field_array_type = dynamic_cast<const ArrayType *>(elem_type.get());
                const bool field_is_complex = field_data_type != nullptr || field_array_type != nullptr;
                if (field_is_complex) {
                    // For complex types, allocate memory and store a pointer
                    llvm::Type *field_type = IR::get_type(ctx.parent->getParent(), elem_type).first;
                    const llvm::DataLayout &data_layout = ctx.parent->getParent()->getDataLayout();
                    llvm::Value *field_size = builder.getInt64(data_layout.getTypeAllocSize(field_type));
                    llvm::Value *field_alloca = builder.CreateCall(                                                      //
                        c_functions.at(MALLOC), {field_size}, declaration_node->name + "_" + std::to_string(i) + "_data" //
                    );

                    // Store the field value in the allocated memory
                    llvm::StoreInst *value_store = builder.CreateStore(expr_val.value().at(i), field_alloca);
                    value_store->setMetadata("comment",
                        llvm::MDNode::get(context,
                            llvm::MDString::get(context,
                                "Store complex data for '" + declaration_node->name + "_" + std::to_string(i) + "'")));

                    // Store the pointer to the complex data in the parent structure
                    llvm::StoreInst *ptr_store = builder.CreateStore(field_alloca, elem_ptr);
                    ptr_store->setMetadata("comment",
                        llvm::MDNode::get(context,
                            llvm::MDString::get(context,
                                "Store pointer to complex data '" + declaration_node->name + "_" + std::to_string(i) + "'")));
                } else {
                    // For primitive types, store directly
                    llvm::StoreInst *store = builder.CreateStore(expr_val.value().at(i), elem_ptr);
                    store->setMetadata("comment",
                        llvm::MDNode::get(context,
                            llvm::MDString::get(context,
                                "Store the actual val of '" + declaration_node->name + "_" + std::to_string(i) + "'")));
                }
            }
            return true;
        } else if (dynamic_cast<const OptionalType *>(declaration_node->type.get()) != nullptr && //
            (typecast_node == nullptr || typecast_node->expr->type->to_string() != "void?")       //
        ) {
            // We do not execute this branch if the rhs is a 'none' literal, as this would cause problems (zero-initializer of T? being
            // stored on the 'value' property of the optional struct, leading to the byte next to the struct being overwritten, e.g. UB)
            // Furthermore, if the RHS already is the correct optional type we also do not execute this branch as this would also lead to a
            // double-store of the optional value. Luckily, we can detect whether the RHS is already a complete optional by just checking
            // whether the LLVM type of the expression's type matches our expected optional type
            llvm::StructType *var_type = IR::add_and_or_get_type(ctx.parent->getParent(), declaration_node->type, false);
            const bool types_match = expr_val.value().front()->getType() == var_type;
            if (!types_match) {
                // Get the pointer to the i1 element of the optional variable and set it to 1
                llvm::Value *var_has_value_ptr = builder.CreateStructGEP(var_type, alloca, 0, declaration_node->name + "_has_value_ptr");
                llvm::StoreInst *store = builder.CreateStore(builder.getInt1(1), var_has_value_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context, "Set 'has_value' property of optional '" + declaration_node->name + "' to 1")));
                llvm::Value *var_value_ptr = builder.CreateStructGEP(var_type, alloca, 1, declaration_node->name + "_value_ptr");
                store = builder.CreateStore(expr_val.value().front(), var_value_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context, "Store result of expr in var '" + declaration_node->name + "'")));
                return true;
            }
        } else if (const VariantType *var_type = dynamic_cast<const VariantType *>(declaration_node->type.get())) {
            // We first check of which type the rhs really is. If it's a typecast, then we know it's one of the "inner" variations of
            // the variant, if it's a variant directly then we can store the variant in the variable as is. This means we dont need to
            // do anything if the typecast is a nullptr
            if (typecast_node != nullptr) {
                // First, we need to get the ID of the type within the variant
                std::optional<unsigned int> index = var_type->get_idx_of_type(typecast_node->expr->type);
                if (!index.has_value()) {
                    // Rhs has wrong type
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                llvm::StructType *variant_type = IR::add_and_or_get_type(ctx.parent->getParent(), declaration_node->type, false);
                llvm::Value *flag_ptr = builder.CreateStructGEP(variant_type, alloca, 0, declaration_node->name + "_flag_ptr");
                llvm::StoreInst *store = builder.CreateStore(builder.getInt8(index.value()), flag_ptr);
                store->setMetadata("comment",
                    llvm::MDNode::get(context,
                        llvm::MDString::get(context,
                            "Set 'flag' property of variant '" + declaration_node->name + "' to '" + std::to_string(index.value()) +
                                "' for type '" + typecast_node->expr->type->to_string() + "'")));
                llvm::Value *value_ptr = builder.CreateStructGEP(variant_type, alloca, 1, declaration_node->name + "_value_ptr");
                store = builder.CreateStore(expr_val.value().front(), value_ptr);
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
    llvm::StoreInst *store = builder.CreateStore(expression, alloca);
    store->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Store the actual val of '" + declaration_node->name + "'")));
    return true;
}

bool Generator::Statement::generate_assignment(llvm::IRBuilder<> &builder, GenerationContext &ctx, const AssignmentNode *assignment_node) {
    Expression::garbage_type garbage;
    auto expr = Expression::generate_expression(builder, ctx, garbage, 0, assignment_node->expression.get());
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
    if (const GroupType *group_type = dynamic_cast<const GroupType *>(assignment_node->expression->type.get())) {
        if (const TupleType *tuple_type = dynamic_cast<const TupleType *>(assignment_node->type.get())) {
            if (group_type->types != tuple_type->types) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            llvm::StructType *tuple_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), assignment_node->type, false);
            for (size_t i = 0; i < tuple_type->types.size(); i++) {
                llvm::Value *element_ptr = builder.CreateStructGEP(tuple_struct_type, lhs, i, "tuple_elem_" + std::to_string(i));
                builder.CreateStore(expr.value()[i], element_ptr);
            }
            return true;
        } else {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    } else if (const OptionalType *optional_type = dynamic_cast<const OptionalType *>(variable_type.get())) {
        const TypeCastNode *rhs_cast = dynamic_cast<const TypeCastNode *>(assignment_node->expression.get());
        llvm::StructType *var_type = IR::add_and_or_get_type(ctx.parent->getParent(), variable_type, false);
        if (optional_type->base_type->to_string() == "str") {
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("str")).first;
            llvm::Value *var_value_ptr = builder.CreateStructGEP(var_type, lhs, 1, assignment_node->name + "value_ptr");
            llvm::Value *actual_str_ptr = builder.CreateLoad(str_type->getPointerTo(), var_value_ptr, "actual_str_ptr");
            builder.CreateCall(c_functions.at(FREE), {actual_str_ptr});
        }
        // We do not execute this branch if the rhs is a 'none' literal, as this would cause problems (zero-initializer of T? being
        // stored on the 'value' property of the optional struct, leading to the byte next to the struct being overwritten, e.g. UB)
        // Furthermore, if the RHS already is the correct optional type we also do not execute this branch as this would also lead to a
        // double-store of the optional value. Luckily, we can detect whether the RHS is already a complete optional by just checking
        // whether the LLVM type of the expression's type matches our expected optional type
        const bool types_match = expr.value().front()->getType() == var_type;
        if (!types_match && (rhs_cast == nullptr || rhs_cast->expr->type->to_string() != "void?")) {
            // Get the pointer to the i1 element of the optional variable and set it to 1
            llvm::Value *var_has_value_ptr = builder.CreateStructGEP(var_type, lhs, 0, assignment_node->name + "_has_value_ptr");
            llvm::StoreInst *store = builder.CreateStore(builder.getInt1(1), var_has_value_ptr);
            store->setMetadata("comment",
                llvm::MDNode::get(context,
                    llvm::MDString::get(context, "Set 'has_value' property of optional '" + assignment_node->name + "' to 1")));
            llvm::Value *var_value_ptr = builder.CreateStructGEP(var_type, lhs, 1, assignment_node->name + "value_ptr");
            store = builder.CreateStore(expr.value().front(), var_value_ptr);
            store->setMetadata("comment",
                llvm::MDNode::get(context, llvm::MDString::get(context, "Store result of expr in var '" + assignment_node->name + "'")));
            return true;
        }
    } else if (const VariantType *var_type = dynamic_cast<const VariantType *>(assignment_node->type.get())) {
        // We first check of which type the rhs really is. If it's a typecast, then we know it's one of the "inner" variations of the
        // variant, if it's a variant directly then we can store the variant in the variable as is. This means we dont need to do
        // anything if the typecast is a nullptr
        const TypeCastNode *typecast_node = dynamic_cast<const TypeCastNode *>(assignment_node->expression.get());
        if (typecast_node != nullptr) {
            // First, we need to get the ID of the type within the variant
            std::optional<unsigned int> index = var_type->get_idx_of_type(typecast_node->expr->type);
            if (!index.has_value()) {
                // Rhs has wrong type
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            llvm::StructType *variant_type = IR::add_and_or_get_type(ctx.parent->getParent(), assignment_node->type, false);
            llvm::Value *flag_ptr = builder.CreateStructGEP(variant_type, lhs, 0, assignment_node->name + "_flag_ptr");
            llvm::StoreInst *store = builder.CreateStore(builder.getInt8(index.value()), flag_ptr);
            store->setMetadata("comment",
                llvm::MDNode::get(context,
                    llvm::MDString::get(context,
                        "Set 'flag' property of variant '" + assignment_node->name + "' to '" + std::to_string(index.value()) +
                            "' for type '" + typecast_node->expr->type->to_string() + "'")));
            llvm::Value *value_ptr = builder.CreateStructGEP(variant_type, lhs, 1, assignment_node->name + "_value_ptr");
            store = builder.CreateStore(expr.value().front(), value_ptr);
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
        llvm::StoreInst *store = builder.CreateStore(expression, lhs);
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
        builder.CreateStore(elem_value, alloca);
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
    group_mapping expression = Expression::generate_expression(builder, ctx, garbage, 0, data_field_assignment->expression.get());
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
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(data_field_assignment->var_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + data_field_assignment->var_name;
    llvm::Value *const var_alloca = ctx.allocations.at(var_name);

    if (data_field_assignment->data_type->to_string() == "bool8") {
        // The 'field access' is actually the bit at the given field index
        // Load the current value of the bool8 (i8)
        llvm::Value *current_value = builder.CreateLoad(builder.getInt8Ty(), var_alloca, var_name + "_val");
        // Get the boolean value from the expression
        llvm::Value *bool_value = expression.value().front();
        // Set or clear the specific bit based on the bool value
        unsigned int bit_index = data_field_assignment->field_id;
        // Get the new value of the bool8 value
        llvm::Value *new_value = Expression::set_bool8_element_at(builder, current_value, bool_value, bit_index);
        // Store the new value back
        llvm::StoreInst *store = builder.CreateStore(new_value, var_alloca);
        store->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Store result of expr in field '" + data_field_assignment->var_name + ".$" +
                        std::to_string(data_field_assignment->field_id) + "'")));
        return true;
    }

    llvm::Type *data_type = IR::get_type(ctx.parent->getParent(), data_field_assignment->data_type).first;
    llvm::Value *field_ptr = builder.CreateStructGEP(data_type, var_alloca, data_field_assignment->field_id);
    llvm::StoreInst *store = builder.CreateStore(expression.value().at(0), field_ptr);
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
        llvm::Value *current_value = builder.CreateLoad(builder.getInt8Ty(), var_alloca, var_name + "_val");
        llvm::Value *new_value = current_value;

        // Process each field in the grouped assignment
        for (size_t i = 0; i < grouped_field_assignment->field_ids.size(); i++) {
            unsigned int bit_index = grouped_field_assignment->field_ids.at(i);
            llvm::Value *bool_value = expression.value().at(i);
            new_value = Expression::set_bool8_element_at(builder, new_value, bool_value, bit_index);
        }

        // Store the final value back
        llvm::StoreInst *store = builder.CreateStore(new_value, var_alloca);

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

    llvm::Type *data_type = IR::get_type(ctx.parent->getParent(), grouped_field_assignment->data_type).first;
    for (size_t i = 0; i < expression.value().size(); i++) {
        llvm::Value *field_ptr = builder.CreateStructGEP(data_type, var_alloca, grouped_field_assignment->field_ids.at(i));
        llvm::StoreInst *store = builder.CreateStore(expression.value().at(i), field_ptr);
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
    if (array_assignment->expression->type != array_assignment->value_type) {
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
        builder.CreateStore(idx_expressions[i], idx_ptr);
    }
    // Get the array value
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(array_assignment->variable_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + array_assignment->variable_name;
    llvm::Value *const array_alloca = ctx.allocations.at(var_name);
    llvm::Type *arr_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first->getPointerTo();
    llvm::Value *array_ptr = builder.CreateLoad(arr_type, array_alloca, "array_ptr");
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
    // Store the main expression result in an 8 byte container
    llvm::Type *to_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("i64")).first;
    const unsigned int expr_bitwidth = expression->getType()->getPrimitiveSizeInBits();
    expression = IR::generate_bitwidth_change(builder, expression, expr_bitwidth, 64, to_type);
    // Call the `assign_at_val` function
    builder.CreateCall(                                                       //
        Module::Array::array_manip_functions.at("assign_arr_val_at"),         //
        {array_ptr, builder.getInt64(expr_bitwidth / 8), indices, expression} //
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
    if (stacked_assignment->expression->type != stacked_assignment->field_type) {
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
    builder.CreateStore(expression, field_ptr);
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
    const GroupType *expr_type = dynamic_cast<const GroupType *>(stacked_assignment->expression->type.get());
    if (expr_type == nullptr) {
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
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
    llvm::Type *base_type = IR::get_type(ctx.parent->getParent(), stacked_assignment->base_expression->type).first;
    for (size_t i = 0; i < expression_result.value().size(); i++) {
        llvm::Value *expression = expression_result.value().at(i);

        if (expr_type->types.at(i) != stacked_assignment->field_types.at(i)) {
            expression = Expression::generate_type_cast(                                                //
                builder, ctx, expression, expr_type->types.at(i), stacked_assignment->field_types.at(i) //
            );
        }
        if (stacked_assignment->field_types.at(i)->to_string() == "bool8") {
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
        llvm::Value *field_ptr = builder.CreateStructGEP(base_type, base_expr, stacked_assignment->field_ids.at(i), "field_ptr");
        builder.CreateStore(expression, field_ptr);
    }
    return true;
}

bool Generator::Statement::generate_unary_op_statement( //
    llvm::IRBuilder<> &builder,                         //
    GenerationContext &ctx,                             //
    const UnaryOpStatement *unary_op                    //
) {
    const VariableNode *var_node = dynamic_cast<const VariableNode *>(unary_op->operand.get());
    if (var_node == nullptr) {
        // Expression is not a variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    const unsigned int scope_id = std::get<1>(ctx.scope->variables.at(var_node->name));
    const std::string var_name = "s" + std::to_string(scope_id) + "::" + var_node->name;
    llvm::Value *const alloca = ctx.allocations.at(var_name);

    llvm::LoadInst *var_value = builder.CreateLoad(                  //
        IR::get_type(ctx.parent->getParent(), var_node->type).first, //
        alloca,                                                      //
        var_node->name + "_val"                                      //
    );
    var_value->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Load val of var '" + var_node->name + "'")));
    llvm::Value *operation_result = nullptr;

    if (dynamic_cast<const GroupType *>(var_node->type.get())) {
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
    llvm::StoreInst *operation_store = builder.CreateStore(operation_result, alloca);
    operation_store->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Store result of unary operation on '" + var_node->name + "'")));
    return true;
}
