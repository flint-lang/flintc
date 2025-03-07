#include "error/error.hpp"
#include "generator/generator.hpp"
#include "parser/parser.hpp"

#include "parser/ast/statements/call_node_statement.hpp"

void Generator::Statement::generate_statement(                             //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const std::unique_ptr<StatementNode> &statement                        //
) {
    if (const auto *call_node = dynamic_cast<const CallNodeStatement *>(statement.get())) {
        Expression::generate_call(builder, parent, scope, allocations, dynamic_cast<const CallNodeBase *>(call_node));
    } else if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement.get())) {
        generate_return_statement(builder, parent, scope, allocations, return_node);
    } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement.get())) {
        std::vector<llvm::BasicBlock *> blocks;
        generate_if_statement(builder, parent, allocations, blocks, 0, if_node);
    } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement.get())) {
        generate_while_loop(builder, parent, allocations, while_node);
    } else if (const auto *for_node = dynamic_cast<const ForLoopNode *>(statement.get())) {
        generate_for_loop(builder, parent, allocations, for_node);
    } else if (const auto *assignment_node = dynamic_cast<const AssignmentNode *>(statement.get())) {
        generate_assignment(builder, parent, scope, allocations, assignment_node);
    } else if (const auto *declaration_node = dynamic_cast<const DeclarationNode *>(statement.get())) {
        generate_declaration(builder, parent, scope, allocations, declaration_node);
    } else if (const auto *throw_node = dynamic_cast<const ThrowNode *>(statement.get())) {
        generate_throw_statement(builder, parent, scope, allocations, throw_node);
    } else if (const auto *catch_node = dynamic_cast<const CatchNode *>(statement.get())) {
        generate_catch_statement(builder, parent, allocations, catch_node);
    } else if (const auto *unary_node = dynamic_cast<const UnaryOpStatement *>(statement.get())) {
        generate_unary_op_statement(builder, parent, scope, allocations, unary_node);
    } else {
        THROW_BASIC_ERR(ERR_GENERATING);
    }
}

void Generator::Statement::generate_body(                                 //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const Scope *scope,                                                   //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    for (const auto &statement : scope->body) {
        generate_statement(builder, parent, scope, allocations, statement);
    }
}

void Generator::Statement::generate_return_statement(                      //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const ReturnNode *return_node                                          //
) {
    // Get the return type of the function
    auto *return_struct_type = llvm::cast<llvm::StructType>(parent->getReturnType());

    // Allocate space for the function's return type (should be a struct type)
    llvm::AllocaInst *return_struct = builder.CreateAlloca(return_struct_type, nullptr, "ret_struct");
    return_struct->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Create ret struct '" + return_struct->getName().str() + "' of type '" + return_struct_type->getName().str() + "'")));

    // First, always store the error code (0 for no error)
    llvm::Value *error_ptr = builder.CreateStructGEP(return_struct_type, return_struct, 0, "err_ptr");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder.getContext()), 0), error_ptr);

    // If we have a return value, store it in the struct
    if (return_node != nullptr && return_node->return_value != nullptr) {
        // Generate the expression for the return value
        llvm::Value *return_value = Expression::generate_expression(builder, parent, scope, allocations, return_node->return_value.get());

        // Ensure the return value matches the function's return type
        if (return_value == nullptr) {
            THROW_BASIC_ERR(ERR_GENERATING);
        }

        // Store the return value in the struct (at index 1)
        llvm::Value *value_ptr = builder.CreateStructGEP(return_struct_type, return_struct, 1, "val_ptr");
        llvm::StoreInst *value_store = builder.CreateStore(return_value, value_ptr);
        value_store->setMetadata("comment",
            llvm::MDNode::get(parent->getContext(),
                llvm::MDString::get(parent->getContext(),
                    "Store result of expr '" + return_value->getName().str() + "' in '" + value_store->getName().str() + "'")));
    }

    // Generate the return instruction with the evaluated value
    llvm::LoadInst *return_struct_val = builder.CreateLoad(return_struct_type, return_struct, "ret_val");
    return_struct_val->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Load allocated ret struct of type '" + return_struct_type->getName().str() + "'")));
    builder.CreateRet(return_struct_val);
}

