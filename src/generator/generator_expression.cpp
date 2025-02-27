#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "parser/ast/expressions/call_node_expression.hpp"

llvm::Value *Generator::Expression::generate_expression(                   //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const ExpressionNode *expression_node                                  //
) {
    if (const auto *variable_node = dynamic_cast<const VariableNode *>(expression_node)) {
        return generate_variable(builder, parent, scope, allocations, variable_node);
    }
    if (const auto *unary_op_node = dynamic_cast<const UnaryOpNode *>(expression_node)) {
        return generate_unary_op(builder, parent, unary_op_node);
    }
    if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expression_node)) {
        return generate_literal(builder, parent, literal_node);
    }
    if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(expression_node)) {
        return generate_call(builder, parent, scope, allocations, call_node);
    }
    if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expression_node)) {
        return generate_binary_op(builder, parent, scope, allocations, binary_op_node);
    }
    if (const auto *type_cast_node = dynamic_cast<const TypeCastNode *>(expression_node)) {
        return generate_type_cast(builder, parent, scope, allocations, type_cast_node);
    }
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}

llvm::Value *Generator::Expression::generate_literal(llvm::IRBuilder<> &builder, llvm::Function *parent, const LiteralNode *literal_node) {
    if (std::holds_alternative<int>(literal_node->value)) {
        return llvm::ConstantInt::get(                    //
            llvm::Type::getInt32Ty(parent->getContext()), //
            std::get<int>(literal_node->value)            //
        );
    }
    if (std::holds_alternative<float>(literal_node->value)) {
        return llvm::ConstantFP::get(                     //
            llvm::Type::getFloatTy(parent->getContext()), //
            std::get<float>(literal_node->value)          //
        );
    }
    if (std::holds_alternative<std::string>(literal_node->value)) {
        // Get the constant string value
        std::string str = std::get<std::string>(literal_node->value);
        std::string processed_str;
        for (unsigned int i = 0; i < str.length(); i++) {
            if (str[i] == '\\' && i + 1 < str.length()) {
                switch (str[i + 1]) {
                    case 'n':
                        processed_str += '\n';
                        break;
                    case 't':
                        processed_str += '\t';
                        break;
                    case 'r':
                        processed_str += '\r';
                        break;
                    case '\\':
                        processed_str += '\\';
                        break;
                    case '0':
                        processed_str += '\0';
                        break;
                }
                i++; // Skip the next character
            } else {
                processed_str += str[i];
            }
        }
        str = processed_str;

        // Create array type for the string (including null terminator)
        llvm::ArrayType *str_type = llvm::ArrayType::get( //
            builder.getInt8Ty(),                          //
            str.length() + 1                              // +1 for null terminator
        );
        // Allocate space for the string data on the stack
        llvm::AllocaInst *str_buf = builder.CreateAlloca(str_type, nullptr, "str_buf");
        // Create the constant string data
        llvm::Constant *str_constant = llvm::ConstantDataArray::getString(parent->getContext(), str);
        // Store the string data in the buffer
        builder.CreateStore(str_constant, str_buf);
        // Return the buffer pointer
        return str_buf;
    }
    if (std::holds_alternative<bool>(literal_node->value)) {
        return llvm::ConstantInt::get(                                 //
            llvm::Type::getInt1Ty(parent->getContext()),               //
            static_cast<uint64_t>(std::get<bool>(literal_node->value)) //
        );
    }
    if (std::holds_alternative<char>(literal_node->value)) {
        return llvm::ConstantInt::get(                   //
            llvm::Type::getInt8Ty(parent->getContext()), //
            std::get<char>(literal_node->value)          //
        );
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return nullptr;
}

llvm::Value *Generator::Expression::generate_variable(                     //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const VariableNode *variable_node                                      //
) {
    if (variable_node == nullptr) {
        // Error: Null Node
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }

    // First, check if this is a function parameter
    for (auto &arg : parent->args()) {
        if (arg.getName() == variable_node->name) {
            // If it's a parameter, return it directly - no need to load
            return &arg;
        }
    }

    // If not a parameter, handle as local variable
    if (scope->variable_types.find(variable_node->name) == scope->variable_types.end()) {
        // Error: Undeclared Variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }
    const unsigned int variable_decl_scope = scope->variable_types.at(variable_node->name).second;
    llvm::AllocaInst *const variable = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);

    // Get the type that the pointer points to
    llvm::Type *value_type = IR::get_type_from_str(parent->getContext(), variable_node->type);

    // Load the variable's value if it's a pointer
    llvm::LoadInst *load = builder.CreateLoad(value_type, variable, variable_node->name + "_val");
    load->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Load val of var '" + variable_node->name + "'")));

    return load;
}

llvm::Value *Generator::Expression::generate_call(                         //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const CallNodeBase *call_node                                          //
) {
    // Get the arguments
    std::vector<llvm::Value *> args;
    args.reserve(call_node->arguments.size());
    for (const auto &arg : call_node->arguments) {
        args.emplace_back(generate_expression(builder, parent, scope, allocations, arg.get()));
    }

    // Check if it is a builtin function and call it
    if (builtin_functions.find(call_node->function_name) != builtin_functions.end()) {
        llvm::Function *builtin_function = builtins.at(builtin_functions.at(call_node->function_name));
        if (builtin_function == nullptr) {
            // Function has not been generated yet, but it should have been
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return nullptr;
        }
        // Call the builtin function 'print'
        if (call_node->function_name == "print" && call_node->arguments.size() == 1 &&
            print_functions.find(call_node->arguments.at(0)->type) != print_functions.end()) {
            return builder.CreateCall(                             //
                print_functions[call_node->arguments.at(0)->type], //
                args                                               //
            );
        }
        return builder.CreateCall(builtin_function, args);
    }

    // Get the function definition from any module
    auto [func_decl_res, is_call_extern] = Function::get_function_definition(parent, call_node);
    if (!func_decl_res.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }
    llvm::Function *func_decl = func_decl_res.value();
    if (func_decl == nullptr) {
        // Is a builtin function, but the code block above should have been executed in that case, because we are here, something went wrong
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }

    // Create the call instruction using the original declaration
    llvm::CallInst *call = builder.CreateCall(                                  //
        func_decl,                                                              //
        args,                                                                   //
        call_node->function_name + std::to_string(call_node->call_id) + "_call" //
    );
    call->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Call of function '" + call_node->function_name + "'")));

    // Store results immideately after call
    const std::string call_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
    const std::string call_err_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";

    // Store function result
    llvm::AllocaInst *res_var = allocations.at(call_ret_name);
    builder.CreateStore(call, res_var);

    // Extract and store error value
    llvm::Value *err_ptr = builder.CreateStructGEP(                                //
        res_var->getAllocatedType(),                                               //
        res_var,                                                                   //
        0,                                                                         //
        call_node->function_name + std::to_string(call_node->call_id) + "_err_ptr" //
    );
    llvm::Value *err_val = builder.CreateLoad(                                     //
        llvm::Type::getInt32Ty(builder.getContext()),                              //
        err_ptr,                                                                   //
        call_node->function_name + std::to_string(call_node->call_id) + "_err_val" //
    );
    llvm::AllocaInst *err_var = allocations.at(call_err_name);
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

    return call;
}

void Generator::Expression::generate_rethrow(                              //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const CallNodeBase *call_node                                          //
) {
    const std::string err_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    llvm::AllocaInst *const err_var = allocations.at(err_ret_name);

    // Load error value
    llvm::LoadInst *err_val = builder.CreateLoad(                                    //
        llvm::Type::getInt32Ty(builder.getContext()),                                //
        err_var,                                                                     //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_val" //
    );
    err_val->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Load err val of call '" + call_node->function_name + "::" + std::to_string(call_node->call_id) + "'")));

    llvm::BasicBlock *last_block = &parent->back();
    llvm::BasicBlock *first_block = &parent->front();
    // Create basic block for the catch block
    llvm::BasicBlock *current_block = builder.GetInsertBlock();

    // Check if the current block is the last block, if it is not, insert right after the current block
    bool will_insert_after = current_block == last_block || current_block != first_block;
    llvm::BasicBlock *insert_before = will_insert_after ? (current_block->getNextNode()) : current_block;

    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(                           //
        builder.getContext(),                                                           //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_catch", //
        parent,                                                                         //
        insert_before                                                                   //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(                           //
        builder.getContext(),                                                           //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_merge", //
        parent,                                                                         //
        insert_before                                                                   //
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
            llvm::MDNode::get(parent->getContext(),
                llvm::MDString::get(parent->getContext(),
                    "Create default struct of type '" + throw_struct_type->getName().str() + "' except first value")));

        // Create the pointer to the error value (the 0th index of the struct)
        llvm::Value *error_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, 0, "err_ptr");
        // Store the error value in the struct
        builder.CreateStore(err_val, error_ptr);

        // Generate the throw (return) instruction with the evaluated value
        llvm::LoadInst *throw_struct_val = builder.CreateLoad(throw_struct_type, throw_struct, "throw_val");
        throw_struct_val->setMetadata("comment",
            llvm::MDNode::get(parent->getContext(),
                llvm::MDString::get(parent->getContext(),
                    "Load allocated throw struct of type '" + throw_struct_type->getName().str() + "'")));
        builder.CreateRet(throw_struct_val);
    }

    // Add branch to the merge block from the catch block if it does not contain a terminator (return or throw)
    if (catch_block->getTerminator() == nullptr) {
        builder.CreateBr(merge_block);
    }

    builder.SetInsertPoint(merge_block);
}

