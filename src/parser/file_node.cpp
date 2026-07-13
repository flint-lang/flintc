#include "parser/ast/file_node.hpp"
#include "error/error.hpp"
#include "parser/parser.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/variant_type.hpp"

#include <utility>

std::optional<ImportNode *> FileNode::add_import(ImportNode &import) {
    auto &imports = file_namespace->public_symbols.imports;
    // Check if we already imported that core module before
    for (const auto &imported : imports) {
        if (import.path == imported->path) {
            // Imported same file / core module
            THROW_ERR(ErrImportSameFileTwice, ERR_PARSING, file_namespace->namespace_hash, &import);
            return std::nullopt;
        }
    }
    imports.emplace_back(std::make_unique<ImportNode>(std::move(import)));
    ImportNode *added_import = static_cast<ImportNode *>(imports.back().get());
    if (std::holds_alternative<std::vector<std::string>>(added_import->path)) {
        const std::vector<std::string> &import_vec = std::get<std::vector<std::string>>(added_import->path);
        if (import_vec.size() == 2 && import_vec.front() == "Core") {
            imported_core_modules.emplace(import_vec.back(), added_import);
        }
    }
    return added_import;
}

std::optional<DataNode *> FileNode::add_data(DataNode &data) {
    auto &definitions = file_namespace->public_symbols.definitions;
    definitions.emplace_back(std::make_unique<DataNode>(std::move(data)));
    DataNode *added_data = static_cast<DataNode *>(definitions.back().get());
    if (!file_namespace->add_type(std::make_shared<DataType>(added_data))) {
        THROW_ERR(ErrDefDataRedefinition, ERR_PARSING, file_hash, added_data->line, added_data->column, added_data->name);
        return std::nullopt;
    }
    // Add all global variables to the public section of this files namespace
    if (added_data->is_shared) {
        for (const auto &field : added_data->fields) {
            const std::string mangled_name = added_data->file_hash.to_string() + ".shared." + added_data->name + "." + field.name;
            file_namespace->public_symbols.globals.emplace(mangled_name,
                Scope::Variable{
                    .type = field.type,
                    .scope_id = 0,
                    .scope_segment = 0,
                    .is_mutable = true,
                    .is_persistent = false,
                    .is_fn_param = false,
                    .is_global = true,
                    .file_hash = added_data->file_hash,
                });
        }
    }
    return added_data;
}

std::optional<FuncNode *> FileNode::add_func(FuncNode &func) {
    auto &definitions = file_namespace->public_symbols.definitions;
    definitions.emplace_back(std::make_unique<FuncNode>(std::move(func)));
    FuncNode *added_func = static_cast<FuncNode *>(definitions.back().get());
    if (!file_namespace->add_type(std::make_shared<FuncType>(added_func))) {
        THROW_ERR(ErrDefFuncRedefinition, ERR_PARSING, file_hash, added_func->line, added_func->column, added_func->name);
        return std::nullopt;
    }
    return added_func;
}

std::optional<ObjectNode *> FileNode::add_object(ObjectNode &object) {
    auto &definitions = file_namespace->public_symbols.definitions;
    definitions.emplace_back(std::make_unique<ObjectNode>(std::move(object)));
    ObjectNode *added_object = static_cast<ObjectNode *>(definitions.back().get());
    if (!file_namespace->add_type(std::make_shared<ObjectType>(added_object))) {
        THROW_ERR(ErrDefFuncRedefinition, ERR_PARSING, file_hash, added_object->line, added_object->column, added_object->name);
        return std::nullopt;
    }
    return added_object;
}

