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

#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>  // A basic function like in c
#include <llvm/IR/IRBuilder.h> // Utility to generate instructions
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h> // Manages types and global states
#include <llvm/IR/Module.h>      // Container for the IR code
#include <llvm/IR/Type.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/Error.h>

#include <llvm/Transforms/Utils/Cloning.h>

#include <llvm/Support/raw_ostream.h>

#include <iterator>
#include <memory>
#include <string>
#include <variant>

std::map<std::string, llvm::StructType *> Generator::type_map;

/// generate_program_ir
///     Generates the llvm IR code for a complete program
std::unique_ptr<llvm::Module> Generator::generate_program_ir(const std::string &program_name, llvm::LLVMContext &context) {
    auto builder = std::make_unique<llvm::IRBuilder<>>(context);
    auto module = std::make_unique<llvm::Module>(program_name, context);

    // Generate built-in functions in the main module
    generate_builtin_print(builder.get(), module.get());

    llvm::Linker linker(*module);

    for (const auto &file : Resolver::get_file_map()) {
        // Generate the IR for a single file
        std::unique_ptr<llvm::Module> file_module = generate_file_ir(file.second, file.first, context, builder.get());

        // TODO DONE:   This results in a segmentation fault when the context goes out of scope / memory, because then the module will have
        //              dangling references to the context. All modules in the Resolver must be cleared and deleted before the context goes
        //              oom! Its the reason to why Resolver::clear() was implemented!
        // Store the generated module in the resolver
        // std::unique_ptr<const llvm::Module> file_module_copy = llvm::CloneModule(*file_module);
        // Resolver::add_ir(file.first, file_module_copy);

        if (linker.linkInModule(std::move(file_module))) {
            throw_err(ERR_LINKING);
        }
    }

    return module;
}

/// generate_file_ir
///     Generates the llvm IR code from a given AST FileNode for a given file
std::unique_ptr<llvm::Module> Generator::generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext &context,
    llvm::IRBuilder<> *builder) {
    Debug::AST::print_file(file);

    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>(file_name, context);

    // Declare the built-in functions in the file module to reference the main module's versions
    for (auto &builtin_func : builtins) {
        if (builtin_func.second != nullptr) {
            module->getOrInsertFunction(               //
                builtin_func.second->getName(),        //
                builtin_func.second->getFunctionType() //
            );
        }
    }
    // Declare the build-in print functions in the file module to reference the main module's versions
    for (auto &prints : print_functions) {
        if (prints.second != nullptr) {
            module->getOrInsertFunction(         //
                prints.second->getName(),        //
                prints.second->getFunctionType() //
            );
        }
    }

    // Declare all functions in the file at the top of the module
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            // Create a forward declaration for the function only if it is not the main function!
            if (function_node->name != "main") {
                llvm::FunctionType *function_type = generate_function_type(context, function_node);
                module->getOrInsertFunction(function_node->name, function_type);
            }
        }
    }

    // Iterate through all AST Nodes in the file and parse them accordingly (only functions for now!)
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            llvm::Function *function_definition = generate_function(module.get(), function_node);
            // No return statement found despite the signature requires return OR
            // Rerutn statement found but the signature has no return type defined (basically a simple xnor between the two booleans)
            if ((function_has_return(function_definition) ^ function_node->return_types.empty()) == 0) {
                throw_err(ERR_GENERATING);
            }
            // If the function is the main function and does not contain a return statement, add 'return 0' to it
            if (function_node->name == "main" && !function_has_return(function_definition)) {
                // Set the insertion point to the end of the last (or only) basic block of the function
                llvm::BasicBlock &last_block = function_definition->back();
                builder->SetInsertPoint(&last_block);
                // Create the return instruction
                builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
            }
        }
    }

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

