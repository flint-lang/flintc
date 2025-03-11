#include "generator/generator.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/return_node.hpp"
#include <string>

void Generator::Allocation::generate_allocations(                         //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const Scope *scope,                                                   //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    for (const auto &statement_node : scope->body) {
        if (auto *call_node = dynamic_cast<CallNodeStatement *>(statement_node.get())) {
            generate_call_allocations(builder, parent, scope, allocations, dynamic_cast<CallNodeBase *>(call_node));
        } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement_node.get())) {
            generate_allocations(builder, parent, while_node->scope.get(), allocations);
        } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement_node.get())) {
            while (if_node != nullptr) {
                generate_allocations(builder, parent, if_node->then_scope.get(), allocations);
                if (if_node->else_scope.has_value()) {
                    if (std::holds_alternative<std::unique_ptr<IfNode>>(if_node->else_scope.value())) {
                        if_node = std::get<std::unique_ptr<IfNode>>(if_node->else_scope.value()).get();
                    } else {
                        Scope *else_scope = std::get<std::unique_ptr<Scope>>(if_node->else_scope.value()).get();
                        generate_allocations(builder, parent, else_scope, allocations);
                        if_node = nullptr;
                    }
                } else {
                    if_node = nullptr;
                }
            }
        } else if (const auto *for_loop_node = dynamic_cast<const ForLoopNode *>(statement_node.get())) {
            generate_allocations(builder, parent, for_loop_node->definition_scope.get(), allocations);
            generate_allocations(builder, parent, for_loop_node->body.get(), allocations);
        } else if (const auto *declaration_node = dynamic_cast<const DeclarationNode *>(statement_node.get())) {
            CallNodeExpression *call_node_expr = nullptr;
            if (declaration_node->initializer.has_value()) {
                call_node_expr = dynamic_cast<CallNodeExpression *>(declaration_node->initializer.value().get());
            }
            if (call_node_expr != nullptr) {
                generate_call_allocations(builder, parent, scope, allocations, call_node_expr);

                // Create the actual variable allocation with the declared type
                const std::string var_alloca_name = "s" + std::to_string(scope->scope_id) + "::" + declaration_node->name;
                generate_allocation(builder, allocations, var_alloca_name,               //
                    IR::get_type_from_str(parent->getContext(), declaration_node->type), //
                    declaration_node->name + "__VAL_1",                                  //
                    "Create alloc of 1st ret var '" + var_alloca_name + "'"              //
                );
            } else {
                // A "normal" allocation
                const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::" + declaration_node->name;
                generate_allocation(builder, allocations, alloca_name,                   //
                    IR::get_type_from_str(parent->getContext(), declaration_node->type), //
                    declaration_node->name + "__VAR",                                    //
                    "Create alloc of var '" + alloca_name + "'"                          //
                );
            }
        } else if (const auto *group_declaration_node = dynamic_cast<const GroupDeclarationNode *>(statement_node.get())) {
            if (auto *call_node_expr = dynamic_cast<CallNodeExpression *>(group_declaration_node->initializer.get())) {
                generate_call_allocations(builder, parent, scope, allocations, call_node_expr);
            } else if (auto *group_expr = dynamic_cast<GroupExpressionNode *>(group_declaration_node->initializer.get())) {
                const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::g" + std::to_string(group_expr->group_id);
                std::vector<std::string> types;
                for (const auto &expr : group_expr->expressions) {
                    types.emplace_back(expr->type);
                }
                generate_allocation(builder, allocations, alloca_name,               //
                    IR::add_and_or_get_type(&builder.getContext(), types, false),    //
                    "group_" + std::to_string(group_expr->group_id),                 //
                    "Create alloc for group " + std::to_string(group_expr->group_id) //
                );
            }

            // Allocating the actual variable values from the LHS
            for (const auto &variable : group_declaration_node->variables) {
                const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::" + variable.second;
                generate_allocation(builder, allocations, alloca_name,           //
                    IR::get_type_from_str(parent->getContext(), variable.first), //
                    variable.second + "__VAR",                                   //
                    "Create alloc of var '" + alloca_name + "'"                  //
                );
            }

        } else if (const auto *assignment_node = dynamic_cast<const AssignmentNode *>(statement_node.get())) {
            if (auto *call_node_expr = dynamic_cast<CallNodeExpression *>(assignment_node->expression.get())) {
                // Generate only the call allocations, not the variable allocations
                generate_call_allocations(builder, parent, scope, allocations, call_node_expr);
            }
        } else if (const auto *group_assignment_node = dynamic_cast<const GroupAssignmentNode *>(statement_node.get())) {
            if (auto *call_node_expr = dynamic_cast<CallNodeExpression *>(group_assignment_node->expression.get())) {
                generate_call_allocations(builder, parent, scope, allocations, call_node_expr);
            } else if (auto *group_expr = dynamic_cast<GroupExpressionNode *>(group_assignment_node->expression.get())) {
                const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::g" + std::to_string(group_expr->group_id);
                std::vector<std::string> types;
                for (const auto &expr : group_expr->expressions) {
                    types.emplace_back(expr->type);
                }
                generate_allocation(builder, allocations, alloca_name,               //
                    IR::add_and_or_get_type(&builder.getContext(), types, false),    //
                    "group_" + std::to_string(group_expr->group_id),                 //
                    "Create alloc for group " + std::to_string(group_expr->group_id) //
                );
            }
        } else if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement_node.get())) {
            if (auto *call_node_expr = dynamic_cast<CallNodeExpression *>(return_node->return_value.get())) {
                generate_call_allocations(builder, parent, scope, allocations, call_node_expr);
            } else if (auto *group_expr = dynamic_cast<GroupExpressionNode *>(return_node->return_value.get())) {
                const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::g" + std::to_string(group_expr->group_id);
                std::vector<std::string> types;
                for (const auto &expr : group_expr->expressions) {
                    types.emplace_back(expr->type);
                }
                generate_allocation(builder, allocations, alloca_name,               //
                    IR::add_and_or_get_type(&builder.getContext(), types, false),    //
                    "group_" + std::to_string(group_expr->group_id),                 //
                    "Create alloc for group " + std::to_string(group_expr->group_id) //
                );
            }
        }
    }
}