std::optional<FunctionNode *> FileNode::add_function(                                  //
    FunctionNode &function,                                                            //
    const std::unordered_map<std::string, std::unique_ptr<Namespace>> &core_namespaces //
) {
    auto &public_definitions = file_namespace->public_symbols.definitions;
    for (const auto &def : public_definitions) {
        if (def->get_variation() != DefinitionNode::Variation::FUNCTION) {
            continue;
        }
        const auto *fn_def = def->as<FunctionNode>();
        if (function.name != fn_def->name) {
            // They do not share the same name, skip
            continue;
        }
        if (function.parameters.size() != fn_def->parameters.size()) {
            // Not the same parameter count, skip
            continue;
        }
        bool all_the_same = true;
        for (size_t i = 0; i < function.parameters.size(); i++) {
            const auto &new_fn_param_ty = std::get<0>(function.parameters.at(i));
            const auto &existing_fn_param_ty = std::get<0>(fn_def->parameters.at(i));
            if (!new_fn_param_ty->equals(existing_fn_param_ty)) {
                all_the_same = false;
                break;
            }
        }
        if (all_the_same) {
            THROW_ERR(ErrFnRedefinition, ERR_PARSING, file_namespace->namespace_hash, &function, fn_def);
            return std::nullopt;
        }
    }
    auto &private_functions = file_namespace->private_symbols.functions;
    for (const auto &[hash, fn_list] : private_functions) {
        for (const auto &fn_def : fn_list) {
            if (function.name != fn_def->name) {
                // They do not share the same name, skip
                continue;
            }
            if (function.parameters.size() != fn_def->parameters.size()) {
                // Not the same parameter count, skip
                continue;
            }
            bool all_the_same = true;
            for (size_t i = 0; i < function.parameters.size(); i++) {
                const auto &new_fn_param_ty = std::get<0>(function.parameters.at(i));
                const auto &existing_fn_param_ty = std::get<0>(fn_def->parameters.at(i));
                if (!new_fn_param_ty->equals(existing_fn_param_ty)) {
                    all_the_same = false;
                    break;
                }
            }
            if (all_the_same) {
                THROW_ERR(ErrFnRedefinition, ERR_PARSING, file_namespace->namespace_hash, &function, fn_def);
                return std::nullopt;
            }
        }
    }
    for (const auto &[module_name, import] : imported_core_modules) {
        // Get the function list of the imported core module namespace
        const auto &core_namespace = core_namespaces.at(module_name);
        for (const auto &def : core_namespace->public_symbols.definitions) {
            if (def->get_variation() != DefinitionNode::Variation::FUNCTION) {
                continue;
            }
            const auto *fn_def = def->as<FunctionNode>();
            if (function.name != fn_def->name) {
                // They do not share the same name, skip
                continue;
            }
            if (function.parameters.size() != fn_def->parameters.size()) {
                // Not the same parameter count, skip
                continue;
            }
            bool all_the_same = true;
            for (size_t i = 0; i < function.parameters.size(); i++) {
                const auto &new_fn_param_ty = std::get<0>(function.parameters.at(i));
                const auto &existing_fn_param_ty = std::get<0>(fn_def->parameters.at(i));
                if (!new_fn_param_ty->equals(existing_fn_param_ty)) {
                    all_the_same = false;
                    break;
                }
            }
            if (all_the_same) {
                THROW_ERR(ErrFnRedefinition, ERR_PARSING, file_namespace->namespace_hash, &function, fn_def);
                return std::nullopt;
            }
        }
    }
    public_definitions.emplace_back(std::make_unique<FunctionNode>(std::move(function)));
    FunctionNode *added_function = static_cast<FunctionNode *>(public_definitions.back().get());
    if (added_function->name == "_main") {
        ASSERT(Parser::main_function.load() == nullptr);
        Parser::main_function.store(added_function, std::memory_order_release);
    }
    if (added_function->scope.has_value()) {
        added_function->scope.value()->function = added_function;
    }
    return added_function;
}

bool FileNode::add_enum(EnumNode &enum_node) {
    auto &definitions = file_namespace->public_symbols.definitions;
    definitions.emplace_back(std::make_unique<EnumNode>(std::move(enum_node)));
    EnumNode *added_enum = static_cast<EnumNode *>(definitions.back().get());
    if (!file_namespace->add_type(std::make_shared<EnumType>(added_enum))) {
        // Enum redifinition
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    return true;
}

bool FileNode::add_error(ErrorNode &error) {
    auto &definitions = file_namespace->public_symbols.definitions;
    definitions.emplace_back(std::make_unique<ErrorNode>(std::move(error)));
    ErrorNode *added_error = static_cast<ErrorNode *>(definitions.back().get());
    if (!file_namespace->add_type(std::make_shared<ErrorSetType>(added_error))) {
        // Error Set redefinition or naming collision
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    return true;
}

bool FileNode::add_variant(VariantNode &variant) {
    auto &definitions = file_namespace->public_symbols.definitions;
    definitions.emplace_back(std::make_unique<VariantNode>(std::move(variant)));
    VariantNode *added_variant = static_cast<VariantNode *>(definitions.back().get());
    if (!file_namespace->add_type(std::make_shared<VariantType>(added_variant, false))) {
        // Varaint type redefinition
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    return true;
}

TestNode *FileNode::add_test(TestNode &test) {
    auto &definitions = file_namespace->public_symbols.definitions;
    definitions.emplace_back(std::make_unique<TestNode>(std::move(test)));
    TestNode *test_node = static_cast<TestNode *>(definitions.back().get());
    test_node->scope->function = test_node;
    return test_node;
}
