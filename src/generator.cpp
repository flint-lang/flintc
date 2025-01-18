#include "generator/generator.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/expressions/call_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/variable_node.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/while_node.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"
#include "types.hpp"

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
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

std::unordered_map<std::string, llvm::StructType *> Generator::type_map;
std::unordered_map<std::string, std::vector<llvm::CallInst *>> Generator::unresolved_functions;
std::unordered_map<std::string, unsigned int> Generator::function_mangle_ids;
std::array<llvm::CallInst *, 1> Generator::main_call_array;

/// generate_program_ir
///     Generates the llvm IR code for a complete program
std::unique_ptr<llvm::Module> Generator::generate_program_ir(const std::string &program_name, llvm::LLVMContext &context) {
    auto builder = std::make_unique<llvm::IRBuilder<>>(context);
    auto module = std::make_unique<llvm::Module>(program_name, context);

    // Generate built-in functions in the main module
    generate_builtin_prints(builder.get(), module.get());

    // Generate main function in the main module
    generate_builtin_main(builder.get(), module.get());

    llvm::Linker linker(*module);

    for (const auto &file : Resolver::get_file_map()) {
        // Generate the IR for a single file
        std::unique_ptr<llvm::Module> file_module = generate_file_ir(file.second, file.first, context, builder.get());

        // TODO DONE:   This results in a segmentation fault when the context goes out of scope / memory, because then the module will have
        //              dangling references to the context. All modules in the Resolver must be cleared and deleted before the context goes
        //              oom! Its the reason to why Resolver::clear() was implemented!
        // Store the generated module in the resolver
        Resolver::add_ir(file.first, file_module.get());

        if (linker.linkInModule(std::move(file_module))) {
            throw_err(ERR_LINKING);
        }
    }

    // Connect the call from the main module to the actual main function declared by the user
    llvm::Function *main_function = module->getFunction("main_custom");
    if (main_function == nullptr) {
        // No main function defined
        throw_err(ERR_GENERATING);
    }
    main_call_array[0]->getCalledOperandUse().set(main_function);

    // Verify and emit the module
    llvm::verifyModule(*module, &llvm::errs());
    return module;
}

/// generate_file_ir
///     Generates the llvm IR code from a given AST FileNode for a given file
std::unique_ptr<llvm::Module> Generator::generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext &context,
    llvm::IRBuilder<> *builder) {

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

    unsigned int mangle_id = 1;
    // Declare all functions in the file at the top of the module
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            // Create a forward declaration for the function only if it is not the main function!
            if (function_node->name != "main") {
                llvm::FunctionType *function_type = generate_function_type(context, function_node);
                module->getOrInsertFunction(function_node->name, function_type);
                function_mangle_ids[function_node->name] = mangle_id++;
            }
        }
    }

    // Iterate through all AST Nodes in the file and parse them accordingly (only functions for now!)
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            llvm::Function *function_definition = generate_function(module.get(), function_node);
            // No return statement found despite the signature requires return OR
            // Rerutn statement found but the signature has no return type defined (basically a simple xnor between the two booleans)

            // TODO: Because i _always_ have a return type (the error return), this does no longer work, as there can be a return type of
            // the function despite the function node not having any return types declared. This error check will be commented out for now
            // because of this reason.
            // if ((function_has_return(function_definition) ^ function_node->return_types.empty()) == 0) {
            //     throw_err(ERR_GENERATING);
            // }
        }
    }

    // Iterate through all unresolved function calls and resolve them to call the _actual_ mangled function, not its definition
    for (const auto &[fn_name, calls] : unresolved_functions) {
        for (llvm::CallInst *call : calls) {
            llvm::Function *actual_function = module->getFunction(fn_name + "." + std::to_string(function_mangle_ids[fn_name]));
            if (actual_function == nullptr) {
                throw_err(ERR_GENERATING);
            }
            call->getCalledOperandUse().set(actual_function);
        }
    }
    unresolved_functions.clear();
    function_mangle_ids.clear();

    return module;
}

