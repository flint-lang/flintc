#include "error/error.hpp"
#include "error/error_type.hpp"
#include "generator/generator.hpp"

#include "globals.hpp"
#include "lexer/builtins.hpp"
#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/parser.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/variant_type.hpp"

#include <llvm/IR/Instructions.h>
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
    if (const auto *variable_node = dynamic_cast<const VariableNode *>(expression_node)) {
        group_map.emplace_back(generate_variable(builder, ctx, variable_node, is_reference));
        return group_map;
    }
    if (is_reference) {
        // Only variables are allowed as references for now
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    if (const auto *unary_op_node = dynamic_cast<const UnaryOpExpression *>(expression_node)) {
        return generate_unary_op_expression(builder, ctx, garbage, expr_depth, unary_op_node);
    }
    if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expression_node)) {
        group_map.emplace_back(generate_literal(builder, literal_node));
        return group_map;
    }
    if (const auto *interpol_node = dynamic_cast<const StringInterpolationNode *>(expression_node)) {
        group_map.emplace_back(generate_string_interpolation(builder, ctx, garbage, expr_depth, interpol_node));
        return group_map;
    }
    if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(expression_node)) {
        return generate_call(builder, ctx, call_node);
    }
    if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expression_node)) {
        return generate_binary_op(builder, ctx, garbage, expr_depth, binary_op_node);
    }
    if (const auto *type_cast_node = dynamic_cast<const TypeCastNode *>(expression_node)) {
        return generate_type_cast(builder, ctx, garbage, expr_depth, type_cast_node);
    }
    if (const auto *group_node = dynamic_cast<const GroupExpressionNode *>(expression_node)) {
        return generate_group_expression(builder, ctx, garbage, expr_depth, group_node);
    }
    if (const auto *initializer = dynamic_cast<const InitializerNode *>(expression_node)) {
        return generate_initializer(builder, ctx, garbage, expr_depth, initializer);
    }
    if (const auto *switch_expression = dynamic_cast<const SwitchExpression *>(expression_node)) {
        return generate_switch_expression(builder, ctx, garbage, expr_depth, switch_expression);
    }
    if (const auto *data_access = dynamic_cast<const DataAccessNode *>(expression_node)) {
        return generate_data_access(builder, ctx, garbage, expr_depth, data_access);
    }
    if (const auto *grouped_data_access = dynamic_cast<const GroupedDataAccessNode *>(expression_node)) {
        return generate_grouped_data_access(builder, ctx, grouped_data_access);
    }
    if (const auto *array_initializer = dynamic_cast<const ArrayInitializerNode *>(expression_node)) {
        group_map.emplace_back(generate_array_initializer(builder, ctx, garbage, expr_depth, array_initializer));
        return group_map;
    }
    if (const auto *array_access = dynamic_cast<const ArrayAccessNode *>(expression_node)) {
        group_map.emplace_back(generate_array_access(builder, ctx, garbage, expr_depth, array_access));
        return group_map;
    }
    THROW_BASIC_ERR(ERR_GENERATING);
    return std::nullopt;
}

