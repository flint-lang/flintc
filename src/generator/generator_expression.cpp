#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "globals.hpp"
#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"

#include <llvm/IR/Instructions.h>
#include <string>
#include <variant>

Generator::group_mapping Generator::Expression::generate_expression(                                    //
    llvm::IRBuilder<> &builder,                                                                         //
    llvm::Function *parent,                                                                             //
    const Scope *scope,                                                                                 //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                                   //
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                      //
    const ExpressionNode *expression_node,                                                              //
    const bool is_reference                                                                             //
) {
    std::vector<llvm::Value *> group_map;
    if (const auto *variable_node = dynamic_cast<const VariableNode *>(expression_node)) {
        group_map.emplace_back(generate_variable(builder, parent, scope, allocations, variable_node, is_reference));
        return group_map;
    }
    if (is_reference) {
        // Only variables are allowed as references for now
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    if (const auto *unary_op_node = dynamic_cast<const UnaryOpExpression *>(expression_node)) {
        return generate_unary_op_expression(builder, parent, scope, allocations, garbage, expr_depth, unary_op_node);
    }
    if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expression_node)) {
        group_map.emplace_back(generate_literal(builder, literal_node));
        return group_map;
    }
    if (const auto *interpol_node = dynamic_cast<const StringInterpolationNode *>(expression_node)) {
        group_map.emplace_back(generate_string_interpolation(builder, parent, scope, allocations, garbage, expr_depth, interpol_node));
        return group_map;
    }
    if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(expression_node)) {
        return generate_call(builder, parent, scope, allocations, call_node);
    }
    if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expression_node)) {
        return generate_binary_op(builder, parent, scope, allocations, garbage, expr_depth, binary_op_node);
    }
    if (const auto *type_cast_node = dynamic_cast<const TypeCastNode *>(expression_node)) {
        return generate_type_cast(builder, parent, scope, allocations, garbage, expr_depth, type_cast_node);
    }
    if (const auto *group_node = dynamic_cast<const GroupExpressionNode *>(expression_node)) {
        return generate_group_expression(builder, parent, scope, allocations, garbage, expr_depth, group_node);
    }
    if (const auto *initializer = dynamic_cast<const InitializerNode *>(expression_node)) {
        return generate_initializer(builder, parent, scope, allocations, garbage, expr_depth, initializer);
    }
    if (const auto *data_access = dynamic_cast<const DataAccessNode *>(expression_node)) {
        group_map.emplace_back(generate_data_access(builder, scope, allocations, data_access));
        return group_map;
    }
    if (const auto *grouped_data_access = dynamic_cast<const GroupedDataAccessNode *>(expression_node)) {
        return generate_grouped_data_access(builder, scope, allocations, grouped_data_access);
    }
    THROW_BASIC_ERR(ERR_GENERATING);
    return std::nullopt;
}