/// generate_main
///     Generates the builtin main function which calls the user defined main function
void Generator::generate_builtin_main(llvm::IRBuilder<> *builder, llvm::Module *module) {
    // Create the functionNode of the main_custom function (in order to forward-declare the user defined main function inside the absolute
    // main module)
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<std::string> return_types;
    std::unique_ptr<Scope> scope;
    FunctionNode function_node = FunctionNode(false, false, "main_custom", parameters, return_types, scope);

    // Create the declaration of the custom main function
    llvm::StructType *custom_main_ret_type = add_and_or_get_type(&builder->getContext(), &function_node);
    llvm::FunctionType *custom_main_type = generate_function_type(module->getContext(), &function_node);
    llvm::FunctionCallee custom_main_callee = module->getOrInsertFunction("main_custom", custom_main_type);

    llvm::FunctionType *main_type = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(module->getContext()),        // Return type: int
        {},                                                  // Takes nothing
        false                                                // no varargs
    );
    llvm::Function *main_function = llvm::Function::Create( //
        main_type,                                          //
        llvm::Function::ExternalLinkage,                    //
        "main",                                             //
        module                                              //
    );

    // Create the functions entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "entry",                                              //
        main_function                                         //
    );
    builder->SetInsertPoint(entry_block);

    // Create the return types of the call of the main function.
    llvm::AllocaInst *main_ret = builder->CreateAlloca(custom_main_ret_type, nullptr, "custom_main_ret");
    llvm::CallInst *main_call = builder->CreateCall(custom_main_callee, {});
    main_call_array[0] = main_call;
    llvm::StoreInst *main_res = builder->CreateStore(main_call, main_ret);

    // First, load the first return value of the return struct
    llvm::Value *err_ptr = builder->CreateStructGEP(custom_main_ret_type, main_ret, 0);
    llvm::LoadInst *err_val = builder->CreateLoad(llvm::Type::getInt32Ty(module->getContext()), err_ptr, "main_err_val");

    llvm::BasicBlock *current_block = builder->GetInsertBlock();
    llvm::BasicBlock *catch_block = llvm::BasicBlock::Create( //
        builder->getContext(),                                //
        "main_catch",                                         //
        main_function                                         //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create( //
        builder->getContext(),                                //
        "main_merge",                                         //
        main_function                                         //
    );
    builder->SetInsertPoint(current_block);

    // Create the if check and compare the err value to 0
    llvm::ConstantInt *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder->getContext()), 0);
    llvm::Value *err_condition = builder->CreateICmpNE( //
        err_val,                                        //
        zero,                                           //
        "errcmp"                                        //
    );

    // Create the branching operation
    builder->CreateCondBr(err_condition, catch_block, merge_block)
        ->setMetadata("comment",
            llvm::MDNode::get(module->getContext(),
                llvm::MDString::get(module->getContext(),
                    "Branch to '" + catch_block->getName().str() + "' if 'main_custom' returned error")));

    // Generate the body of the catch block
    builder->SetInsertPoint(catch_block);

    // Function to create a string literal
    std::function<llvm::Value *(llvm::IRBuilder<> *, llvm::Module *, llvm::Function *, std::string)>      //
        create_str_lit =                                                                                  //
        [](llvm::IRBuilder<> *builder, llvm::Module *module, llvm::Function *parent, std::string message) //
        -> llvm::Value * {
        std::variant<int, double, std::string, bool, char> value = message;
        LiteralNode message_literal = LiteralNode(value, "str");
        return generate_literal(*builder, parent, &message_literal);
    };

    // Create the message that an error has occured
    llvm::Value *message_begin_ptr = create_str_lit(builder, module, main_function, "ERROR: Program exited with exit code '");
    builder->CreateCall(print_functions.at("str"), {message_begin_ptr});
    // Print the actual error value
    builder->CreateCall(print_functions.at("int"), {err_val});
    // Print the rest of the string
    llvm::Value *message_end_ptr = create_str_lit(builder, module, main_function, "'\n");
    builder->CreateCall(print_functions.at("str"), {message_end_ptr});

    builder->CreateBr(merge_block);
    builder->SetInsertPoint(merge_block);
    llvm::Value *ret = builder->CreateRet(err_val);
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

/// resolve_ir_comments
///     Resolves all IR file module comments
std::string Generator::resolve_ir_comments(const std::string &ir_string) {
    // LLVM's automatic comments tart at the 50th character, so we will start 10 characters later
    static const unsigned int COMMENT_OFFSET = 60;

    // Step 1: Create containers for regex operations
    std::unordered_map<std::string, std::string> metadata_map;
    std::regex metadata_regex(R"(\!(\d+) = \!\{\!\s*\"([^\"]*)\"\})");
    std::regex comment_reference_regex(R"(, \!comment \!(\d+))");
    std::regex metadata_line_regex(R"(\!(\d+) = \!\{\!\s*\"([^\"]*)\"\}\n?)");

    // Step 2: Extract metadata definitions
    std::sregex_iterator metadata_begin(ir_string.begin(), ir_string.end(), metadata_regex);
    std::sregex_iterator metadata_end;
    for (auto it = metadata_begin; it != metadata_end; ++it) {
        // Map '!X' to its comment text
        metadata_map[it->str(1)] = it->str(2);
    }

    // Step 3: Collect all matches and save them into the 'remplacements' vector
    std::string processed_ir = ir_string;
    std::vector<std::pair<size_t, std::pair<size_t, std::string>>> replacements;
    std::sregex_iterator comment_begin(processed_ir.begin(), processed_ir.end(), comment_reference_regex);
    std::sregex_iterator comment_end;

    for (auto it = comment_begin; it != comment_end; ++it) {
        std::string comment_id = it->str(1);
        auto metadata_it = metadata_map.find(comment_id);
        if (metadata_it != metadata_map.end()) {
            // Find the start of the line containing this comment
            size_t line_start = processed_ir.rfind('\n', it->position());
            if (line_start == std::string::npos) {
                line_start = 0;
            } else {
                // Move past the newline
                line_start++;
            }

            // Calculate the current line length up to the comma
            size_t line_length = it->position() - line_start;

            // Calculate needed spacing
            unsigned int spaces = (line_length > COMMENT_OFFSET ? 1 : COMMENT_OFFSET - line_length);

            // Create the replacement string with proper spacing
            std::string comment_text = std::string(spaces, ' ') + "; " + metadata_it->second;

            // Store position, length and replacement text
            replacements.push_back({(size_t)it->position(), {it->length(), comment_text}});
        }
    }

    // Step 4: Apply replacements in reverse order to maintain correct positions
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
        processed_ir.replace(it->first, it->second.first, it->second.second);
    }

    // Step 5: Remove metadata definition lines
    processed_ir = std::regex_replace(processed_ir, metadata_line_regex, "");

    return processed_ir;
}

