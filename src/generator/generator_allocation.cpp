#include "generator/generator.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/instance_call_node_expression.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/instance_call_node_statement.hpp"
#include "parser/ast/statements/statement_node.hpp"

#include <string>

std::optional<llvm::StructType *> Generator::Allocation::generate_function_allocations( //
    llvm::IRBuilder<> &builder,                                                         //
    llvm::Function *parent,                                                             //
    const FunctionNode *function,                                                       //
    std::unordered_map<std::string, llvm::Value *const> &allocations                    //
) {
    assert(function->scope.has_value() && !function->is_extern);
    std::vector<std::pair<std::string, llvm::Type *const>> types_list;

    // We start with all the return values of the function for the type list for the function frame
    llvm::StructType *const ts_fn_ty = type_map.at("type.ts.function");
    for (size_t ret_id = 0; ret_id < function->return_types.size(); ret_id++) {
        const auto &ret_type = function->return_types.at(ret_id);
        auto ret_ty = IR::get_type(parent->getParent(), ret_type);
        const std::string ret_name = "flint.ret." + std::to_string(ret_id);
        if (ret_ty.second.first) {
            types_list.emplace_back(ret_name, ret_ty.first->getPointerTo());
        } else {
            types_list.emplace_back(ret_name, ret_ty.first);
        }
    }

    // Then we move on to the function parameters for the allocation map
    for (size_t param_id = 0; param_id < function->parameters.size(); param_id++) {
        const auto &param = function->parameters.at(param_id);
        const std::string param_name = "s" + std::to_string(function->scope.value()->scope_id) + "::" + std::get<1>(param);
        auto param_type = IR::get_type(parent->getParent(), std::get<0>(function->parameters.at(param_id)));
        assert(param_type.first != nullptr);
        if (param_type.second.first) {
            types_list.emplace_back(param_name, param_type.first->getPointerTo());
        } else {
            types_list.emplace_back(param_name, param_type.first);
        }
    }

    std::string frame_type_name = function->file_hash.to_string() + ".type.ts." + function->name;
    if (!generate_allocations(builder, parent, function->scope.value(), types_list)) {
        return std::nullopt;
    }
    std::vector<llvm::Type *> type_list = {ts_fn_ty};
    for (const auto &[_, type] : types_list) {
        type_list.emplace_back(type);
    }

    for (size_t i = 0; i < function->parameters.size(); i++) {
        if (i == 0) {
            frame_type_name += ".";
        }
        if (i > 0) {
            frame_type_name += "_";
        }
        frame_type_name += std::get<0>(function->parameters.at(i))->to_string();
    }
    llvm::StructType *frame_type = IR::create_struct_type(frame_type_name, type_list);
    const size_t fn_id = function->get_id();

    // Add the frame type to the `ts_frames` map
    assert(Module::ThreadStack::ts_frames.find(fn_id) == Module::ThreadStack::ts_frames.end());
    Module::ThreadStack::ts_frames[fn_id] = frame_type;

    // Create the default frame of the function frame
    assert(Module::ThreadStack::ts_defaults.find(fn_id) == Module::ThreadStack::ts_defaults.end());
    llvm::Constant *ts_fn_default = llvm::ConstantStruct::get(ts_fn_ty,
        {
            llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
            builder.getInt64(fn_id),
            llvm::Constant::getNullValue(type_map.at("type.flint.err")),
        });
    std::vector<llvm::Constant *> frame_elems = {ts_fn_default};
    for (auto type_it = types_list.begin(); type_it != types_list.end(); ++type_it) {
        frame_elems.push_back(IR::get_default_value_of_type(type_it->second));
    }
    llvm::Constant *frame_default = llvm::ConstantStruct::get(frame_type, frame_elems);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
    Module::ThreadStack::ts_defaults[fn_id] = new llvm::GlobalVariable(                            //
        *parent->getParent(), frame_type, false, llvm::GlobalValue::WeakODRLinkage, frame_default, //
        function->file_hash.to_string() + ".default.ts." + function->name                          //
    );
#pragma GCC diagnostic pop

    // Finally add all the struct GEPs to the allocations map
    allocations.emplace("flint.stack", parent->arg_begin());
    for (auto type_it = types_list.begin(); type_it != types_list.end(); ++type_it) {
        const std::string &alloca_name = type_it->first;
        assert(allocations.find(alloca_name) == allocations.end());
        const size_t idx = std::distance(types_list.begin(), type_it);
        allocations.emplace(alloca_name, builder.CreateStructGEP(frame_type, parent->arg_begin(), idx + 1, alloca_name));
    }
    llvm::Value *next_stack_frame = builder.CreateGEP(frame_type, parent->arg_begin(), builder.getInt32(1), "next_stack_frame");
    allocations.emplace("flint.stack.next", next_stack_frame);
    return frame_type;
}