void Generator::Allocation::generate_call_allocations(                     //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    CallNodeBase *call_node                                                //
) {
    // Get the function definition from any module
    auto [func_decl_res, is_call_extern] = Function::get_function_definition(parent, call_node);
    if (!func_decl_res.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return;
    }
    llvm::Function *func_decl = func_decl_res.value();
    if (func_decl == nullptr) {
        // Builtin function call with void return
        return;
    }
    // Set the scope the call happens in
    call_node->scope_id = scope->scope_id;

    // Temporary allocation for the entire return struct
    const std::string ret_alloca_name = "s" + std::to_string(scope->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
    generate_allocation(builder, allocations, ret_alloca_name,                                                               //
        func_decl->getReturnType(),                                                                                          //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "__RET",                                       //
        "Create alloc of struct for called function '" + call_node->function_name + "', called by '" + ret_alloca_name + "'" //
    );

    // Create the error return valua allocation
    const std::string err_alloca_name = "s" + std::to_string(scope->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    generate_allocation(builder, allocations, err_alloca_name,                         //
        llvm::Type::getInt32Ty(parent->getContext()),                                  //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "__ERR", //
        "Create alloc of err ret var '" + err_alloca_name + "'"                        //
    );
}

void Generator::Allocation::generate_allocation(                           //
    llvm::IRBuilder<> &builder,                                            //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const std::string &alloca_name,                                        //
    llvm::Type *type,                                                      //
    const std::string &ir_name,                                            //
    const std::string &ir_comment                                          //
) {
    llvm::AllocaInst *alloca = builder.CreateAlloca( //
        type,                                        //
        nullptr,                                     //
        ir_name                                      //
    );
    alloca->setMetadata("comment", llvm::MDNode::get(builder.getContext(), llvm::MDString::get(builder.getContext(), ir_comment)));
    if (allocations.find(alloca_name) != allocations.end()) {
        // Variable allocation was already made somewhere else
        THROW_BASIC_ERR(ERR_GENERATING);
    }
    allocations.insert({alloca_name, alloca});
}

llvm::AllocaInst *Generator::Allocation::generate_default_struct( //
    llvm::IRBuilder<> &builder,                                   //
    llvm::StructType *type,                                       //
    const std::string &name,                                      //
    bool ignore_first                                             //
) {
    llvm::AllocaInst *alloca = builder.CreateAlloca(type, nullptr, name);

    for (unsigned int i = (ignore_first ? 1 : 0); i < type->getNumElements(); ++i) {
        llvm::Type *field_type = type->getElementType(i);
        llvm::Value *default_value = IR::get_default_value_of_type(field_type);
        llvm::Value *field_ptr = builder.CreateStructGEP(type, alloca, i);
        builder.CreateStore(default_value, field_ptr);
    }

    return alloca;
}
