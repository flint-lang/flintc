#ifndef __FOR_LOOP_NODE_HPP__
#define __FOR_LOOP_NODE_HPP__

#include "statement_node.hpp"
#include "../expressions/expression_node.hpp"

#include <string>
#include <vector>
#include <utility>
#include <memory>

/// ForLoopNode
///     Represents both traditional and enhanced for loops.
class ForLoopNode : public StatementNode {
    public:
        ForLoopNode(std::string &iterator_name,
            std::unique_ptr<ExpressionNode> &iterable,
            std::vector<std::unique_ptr<StatementNode>> &body)
        : iterator_name(iterator_name),
        iterable(std::move(iterable)),
        body(std::move(body)) {}

        // constructor
        ForLoopNode() = default;
        // deconstructor
        ~ForLoopNode() override = default;
        // copy operations - deleted because of unique_ptr member
        ForLoopNode(const ForLoopNode &) = delete;
        ForLoopNode& operator=(const ForLoopNode &) = delete;
        // move operations
        ForLoopNode(ForLoopNode &&) = default;
        ForLoopNode& operator=(ForLoopNode &&) = delete;
    private:
        /// iterator_name
        ///     The name of the iterator variable
        std::string iterator_name;
        /// iterable
        ///     Iterable expression (e.g. range or array)
        std::unique_ptr<ExpressionNode> iterable;
        /// body
        ///     The body of the loop
        std::vector<std::unique_ptr<StatementNode>> body;
};

#endif
