#pragma once

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/definitions/func_node.hpp"
#include "parser/ast/definitions/link_node.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `EntityNode`
/// @brief Represents entities and their func / data relationships
class EntityNode : public DefinitionNode {
  public:
    explicit EntityNode(                                                                  //
        const Hash &file_hash,                                                            //
        const unsigned int line,                                                          //
        const unsigned int column,                                                        //
        const unsigned int length,                                                        //
        const std::string &name,                                                          //
        const std::vector<std::pair<std::shared_ptr<Type>, std::string>> &parent_entities //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        name(name),
        parent_entities(parent_entities) {}

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
    /// @brief The list of data modules defined inside the entity and an optional accessor for that data to be used in free-floating
    /// functions or the constructor of the entity
    std::vector<std::pair<DataNode *, std::optional<std::string>>> data_modules;

    /// @var `func_modules`
    /// @brief The list of func modules used inside the entity
    std::vector<FuncNode *> func_modules;

    /// @var `link_nodes`
    /// @brief The list of all links (from -> to) inside the 'links:' part of the entity
    std::vector<std::unique_ptr<LinkNode>> link_nodes;

    /// @var `functions`
    /// @brief A list of all functions defined as free-floating functions within this entity definition
    std::vector<FunctionNode *> functions{};

    /// @var `parent_entities`
    /// @brief The parent entities, whose data and func modules and link modules will be used. The first value in the pair is the parent
    /// entity type itself, the second value is it's accessor name
    std::vector<std::pair<std::shared_ptr<Type>, std::string>> parent_entities;

    /// @var `constructor_order`
    /// @brief The order of the data modules in which they have to be constructed
    std::vector<size_t> constructor_order;

    /// @var `ctdt`
    /// @brief The compile-time dispatch-table which is needed to resolve all links of an entity, all *source* function IDs won't be added
    /// to the list of functions the entity "provides"
    std::unordered_map<size_t, size_t> ctdt;
};
