#ifndef __ENTITY_NODE_HPP__
#define __ENTITY_NODE_HPP__

#include "../ast_node.hpp"
#include "link_node.hpp"

#include <string>
#include <vector>
#include <utility>
#include <memory>

/// EntityNode
///     Represents entities and their func / data relationships
///     Because an entity can either be monolithic or modular, there are two possibilities for the entity
class EntityNode : public ASTNode {
    public:
        explicit EntityNode(std::string &name,
            std::vector<std::string> &data_modules,
            std::vector<std::string> &func_modules,
            std::vector<std::unique_ptr<LinkNode>> link_nodes,
            std::vector<std::pair<std::string, std::string>> &parent_entities,
            std::vector<std::string> &constructor_order)
            : name(name),
            data_modules(std::move(data_modules)),
            func_modules(std::move(func_modules)),
            link_nodes(std::move(link_nodes)),
            parent_entities(std::move(parent_entities)),
            constructor_order(std::move(constructor_order)) {}

        // empty constructor
        EntityNode() = default;
        // destructor
        ~EntityNode() override = default;
        // copy operations - disabled due to unique_ptr member
        EntityNode(const EntityNode &) = delete;
        EntityNode& operator=(const EntityNode &) = delete;
        // move operations
        EntityNode(EntityNode &&) = default;
        EntityNode& operator=(EntityNode &&) = default;
    private:
        /// name
        ///     The name of the entity
        std::string name;
        /// data_modules
        ///     The list of data modules used inside the entity
        std::vector<std::string> data_modules;
        /// func_modules
        ///     The list of func modules used inside the entity
        std::vector<std::string> func_modules;
        /// link_nodes
        ///     The list of all links (from -> to) inside the 'links:' part of the entity
        std::vector<std::unique_ptr<LinkNode>> link_nodes;
        /// parent_entities
        ///     The parent entities, whose data and func modules and link modules will be used
        std::vector<std::pair<std::string, std::string>> parent_entities;
        /// constructor_order
        ///     The order of the data modules in which they have to be constructed
        std::vector<std::string> constructor_order;
};

#endif
