#include "generator/generator.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "profiler.hpp"
#include "resolver/resolver.hpp"

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

#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

std::unordered_map<std::string, llvm::StructType *> Generator::type_map;
std::unordered_map<std::string, std::vector<llvm::CallInst *>> Generator::unresolved_functions;
std::unordered_map<std::string, std::unordered_map<std::string, std::vector<llvm::CallInst *>>> Generator::file_unresolved_functions;
std::unordered_map<std::string, unsigned int> Generator::function_mangle_ids;
std::unordered_map<std::string, std::unordered_map<std::string, unsigned int>> Generator::file_function_mangle_ids;
std::unordered_map<std::string, std::vector<std::string>> Generator::file_function_names;
std::vector<std::string> Generator::function_names;
std::array<llvm::CallInst *, 1> Generator::main_call_array;
std::array<llvm::Module *, 1> Generator::main_module;

std::unique_ptr<llvm::Module> Generator::generate_program_ir( //
    const std::string &program_name,                          //
    llvm::LLVMContext &context,                               //
    std::shared_ptr<DepNode> &dep_graph                       //
) {
    PROFILE_SCOPE("Generate program IR");
    auto builder = std::make_unique<llvm::IRBuilder<>>(context);
    auto module = std::make_unique<llvm::Module>(program_name, context);
    main_module[0] = module.get();

    // Generate built-in functions in the main module
    Builtin::generate_builtin_prints(builder.get(), module.get());

    // Generate main function in the main module
    Builtin::generate_builtin_main(builder.get(), module.get());

    // std::cout << " -------- MAIN -------- \n"
    //           << resolve_ir_comments(get_module_ir_string(module.get())) << "\n ---------------- \n"
    //           << std::endl;

    // for (const auto &func : main_module[0]->getFunctionList()) {
    //     std::cout << "FuncName: " << func.getName().str() << std::endl;
    // }

    llvm::Linker linker(*module);

    // Start with the tips of the dependency graph and then work towards the root node. First, get all tips of the graph:
    std::vector<std::weak_ptr<DepNode>> tips;
    Resolver::get_dependency_graph_tips(dep_graph, tips);
    // If tips are empty, only a single file needs to be generated
    if (tips.empty()) {
        tips.emplace_back(dep_graph);
    }

    while (!tips.empty()) {
        std::vector<std::weak_ptr<DepNode>> new_tips;
        // Go through all tips and generate their respective IR code and link them to the main module
        for (const std::weak_ptr<DepNode> &dep : tips) {
            const auto shared_tip = dep.lock();
            if (!shared_tip) {
                // DepNode somehow does not exist any more
                throw_err(ERR_GENERATING);
            }
            // Add the dependencies root only if all dependants of its root have been compiled
            // Or add it when only weak dependants have not been compiled yet (the content of the file will be forward-declared)
            if (shared_tip->root != nullptr) {
                std::vector<std::string> tips_names;
                tips_names.reserve(tips.size());
                for (const auto &tip : tips) {
                    tips_names.push_back(tip.lock()->file_name);
                }

                bool dependants_compiled = true;
                // Check if the dependencies have been compiled already or will be compiled in this phase
                // If the dependency is weak, it does not matter if it already waas compiled or not
                for (const auto &dependant : shared_tip->root->dependencies) {
                    if (std::holds_alternative<std::shared_ptr<DepNode>>(dependant)) {
                        std::shared_ptr<DepNode> shared_dep = std::get<std::shared_ptr<DepNode>>(dependant);
                        if (Resolver::module_map.find(shared_dep->file_name) == Resolver::module_map.end() //
                            && std::find(tips_names.begin(), tips_names.end(), shared_dep->file_name) == tips_names.end()) {
                            dependants_compiled = false;
                        }
                    }
                }
                if (dependants_compiled) {
                    new_tips.emplace_back(shared_tip->root);
                }
            }

            // Check if this file has already been generated. If so, skip it
            if (Resolver::module_map.find(shared_tip->file_name) != Resolver::module_map.end()) {
                continue;
            }

            // Generate the IR code from the given FileNode
            const FileNode *file = &Resolver::file_map.at(shared_tip->file_name);
            std::unique_ptr<llvm::Module> file_module = generate_file_ir(builder.get(), context, shared_tip, *file);

            // Store the generated module in the resolver
            Resolver::add_ir(shared_tip->file_name, file_module.get());

            // std::cout << " -------- MODULE -------- \n"
            //           << resolve_ir_comments(get_module_ir_string(file_module.get())) << "\n ---------------- \n"
            //           << std::endl;

            // Link the generated module in the main module
            if (linker.linkInModule(std::move(file_module))) {
                throw_err(ERR_LINKING);
            }
        }

        tips.assign(new_tips.begin(), new_tips.end());
    }

    // Resolve all inter-module calls
    for (const auto &[file_name, unresolved_calls] : file_unresolved_functions) {
        for (const auto &[fn_name, calls] : unresolved_calls) {
            for (llvm::CallInst *call : calls) {
                std::string mangle_id_string = std::to_string(file_function_mangle_ids[file_name][fn_name]);
                llvm::Function *actual_function = module->getFunction(fn_name + "." + mangle_id_string);
                if (actual_function == nullptr) {
                    throw_err(ERR_GENERATING);
                }
                call->getCalledOperandUse().set(actual_function);
            }
        }
    }

    // Connect the call from the main module to the actual main function declared by the user
    llvm::Function *main_function = module->getFunction("main_custom");
    if (main_function == nullptr) {
        // No main function defined
        throw_err(ERR_GENERATING);
    }
    main_call_array[0]->getCalledOperandUse().set(main_function);

    std::cout << " -------- IR-CODE -------- \n"
              << resolve_ir_comments(get_module_ir_string(module.get())) << "\n ---------------- \n"
              << std::endl;

    // Verify and emit the module
    llvm::verifyModule(*module, &llvm::errs());
    return module;
}