llvm::Value *Generator::Expression::generate_literal( //
    llvm::IRBuilder<> &builder,                       //
    const LiteralNode *literal_node                   //
) {
    if (std::holds_alternative<unsigned long>(literal_node->value)) {
        return llvm::ConstantInt::get(                    //
            llvm::Type::getInt64Ty(context),              //
            std::get<unsigned long>(literal_node->value), //
            false                                         //
        );
    }
    if (std::holds_alternative<long>(literal_node->value)) {
        return llvm::ConstantInt::get(           //
            llvm::Type::getInt64Ty(context),     //
            std::get<long>(literal_node->value), //
            true                                 //
        );
    }
    if (std::holds_alternative<unsigned int>(literal_node->value)) {
        return llvm::ConstantInt::get(                   //
            llvm::Type::getInt32Ty(context),             //
            std::get<unsigned int>(literal_node->value), //
            false                                        //
        );
    }
    if (std::holds_alternative<int>(literal_node->value)) {
        return llvm::ConstantInt::get(          //
            llvm::Type::getInt32Ty(context),    //
            std::get<int>(literal_node->value), //
            true                                //
        );
    }
    if (std::holds_alternative<double>(literal_node->value)) {
        return llvm::ConstantFP::get(             //
            llvm::Type::getDoubleTy(context),     //
            std::get<double>(literal_node->value) //
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
    if (std::holds_alternative<std::optional<void *>>(literal_node->value)) {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
    }
    THROW_BASIC_ERR(ERR_PARSING);
    return nullptr;
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
            // If it's a parameter, and its an mutable primitive type, we dont return it directly, toherwise we do
            if (ctx.scope->variables.find(arg.getName().str()) != ctx.scope->variables.end()) {
                auto var = ctx.scope->variables.at(arg.getName().str());
                if (primitives.find(std::get<0>(var)->to_string()) != primitives.end() && std::get<2>(var)) {
                    continue;
                }
            }
            return &arg;
        }
    }

    // If not a parameter, handle as local variable
    if (ctx.scope->variables.find(variable_node->name) == ctx.scope->variables.end()) {
        // Error: Undeclared Variable
        THROW_BASIC_ERR(ERR_GENERATING);
        return nullptr;
    }
    const unsigned int variable_decl_scope = std::get<1>(ctx.scope->variables.at(variable_node->name));
    llvm::Value *const variable = ctx.allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);
    if (is_reference) {
        return variable;
    }

    // Get the type that the pointer points to
    llvm::Type *value_type = IR::get_type(ctx.parent->getParent(), variable_node->type).first;

    // Load the variable's value if it's a pointer
    llvm::LoadInst *load = builder.CreateLoad(value_type, variable, variable_node->name + "_val");
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
        const std::string lit_string = std::get<std::string>(std::get<std::unique_ptr<LiteralNode>>(*it)->value);
        llvm::Value *lit_str = IR::generate_const_string(builder, lit_string);
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
        if (std::next(it) == interpol_node->string_content.end() && dynamic_cast<const VariableNode *>(expr) != nullptr) {
            return str_value;
        }
    }
    ++it;

    llvm::Function *add_str_lit = Module::String::string_manip_functions.at("add_str_lit");
    llvm::Function *add_str_str = Module::String::string_manip_functions.at("add_str_str");
    for (; it != interpol_node->string_content.end(); ++it) {
        if (std::holds_alternative<std::unique_ptr<LiteralNode>>(*it)) {
            const std::string lit_string = std::get<std::string>(std::get<std::unique_ptr<LiteralNode>>(*it)->value);
            llvm::Value *lit_str = IR::generate_const_string(builder, lit_string);
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

Generator::group_mapping Generator::Expression::generate_call( //
    llvm::IRBuilder<> &builder,                                //
    GenerationContext &ctx,                                    //
    const CallNodeBase *call_node                              //
) {
    // Get the arguments
    std::vector<llvm::Value *> args;
    args.reserve(call_node->arguments.size());
    garbage_type garbage;
    for (const auto &arg : call_node->arguments) {
        // Complex rguments of function calls are always passed as references, but if the data type is complex this "reference" is a pointer
        // to the actual data of the variable, not a pointer to its allocation. So, in this case we are not allowed to pass in any variable
        // as "reference" because then a double pointer is passed to the function where a single pointer is expected This behaviour should
        // only effect array types, as data and strings are handled differently
        bool is_reference = arg.second && dynamic_cast<const ArrayType *>(arg.first->type.get()) == nullptr;
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
        args.emplace_back(expr_val);
    }

    llvm::Function *func_decl = nullptr;
    enum class FunctionOrigin { INTERN, EXTERN, BUILTIN };
    FunctionOrigin function_origin = FunctionOrigin::INTERN;
    // First check which core modules have been imported
    auto builtin_function = Parser::get_builtin_function(call_node->function_name, ctx.imported_core_modules);
    if (builtin_function.has_value()) {
        std::vector<llvm::Value *> return_value;
        const std::string &module_name = std::get<0>(builtin_function.value());
        if (module_name == "print" && call_node->function_name == "print" && call_node->arguments.size() == 1 && //
            Module::Print::print_functions.find(call_node->arguments.front().first->type->to_string()) !=        //
                Module::Print::print_functions.end()                                                             //
        ) {
            // Call the builtin function 'print'
            const std::string type_str = call_node->arguments.front().first->type->to_string();
            return_value.emplace_back(builder.CreateCall(Module::Print::print_functions.at(type_str), args));
            if (!Statement::clear_garbage(builder, garbage)) {
                return std::nullopt;
            }
            return return_value;
        } else if (module_name == "read" && call_node->arguments.size() == 0 &&                               //
            Module::Read::read_functions.find(call_node->function_name) != Module::Read::read_functions.end() //
        ) {
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (std::get<2>(std::get<1>(builtin_function.value()).front())) {
                // Function returns error
                func_decl = Module::Read::read_functions.at(call_node->function_name);
                function_origin = FunctionOrigin::BUILTIN;
            } else {
                // Function does not return error
                func_decl = Module::Read::read_functions.at(call_node->function_name);
                return_value.emplace_back(builder.CreateCall(func_decl, args));
                return return_value;
            }
        } else if (module_name == "assert" && call_node->arguments.size() == 1 &&
            Module::Assert::assert_functions.find(call_node->function_name) != Module::Assert::assert_functions.end()) {
            func_decl = Module::Assert::assert_functions.at(call_node->function_name);
            function_origin = FunctionOrigin::BUILTIN;
        } else if (module_name == "filesystem" &&
            Module::FileSystem::fs_functions.find(call_node->function_name) != Module::FileSystem::fs_functions.end()) {
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (std::get<2>(std::get<1>(builtin_function.value()).front())) {
                // Function returns error
                func_decl = Module::FileSystem::fs_functions.at(call_node->function_name);
                function_origin = FunctionOrigin::BUILTIN;
            } else {
                // Function does not return error
                func_decl = Module::FileSystem::fs_functions.at(call_node->function_name);
                return_value.emplace_back(builder.CreateCall(func_decl, args));
                return return_value;
            }
        } else if (module_name == "env" && Module::Env::env_functions.find(call_node->function_name) != Module::Env::env_functions.end()) {
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (std::get<2>(std::get<1>(builtin_function.value()).front())) {
                // Function returns error
                func_decl = Module::Env::env_functions.at(call_node->function_name);
                function_origin = FunctionOrigin::BUILTIN;
            } else {
                // Function does not return error
                func_decl = Module::Env::env_functions.at(call_node->function_name);
                return_value.emplace_back(builder.CreateCall(func_decl, args));
                return return_value;
            }
        } else if (module_name == "system" &&
            Module::System::system_functions.find(call_node->function_name) != Module::System::system_functions.end()) {
            if (std::get<1>(builtin_function.value()).size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (std::get<2>(std::get<1>(builtin_function.value()).front())) {
                // Function returns error
                func_decl = Module::System::system_functions.at(call_node->function_name);
                function_origin = FunctionOrigin::BUILTIN;
            } else {
                // Function does not return error
                func_decl = Module::System::system_functions.at(call_node->function_name);
                return_value.emplace_back(builder.CreateCall(func_decl, args));
                return return_value;
            }
        } else {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
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
    llvm::Value *res_var = ctx.allocations.at(call_ret_name);
    builder.CreateStore(call, res_var);

    // Extract and store error value
    llvm::StructType *return_type = static_cast<llvm::StructType *>(IR::add_and_or_get_type(ctx.parent->getParent(), call_node->type));
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
    llvm::Value *err_var = ctx.allocations.at(call_err_name);
    builder.CreateStore(err_val, err_var);

    // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
    if (!call_node->has_catch) {
        generate_rethrow(builder, ctx, call_node);
    }

    // Add the call instruction to the list of unresolved functions only if it was a module-intern call
    if (function_origin == FunctionOrigin::INTERN) {
        if (unresolved_functions.find(call_node->function_name) == unresolved_functions.end()) {
            unresolved_functions[call_node->function_name] = {call};
        } else {
            unresolved_functions[call_node->function_name].push_back(call);
        }
    } else if (function_origin == FunctionOrigin::EXTERN) {
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
        llvm::LoadInst *elem_value = builder.CreateLoad(                                                             //
            return_type->getElementType(i),                                                                          //
            elem_ptr,                                                                                                //
            call_node->function_name + "_" + std::to_string(call_node->call_id) + "_" + std::to_string(i) + "_value" //
        );
        return_value.emplace_back(elem_value);
    }

    // Finally, clear all garbage
    if (!Statement::clear_garbage(builder, garbage)) {
        return std::nullopt;
    }
    return return_value;
}

void Generator::Expression::generate_rethrow( //
    llvm::IRBuilder<> &builder,               //
    GenerationContext &ctx,                   //
    const CallNodeBase *call_node             //
) {
    const std::string err_ret_name = "s" + std::to_string(call_node->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    llvm::Value *const err_var = ctx.allocations.at(err_ret_name);

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

    // Create basic block for the catch block
    llvm::BasicBlock *current_block = builder.GetInsertBlock();
    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create(                           //
        context,                                                                        //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_catch", //
        ctx.parent                                                                      //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create(                           //
        context,                                                                        //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "_merge", //
        ctx.parent                                                                      //
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

Generator::group_mapping Generator::Expression::generate_initializer( //
    llvm::IRBuilder<> &builder,                                       //
    GenerationContext &ctx,                                           //
    garbage_type &garbage,                                            //
    const unsigned int expr_depth,                                    //
    const InitializerNode *initializer                                //
) {
    // Check if its a data initializer
    if (initializer->is_data) {
        if (dynamic_cast<const GroupType *>(initializer->type.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        // Check if the initializer data type is even present
        if (data_nodes.find(initializer->type->to_string()) == data_nodes.end()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        std::vector<llvm::Value *> return_values;
        for (const auto &expression : initializer->args) {
            auto expr_result = generate_expression(builder, ctx, garbage, expr_depth + 1, expression.get());
            if (!expr_result.has_value()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            // For initializers, we need pure single-value types
            if (expr_result.value().size() > 1) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return_values.emplace_back(expr_result.value().front());
        }
        return return_values;
    }
    return std::nullopt;
}

Generator::group_mapping Generator::Expression::generate_optional_switch_expression( //
    llvm::IRBuilder<> &builder,                                                      //
    GenerationContext &ctx,                                                          //
    garbage_type &garbage,                                                           //
    const unsigned int expr_depth,                                                   //
    const SwitchExpression *switch_expression,                                       //
    llvm::Value *switch_value                                                        //
) {
    auto switcher_var_node = dynamic_cast<const VariableNode *>(switch_expression->switcher.get());
    if (switcher_var_node == nullptr) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_expression->switcher->type, false);
    if (switch_value->getType()->isPointerTy()) {
        switch_value = builder.CreateLoad(opt_struct_type, switch_value, "loaded_rhs");
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
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
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

        const VariableNode *var_node = dynamic_cast<const VariableNode *>(branch.matches.front().get());
        if (var_node != nullptr) {
            const std::string var_str = "s" + std::to_string(branch.scope->scope_id) + "::" + var_node->name;
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
    llvm::Value *has_value = builder.CreateLoad(builder.getInt1Ty(), has_value_ptr, "has_value");
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

    auto switcher_var_node = dynamic_cast<const VariableNode *>(switch_expression->switcher.get());
    if (switcher_var_node == nullptr) {
        // Switching on non-variables is not supported yet
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return std::nullopt;
    }
    const unsigned int switcher_scope_id = std::get<1>(ctx.scope->variables.at(switcher_var_node->name));
    const std::string switcher_var_str = "s" + std::to_string(switcher_scope_id) + "::" + switcher_var_node->name;
    llvm::StructType *variant_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), switch_expression->switcher->type, false);
    if (switch_value->getType()->isPointerTy()) {
        switch_value = builder.CreateLoad(variant_struct_type, switch_value, "loaded_rhs");
    }
    switch_value = builder.CreateExtractValue(switch_value, {0}, "variant_flag");
    llvm::Value *var_alloca = ctx.allocations.at(switcher_var_str);

    // First pass: create all branch blocks and detect default case
    for (size_t i = 0; i < switch_expression->branches.size(); i++) {
        const auto &branch = switch_expression->branches[i];

        // Check if it's the default branch (represented by "else")
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
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
        const DataAccessNode *access_node = dynamic_cast<const DataAccessNode *>(branch.matches.front().get());
        assert(access_node != nullptr);

        // Add a reference to the 'value' of the variant to the block the switch expression takes place in
        const std::string access_name = std::get<std::string>(access_node->variable);
        const std::string var_str = "s" + std::to_string(branch.scope->scope_id) + "::" + access_name;
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
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
            continue;
        }

        // Generate the case value
        const DataAccessNode *access_node = dynamic_cast<const DataAccessNode *>(branch.matches.front().get());
        assert(access_node != nullptr);
        switch_inst->addCase(builder.getInt8(access_node->field_id), branch_blocks[i]);
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

    if (dynamic_cast<const OptionalType *>(switch_expression->switcher->type.get())) {
        return generate_optional_switch_expression(builder, ctx, garbage, expr_depth, switch_expression, switch_value);
    }
    if (dynamic_cast<const VariantType *>(switch_expression->switcher->type.get())) {
        return generate_variant_switch_expression(builder, ctx, garbage, expr_depth, switch_expression, switch_value);
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
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
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
        if (dynamic_cast<const DefaultNode *>(branch.matches.front().get())) {
            continue;
        }

        // Generate the case value
        for (const auto &match : branch.matches) {
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
        builder.CreateStore(length_expressions.at(i), array_element_ptr);
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
    if (dynamic_cast<const DefaultNode *>(initializer->initializer_value.get())) {
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
        if (init_expr_type != initializer->element_type) {
            initializer_expression = generate_type_cast(builder, ctx, initializer_expression, init_expr_type, initializer->element_type);
        }
    }
    if (initializer->element_type->to_string() == "str") {
        llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
        llvm::Value *str_len_ptr = builder.CreateStructGEP(str_type, initializer_expression, 0, "str_len_ptr");
        llvm::Value *str_len = builder.CreateLoad(builder.getInt64Ty(), str_len_ptr, "str_len");
        uint64_t str_size = data_layout.getTypeAllocSize(str_type);
        str_len = builder.CreateAdd(str_len, builder.getInt64(str_size));
        llvm::CallInst *fill_call = builder.CreateCall(               //
            Module::Array::array_manip_functions.at("fill_arr_deep"), //
            {created_array, str_len, initializer_expression}          //
        );
        fill_call->setMetadata("comment", llvm::MDNode::get(context, llvm::MDString::get(context, "Fill the array")));
    } else if (dynamic_cast<const PrimitiveType *>(initializer->element_type.get())) {
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
    } else if (dynamic_cast<const MultiType *>(initializer->element_type.get())) {
        // TODO
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
    // First, generate the index expressions
    std::vector<llvm::Value *> index_expressions;
    for (auto &index_expression : access->indexing_expressions) {
        group_mapping index = generate_expression(builder, ctx, garbage, expr_depth, index_expression.get());
        if (!index.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        if (index.value().size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        llvm::Value *index_expr = index.value().front();
        if (dynamic_cast<const GroupType *>(index_expression->type.get())) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        std::shared_ptr<Type> from_type = index_expression->type;
        std::shared_ptr<Type> to_type = Type::get_primitive_type("u64");
        if (from_type != to_type) {
            index_expr = generate_type_cast(builder, ctx, index_expr, from_type, to_type);
        }
        index_expressions.emplace_back(index_expr);
    }
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(access->variable_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + access->variable_name;
    llvm::Value *const array_alloca = ctx.allocations.at(var_name);
    if (access->variable_type->to_string() == "str") {
        // "Array" accesses on strings dont need all the things below, they are much simpler to handle
        if (index_expressions.size() > 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
        // Check if the variable is a function parameter, if it is we do not need to load the pointer, as the array alloca, e.g. the
        // parameter variable, is the pointer to the string struct directly
        llvm::Value *str_value = array_alloca;
        if (!std::get<3>(ctx.scope->variables.at(access->variable_name))) {
            str_value = builder.CreateLoad(str_type->getPointerTo(), array_alloca, "str_value");
        }
        llvm::Function *access_str_at_fn = Module::String::string_manip_functions.at("access_str_at");
        return builder.CreateCall(access_str_at_fn, {str_value, index_expressions.front()});
    }
    llvm::Value *const temp_array_indices = ctx.allocations.at("arr::idx::" + std::to_string(index_expressions.size()));
    // Save all the indices in the temp array
    for (size_t i = 0; i < index_expressions.size(); i++) {
        llvm::Value *index_ptr = builder.CreateGEP(                                                            //
            builder.getInt64Ty(), temp_array_indices, builder.getInt64(i), "idx_" + std::to_string(i) + "_ptr" //
        );
        llvm::StoreInst *index_store = builder.CreateStore(index_expressions[i], index_ptr);
        index_store->setMetadata("comment",                                                                       //
            llvm::MDNode::get(context, llvm::MDString::get(context, "Save the index of id " + std::to_string(i))) //
        );
    }
    const llvm::DataLayout &data_layout = ctx.parent->getParent()->getDataLayout();
    llvm::Type *element_type = IR::get_type(ctx.parent->getParent(), access->type).first;
    size_t element_size_in_bytes = data_layout.getTypeAllocSize(element_type);
    llvm::Value *array_ptr = nullptr;
    if (std::get<3>(ctx.scope->variables.at(access->variable_name))) {
        // Its a function parameter, so its of type 'str*' directly
        array_ptr = array_alloca;
    } else {
        // Its a local variable, so the alloca is of type 'str**'
        array_ptr = builder.CreateLoad(IR::get_type(ctx.parent->getParent(), access->variable_type).first, array_alloca, "array_ptr");
    }
    if (access->type->to_string() == "str") {
        // We get a 'str**' from the 'access_arr' function, so we need to dereference it first before returning it
        llvm::Value *result = builder.CreateCall(Module::Array::array_manip_functions.at("access_arr"), //
            {array_ptr, builder.getInt64(element_size_in_bytes), temp_array_indices}                    //
        );
        return builder.CreateLoad(element_type, result, "str_value");
    } else if (dynamic_cast<const PrimitiveType *>(access->type.get())) {
        llvm::Value *result = builder.CreateCall(Module::Array::array_manip_functions.at("access_arr_val"), //
            {array_ptr, builder.getInt64(element_size_in_bytes), temp_array_indices}                        //
        );
        return IR::generate_bitwidth_change(builder, result, 64, element_type->getPrimitiveSizeInBits(), element_type);
    } else if (dynamic_cast<const MultiType *>(access->type.get())) {
        // TODO
        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
        return nullptr;
    }
    THROW_BASIC_ERR(ERR_GENERATING);
    return nullptr;
}

llvm::Value *Generator::Expression::get_bool8_element_at(llvm::IRBuilder<> &builder, llvm::Value *b8_val, unsigned int elem_idx) {
    // Shift right by i bits and AND with 1 to extract the i-th bit
    llvm::Value *bit_i = builder.CreateAnd(builder.CreateLShr(b8_val, builder.getInt8(elem_idx)), builder.getInt8(1), "extract_bit");
    // Convert the result (i8 with value 0 or 1) to i1 (boolean)
    llvm::Value *bool_bit = builder.CreateTrunc(bit_i, builder.getInt1Ty(), "to_bool");
    return bool_bit;
}

llvm::Value *Generator::Expression::set_bool8_element_at(llvm::IRBuilder<> &builder, llvm::Value *b8_val, llvm::Value *bit_value,
    unsigned int elem_idx) {
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
Generator::group_mapping Generator::Expression::generate_data_variable_access( //
    llvm::IRBuilder<> &builder,                                                //
    GenerationContext &ctx,                                                    //
    const DataAccessNode *data_access                                          //
) {
    std::string access_var_name = std::get<std::string>(data_access->variable);
    // Get the alloca instance of the given data variable
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(access_var_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + access_var_name;
    llvm::Value *var_alloca = ctx.allocations.at(var_name);
    // Get the type of the data variable to access
    llvm::Type *data_type;
    if (data_access->data_type->to_string() == "str" && data_access->field_name == "length") {
        data_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
        // Check if the accessed variable is a function parameter, if it is we dont need to change the alloca at all
        if (std::holds_alternative<std::string>(data_access->variable)) {
            std::string variable_name = std::get<std::string>(data_access->variable);
            if (!std::get<3>(ctx.scope->variables.at(variable_name))) {
                var_alloca = builder.CreateLoad(data_type->getPointerTo(), var_alloca, access_var_name + "_str_val");
            }
        }
    } else if (data_access->data_type->to_string() == "bool8") {
        // Special case for accessing an "element" on a bool8 type
        std::vector<llvm::Value *> values;
        llvm::LoadInst *b8_val = builder.CreateLoad( //
            builder.getInt8Ty(),                     //
            var_alloca,                              //
            access_var_name + "_val"                 //
        );

        b8_val->setMetadata("comment",
            llvm::MDNode::get(context, llvm::MDString::get(context, "Load val of var '" + access_var_name + "'")));
        values.emplace_back(get_bool8_element_at(builder, b8_val, data_access->field_id));
        return values;
    } else if (const ArrayType *array_type = dynamic_cast<const ArrayType *>(data_access->data_type.get())) {
        if (data_access->field_name != "length") {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        std::vector<llvm::Value *> length_values;
        llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
        llvm::Value *arr_val = nullptr;
        if (std::get<3>(ctx.scope->variables.at(access_var_name))) {
            // Its a function argument, so the alloca is of 'str*' type
            arr_val = var_alloca;
        } else {
            // Its a local variable, so the alloca is a 'str**' type
            arr_val = builder.CreateLoad(str_type->getPointerTo(), var_alloca, "arr_val");
        }
        llvm::Value *length_ptr = builder.CreateStructGEP(str_type, arr_val, 1);
        for (size_t i = 0; i < array_type->dimensionality; i++) {
            llvm::Value *actual_length_ptr = builder.CreateGEP(builder.getInt64Ty(), length_ptr, builder.getInt64(i));
            llvm::Value *length_value = builder.CreateLoad(builder.getInt64Ty(), actual_length_ptr, "length_value_" + std::to_string(i));
            length_values.emplace_back(length_value);
        }
        return length_values;
    } else {
        data_type = IR::get_type(ctx.parent->getParent(), data_access->data_type).first;
    }

    llvm::Value *value_ptr = builder.CreateStructGEP(data_type, var_alloca, data_access->field_id);
    const DataType *field_data_type = dynamic_cast<const DataType *>(data_access->type.get());
    const ArrayType *field_array_type = dynamic_cast<const ArrayType *>(data_access->type.get());
    const bool field_str_type = data_access->type->to_string() == "str";
    const bool field_is_complex = field_data_type != nullptr || field_array_type != nullptr || field_str_type;
    llvm::Type *field_base_type = IR::get_type(ctx.parent->getParent(), data_access->type).first;
    const std::string field_name = data_access->field_name.has_value() //
        ? data_access->field_name.value()                              //
        : "$" + std::to_string(data_access->field_id);
    llvm::LoadInst *loaded_value = builder.CreateLoad(                        //
        field_is_complex ? field_base_type->getPointerTo() : field_base_type, //
        value_ptr,                                                            //
        access_var_name + "_" + field_name + "_val"                           //
    );
    std::vector<llvm::Value *> values;
    values.emplace_back(loaded_value);
    return values;
}

Generator::group_mapping Generator::Expression::generate_data_access( //
    llvm::IRBuilder<> &builder,                                       //
    GenerationContext &ctx,                                           //
    garbage_type &garbage,                                            //
    const unsigned int expr_depth,                                    //
    const DataAccessNode *data_access                                 //
) {
    // Check if the "data access" is a enum access
    if (dynamic_cast<const EnumType *>(data_access->data_type.get())) {
        std::vector<llvm::Value *> values;
        values.emplace_back(builder.getInt32(data_access->field_id));
        return values;
    }
    // Check if the data access variable is an expression in of itself. If it is then we need to handle everything a bit different
    if (std::holds_alternative<std::string>(data_access->variable)) {
        return generate_data_variable_access(builder, ctx, data_access);
    } else {
        const ExpressionNode *left_expr_node = std::get<std::unique_ptr<ExpressionNode>>(data_access->variable).get();
        group_mapping left_expr = generate_expression(builder, ctx, garbage, expr_depth + 1, left_expr_node, false);
        if (!left_expr.has_value()) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        if (left_expr.value().size() != 1) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        }
        llvm::Value *expr_val = left_expr.value().front();
        llvm::Type *data_type;
        if (data_access->data_type->to_string() == "str" && data_access->field_name == "length") {
            data_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
        } else if (const ArrayType *array_type = dynamic_cast<const ArrayType *>(data_access->data_type.get())) {
            if (data_access->field_name != "length") {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            std::vector<llvm::Value *> length_values;
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
            llvm::Value *length_ptr = builder.CreateStructGEP(str_type, expr_val, 1);
            for (size_t i = 0; i < array_type->dimensionality; i++) {
                llvm::Value *actual_length_ptr = builder.CreateGEP(builder.getInt64Ty(), length_ptr, builder.getInt64(i));
                llvm::Value *length_value =
                    builder.CreateLoad(builder.getInt64Ty(), actual_length_ptr, "length_value_" + std::to_string(i));
                length_values.emplace_back(length_value);
            }
            return length_values;
        } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(data_access->data_type.get())) {
            std::vector<llvm::Value *> values;
            if (multi_type->base_type->to_string() == "bool") {
                // Special case for accessing an "element" on a bool8 type
                values.emplace_back(get_bool8_element_at(builder, expr_val, data_access->field_id));
            } else {
                values.emplace_back(builder.CreateExtractElement(expr_val, data_access->field_id));
            }
            return values;
        } else {
            data_type = IR::get_type(ctx.parent->getParent(), data_access->data_type).first;
        }

        llvm::Value *value_ptr = builder.CreateStructGEP(data_type, expr_val, data_access->field_id);
        const std::string field_name = data_access->field_name.has_value() //
            ? data_access->field_name.value()                              //
            : "$" + std::to_string(data_access->field_id);
        llvm::LoadInst *loaded_value = builder.CreateLoad(                  //
            IR::get_type(ctx.parent->getParent(), data_access->type).first, //
            value_ptr,                                                      //
            "__flint_expr_stack_" + field_name + "_val"                     //
        );
        std::vector<llvm::Value *> values;
        values.emplace_back(loaded_value);
        return values;
    }
}

Generator::group_mapping Generator::Expression::generate_grouped_data_access( //
    llvm::IRBuilder<> &builder,                                               //
    GenerationContext &ctx,                                                   //
    const GroupedDataAccessNode *grouped_data_access                          //
) {
    // First, get the alloca instance of the given data variable
    const unsigned int var_decl_scope = std::get<1>(ctx.scope->variables.at(grouped_data_access->var_name));
    const std::string var_name = "s" + std::to_string(var_decl_scope) + "::" + grouped_data_access->var_name;
    llvm::Value *const var_alloca = ctx.allocations.at(var_name);

    llvm::Type *data_type = IR::get_type(ctx.parent->getParent(), grouped_data_access->data_type).first;
    const GroupType *group_type = dynamic_cast<const GroupType *>(grouped_data_access->type.get());
    assert(group_type != nullptr);

    std::vector<llvm::Value *> return_values;
    return_values.reserve(group_type->types.size());
    // Its a grouped access on a bool8 variable, we need to handle this specially
    if (grouped_data_access->data_type->to_string() == "bool8") {
        llvm::Value *b8_val = builder.CreateLoad(builder.getInt8Ty(), var_alloca, "b8_val");
        for (auto it = grouped_data_access->field_ids.begin(); it != grouped_data_access->field_ids.end(); ++it) {
            return_values.push_back(get_bool8_element_at(builder, b8_val, *it));
        }
        return return_values;
    }
    // Its a normal grouped data access
    for (size_t i = 0; i < grouped_data_access->field_names.size(); i++) {
        llvm::Value *value_ptr = builder.CreateStructGEP(data_type, var_alloca, grouped_data_access->field_ids.at(i));
        llvm::LoadInst *loaded_value = builder.CreateLoad(                                        //
            IR::get_type(ctx.parent->getParent(), group_type->types.at(i)).first,                 //
            value_ptr,                                                                            //
            grouped_data_access->var_name + "_" + grouped_data_access->field_names.at(i) + "_val" //
        );
        return_values.push_back(loaded_value);
    }
    return return_values;
}

Generator::group_mapping Generator::Expression::generate_type_cast( //
    llvm::IRBuilder<> &builder,                                     //
    GenerationContext &ctx,                                         //
    garbage_type &garbage,                                          //
    const unsigned int expr_depth,                                  //
    const TypeCastNode *type_cast_node                              //
) {
    // First, generate the expression
    auto expr_res = generate_expression(builder, ctx, garbage, expr_depth + 1, type_cast_node->expr.get());
    std::vector<llvm::Value *> expr = expr_res.value();
    std::shared_ptr<Type> to_type;
    if (const GroupType *group_type = dynamic_cast<const GroupType *>(type_cast_node->type.get())) {
        const std::vector<std::shared_ptr<Type>> &types = group_type->types;
        if (types.size() > 1) {
            const MultiType *multi_type = dynamic_cast<const MultiType *>(type_cast_node->expr->type.get());
            if (multi_type == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            assert(expr.size() == 1);
            llvm::Value *mult_expr = expr.front();
            expr.clear();
            for (size_t i = 0; i < multi_type->width; i++) {
                expr.emplace_back(builder.CreateExtractElement(mult_expr, i, "name"));
            }
            return expr;
        }
        to_type = types.front();
    } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(type_cast_node->type.get())) {
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
    } else {
        to_type = type_cast_node->type;
    }
    if (to_type->to_string() == "str" && type_cast_node->expr->type->to_string() == "__flint_type_str_lit") {
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
    if (from_type == to_type) {
        return expr;
    } else if (from_type_str == "__flint_type_str_lit" && to_type_str == "str") {
        llvm::Function *init_str_fn = Module::String::string_manip_functions.at("init_str");
        // Get the length of the literal
        llvm::Value *str_len = builder.CreateCall(c_functions.at(STRLEN), {expr}, "lit_len");
        // Call the `init_str` function
        return builder.CreateCall(init_str_fn, {expr, str_len}, "str_init");
    } else if (dynamic_cast<const MultiType *>(from_type.get())) {
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
    } else if (from_type_str == "i32") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("i32_to_str"), {expr}, "i32_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::i32_to_u8(builder, expr);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::i32_to_u32(builder, expr);
        } else if (to_type_str == "i64") {
            return Module::TypeCast::i32_to_i64(builder, expr);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::i32_to_u64(builder, expr);
        } else if (to_type_str == "f32") {
            return Module::TypeCast::i32_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::i32_to_f64(builder, expr);
        }
    } else if (from_type_str == "u32") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("u32_to_str"), {expr}, "u32_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::u32_to_u8(builder, expr);
        } else if (to_type_str == "i32") {
            return Module::TypeCast::u32_to_i32(builder, expr);
        } else if (to_type_str == "i64") {
            return Module::TypeCast::u32_to_i64(builder, expr);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::u32_to_u64(builder, expr);
        } else if (to_type_str == "f32") {
            return Module::TypeCast::u32_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::u32_to_f64(builder, expr);
        }
    } else if (from_type_str == "i64") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("i64_to_str"), {expr}, "i64_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::i64_to_u8(builder, expr);
        } else if (to_type_str == "i32") {
            return Module::TypeCast::i64_to_i32(builder, expr);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::i64_to_u32(builder, expr);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::i64_to_u64(builder, expr);
        } else if (to_type_str == "f32") {
            return Module::TypeCast::i64_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::i64_to_f64(builder, expr);
        }
    } else if (from_type_str == "u64") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("u64_to_str"), {expr}, "u64_to_str_res");
        } else if (to_type_str == "u8") {
            return Module::TypeCast::u64_to_u8(builder, expr);
        } else if (to_type_str == "i32") {
            return Module::TypeCast::u64_to_i32(builder, expr);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::u64_to_u32(builder, expr);
        } else if (to_type_str == "i64") {
            return Module::TypeCast::u64_to_i64(builder, expr);
        } else if (to_type_str == "f32") {
            return Module::TypeCast::u64_to_f32(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::u64_to_f64(builder, expr);
        }
    } else if (from_type_str == "f32") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("f32_to_str"), {expr}, "f32_to_str_res");
        } else if (to_type_str == "i32") {
            return Module::TypeCast::f32_to_i32(builder, expr);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::f32_to_u32(builder, expr);
        } else if (to_type_str == "i64") {
            return Module::TypeCast::f32_to_i64(builder, expr);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::f32_to_u64(builder, expr);
        } else if (to_type_str == "f64") {
            return Module::TypeCast::f32_to_f64(builder, expr);
        }
    } else if (from_type_str == "f64") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("f64_to_str"), {expr}, "f64_to_str_res");
        } else if (to_type_str == "i32") {
            return Module::TypeCast::f64_to_i32(builder, expr);
        } else if (to_type_str == "u32") {
            return Module::TypeCast::f64_to_u32(builder, expr);
        } else if (to_type_str == "i64") {
            return Module::TypeCast::f64_to_i64(builder, expr);
        } else if (to_type_str == "u64") {
            return Module::TypeCast::f64_to_u64(builder, expr);
        } else if (to_type_str == "f32") {
            return Module::TypeCast::f64_to_f32(builder, expr);
        }
    } else if (from_type_str == "bool") {
        if (to_type_str == "str") {
            return builder.CreateCall(Module::TypeCast::typecast_functions.at("bool_to_str"), {expr}, "bool_to_str_res");
        }
    } else if (from_type_str == "u8") {
        if (to_type_str == "str") {
            // Create a new string of size 1 and set its first byte to the char
            llvm::Value *str_value = builder.CreateCall(                                                   //
                Module::String::string_manip_functions.at("create_str"), {builder.getInt64(1)}, "char_val" //
            );
            llvm::Type *str_type = IR::get_type(ctx.parent->getParent(), Type::get_primitive_type("__flint_type_str_struct")).first;
            llvm::Value *val_ptr = builder.CreateStructGEP(str_type, str_value, 1);
            builder.CreateStore(expr, val_ptr);
            return str_value;
        } else if (to_type_str == "i32") {
            return builder.CreateSExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "i64") {
            return builder.CreateSExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "u32") {
            return builder.CreateZExt(expr, builder.getInt32Ty());
        } else if (to_type_str == "u64") {
            return builder.CreateZExt(expr, builder.getInt64Ty());
        } else if (to_type_str == "bool8") {
            return expr;
        }
    } else if (from_type_str == "void?") {
        // The 'none' literal
        return IR::get_default_value_of_type(builder, ctx.parent->getParent(), to_type);
    }
    if (const OptionalType *to_opt_type = dynamic_cast<const OptionalType *>(to_type.get())) {
        if (from_type != to_opt_type->base_type) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return nullptr;
        }
        // "casting" the actual value of the optional and storing it in the optional struct value itself, but the storing part is done
        // in the assignment / declaration generation
        return expr;
    } else if (const VariantType *to_var_type = dynamic_cast<const VariantType *>(to_type.get())) {
        for (const auto &[_, variant] : to_var_type->variant_node->possible_types) {
            if (from_type == variant) {
                // It's allowed to "cast" the type to the variant
                return expr;
            }
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
    std::vector<llvm::Value *> operand = generate_expression(builder, ctx, garbage, expr_depth + 1, expression).value();
    for (size_t i = 0; i < operand.size(); i++) {
        const std::string &expression_type = expression->type->to_string();
        switch (unary_op->operator_token) {
            default:
                // Unknown unary operator
                THROW_BASIC_ERR(ERR_GENERATING);
                break;
            case TOK_EXCLAMATION: {
                // Optional unwrapping
                const OptionalType *opt_type = dynamic_cast<const OptionalType *>(unary_op->operand->type.get());
                // It's the job of the parser to ensure correct types
                assert(opt_type != nullptr);
                if (operand.at(i)->getType()->isPointerTy()) {
                    llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), unary_op->operand->type, false);
                    operand.at(i) = builder.CreateLoad(opt_struct_type, operand.at(i), "loaded_operand");
                }
                if (unwrap_mode == OptionalUnwrapMode::UNSAFE) {
                    // Directly unwrap the value when in unsafe mode, possibly breaking stuff, but it's much faster too
                    operand.at(i) = builder.CreateExtractValue(operand.at(i), {1}, "opt_value_unsafe");
                    break;
                }
                // First, check if the optional even has a value at all
                llvm::BasicBlock *inserter = builder.GetInsertBlock();
                llvm::BasicBlock *has_no_value = llvm::BasicBlock::Create(context, "opt_upwrap_no_value", ctx.parent);
                llvm::BasicBlock *merge = llvm::BasicBlock::Create(context, "opt_upwrap_value", ctx.parent);
                builder.SetInsertPoint(inserter);
                llvm::Value *opt_has_value = builder.CreateExtractValue(operand.at(i), {0}, "opt_has_value");
                llvm::BranchInst *branch = builder.CreateCondBr(opt_has_value, merge, has_no_value, IR::generate_weights(100, 1));
                branch->setMetadata("comment",
                    llvm::MDNode::get(context, llvm::MDString::get(context, "Check if the 'has_value' property is true")));

                // The crash block, in the case of a bad optional access
                builder.SetInsertPoint(has_no_value);
                llvm::Value *err_msg = IR::generate_const_string(builder, "Bad optional access occurred\n");
                builder.CreateCall(c_functions.at(PRINTF), {err_msg});
                builder.CreateCall(c_functions.at(ABORT), {});
                builder.CreateUnreachable();

                // The merge block, when the optional access was okay
                builder.SetInsertPoint(merge);
                operand.at(i) = builder.CreateExtractValue(operand.at(i), {1}, "opt_value");
                break;
            }
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
    auto lhs_maybe = generate_expression(builder, ctx, garbage, expr_depth + 1, bin_op_node->left.get());
    if (!lhs_maybe.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    std::vector<llvm::Value *> lhs = lhs_maybe.value();
    auto rhs_maybe = generate_expression(builder, ctx, garbage, expr_depth + 1, bin_op_node->right.get());
    if (!rhs_maybe.has_value()) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return std::nullopt;
    }
    std::vector<llvm::Value *> rhs = rhs_maybe.value();
    assert(lhs.size() == rhs.size());
    std::vector<llvm::Value *> return_value;

    const MultiType *lhs_mult = dynamic_cast<const MultiType *>(bin_op_node->left->type.get());
    const MultiType *rhs_mult = dynamic_cast<const MultiType *>(bin_op_node->right->type.get());
    if (lhs_mult != nullptr && rhs_mult != nullptr) {
        if (lhs_mult->base_type != rhs_mult->base_type) {
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
    return return_value;
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
        bin_op_node->left,           //
        bin_op_node->right,          //
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
    switch (bin_op_node->operator_token) {
        default:
            THROW_BASIC_ERR(ERR_GENERATING);
            return std::nullopt;
        case TOK_PLUS:
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE && lhs->getType()->isIntegerTy()) {
                return builder.CreateAdd(lhs, rhs, "add_res");
            }
            if (type_str == "i32" || type_str == "i64" || type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_add"), {lhs, rhs}, "safe_add_res");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFAdd(lhs, rhs, "faddtmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Module::String::generate_string_addition(builder, //
                    ctx.scope, ctx.allocations,                          //
                    garbage, expr_depth + 1,                             //
                    lhs, bin_op_node->left.get(),                        //
                    rhs, bin_op_node->right.get(),                       //
                    bin_op_node->is_shorthand                            //
                );
            }
            break;
        case TOK_MINUS:
            if (overflow_mode == ArithmeticOverflowMode::UNSAFE && lhs->getType()->isIntegerTy()) {
                return builder.CreateSub(lhs, rhs, "sub_res");
            }
            if (type_str == "i32" || type_str == "i64" || type_str == "u32" || type_str == "u64" || type_str == "u8") {
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
            if (type_str == "i32" || type_str == "i64" || type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_mul"), {lhs, rhs}, "safe_mul_res");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFMul(lhs, rhs, "fmultmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            break;
        case TOK_DIV:
            if (type_str == "i32" || type_str == "i64") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateSDiv(lhs, rhs, "sdiv_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_div"), {lhs, rhs}, "safe_sdiv_res");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateUDiv(lhs, rhs, "udiv_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_div"), {lhs, rhs}, "safe_udiv_res");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFDiv(lhs, rhs, "fdivtmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            }
            break;
        case TOK_POW:
            if (type_str == "i32" || type_str == "i64" || type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_pow"), {lhs, rhs}, "pow_res");
            }
            break;
        case TOK_MOD:
            if (type_str == "i32" || type_str == "i64") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateSRem(lhs, rhs, "srem_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_mod"), {lhs, rhs}, "safe_smod_res");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return overflow_mode == ArithmeticOverflowMode::UNSAFE //
                    ? builder.CreateURem(lhs, rhs, "urem_res")         //
                    : builder.CreateCall(Module::Arithmetic::arithmetic_functions.at(type_str + "_safe_mod"), {lhs, rhs}, "safe_umod_res");
            }
            break;
        case TOK_LESS:
            if (type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSLT(lhs, rhs, "icmptmp");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateICmpULT(lhs, rhs, "ucmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOLT(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Logical::generate_string_cmp_lt(builder, lhs, bin_op_node->left.get(), rhs, bin_op_node->right.get());
            }
            break;
        case TOK_GREATER:
            if (type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSGT(lhs, rhs, "icmptmp");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateICmpUGT(lhs, rhs, "ucmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOGT(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Logical::generate_string_cmp_gt(builder, lhs, bin_op_node->left.get(), rhs, bin_op_node->right.get());
            }
            break;
        case TOK_LESS_EQUAL:
            if (type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSLE(lhs, rhs, "icmptmp");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateICmpULE(lhs, rhs, "ucmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOLE(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Logical::generate_string_cmp_le(builder, lhs, bin_op_node->left.get(), rhs, bin_op_node->right.get());
            }
            break;
        case TOK_GREATER_EQUAL:
            if (type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpSGE(lhs, rhs, "icmptmp");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateICmpUGE(lhs, rhs, "ucmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOGE(lhs, rhs, "fcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Logical::generate_string_cmp_ge(builder, lhs, bin_op_node->left.get(), rhs, bin_op_node->right.get());
            }
            break;
        case TOK_EQUAL_EQUAL:
            if (type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpEQ(lhs, rhs, "icmptmp");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateICmpEQ(lhs, rhs, "ucmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpOEQ(lhs, rhs, "fcmptmp");
            } else if (type_str == "bool") {
                return builder.CreateICmpEQ(lhs, rhs, "bcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Logical::generate_string_cmp_eq(builder, lhs, bin_op_node->left.get(), rhs, bin_op_node->right.get());
            } else if (dynamic_cast<const EnumType *>(bin_op_node->left->type.get()) &&
                dynamic_cast<const EnumType *>(bin_op_node->right->type.get())) {
                return builder.CreateICmpEQ(lhs, rhs, "enumeq");
            } else if (dynamic_cast<const OptionalType *>(bin_op_node->left->type.get()) && //
                dynamic_cast<const OptionalType *>(bin_op_node->right->type.get())          //
            ) {
                return generate_optional_cmp(builder, ctx, garbage, expr_depth, lhs, bin_op_node->left, rhs, bin_op_node->right, true);
            }
            break;
        case TOK_NOT_EQUAL:
            if (type_str == "i32" || type_str == "i64") {
                return builder.CreateICmpNE(lhs, rhs, "icmptmp");
            } else if (type_str == "u32" || type_str == "u64" || type_str == "u8") {
                return builder.CreateICmpNE(lhs, rhs, "ucmptmp");
            } else if (type_str == "f32" || type_str == "f64") {
                return builder.CreateFCmpONE(lhs, rhs, "fcmptmp");
            } else if (type_str == "bool") {
                return builder.CreateICmpNE(lhs, rhs, "bcmptmp");
            } else if (type_str == "flint") {
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                return std::nullopt;
            } else if (type_str == "str") {
                return Logical::generate_string_cmp_neq(builder, lhs, bin_op_node->left.get(), rhs, bin_op_node->right.get());
            } else if (dynamic_cast<const EnumType *>(bin_op_node->left->type.get()) &&
                dynamic_cast<const EnumType *>(bin_op_node->right->type.get())) {
                return builder.CreateICmpNE(lhs, rhs, "enumneq");
            } else if (dynamic_cast<const OptionalType *>(bin_op_node->left->type.get()) && //
                dynamic_cast<const OptionalType *>(bin_op_node->right->type.get())          //
            ) {
                return generate_optional_cmp(builder, ctx, garbage, expr_depth, lhs, bin_op_node->left, rhs, bin_op_node->right, false);
            }
            break;
        case TOK_OPT_DEFAULT: {
            // Both the lhs and rhs expressions have already been parsed, this means we can do a simple select based on whether the lhs
            // optional has a value stored in it
            if (lhs->getType()->isPointerTy()) {
                llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), bin_op_node->left->type, false);
                lhs = builder.CreateLoad(opt_struct_type, lhs, "loaded_lhs");
            }
            llvm::Value *has_value = builder.CreateExtractValue(lhs, {0}, "has_value");
            llvm::Value *lhs_value = builder.CreateExtractValue(lhs, {1}, "value");
            return builder.CreateSelect(has_value, lhs_value, rhs, "selected_value");
            break;
        }
        case TOK_AND:
            if (type_str != "bool") {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateLogicalAnd(lhs, rhs, "band");
            break;
        case TOK_OR:
            if (type_str != "bool") {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            return builder.CreateLogicalOr(lhs, rhs, "bor");
            break;
    }
    return std::nullopt;
}

std::optional<llvm::Value *> Generator::Expression::generate_optional_cmp( //
    llvm::IRBuilder<> &builder,                                            //
    GenerationContext &ctx,                                                //
    garbage_type &garbage,                                                 //
    const unsigned int expr_depth,                                         //
    llvm::Value *lhs,                                                      //
    const std::unique_ptr<ExpressionNode> &lhs_expr,                       //
    llvm::Value *rhs,                                                      //
    const std::unique_ptr<ExpressionNode> &rhs_expr,                       //
    const bool eq                                                          //
) {
    // If both sides are the 'none' literal, we can return a constant as the result directly
    if (lhs_expr->type->to_string() == "void?" && rhs_expr->type->to_string() == "void?") {
        return builder.getInt1(eq ? 1 : 0);
    }
    // First, we check if one of the sides is a TypeCast Node, and if one side is a TypeCast we can check if the base type
    // is of type `void?`, indicating that we check if one side is the 'none' literal.
    if (const TypeCastNode *lhs_type_cast = dynamic_cast<const TypeCastNode *>(lhs_expr.get())) {
        if (lhs_type_cast->expr->type->to_string() == "void?") {
            // We can just extract the first bit of the rhs and return it's (negated) value directly
            if (lhs->getType()->isPointerTy()) {
                // The optional is a function argument
                llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), rhs_expr->type, false);
                rhs = builder.CreateLoad(opt_struct_type, rhs, "loaded_rhs");
            }
            llvm::Value *has_value = builder.CreateExtractValue(rhs, {0}, "has_value");
            if (eq) {
                return builder.CreateNot(has_value, "has_no_value");
            } else {
                return has_value;
            }
        }
    }
    if (const TypeCastNode *rhs_type_cast = dynamic_cast<const TypeCastNode *>(rhs_expr.get())) {
        if (rhs_type_cast->expr->type->to_string() == "void?") {
            // We can just extract the first bit of the rhs and return it's (negated) value directly
            if (lhs->getType()->isPointerTy()) {
                // The optional is a function argument
                llvm::StructType *opt_struct_type = IR::add_and_or_get_type(ctx.parent->getParent(), lhs_expr->type, false);
                lhs = builder.CreateLoad(opt_struct_type, lhs, "loaded_lhs");
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
        lhs = builder.CreateLoad(opt_struct_type, lhs, "loaded_lhs");
    }
    if (rhs->getType()->isPointerTy()) {
        rhs = builder.CreateLoad(opt_struct_type, rhs, "loaded_rhs");
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
    const OptionalType *lhs_opt_type = dynamic_cast<const OptionalType *>(lhs_expr->type.get());
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

    // In the merge blcok we check from which block we come and choose the boolean value to use accordingly through phi nodes
    builder.SetInsertPoint(merge_block);
    llvm::PHINode *selected_value = builder.CreatePHI(builder.getInt1Ty(), 2, "result");
    selected_value->addIncoming(both_empty, one_no_value_block);
    selected_value->addIncoming(result_value.value(), both_value_block);
    return selected_value;
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
