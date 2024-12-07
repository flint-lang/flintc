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

namespace Debug {
    std::string get_string_container(int &size, std::string &value);
    void print_token_context_vector(const token_list &tokens);

    namespace AST {
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
