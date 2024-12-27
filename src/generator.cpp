#include "generator/generator.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "resolver/resolver.hpp"

// #include <llvm/IR/DerivedTypes.h>
// #include <llvm/IRReader/IRReader.h>
// #include <llvm/Support/SourceMgr.h>
// #include <llvm/Support/TargetSelect.h>
// #include <llvm/Support/raw_ostream.h>
// #include <llvm/Target/TargetMachine.h>
// #include <llvm/Target/TargetOptions.h>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>    // A basic function like in c
#include <llvm/IR/IRBuilder.h>   // Utility to generate instructions
#include <llvm/IR/LLVMContext.h> // Manages types and global states
#include <llvm/IR/Module.h>      // Container for the IR code
#include <llvm/IR/Type.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/Error.h>

#include <llvm/Transforms/Utils/Cloning.h>

#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>
#include <variant>

/// generate_program_ir
///     Generates the llvm IR code for a complete program
std::unique_ptr<llvm::Module> Generator::generate_program_ir(const std::string &program_name, llvm::LLVMContext *context) {
    auto builder = std::make_unique<llvm::IRBuilder<>>(*context);
    auto module = std::make_unique<llvm::Module>(program_name, *context);

    llvm::Linker linker(*module);

    for (const auto &file : Resolver::get_file_map()) {
        // Generate the IR for a single file
        std::unique_ptr<llvm::Module> file_module = generate_file_ir(file.second, file.first, context, builder.get());

        // TODO DONE:   This results in a segmentation fault when the context goes out of scope / memory, because then the module will have
        //              dangling references to the context. All modules in the Resolver must be cleared and deleted before the context goes
        //              oom! Its the reason to why Resolver::clear() was implemented!
        // Store the generated module in the resolver
        std::unique_ptr<const llvm::Module> file_module_copy = llvm::CloneModule(*file_module);
        Resolver::add_ir(file.first, file_module_copy);

        if (linker.linkInModule(std::move(file_module))) {
            throw_err(ERR_LINKING);
        }
    }

    return module;
}

/// generate_file_ir
///     Generates the llvm IR code from a given AST FileNode for a given file
std::unique_ptr<llvm::Module> Generator::generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext *context,
    llvm::IRBuilder<> *builder) {
    Debug::AST::print_file(file);

    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>(file_name, *context);

    // Iterate through all AST Nodes in the file and parse them accordingly (only functions for now!)
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            llvm::Function *function_definition = generate_function(module.get(), function_node);
        }
    }

    llvm::Function *main_func = generate_builtin_main(module.get());

    // Create the entry block for main function
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context, "entry", main_func);
    builder->SetInsertPoint(entry);

    // Create the "Hello, World!\n" string
    llvm::Value *helloWorld = builder->CreateGlobalStringPtr("Hello, World!\n");

    llvm::Function *print_function = generate_builtin_print(module.get());
    builder->CreateCall(print_function, helloWorld);

    // Add return statement
    builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0));

    // Verify and emit the module
    llvm::verifyModule(*module, &llvm::errs());
    return module;
}

/// get_module_ir_string
///     Returns the module string
std::string Generator::get_module_ir_string(const llvm::Module *module) {
    std::string ir_string;
    llvm::raw_string_ostream stream(ir_string);
    module->print(stream, nullptr);
    stream.flush();
    return ir_string;
}

llvm::Function *Generator::generate_builtin_main(llvm::Module *module) {
    llvm::FunctionType *main_func_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(module->getContext()),             // Return type: int
        false                                                     // No arguments
    );

    llvm::Function *main_func = llvm::Function::Create( //
        main_func_type,                                 //
        llvm::Function::ExternalLinkage,                //
        "main",                                         //
        module                                          //
    );
    return main_func;
}

/// generate_builtin_print
///     Generates the builtin 'print()' function to utilize C io calls of the IO C stdlib
llvm::Function *Generator::generate_builtin_print(llvm::Module *module) {
    llvm::FunctionType *printf_type = llvm::FunctionType::get(                     //
        llvm::Type::getInt32Ty(module->getContext()),                              // Return type: int
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(module->getContext())), // Takes char*
        true                                                                       // Variadic arguments
    );
    llvm::Function *printf_func = llvm::Function::Create( //
        printf_type,                                      //
        llvm::Function::ExternalLinkage,                  //
        "printf",                                         //
        module                                            //
    );
    return printf_func;
}

