#include "lsp_server.hpp"

#include "lexer/builtins.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "resolver/resolver.hpp"

void LspServer::add_nodes_from_namespace_to_completions( //
    const Namespace *file_namespace,                     //
    std::vector<CompletionItem> &completions,            //
    std::vector<const ImportNode *> &imported_files,     //
    const bool is_root_file                              //
) {
    for (const std::unique_ptr<DefinitionNode> &definition : file_namespace->public_symbols.definitions) {
        switch (definition->get_variation()) {
            case DefinitionNode::Variation::DATA: {
                const auto *node = definition->as<DataNode>();
                completions.emplace_back(node->name, CompletionItemKind::Class, "The '" + node->name + "' data type", node->name, false);
                break;
            }
            case DefinitionNode::Variation::ENTITY:
                // Entities are not supported yet
                break;
            case DefinitionNode::Variation::ENUM: {
                const auto *node = definition->as<EnumNode>();
                completions.emplace_back(node->name, CompletionItemKind::Class, "The '" + node->name + "' enum type", node->name, false);
                break;
            }
            case DefinitionNode::Variation::ERROR: {
                const auto *node = definition->as<ErrorNode>();
                completions.emplace_back(node->name, CompletionItemKind::Class, "The '" + node->name + "' error type", node->name, false);
                break;
            }
            case DefinitionNode::Variation::FUNC:
                // Func modules are not supported yet
                break;
            case DefinitionNode::Variation::FUNCTION: {
                const auto *node = definition->as<FunctionNode>();
                if (node->name == "_main") {
                    continue;
                }
                completions.emplace_back(node->name, CompletionItemKind::Function, "The '" + node->name + "' function", node->name, false);
                break;
            }
            case DefinitionNode::Variation::IMPORT: {
                const auto *node = definition->as<ImportNode>();
                if (!is_root_file) {
                    // Only add imports if it's the root node we are in
                    continue;
                }
                // Only add "real" file imports to the list of imported files, skip library imports like 'use Core.xxx'
                if (std::holds_alternative<Hash>(node->path)) {
                    imported_files.emplace_back(node);
                }
                break;
            }
            case DefinitionNode::Variation::LINK:
                // Links are not supported yet
                break;
            case DefinitionNode::Variation::TEST:
                // Tests don't need any completions since they cannot be referenced
                break;
            case DefinitionNode::Variation::VARIANT: {
                const auto *node = definition->as<VariantNode>();
                completions.emplace_back(node->name, CompletionItemKind::Class, "The '" + node->name + "' variant type", node->name, false);
                break;
            }
        }
    }
}

void LspServer::try_parse_and_add_completions( //
    const std::string &file_path,              //
    [[maybe_unused]] int line,                 //
    [[maybe_unused]] int character,            //
    std::vector<CompletionItem> &completions   //
) {
    // Parse the program
    std::optional<FileNode *> file = LspServer::parse_program(file_path, std::nullopt);
    if (!file.has_value()) {
        return;
    }

    // Add all the definitions to the completions and collect all imported file nodes
    std::vector<const ImportNode *> imported_files;
    add_nodes_from_namespace_to_completions(file.value()->file_namespace.get(), completions, imported_files, true);

    // Add all the definitions from the files directly imported in this file
    for (const auto &imported_file : imported_files) {
        std::optional<Namespace *> file_namespace = Resolver::get_namespace_from_hash(imported_file->file_hash);
        if (!file_namespace.has_value()) {
            continue;
        }
        add_nodes_from_namespace_to_completions(file_namespace.value(), completions, imported_files, false);
    }

    // Add all function definitions for all core modules imported in this file
    for (const auto &[module_name, import_node] : file.value()->imported_core_modules) {
        // Go through all the functions the imported Core module supports and add them as completions
        const function_overload_list &module = core_module_functions.at(module_name);
        for (const auto &[function_name, overloads] : module) {
            const std::string fn_name(function_name);
            completions.emplace_back(                                                        //
                fn_name,                                                                     //
                CompletionItemKind::Function,                                                //
                "The '" + fn_name + "' function from the '" + module_name + "' Core module", //
                fn_name,                                                                     //
                false                                                                        //
            );
        }
    }
}

std::vector<CompletionItem> LspServer::get_context_aware_completions(const std::string &file_path, int line, int character) {
    // Start with base completions
    auto completions = CompletionData::get_all_completions();
    // Try to parse and add the other completions
    try_parse_and_add_completions(file_path, line, character, completions);

    // Cleanup
    return completions;
}
