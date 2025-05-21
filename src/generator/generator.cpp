#include "generator/generator.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "globals.hpp"
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
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h> // Container for the IR code
#include <llvm/IR/Type.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

void Generator::get_data_nodes() {
    std::lock_guard<std::mutex> lock(Parser::parsed_data_mutex);
    for (const auto &file : Parser::parsed_data) {
        for (const auto *data : file.second) {
            if (data_nodes.find(data->name) != data_nodes.end()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                exit(1);
            }
            data_nodes.emplace(data->name, data);
        }
    }
}

std::filesystem::path Generator::get_flintc_cache_path() {
#ifdef __WIN32__
    const char *local_appdata = std::getenv("LOCALAPPDATA");
    if (local_appdata == nullptr) {
        return std::filesystem::path();
    }
    std::filesystem::path cache_path = std::filesystem::path(local_appdata) / "Flint" / "Cache" / "flintc";
#else
    const char *home = std::getenv("HOME");
    if (home == nullptr) {
        return std::filesystem::path();
    }
    std::filesystem::path home_path = std::filesystem::path(home);
    std::filesystem::path cache_path = home_path / ".cache" / "flintc";
#endif
    // Check if the cache path exists, if not create directories, like `mkdir -p`
    try {
        if (!std::filesystem::exists(cache_path)) {
            std::filesystem::create_directories(cache_path);
        }
        return cache_path;
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error: Could not create cache path: '" << cache_path.string() << "'" << std::endl;
        return std::filesystem::path();
    }
}

bool Generator::compile_module(llvm::Module *module, const std::filesystem::path &module_path) {
    PROFILE_SCOPE("Compile module " + module->getName().str());
    // Initialize LLVM targets (should only be called once in the compiler)
    static bool initialized = false;
    if (!initialized) {
        PROFILE_SCOPE("Initialize LLVM");
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86Target();
        LLVMInitializeX86TargetMC();
        LLVMInitializeX86AsmParser();
        LLVMInitializeX86AsmPrinter();
        initialized = true;
    }

    // Get the target triple (architecture, OS, etc.)
#ifdef __WIN32__
    std::string target_triple = "x86_64-pc-windows-msvc";
#else
    std::string target_triple = llvm::sys::getDefaultTargetTriple();
#endif

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Target triple information" << DEFAULT << "\n" << target_triple << "\n" << std::endl;
    }
    module->setTargetTriple(target_triple);

    std::string error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        llvm::errs() << "Error: " << error << "\n";
        return false;
    }

    // Create the target machine
    llvm::TargetOptions opt;
    auto target_machine = target->createTargetMachine( //
        target_triple,                                 //
        llvm::sys::getHostCPUName(),                   //
        "",                                            //
        opt,                                           //
        llvm::Reloc::Static                            //
    );
    // Disable individual sections for functions and data
    target_machine->Options.FunctionSections = false;
    target_machine->Options.DataSections = false;
    module->setDataLayout(target_machine->createDataLayout());

    // Create an output file
    std::error_code EC;
#ifdef __WIN32__
    const std::string obj_file = module_path.string() + ".obj";
#else
    const std::string obj_file = module_path.string() + ".o";
#endif
    llvm::raw_fd_ostream dest(obj_file, EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message() << "\n";
        return false;
    }

    // Set up the pass manager and code generation
    llvm::legacy::PassManager pass;
    llvm::CodeGenFileType fileType = llvm::CodeGenFileType::ObjectFile;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        llvm::errs() << "TargetMachine can't emit a file of this type!\n";
        return false;
    }

    if (DEBUG_MODE) {
        verify_module(module);
    }

    // Run the passes to generate machine code
    Profiler::start_task("Generate machine code");
    pass.run(*module);
    dest.flush(); // Ensure the file is written
    Profiler::end_task("Generate machine code");

    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Code generation status" << DEFAULT << std::endl;
        std::cout << "-- Machine code generated: " << obj_file << "\n" << std::endl;
    }
    return true;
}

