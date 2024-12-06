#ifndef __PROGRAM_NODE_HPP__
#define __PROGRAM_NODE_HPP__

#include "ast_node.hpp"
#include "definitions/data_node.hpp"
#include "definitions/enum_node.hpp"
#include "definitions/error_node.hpp"
#include "definitions/func_node.hpp"
#include "definitions/entity_node.hpp"
#include "definitions/function_node.hpp"
#include "definitions/import_node.hpp"
#include "definitions/variant_node.hpp"

#include <vector>

/// ProgramNode
///     Root node of the AST
class ProgramNode : public ASTNode {
    public:
        ProgramNode() = default;

        void add_import(ImportNode &import) {
            definitions.push_back(import);
        }

        void add_data(DataNode &data) {
            definitions.push_back(data);
        }

        void add_func(FuncNode &func) {
            definitions.push_back(func);
        }

        void add_entity(EntityNode &entity) {
            definitions.push_back(entity);
        }

        void add_function(FunctionNode &function) {
            definitions.push_back(function);
        }

        void add_enum(EnumNode &enum_node) {
            definitions.push_back(enum_node);
        }

        void add_error(ErrorNode &error) {
            definitions.push_back(error);
        }

        void add_variant(VariantNode &variant) {
            definitions.push_back(variant);
        }
    //private:
        /// definitions
        ///     All top-level definitions (functions, data, entities, etc.)
        std::vector<ASTNode> definitions;
};

#endif
