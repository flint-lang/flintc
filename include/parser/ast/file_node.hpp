#pragma once

#include "ast_node.hpp"
#include "definitions/data_node.hpp"
#include "definitions/definition_node.hpp"
#include "definitions/entity_node.hpp"
#include "definitions/enum_node.hpp"
#include "definitions/error_node.hpp"
#include "definitions/func_node.hpp"
#include "definitions/function_node.hpp"
#include "definitions/import_node.hpp"
#include "definitions/test_node.hpp"
#include "definitions/variant_node.hpp"
#include "error/error.hpp"
#include "parser/ast/namespace.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/variant_type.hpp"

#include <memory>
#include <utility>
#include <vector>

/// @class `FileNode`
/// @brief Root node of the File AST
class FileNode : public ASTNode {
  public:
    FileNode() = default;

    explicit FileNode(const std::filesystem::path &file_abs) :
        file_namespace(std::make_unique<Namespace>(file_abs.string())),
        file_name(file_abs.filename()) {}

    FileNode(std::vector<std::unique_ptr<DefinitionNode>> &definitions, const std::filesystem::path &file_abs) :
        file_namespace(std::make_unique<Namespace>(definitions, file_abs.string())),
        file_name(file_abs.filename()) {}

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
    std::optional<ImportNode *> add_import(ImportNode &import) {
        auto &definitions = file_namespace->public_symbols.definitions;
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

    /// @function `add_type`
    /// @brief Adds the given type to the public or global type list
    ///
    /// @param `type` The type to add to this namespace
    /// @return `bool` Whether the type was newly added (true) or already present (false) in this namespace
    bool add_type(const std::shared_ptr<Type> &type) {
        // First we check if the type already exists in the global type map
        const auto global_type = Type::get_type_from_str(type->to_string());
        if (global_type.has_value()) {
            return true;
        }

        // Then check if the type contains any user-defined types. If not it will definitely be stored in the global type map
        if (can_be_global(type)) {
            return Type::add_type(type);
        }

        // If it contains user-defined types we check if it's already present in the public type section of this file
        //
        // If it's not contained yet we add it
        // TODO: For now the old type system with "everything global" is used, need to replace with the per-namespace type system
        return Type::add_type(type);
    }

    /// @function `can_be_global`
    /// @brief Checks whether the given type can be put into the global type map, e.g. whether it does not contain any user-defined types
    ///
    /// @param `type` The type to check
    /// @return `bool` Whether the given type is considered to be globally available
    bool can_be_global(const std::shared_ptr<Type> &type) {
        switch (type->get_variation()) {
            default:
                return true;
            case Type::Variation::ARRAY: {
                const auto *array_type = type->as<ArrayType>();
                return can_be_global(array_type->type);
            }
            case Type::Variation::DATA:
                // Data is always user-defined
                return false;
            case Type::Variation::ENUM:
                // Enums are always user-defined
                return false;
            case Type::Variation::ERROR_SET:
                // Error Sets are always user-defined
                return false;
            case Type::Variation::GROUP: {
                const auto *group_type = type->as<GroupType>();
                for (const auto &elem_type : group_type->types) {
                    if (!can_be_global(elem_type)) {
                        return false;
                    }
                }
                return true;
            }
            case Type::Variation::OPTIONAL: {
                const auto *optional_type = type->as<OptionalType>();
                return can_be_global(optional_type->base_type);
            }
            case Type::Variation::POINTER: {
                const auto *pointer_type = type->as<PointerType>();
                return can_be_global(pointer_type->base_type);
            }
            case Type::Variation::TUPLE: {
                const auto *tuple_type = type->as<TupleType>();
                for (const auto &elem_type : tuple_type->types) {
                    if (!can_be_global(elem_type)) {
                        return false;
                    }
                }
                return true;
            }
            case Type::Variation::UNKNOWN:
                return false;
            case Type::Variation::VARIANT: {
                const auto *variant_type = type->as<VariantType>();
                if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                    // User-defined variants can never be global
                    return false;
                } else {
                    const auto &type_list = std::get<std::vector<std::shared_ptr<Type>>>(variant_type->var_or_list);
                    for (const auto &possible_type : type_list) {
                        if (!can_be_global(possible_type)) {
                            return false;
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    /// @function `add_data`
    /// @brief Adds a data node to this file node
    ///
    /// @param `data` The data node to add
    /// @return `std::optional<DataNode *>` A pointer to the added data node, because this function takes ownership of `data`, nullopt if
    /// adding the data type to this namespace failed
    std::optional<DataNode *> add_data(DataNode &data) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<DataNode>(std::move(data)));
        DataNode *added_data = static_cast<DataNode *>(definitions.back().get());
        if (!add_type(std::make_shared<DataType>(added_data))) {
            THROW_ERR(ErrDefDataRedefinition, ERR_PARSING, file_name, added_data->line, added_data->column, added_data->name);
            return std::nullopt;
        }
        return added_data;
    }

    /// @function `add_func`
    /// @brief Adds a func node to this file node
    ///
    /// @param `func` The func node to add
    void add_func(FuncNode &func) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<FuncNode>(std::move(func)));
    }

    /// @function `add_entity`
    /// @brief Adds an entity node to this file node
    ///
    /// @param `entity` The entity node to add
    void add_entity(EntityNode &entity) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<EntityNode>(std::move(entity)));
    }

