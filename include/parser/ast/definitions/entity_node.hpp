#pragma once

#include "link_node.hpp"
#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/definitions/func_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `EntityNode`
/// @brief Represents entities and their func / data relationships
/// @info Because an entity can either be monolithic or modular, there are two possibilities for the entity. For now, only modular entities
/// are supported. Monolithic entities will be supported *eventually*
class EntityNode : public DefinitionNode {
  public:
    explicit EntityNode(                                                                   //
        const Hash &file_hash,                                                             //
        const unsigned int line,                                                           //
        const unsigned int column,                                                         //
        const unsigned int length,                                                         //
        const std::string &name,                                                           //
        const std::vector<DataNode *> &data_modules,                                       //
        const std::vector<FuncNode *> &func_modules,                                       //
        std::vector<std::unique_ptr<LinkNode>> &link_nodes,                                //
        const std::vector<std::pair<std::shared_ptr<Type>, std::string>> &parent_entities, //
        const std::vector<size_t> &constructor_order,                                      //
        const bool is_monolithic                                                           //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        name(name),
        data_modules(data_modules),
        func_modules(func_modules),
        link_nodes(std::move(link_nodes)),
        parent_entities(parent_entities),
        constructor_order(constructor_order),
        is_monolithic(is_monolithic) {}

    Variation get_variation() const override {
        return Variation::ENTITY;
    }

    // destructor
    ~EntityNode() override = default;
    // copy operations - disabled due to unique_ptr member
    EntityNode(const EntityNode &) = delete;
    EntityNode &operator=(const EntityNode &) = delete;
    // move operations
    EntityNode(EntityNode &&) = default;
    EntityNode &operator=(EntityNode &&) = default;

    /// @var `name`
    /// @brief The name of the entity
    std::string name;

    /// @var `data_modules`
    /// @brief The list of data modules used inside the entity
    std::vector<DataNode *> data_modules;

    /// @var `func_modules`
    /// @brief The list of func modules used inside the entity
    std::vector<FuncNode *> func_modules;

    /// @var `link_nodes`
    /// @brief The list of all links (from -> to) inside the 'links:' part of the entity
    std::vector<std::unique_ptr<LinkNode>> link_nodes;

    /// @var `parent_entities`
    /// @brief The parent entities, whose data and func modules and link modules will be used. The first value in the pair is the parent
    /// entity type itself, the second value is it's accessor name
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> parent_entities;

    /// @var `constructor_order`
    /// @brief The order of the data modules in which they have to be constructed
    std::vector<size_t> constructor_order;

    /// @var `is_monolithic`
    /// @brief Whether this entity is monolithic. In that case it only contains one data and func module respectively
    bool is_monolithic;
};