/// lookup_variable
///     Looks up if the specified variable exists within the given function and returns its Value
llvm::Value *Generator::lookup_variable(llvm::Function *parent, const std::string &var_name) {
    llvm::ValueSymbolTable *symbol_table = parent->getValueSymbolTable();
    llvm::Value *variable = symbol_table->lookup(var_name);
    return variable;
}

/// generate_function
///     Generates a function from a given FunctionNode
llvm::Function *Generator::generate_function(llvm::Module *module, FunctionNode *function_node) {
    // Get the return types
    std::vector<llvm::Type *> return_types_vec;
    return_types_vec.reserve(function_node->return_types.size());
    for (const auto &ret_value : function_node->return_types) {
        return_types_vec.emplace_back(get_type_from_str(module, ret_value));
    }
    llvm::ArrayRef<llvm::Type *> return_types(return_types_vec);
    llvm::StructType *return_types_struct = llvm::StructType::create( //
        module->getContext(),                                         //
        return_types,                                                 //
        function_node->name + "__RET",                                //
        true                                                          //
    );

    // Get the parameter types
    std::vector<llvm::Type *> param_types_vec;
    param_types_vec.reserve(function_node->parameters.size());
    for (const auto &param : function_node->parameters) {
        param_types_vec.emplace_back(get_type_from_str(module, param.first));
    }
    llvm::ArrayRef<llvm::Type *> param_types(param_types_vec);

    // Complete the functions definition
    llvm::FunctionType *function_type = llvm::FunctionType::get( //
        return_types_struct,                                     //
        param_types,                                             //
        false                                                    //
    );

    // Creating the function itself
    llvm::Function *function = llvm::Function::Create( //
        function_type,                                 //
        llvm::Function::ExternalLinkage,               //
        function_node->name,                           //
        module                                         //
    );

    generate_body(function, function_node->body);

    return function;
}

/// generate_body
///     Generates a whole body
void Generator::generate_body(llvm::Function *parent, const std::vector<body_statement> &body) {
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        parent->getParent()->getContext(),                    //
        "entry",                                              //
        parent                                                //
    );
    llvm::IRBuilder<> builder(entry_block);

    // Example: Add instructions to the body
    for (const auto &stmt : body) {
        generate_statement(builder, parent, stmt);
    }
}

/// generate_statement
///     Generates a single statement
void Generator::generate_statement(llvm::IRBuilder<> &builder, llvm::Function *parent, const body_statement &statement) {
    if (std::holds_alternative<std::unique_ptr<StatementNode>>(statement)) {
        const StatementNode *statement_node = std::get<std::unique_ptr<StatementNode>>(statement).get();

        if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement_node)) {
            generate_return_statement(builder, parent, return_node);
        } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement_node)) {
            generate_if_statement(builder, parent, if_node);
        } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement_node)) {
            generate_while_loop(builder, parent, while_node);
        } else if (const auto *for_node = dynamic_cast<const ForLoopNode *>(statement_node)) {
            generate_for_loop(builder, parent, for_node);
        } else if (const auto *assignment_node = dynamic_cast<const AssignmentNode *>(statement_node)) {
            generate_assignment(builder, parent, assignment_node);
        } else if (const auto *declaration_node = dynamic_cast<const DeclarationNode *>(statement_node)) {
            generate_declaration(builder, parent, declaration_node);
        } else {
            throw_err(ERR_GENERATING);
        }
    } else {
        const CallNode *call_node = std::get<std::unique_ptr<CallNode>>(statement).get();
        llvm::Value *call = generate_call(builder, parent, call_node);
    }
}

