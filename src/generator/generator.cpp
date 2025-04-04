#include "generator/generator.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/parser.hpp"
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
#include <memory>
#include <mutex>
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
std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> Generator::tests;
std::unordered_map<std::string, const DataNode *const> Generator::data_nodes;

void Generator::get_data_nodes() {
    std::lock_guard<std::mutex> lock(Parser::parsed_data_mutex);
    for (const auto &file : Parser::parsed_data) {
        for (const auto &data : file.second) {
            if (data_nodes.find(data->name) != data_nodes.end()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                exit(1);
            }
            data_nodes.emplace(data->name, data);
        }
    }
}

std::unique_ptr<llvm::Module> Generator::generate_program_ir( //
    const std::string &program_name,                          //
    llvm::LLVMContext &context,                               //
    const std::shared_ptr<DepNode> &dep_graph,                //
    const bool is_test                                        //
) {
    PROFILE_SCOPE("Generate program '" + program_name + "'");
    auto builder = std::make_unique<llvm::IRBuilder<>>(context);
    auto module = std::make_unique<llvm::Module>(program_name, context);
    main_module[0] = module.get();

    // First, get all the data definitions from the parser
    get_data_nodes();

    // Generate built-in functions in the main module
    Builtin::generate_builtin_prints(builder.get(), module.get());

    // Generate all the c functions
    Builtin::generate_c_functions(module.get());

    // Generate all the "hidden" builtin string manipulation functions
    String::generate_string_manip_functions(builder.get(), module.get());

    // Generate all the "hidden" typecast helper functions
    TypeCast::generate_helper_functions(builder.get(), module.get());

    if (!is_test) {
        // Generate main function in the main module
        Builtin::generate_builtin_main(builder.get(), module.get());
    }

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
                THROW_BASIC_ERR(ERR_GENERATING);
            }
            PROFILE_SCOPE("Processing tip '" + shared_tip.get()->file_name + "'");

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
            std::unique_ptr<llvm::Module> file_module = generate_file_ir(context, shared_tip, *file, is_test);

            // Store the generated module in the resolver
            Resolver::add_ir(shared_tip->file_name, file_module.get());

            // std::cout << " -------- MODULE -------- \n"
            //           << resolve_ir_comments(get_module_ir_string(file_module.get())) << "\n ---------------- \n"
            //           << std::endl;
            // llvm::verifyModule(*file_module, &llvm::errs());

            // Link the generated module in the main module
            if (linker.linkInModule(std::move(file_module))) {
                THROW_BASIC_ERR(ERR_LINKING);
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
                    THROW_BASIC_ERR(ERR_GENERATING);
                }
                call->getCalledOperandUse().set(actual_function);
            }
        }
    }

    // Finally, create the entry point of the tests _or_ check if a main function is present
    if (is_test) {
        // Generate the entry point of the program when in test mode
        Builtin::generate_builtin_test(builder.get(), module.get());
    } else {
        // Connect the call from the main module to the actual main function declared by the user
        llvm::Function *main_function = module->getFunction("main");
        if (main_function == nullptr) {
            // No main function defined
            THROW_BASIC_ERR(ERR_GENERATING);
        }
        main_call_array[0]->getCalledOperandUse().set(main_function);
    }

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Generated IR code of the whole program\n"
                  << DEFAULT << resolve_ir_comments(get_module_ir_string(module.get())) << std::endl;
    }

    // Verify and emit the module
    llvm::verifyModule(*module, &llvm::errs());
    return module;
}