/// get_type_map_key
///     Returns the string-encoded type representation of a given functions signature
llvm::StructType *Generator::add_and_or_get_type(llvm::LLVMContext *context, const FunctionNode *function_node) {
    std::string return_types;
    for (auto return_it = function_node->return_types.begin(); return_it < function_node->return_types.end(); ++return_it) {
        return_types.append(*return_it);
        if (std::distance(return_it, function_node->return_types.end()) > 1) {
            return_types.append(",");
        }
    }
    if (type_map.find(return_types) != type_map.end()) {
        return type_map[return_types];
    }

    // Get the return types
    std::vector<llvm::Type *> return_types_vec;
    return_types_vec.reserve(function_node->return_types.size() + 1);
    // First element is always the error code (i32)
    return_types_vec.push_back(llvm::Type::getInt32Ty(*context));
    // Rest of the elements are the return types
    for (const auto &ret_value : function_node->return_types) {
        return_types_vec.emplace_back(get_type_from_str(*context, ret_value));
    }
    llvm::ArrayRef<llvm::Type *> return_types_arr(return_types_vec);
    type_map[return_types] = llvm::StructType::create( //
        *context,                                      //
        return_types_arr,                              //
        "type_" + return_types,                        //
        true                                           //
    );
    return type_map[return_types];
}

/// generate_builtin_print
///     Generates the builtin 'print()' function to utilize C io calls of the IO C stdlib
void Generator::generate_builtin_print(llvm::IRBuilder<> *builder, llvm::Module *module) {
    if (builtins[PRINT] != nullptr) {
        return;
    }
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
    builtins[PRINT] = printf_func;
    generate_builtin_print_int(builder, module);
    generate_builtin_print_flint(builder, module);
    generate_builtin_print_char(builder, module);
    generate_builtin_print_str(builder, module);
    generate_builtin_print_bool(builder, module);
    generate_builtin_print_byte(builder, module);
}

/// generate_builtin_print_int
///     Generates the printf call with the correct format string for integer types
void Generator::generate_builtin_print_int(llvm::IRBuilder<> *builder, llvm::Module *module) {
    if (print_functions["int"] != nullptr) {
        return;
    }

    // Create print_int function type (takes one i32, returns void)
    llvm::FunctionType *print_int_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(module->getContext()),              // return type
        {llvm::Type::getInt32Ty(module->getContext())},           // parameter type
        false                                                     // no vararg
    );

    // Create the print_int function
    llvm::Function *print_int = llvm::Function::Create( //
        print_int_type,                                 //
        llvm::Function::ExternalLinkage,                //
        "print_int",                                    //
        module                                          //
    );
    llvm::BasicBlock *block = llvm::BasicBlock::Create( //
        module->getContext(),                           //
        "entry",                                        //
        print_int                                       //
    );

    // Call printf with format string and argument
    builder->SetInsertPoint(block);
    llvm::Value *format_str = builder->CreateGlobalStringPtr("%i\n");
    builder->CreateCall(builtins[PRINT],   //
        {format_str, print_int->getArg(0)} //
    );

    builder->CreateRetVoid();
    print_functions["int"] = print_int;
}

/// generate_builtin_print_flint
///     Generates the printf call with the correct format string for floating point types
void Generator::generate_builtin_print_flint(llvm::IRBuilder<> *builder, llvm::Module *module) {}

/// generate_builtin_print_char
///     Generates the printf call with the correct format string for char types
void Generator::generate_builtin_print_char(llvm::IRBuilder<> *builder, llvm::Module *module) {}

/// generate_builtin_print_str
///     Generates the printf call with the correct format string for string types
void Generator::generate_builtin_print_str(llvm::IRBuilder<> *builder, llvm::Module *module) {}

/// generate_builtin_print_bool
///     Generates the printf call with the correct format string for bool types
void Generator::generate_builtin_print_bool(llvm::IRBuilder<> *builder, llvm::Module *module) {}

/// generate_builtin_print_byte
///     Generates the printf call with the correct format string for byte types
void Generator::generate_builtin_print_byte(llvm::IRBuilder<> *builder, llvm::Module *module) {}

/// generate_pow_instruction
///     Generates the instruction to power the lhs rhs times (lhs ** rhs)
llvm::Value *Generator::generate_pow_instruction(llvm::IRBuilder<> &builder, llvm::Function *parent, llvm::Value *lhs, llvm::Value *rhs) {
    return nullptr;
}