void Generator::Statement::generate_throw_statement(                       //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const ThrowNode *throw_node                                            //
) {
    // Get the return type of the function
    auto *throw_struct_type = llvm::cast<llvm::StructType>(parent->getReturnType());

    // Allocate the struct and set all of its values to their respective default values
    llvm::AllocaInst *throw_struct = Allocation::generate_default_struct(builder, throw_struct_type, "throw_ret", true);
    throw_struct->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Create default struct of type '" + throw_struct_type->getName().str() + "' except first value")));

    // Create the pointer to the error value (the 0th index of the struct)
    llvm::Value *error_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, 0, "err_ptr");
    // Generate the expression right of the throw statement, it has to be of type int
    llvm::Value *err_value = Expression::generate_expression(builder, parent, scope, allocations, throw_node->throw_value.get());
    // Store the error value in the struct
    builder.CreateStore(err_value, error_ptr);

    // Generate the throw (return) instruction with the evaluated value
    llvm::LoadInst *throw_struct_val = builder.CreateLoad(throw_struct_type, throw_struct, "throw_val");
    throw_struct_val->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Load allocated throw struct of type '" + throw_struct_type->getName().str() + "'")));
    builder.CreateRet(throw_struct_val);
}

void Generator::Statement::generate_if_blocks( //
    llvm::Function *parent,                    //
    std::vector<llvm::BasicBlock *> &blocks,   //
    const IfNode *if_node                      //
) {
    // Count total number of branches and create blocks
    const IfNode *current = if_node;
    unsigned int branch_count = 0;
    llvm::LLVMContext &context = parent->getContext();

    while (current != nullptr) {
        if (branch_count != 0) {
            // Create then condition block (for the else if blocks)
            blocks.push_back(llvm::BasicBlock::Create(      //
                context,                                    //
                "then_cond" + std::to_string(branch_count), //
                parent)                                     //
            );
        }

        // Create then block
        blocks.push_back(llvm::BasicBlock::Create( //
            context,                               //
            "then" + std::to_string(branch_count), //
            parent)                                //
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
            const auto &else_statements = std::get<std::unique_ptr<Scope>>(else_scope)->body;
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
    blocks.push_back(llvm::BasicBlock::Create(context, "merge", parent));
}

void Generator::Statement::generate_if_statement(                          //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    std::vector<llvm::BasicBlock *> &blocks,                               //
    unsigned int nesting_level,                                            //
    const IfNode *if_node                                                  //
) {
    if (if_node == nullptr || if_node->condition == nullptr) {
        // Invalid IfNode: missing condition
        THROW_BASIC_ERR(ERR_GENERATING);
    }

    // First call (nesting_level == 0): Create all blocks for entire if-chain
    if (nesting_level == 0) {
        generate_if_blocks(parent, blocks, if_node);
    }

    // Index calculation for current blocks
    unsigned int then_idx = nesting_level;
    // Check if this is the if and the next index is not the last index. This needs to be done for the elif branches
    if (then_idx != 0 && then_idx + 1 < blocks.size()) {
        then_idx++;
    }
    // Defaults to the block after the current block
    unsigned int next_idx = then_idx + 1;

    // Generate the condition
    llvm::Value *condition = Expression::generate_expression( //
        builder,                                              //
        parent,                                               //
        if_node->then_scope->parent_scope,                    //
        allocations,                                          //
        if_node->condition.get()                              //
    );
    if (condition == nullptr) {
        // Failed to generate condition expression
        THROW_BASIC_ERR(ERR_GENERATING);
    }
    // Create conditional branch
    llvm::BranchInst *branch = builder.CreateCondBr( //
        condition,                                   //
        blocks[then_idx],                            //
        blocks[next_idx]                             //
    );
    branch->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Branch between '" + blocks[then_idx]->getName().str() + "' and '" + blocks[next_idx]->getName().str() +
                    "' based on condition '" + condition->getName().str() + "'")));

    // Generate then branch
    builder.SetInsertPoint(blocks[then_idx]);
    generate_body(builder, parent, if_node->then_scope.get(), allocations);
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Branch to merge block
        builder.CreateBr(blocks.back());
    }

    // Handle else-if or else
    if (if_node->else_scope.has_value()) {
        const auto &else_scope = if_node->else_scope.value();
        if (std::holds_alternative<std::unique_ptr<IfNode>>(else_scope)) {
            // Recursive call for else-if
            builder.SetInsertPoint(blocks[next_idx]);
            generate_if_statement(                                  //
                builder,                                            //
                parent,                                             //
                allocations,                                        //
                blocks,                                             //
                nesting_level + 1,                                  //
                std::get<std::unique_ptr<IfNode>>(else_scope).get() //
            );
        } else {
            // Handle final else, if it exists
            const auto &last_else_scope = std::get<std::unique_ptr<Scope>>(else_scope);
            if (!last_else_scope->body.empty()) {
                builder.SetInsertPoint(blocks[next_idx]);
                generate_body(builder, parent, last_else_scope.get(), allocations);
                if (builder.GetInsertBlock()->getTerminator() == nullptr) {
                    // Branch to the merge block
                    builder.CreateBr(blocks.back());
                }
            }
        }
    }

    // Set the insert point to the merge block
    if (nesting_level == 0) {
        builder.SetInsertPoint(blocks.back());
    }
}

