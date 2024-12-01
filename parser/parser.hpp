#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include "../types.hpp"

#include "ast/definitions/function_node.hpp"
#include "ast/definitions/data_node.hpp"
#include "ast/definitions/func_node.hpp"
#include "ast/definitions/entity_node.hpp"
#include "ast/program_node.hpp"

class Parser {
    public:
        Parser() = default;

        static void parse_file(ProgramNode &program, std::string &file);
    private:
        static void add_next_main_node(ProgramNode &program, token_list &tokens);
        static NodeType what_is_this(const token_list &tokens);
        static token_list get_definition_tokens(token_list &tokens);
        static token_list get_body_tokens(uint definition_indentation, token_list &tokens);

        static FunctionNode create_function(const token_list &definition, const token_list &body);
        static DataNode create_data(const token_list &definition, const token_list &body);
        static FuncNode create_func(const token_list &definition, const token_list &body);
        static EntityNode create_entity(const token_list &definition, const token_list &body);
};

#endif