/// add_and_or_get_type
///     Returns the struct return type of a given function node.
///     Returns a reference to the already existent type
llvm::StructType *Generator::add_and_or_get_type(llvm::LLVMContext *context, const FunctionNode *function_node) {
    std::string return_types;
    for (auto return_it = function_node->return_types.begin(); return_it < function_node->return_types.end(); ++return_it) {
        return_types.append(*return_it);
        if (std::distance(return_it, function_node->return_types.end()) > 1) {
            return_types.append(",");
        }
    }
    if (return_types == "") {
        return_types = "void";
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
void Generator::generate_builtin_prints(llvm::IRBuilder<> *builder, llvm::Module *module) {
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
    generate_builtin_print(builder, module, "int", "%i");
    generate_builtin_print(builder, module, "flint", "%d");
    generate_builtin_print(builder, module, "char", "%c");
    generate_builtin_print(builder, module, "str", "%s");
    generate_builtin_print(builder, module, "byte", "%i");
    generate_builtin_print_bool(builder, module);
}

/// generate_builtin_print
///     Generates the builtin print function for the specified type
void Generator::generate_builtin_print( //
    llvm::IRBuilder<> *builder,         //
    llvm::Module *module,               //
    const std::string &type,            //
    const std::string &format           //
) {
    if (print_functions.at(type) != nullptr) {
        return;
    }

    // Create print function type
    llvm::FunctionType *print_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(module->getContext()),          // return void
        {get_type_from_str(module->getContext(), type)},      // takes type
        false                                                 // no vararg
    );

    // Create the print_int function
    llvm::Function *print_function = llvm::Function::Create( //
        print_type,                                          //
        llvm::Function::ExternalLinkage,                     //
        "print_" + type,                                     //
        module                                               //
    );
    llvm::BasicBlock *block = llvm::BasicBlock::Create( //
        module->getContext(),                           //
        "entry",                                        //
        print_function                                  //
    );

    // Call printf with format string and argument
    builder->SetInsertPoint(block);
    llvm::Value *format_str = builder->CreateGlobalStringPtr(format);
    builder->CreateCall(builtins[PRINT],        //
        {format_str, print_function->getArg(0)} //
    );

    builder->CreateRetVoid();
    print_functions[type] = print_function;
}

/// generate_builtin_print_bool
///     Generates the builtin print_bool function which prints 'true' or 'false' depending on the bool value!
void Generator::generate_builtin_print_bool(llvm::IRBuilder<> *builder, llvm::Module *module) {
    if (print_functions.at("bool") != nullptr) {
        return;
    }

    // Create print function type
    llvm::FunctionType *print_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(module->getContext()),          // return void
        {get_type_from_str(module->getContext(), "bool")},    // takes type
        false                                                 // no vararg
    );

    // Create the print_int function
    llvm::Function *print_function = llvm::Function::Create( //
        print_type,                                          //
        llvm::Function::ExternalLinkage,                     //
        "print_bool",                                        //
        module                                               //
    );
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "entry",                                              //
        print_function                                        //
    );
    llvm::BasicBlock *true_block = llvm::BasicBlock::Create( //
        module->getContext(),                                //
        "bool_true",                                         //
        print_function                                       //
    );
    llvm::BasicBlock *false_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "bool_false",                                         //
        print_function                                        //
    );
    llvm::BasicBlock *merge_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "merge",                                              //
        print_function                                        //
    );

    llvm::Argument *arg = print_function->getArg(0);

    // Create the constant "true" string and the "false" string
    llvm::Value *format_str = builder->CreateGlobalStringPtr("%s");
    llvm::Value *str_true = builder->CreateGlobalStringPtr("true");
    llvm::Value *str_false = builder->CreateGlobalStringPtr("false");

    // The entry block, create condition and branch
    builder->SetInsertPoint(entry_block);
    builder->CreateCondBr(arg, true_block, false_block);

    // True block
    builder->SetInsertPoint(true_block);
    builder->CreateCall(builtins[PRINT], {format_str, str_true});
    builder->CreateBr(merge_block);

    // False block
    builder->SetInsertPoint(false_block);
    builder->CreateCall(builtins[PRINT], {format_str, str_false});
    builder->CreateBr(merge_block);

    // Merge block
    builder->SetInsertPoint(merge_block);
    builder->CreateRetVoid();

    print_functions["bool"] = print_function;
}

/// generate_pow_instruction
///     Generates the instruction to power the lhs rhs times (lhs ** rhs)
llvm::Value *Generator::generate_pow_instruction(llvm::IRBuilder<> &builder, llvm::Function *parent, llvm::Value *lhs, llvm::Value *rhs) {
    return nullptr;
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

/// generate_phi_calls
///     Generates all phi calls from the given phi_lookup within the currently active block of the builder
void Generator::generate_phi_calls(                                                                        //
    llvm::IRBuilder<> &builder,                                                                            //
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup //
) {
    for (const auto &variable : phi_lookup) {
        if (variable.second.empty()) {
            // No mutations of this variable occured
            continue;
        }

        // Create a new phi node
        llvm::PHINode *phi = builder.CreatePHI(      //
            variable.second.at(0).second->getType(), // Type of the variable
            variable.second.size(),                  // Number of mutations (number of blocks in which the variable got mutated)
            variable.first + "_phi"                  // Name of the phi node
        );

        // Add all the variable mutations from all the blocks it was mutated
        for (const auto &mutation : variable.second) {
            phi->addIncoming(mutation.second, mutation.first);
        }
    }
}

/// generate_allocation
///     Generates a custom allocation call
void Generator::generate_allocation(                                       //
    llvm::IRBuilder<> &builder,                                            //
    Scope *scope,                                                          //
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
        throw_err(ERR_GENERATING);
    }
    allocations.insert({alloca_name, alloca});
}