void Generator::Statement::generate_while_loop(                            //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const WhileNode *while_node                                            //
) {
    // Get the current block, we need to add a branch instruction to this block to point to the while condition block
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Create the basic blocks for the condition check, the while body and the merge block
    std::array<llvm::BasicBlock *, 3> while_blocks{};
    // Create then condition block (for the else if blocks)
    while_blocks[0] = llvm::BasicBlock::Create(builder.getContext(), "while_cond", parent);
    while_blocks[1] = llvm::BasicBlock::Create(builder.getContext(), "while_body", parent);
    while_blocks[2] = llvm::BasicBlock::Create(builder.getContext(), "merge", parent);

    // Create the branch instruction in the predecessor block to point to the while_cond block
    builder.SetInsertPoint(pred_block);
    llvm::BranchInst *init_while_br = builder.CreateBr(while_blocks[0]);
    init_while_br->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Start while loop in '" + while_blocks[0]->getName().str() + "'")));

    // Create the condition block's content
    builder.SetInsertPoint(while_blocks[0]);
    llvm::Value *expression = Expression::generate_expression( //
        builder,                                               //
        parent,                                                //
        while_node->scope->get_parent(),                       //
        allocations,                                           //
        while_node->condition.get()                            //
    );
    llvm::BranchInst *branch = builder.CreateCondBr(expression, while_blocks[1], while_blocks[2]);
    branch->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Continue loop in '" + while_blocks[1]->getName().str() + "' based on cond '" + expression->getName().str() + "'")));

    // Create the while block's body
    builder.SetInsertPoint(while_blocks[1]);
    generate_body(builder, parent, while_node->scope.get(), allocations);
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Point back to the condition block to create the loop
        builder.CreateBr(while_blocks[0]);
    }

    // Finally set the builder to the merge block again
    builder.SetInsertPoint(while_blocks[2]);
}

