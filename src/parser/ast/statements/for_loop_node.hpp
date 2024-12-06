#ifndef __FOR_LOOP_NODE_HPP__
#define __FOR_LOOP_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <string>
#include <vector>
#include <utility>

/// ForLoopNode
///     Represents both traditional and enhanced for loops.
class ForLoopNode : public StatementNode {
    public:
        ForLoopNode(std::string &iterator_name, ExpressionNode &iterable, std::vector<StatementNode> &body)
        : iterator_name(iterator_name), iterable(iterable), body(std::move(body)) {}
    private:
        /// iterator_name
        ///     The name of the iterator variable
        std::string iterator_name;
        /// iterable
        ///     Iterable expression (e.g. range or array)
        ExpressionNode iterable;
        /// body
        ///     The body of the loop
        std::vector<StatementNode> body;
};

#endif
