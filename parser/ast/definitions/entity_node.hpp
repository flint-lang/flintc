#ifndef __ENTITY_NODE_HPP__
#define __ENTITY_NODE_HPP__

#include "../ast_node.hpp"
#include "data_node.hpp"
#include "func_node.hpp"
#include "link_node.hpp"

#include <string>
#include <vector>

/// EntityNode
///     Represents entities and their func / data relationships
class EntityNode : public ASTNode {
    public:

    private:
        /// name
        ///     The name of the entity
        std::string name;
        /// data_modules
        ///     The list of data modules used inside the entity
        std::vector<std::shared_ptr<DataNode>> data_modules;
        /// func_modules
        ///     The list of func modules used inside the entity
        std::vector<FuncNode> func_modules;
        /// link_nodes
        ///     The list of all links (from -> to) inside the 'links:' part of the entity
        std::vector<LinkNode> link_nodes;
        /// parent_entities
        ///     The parent entities, whose data and func modules and link modules will be used
        std::vector<std::string> parent_entities;
};

#endif