void Generator::Statement::generate_for_loop(                              //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const ForLoopNode *for_node                                            //
) {
    // Get the current block, we need to add a branch instruction to this block to point to the while condition block
    llvm::BasicBlock *pred_block = builder.GetInsertBlock();

    // Generate the instructions from the definition scope (it should only contain the initializer statement, for example 'i32 i = 0')
    generate_body(builder, parent, for_node->definition_scope.get(), allocations);

    // Create the basic blocks for the condition check, the while body and the merge block
    std::array<llvm::BasicBlock *, 3> for_blocks{};
    // Create then condition block (for the else if blocks)
    for_blocks[0] = llvm::BasicBlock::Create(builder.getContext(), "for_cond", parent);
    for_blocks[1] = llvm::BasicBlock::Create(builder.getContext(), "for_body", parent);
    for_blocks[2] = llvm::BasicBlock::Create(builder.getContext(), "merge", parent);

    // Create the branch instruction in the predecessor block to point to the for_cond block
    builder.SetInsertPoint(pred_block);
    generate_body(builder, parent, for_node->definition_scope.get(), allocations);
    llvm::BranchInst *init_for_br = builder.CreateBr(for_blocks[0]);
    init_for_br->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Start for loop in '" + for_blocks[0]->getName().str() + "'")));

    // Create the condition block's content
    builder.SetInsertPoint(for_blocks[0]);
    llvm::Value *expression = Expression::generate_expression( //
        builder,                                               //
        parent,                                                //
        for_node->definition_scope.get(),                      //
        allocations,                                           //
        for_node->condition.get()                              //
    );
    llvm::BranchInst *branch = builder.CreateCondBr(expression, for_blocks[1], for_blocks[2]);
    branch->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Continue loop in '" + for_blocks[1]->getName().str() + "' based on cond '" + expression->getName().str() + "'")));

    // Create the while block's body
    builder.SetInsertPoint(for_blocks[1]);
    generate_body(builder, parent, for_node->body.get(), allocations);
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Point back to the condition block to create the loop
        builder.CreateBr(for_blocks[0]);
    }

    // Finally set the builder to the merge block again
    builder.SetInsertPoint(for_blocks[2]);
}

void Generator::Statement::generate_catch_statement(                       //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const CatchNode *catch_node                                            //
) {
    // The catch statement is basically just an if check if the err value of the function return is != 0 or not
    const std::optional<CallNodeBase *> call_node = Parser::get_call_from_id(catch_node->call_id);
    if (!call_node.has_value()) {
        // Catch does not have a referenced function
        THROW_BASIC_ERR(ERR_GENERATING);
        return;
    }
    const std::string err_ret_name =
        "s" + std::to_string(call_node.value()->scope_id) + "::c" + std::to_string(call_node.value()->call_id) + "::err";
    llvm::AllocaInst *const err_var = allocations.at(err_ret_name);

    // Load the error value
    llvm::LoadInst *err_val = builder.CreateLoad(                                                    //
        llvm::Type::getInt32Ty(builder.getContext()),                                                //
        err_var,                                                                                     //
        call_node.value()->function_name + "_" + std::to_string(call_node.value()->call_id) + "_err" //
    );
    err_val->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Load err val of call '" + call_node.value()->function_name + "::" + std::to_string(call_node.value()->call_id) + "'")));

    llvm::BasicBlock *last_block = &parent->back();
    llvm::BasicBlock *first_block = &parent->front();
    // Create basic block for the catch block
    llvm::BasicBlock *current_block = builder.GetInsertBlock();

    // Check if the current block is the last block, if it is not, insert right after the current block
    bool will_insert_after = current_block == last_block || current_block != first_block;
    llvm::BasicBlock *insert_before = will_insert_after ? (current_block->getNextNode()) : current_block;

    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(                                           //
        builder.getContext(),                                                                           //
        call_node.value()->function_name + "_" + std::to_string(call_node.value()->call_id) + "_catch", //
        parent,                                                                                         //
        insert_before                                                                                   //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(                                           //
        builder.getContext(),                                                                           //
        call_node.value()->function_name + "_" + std::to_string(call_node.value()->call_id) + "_merge", //
        parent,                                                                                         //
        insert_before                                                                                   //
    );
    builder.SetInsertPoint(current_block);

    // Create the if check and compare the err value to 0
    llvm::ConstantInt *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder.getContext()), 0);
    llvm::Value *err_condition = builder.CreateICmpNE( //
        err_val,                                       //
        zero,                                          //
        "errcmp"                                       //
    );

    // Create the branching operation
    builder.CreateCondBr(err_condition, catch_block, merge_block)
        ->setMetadata("comment",
            llvm::MDNode::get(parent->getContext(),
                llvm::MDString::get(parent->getContext(),
                    "Branch to '" + catch_block->getName().str() + "' if '" + call_node.value()->function_name + "' returned error")));

    // Add the error variable to the list of allocations (temporarily)
    // llvm::AllocaInst *const variable = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);
    const std::string err_alloca_name = "s" + std::to_string(catch_node->scope->scope_id) + "::" + catch_node->var_name.value();
    allocations.insert({err_alloca_name, allocations.at(err_ret_name)});

    // Generate the body of the catch block
    builder.SetInsertPoint(catch_block);
    generate_body(builder, parent, catch_node->scope.get(), allocations);

    // Remove the error variable from the list of allocations
    allocations.erase(allocations.find(err_alloca_name));

    // Add branch to the merge block from the catch block if it does not contain a terminator (return or throw)
    if (catch_block->getTerminator() == nullptr) {
        builder.CreateBr(merge_block);
    }

    // Set the insert block to the merge block again
    builder.SetInsertPoint(merge_block);
}