llvm::Value *Generator::Expression::generate_type_cast(                    //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const TypeCastNode *type_cast_node                                     //
) {
    // First, generate the expression
    llvm::Value *expr = generate_expression(builder, parent, scope, allocations, type_cast_node->expr.get());
    // Then, check if the expression is castable
    const std::string &from_type = type_cast_node->expr->type;
    const std::string &to_type = type_cast_node->type;
    if (from_type == to_type) {
        return expr;
    } else if (from_type == "i32") {
        if (to_type == "u32") {
            return TypeCast::i32_to_u32(builder, expr);
        }
        if (to_type == "i64") {
            return TypeCast::i32_to_i64(builder, expr);
        }
        if (to_type == "u64") {
            return TypeCast::i32_to_u64(builder, expr);
        }
        if (to_type == "f32") {
            return TypeCast::i32_to_f32(builder, expr);
        }
        if (to_type == "f64") {
            return TypeCast::i32_to_f64(builder, expr);
        }
    } else if (from_type == "u32") {
        if (to_type == "i32") {
            return TypeCast::u32_to_i32(builder, expr);
        }
        if (to_type == "i64") {
            return TypeCast::u32_to_i64(builder, expr);
        }
        if (to_type == "u64") {
            return TypeCast::u32_to_u64(builder, expr);
        }
        if (to_type == "f32") {
            return TypeCast::u32_to_f32(builder, expr);
        }
        if (to_type == "f64") {
            return TypeCast::u32_to_f64(builder, expr);
        }
    } else if (from_type == "i64") {
        if (to_type == "i32") {
            return TypeCast::i64_to_i32(builder, expr);
        }
        if (to_type == "u32") {
            return TypeCast::i64_to_u32(builder, expr);
        }
        if (to_type == "u64") {
            return TypeCast::i64_to_u64(builder, expr);
        }
        if (to_type == "f32") {
            return TypeCast::i64_to_f32(builder, expr);
        }
        if (to_type == "f64") {
            return TypeCast::i64_to_f64(builder, expr);
        }
    } else if (from_type == "u64") {
        if (to_type == "i32") {
            return TypeCast::u64_to_i32(builder, expr);
        }
        if (to_type == "u32") {
            return TypeCast::u64_to_u32(builder, expr);
        }
        if (to_type == "i64") {
            return TypeCast::u64_to_i64(builder, expr);
        }
        if (to_type == "f32") {
            return TypeCast::u64_to_f32(builder, expr);
        }
        if (to_type == "f64") {
            return TypeCast::u64_to_f64(builder, expr);
        }
    } else if (from_type == "f32") {
        if (to_type == "i32") {
            return TypeCast::f32_to_i32(builder, expr);
        }
        if (to_type == "u32") {
            return TypeCast::f32_to_u32(builder, expr);
        }
        if (to_type == "i64") {
            return TypeCast::f32_to_i64(builder, expr);
        }
        if (to_type == "u64") {
            return TypeCast::f32_to_u64(builder, expr);
        }
        if (to_type == "f64") {
            return TypeCast::f32_to_f64(builder, expr);
        }
    } else if (from_type == "f64") {
        if (to_type == "i32") {
            return TypeCast::f64_to_i32(builder, expr);
        }
        if (to_type == "u32") {
            return TypeCast::f64_to_u32(builder, expr);
        }
        if (to_type == "i64") {
            return TypeCast::f64_to_i64(builder, expr);
        }
        if (to_type == "u64") {
            return TypeCast::f64_to_u64(builder, expr);
        }
        if (to_type == "f32") {
            return TypeCast::f64_to_f32(builder, expr);
        }
    }
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}