llvm::Value *Generator::Expression::generate_literal( //
    llvm::IRBuilder<> &builder,                       //
    const LiteralNode *literal_node                   //
) {
    if (std::holds_alternative<int>(literal_node->value)) {
        return llvm::ConstantInt::get(         //
            llvm::Type::getInt32Ty(context),   //
            std::get<int>(literal_node->value) //
        );
    }
    if (std::holds_alternative<float>(literal_node->value)) {
        return llvm::ConstantFP::get(                                 //
            llvm::Type::getFloatTy(context),                          //
            static_cast<double>(std::get<float>(literal_node->value)) //
        );
    }
    if (std::holds_alternative<std::string>(literal_node->value)) {
        // Get the constant string value
        std::string str = std::get<std::string>(literal_node->value);
        return IR::generate_const_string(builder, str);
    }
    if (std::holds_alternative<bool>(literal_node->value)) {
        return llvm::ConstantInt::get(                             //
            llvm::Type::getInt1Ty(context),                        //
            static_cast<char>(std::get<bool>(literal_node->value)) //
        );
    }
    if (std::holds_alternative<char>(literal_node->value)) {
        return llvm::ConstantInt::get(          //
            llvm::Type::getInt8Ty(context),     //
            std::get<char>(literal_node->value) //
        );
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return nullptr;
}

llvm::Value *Generator::Expression::generate_variable(                //
    llvm::IRBuilder<> &builder,                                       //
    llvm::Function *parent,                                           //
    const Scope *scope,                                               //
    std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const VariableNode *variable_node,                                //
    const bool is_reference                                           //
) {
    if (variable_node == nullptr) {
        // Error: Null Node
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }

    // First, check if this is a function parameter
    for (auto &arg : parent->args()) {
        if (arg.getName() == variable_node->name) {
            // If it's a parameter, and its an mutable primitive type, we dont return it directly, toherwise we do
            if (scope->variables.find(arg.getName().str()) != scope->variables.end()) {
                auto var = scope->variables.at(arg.getName().str());
                if (keywords.find(std::get<0>(var)->to_string()) != keywords.end() && std::get<2>(var)) {
                    continue;
                }
            }
            return &arg;
        }
    }

    // If not a parameter, handle as local variable
    if (scope->variables.find(variable_node->name) == scope->variables.end()) {
        // Error: Undeclared Variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }
    const unsigned int variable_decl_scope = std::get<1>(scope->variables.at(variable_node->name));
    llvm::Value *const variable = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);
    if (is_reference) {
        return variable;
    }

    // Get the type that the pointer points to
    llvm::Type *value_type = IR::get_type(std::get<std::shared_ptr<Type>>(variable_node->type)).first;

    // Load the variable's value if it's a pointer
    llvm::LoadInst *load = builder.CreateLoad(value_type, variable, variable_node->name + "_val");
    load->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Load val of var '" + variable_node->name + "'")));

    return load;
}

llvm::Value *Generator::Expression::generate_string_interpolation(                                      //
    llvm::IRBuilder<> &builder,                                                                         //
    llvm::Function *parent,                                                                             //
    const Scope *scope,                                                                                 //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                                   //
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                      //
    const StringInterpolationNode *interpol_node                                                        //
) {
    assert(!interpol_node->string_content.empty());
    // The string interpolation works by adding all strings from the most left up to the most right one
    // First, create the string variable
    auto it = interpol_node->string_content.begin();
    llvm::Value *str_value = nullptr;
    if (std::holds_alternative<std::unique_ptr<LiteralNode>>(*it)) {
        llvm::Function *init_str_fn = String::string_manip_functions.at("init_str");
        const std::string lit_string = std::get<std::string>(std::get<std::unique_ptr<LiteralNode>>(*it)->value);
        llvm::Value *lit_str = IR::generate_const_string(builder, lit_string);
        str_value = builder.CreateCall(init_str_fn, {lit_str, builder.getInt64(lit_string.length())}, "init_str_value");
    } else {
        // Currently only the first output of a group is supported in string interpolation, as there currently is no group printing yet
        group_mapping res = generate_type_cast(                                                                              //
            builder, parent, scope, allocations, garbage, expr_depth + 1, std::get<std::unique_ptr<TypeCastNode>>(*it).get() //
        );
        if (!res.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        str_value = res.value().at(0);
    }
    ++it;

    llvm::Function *add_str_lit = String::string_manip_functions.at("add_str_lit");
    llvm::Function *add_str_str = String::string_manip_functions.at("add_str_str");
    for (; it != interpol_node->string_content.end(); ++it) {
        if (std::holds_alternative<std::unique_ptr<LiteralNode>>(*it)) {
            const std::string lit_string = std::get<std::string>(std::get<std::unique_ptr<LiteralNode>>(*it)->value);
            llvm::Value *lit_str = IR::generate_const_string(builder, lit_string);
            str_value = builder.CreateCall(add_str_lit, {str_value, lit_str, builder.getInt64(lit_string.length())});
        } else {
            group_mapping res = generate_type_cast(                                                                              //
                builder, parent, scope, allocations, garbage, expr_depth + 1, std::get<std::unique_ptr<TypeCastNode>>(*it).get() //
            );
            if (!res.has_value()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
            str_value = builder.CreateCall(add_str_str, {str_value, res.value().at(0)});
        }
    }
    if (garbage.count(expr_depth) == 0) {
        garbage[expr_depth].emplace_back("str", str_value);
    } else {
        garbage.at(expr_depth).emplace_back("str", str_value);
    }
    return str_value;
}

Generator::group_mapping Generator::Expression::generate_call(        //
    llvm::IRBuilder<> &builder,                                       //
    llvm::Function *parent,                                           //
    const Scope *scope,                                               //
    std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const CallNodeBase *call_node                                     //
) {
    // Get the arguments
    std::vector<llvm::Value *> args;
    args.reserve(call_node->arguments.size());
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> garbage;
    for (const auto &arg : call_node->arguments) {
        group_mapping expression = generate_expression(builder, parent, scope, allocations, garbage, 0, arg.first.get(), arg.second);
        if (!expression.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (expression.value().empty()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *expr_val = expression.value().at(0);
        if (call_node->function_name != "print" && std::holds_alternative<std::shared_ptr<Type>>(arg.first->type) &&
            std::get<std::shared_ptr<Type>>(arg.first->type)->to_string() == "str") {
            expr_val = String::generate_string_declaration(builder, expr_val, arg.first.get());
        }
        args.emplace_back(expr_val);
    }

    // Check if it is a builtin function and call it
    if (builtin_functions.find(call_node->function_name) != builtin_functions.end()) {
        llvm::Function *builtin_function = builtins.at(builtin_functions.at(call_node->function_name));
        if (builtin_function == nullptr) {
            // Function has not been generated yet, but it should have been
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        // There doesnt exist a builtin print function for any groups
        if (!std::holds_alternative<std::shared_ptr<Type>>(call_node->arguments.at(0).first->type)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        // Call the builtin function 'print'
        std::vector<llvm::Value *> return_value;
        if (call_node->function_name == "print" && call_node->arguments.size() == 1 &&
            print_functions.find(std::get<std::shared_ptr<Type>>(call_node->arguments.at(0).first->type)->to_string()) !=
                print_functions.end()) {
            // If the argument is of type str we need to differentiate between literals and str variables
            if (std::get<std::shared_ptr<Type>>(call_node->arguments.at(0).first->type)->to_string() == "str") {
                if (dynamic_cast<const LiteralNode *>(call_node->arguments.at(0).first.get())) {
                    return_value.emplace_back(builder.CreateCall(print_functions.at("str"), args));
                    Statement::clear_garbage(builder, garbage);
                    return return_value;
                } else {
                    return_value.emplace_back(builder.CreateCall(print_functions.at("str_var"), args));
                    Statement::clear_garbage(builder, garbage);
                    return return_value;
                }
            }
            return_value.emplace_back(builder.CreateCall(                                                                 //
                print_functions.at(std::get<std::shared_ptr<Type>>(call_node->arguments.at(0).first->type)->to_string()), //
                args                                                                                                      //
                ));
            Statement::clear_garbage(builder, garbage);
            return return_value;
        }
        return_value.emplace_back(builder.CreateCall(builtin_function, args));
        return return_value;
    }

    // Get the function definition from any module
    auto [func_decl_res, is_call_extern] = Function::get_function_definition(parent, call_node);
    if (!func_decl_res.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    llvm::Function *func_decl = func_decl_res.value();
    if (func_decl == nullptr) {
        // Is a builtin function, but the code block above should have been executed in that case, because we are here, something went
        // wrong
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }

    // Create the call instruction using the original declaration
    llvm::CallInst *call = builder.CreateCall(                                  //
        func_decl,                                                              //
        args,                                                                   //
        call_node->function_name + std::to_string(call_node->call_id) + "_call" //
    );
    call->setMetadata("comment",
        llvm::MDNode::get(context, llvm::MDString::get(context, "Call of function '" + call_node->function_name + "'")));

    // Store results immideately after call
    const std::string call_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
    const std::string call_err_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";

    // Store function result
    llvm::Value *res_var = allocations.at(call_ret_name);
    builder.CreateStore(call, res_var);

    // Extract and store error value
    llvm::StructType *return_type = static_cast<llvm::StructType *>(                           //
        IR::add_and_or_get_type(std::get<std::vector<std::shared_ptr<Type>>>(call_node->type)) //
    );
    llvm::Value *err_ptr = builder.CreateStructGEP(                                //
        return_type,                                                               //
        res_var,                                                                   //
        0,                                                                         //
        call_node->function_name + std::to_string(call_node->call_id) + "_err_ptr" //
    );
    llvm::Value *err_val = builder.CreateLoad(                                     //
        llvm::Type::getInt32Ty(context),                                           //
        err_ptr,                                                                   //
        call_node->function_name + std::to_string(call_node->call_id) + "_err_val" //
    );
    llvm::Value *err_var = allocations.at(call_err_name);
    builder.CreateStore(err_val, err_var);

    // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
    if (!call_node->has_catch) {
        generate_rethrow(builder, parent, allocations, call_node);
    }

    // Add the call instruction to the list of unresolved functions only if it was a module-intern call
    if (!is_call_extern) {
        if (unresolved_functions.find(call_node->function_name) == unresolved_functions.end()) {
            unresolved_functions[call_node->function_name] = {call};
        } else {
            unresolved_functions[call_node->function_name].push_back(call);
        }
    } else {
        for (const auto &[file_name, function_list] : file_function_names) {
            if (std::find(function_list.begin(), function_list.end(), call_node->function_name) != function_list.end()) {
                // Check if any unresolved function call from a function of that file even exists, if not create the first one
                if (file_unresolved_functions.find(file_name) == file_unresolved_functions.end()) {
                    file_unresolved_functions[file_name][call_node->function_name] = {call};
                    break;
                }
                // Check if this function the call references has been referenced before. If not, create a new entry to the map
                // otherwise just add the call
                if (file_unresolved_functions.at(file_name).find(call_node->function_name) ==
                    file_unresolved_functions.at(file_name).end()) {
                    file_unresolved_functions.at(file_name)[call_node->function_name] = {call};
                } else {
                    file_unresolved_functions.at(file_name).at(call_node->function_name).push_back(call);
                }
                break;
            }
        }
    }

    // Extract all the return values from the call (everything except the error return)
    std::vector<llvm::Value *> return_value;
    for (unsigned int i = 1; i < return_type->getNumElements(); i++) {
        llvm::Value *elem_ptr = builder.CreateStructGEP(                                                                 //
            return_type,                                                                                                 //
            res_var,                                                                                                     //
            i,                                                                                                           //
            call_node->function_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value_ptr" //
        );
        llvm::LoadInst *elem_value = builder.CreateLoad(                                                       //
            return_type->getElementType(i),                                                                    //
            elem_ptr,                                                                                          //
            call_node->function_name + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value" //
        );
        return_value.emplace_back(elem_value);
    }

    // Finally, clear all garbage
    Statement::clear_garbage(builder, garbage);
    return return_value;
}

void Generator::Expression::generate_rethrow(                         //
    llvm::IRBuilder<> &builder,                                       //
    llvm::Function *parent,                                           //
    std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const CallNodeBase *call_node                                     //
) {
    const std::string err_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    llvm::Value *const err_var = allocations.at(err_ret_name);

    // Load error value
    llvm::LoadInst *err_val = builder.CreateLoad(                                    //
        llvm::Type::getInt32Ty(context),                                             //
        err_var,                                                                     //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_val" //
    );
    err_val->setMetadata("comment",
        llvm::MDNode::get(context,
            llvm::MDString::get(context,
                "Load err val of call '" + call_node->function_name + "::" + std::to_string(call_node->call_id) + "'")));

    llvm::BasicBlock *last_block = &parent->back();
    llvm::BasicBlock *first_block = &parent->front();
    // Create basic block for the catch block
    llvm::BasicBlock *current_block = builder.GetInsertBlock();

    // Check if the current block is the last block, if it is not, insert right after the current block
    bool will_insert_after = current_block == last_block || current_block != first_block;
    llvm::BasicBlock *insert_before = will_insert_after ? (current_block->getNextNode()) : current_block;

    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(                           //
        context,                                                                        //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_catch", //
        parent,                                                                         //
        insert_before                                                                   //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(                           //
        context,                                                                        //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_merge", //
        parent,                                                                         //
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

    // Generate the body of the catch block, it only contains re-throwing the error
    builder.SetInsertPoint(catch_block);
    // Copy of the generate_throw function
    {
        // Get the return type of the function
        auto *throw_struct_type = llvm::cast<llvm::StructType>(parent->getReturnType());

        // Allocate the struct and set all of its values to their respective default values
        llvm::AllocaInst *throw_struct = Allocation::generate_default_struct(builder, throw_struct_type, "throw_ret", true);
        throw_struct->setMetadata("comment",
            llvm::MDNode::get(context,
                llvm::MDString::get(context,
                    "Create default struct of type '" + throw_struct_type->getName().str() + "' except first value")));

        // Create the pointer to the error value (the 0th index of the struct)
        llvm::Value *error_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, 0, "err_ptr");
        // Store the error value in the struct
        builder.CreateStore(err_val, error_ptr);

        // Generate the throw (return) instruction with the evaluated value
        llvm::LoadInst *throw_struct_val = builder.CreateLoad(throw_struct_type, throw_struct, "throw_val");
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

Generator::group_mapping Generator::Expression::generate_group_expression(                              //
    llvm::IRBuilder<> &builder,                                                                         //
    llvm::Function *parent,                                                                             //
    const Scope *scope,                                                                                 //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                                   //
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                      //
    const GroupExpressionNode *group_node                                                               //
) {
    std::vector<llvm::Value *> group_values;
    group_values.reserve(group_node->expressions.size());
    for (const auto &expr : group_node->expressions) {
        std::vector<llvm::Value *> expr_val =
            generate_expression(builder, parent, scope, allocations, garbage, expr_depth + 1, expr.get()).value();
        assert(expr_val.size() == 1); // Nested groups are not allowed
        assert(expr_val.at(0) != nullptr);
        group_values.push_back(expr_val[0]);
    }
    return group_values;
}

Generator::group_mapping Generator::Expression::generate_initializer(                                   //
    llvm::IRBuilder<> &builder,                                                                         //
    llvm::Function *parent,                                                                             //
    const Scope *scope,                                                                                 //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                                   //
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                      //
    const InitializerNode *initializer                                                                  //
) {
    // Check if its a data initializer
    if (initializer->is_data) {
        if (!std::holds_alternative<std::shared_ptr<Type>>(initializer->type)) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        // Check if the initializer data type is even present
        if (data_nodes.find(std::get<std::shared_ptr<Type>>(initializer->type)->to_string()) == data_nodes.end()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        std::vector<llvm::Value *> return_values;
        for (const auto &expression : initializer->args) {
            auto expr_result = generate_expression(builder, parent, scope, allocations, garbage, expr_depth + 1, expression.get());
            if (!expr_result.has_value()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            // For initializers, we need pure single-value types
            if (expr_result.value().size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return_values.emplace_back(expr_result.value().at(0));
        }
        return return_values;
    }
    return std::nullopt;
}

llvm::Value *Generator::Expression::generate_data_access(             //
    llvm::IRBuilder<> &builder,                                       //
    const Scope *scope,                                               //
    std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const DataAccessNode *data_access                                 //
) {
    // First, get the alloca instance of the given data variable
    const unsigned int var_decl_scope = std::get<1>(scope->variables.at(data_access->var_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + data_access->var_name;
    llvm::Value *var_alloca = allocations.at(var_name);

    llvm::Type *data_type;
    if (data_access->data_type->to_string() == "str" && data_access->field_name == "length") {
        data_type = IR::get_type(Type::get_simple_type("str_var")).first;
        var_alloca = builder.CreateLoad(data_type->getPointerTo(), var_alloca, data_access->var_name + "_str_val");
    } else {
        data_type = IR::get_type(data_access->data_type).first;
    }

    llvm::Value *value_ptr = builder.CreateStructGEP(data_type, var_alloca, data_access->field_id);
    llvm::LoadInst *loaded_value = builder.CreateLoad(                          //
        IR::get_type(std::get<std::shared_ptr<Type>>(data_access->type)).first, //
        value_ptr,                                                              //
        data_access->var_name + "_" + data_access->field_name + "_val"          //
    );
    return loaded_value;
}

Generator::group_mapping Generator::Expression::generate_grouped_data_access( //
    llvm::IRBuilder<> &builder,                                               //
    const Scope *scope,                                                       //
    std::unordered_map<std::string, llvm::Value *const> &allocations,         //
    const GroupedDataAccessNode *grouped_data_access                          //
) {
    // First, get the alloca instance of the given data variable
    const unsigned int var_decl_scope = std::get<1>(scope->variables.at(grouped_data_access->var_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + grouped_data_access->var_name;
    llvm::Value *const var_alloca = allocations.at(var_name);

    llvm::Type *data_type = IR::get_type(grouped_data_access->data_type).first;
    assert(std::holds_alternative<std::vector<std::shared_ptr<Type>>>(grouped_data_access->type));
    std::vector<std::shared_ptr<Type>> group_types = std::get<std::vector<std::shared_ptr<Type>>>(grouped_data_access->type);

    std::vector<llvm::Value *> return_values;
    return_values.reserve(group_types.size());
    for (size_t i = 0; i < grouped_data_access->field_names.size(); i++) {
        llvm::Value *value_ptr = builder.CreateStructGEP(data_type, var_alloca, grouped_data_access->field_ids.at(i));
        llvm::LoadInst *loaded_value = builder.CreateLoad(                                        //
            IR::get_type(group_types.at(i)).first,                                                //
            value_ptr,                                                                            //
            grouped_data_access->var_name + "_" + grouped_data_access->field_names.at(i) + "_val" //
        );
        return_values.push_back(loaded_value);
    }
    return return_values;
}

Generator::group_mapping Generator::Expression::generate_type_cast(                                     //
    llvm::IRBuilder<> &builder,                                                                         //
    llvm::Function *parent,                                                                             //
    const Scope *scope,                                                                                 //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                                   //
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                      //
    const TypeCastNode *type_cast_node                                                                  //
) {
    // First, generate the expression
    std::vector<llvm::Value *> expr =
        generate_expression(builder, parent, scope, allocations, garbage, expr_depth + 1, type_cast_node->expr.get()).value();
    // Then, make sure that the expressions types are "normal" strings
    if (std::holds_alternative<std::vector<std::shared_ptr<Type>>>(type_cast_node->expr->type)) {
        std::vector<std::shared_ptr<Type>> &types = std::get<std::vector<std::shared_ptr<Type>>>(type_cast_node->expr->type);
        if (types.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        std::shared_ptr<Type> single_type = types.at(0);
        types.clear();
        type_cast_node->expr->type = single_type;
    }
    const std::shared_ptr<Type> from_type = std::get<std::shared_ptr<Type>>(type_cast_node->expr->type);
    std::shared_ptr<Type> to_type;
    if (std::holds_alternative<std::vector<std::shared_ptr<Type>>>(type_cast_node->type)) {
        const std::vector<std::shared_ptr<Type>> &types = std::get<std::vector<std::shared_ptr<Type>>>(type_cast_node->type);
        if (types.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        to_type = types.at(0);
    } else {
        to_type = std::get<std::shared_ptr<Type>>(type_cast_node->type);
    }
    // Lastly, check if the expression is castable, and if it is generate the type cast
    for (size_t i = 0; i < expr.size(); i++) {
        expr.at(i) = generate_type_cast(builder, expr.at(i), from_type, to_type);
    }
    return expr;
}

llvm::Value *Generator::Expression::generate_type_cast( //
    llvm::IRBuilder<> &builder,                         //
    llvm::Value *expr,                                  //
    const std::shared_ptr<Type> &from_type,             //
    const std::shared_ptr<Type> &to_type                //
) {
    const std::string from_type_str = from_type->to_string();
    const std::string to_type_str = to_type->to_string();
    if (from_type == to_type) {
        return expr;
    } else if (from_type_str == "i32") {
        if (to_type_str == "str") {
            return builder.CreateCall(TypeCast::typecast_functions.at("i32_to_str"), {expr}, "i32_to_str_res");
        }
        if (to_type_str == "u32") {
            return TypeCast::i32_to_u32(builder, expr);
        }
        if (to_type_str == "i64") {
            return TypeCast::i32_to_i64(builder, expr);
        }
        if (to_type_str == "u64") {
            return TypeCast::i32_to_u64(builder, expr);
        }
        if (to_type_str == "f32") {
            return TypeCast::i32_to_f32(builder, expr);
        }
        if (to_type_str == "f64") {
            return TypeCast::i32_to_f64(builder, expr);
        }
    } else if (from_type_str == "u32") {
        if (to_type_str == "str") {
            return builder.CreateCall(TypeCast::typecast_functions.at("u32_to_str"), {expr}, "u32_to_str_res");
        }
        if (to_type_str == "i32") {
            return TypeCast::u32_to_i32(builder, expr);
        }
        if (to_type_str == "i64") {
            return TypeCast::u32_to_i64(builder, expr);
        }
        if (to_type_str == "u64") {
            return TypeCast::u32_to_u64(builder, expr);
        }
        if (to_type_str == "f32") {
            return TypeCast::u32_to_f32(builder, expr);
        }
        if (to_type_str == "f64") {
            return TypeCast::u32_to_f64(builder, expr);
        }
    } else if (from_type_str == "i64") {
        if (to_type_str == "str") {
            return builder.CreateCall(TypeCast::typecast_functions.at("i64_to_str"), {expr}, "i64_to_str_res");
        }
        if (to_type_str == "i32") {
            return TypeCast::i64_to_i32(builder, expr);
        }
        if (to_type_str == "u32") {
            return TypeCast::i64_to_u32(builder, expr);
        }
        if (to_type_str == "u64") {
            return TypeCast::i64_to_u64(builder, expr);
        }
        if (to_type_str == "f32") {
            return TypeCast::i64_to_f32(builder, expr);
        }
        if (to_type_str == "f64") {
            return TypeCast::i64_to_f64(builder, expr);
        }
    } else if (from_type_str == "u64") {
        if (to_type_str == "str") {
            return builder.CreateCall(TypeCast::typecast_functions.at("u64_to_str"), {expr}, "u64_to_str_res");
        }
        if (to_type_str == "i32") {
            return TypeCast::u64_to_i32(builder, expr);
        }
        if (to_type_str == "u32") {
            return TypeCast::u64_to_u32(builder, expr);
        }
        if (to_type_str == "i64") {
            return TypeCast::u64_to_i64(builder, expr);
        }
        if (to_type_str == "f32") {
            return TypeCast::u64_to_f32(builder, expr);
        }
        if (to_type_str == "f64") {
            return TypeCast::u64_to_f64(builder, expr);
        }
    } else if (from_type_str == "f32") {
        if (to_type_str == "str") {
            return builder.CreateCall(TypeCast::typecast_functions.at("f32_to_str"), {expr}, "f32_to_str_res");
        }
        if (to_type_str == "i32") {
            return TypeCast::f32_to_i32(builder, expr);
        }
        if (to_type_str == "u32") {
            return TypeCast::f32_to_u32(builder, expr);
        }
        if (to_type_str == "i64") {
            return TypeCast::f32_to_i64(builder, expr);
        }
        if (to_type_str == "u64") {
            return TypeCast::f32_to_u64(builder, expr);
        }
        if (to_type_str == "f64") {
            return TypeCast::f32_to_f64(builder, expr);
        }
    } else if (from_type_str == "f64") {
        if (to_type_str == "str") {
            return builder.CreateCall(TypeCast::typecast_functions.at("f64_to_str"), {expr}, "f64_to_str_res");
        }
        if (to_type_str == "i32") {
            return TypeCast::f64_to_i32(builder, expr);
        }
        if (to_type_str == "u32") {
            return TypeCast::f64_to_u32(builder, expr);
        }
        if (to_type_str == "i64") {
            return TypeCast::f64_to_i64(builder, expr);
        }
        if (to_type_str == "u64") {
            return TypeCast::f64_to_u64(builder, expr);
        }
        if (to_type_str == "f32") {
            return TypeCast::f64_to_f32(builder, expr);
        }
    } else if (from_type_str == "bool") {
        if (to_type_str == "str") {
            return builder.CreateCall(TypeCast::typecast_functions.at("bool_to_str"), {expr}, "bool_to_str_res");
        }
    }
    std::cout << "FROM_TYPE: " << from_type_str << ", TO_TYPE: " << to_type_str << std::endl;
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}

Generator::group_mapping Generator::Expression::generate_unary_op_expression(                           //
    llvm::IRBuilder<> &builder,                                                                         //
    llvm::Function *parent,                                                                             //
    const Scope *scope,                                                                                 //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                                   //
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                      //
    const UnaryOpExpression *unary_op                                                                   //
) {
    const ExpressionNode *expression = unary_op->operand.get();
    std::vector<llvm::Value *> operand =
        generate_expression(builder, parent, scope, allocations, garbage, expr_depth + 1, expression).value();
    for (size_t i = 0; i < operand.size(); i++) {
        const std::string &expression_type = std::get<std::shared_ptr<Type>>(expression->type)->to_string();
        switch (unary_op->operator_token) {
            default:
                // Unknown unary operator
                THROW_BASIC_ERR(ERR_GENERATING);
                break;
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
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("i32_safe_add"), {operand.at(i), one}, "safe_add_res" //
                          );
                } else if (expression_type == "i64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateAdd(operand.at(i), one, "add_res")
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("i64_safe_add"), {operand.at(i), one}, "safe_add_res" //
                          );
                } else if (expression_type == "u32") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateAdd(operand.at(i), one, "add_res")
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("u32_safe_add"), {operand.at(0), one}, "safe_add_res" //
                          );
                } else if (expression_type == "u64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateAdd(operand.at(i), one, "add_res")
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("u64_safe_add"), {operand.at(0), one}, "safe_add_res" //
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
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("i32_safe_sub"), {operand.at(i), one}, "safe_sub_res" //
                          );
                } else if (expression_type == "i64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateSub(operand.at(i), one, "sub_res")
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("i64_safe_sub"), {operand.at(i), one}, "safe_sub_res" //
                          );
                } else if (expression_type == "u32") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateSub(operand.at(i), one, "sub_res")
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("u32_safe_sub"), {operand.at(0), one}, "safe_sub_res" //
                          );
                } else if (expression_type == "u64") {
                    llvm::Value *one = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
                    operand.at(i) = overflow_mode == ArithmeticOverflowMode::UNSAFE
                        ? builder.CreateSub(operand.at(i), one, "sub_res")
                        : builder.CreateCall(                                                                           //
                              Arithmetic::arithmetic_functions.at("u64_safe_sub"), {operand.at(0), one}, "safe_sub_res" //
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
        }
    }
    return operand;
}

Generator::group_mapping Generator::Expression::generate_binary_op(                                     //
    llvm::IRBuilder<> &builder,                                                                         //
    llvm::Function *parent,                                                                             //
    const Scope *scope,                                                                                 //
    std::unordered_map<std::string, llvm::Value *const> &allocations,                                   //
    std::unordered_map<unsigned int, std::vector<std::pair<std::string, llvm::Value *const>>> &garbage, //
    const unsigned int expr_depth,                                                                      //
    const BinaryOpNode *bin_op_node                                                                     //
) {
    auto lhs_maybe = generate_expression(builder, parent, scope, allocations, garbage, expr_depth + 1, bin_op_node->left.get());
    if (!lhs_maybe.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    std::vector<llvm::Value *> lhs = lhs_maybe.value();
    auto rhs_maybe = generate_expression(builder, parent, scope, allocations, garbage, expr_depth + 1, bin_op_node->right.get());
    if (!rhs_maybe.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    std::vector<llvm::Value *> rhs = rhs_maybe.value();
    assert(lhs.size() == rhs.size());
    std::vector<llvm::Value *> return_value;

    for (size_t i = 0; i < lhs.size(); i++) {
        const std::string type = std::holds_alternative<std::shared_ptr<Type>>(bin_op_node->left->type)
            ? std::get<std::shared_ptr<Type>>(bin_op_node->left->type)->to_string()
            : std::get<std::vector<std::shared_ptr<Type>>>(bin_op_node->left->type).at(i)->to_string();
        switch (bin_op_node->operator_token) {
            default:
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            case TOK_PLUS:
                if (type == "i32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateAdd(lhs.at(i), rhs.at(i), "add_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("i32_safe_add"), {lhs.at(i), rhs.at(i)}, "safe_add_res") //
                    );
                } else if (type == "i64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateAdd(lhs.at(i), rhs.at(i), "add_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("i64_safe_add"), {lhs.at(i), rhs.at(i)}, "safe_add_res") //
                    );
                } else if (type == "u32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateAdd(lhs.at(i), rhs.at(i), "add_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("u32_safe_add"), {lhs.at(i), rhs.at(i)}, "safe_add_res") //
                    );
                } else if (type == "u64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateAdd(lhs.at(i), rhs.at(i), "add_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("u64_safe_add"), {lhs.at(i), rhs.at(i)}, "safe_add_res") //
                    );
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFAdd(lhs.at(i), rhs.at(i), "faddtmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else if (type == "str") {
                    return_value.emplace_back(String::generate_string_addition(builder, scope, allocations, garbage, expr_depth + 1, //
                        lhs.at(i), bin_op_node->left.get(),                                                                          //
                        rhs.at(i), bin_op_node->right.get(),                                                                         //
                        bin_op_node->is_shorthand)                                                                                   //
                    );
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_MINUS:
                if (type == "i32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateSub(lhs.at(i), rhs.at(i), "sub_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("i32_safe_sub"), {lhs.at(i), rhs.at(i)}, "safe_sub_res") //
                    );
                } else if (type == "i64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateSub(lhs.at(i), rhs.at(i), "sub_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("i64_safe_sub"), {lhs.at(i), rhs.at(i)}, "safe_sub_res") //
                    );
                } else if (type == "u32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE //
                            ? builder.CreateSub(lhs.at(i), rhs.at(i), "sub_res")
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("u32_safe_sub"), {lhs.at(i), rhs.at(i)}, "safe_sub_res") //
                    );
                } else if (type == "u64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateSub(lhs.at(i), rhs.at(i), "sub_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("u64_safe_sub"), {lhs.at(i), rhs.at(i)}, "safe_sub_res") //
                    );
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFSub(lhs.at(i), rhs.at(i), "fsubtmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_MULT:
                if (type == "i32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateMul(lhs.at(i), rhs.at(i), "mul_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("i32_safe_mul"), {lhs.at(i), rhs.at(i)}, "safe_mul_res") //
                    );
                } else if (type == "i64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateMul(lhs.at(i), rhs.at(i), "mul_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("i64_safe_mul"), {lhs.at(i), rhs.at(i)}, "safe_mul_res") //
                    );
                } else if (type == "u32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateMul(lhs.at(i), rhs.at(i), "mul_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("u32_safe_mul"), {lhs.at(i), rhs.at(i)}, "safe_mul_res") //
                    );
                } else if (type == "u64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                  //
                            ? builder.CreateMul(lhs.at(i), rhs.at(i), "mul_res")                                               //
                            : builder.CreateCall(                                                                              //
                                  Arithmetic::arithmetic_functions.at("u64_safe_mul"), {lhs.at(i), rhs.at(i)}, "safe_mul_res") //
                    );
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFMul(lhs.at(i), rhs.at(i), "fmultmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_DIV:
                if (type == "i32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                   //
                            ? builder.CreateSDiv(lhs.at(i), rhs.at(i), "sdiv_res")                                              //
                            : builder.CreateCall(                                                                               //
                                  Arithmetic::arithmetic_functions.at("i32_safe_div"), {lhs.at(i), rhs.at(i)}, "safe_sdiv_res") //
                    );
                } else if (type == "i64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                   //
                            ? builder.CreateSDiv(lhs.at(i), rhs.at(i), "sdiv_res")                                              //
                            : builder.CreateCall(                                                                               //
                                  Arithmetic::arithmetic_functions.at("i64_safe_div"), {lhs.at(i), rhs.at(i)}, "safe_sdiv_res") //
                    );
                } else if (type == "u32") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                   //
                            ? builder.CreateUDiv(lhs.at(i), rhs.at(i), "udiv_res")                                              //
                            : builder.CreateCall(                                                                               //
                                  Arithmetic::arithmetic_functions.at("u32_safe_div"), {lhs.at(i), rhs.at(i)}, "safe_udiv_res") //
                    );
                } else if (type == "u64") {
                    return_value.emplace_back(overflow_mode == ArithmeticOverflowMode::UNSAFE                                   //
                            ? builder.CreateUDiv(lhs.at(i), rhs.at(i), "udiv_res")                                              //
                            : builder.CreateCall(                                                                               //
                                  Arithmetic::arithmetic_functions.at("u64_safe_div"), {lhs.at(i), rhs.at(i)}, "safe_udiv_res") //
                    );
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFDiv(lhs.at(i), rhs.at(i), "fdivtmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_POW:
                return_value.emplace_back(IR::generate_pow_instruction(builder, parent, lhs.at(i), rhs.at(i)));
                break;
            case TOK_LESS:
                if (type == "i32" || type == "i64") {
                    return_value.emplace_back(builder.CreateICmpSLT(lhs.at(i), rhs.at(i), "icmptmp"));
                } else if (type == "u32" || type == "u64") {
                    return_value.emplace_back(builder.CreateICmpULT(lhs.at(i), rhs.at(i), "ucmptmp"));
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFCmpOLT(lhs.at(i), rhs.at(i), "fcmptmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else if (type == "str") {
                    return_value.emplace_back(Logical::generate_string_cmp_lt(                            //
                        builder, lhs.at(i), bin_op_node->left.get(), rhs.at(i), bin_op_node->right.get()) //
                    );
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_GREATER:
                if (type == "i32" || type == "i64") {
                    return_value.emplace_back(builder.CreateICmpSGT(lhs.at(i), rhs.at(i), "icmptmp"));
                } else if (type == "u32" || type == "u64") {
                    return_value.emplace_back(builder.CreateICmpUGT(lhs.at(i), rhs.at(i), "ucmptmp"));
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFCmpOGT(lhs.at(i), rhs.at(i), "fcmptmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else if (type == "str") {
                    return_value.emplace_back(Logical::generate_string_cmp_gt(                            //
                        builder, lhs.at(i), bin_op_node->left.get(), rhs.at(i), bin_op_node->right.get()) //
                    );
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_LESS_EQUAL:
                if (type == "i32" || type == "i64") {
                    return_value.emplace_back(builder.CreateICmpSLE(lhs.at(i), rhs.at(i), "icmptmp"));
                } else if (type == "u32" || type == "u64") {
                    return_value.emplace_back(builder.CreateICmpULE(lhs.at(i), rhs.at(i), "ucmptmp"));
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFCmpOLE(lhs.at(i), rhs.at(i), "fcmptmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else if (type == "str") {
                    return_value.emplace_back(Logical::generate_string_cmp_le(                            //
                        builder, lhs.at(i), bin_op_node->left.get(), rhs.at(i), bin_op_node->right.get()) //
                    );
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_GREATER_EQUAL:
                if (type == "i32" || type == "i64") {
                    return_value.emplace_back(builder.CreateICmpSGE(lhs.at(i), rhs.at(i), "icmptmp"));
                } else if (type == "u32" || type == "u64") {
                    return_value.emplace_back(builder.CreateICmpUGE(lhs.at(i), rhs.at(i), "ucmptmp"));
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFCmpOGE(lhs.at(i), rhs.at(i), "fcmptmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else if (type == "str") {
                    return_value.emplace_back(Logical::generate_string_cmp_ge(                            //
                        builder, lhs.at(i), bin_op_node->left.get(), rhs.at(i), bin_op_node->right.get()) //
                    );
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_EQUAL_EQUAL:
                if (type == "i32" || type == "i64") {
                    return_value.emplace_back(builder.CreateICmpEQ(lhs.at(i), rhs.at(i), "icmptmp"));
                } else if (type == "u32" || type == "u64") {
                    return_value.emplace_back(builder.CreateICmpEQ(lhs.at(i), rhs.at(i), "ucmptmp"));
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFCmpOEQ(lhs.at(i), rhs.at(i), "fcmptmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else if (type == "str") {
                    return_value.emplace_back(Logical::generate_string_cmp_eq(                            //
                        builder, lhs.at(i), bin_op_node->left.get(), rhs.at(i), bin_op_node->right.get()) //
                    );
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_NOT_EQUAL:
                if (type == "i32" || type == "i64") {
                    return_value.emplace_back(builder.CreateICmpNE(lhs.at(i), rhs.at(i), "icmptmp"));
                } else if (type == "u32" || type == "u64") {
                    return_value.emplace_back(builder.CreateICmpNE(lhs.at(i), rhs.at(i), "ucmptmp"));
                } else if (type == "f32" || type == "f64") {
                    return_value.emplace_back(builder.CreateFCmpONE(lhs.at(i), rhs.at(i), "fcmptmp"));
                } else if (type == "flint") {
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    return std::nullopt;
                } else if (type == "str") {
                    return_value.emplace_back(Logical::generate_string_cmp_neq(                           //
                        builder, lhs.at(i), bin_op_node->left.get(), rhs.at(i), bin_op_node->right.get()) //
                    );
                } else {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                break;
            case TOK_AND:
                if (type != "bool") {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                return_value.emplace_back(builder.CreateLogicalAnd(lhs.at(i), rhs.at(i), "band"));
                break;
            case TOK_OR:
                if (type != "bool") {
                    THROW_BASIC_ERR(ERR_GENERATING);
                    return std::nullopt;
                }
                return_value.emplace_back(builder.CreateLogicalOr(lhs.at(i), rhs.at(i), "bor"));
                break;
        }
    }
    return return_value;
}