/// generate_call_allocations
///     Generates the allocations for calls
void Generator::generate_call_allocations(                                 //
    llvm::IRBuilder<> &builder,                                            //
    llvm::Function *parent,                                                //
    Scope *scope,                                                          //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
    CallNode *call_node                                                    //
) {
    // Get the (already existent) function definition
    llvm::Function *func_decl = parent->getParent()->getFunction(call_node->function_name);
    // Set the scope the call happens in
    call_node->scope_id = scope->scope_id;

    // Temporary allocation for the entire return struct
    const std::string ret_alloca_name = "s" + std::to_string(scope->scope_id) + "::c" + std::to_string(call_node->call_id) + "::ret";
    generate_allocation(builder, scope, allocations, ret_alloca_name,                                                        //
        func_decl->getReturnType(),                                                                                          //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "__RET",                                       //
        "Create alloc of struct for called function '" + call_node->function_name + "', called by '" + ret_alloca_name + "'" //
    );

    // Create the error return valua allocation
    const std::string err_alloca_name = "s" + std::to_string(scope->scope_id) + "::c" + std::to_string(call_node->call_id) + "::err";
    generate_allocation(builder, scope, allocations, err_alloca_name,                  //
        llvm::Type::getInt32Ty(parent->getContext()),                                  //
        call_node->function_name + "_" + std::to_string(call_node->call_id) + "__ERR", //
        "Create alloc of err ret var '" + err_alloca_name + "'"                        //
    );
}

/// generate_allocation
///     Generates all allocations of the given scope. Adds all AllocaInst pointer to the allocations map
void Generator::generate_allocations(                                     //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    for (const auto &statement : scope->body) {
        if (!std::holds_alternative<std::unique_ptr<StatementNode>>(statement)) {
            CallNode *call_node = std::get<std::unique_ptr<CallNode>>(statement).get();
            generate_call_allocations(builder, parent, scope, allocations, call_node);
            continue;
        }

        StatementNode *statement_node = std::get<std::unique_ptr<StatementNode>>(statement).get();
        if (const auto *while_node = dynamic_cast<const WhileNode *>(statement_node)) {
            generate_allocations(builder, parent, while_node->scope.get(), allocations);
        } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement_node)) {
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
        } else if (const auto *for_loop_node = dynamic_cast<const ForLoopNode *>(statement_node)) {
            //
        } else if (const auto *declaration_node = dynamic_cast<const DeclarationNode *>(statement_node)) {
            if (auto *call_node = dynamic_cast<CallNode *>(declaration_node->initializer.get())) {
                generate_call_allocations(builder, parent, scope, allocations, call_node);

                // Create the actual variable allocation with the declared type
                const std::string var_alloca_name = "s" + std::to_string(scope->scope_id) + "::" + declaration_node->name;
                generate_allocation(builder, scope, allocations, var_alloca_name,    //
                    get_type_from_str(parent->getContext(), declaration_node->type), //
                    declaration_node->name + "__VAL_1",                              //
                    "Create alloc of 1st ret var '" + var_alloca_name + "'"          //
                );
            } else {
                // A "normal" allocation
                const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::" + declaration_node->name;
                generate_allocation(builder, scope, allocations, alloca_name,        //
                    get_type_from_str(parent->getContext(), declaration_node->type), //
                    declaration_node->name + "__VAR",                                //
                    "Create alloc of var '" + alloca_name + "'"                      //
                );
            }
        }
    }
}