bool Generator::verify_module(llvm::Module *module) {
    // Capture verification errors in a string
    std::string errorOutput;
    llvm::raw_string_ostream errorStream(errorOutput);

    // Verify the module, capturing errors in errorStream
    bool hasErrors = llvm::verifyModule(*module, &errorStream);
    errorStream.flush(); // Make sure all output is written

    // Process the errors - filter out "Referencing function in another module!" errors
    std::istringstream ss(errorOutput);
    std::string line;
    bool hasRealErrors = false;
    std::string filteredErrors;

    unsigned char lines_to_skip = 0;
    while (std::getline(ss, line)) {
        if (line.find("Referencing function in another module!") != std::string::npos) {
            lines_to_skip = 4;
        } else if (lines_to_skip > 0) {
            lines_to_skip--;
        } else {
            filteredErrors += line + "\n";
            hasRealErrors = true;
        }
    }

    // Only print the module if there are real errors
    if (hasRealErrors) {
        llvm::errs() << filteredErrors;
        std::cout << "\n\n -------- MODULE WITH ERRORS -------- \n"
                  << resolve_ir_comments(get_module_ir_string(module)) << "\n ---------------- \n"
                  << std::endl;
        return false;
    } else if (hasErrors) {
        // Optionally print a message indicating only reference "errors" were found
        std::cout << YELLOW << "[Debug Info] Module verification for module '" << module->getName().str() << "' successful\n" << std::endl;
        return true;
    }
    return true;
}

std::optional<std::unique_ptr<llvm::Module>> Generator::generate_program_ir( //
    const std::string &program_name,                                         //
    const std::shared_ptr<DepNode> &dep_graph,                               //
    const bool is_test                                                       //
) {
    Profiler::start_task("Generate builtin libraries");
    if (!Module::generate_modules()) {
        std::cerr << "Error: Failed to generate builtin modules. aborting..." << std::endl;
        abort();
    }
    Profiler::end_task("Generate builtin libraries");

    PROFILE_SCOPE("Generate program '" + program_name + "'");
    auto builder = std::make_unique<llvm::IRBuilder<>>(context);
    auto module = std::make_unique<llvm::Module>(program_name, context);
    main_module[0] = module.get();

    // First, get all the data definitions from the parser
    get_data_nodes();

    // Generate all the c functions
    Builtin::generate_c_functions(module.get());

    // Generate built-in functions in the main module
    Module::Print::generate_print_functions(builder.get(), module.get());

    // Generate all the "hidden" string helper functions
    Module::String::generate_string_manip_functions(builder.get(), module.get());

    // Generate all the "hidden" read helper functions
    Module::Read::generate_read_functions(builder.get(), module.get());

    // Generate all the "hidden" typecast helper functions
    Module::TypeCast::generate_typecast_functions(builder.get(), module.get());

    // Generate all the "hidden" arithmetic helper functions
    Module::Arithmetic::generate_arithmetic_functions(builder.get(), module.get());

    // Gneerate all the "hidden" array helper functions
    Module::Array::generate_array_manip_functions(builder.get(), module.get());

    // Generate all the "hidden" assert helper functions
    Module::Assert::generate_assert_functions(builder.get(), module.get());

    // Generate all the "hidden" filesystem helper functions
    Module::FS::generate_filesystem_functions(builder.get(), module.get());

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
    std::vector<std::string> added_file_names;

    while (!tips.empty()) {
        std::vector<std::weak_ptr<DepNode>> new_tips;
        // Go through all tips and generate their respective IR code and link them to the main module
        for (const std::weak_ptr<DepNode> &dep : tips) {
            const auto shared_tip = dep.lock();
            if (!shared_tip) {
                // DepNode somehow does not exist any more
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
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
                        if (!Resolver::generated_files_contain(shared_dep->file_name) //
                            && std::find(tips_names.begin(), tips_names.end(), shared_dep->file_name) == tips_names.end()) {
                            dependants_compiled = false;
                        }
                    }
                }
                if (dependants_compiled &&                                                                                             //
                    std::find(added_file_names.begin(), added_file_names.end(), shared_tip->root->file_name) == added_file_names.end() //
                ) {
                    new_tips.emplace_back(shared_tip->root);
                    added_file_names.emplace_back(shared_tip->root->file_name);
                }
            }

            // Check if this file has already been generated. If so, skip it
            if (Resolver::generated_files_contain(shared_tip->file_name)) {
                continue;
            }

            // Generate the IR code from the given FileNode
            const FileNode *file = Resolver::get_file_from_name(shared_tip->file_name);
            std::optional<std::unique_ptr<llvm::Module>> file_module = generate_file_ir(shared_tip, *file, is_test);
            if (!file_module.has_value()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }

            // Store that this file is now finished with its generation
            Resolver::file_generation_finished(shared_tip->file_name);

            if (DEBUG_MODE) {
                if (!verify_module(file_module.value().get())) {
                    THROW_BASIC_ERR(ERR_LINKING);
                    return std::nullopt;
                }
            }

            // Link the generated module in the main module
            if (linker.linkInModule(std::move(file_module.value()))) {
                THROW_BASIC_ERR(ERR_LINKING);
                return std::nullopt;
            }
        }

        tips.assign(new_tips.begin(), new_tips.end());
        added_file_names.clear();
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
        llvm::Function *main_function = module->getFunction("_main");
        if (main_function == nullptr) {
            // No main function defined
            THROW_BASIC_ERR(ERR_GENERATING);
        }
        main_call_array[0]->getCalledOperandUse().set(main_function);
    }

    if (DEBUG_MODE && PRINT_IR_PROGRAM) {
        std::cout << YELLOW << "[Debug Info] Generated IR code of the whole program\n"
                  << DEFAULT << resolve_ir_comments(get_module_ir_string(module.get())) << std::endl;
    }

    // Verify and emit the module
    llvm::verifyModule(*module, &llvm::errs());
    return module;
}