/// lookup_variable
///     Looks up if the specified variable exists within the given function and returns its Value
llvm::Value *Generator::lookup_variable(llvm::Function *parent, const std::string &var_name) {
    llvm::ValueSymbolTable *symbol_table = parent->getValueSymbolTable();
    llvm::Value *variable = symbol_table->lookup(var_name);
    return variable;
}

/// function_has_return
///     Checks if a given function has a return statement within its bodies instructions
bool Generator::function_has_return(llvm::Function *function) {
    for (const llvm::BasicBlock &block : *function) {
        for (const llvm::Instruction &instr : block) {
            if (llvm::isa<llvm::ReturnInst>(&instr)) {
                return true;
            }
        }
    }
    return false;
}

/// generate_function_type
///     Generates the type information of a given FunctionNode
llvm::FunctionType *Generator::generate_function_type(llvm::LLVMContext &context, FunctionNode *function_node) {
    llvm::Type *return_types = nullptr;
    if (function_node->name == "main") {
        return_types = llvm::Type::getInt32Ty(context);
    } else {
        return_types = add_and_or_get_type(&context, function_node);
    }

    // Get the parameter types
    std::vector<llvm::Type *> param_types_vec;
    param_types_vec.reserve(function_node->parameters.size());
    for (const auto &param : function_node->parameters) {
        param_types_vec.emplace_back(get_type_from_str(context, param.first));
    }
    llvm::ArrayRef<llvm::Type *> param_types(param_types_vec);

    // Complete the functions definition
    llvm::FunctionType *function_type = llvm::FunctionType::get( //
        return_types,                                            //
        param_types,                                             //
        false                                                    //
    );
    return function_type;
}

/// generate_function
///     Generates a function from a given FunctionNode
llvm::Function *Generator::generate_function(llvm::Module *module, FunctionNode *function_node) {
    llvm::FunctionType *function_type = generate_function_type(module->getContext(), function_node);

    // Creating the function itself
    llvm::Function *function = llvm::Function::Create( //
        function_type,                                 //
        llvm::Function::ExternalLinkage,               //
        function_node->name,                           //
        module                                         //
    );

    // Assign names to function arguments and add them to the function's body
    size_t paramIndex = 0;
    for (auto &arg : function->args()) {
        arg.setName(function_node->parameters.at(paramIndex).second);
        ++paramIndex;
    }

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
    // Get the return type of the function
    auto *return_struct_type = llvm::cast<llvm::StructType>(parent->getReturnType());

    // Allocate space for the function's return type (should be a struct type)
    llvm::AllocaInst *return_struct = builder.CreateAlloca(return_struct_type, nullptr, "return_struct");

    // First, always store the error code (0 for no error)
    llvm::Value *error_ptr = builder.CreateStructGEP(return_struct_type, return_struct, 0, "error_ptr");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder.getContext()), 0), error_ptr);

    // If we have a return value, store it in the struct
    if (return_node != nullptr && return_node->return_value != nullptr) {
        // Generate the expression for the return value
        llvm::Value *return_value = generate_expression(builder, parent, return_node->return_value.get());

        // Ensure the return value matches the function's return type
        if (return_value == nullptr) {
            throw_err(ERR_GENERATING);
        }

        // Store the return value in the struct (at index 1)
        llvm::Value *value_ptr = builder.CreateStructGEP(return_struct_type, return_struct, 1, "value_ptr");
        builder.CreateStore(return_value, value_ptr);
    }

    // Generate the return instruction with the evaluated value
    llvm::Value *return_struct_val = builder.CreateLoad(return_struct_type, return_struct, "return_value");
    builder.CreateRet(return_struct_val);
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
void Generator::generate_assignment(llvm::IRBuilder<> &builder, llvm::Function *parent, const AssignmentNode *assignment_node) {
    llvm::Value *expression = generate_expression(builder, parent, assignment_node->value.get());
    llvm::Value *lhs = lookup_variable(parent, assignment_node->var_name);
    builder.CreateStore(expression, lhs);
}