/// generate_return_statement
///     Generates the return statement from the given ReturnNode
void Generator::generate_return_statement(llvm::IRBuilder<> &builder, llvm::Function *parent, const ReturnNode *return_node) {
    // TODO: The result type string is not yet saved in the ExpressionNode, so it cannot be used for now as this results in an error
    // llvm::Type *return_type = get_type_from_str(parent->getParent(), return_node->return_value->result_type);

    // Check if return_node or return_value is null
    if (return_node == nullptr || return_node->return_value == nullptr) {
        // Generate a void return for functions with no return value
        builder.CreateRetVoid();
        return;
    }

    // Generate the expression for the return value
    llvm::Value *return_value = generate_expression(builder, parent, return_node->return_value.get());

    // Ensure the return value matches the function's return type
    if (return_value == nullptr) {
        throw_err(ERR_GENERATING);
    }

    // Check if the return type specified in the ReturnNode is the same as the return type of the expression
    // SEE TODO ABOVE
    // if (return_value->getType() != return_type) {
    //     throw_err(ERR_GENERATING);
    // }

    // Generate the return instruction with the evaluated value
    builder.CreateRet(return_value);
}

/// generate_if_statement
///     Generates the if statement from the given IfNode
void Generator::generate_if_statement(llvm::IRBuilder<> &builder, llvm::Function *parent, const IfNode *if_node) {
    if (if_node == nullptr || if_node->condition == nullptr) {
        // throw std::runtime_error("Invalid IfNode: missing condition.");
        throw_err(ERR_GENERATING);
    }

    // Generate the condition expression
    llvm::Value *condition = generate_expression(builder, parent, if_node->condition.get());
    if (condition == nullptr) {
        // throw std::runtime_error("Failed to generate condition expression.");
        throw_err(ERR_GENERATING);
    }

    // Convert the condition to a boolean (i1 type)
    llvm::Value *cond_bool = builder.CreateICmpNE(condition, llvm::ConstantInt::get(condition->getType(), 0), "ifcond");

    llvm::BasicBlock *then_bb = llvm::BasicBlock::Create(builder.getContext(), "then", parent);
    llvm::BasicBlock *else_bb = llvm::BasicBlock::Create(builder.getContext(), "else");
    llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(builder.getContext(), "ifcont");

    // Generate conditional branch
    if (if_node->else_branch.empty()) {
        builder.CreateCondBr(cond_bool, then_bb, merge_bb);
    } else {
        builder.CreateCondBr(cond_bool, then_bb, else_bb);
    }

    // Generate 'then' block
    builder.SetInsertPoint(then_bb);
    generate_body(parent, if_node->then_branch);
    builder.CreateBr(merge_bb); // Jump to merge after 'then'

    // Generate 'else' block (if it exists)
    if (!if_node->else_branch.empty()) {
        else_bb->insertInto(parent);
        builder.SetInsertPoint(else_bb);

        generate_body(parent, if_node->else_branch);

        builder.CreateBr(merge_bb); // Jump to merge after 'else'
    }

    // Set insertion point to the merge block
    merge_bb->insertInto(parent);
    builder.SetInsertPoint(merge_bb);
}

/// generate_while_loop
///     Generates the while loop from the given WhileNode
void Generator::generate_while_loop(llvm::IRBuilder<> &builder, llvm::Function *parent, const WhileNode *while_node) {}

/// generate_for_loop
///     Generates the for loop from the given ForLoopNode
void Generator::generate_for_loop(llvm::IRBuilder<> &builder, llvm::Function *parent, const ForLoopNode *for_node) {}

/// generate_assignment
///     Generates the assignment from the given AssignmentNode
void Generator::generate_assignment(llvm::IRBuilder<> &builder, llvm::Function *parent, const AssignmentNode *assignment_node) {}

/// generate_declaration
///     Generates the declaration from the given DeclarationNode
void Generator::generate_declaration(llvm::IRBuilder<> &builder, llvm::Function *parent, const DeclarationNode *declaration_node) {}

/// generate_expression
///     Generates an expression from the given ExpressionNode
llvm::Value *Generator::generate_expression(llvm::IRBuilder<> &builder, llvm::Function *parent, const ExpressionNode *expression_node) {
    if (const auto *variable_node = dynamic_cast<const VariableNode *>(expression_node)) {
        return generate_variable(builder, parent, variable_node);
    }
    if (const auto *unary_op_node = dynamic_cast<const UnaryOpNode *>(expression_node)) {
        return generate_unary_op(builder, parent, unary_op_node);
    }
    if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expression_node)) {
        return generate_literal(builder, parent, literal_node);
    }
    if (const auto *call_node = dynamic_cast<const CallNode *>(expression_node)) {
        return generate_call(builder, parent, call_node);
    }
    if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expression_node)) {
        return generate_binary_op(builder, parent, binary_op_node);
    }
    throw_err(ERR_GENERATING);
    return nullptr;
}

