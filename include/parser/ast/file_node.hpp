#pragma once

#include "ast_node.hpp"
#include "definitions/data_node.hpp"
#include "definitions/entity_node.hpp"
#include "definitions/enum_node.hpp"
#include "definitions/error_node.hpp"
#include "definitions/func_node.hpp"
#include "definitions/function_node.hpp"
#include "definitions/import_node.hpp"
#include "definitions/test_node.hpp"
#include "definitions/variant_node.hpp"

#include <memory>
#include <utility>
#include <vector>

/// @class `FileNode`
/// @brief Root node of the File AST
class FileNode : public ASTNode {
  public:
    FileNode() = default;

    explicit FileNode(const std::string &file_name) :
        file_name(file_name) {}

    FileNode(std::vector<std::unique_ptr<ASTNode>> &definitions, std::string &file_name) :
        definitions(std::move(definitions)),
        file_name(file_name) {}

    /// @var `definitions`
    /// @brief All top-level definitions (functions, data, entities, etc.)
    std::vector<std::unique_ptr<ASTNode>> definitions;

    /// @var `file_name`
    /// @brief The name of the file
    std::string file_name;

    /// @var `imported_core_modules`
    /// @brief A map containing all imported core modules together with a pointer to the ImportNode from which they got imported. The
    /// ImportNode is used to check for import aliasing of the core modules.
    std::unordered_map<std::string, ImportNode *const> imported_core_modules;

    /// @var `tokens`
    /// @brief The source tokens of this file. The whole parser only has views into this list, but the FileNode owns it
    token_list tokens;

    /// @function `add_import`
    /// @brief Adds an import node to this file node
    ///
    /// @param `import` The import node to add
    /// @return `ImportNode *` A pointer to the added import node, because this function takes ownership of `import`
    std::optional<ImportNode *> add_import(ImportNode &import) {
        definitions.emplace_back(std::make_unique<ImportNode>(std::move(import)));
        ImportNode *added_import = static_cast<ImportNode *>(definitions.back().get());
        if (std::holds_alternative<std::vector<std::string>>(added_import->path)) {
            const std::vector<std::string> &import_vec = std::get<std::vector<std::string>>(added_import->path);
            if (import_vec.size() == 2 && import_vec.front() == "Core") {
                imported_core_modules.emplace(import_vec.back(), added_import);
            }
        }
        return added_import;
    }

    /// @function `add_data`
    /// @brief Adds a data node to this file node
    ///
    /// @param `data` The data node to add
    /// @return `DataNode *` A pointer to the added data node, because this function takes ownership of `data`
    DataNode *add_data(DataNode &data) {
        definitions.emplace_back(std::make_unique<DataNode>(std::move(data)));
        return static_cast<DataNode *>(definitions.back().get());
    }

    /// @function `add_func`
    /// @brief Adds a func node to this file node
    ///
    /// @param `func` The func node to add
    void add_func(FuncNode &func) {
        definitions.emplace_back(std::make_unique<FuncNode>(std::move(func)));
    }

    /// @function `add_entity`
    /// @brief Adds an entity node to this file node
    ///
    /// @param `entity` The entity node to add
    void add_entity(EntityNode &entity) {
        definitions.emplace_back(std::make_unique<EntityNode>(std::move(entity)));
    }

    /// @function `add_function`
    /// @brief Adds a function node to this file node
    ///
    /// @param `function` The function node to add
    /// @return `FunctionNode *` A pointer to the added function node, because this function takes ownership of `function`
    FunctionNode *add_function(FunctionNode &function) {
        definitions.emplace_back(std::make_unique<FunctionNode>(std::move(function)));
        return static_cast<FunctionNode *>(definitions.back().get());
    }

    /// @function `add_enum`
    /// @brief Adds an enum node to this file node
    ///
    /// @param `enum_node` The enum node to add
    /// @return `EnumNode *` A pointer to the added enum node, because this function takes ownership of `enum_node`
    EnumNode *add_enum(EnumNode &enum_node) {
        definitions.emplace_back(std::make_unique<EnumNode>(std::move(enum_node)));
        return static_cast<EnumNode *>(definitions.back().get());
    }

    /// @function `add_error`
    /// @brief Adds an error node to this file node
    ///
    /// @param `error` The error node to add
    void add_error(ErrorNode &error) {
        definitions.emplace_back(std::make_unique<ErrorNode>(std::move(error)));
    }

    /// @function `add_variant`
    /// @brief Adds a variant node to this file node
    ///
    /// @param `variant` The variant node to add
    /// @return `VariantNode *` A pointer to the added variant node, because this function takes ownership of `variant`
    VariantNode *add_variant(VariantNode &variant) {
        definitions.emplace_back(std::make_unique<VariantNode>(std::move(variant)));
        return static_cast<VariantNode *>(definitions.back().get());
    }

    /// @function `add_test`
    /// @brief Adds a test node to this file node
    ///
    /// @param `test` The test node to add
    /// @return `TestNode *` A pointer to the added test node, because this function takes ownership of `test`
    TestNode *add_test(TestNode &test) {
        definitions.emplace_back(std::make_unique<TestNode>(std::move(test)));
        return static_cast<TestNode *>(definitions.back().get());
    }
};