bool Generator::Allocation::generate_allocations(                        //
    llvm::IRBuilder<> &builder,                                          //
    llvm::Function *parent,                                              //
    const std::shared_ptr<Scope> scope,                                  //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types //
) {
    for (const auto &statement : scope->body) {
        switch (statement->get_variation()) {
            case StatementNode::Variation::ARRAY_ASSIGNMENT: {
                const auto *node = statement->as<ArrayAssignmentNode>();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                generate_array_indexing_allocation(builder, struct_types, node->indexing_expressions);
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->expression.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::ASSIGNMENT: {
                const auto *node = statement->as<AssignmentNode>();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->expression.get())) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::BREAK:
                break;
            case StatementNode::Variation::CALL: {
                const auto *node = statement->as<CallNodeStatement>();
                if (!generate_call_allocations(builder, parent, scope, struct_types, static_cast<const CallNodeBase *>(node))) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::CATCH: {
                const auto *node = statement->as<CatchNode>();
                if (!generate_allocations(builder, parent, node->scope, struct_types)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::CONTINUE:
                break;
            case StatementNode::Variation::DATA_FIELD_ASSIGNMENT: {
                const auto *node = statement->as<DataFieldAssignmentNode>();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->expression.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::DECLARATION: {
                const auto *node = statement->as<DeclarationNode>();
                if (!generate_declaration_allocations(builder, parent, scope, struct_types, node)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::DO_WHILE: {
                const auto *node = statement->as<DoWhileNode>();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->condition.get())) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                if (!generate_allocations(builder, parent, node->scope, struct_types)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::ENHANCED_FOR_LOOP: {
                const auto *node = statement->as<EnhForLoopNode>();
                if (!generate_enh_for_allocations(builder, parent, struct_types, node)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::FOR_LOOP: {
                const auto *node = statement->as<ForLoopNode>();
                if (!generate_expression_allocations(builder, parent, node->definition_scope, struct_types, node->condition.get()) //
                ) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                if (!generate_allocations(builder, parent, node->definition_scope, struct_types)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                if (!generate_allocations(builder, parent, node->body, struct_types)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::GROUP_ASSIGNMENT: {
                const auto *node = statement->as<GroupAssignmentNode>();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->expression.get())) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::GROUP_DECLARATION: {
                const auto *node = statement->as<GroupDeclarationNode>();
                if (!generate_group_declaration_allocations(builder, parent, scope, struct_types, node)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT: {
                const auto *node = statement->as<GroupedDataFieldAssignmentNode>();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->expression.get())) {
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::IF: {
                const auto *node = statement->as<IfNode>();
                if (!generate_if_allocations(builder, parent, struct_types, node)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::INSTANCE_CALL: {
                const auto *node = statement->as<InstanceCallNodeStatement>();
                if (!generate_call_allocations(builder, parent, scope, struct_types, static_cast<const CallNodeBase *>(node))) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::RETURN: {
                const auto *node = statement->as<ReturnNode>();
                if (!node->return_value.has_value()) {
                    continue;
                }
                if (!generate_expression_allocations(                                           //
                        builder, parent, scope, struct_types, node->return_value.value().get()) //
                ) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::SWITCH: {
                const auto *node = statement->as<SwitchStatement>();
                if (!generate_switch_statement_allocations(builder, parent, scope, struct_types, node)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
            case StatementNode::Variation::THROW:
                break;
            case StatementNode::Variation::UNARY_OP:
                break;
            case StatementNode::Variation::WHILE: {
                const auto *node = statement->as<WhileNode>();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, node->condition.get())) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                if (!generate_allocations(builder, parent, node->scope, struct_types)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
                break;
            }
        }
    }
    return true;
}

bool Generator::Allocation::generate_call_allocations(                    //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const std::shared_ptr<Scope> scope,                                   //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const CallNodeBase *call_node                                         //
) {
    // Generate the allocations of all the parameter expressions of the call node
    for (const auto &arg : call_node->arguments) {
        if (!generate_expression_allocations(builder, parent, scope, struct_types, arg.first.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    return true;
}

bool Generator::Allocation::generate_if_allocations(                      //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const IfNode *if_node                                                 //
) {
    while (if_node != nullptr) {
        if (!generate_expression_allocations(                                                               //
                builder, parent, if_node->then_scope->parent_scope, struct_types, if_node->condition.get()) //
        ) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (!generate_allocations(builder, parent, if_node->then_scope, struct_types)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
        if (if_node->else_scope.has_value()) {
            if (std::holds_alternative<std::unique_ptr<IfNode>>(if_node->else_scope.value())) {
                if_node = std::get<std::unique_ptr<IfNode>>(if_node->else_scope.value()).get();
            } else {
                std::shared_ptr<Scope> else_scope = std::get<std::shared_ptr<Scope>>(if_node->else_scope.value());
                if (!generate_allocations(builder, parent, else_scope, struct_types)) {
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

bool Generator::Allocation::generate_enh_for_allocations(                 //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const EnhForLoopNode *for_node                                        //
) {
    if (!generate_expression_allocations(builder, parent, for_node->definition_scope, struct_types, for_node->iterable.get())) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    if (std::holds_alternative<std::string>(for_node->iterators)) {
        const std::string it_name = std::get<std::string>(for_node->iterators);
        const auto it_variable = for_node->definition_scope->variables.at(it_name);
        llvm::Type *it_type = IR::get_type(parent->getParent(), it_variable.type).first;
        const unsigned int scope_id = for_node->definition_scope->scope_id;
        std::string alloca_name = "s" + std::to_string(scope_id) + "::" + it_name;
        struct_types.emplace_back(alloca_name, it_type);
    } else {
        const auto iterators = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(for_node->iterators);
        const unsigned int scope_id = for_node->definition_scope->scope_id;
        if (iterators.first.has_value()) {
            const std::string index_name = iterators.first.value();
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::" + index_name;
            struct_types.emplace_back(index_alloca_name, builder.getInt64Ty());
        } else {
            const std::string index_alloca_name = "s" + std::to_string(scope_id) + "::IDX";
            struct_types.emplace_back(index_alloca_name, builder.getInt64Ty());
        }
        if (iterators.second.has_value()) {
            const std::string element_name = iterators.second.value();
            const std::string element_alloca_name = "s" + std::to_string(scope_id) + "::" + element_name;
            if (for_node->iterable->type->get_variation() == Type::Variation::RANGE) {
                struct_types.emplace_back(element_alloca_name, builder.getInt64Ty());
            }
        }
    }
    if (!generate_allocations(builder, parent, for_node->body, struct_types)) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    return true;
}

bool Generator::Allocation::generate_switch_statement_allocations(        //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const std::shared_ptr<Scope> scope,                                   //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const SwitchStatement *switch_statement                               //
) {
    if (!generate_expression_allocations(builder, parent, scope, struct_types, switch_statement->switcher.get())) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    for (const auto &branch : switch_statement->branches) {
        for (const auto &match : branch.matches) {
            if (!generate_expression_allocations(builder, parent, branch.body, struct_types, match.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
        if (!generate_allocations(builder, parent, branch.body, struct_types)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    return true;
}

bool Generator::Allocation::generate_switch_expression_allocations(       //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const std::shared_ptr<Scope> scope,                                   //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const SwitchExpression *switch_expression                             //
) {
    if (!generate_expression_allocations(builder, parent, scope, struct_types, switch_expression->switcher.get())) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    for (const auto &branch : switch_expression->branches) {
        for (const auto &match : branch.matches) {
            if (!generate_expression_allocations(builder, parent, branch.scope, struct_types, match.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
        if (!generate_expression_allocations(builder, parent, branch.scope, struct_types, branch.expr.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    return true;
}

bool Generator::Allocation::generate_declaration_allocations(             //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const std::shared_ptr<Scope> scope,                                   //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const DeclarationNode *declaration_node                               //
) {
    CallNodeExpression *call_node_expr = nullptr;
    if (declaration_node->initializer.has_value()) {
        call_node_expr = dynamic_cast<CallNodeExpression *>(declaration_node->initializer.value().get());
        if (call_node_expr == nullptr) {
            if (!generate_expression_allocations(                                                      //
                    builder, parent, scope, struct_types, declaration_node->initializer.value().get()) //
            ) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
        }
    }
    if (call_node_expr != nullptr) {
        if (!generate_call_allocations(builder, parent, scope, struct_types, call_node_expr)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }

    const std::string var_name = "s" + std::to_string(scope->scope_id) + "::" + declaration_node->name;
    auto type = IR::get_type(parent->getParent(), declaration_node->type);
    struct_types.emplace_back(var_name, type.second.first ? type.first->getPointerTo() : type.first);

    return true;
}

bool Generator::Allocation::generate_group_declaration_allocations(       //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const std::shared_ptr<Scope> scope,                                   //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const GroupDeclarationNode *group_declaration_node                    //
) {
    if (!generate_expression_allocations(builder, parent, scope, struct_types, group_declaration_node->initializer.get())) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }

    for (const auto &variable : group_declaration_node->variables) {
        const std::string var_name = "s" + std::to_string(scope->scope_id) + "::" + variable.second;
        auto type = IR::get_type(parent->getParent(), variable.first);
        struct_types.emplace_back(var_name, type.second.first ? type.first->getPointerTo() : type.first);
    }
    return true;
}

void Generator::Allocation::generate_array_indexing_allocation(              //
    llvm::IRBuilder<> &builder,                                              //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types,    //
    const std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
) {
    size_t idx_size = indexing_expressions.size();
    for (const auto &idx_expr : indexing_expressions) {
        if (idx_expr->get_variation() == ExpressionNode::Variation::RANGE_EXPRESSION) {
            idx_size *= 2;
            break;
        }
    }
    const std::string alloca_name = "arr::idx::" + std::to_string(idx_size);
    if (std::find_if(struct_types.begin(), struct_types.end(), [&](const auto &p) { return p.first == alloca_name; }) //
        != struct_types.end()                                                                                         //
    ) {
        return;
    }
    llvm::Type *length_array_type = llvm::ArrayType::get(builder.getInt64Ty(), idx_size);
    struct_types.emplace_back(alloca_name, length_array_type);
}

bool Generator::Allocation::generate_expression_allocations(              //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const std::shared_ptr<Scope> scope,                                   //
    std::vector<std::pair<std::string, llvm::Type *const>> &struct_types, //
    const ExpressionNode *expression                                      //
) {
    switch (expression->get_variation()) {
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            const auto *node = expression->as<ArrayAccessNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            generate_array_indexing_allocation(builder, struct_types, node->indexing_expressions);
            break;
        }
        case ExpressionNode::Variation::ARRAY_INITIALIZER: {
            const auto *node = expression->as<ArrayInitializerNode>();
            generate_array_indexing_allocation(builder, struct_types, node->length_expressions);
            break;
        }
        case ExpressionNode::Variation::BINARY_OP: {
            const auto *node = expression->as<BinaryOpNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->left.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->right.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::CALL: {
            const auto *node = expression->as<CallNodeExpression>();
            if (!generate_call_allocations(builder, parent, scope, struct_types, static_cast<const CallNodeBase *>(node))) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::DATA_ACCESS: {
            const auto *node = expression->as<DataAccessNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::DEFAULT:
            break;
        case ExpressionNode::Variation::FUNCTION_REFERENCE:
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return false;
        case ExpressionNode::Variation::GROUP_EXPRESSION: {
            const auto *node = expression->as<GroupExpressionNode>();
            for (const auto &expr : node->expressions) {
                if (!generate_expression_allocations(builder, parent, scope, struct_types, expr.get())) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
            const auto *node = expression->as<GroupedDataAccessNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::INITIALIZER: {
            const auto *node = expression->as<InitializerNode>();
            for (const auto &arg : node->args) {
                if (!generate_expression_allocations(builder, parent, scope, struct_types, arg.get())) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::INSTANCE_CALL: {
            const auto *node = expression->as<InstanceCallNodeExpression>();
            if (!generate_call_allocations(builder, parent, scope, struct_types, static_cast<const CallNodeBase *>(node))) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::LITERAL:
            break;
        case ExpressionNode::Variation::OPTIONAL_CHAIN: {
            const auto *node = expression->as<OptionalChainNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::OPTIONAL_UNWRAP: {
            const auto *node = expression->as<OptionalUnwrapNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::RANGE_EXPRESSION: {
            const auto *node = expression->as<RangeExpressionNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->lower_bound.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->upper_bound.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::STRING_INTERPOLATION: {
            const auto *node = expression->as<StringInterpolationNode>();
            for (const auto &val : node->string_content) {
                if (!std::holds_alternative<std::unique_ptr<ExpressionNode>>(val)) {
                    continue;
                }
                const ExpressionNode *expr = std::get<std::unique_ptr<ExpressionNode>>(val).get();
                if (!generate_expression_allocations(builder, parent, scope, struct_types, expr)) {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return false;
                }
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_EXPRESSION: {
            const auto *node = expression->as<SwitchExpression>();
            if (!generate_switch_expression_allocations(builder, parent, scope, struct_types, node)) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::SWITCH_MATCH:
            break;
        case ExpressionNode::Variation::TYPE_CAST: {
            const auto *node = expression->as<TypeCastNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::TYPE:
            break;
        case ExpressionNode::Variation::UNARY_OP: {
            const auto *node = expression->as<UnaryOpExpression>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->operand.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::VARIABLE:
            break;
        case ExpressionNode::Variation::VARIANT_EXTRACTION: {
            const auto *node = expression->as<VariantExtractionNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
        case ExpressionNode::Variation::VARIANT_UNWRAP: {
            const auto *node = expression->as<VariantUnwrapNode>();
            if (!generate_expression_allocations(builder, parent, scope, struct_types, node->base_expr.get())) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return false;
            }
            break;
        }
    }
    return true;
}

unsigned int Generator::Allocation::calculate_type_alignment(llvm::Type *type) {
    // Default alignment
    unsigned int alignment = 8;

    if (type->isVectorTy()) {
        // For vector types, always use the alignment of the base element of the vector
        llvm::VectorType *vectorType = llvm::cast<llvm::VectorType>(type);
        llvm::Type *elemType = vectorType->getElementType();
        alignment = calculate_type_alignment(elemType);
    } else if (type->isStructTy()) {
        llvm::StructType *structType = llvm::cast<llvm::StructType>(type);

        // For struct types, calculate the maximum alignment needed by any of its elements
        for (unsigned i = 0; i < structType->getNumElements(); i++) {
            llvm::Type *elemType = structType->getElementType(i);
            alignment = std::max(alignment, calculate_type_alignment(elemType));
        }
    } else if (type->isArrayTy()) {
        // For array types, check the element type
        llvm::Type *elemType = type->getArrayElementType();
        alignment = calculate_type_alignment(elemType);
    } else if (type->isIntegerTy()) {
        // For integer types check the bit width
        alignment = type->getIntegerBitWidth() / 8;
        if (alignment == 0) {
            alignment = 1;
        }
    } else if (type->isFloatTy()) {
        // For float types the alignment is 4
        alignment = 4;
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
        IR::aligned_store(builder, default_value, field_ptr);
    }

    return alloca;
}