/// get_type_from_str
///     Returns the type of the given string
llvm::Type *Generator::get_type_from_str(llvm::LLVMContext &context, const std::string &str) {
    // Check if its a primitive or not. If it is not a primitive, its just a pointer type
    if (keywords.find(str) != keywords.end()) {
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

/// get_default_value_of_type
///     Returns the default value of the given llvm Type
llvm::Value *Generator::get_default_value_of_type(llvm::Type *type) {
    if (type->isIntegerTy()) {
        return llvm::ConstantInt::get(type, 0);
    }
    if (type->isDoubleTy()) {
        return llvm::ConstantFP::get(type, 0.0);
    }
    if (type->isPointerTy()) {
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
    }
    // No conversion available
    throw_err(ERR_GENERATING);
    return nullptr;
}

/// get_default_struct
///     Allocates a struct and adds default values to every element of the struct
///     If 'ignore_first' is set, the first struct element wont be set to any value (because its the error return and will be set anyway)
llvm::AllocaInst *Generator::generate_default_struct( //
    llvm::IRBuilder<> &builder,                       //
    llvm::StructType *type,                           //
    const std::string &name,                          //
    bool ignore_first                                 //
) {
    llvm::AllocaInst *alloca = builder.CreateAlloca(type, nullptr, name);

    for (unsigned int i = (ignore_first ? 1 : 0); i < type->getNumElements(); ++i) {
        llvm::Type *field_type = type->getElementType(i);
        llvm::Value *default_value = get_default_value_of_type(field_type);
        llvm::Value *field_ptr = builder.CreateStructGEP(type, alloca, i);
        builder.CreateStore(default_value, field_ptr);
    }

    return alloca;
}

/// generate_function_type
///     Generates the type information of a given FunctionNode
llvm::FunctionType *Generator::generate_function_type(llvm::LLVMContext &context, FunctionNode *function_node) {
    llvm::Type *return_types = add_and_or_get_type(&context, function_node);

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
    llvm::Function *function = llvm::Function::Create(                         //
        function_type,                                                         //
        llvm::Function::ExternalLinkage,                                       //
        (function_node->name == "main") ? "main_custom" : function_node->name, //
        module                                                                 //
    );

    // Assign names to function arguments and add them to the function's body
    size_t paramIndex = 0;
    for (auto &arg : function->args()) {
        arg.setName(function_node->parameters.at(paramIndex).second);
        ++paramIndex;
    }

    // Create the functions entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        module->getContext(),                                 //
        "entry",                                              //
        function                                              //
    );
    llvm::IRBuilder<> builder(entry_block);

    // Create all the functions allocations (declarations, etc.) at the beginning, before the actual function body
    // The key is a combination of the scope id and the variable name, e.g. 1::var1, 2::var2
    std::unordered_map<std::string, llvm::AllocaInst *const> allocations;
    generate_allocations(builder, function, function_node->scope.get(), allocations);

    // Generate all instructions of the functions body
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> phi_lookup;
    generate_body(builder, function, function_node->scope.get(), phi_lookup, allocations);

    // Check if the function has a terminator, if not add an "empty" return (only the error return)
    if (!function->empty() && function->back().getTerminator() == nullptr) {
        generate_return_statement(builder, function, function_node->scope.get(), {}, allocations);
    }

    return function;
}

/// generate_body
///     Generates a whole body
void Generator::generate_body(                                                                              //
    llvm::IRBuilder<> &builder,                                                                             //
    llvm::Function *parent,                                                                                 //
    Scope *scope,                                                                                           //
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
) {
    for (const auto &stmt : scope->body) {
        generate_statement(builder, parent, scope, stmt, phi_lookup, allocations);
        // Check if the statment was an if statement, if so check if the last block does contain any instructions
        // If it does not, delete the last block
        if (std::holds_alternative<std::unique_ptr<StatementNode>>(stmt)) {
            if (const auto *if_node = dynamic_cast<const IfNode *>(std::get<std::unique_ptr<StatementNode>>(stmt).get())) {
                if (builder.GetInsertBlock()->empty() && stmt == scope->body.back()) {
                    builder.GetInsertBlock()->eraseFromParent();
                }
            }
        }
    }
}

/// generate_statement
///     Generates a single statement
void Generator::generate_statement(                                                                         //
    llvm::IRBuilder<> &builder,                                                                             //
    llvm::Function *parent,                                                                                 //
    Scope *scope,                                                                                           //
    const body_statement &statement,                                                                        //
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
) {
    if (std::holds_alternative<std::unique_ptr<StatementNode>>(statement)) {
        const StatementNode *statement_node = std::get<std::unique_ptr<StatementNode>>(statement).get();

        if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement_node)) {
            generate_return_statement(builder, parent, scope, return_node, allocations);
        } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement_node)) {
            std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> phi_lookup;
            generate_if_statement(builder, parent, if_node, 0, {}, phi_lookup, allocations);
        } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement_node)) {
            generate_while_loop(builder, parent, while_node, allocations);
        } else if (const auto *for_node = dynamic_cast<const ForLoopNode *>(statement_node)) {
            generate_for_loop(builder, parent, for_node);
        } else if (const auto *assignment_node = dynamic_cast<const AssignmentNode *>(statement_node)) {
            generate_assignment(builder, parent, scope, assignment_node, phi_lookup, allocations);
        } else if (const auto *declaration_node = dynamic_cast<const DeclarationNode *>(statement_node)) {
            generate_declaration(builder, parent, scope, declaration_node, allocations);
        } else if (const auto *throw_node = dynamic_cast<const ThrowNode *>(statement_node)) {
            generate_throw_statement(builder, parent, scope, throw_node, allocations);
        } else if (const auto *catch_node = dynamic_cast<const CatchNode *>(statement_node)) {
            generate_catch_statement(builder, parent, scope, catch_node, allocations);
        } else {
            throw_err(ERR_GENERATING);
        }
    } else {
        const CallNode *call_node = std::get<std::unique_ptr<CallNode>>(statement).get();
        llvm::Value *call = generate_call(builder, parent, scope, call_node, allocations);
    }
}

