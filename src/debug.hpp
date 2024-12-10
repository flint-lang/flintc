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
#include "types.hpp"

#include "parser/ast/program_node.hpp"

#include <map>
#include <string>
#include <vector>
#include <utility>

namespace Debug {
    enum TreeType {
        VERT, BRANCH, BRANCH_L, SINGLE, HOR, NONE
    };
    static const std::map<TreeType, std::string> tree_characters = {
        {VERT, "\u2502"},
        {BRANCH, "\u251C"},
        {BRANCH_L, "\u2524"},
        {SINGLE, "\u2514"},
        {HOR, "\u2500"},
    };

    std::string get_string_container(unsigned int size, const std::string &value);
    void print_in_container(unsigned int size, const std::string &str);
    std::string fill_container_with(const unsigned int &size, const char &character);
    void print_token_context_vector(const token_list &tokens);
    void print_tree_row(const std::vector<TreeType> &types, bool is_test);
    void print_tree_characters(const std::vector<TreeType> &types);
    std::string create_n_str(unsigned int n, const std::string &str);

    namespace AST {
        namespace {
            void print_table_header(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells);
            void print_table_row(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells);
        }
        void print_program(const ProgramNode &program);

        void print_data(const DataNode &data);
        void print_entity(const EntityNode &entity);
        void print_enum(const EnumNode &enum_node);
        void print_error(const ErrorNode &error);
        void print_func(const FuncNode &func);
        void print_function(const FunctionNode &function);
        void print_import(const ImportNode &import);
        void print_link(const LinkNode &link);
        void print_variant(const VariantNode &variant);
    }
}

#endif
