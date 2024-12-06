#ifndef __DEBUG_HPP__
#define __DEBUG_HPP__

#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "types.hpp"

#include "parser/ast/ast_node.hpp"
#include "parser/ast/node_type.hpp"
#include "parser/ast/program_node.hpp"

namespace Debug {
    static void print_token_context_vector(const token_list &tokens);

    namespace AST {
        static void print_ast_tree(const ProgramNode &program);

        static NodeType get_ast_type(const ASTNode &node);
        static NodeType get_expression_type(const ExpressionNode &node);
        static NodeType get_statement_type(const StatementNode &node);
    }
}

#endif
