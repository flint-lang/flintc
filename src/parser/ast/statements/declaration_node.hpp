#ifndef __DECLARATION_NODE_HPP__
#define __DECLARATION_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <string>
#include <memory>

/// DeclarationNode
///     Represents variable or data declarations
class DeclarationNode : public StatementNode {
    public:

    private:
        /// var_name
        ///     The name of the variable
        std::string var_name;
        /// var_type
        ///     The type of the variable
        std::string var_type;
        /// initializer
        ///     The initial value
        std::shared_ptr<ExpressionNode> initializer;
};

#endif