llvm::Value *Generator::Expression::generate_unary_op( //
    llvm::IRBuilder<> &builder,                        //
    llvm::Function *parent,                            //
    const UnaryOpNode *unary_op_node                   //
) {
    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
    return nullptr;
}

llvm::Value *Generator::Expression::generate_binary_op(                    //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    const Scope *scope,                                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    const BinaryOpNode *bin_op_node                                        //
) {
    llvm::Value *lhs = generate_expression(builder, parent, scope, allocations, bin_op_node->left.get());
    llvm::Value *rhs = generate_expression(builder, parent, scope, allocations, bin_op_node->right.get());
    const std::string_view type = bin_op_node->left->type;
    switch (bin_op_node->operator_token) {
        default:
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        case TOK_PLUS:
            if (type == "i32" || type == "i64") {
                return Arithmetic::int_safe_add(builder, lhs, rhs);
            } else if (type == "u32" || type == "u64") {
                return Arithmetic::uint_safe_add(builder, lhs, rhs);
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFAdd(lhs, rhs, "faddtmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_MINUS:
            if (type == "i32" || type == "i64") {
                return Arithmetic::int_safe_sub(builder, lhs, rhs);
            } else if (type == "u32" || type == "u64") {
                return Arithmetic::uint_safe_sub(builder, lhs, rhs);
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFSub(lhs, rhs, "fsubtmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_MULT:
            if (type == "i32" || type == "i64") {
                return Arithmetic::int_safe_mul(builder, lhs, rhs);
            } else if (type == "u32" || type == "u64") {
                return Arithmetic::uint_safe_mul(builder, lhs, rhs);
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFMul(lhs, rhs, "fmultmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_DIV:
            if (type == "i32" || type == "i64") {
                return Arithmetic::int_safe_div(builder, lhs, rhs);
            } else if (type == "u32" || type == "u64") {
                return Arithmetic::uint_safe_div(builder, lhs, rhs);
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFDiv(lhs, rhs, "fdivtmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_SQUARE:
            return IR::generate_pow_instruction(builder, parent, lhs, rhs);
        case TOK_LESS:
            if (type == "i32" || type == "i64") {
                return builder.CreateICmpSLT(lhs, rhs, "icmptmp");
            } else if (type == "u32" || type == "u64") {
                return builder.CreateICmpULT(lhs, rhs, "ucmptmp");
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFCmpOLT(lhs, rhs, "fcmptmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_GREATER:
            if (type == "i32" || type == "i64") {
                return builder.CreateICmpSGT(lhs, rhs, "icmptmp");
            } else if (type == "u32" || type == "u64") {
                return builder.CreateICmpUGT(lhs, rhs, "ucmptmp");
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFCmpOGT(lhs, rhs, "fcmptmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_LESS_EQUAL:
            if (type == "i32" || type == "i64") {
                return builder.CreateICmpSLE(lhs, rhs, "icmptmp");
            } else if (type == "u32" || type == "u64") {
                return builder.CreateICmpULE(lhs, rhs, "ucmptmp");
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFCmpOLE(lhs, rhs, "fcmptmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_GREATER_EQUAL:
            if (type == "i32" || type == "i64") {
                return builder.CreateICmpSGE(lhs, rhs, "icmptmp");
            } else if (type == "u32" || type == "u64") {
                return builder.CreateICmpUGE(lhs, rhs, "ucmptmp");
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFCmpOGE(lhs, rhs, "fcmptmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_EQUAL_EQUAL:
            if (type == "i32" || type == "i64") {
                return builder.CreateICmpEQ(lhs, rhs, "icmptmp");
            } else if (type == "u32" || type == "u64") {
                return builder.CreateICmpEQ(lhs, rhs, "ucmptmp");
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFCmpOEQ(lhs, rhs, "fcmptmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
        case TOK_NOT_EQUAL:
            if (type == "i32" || type == "i64") {
                return builder.CreateICmpNE(lhs, rhs, "icmptmp");
            } else if (type == "u32" || type == "u64") {
                return builder.CreateICmpNE(lhs, rhs, "ucmptmp");
            } else if (type == "f32" || type == "f64") {
                return builder.CreateFCmpONE(lhs, rhs, "fcmptmp");
            } else if (type == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return nullptr;
            } else {
                THROW_BASIC_ERR(ERR_GENERATING);
                return nullptr;
            }
    }
}
