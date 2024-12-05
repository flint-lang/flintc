#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include "../types.hpp"

#include "ast/definitions/enum_node.hpp"
#include "ast/definitions/error_node.hpp"
#include "ast/definitions/function_node.hpp"
#include "ast/definitions/data_node.hpp"
#include "ast/definitions/func_node.hpp"
#include "ast/definitions/entity_node.hpp"
#include "ast/definitions/link_node.hpp"
#include "ast/definitions/variant_node.hpp"
#include "ast/program_node.hpp"
#include "ast/statements/assignment_node.hpp"
#include "ast/statements/declaration_node.hpp"
#include "ast/statements/for_loop_node.hpp"
#include "ast/statements/if_node.hpp"
#include "ast/statements/statement_node.hpp"
#include "ast/statements/while_node.hpp"

#include <utility>
#include <optional>

class Parser {
    public:
        Parser() = default;

        static void parse_file(ProgramNode &program, std::string &file);
    private:
        static void add_next_main_node(ProgramNode &program, token_list &tokens);
        static NodeType what_is_this(const token_list &tokens);
        static token_list get_definition_tokens(token_list &tokens);
        static token_list get_body_tokens(unsigned int definition_indentation, token_list &tokens);
        static token_list extract_from_to(unsigned int from, unsigned int to, token_list &tokens);

        static std::optional<IfNode> create_if(const token_list &tokens);
        static std::optional<WhileNode> create_while_loop(const token_list &tokens);
        static std::optional<ForLoopNode> create_for_loop(const token_list &tokens, const bool &is_enhanced);
        static std::optional<AssignmentNode> create_assignment(const token_list &tokens);
        static std::optional<DeclarationNode> create_declaration_statement(const token_list &tokens, const bool &is_infered);
        static std::optional<StatementNode> create_statement(const token_list &tokens);
        static std::vector<StatementNode> create_body(token_list &body);

        static FunctionNode create_function(const token_list &definition, token_list &body);
        static DataNode create_data(const token_list &definition, const token_list &body);
        static FuncNode create_func(const token_list &definition, token_list &body);
        static std::pair<EntityNode, std::optional<std::pair<DataNode, FuncNode>>> create_entity(const token_list &definition, token_list &body);
        static std::vector<LinkNode> create_links(token_list &body);
        static LinkNode create_link(const token_list &tokens);

        static EnumNode create_enum(const token_list &definition, const token_list &body);
        static ErrorNode create_error(const token_list &definition, const token_list &body);
        static VariantNode create_variant(const token_list &definition, const token_list &body);
};

#endif
