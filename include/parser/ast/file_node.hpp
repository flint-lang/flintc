#pragma once

#include "ast_node.hpp"
#include "definitions/data_node.hpp"
#include "definitions/definition_node.hpp"
#include "definitions/enum_node.hpp"
#include "definitions/error_node.hpp"
#include "definitions/func_node.hpp"
#include "definitions/function_node.hpp"
#include "definitions/import_node.hpp"
#include "definitions/interface_node.hpp"
#include "definitions/object_node.hpp"
#include "definitions/test_node.hpp"
#include "definitions/variant_node.hpp"
#include "parser/ast/namespace.hpp"

#include <memory>
#include <vector>

/// @class `FileNode`
/// @brief Root node of the File AST
class FileNode : public ASTNode {
  public:
    FileNode() = default;

    explicit FileNode(const std::filesystem::path &file_abs) :
        file_namespace(std::make_unique<Namespace>(std::filesystem::absolute(file_abs))),
        file_name(file_abs.filename().string()) {}

    FileNode(std::vector<std::unique_ptr<DefinitionNode>> &definitions, const std::filesystem::path &file_abs) :
        file_namespace(std::make_unique<Namespace>(definitions, file_abs)),
        file_name(file_abs.filename().string()) {}

    /// @var `namespace`
    /// @brief The namespace this file represents
    std::unique_ptr<Namespace> file_namespace;

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
    std::optional<ImportNode *> add_import(ImportNode &import);

    /// @function `add_data`
    /// @brief Adds a data node to this file node
    ///
    /// @param `data` The data node to add
    /// @return `std::optional<DataNode *>` A pointer to the added data node, because this function takes ownership of `data`, nullopt if
    /// adding the data type to this namespace failed
    std::optional<DataNode *> add_data(DataNode &data);

    /// @function `add_func`
    /// @brief Adds a func node to this file node
    ///
    /// @param `func` The func node to add
    /// @return `std::optional<FuncNode *>` A pointer to the added func node, because this function takes ownership of `func`, nullopt if
    /// adding the func type to this namespace failed
    std::optional<FuncNode *> add_func(FuncNode &func);

    /// @function `add_interface`
    /// @brief Adds an interface node to this file node
    ///
    /// @param `interface` The interface node to add
    /// @return `std::optional<InterfaceNode *>` A pointer to the added interface node, because this function takes ownership of
    /// `interface`, nullopt if adding the interface type to this namespace failed
    std::optional<InterfaceNode *> add_interface(InterfaceNode &interface);

    /// @function `add_object`
    /// @brief Adds an object node to this file node
    ///
    /// @param `object` The object node to add
    /// @return `std::optional<EnttiyNode *>` A pointer to the added object node, because this function takes ownership of `object`, nullopt
    /// if adding the object type to this namespace failed
    std::optional<ObjectNode *> add_object(ObjectNode &object);

    /// @function `add_function`
    /// @brief Adds a function node to this file node
    ///
    /// @param `function` The function node to add
    /// @param `core_namespaces` Reference to the Parser-internal initialized core namespaces map
    /// @return `std::optional<FunctionNode *>` A pointer to the added function node, because this function takes ownership of `function`,
    /// nullopt if the given function already existed in the file's namespace, e.g. duplicate definition
    std::optional<FunctionNode *> add_function(                                            //
        FunctionNode &function,                                                            //
        const std::unordered_map<std::string, std::unique_ptr<Namespace>> &core_namespaces //
    );

    /// @function `add_enum`
    /// @brief Adds an enum node to this file node
    ///
    /// @param `enum_node` The enum node to add
    /// @return `bool` Whether the enum was successfully added (true) or already present in this namespace (false)
    bool add_enum(EnumNode &enum_node);

    /// @function `add_error`
    /// @brief Adds an error node to this file node
    ///
    /// @param `error` The error node to add
    /// @return `bool` Whether the error was successfully added (true) or already present in this namespace (false)
    bool add_error(ErrorNode &error);

    /// @function `add_variant`
    /// @brief Adds a variant node to this file node
    ///
    /// @param `variant` The variant node to add
    /// @return `bool` Whether the variant was successfully added (true) or already present in this napespace (false)
    bool add_variant(VariantNode &variant);

    /// @function `add_test`
    /// @brief Adds a test node to this file node
    ///
    /// @param `test` The test node to add
    /// @return `TestNode *` A pointer to the added test node, because this function takes ownership of `test`
    TestNode *add_test(TestNode &test);
};