/// generate_return_statement
///     Generates the return statement from the given ReturnNode
void Generator::generate_return_statement(                                //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const ReturnNode *return_node,                                        //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
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
        llvm::Value *return_value = generate_expression(builder, parent, scope, return_node->return_value.get(), allocations);

        // Ensure the return value matches the function's return type
        if (return_value == nullptr) {
            throw_err(ERR_GENERATING);
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

/// generate_throw_statement
///     Generates the throw statement from the given ThrowNode
void Generator::generate_throw_statement(                                 //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const ThrowNode *throw_node,                                          //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    // Get the return type of the function
    auto *throw_struct_type = llvm::cast<llvm::StructType>(parent->getReturnType());

    // Allocate the struct and set all of its values to their respective default values
    llvm::AllocaInst *throw_struct = generate_default_struct(builder, throw_struct_type, "throw_ret", true);
    throw_struct->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Create default struct of type '" + throw_struct_type->getName().str() + "' except first value")));

    // Create the pointer to the error value (the 0th index of the struct)
    llvm::Value *error_ptr = builder.CreateStructGEP(throw_struct_type, throw_struct, 0, "err_ptr");
    // Generate the expression right of the throw statement, it has to be of type int
    llvm::Value *err_value = generate_expression(builder, parent, scope, throw_node->throw_value.get(), allocations);
    // Store the error value in the struct
    builder.CreateStore(err_value, error_ptr);

    // Generate the throw (return) instruction with the evaluated value
    llvm::LoadInst *throw_struct_val = builder.CreateLoad(throw_struct_type, throw_struct, "throw_val");
    throw_struct_val->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Load allocated throw struct of type '" + throw_struct_type->getName().str() + "'")));
    builder.CreateRet(throw_struct_val);
}

/// generate_if_statement
///     Generates the if statement from the given IfNode
void Generator::generate_if_statement(                                                                      //
    llvm::IRBuilder<> &builder,                                                                             //
    llvm::Function *parent,                                                                                 //
    const IfNode *if_node,                                                                                  //
    unsigned int nesting_level,                                                                             //
    const std::vector<llvm::BasicBlock *> &blocks,                                                          //
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
) {
    if (if_node == nullptr || if_node->condition == nullptr) {
        // Invalid IfNode: missing condition
        throw_err(ERR_GENERATING);
    }

    llvm::LLVMContext &context = parent->getContext();
    std::vector<llvm::BasicBlock *> current_blocks = blocks;

    // First call (nesting_level == 0): Create all blocks for entire if-chain
    if (nesting_level == 0) {
        // Count total number of branches and create blocks
        const IfNode *current = if_node;
        unsigned int branch_count = 0;

        while (current != nullptr) {
            if (branch_count != 0) {
                // Create then condition block (for the else if blocks)
                current_blocks.push_back(llvm::BasicBlock::Create( //
                    context,                                       //
                    "then_cond" + std::to_string(branch_count),    //
                    parent)                                        //
                );
            }

            // Create then block
            current_blocks.push_back(llvm::BasicBlock::Create( //
                context,                                       //
                "then" + std::to_string(branch_count),         //
                parent)                                        //
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
                    throw_err(ERR_GENERATING);
                }
                current_blocks.push_back(llvm::BasicBlock::Create( //
                    context,                                       //
                    "else" + std::to_string(branch_count),         //
                    parent)                                        //
                );
                current = nullptr;
            }
        }

        // Create merge block (shared by all branches)
        current_blocks.push_back(llvm::BasicBlock::Create(context, "merge", parent));
    }

    // Reference to the merge block
    llvm::BasicBlock *merge_block = current_blocks.back();

    // Index calculation for current blocks
    unsigned int then_idx = nesting_level;
    // Defaults to the block after the current block
    unsigned int next_idx = nesting_level + 1;

    // Determine the next block (either else-if/else block or merge block)
    if (if_node->else_scope.has_value()) {
        const auto &else_scope = if_node->else_scope.value();
        if (std::holds_alternative<std::unique_ptr<IfNode>>(else_scope)) {
            // Next block is the final else block
            next_idx = then_idx + 1;
        } else {
            const auto &else_statements = std::get<std::unique_ptr<Scope>>(else_scope)->body;
            if (!else_statements.empty()) {
                next_idx = then_idx + 1;
            }
        }
    }

    // Cehck if this is the if and the next index is not the last index. This needs to be done for the elif branches
    if (then_idx != 0 && next_idx + 1 < current_blocks.size()) {
        then_idx++;
        next_idx++;
    }

    // Generate the condition
    llvm::Value *condition = generate_expression(builder, parent, if_node->then_scope->parent_scope, if_node->condition.get(), allocations);
    if (condition == nullptr) {
        // Failed to generate condition expression
        throw_err(ERR_GENERATING);
    }
    // Create conditional branch
    llvm::BranchInst *branch = builder.CreateCondBr( //
        condition,                                   //
        current_blocks[then_idx],                    //
        current_blocks[next_idx]                     //
    );
    branch->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Branch between '" + current_blocks[then_idx]->getName().str() + "' and '" + current_blocks[next_idx]->getName().str() +
                    "' based on condition '" + condition->getName().str() + "'")));

    // Create the phi variable mutation lookup map if the phi_lookup is empty (for initial call of the if node. In recursive calls its not
    // empty of course)
    if (phi_lookup.empty()) {
        for (const auto &scoped_variable : if_node->then_scope->get_parent()->variable_types) {
            phi_lookup[scoped_variable.first] = {};
        }
    }

    // Generate then branch
    builder.SetInsertPoint(current_blocks[then_idx]);
    generate_body(builder, parent, if_node->then_scope.get(), phi_lookup, allocations);
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Branch to merge block
        builder.CreateBr(merge_block);
    }

    // Handle else-if or else
    if (if_node->else_scope.has_value()) {
        const auto &else_scope = if_node->else_scope.value();
        if (std::holds_alternative<std::unique_ptr<IfNode>>(else_scope)) {
            // Recursive call for else-if
            builder.SetInsertPoint(current_blocks[next_idx]);
            generate_if_statement(                                   //
                builder,                                             //
                parent,                                              //
                std::get<std::unique_ptr<IfNode>>(else_scope).get(), //
                nesting_level + 1,                                   //
                current_blocks,                                      //
                phi_lookup,                                          //
                allocations                                          //
            );
        } else {
            // Handle final else, if it exists
            const auto &last_else_scope = std::get<std::unique_ptr<Scope>>(else_scope);
            if (!last_else_scope->body.empty()) {
                builder.SetInsertPoint(current_blocks[next_idx]);
                generate_body(builder, parent, last_else_scope.get(), phi_lookup, allocations);
                if (builder.GetInsertBlock()->getTerminator() == nullptr) {
                    builder.CreateBr(merge_block);
                }
            }
        }
    }

    // Only create phi nodes for the variables mutated in one of the if blocks
    if (nesting_level == 0) {
        builder.SetInsertPoint(merge_block);
        generate_phi_calls(builder, phi_lookup);
    }
}