std::unique_ptr<llvm::Module> Generator::generate_file_ir( //
    llvm::IRBuilder<> *builder,                            //
    llvm::LLVMContext &context,                            //
    const std::shared_ptr<DepNode> &dep_node,              //
    const FileNode &file                                   //
) {
    PROFILE_SCOPE("Generate IR for '" + file.file_name + "'");
    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>(dep_node->file_name, context);

    // Declare the built-in functions in the file module to reference the main module's versions
    for (auto &builtin_func : builtins) {
        if (builtin_func.second != nullptr) {
            module->getOrInsertFunction(               //
                builtin_func.second->getName(),        //
                builtin_func.second->getFunctionType() //
            );
        }
    }
    // Declare the built-in print functions in the file module to reference the main module's versions
    for (auto &prints : print_functions) {
        if (prints.second != nullptr) {
            module->getOrInsertFunction(         //
                prints.second->getName(),        //
                prints.second->getFunctionType() //
            );
        }
    }
    // Forward declare all functions from all files where this file has a wak reference to
    for (const auto &dep : dep_node->dependencies) {
        if (std::holds_alternative<std::weak_ptr<DepNode>>(dep)) {
            std::weak_ptr<DepNode> weak_dep = std::get<std::weak_ptr<DepNode>>(dep);
            IR::generate_forward_declarations(*builder, module.get(), Resolver::file_map.at(weak_dep.lock()->file_name));
        }
    }

    unsigned int mangle_id = 1;
    file_function_names[file.file_name] = {};
    // Declare all functions in the file at the top of the module
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            // Create a forward declaration for the function only if it is not the main function!
            if (function_node->name != "main") {
                llvm::FunctionType *function_type = Function::generate_function_type(context, function_node);
                module->getOrInsertFunction(function_node->name, function_type);
                function_mangle_ids[function_node->name] = mangle_id++;
                file_function_names.at(file.file_name).emplace_back(function_node->name);
            }
        }
    }
    function_names = file_function_names.at(file.file_name);
    // Store the mangle ids of this file within the file_function_mangle_ids
    file_function_mangle_ids[file.file_name] = function_mangle_ids;

    // Iterate through all AST Nodes in the file and generate them accordingly (only functions for now!)
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            llvm::Function *function_definition = Function::generate_function(module.get(), function_node);
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
    // Function calls to functions in outside modules already have the correct call, they dont need to be resolved here
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
    function_names.clear();

    return module;
}

std::string Generator::get_module_ir_string(const llvm::Module *module) {
    std::string ir_string;
    llvm::raw_string_ostream stream(ir_string);
    module->print(stream, nullptr);
    stream.flush();
    return ir_string;
}

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