std::optional<std::unique_ptr<llvm::Module>> Generator::generate_file_ir( //
    const std::shared_ptr<DepNode> &dep_node,                             //
    const FileNode &file,                                                 //
    const bool is_test                                                    //
) {
    PROFILE_SCOPE("Generate IR for '" + file.file_name + "'");
    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>(dep_node->file_name, context);

    // Declare all the core functions that are used in every file nonetheless
    Builtin::generate_c_functions(module.get());
    Module::Arithmetic::generate_arithmetic_functions(nullptr, module.get(), true);
    Module::Array::generate_array_manip_functions(nullptr, module.get(), true);
    Module::String::generate_string_manip_functions(nullptr, module.get(), true);
    Module::TypeCast::generate_typecast_functions(nullptr, module.get(), true);

    for (auto &imported_core_module : file.imported_core_modules) {
        const std::string &core_module_name = imported_core_module.first;
        if (core_module_name == "print") {
            Module::Print::generate_print_functions(nullptr, module.get(), true);
        } else if (core_module_name == "read") {
            Module::Read::generate_read_functions(nullptr, module.get(), true);
        } else if (core_module_name == "assert") {
            Module::Assert::generate_assert_functions(nullptr, module.get(), true);
        } else if (core_module_name == "fs") {
            Module::FS::generate_filesystem_functions(nullptr, module.get(), true);
        }
    }
    // Forward declare all functions from all files where this file has a weak reference to
    for (const auto &dep : dep_node->dependencies) {
        if (std::holds_alternative<std::weak_ptr<DepNode>>(dep)) {
            std::weak_ptr<DepNode> weak_dep = std::get<std::weak_ptr<DepNode>>(dep);
            IR::generate_forward_declarations(module.get(), *Resolver::get_file_from_name(weak_dep.lock()->file_name));
        }
    }

    unsigned int mangle_id = 1;
    file_function_names[file.file_name] = {};
    // Declare all functions in the file at the top of the module
    for (const std::unique_ptr<ASTNode> &node : file.definitions) {
        if (auto *function_node = dynamic_cast<FunctionNode *>(node.get())) {
            // Create a forward declaration for the function only if it is not the main function!
            if (function_node->name != "_main") {
                llvm::FunctionType *function_type = Function::generate_function_type(function_node);
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
            if (is_test && function_node->name == "_main") {
                continue;
            }
            if (!Function::generate_function(module.get(), function_node, file.imported_core_modules)) {
                return std::nullopt;
            }
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
            std::optional<llvm::Function *> test_function = Function::generate_test_function( //
                module.get(), test_node, file.imported_core_modules                           //
            );
            if (!test_function.has_value()) {
                THROW_BASIC_ERR(ERR_GENERATING);
                return std::nullopt;
            }
            if (tests.count(test_node->file_name) == 0) {
                tests[test_node->file_name].emplace_back(test_node->name, test_function.value()->getName().str());
            } else {
                tests.at(test_node->file_name).emplace_back(test_node->name, test_function.value()->getName().str());
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
                return std::nullopt;
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
    // Check if the whole ir_string even contains a single comment at all. If it doesnt, we return the ir_string as is
    if (ir_string.find(", !comment !") == std::string::npos) {
        return ir_string;
    }

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