/// generate_while_loop
///     Generates the while loop from the given WhileNode
void Generator::generate_while_loop(                                      //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const WhileNode *while_node,                                          //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
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
    llvm::Value *expression = generate_expression( //
        builder,                                   //
        parent,                                    //
        while_node->scope->get_parent(),           //
        while_node->condition.get(),               //
        allocations                                //
    );
    llvm::BranchInst *branch = builder.CreateCondBr(expression, while_blocks[1], while_blocks[2]);
    branch->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(),
                "Continue loop in '" + while_blocks[1]->getName().str() + "' based on cond '" + expression->getName().str() + "'")));

    // Create the phi lookup to detect all variable mutations done within the while loops body
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> phi_lookup;
    for (const auto &[var_name, _] : while_node->scope->get_parent()->variable_types) {
        phi_lookup[var_name] = {};
    }

    // Create the while block's body
    builder.SetInsertPoint(while_blocks[1]);
    generate_body(builder, parent, while_node->scope.get(), phi_lookup, allocations);
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        // Point back to the condition block to create the loop
        builder.CreateBr(while_blocks[0]);
    }

    // Finally set the builder to the merge block again and resolve all phi mutations
    builder.SetInsertPoint(while_blocks[2]);
    generate_phi_calls(builder, phi_lookup);
}

/// generate_for_loop
///     Generates the for loop from the given ForLoopNode
void Generator::generate_for_loop(llvm::IRBuilder<> &builder, llvm::Function *parent, const ForLoopNode *for_node) {}

/// generate_catch_statement
///     Generates the catch statement from the given CatchNode
void Generator::generate_catch_statement(                                 //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const CatchNode *catch_node,                                          //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    // The catch statement is basically just an if check if the err value of the function return is != 0 or not
    const std::optional<CallNode *> call_node = Parser::get_call_from_id(catch_node->call_id);
    if (!call_node.has_value()) {
        // Catch does not have a referenced function
        throw_err(ERR_GENERATING);
        return;
    }
    const std::string err_ret_name =
        "s" + std::to_string(call_node.value()->scope_id) + "::c" + std::to_string(call_node.value()->call_id) + "::err";
    llvm::AllocaInst *const err_var = allocations.at(err_ret_name);

    // Load the error value
    llvm::LoadInst *err_val = builder.CreateLoad(                                                    //
        llvm::Type::getInt32Ty(builder.getContext()),                                                //
        err_var,                                                                                     //
        call_node.value()->function_name + "_" + std::to_string(call_node.value()->call_id) + "_val" //
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
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> phi_lookup;
    generate_body(builder, parent, catch_node->scope.get(), phi_lookup, allocations);

    // Remove the error variable from the list of allocations
    allocations.erase(allocations.find(err_alloca_name));

    // Add branch to the merge block from the catch block if it does not contain a terminator (return or throw)
    if (catch_block->getTerminator() == nullptr) {
        builder.CreateBr(merge_block);
    }

    // Generate phi calls
    builder.SetInsertPoint(merge_block);
    generate_phi_calls(builder, phi_lookup);
}

/// generate_assignment
///     Generates the assignment from the given AssignmentNode
void Generator::generate_assignment(                                                                        //
    llvm::IRBuilder<> &builder,                                                                             //
    llvm::Function *parent,                                                                                 //
    Scope *scope,                                                                                           //
    const AssignmentNode *assignment_node,                                                                  //
    std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
) {
    llvm::Value *expression = generate_expression(builder, parent, scope, assignment_node->value.get(), allocations);

    if (scope->variable_types.find(assignment_node->var_name) == scope->variable_types.end()) {
        // Error: Undeclared Variable
        throw_err(ERR_GENERATING);
    }
    const unsigned int variable_decl_scope = scope->variable_types.at(assignment_node->var_name).second;
    llvm::AllocaInst *const lhs = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + assignment_node->var_name);

    llvm::StoreInst *store = builder.CreateStore(expression, lhs);
    store->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Store result of expr in var '" + assignment_node->var_name + "'")));

    // Set the value of the variable mutation
    phi_lookup[assignment_node->var_name].emplace_back(std::make_pair(builder.GetInsertBlock(), expression));
}