/// generate_declaration
///     Generates the declaration from the given DeclarationNode
void Generator::generate_declaration(llvm::IRBuilder<> &builder, llvm::Function *parent, const DeclarationNode *declaration_node) {
    llvm::Value *expression = generate_expression(builder, parent, declaration_node->initializer.get());

    // TODO: Change 'expression->getType()' to 'get_type_from_str(parent->getParent(), declaration_node->type)' when saving of an
    // expressions type is actually implemented in the parser
    llvm::AllocaInst *alloca = builder.CreateAlloca(expression->getType(), nullptr, declaration_node->name);
    builder.CreateStore(expression, alloca);
}

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
    llvm::Value *variable = lookup_variable(parent, variable_node->name);
    if (variable == nullptr) {
        // Error: Undeclared Variable
        throw_err(ERR_GENERATING);
        return nullptr;
    }

    // Get the type that the pointer points to
    llvm::Type *value_type = get_type_from_str(parent->getParent()->getContext(), variable_node->type);

    // Load the variable's value if it's a pointer
    return builder.CreateLoad(value_type, variable, variable_node->name + "_value");
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
    // Get the arguments
    std::vector<llvm::Value *> args;
    args.reserve(call_node->arguments.size());
    for (const auto &arg : call_node->arguments) {
        args.emplace_back(generate_expression(builder, parent, arg.get()));
    }

    // Check if it is a builtin function and call it
    if (builtin_functions.find(call_node->function_name) != builtin_functions.end()) {
        llvm::Function *builtin_function = builtins.at(builtin_functions.at(call_node->function_name));
        if (builtin_function == nullptr) {
            // Function has not been generated yet, but it should have been
            throw_err(ERR_NOT_IMPLEMENTED_YET);
            return nullptr;
        }
        if (call_node->function_name == "print" && call_node->arguments.size() == 1 &&
            print_functions.find(call_node->arguments.at(0)->type) != print_functions.end()) {
            return builder.CreateCall(                             //
                print_functions[call_node->arguments.at(0)->type], //
                args                                               //
            );
        }
        return builder.CreateCall(builtin_function, args);
    }

    // Calling custom functions
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return nullptr;
}

/// generate_binary_op
///     Generates a binary operation from the given BinaryOpNode
llvm::Value *Generator::generate_binary_op(llvm::IRBuilder<> &builder, llvm::Function *parent, const BinaryOpNode *bin_op_node) {
    llvm::Value *lhs = generate_expression(builder, parent, bin_op_node->left.get());
    llvm::Value *rhs = generate_expression(builder, parent, bin_op_node->right.get());
    switch (bin_op_node->operator_token) {
        default:
            throw_err(ERR_GENERATING);
            return nullptr;
        case TOK_PLUS:
            return builder.CreateAdd(lhs, rhs, "addtmp");
        case TOK_MINUS:
            return builder.CreateSub(lhs, rhs, "subtmp");
        case TOK_MULT:
            return builder.CreateMul(lhs, rhs, "multmp");
        case TOK_DIV:
            return builder.CreateSDiv(lhs, rhs, "sdivtmp");
        case TOK_SQUARE:
            return generate_pow_instruction(builder, parent, lhs, rhs);
    }
}

/// get_type_from_str
///     Returns the type of the given string
llvm::Type *Generator::get_type_from_str(llvm::LLVMContext &context, const std::string &str) {
    // Check if its a primitive or not. If it is not a primitive, its just a pointer type
    if (keywords.find(str) != keywords.end()) {
        //
        switch (keywords.at(str)) {
            default:
                throw_err(ERR_GENERATING);
                return nullptr;
            case TOK_INT:
                return llvm::Type::getInt32Ty(context);
            case TOK_FLINT:
                return llvm::Type::getFloatTy(context);
            case TOK_CHAR:
                return llvm::Type::getInt8Ty(context);
            case TOK_STR:
                // Pointer to an array of i8 values representing the characters
                return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
            case TOK_BOOL:
                return llvm::Type::getInt1Ty(context);
            case TOK_BYTE:
                return llvm::IntegerType::get(context, 8);
            case TOK_VOID:
                return llvm::Type::getVoidTy(context);
        }
    }
    // Pointer to more complex data type
    throw_err(ERR_NOT_IMPLEMENTED_YET);
    return nullptr;
}
