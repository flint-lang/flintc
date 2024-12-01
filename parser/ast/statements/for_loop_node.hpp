#ifndef __FOR_LOOP_NODE_HPP__
#define __FOR_LOOP_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <string>

/// ForLoopNode
///     Represents both traditional and enhanced for loops.
class ForLoopNode : public StatementNode {
    public:

    private:
        /// iterator_name
        ///     The name of the iterator variable
        std::string iterator_name;
        /// iterable
        ///     Iterable expression (e.g. range or array)
        std::shared_ptr<ExpressionNode> iterable;
        /// body
        ///     The body of the loop
        std::shared_ptr<StatementNode> body;
};

#endif