void Generator::Statement::generate_declaration(                           //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const DeclarationNode *declaration_node                                //
) {
    llvm::Value *expression = Expression::generate_expression(builder, parent, scope, allocations, declaration_node->initializer.get());

    // Check if the declaration_node is a function call.
    // If it is, the "real" value of the call has to be extracted. Otherwise, it can be used directly!
    if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(declaration_node->initializer.get())) {
        // Call the function and store its result in the return stuct
        const std::string call_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
        llvm::AllocaInst *const alloca_ret = allocations.at(call_ret_name);

        // Extract the second field (index 1) from the struct - this is the actual return value
        llvm::Value *value_ptr = builder.CreateStructGEP( //
            expression->getType(),                        //
            alloca_ret,                                   //
            1,                                            //
            declaration_node->name + "__VAL_PTR"          //
        );
        llvm::LoadInst *value_load = builder.CreateLoad(                         //
            IR::get_type_from_str(parent->getContext(), declaration_node->type), //
            value_ptr,                                                           //
            declaration_node->name + "__VAL"                                     //
        );
        value_load->setMetadata("comment",
            llvm::MDNode::get(parent->getContext(),
                llvm::MDString::get(parent->getContext(), "Load the actual val of '" + declaration_node->name + "' from its ptr")));

        // Store the actual value in the declared variable
        const std::string call_val_1_name = "s" + std::to_string(call_node->scope_id) + "::" + declaration_node->name;
        llvm::AllocaInst *const call_val_1_alloca = allocations.at(call_val_1_name);
        builder.CreateStore(value_load, call_val_1_alloca)
            ->setMetadata("comment",
                llvm::MDNode::get(parent->getContext(),
                    llvm::MDString::get(parent->getContext(), "Store the actual val of '" + declaration_node->name + "'")));
        return;
    }

    const unsigned int scope_id = scope->variable_types.at(declaration_node->name).second;
    const std::string var_name = "s" + std::to_string(scope_id) + "::" + declaration_node->name;
    llvm::AllocaInst *const alloca = allocations.at(var_name);

    llvm::StoreInst *store = builder.CreateStore(expression, alloca);
    store->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Store the actual val of '" + declaration_node->name + "'")));
}