    /// @function `add_function`
    /// @brief Adds a function node to this file node
    ///
    /// @param `function` The function node to add
    /// @return `FunctionNode *` A pointer to the added function node, because this function takes ownership of `function`
    FunctionNode *add_function(FunctionNode &function) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<FunctionNode>(std::move(function)));
        return static_cast<FunctionNode *>(definitions.back().get());
    }

    /// @function `add_enum`
    /// @brief Adds an enum node to this file node
    ///
    /// @param `enum_node` The enum node to add
    /// @return `bool` Whether the enum was successfully added (true) or already present in this namespace (false)
    bool add_enum(EnumNode &enum_node) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<EnumNode>(std::move(enum_node)));
        EnumNode *added_enum = static_cast<EnumNode *>(definitions.back().get());
        if (!add_type(std::make_shared<EnumType>(added_enum))) {
            // Enum redifinition
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        return true;
    }

    /// @function `add_error`
    /// @brief Adds an error node to this file node
    ///
    /// @param `error` The error node to add
    /// @return `bool` Whether the error was successfully added (true) or already present in this namespace (false)
    bool add_error(ErrorNode &error) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<ErrorNode>(std::move(error)));
        ErrorNode *added_error = static_cast<ErrorNode *>(definitions.back().get());
        if (!add_type(std::make_shared<ErrorSetType>(added_error))) {
            // Error Set redefinition or naming collision
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        return true;
    }

    /// @function `add_variant`
    /// @brief Adds a variant node to this file node
    ///
    /// @param `variant` The variant node to add
    /// @return `bool` Whether the variant was successfully added (true) or already present in this napespace (false)
    bool add_variant(VariantNode &variant) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<VariantNode>(std::move(variant)));
        VariantNode *added_variant = static_cast<VariantNode *>(definitions.back().get());
        if (!add_type(std::make_shared<VariantType>(added_variant, false))) {
            // Varaint type redefinition
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        return true;
    }

    /// @function `add_test`
    /// @brief Adds a test node to this file node
    ///
    /// @param `test` The test node to add
    /// @return `TestNode *` A pointer to the added test node, because this function takes ownership of `test`
    TestNode *add_test(TestNode &test) {
        auto &definitions = file_namespace->public_symbols.definitions;
        definitions.emplace_back(std::make_unique<TestNode>(std::move(test)));
        return static_cast<TestNode *>(definitions.back().get());
    }
};