std::unique_ptr<llvm::Module> Generator::generate_file_ir( //
    llvm::LLVMContext &context,                            //
    const std::shared_ptr<DepNode> &dep_node,              //
    const FileNode &file,                                  //
    const bool is_test                                     //
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
            IR::generate_forward_declarations(module.get(), Resolver::file_map.at(weak_dep.lock()->file_name));
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
            if (is_test && function_node->name == "main") {
                continue;
            }
            Function::generate_function(module.get(), function_node);
            // No return statement found despite the signature requires return OR
            // Rerutn statement found but the signature has no return type defined (basically a simple xnor between the two booleans)

            // TODO: Because i _always_ have a return type (the error return), this does no longer work, as there can be a return type of
            // the function despite the function node not having any return types declared. This error check will be commented out for now
            // because of this reason.
            // if ((function_has_return(function_definition) ^ function_node->return_types.empty()) == 0) {
            //     throw_err(ERR_GENERATING);
            // }
        } else if (auto *test_node = dynamic_cast<TestNode *>(node.get())) {
            if (!is_test) {
                continue;
            }
            llvm::Function *test_function = Builtin::generate_test_function(module.get(), test_node);
            if (tests.count(test_node->file_name) == 0) {
                tests[test_node->file_name].emplace_back(test_node->name, test_function->getName().str());
            } else {
                tests.at(test_node->file_name).emplace_back(test_node->name, test_function->getName().str());
            }
        }
    }

    // Iterate through all unresolved function calls and resolve them to call the _actual_ mangled function, not its definition
    // Function calls to functions in outside modules already have the correct call, they dont need to be resolved here
    for (const auto &[fn_name, calls] : unresolved_functions) {
        for (llvm::CallInst *call : calls) {
            llvm::Function *actual_function = module->getFunction(fn_name + "." + std::to_string(function_mangle_ids[fn_name]));
            if (actual_function == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
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
    PROFILE_SCOPE("Resolving IR comments");
    // LLVM's automatic comments tart at the 50th character, so we will start 10 characters later
    static const unsigned int COMMENT_OFFSET = 60;

    // A segment can either be
    //      - a pair of ids, representing the start and the end of a given string or
    //      - a number representing the comment id + the index in the line the comment starts at (for the COMMENT_OFFSET)
    std::vector<std::variant<std::pair<unsigned int, unsigned int>, std::pair<int, unsigned int>>> segments;
    std::unordered_map<int, std::string> comments;

    // Go through the whole string line by line and extract all the "normal" text segments as well as the comment segments from it
    std::stringstream ir_stream(ir_string);
    std::string line;
    // The regex pattern for a comment
    const std::regex comment_id_pattern(", !comment !(\\d+)");
    const std::regex comment_pattern("^\\!(\\d+) = !\\{!\"([^\"]*)\"\\}$");
    std::smatch match;
    unsigned int substr_start_id = 0;
    unsigned int substr_end_id = 0;
    while (std::getline(ir_stream, line)) {
        // Skip empty lines
        if (line.length() == 0) {
            // Increment for the \n symbol
            substr_end_id++;
            continue;
        }

        // First, check if this line contains a '!comment !', this check is quite fast. If it contains it, we can initialize a regex search
        if (line.find("!comment !") != std::string::npos && std::regex_search(line, match, comment_id_pattern) && match.size() > 1) {
            // The line does contain a comment, so the `substr_end_id` is right where the comment starts
            substr_end_id += static_cast<unsigned int>(match.position());
            // Add the substring ids to the segments vector
            segments.emplace_back(std::make_pair(substr_start_id, substr_end_id));
            // Set the start index for the next string match to one after the match
            substr_start_id = substr_end_id + match[0].str().length() + 1;
            substr_end_id = substr_start_id;

            // Get the actual group (e.g. the commend id)
            const std::string comment_id_str = match[1].str();
            // std::cout << match[0].str() << std::endl;
            size_t pos;
            int c_id = std::stoi(comment_id_str, &pos);
            if (pos != comment_id_str.length()) {
                std::cout << "Failed to convert number: " << comment_id_str << std::endl;
                THROW_BASIC_ERR(ERR_GENERATING);
                return "";
            }
            // Add the commend id to the segments list
            segments.emplace_back(std::make_pair(c_id, static_cast<unsigned int>(match.position())));
            continue;
        } else if (line.find("!{!\"") != std::string::npos && std::regex_search(line, match, comment_pattern) && match.size() > 2) {
            // This line is a comment line already, if it is, only comment lines will follow
            // Add everything until here to the segments list
            segments.emplace_back(std::make_pair(substr_start_id, substr_end_id));

            // Get the actual group (e.g. the commend id)
            const std::string comment_id_str = match[1].str();
            size_t pos;
            unsigned int c_id = static_cast<unsigned int>(std::stoi(comment_id_str, &pos));
            if (pos != comment_id_str.length()) {
                std::cout << "Failed to convert number: " << comment_id_str << std::endl;
                THROW_BASIC_ERR(ERR_GENERATING);
                return "";
            }
            if (comments.find(c_id) != comments.end()) {
                std::cout << "Comment " << comment_id_str << " already exists in comments map!" << std::endl;
                THROW_BASIC_ERR(ERR_GENERATING);
                return "";
            }
            // Add the comment to the comments map
            comments.emplace(c_id, match[2].str());
            break;
        }
        // Does not contain a comment, so we just increment the end_id and move on to the next line
        substr_end_id += static_cast<unsigned int>(line.length() + 1);
    }
    while (std::getline(ir_stream, line)) {
        // First, check this line is a comment line already, if it is, only comment lines will follow
        if (std::regex_search(line, match, comment_pattern) && match.size() > 2) {
            // Get the actual group (e.g. the commend id)
            const std::string comment_id_str = match[1].str();
            size_t pos;
            int c_id = std::stoi(comment_id_str, &pos);
            if (pos != comment_id_str.length()) {
                std::cout << "Failed to convert number: " << comment_id_str << std::endl;
                THROW_BASIC_ERR(ERR_GENERATING);
                return "";
            }
            if (comments.find(c_id) != comments.end()) {
                std::cout << "Comment " << comment_id_str << " already exists in comments map!" << std::endl;
                THROW_BASIC_ERR(ERR_GENERATING);
                return "";
            }
            // Add the comment to the comments map
            comments.emplace(c_id, match[2].str());
        } else {
            break;
        }
    }

    // Finally, put the whole thing together
    std::stringstream result;
    for (auto &segment : segments) {
        if (std::holds_alternative<std::pair<int, unsigned int>>(segment)) {
            // Add the comment string to the result
            auto &[comment_id, start_idx] = std::get<std::pair<int, unsigned int>>(segment);
            if (start_idx < COMMENT_OFFSET) {
                result << std::string(COMMENT_OFFSET - start_idx, ' ');
            }
            if (comments.find(comment_id) == comments.end()) {
                std::cout << "Comment of ID " << comment_id << " could not be found in the comments map!" << std::endl;
                exit(1);
            }
            result << "; " << comments[comment_id];
        } else {
            auto &[substr_start, substr_end] = std::get<std::pair<unsigned int, unsigned int>>(segment);
            const std::string &ir_substr = ir_string.substr(substr_start, substr_end - substr_start);
            result << "\n" << ir_substr;
        }
    }
    return result.str();
}
