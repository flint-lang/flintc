#include "generator/generator.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer_utils.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/parser.hpp"

#include <string>

bool Generator::Allocation::generate_allocations(                                   //
    llvm::IRBuilder<> &builder,                                                     //
    llvm::Function *parent,                                                         //
    const std::shared_ptr<Scope> scope,                                             //
    std::unordered_map<std::string, llvm::Value *const> &allocations,               //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
) {
    for (const auto &statement_node : scope->body) {
        if (auto *call_node = dynamic_cast<CallNodeStatement *>(statement_node.get())) {
            if (!generate_call_allocations(builder, parent, scope, allocations,     //
                    imported_core_modules, dynamic_cast<CallNodeBase *>(call_node)) //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement_node.get())) {
            if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, while_node->condition.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            if (!generate_allocations(builder, parent, while_node->scope, allocations, imported_core_modules)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement_node.get())) {
            if (!generate_if_allocations(builder, parent, allocations, imported_core_modules, if_node)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *for_loop_node = dynamic_cast<const ForLoopNode *>(statement_node.get())) {
            if (!generate_expression_allocations(                                       //
                    builder, parent, for_loop_node->definition_scope,                   //
                    allocations, imported_core_modules, for_loop_node->condition.get()) //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            if (!generate_allocations(builder, parent, for_loop_node->definition_scope, allocations, imported_core_modules)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            if (!generate_allocations(builder, parent, for_loop_node->body, allocations, imported_core_modules)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *enh_for_loop_node = dynamic_cast<const EnhForLoopNode *>(statement_node.get())) {
            if (!generate_enh_for_allocations(builder, parent, allocations, imported_core_modules, enh_for_loop_node)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *declaration_node = dynamic_cast<const DeclarationNode *>(statement_node.get())) {
            if (!generate_declaration_allocations(builder, parent, scope, allocations, imported_core_modules, declaration_node)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *group_declaration_node = dynamic_cast<const GroupDeclarationNode *>(statement_node.get())) {
            if (!generate_group_declaration_allocations(builder, parent, scope, allocations, //
                    imported_core_modules, group_declaration_node)                           //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *assignment_node = dynamic_cast<const AssignmentNode *>(statement_node.get())) {
            if (!generate_expression_allocations(builder, parent, scope, allocations, //
                    imported_core_modules, assignment_node->expression.get())         //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *group_assignment_node = dynamic_cast<const GroupAssignmentNode *>(statement_node.get())) {
            if (!generate_expression_allocations(                                                                        //
                    builder, parent, scope, allocations, imported_core_modules, group_assignment_node->expression.get()) //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement_node.get())) {
            if (return_node->return_value.has_value()) {
                if (!generate_expression_allocations(                                                                        //
                        builder, parent, scope, allocations, imported_core_modules, return_node->return_value.value().get()) //
                ) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
            }
        } else if (const auto *catch_node = dynamic_cast<const CatchNode *>(statement_node.get())) {
            if (!generate_allocations(builder, parent, catch_node->scope, allocations, imported_core_modules)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        } else if (const auto *array_assignment = dynamic_cast<const ArrayAssignmentNode *>(statement_node.get())) {
            generate_array_indexing_allocation(builder, allocations, array_assignment->indexing_expressions.size());
        } else if (const auto *switch_statement = dynamic_cast<const SwitchStatement *>(statement_node.get())) {
            if (!generate_switch_statement_allocations(builder, parent, scope, allocations, imported_core_modules, switch_statement)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
    }
    return true;
}

void Generator::Allocation::generate_function_allocations(            //
    llvm::IRBuilder<> &builder,                                       //
    llvm::Function *parent,                                           //
    std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const FunctionNode *function                                      //
) {
    assert(parent->arg_size() == function->parameters.size());
    unsigned int param_id = 0;
    for (auto &arg : parent->args()) {
        const auto &param = function->parameters.at(param_id);
        const std::string param_name = "s" + std::to_string(function->scope->scope_id) + "::" + std::get<1>(param);
        if (primitives.find(std::get<0>(param)->to_string()) == primitives.end()) {
            // Its not a primitive type, this means it must be passed by reference
            allocations.emplace(param_name, &arg);
        } else {
            // If its a primitive type, it either has to be set as the alloca inst directly, or we need to create a local alloca inst for
            // the argument, to make the argument mutable. We also need to store the value of the argument in this alloca inst
            if (std::get<2>(param)) {
                // Is mutable
                llvm::AllocaInst *argAlloca = builder.CreateAlloca(arg.getType(), nullptr, arg.getName() + ".addr");
                argAlloca->setAlignment(llvm::Align(calculate_type_alignment(arg.getType())));

                // Store the initial argument value into the alloca
                builder.CreateStore(&arg, argAlloca);

                // Register the alloca in allocations map
                allocations.emplace(param_name, argAlloca);
            } else {
                // Is immutable
                allocations.emplace(param_name, &arg);
            }
        }
        param_id++;
    }
}

bool Generator::Allocation::generate_call_allocations(                               //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    const std::shared_ptr<Scope> scope,                                              //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const CallNodeBase *call_node                                                    //
) {
    // First, generate the allocations of all the parameter expressions of the call node
    for (const auto &arg : call_node->arguments) {
        if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, arg.first.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    llvm::Type *function_return_type = nullptr;
    // Check if the call targets any builtin functions
    auto builtin_function = Parser::get_builtin_function(call_node->function_name, imported_core_modules);
    if (builtin_function.has_value()) {
        // We only need to create an allocation of the call if it can return an error
        auto &overload = std::get<1>(builtin_function.value()).front();
        if (std::get<1>(builtin_function.value()).size() > 1) {
            // Go through the overloads and check if we find a match, if we do then thats our target function
            bool found_overload = false;
            for (auto &ov : std::get<1>(builtin_function.value())) {
                auto &args = std::get<0>(ov);
                if (args.size() != call_node->arguments.size()) {
                    continue;
                }
                bool all_match = true;
                for (size_t i = 0; i < args.size(); i++) {
                    if (call_node->arguments[i].first->type->to_string() != args[i]) {
                        all_match = false;
                        break;
                    }
                }
                if (!all_match) {
                    continue;
                }
                overload = ov;
                found_overload = true;
                break;
            }
            if (!found_overload) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
        if (std::get<2>(overload)) {
            // Function returns error
            function_return_type = IR::add_and_or_get_type(parent->getParent(), call_node->type);
        } else {
            // Function does not return error
            return true;
        }
    } else {
        // Get the function definition from any module
        auto [func_decl_res, is_call_extern] = Function::get_function_definition(parent, call_node);
        if (!func_decl_res.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        function_return_type = func_decl_res.value()->getReturnType();
    }

    // Temporary allocation for the entire return struct
    const std::string ret_alloca_name = "s" + std::to_string(scope->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
    generate_allocation(builder, allocations, ret_alloca_name,                                                               //
        function_return_type,                                                                                                //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "__RET",                                       //
        "Create alloc of struct for called function '" + call_node->function_name + "', called by '" + ret_alloca_name + "'" //
    );

    // Create the error return valua allocation
    const std::string err_alloca_name = "s" + std::to_string(scope->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    generate_allocation(builder, allocations, err_alloca_name,                         //
        llvm::Type::getInt32Ty(context),                                               //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "__ERR", //
        "Create alloc of err ret var '" + err_alloca_name + "'"                        //
    );
    return true;
}

bool Generator::Allocation::generate_if_allocations(                                 //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const IfNode *if_node                                                            //
) {
    while (if_node != nullptr) {
        if (!generate_expression_allocations(                                                                                     //
                builder, parent, if_node->then_scope->parent_scope, allocations, imported_core_modules, if_node->condition.get()) //
        ) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (!generate_allocations(builder, parent, if_node->then_scope, allocations, imported_core_modules)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (if_node->else_scope.has_value()) {
            if (std::holds_alternative<std::unique_ptr<IfNode>>(if_node->else_scope.value())) {
                if_node = std::get<std::unique_ptr<IfNode>>(if_node->else_scope.value()).get();
            } else {
                std::shared_ptr<Scope> else_scope = std::get<std::shared_ptr<Scope>>(if_node->else_scope.value());
                if (!generate_allocations(builder, parent, else_scope, allocations, imported_core_modules)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                if_node = nullptr;
            }
        } else {
            if_node = nullptr;
        }
    }
    return true;
}

bool Generator::Allocation::generate_enh_for_allocations(                            //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const EnhForLoopNode *for_node                                                   //
) {
    if (!generate_expression_allocations(                                 //
            builder, parent, for_node->definition_scope,                  //
            allocations, imported_core_modules, for_node->iterable.get()) //
    ) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (std::holds_alternative<std::string>(for_node->iterators)) {
        // A single iterator tuple
        const std::string it_name = std::get<std::string>(for_node->iterators);
        const auto it_variable = for_node->definition_scope->variables.at(it_name);
        std::shared_ptr<Type> it_type_ptr = std::get<0>(it_variable);
        llvm::Type *it_type = IR::add_and_or_get_type(parent->getParent(), it_type_ptr);
        const unsigned int scope_id = for_node->definition_scope->scope_id;
        std::string alloca_name = "s" + std::to_string(scope_id) + "::" + it_name;
        generate_allocation(builder, allocations, alloca_name, it_type, it_name + "__ITER_TUPL",        //
            "Create iterator tuple '" + it_name + "' of enh for loop in s::" + std::to_string(scope_id) //
        );
    } else {
        // One index and one element iterator
        const auto iterators = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(for_node->iterators);
        const unsigned int scope_id = for_node->definition_scope->scope_id;
        if (iterators.first.has_value()) {
            const std::string index_name = iterators.first.value();
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::" + index_name;
            generate_allocation(builder, allocations, index_alloca_name, builder.getInt64Ty(), index_name + "__ITER_IDX", //
                "Create index iter alloca '" + index_name + "' of enh for loop in s::" + std::to_string(scope_id)         //
            );
        } else {
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::IDX";
            generate_allocation(builder, allocations, index_alloca_name, builder.getInt64Ty(), "__ITER_IDX", //
                "Create index iter alloca of enh for loop in s::" + std::to_string(scope_id)                 //
            );
        }
        if (iterators.second.has_value()) {
            const std::string element_name = iterators.second.value();
            const std::string element_alloca_name = "s" + std::to_string(scope_id) + "::" + element_name;
            // There is no allocation for the iterable, the allocation is actually a loaded pointer in the enh for loops body
            allocations.emplace(element_alloca_name, nullptr);
        }
    }
    if (!generate_allocations(builder, parent, for_node->body, allocations, imported_core_modules)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    return true;
}

bool Generator::Allocation::generate_switch_statement_allocations(                   //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    const std::shared_ptr<Scope> scope,                                              //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const SwitchStatement *switch_statement                                          //
) {
    if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, switch_statement->switcher.get())) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    for (const auto &branch : switch_statement->branches) {
        for (const auto &match : branch.matches) {
            if (!generate_expression_allocations(builder, parent, branch.body, allocations, imported_core_modules, match.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
        if (!generate_allocations(builder, parent, branch.body, allocations, imported_core_modules)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    return true;
}

bool Generator::Allocation::generate_switch_expression_allocations(                  //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    const std::shared_ptr<Scope> scope,                                              //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const SwitchExpression *switch_expression                                        //
) {
    if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, switch_expression->switcher.get())) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    for (const auto &branch : switch_expression->branches) {
        for (const auto &match : branch.matches) {
            if (!generate_expression_allocations(builder, parent, branch.scope, allocations, imported_core_modules, match.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
        if (!generate_expression_allocations(builder, parent, branch.scope, allocations, imported_core_modules, branch.expr.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    return true;
}

bool Generator::Allocation::generate_declaration_allocations(                        //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    const std::shared_ptr<Scope> scope,                                              //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const DeclarationNode *declaration_node                                          //
) {
    CallNodeExpression *call_node_expr = nullptr;
    if (declaration_node->initializer.has_value()) {
        call_node_expr = dynamic_cast<CallNodeExpression *>(declaration_node->initializer.value().get());
        if (call_node_expr == nullptr) {
            if (!generate_expression_allocations(                                                                            //
                    builder, parent, scope, allocations, imported_core_modules, declaration_node->initializer.value().get()) //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
    }
    if (call_node_expr != nullptr) {
        if (!generate_call_allocations(builder, parent, scope, allocations, imported_core_modules, call_node_expr)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }

        // Create the actual variable allocation with the declared type
        const std::string var_alloca_name = "s" + std::to_string(scope->scope_id) + "::" + declaration_node->name;
        generate_allocation(builder, allocations, var_alloca_name,           //
            IR::get_type(parent->getParent(), declaration_node->type).first, //
            declaration_node->name + "__VAL_1",                              //
            "Create alloc of 1st ret var '" + var_alloca_name + "'"          //
        );
    } else {
        // A "normal" allocation
        const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::" + declaration_node->name;
        generate_allocation(builder, allocations, alloca_name,               //
            IR::get_type(parent->getParent(), declaration_node->type).first, //
            declaration_node->name + "__VAR",                                //
            "Create alloc of var '" + alloca_name + "'"                      //
        );
    }
    return true;
}

bool Generator::Allocation::generate_group_declaration_allocations(                  //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    const std::shared_ptr<Scope> scope,                                              //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const GroupDeclarationNode *group_declaration_node                               //
) {
    if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, //
            group_declaration_node->initializer.get())                                               //
    ) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    // Allocating the actual variable values from the LHS
    for (const auto &variable : group_declaration_node->variables) {
        const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::" + variable.second;
        generate_allocation(builder, allocations, alloca_name,       //
            IR::get_type(parent->getParent(), variable.first).first, //
            variable.second + "__VAR",                               //
            "Create alloc of var '" + alloca_name + "'"              //
        );
    }
    return true;
}

void Generator::Allocation::generate_array_indexing_allocation(       //
    llvm::IRBuilder<> &builder,                                       //
    std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const size_t dimensionality                                       //
) {
    const std::string alloca_name = "arr::idx::" + std::to_string(dimensionality);
    if (allocations.find(alloca_name) != allocations.end()) {
        return;
    }
    // Create a i64 array with the length of the dimensions
    llvm::Type *length_array_type = llvm::ArrayType::get(builder.getInt64Ty(), dimensionality);
    generate_allocation(builder, allocations, alloca_name, length_array_type, "__arr_idx_" + std::to_string(dimensionality) + "d", //
        "Shared " + std::to_string(dimensionality) + "D indexing array"                                                            //
    );
}

bool Generator::Allocation::generate_expression_allocations(                         //
    llvm::IRBuilder<> &builder,                                                      //
    llvm::Function *parent,                                                          //
    const std::shared_ptr<Scope> scope,                                              //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
    const ExpressionNode *expression                                                 //
) {
    if (const auto *binary_op = dynamic_cast<const BinaryOpNode *>(expression)) {
        if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, binary_op->left.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, binary_op->right.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    } else if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(expression)) {
        if (!generate_call_allocations(builder, parent, scope, allocations, imported_core_modules, //
                dynamic_cast<const CallNodeBase *>(call_node))                                     //
        ) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    } else if (const auto *group_expression = dynamic_cast<const GroupExpressionNode *>(expression)) {
        for (const auto &expr : group_expression->expressions) {
            if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
    } else if (const auto *type_cast = dynamic_cast<const TypeCastNode *>(expression)) {
        if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, type_cast->expr.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    } else if (const auto *unary_op = dynamic_cast<const UnaryOpExpression *>(expression)) {
        if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, unary_op->operand.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    } else if (const auto *interpol = dynamic_cast<const StringInterpolationNode *>(expression)) {
        for (const auto &val : interpol->string_content) {
            if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(val)) {
                const ExpressionNode *expr = std::get<std::unique_ptr<ExpressionNode>>(val).get();
                if (!generate_expression_allocations(builder, parent, scope, allocations, imported_core_modules, expr)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
            }
        }
    } else if (const auto *array_initializer = dynamic_cast<const ArrayInitializerNode *>(expression)) {
        generate_array_indexing_allocation(builder, allocations, array_initializer->length_expressions.size());
    } else if (const auto *array_access = dynamic_cast<const ArrayAccessNode *>(expression)) {
        generate_array_indexing_allocation(builder, allocations, array_access->indexing_expressions.size());
    } else if (const auto *switch_expression = dynamic_cast<const SwitchExpression *>(expression)) {
        if (!generate_switch_expression_allocations(builder, parent, scope, allocations, imported_core_modules, switch_expression)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    return true;
}

void Generator::Allocation::generate_allocation(                      //
    llvm::IRBuilder<> &builder,                                       //
    std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const std::string &alloca_name,                                   //
    llvm::Type *type,                                                 //
    const std::string &ir_name,                                       //
    const std::string &ir_comment                                     //
) {
    llvm::AllocaInst *alloca = builder.CreateAlloca( //
        type,                                        //
        nullptr,                                     //
        ir_name                                      //
    );
    alloca->setAlignment(llvm::Align(calculate_type_alignment(type)));

    alloca->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, ir_comment)));
    if (allocations.find(alloca_name) != allocations.end()) {
        // Variable allocation was already made somewhere else
        THROW_BASIC_ERR(ERR_GENERATING);
    }
    allocations.insert({alloca_name, alloca});
}

unsigned int Generator::Allocation::calculate_type_alignment(llvm::Type *type) {
    // Default alignment
    unsigned int alignment = 8;

    // Check if the type is a vector type
    if (type->isVectorTy()) {
        // For vector types, use alignment of 16 bytes (or vector size if larger)
        unsigned int prim_size = type->getPrimitiveSizeInBits() / 8;
        alignment = prim_size > 16 ? prim_size : 16;
    }
    // Check if the type is a struct type
    else if (type->isStructTy()) {
        llvm::StructType *structType = llvm::cast<llvm::StructType>(type);

        // For struct types, calculate the maximum alignment needed by any of its elements
        for (unsigned i = 0; i < structType->getNumElements(); i++) {
            llvm::Type *elemType = structType->getElementType(i);
            alignment = std::max(alignment, calculate_type_alignment(elemType));
        }
    }
    // For array types, check the element type
    else if (type->isArrayTy()) {
        llvm::Type *elemType = type->getArrayElementType();
        alignment = calculate_type_alignment(elemType);
    }

    return alignment;
}

size_t Generator::Allocation::get_type_size(llvm::Module *module, llvm::Type *type) {
    const llvm::DataLayout &data_layout = module->getDataLayout();
    return data_layout.getTypeAllocSize(type);
}

llvm::AllocaInst *Generator::Allocation::generate_default_struct( //
    llvm::IRBuilder<> &builder,                                   //
    llvm::StructType *type,                                       //
    const std::string &name,                                      //
    bool ignore_first                                             //
) {
    llvm::AllocaInst *alloca = builder.CreateAlloca(type, nullptr, name);
    alloca->setAlignment(llvm::Align(calculate_type_alignment(type)));

    for (unsigned int i = (ignore_first ? 1 : 0); i < type->getNumElements(); ++i) {
        llvm::Type *field_type = type->getElementType(i);
        llvm::Value *default_value = IR::get_default_value_of_type(field_type);
        llvm::Value *field_ptr = builder.CreateStructGEP(type, alloca, i);
        builder.CreateStore(default_value, field_ptr);
    }

    return alloca;
}