/// generate_declaration
///     Generates the declaration from the given DeclarationNode
void Generator::generate_declaration(                                     //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const DeclarationNode *declaration_node,                              //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    llvm::Value *expression = generate_expression(builder, parent, scope, declaration_node->initializer.get(), allocations);

    // Check if the declaration_node is a function call.
    // If it is, the "real" value of the call has to be extracted. Otherwise, it can be used directly!
    if (const auto *call_node = dynamic_cast<const CallNode *>(declaration_node->initializer.get())) {
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
        llvm::LoadInst *value_load = builder.CreateLoad(                     //
            get_type_from_str(parent->getContext(), declaration_node->type), //
            value_ptr,                                                       //
            declaration_node->name + "__VAL"                                 //
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

/// generate_expression
///     Generates an expression from the given ExpressionNode
llvm::Value *Generator::generate_expression(                              //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const ExpressionNode *expression_node,                                //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    if (const auto *variable_node = dynamic_cast<const VariableNode *>(expression_node)) {
        return generate_variable(builder, parent, scope, variable_node, allocations);
    }
    if (const auto *unary_op_node = dynamic_cast<const UnaryOpNode *>(expression_node)) {
        return generate_unary_op(builder, parent, unary_op_node);
    }
    if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expression_node)) {
        return generate_literal(builder, parent, literal_node);
    }
    if (const auto *call_node = dynamic_cast<const CallNode *>(expression_node)) {
        return generate_call(builder, parent, scope, call_node, allocations);
    }
    if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expression_node)) {
        return generate_binary_op(builder, parent, scope, binary_op_node, allocations);
    }
    throw_err(ERR_GENERATING);
    return nullptr;
}

/// generate_variable
///     Generates the variable from the given VariableNode
llvm::Value *Generator::generate_variable(                                //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const VariableNode *variable_node,                                    //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
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
    if (scope->variable_types.find(variable_node->name) == scope->variable_types.end()) {
        // Error: Undeclared Variable
        throw_err(ERR_GENERATING);
        return nullptr;
    }
    const unsigned int variable_decl_scope = scope->variable_types.at(variable_node->name).second;
    llvm::AllocaInst *const variable = allocations.at("s" + std::to_string(variable_decl_scope) + "::" + variable_node->name);

    // Get the type that the pointer points to
    llvm::Type *value_type = get_type_from_str(parent->getContext(), variable_node->type);

    // Load the variable's value if it's a pointer
    llvm::LoadInst *load = builder.CreateLoad(value_type, variable, variable_node->name + "_val");
    load->setMetadata("comment",
        llvm::MDNode::get(parent->getContext(),
            llvm::MDString::get(parent->getContext(), "Load val of var '" + variable_node->name + "'")));

    return load;
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
    throw_err(ERR_PARSING);
    return nullptr;
}

/// generate_call
///     Generates the call from the given CallNode
llvm::Value *Generator::generate_call(                                    //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const CallNode *call_node,                                            //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    // Get the arguments
    std::vector<llvm::Value *> args;
    args.reserve(call_node->arguments.size());
    for (const auto &arg : call_node->arguments) {
        args.emplace_back(generate_expression(builder, parent, scope, arg.get(), allocations));
    }

    // Check if it is a builtin function and call it
    if (builtin_functions.find(call_node->function_name) != builtin_functions.end()) {
        llvm::Function *builtin_function = builtins.at(builtin_functions.at(call_node->function_name));
        if (builtin_function == nullptr) {
            // Function has not been generated yet, but it should have been
            throw_err(ERR_NOT_IMPLEMENTED_YET);
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

    // Get the (already existent) function definition
    llvm::Function *func_decl = parent->getParent()->getFunction(call_node->function_name);

    // Create the call instruction using the original declaration
    llvm::CallInst *call = builder.CreateCall(func_decl, args);
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
    llvm::Value *err_ptr = builder.CreateStructGEP(res_var->getAllocatedType(), res_var, 0);
    llvm::Value *err_val = builder.CreateLoad(        //
        llvm::Type::getInt32Ty(builder.getContext()), //
        err_ptr                                       //
    );
    llvm::AllocaInst *err_var = allocations.at(call_err_name);
    builder.CreateStore(err_val, err_var);

    // Check if the call has a catch block following. If not, create an automatic re-throwing of the error value
    if (!call_node->has_catch) {
        generate_rethrow(builder, parent, call_node, allocations);
    }

    // Add the call instruction to the list of unresolved functions
    if (unresolved_functions.find(call_node->function_name) == unresolved_functions.end()) {
        unresolved_functions[call_node->function_name] = {call};
    } else {
        unresolved_functions[call_node->function_name].push_back(call);
    }

    return call;
}

/// generate_rethrow
///     Generates a catch block which re-throws the error of the call, if the call had an error
void Generator::generate_rethrow(                                         //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    const CallNode *call_node,                                            //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
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
        llvm::AllocaInst *throw_struct = generate_default_struct(builder, throw_struct_type, "throw_ret", true);
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

/// generate_binary_op
///     Generates a binary operation from the given BinaryOpNode
llvm::Value *Generator::generate_binary_op(                               //
    llvm::IRBuilder<> &builder,                                           //
    llvm::Function *parent,                                               //
    Scope *scope,                                                         //
    const BinaryOpNode *bin_op_node,                                      //
    std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
) {
    llvm::Value *lhs = generate_expression(builder, parent, scope, bin_op_node->left.get(), allocations);
    llvm::Value *rhs = generate_expression(builder, parent, scope, bin_op_node->right.get(), allocations);
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
        case TOK_LESS:
            return builder.CreateICmpSLT(lhs, rhs, "cmptmp");
        case TOK_GREATER:
            return builder.CreateICmpSGT(lhs, rhs, "cmptmp");
        case TOK_LESS_EQUAL:
            return builder.CreateICmpSLE(lhs, rhs, "cmptmp");
        case TOK_GREATER_EQUAL:
            return builder.CreateICmpSGE(lhs, rhs, "cmptmp");
        case TOK_EQUAL_EQUAL:
            return builder.CreateICmpEQ(lhs, rhs, "cmptmp");
        case TOK_NOT_EQUAL:
            return builder.CreateICmpNE(lhs, rhs, "cmptmp");
    }
}
