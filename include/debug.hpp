#ifndef __DEBUG_HPP__
#define __DEBUG_HPP__

#ifdef DEBUG_BUILD
constexpr inline bool DEBUG_MODE = true;
#else
constexpr inline bool DEBUG_MODE = false;
#endif

#include "resolver/resolver.hpp"
#include "result.hpp"
#include "types.hpp"

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/entity_node.hpp"
#include "parser/ast/definitions/enum_node.hpp"
#include "parser/ast/definitions/func_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "parser/ast/definitions/link_node.hpp"
#include "parser/ast/definitions/test_node.hpp"
#include "parser/ast/definitions/variant_node.hpp"

#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/variable_node.hpp"

#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/throw_node.hpp"
#include "parser/ast/statements/while_node.hpp"

#include "parser/ast/call_node_base.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/unary_op_base.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Debug {
    enum TreeType { VERT, BRANCH, BRANCH_L, SINGLE, HOR, NONE };
    static const std::unordered_map<TreeType, std::string> tree_characters = {
        {VERT, "\u2502"},
        {BRANCH, "\u251C"},
        {BRANCH_L, "\u2524"},
        {SINGLE, "\u2514"},
        {HOR, "\u2500"},
    };

    std::string get_string_container(unsigned int size, const std::string &value);
    void print_in_container(unsigned int size, const std::string &str);
    std::string fill_container_with(const unsigned int &size, const char &character);
    void print_token_context_vector(const token_list &tokens, const std::string &file_name);
    void print_tree_row(const std::vector<TreeType> &types, TestResult *result);
    void print_tree_characters(const std::vector<TreeType> &types);
    std::string create_n_str(unsigned int n, const std::string &str);

    namespace Dep {
        void print_dep_tree(unsigned int indent_lvl, const std::variant<std::shared_ptr<DepNode>, std::weak_ptr<DepNode>> &dep_node);
    } // namespace Dep

    namespace AST {
        namespace {
            void print_table_header(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells);
            void print_table_row(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells);
            void print_header(unsigned int indent_lvl, uint2 empty, const std::string &header);
        } // namespace

        void print_file(const FileNode &file);

        // --- EXPRESSIONS ---
        void print_variable(unsigned int indent_lvl, uint2 empty, const VariableNode &var);
        void print_unary_op(unsigned int indent_lvl, uint2 empty, const UnaryOpBase &unary);
        void print_literal(unsigned int indent_lvl, uint2 empty, const LiteralNode &lit);
        void print_call(unsigned int indent_lvl, uint2 empty, const CallNodeBase &call);
        void print_binary_op(unsigned int indent_lvl, uint2 empty, const BinaryOpNode &bin);
        void print_type_cast(unsigned int indent_lvl, uint2 empty, const TypeCastNode &cast);
        void print_expression(unsigned int indent_lvl, uint2 empty, const std::unique_ptr<ExpressionNode> &expr);

        // --- STATEMENTS ---
        void print_throw(unsigned int indent_lvl, uint2 empty, const ReturnNode &return_node);
        void print_return(unsigned int indent_lvl, uint2 empty, const ThrowNode &return_node);
        void print_if(unsigned int indent_lvl, uint2 empty, const IfNode &if_node);
        void print_while(unsigned int indent_lvl, uint2 empty, const WhileNode &while_node);
        void print_for(unsigned int indent_lvl, uint2 empty, const ForLoopNode &for_node);
        void print_catch(unsigned int indent_lvl, uint2 empty, const CatchNode &catch_node);
        void print_assignment(unsigned int indent_lvl, uint2 empty, const AssignmentNode &assign);
        void print_declaration(unsigned int indent_lvl, uint2 empty, const DeclarationNode &decl);
        void print_statement(unsigned int indent_lvl, uint2 empty, const std::unique_ptr<StatementNode> &statement);
        void print_body(unsigned int indent_lvl, uint2 empty, const std::vector<std::unique_ptr<StatementNode>> &body);

        // --- DEFINITIONS ---
        void print_data(unsigned int indent_lvl, const DataNode &data);
        void print_entity(unsigned int indent_lvl, const EntityNode &entity);
        void print_enum(unsigned int indent_lvl, const EnumNode &enum_node);
        void print_error(unsigned int indent_lvl, const ErrorNode &error);
        void print_func(unsigned int indent_lvl, const FuncNode &func);
        void print_function(unsigned int indent_lvl, uint2 empty, const FunctionNode &function);
        void print_import(unsigned int indent_lvl, const ImportNode &import);
        void print_link(unsigned int indent_lvl, uint2 empty, const LinkNode &link);
        void print_variant(unsigned int indent_lvl, const VariantNode &variant);
        void print_test(unsigned int indent_lvl, uint2 empty, const TestNode &test);
    } // namespace AST
} // namespace Debug

#endif
