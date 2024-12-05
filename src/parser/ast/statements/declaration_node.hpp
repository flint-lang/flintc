#ifndef __DECLARATION_NODE_HPP__
#define __DECLARATION_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <string>

/// DeclarationNode
///     Represents variable or data declarations
class DeclarationNode : public StatementNode {
    public:
        DeclarationNode(std::string &type, std::string &name, ExpressionNode &initializer)
        : type(type), name(name), initializer(initializer) {}
    private:
        /// var_type
        ///     The type of the variable
        std::string type;
        /// var_name
        ///     The name of the variable
        std::string name;
        /// initializer
        ///     The initial value
        ExpressionNode initializer;
};

#endif
