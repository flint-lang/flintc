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

/// FileNode
///     Root node of the File AST
class FileNode : public ASTNode {
  public:
    /// definitions
    ///     All top-level definitions (functions, data, entities, etc.)
    std::vector<std::unique_ptr<ASTNode>> definitions;
    std::string file_name;

    FileNode() = default;
    explicit FileNode(const std::string &file_name) :
        file_name(file_name) {}
    FileNode(std::vector<std::unique_ptr<ASTNode>> &definitions, std::string &file_name) :
        definitions(std::move(definitions)),
        file_name(file_name) {}

    ImportNode *add_import(ImportNode &import) {
        definitions.emplace_back(std::make_unique<ImportNode>(std::move(import)));
        return static_cast<ImportNode *>(definitions.back().get());
    }

    void add_data(DataNode &data) {
        definitions.emplace_back(std::make_unique<DataNode>(std::move(data)));
    }

    void add_func(FuncNode &func) {
        definitions.emplace_back(std::make_unique<FuncNode>(std::move(func)));
    }

    void add_entity(EntityNode &entity) {
        definitions.emplace_back(std::make_unique<EntityNode>(std::move(entity)));
    }

    FunctionNode *add_function(FunctionNode &function) {
        definitions.emplace_back(std::make_unique<FunctionNode>(std::move(function)));
        return static_cast<FunctionNode *>(definitions.back().get());
    }

    void add_enum(EnumNode &enum_node) {
        definitions.emplace_back(std::make_unique<EnumNode>(std::move(enum_node)));
    }

    void add_error(ErrorNode &error) {
        definitions.emplace_back(std::make_unique<ErrorNode>(std::move(error)));
    }

    void add_variant(VariantNode &variant) {
        definitions.emplace_back(std::make_unique<VariantNode>(std::move(variant)));
    }

    TestNode *add_test(TestNode &test) {
        definitions.emplace_back(std::make_unique<TestNode>(std::move(test)));
        return static_cast<TestNode *>(definitions.back().get());
    }
};