/// generate_variable
///     Generates the variable from the given VariableNode
llvm::Value *Generator::generate_variable(llvm::IRBuilder<> &builder, llvm::Function *parent, const VariableNode *variable_node) {
    if (variable_node == nullptr) {
        // Error: Null Node
        throw_err(ERR_GENERATING);
    }

    llvm::Value *variable = lookup_variable(parent, variable_node->name);
    if (variable == nullptr) {
        // Error: Undeclared Variable
        throw_err(ERR_GENERATING);
    }

    // Ensure the variable's type matches the expected result type
    // TODO: The ExpressionNode does not yet implement its result_type string correctly, so this check would actually always cause an error!
    // llvm::Type *expected_type = get_type_from_str(parent->getParent(), variable_node->result_type);
    // if (variable->getType() != expected_type->getPointerTo()) {
    //     // Error: Type Mismatch
    //     throw_err(ERR_GENERATING);
    // }

    // Load the variable's value if it's a pointer
    return builder.CreateLoad(variable->getType(), variable, variable_node->name + "_value");
}

/// generate_unary_op
///     Generates the unary operation value from the given UnaryOpNode
llvm::Value *Generator::generate_unary_op(llvm::IRBuilder<> &builder, llvm::Function *parent, const UnaryOpNode *unary_op_node) {
    return nullptr;
}

/// generate_literal
///     Generates the literal value from the given LiteralNode
llvm::Value *Generator::generate_literal(llvm::IRBuilder<> &builder, llvm::Function *parent, const LiteralNode *literal_node) {
    if (std::holds_alternative<int>(literal_node->value)) {
        return llvm::ConstantInt::get(                    //
            llvm::Type::getInt32Ty(parent->getContext()), //
            std::get<int>(literal_node->value)            //
        );
    }
    if (std::holds_alternative<double>(literal_node->value)) {
        return llvm::ConstantFP::get(                     //
            llvm::Type::getFloatTy(parent->getContext()), //
            std::get<double>(literal_node->value)         //
        );
    }
    if (std::holds_alternative<std::string>(literal_node->value)) {
        throw_err(ERR_NOT_IMPLEMENTED_YET);
        return nullptr;
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
    throw_err(ERR_PARSING);
    return nullptr;
}

/// generate_call
///     Generates the call from the given CallNode
llvm::Value *Generator::generate_call(llvm::IRBuilder<> &builder, llvm::Function *parent, const CallNode *call_node) {
    return nullptr;
}

/// generate_binary_op
///     Generates a binary operation from the given BinaryOpNode
llvm::Value *Generator::generate_binary_op(llvm::IRBuilder<> &builder, llvm::Function *parent, const BinaryOpNode *bin_op_node) {
    return nullptr;
}

/// get_type_from_str
///
llvm::Type *Generator::get_type_from_str(llvm::Module *module, const std::string &str) {
    // Check if its a primitive or not. If it is not a primitive, its just a pointer type
    if (keywords.find(str) != keywords.end()) {
        //
        switch (keywords.at(str)) {
            default:
                throw_err(ERR_GENERATING);
                return nullptr;
            case TOK_INT:
                return llvm::Type::getInt32Ty(module->getContext());
            case TOK_FLINT:
                return llvm::Type::getFloatTy(module->getContext());
            case TOK_CHAR:
                return llvm::Type::getInt8Ty(module->getContext());
            case TOK_STR:
                // Pointer to an array of i8 values representing the characters
                return llvm::PointerType::get(llvm::Type::getInt8Ty(module->getContext()), 0);
            case TOK_BOOL:
                return llvm::Type::getInt1Ty(module->getContext());
            case TOK_BYTE:
                return llvm::IntegerType::get(module->getContext(), 8);
            case TOK_VOID:
                return llvm::Type::getVoidTy(module->getContext());
        }
    }
    // Pointer to more complex data type
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return nullptr;
}