void Generator::Statement::generate_assignment(                            //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const AssignmentNode *assignment_node                                  //
) {
    llvm::Value *expression = Expression::generate_expression(builder, parent, scope, allocations, assignment_node->expression.get());

    // Check if the variable is declared
    if (scope->variable_types.find(assignment_node->name) == scope->variable_types.end()) {
        // Error: Undeclared Variable
        THROW_BASIC_ERR(ERR_GENERATING);
    }
    // Get the allocation of the lhs
    const unsigned int variable_decl_scope = scope->variable_types.at(assignment_node->name).second;
    llvm::AllocaInst *const lhs = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + assignment_node->name);

    // Check if the rhs is a function call
    if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(assignment_node->expression.get())) {
        const std::string call_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
        llvm::AllocaInst *const alloca_ret = allocations.at(call_ret_name);

        // Extract the actual return value (index 1)
        llvm::Value *value_ptr = builder.CreateStructGEP( //
            expression->getType(),                        //
            alloca_ret,                                   //
            1,                                            //
            assignment_node->name + "__VAL_PTR"           //
        );
        llvm::LoadInst *value_load = builder.CreateLoad( //
            lhs->getAllocatedType(),                     //
            value_ptr,                                   //
            assignment_node->name + "__VAL"              //
        );
        // Use the extracted value instead of the struct
        expression = value_load;
    }

    llvm::StoreInst *store = builder.CreateStore(expression, lhs);
    store->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Store result of expr in var '" + assignment_node->name + "'")));
}

void Generator::Statement::generate_unary_op_statement(                    //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const UnaryOpStatement *unary_op                                       //
) {
    const VariableNode *var_node = dynamic_cast<const VariableNode *>(unary_op->operand.get());
    if (var_node == nullptr) {
        // Expression is not a variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return;
    }
    const unsigned int scope_id = scope->variable_types.at(var_node->name).second;
    const std::string var_name = "s" + std::to_string(scope_id) + "::" + var_node->name;
    llvm::AllocaInst *const alloca = allocations.at(var_name);

    llvm::LoadInst *var_value = builder.CreateLoad(alloca->getAllocatedType(), alloca, var_node->name + "_val");
    var_value->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(), llvm::MDString::get(parent->getContext(), "Load val of var '" + var_node->name + "'")));
    llvm::Value *operation_result = nullptr;

    switch (unary_op->operator_token) {
        default:
            // Unknown unary operator
            THROW_BASIC_ERR(ERR_GENERATING);
            return;
        case TOK_INCREMENT:
            if (var_node->type == "i32" || var_node->type == "i64") {
                llvm::Value *one = llvm::ConstantInt::get(var_value->getType(), 1);
                operation_result = Arithmetic::int_safe_add(builder, var_value, one);
            } else if (var_node->type == "u32" || var_node->type == "u64") {
                llvm::Value *one = llvm::ConstantInt::get(var_value->getType(), 1);
                operation_result = Arithmetic::uint_safe_add(builder, var_value, one);
            } else if (var_node->type == "f32" || var_node->type == "f64") {
                llvm::Value *one = llvm::ConstantFP::get(var_value->getType(), 1.0);
                operation_result = builder.CreateFAdd(var_value, one);
            } else {
                // Type not allowed for decrement operator
                THROW_BASIC_ERR(ERR_GENERATING);
                return;
            }
            break;
        case TOK_DECREMENT:
            if (var_node->type == "i32" || var_node->type == "i64") {
                llvm::Value *one = llvm::ConstantInt::get(var_value->getType(), 1);
                operation_result = Arithmetic::int_safe_sub(builder, var_value, one);
            } else if (var_node->type == "u32" || var_node->type == "u64") {
                llvm::Value *one = llvm::ConstantInt::get(var_value->getType(), 1);
                operation_result = Arithmetic::uint_safe_sub(builder, var_value, one);
            } else if (var_node->type == "f32" || var_node->type == "f64") {
                llvm::Value *one = llvm::ConstantFP::get(var_value->getType(), 1.0);
                operation_result = builder.CreateFSub(var_value, one);
            } else {
                // Type not allowed for decrement operator
                THROW_BASIC_ERR(ERR_GENERATING);
                return;
            }
            break;
    }
    llvm::StoreInst *operation_store = builder.CreateStore(operation_result, alloca);
    operation_store->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Store result of unary operation on '" + var_node->name + "'")));
}
