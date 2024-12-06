#ifndef __DEBUG_HPP__
#define __DEBUG_HPP__

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/entity_node.hpp"
#include "parser/ast/definitions/enum_node.hpp"
#include "parser/ast/definitions/func_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "parser/ast/definitions/link_node.hpp"
#include "parser/ast/definitions/variant_node.hpp"
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

        static void print_data(const DataNode &data);
        static void print_entity(const EntityNode &entity);
        static void print_enum(const EnumNode &enum_node);
        static void print_error(const ErrorNode &error);
        static void print_func(const FuncNode &func);
        static void print_function(const FunctionNode &function);
        static void print_import(const ImportNode &import);
        static void print_link(const LinkNode &link);
        static void print_variant(const VariantNode &variant);
    }
}

#endif
