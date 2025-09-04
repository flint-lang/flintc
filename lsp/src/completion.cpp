#include "lsp_server.hpp"

#include "lexer/builtins.hpp"
#include "parser/parser.hpp"
#include "profiler.hpp"

void try_parse_and_add_completions(          //
    const std::filesystem::path &file_path,  //
    int line,                                //
    int character,                           //
    std::vector<CompletionItem> &completions //
) {
    const bool parse_parallel = false;
    Type::init_types();
    Resolver::add_path(file_path.filename().string(), file_path.parent_path());
    std::optional<FileNode *> file = Parser::create(file_path)->parse();
    if (!file.has_value()) {
        std::cerr << RED << "Error" << DEFAULT << ": Failed to parse file " << YELLOW << file_path.filename() << DEFAULT << std::endl;
        return;
    }
    auto dep_graph = Resolver::create_dependency_graph(file.value(), file_path.parent_path(), parse_parallel);
    Parser::resolve_all_unknown_types();
    bool parsed_successful = Parser::parse_all_open_functions(parse_parallel);
    if (!parsed_successful) {
        return;
    }

    // Add all the definitions to it
    for (const std::unique_ptr<ASTNode> &node : file.value()->definitions) {
        if (const FunctionNode *function_node = dynamic_cast<const FunctionNode *>(node.get())) {
            if (function_node->name == "_main") {
                continue;
            }
            // Remove the 'fc_' prefix from the function names
            const std::string fn_name = function_node->name.substr(3);
            completions.emplace_back(fn_name, CompletionItemKind::Function, "The '" + fn_name + "' function", fn_name);
        } else if (const DataNode *data_node = dynamic_cast<const DataNode *>(node.get())) {
            // Add the data type to the completions list
            completions.emplace_back(                                                                                  //
                data_node->name, CompletionItemKind::Class, "The '" + data_node->name + "' data type", data_node->name //
            );
        } else if (const EnumNode *enum_node = dynamic_cast<const EnumNode *>(node.get())) {
            // Add the enum type to the completions list
            completions.emplace_back(                                                                                  //
                enum_node->name, CompletionItemKind::Class, "The '" + enum_node->name + "' enum type", enum_node->name //
            );
        } else if (const VariantNode *variant_node = dynamic_cast<const VariantNode *>(node.get())) {
            // Add the variant type to the completions list
            completions.emplace_back(                                                                                              //
                variant_node->name, CompletionItemKind::Class, "The '" + variant_node->name + "' variant type", variant_node->name //
            );
        }
    }

    // Add all function definitions for all imported function modules imported in this file
    for (const auto &[module_name, import_node] : file.value()->imported_core_modules) {
        // Go through all the functions the imported Core module supports and add them as completions
        const auto &module = core_module_functions.at(module_name);
        for (const auto &[function_name, overloads] : module) {
            const std::string fn_name(function_name);
            completions.emplace_back(                                                        //
                fn_name,                                                                     //
                CompletionItemKind::Function,                                                //
                "The '" + fn_name + "' function from the '" + module_name + "' Core module", //
                fn_name                                                                      //
            );
        }
    }
}

std::vector<CompletionItem> LspServer::get_context_aware_completions(const std::string &file_path, int line, int character) {
    log_info("At Begin of get_context_aware_completions");
    static std::mutex parsing_mutex;
    std::lock_guard<std::mutex> lock(parsing_mutex);
    log_info("After Mutex of get_context_aware_completions");
    // Start with base completions
    auto completions = CompletionData::get_all_completions();
    // Try to parse and add the other completions
    Profiler::start_task("ALL");
    std::filesystem::path source_file_path(file_path);
    try_parse_and_add_completions(source_file_path, line, character, completions);

    // Cleanup
    Profiler::end_task("ALL");
    Resolver::clear();
    Parser::clear_instances();
    Type::clear_types();
    return completions;
}
