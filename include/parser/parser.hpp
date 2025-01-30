#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include "types.hpp"

#include "ast/call_node_base.hpp"
#include "ast/file_node.hpp"

#include "ast/definitions/data_node.hpp"
#include "ast/definitions/entity_node.hpp"
#include "ast/definitions/enum_node.hpp"
#include "ast/definitions/error_node.hpp"
#include "ast/definitions/func_node.hpp"
#include "ast/definitions/function_node.hpp"
#include "ast/definitions/import_node.hpp"
#include "ast/definitions/link_node.hpp"
#include "ast/definitions/variant_node.hpp"

#include "ast/statements/assignment_node.hpp"
#include "ast/statements/call_node_statement.hpp"
#include "ast/statements/declaration_node.hpp"
#include "ast/statements/for_loop_node.hpp"
#include "ast/statements/if_node.hpp"
#include "ast/statements/return_node.hpp"
#include "ast/statements/statement_node.hpp"
#include "ast/statements/while_node.hpp"

#include "ast/expressions/binary_op_node.hpp"
#include "ast/expressions/call_node_expression.hpp"
#include "ast/expressions/expression_node.hpp"
#include "ast/expressions/literal_node.hpp"
#include "ast/expressions/unary_op_node.hpp"
#include "ast/expressions/variable_node.hpp"
#include "ast/statements/catch_node.hpp"
#include "ast/statements/throw_node.hpp"

#include <cassert>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <utility>

/// @class `Parser`
/// @brief The class which is responsible for the AST generation (parsing)
/// @note This class cannot be initialized and all functions within this class are static
class Parser {
  public:
    Parser() = delete;

    /// @function `parse_file`
    /// @brief Parses a file. It will tokenize it using the Lexer and then create the AST of the file and return the parsed FileNode
    ///
    /// @param `file` The path to the file to tokenize and parse
    /// @return `FileNode` The parsed file
    ///
    /// @note This function creates a new Lexer class to tokenize the given file, so no further tokenization has to be made, this function
    /// takes care of the tokenization too
    static FileNode parse_file(const std::filesystem::path &file);

    /// @function `get_call_from_id`
    /// @brief Returns the call node from the given id
    ///
    /// @param `call_id` The id from which the call node will be returned
    /// @return `std::optional<CallNodeBase *>` An optional pointer to the CallNodeBase, nullopt if no call with the given call id exists
    static std::optional<CallNodeBase *> get_call_from_id(unsigned int call_id);

    /// @function `resolve_call_types`
    /// @brief Resolves all types of all calls
    static void resolve_call_types();

  private:
    /// @var `token_precedence`
    /// @brief
    ///
    /// This map exists to map the token types to their respective precedence values. The higher the precedence the sooner this token will
    /// be evaluated in, for example, a binary operation.
    ///
    /// @details
    /// - **Key** `Token` - The enum value of token type whose precedence is set
    /// - **Value** `unsigned int` - The precedence of the token type
    static const inline std::unordered_map<Token, unsigned int> token_precedence = {
        {TOK_SQUARE, 8},
        {TOK_MULT, 7},
        {TOK_DIV, 7},
        {TOK_PLUS, 6},
        {TOK_MINUS, 6},
        {TOK_LESS, 5},
        {TOK_GREATER, 5},
        {TOK_LESS_EQUAL, 5},
        {TOK_GREATER_EQUAL, 5},
        {TOK_NOT, 4},
        {TOK_AND, 3},
        {TOK_OR, 2},
        {TOK_EQUAL_EQUAL, 1},
        {TOK_NOT_EQUAL, 1},
        {TOK_EQUAL, 0},
    };

    /// @var `call_nodes`
    /// @brief Stores all the calls that have been parsed
    ///
    /// This map exists to keep track of all parsed call nodes. This map is not allowed to be changed to an unordered_map, because the
    /// elements of the map are required to perserve their ordering. Most of times only the last element from this map is searched for, this
    /// is the reason to why the ordering is important.
    static std::map<unsigned int, CallNodeBase *> call_nodes;

    /// @function `set_last_parsed_call`
    /// @brief Sets the last parsed call
    ///
    /// @param `call_id` The call ID of the CallNode to add
    /// @param `call` The pointer to the base of the CallNode to add
    static inline void set_last_parsed_call(unsigned int call_id, CallNodeBase *call) {
        call_nodes.emplace(call_id, call);
    }

    /// @function `get_last_parsed_call_id`
    /// @brief Returns the ID of the last parsed call
    ///
    /// @return The ID of the last parsed call
    ///
    /// @attention Asserts that the call_nodes map contains at least one element
    static inline unsigned int get_last_parsed_call_id() {
        assert(!call_nodes.empty());
        return call_nodes.rbegin()->first;
    }

    // @class `Util`
    /// @brief This class is responsible for parsing everything about expressions
    /// @note This class cannot be initialized and all functions within this class are static
    class Util {
      public:
        Util() = delete;

        /// @function `add_next_main_node`
        /// @brief Creates the next main node from the list of tokens and adds it to the file node
        ///
        /// @details A main node is every node which is able to be at the top-level of a file (functions, modules, file imports etc.)
        ///
        /// @param `file_node` The file node the next created main node will be added to
        /// @param `tokens` The list of tokens from which the next main node will be created from
        static void add_next_main_node(FileNode &file_node, token_list &tokens);

        /// @function `get_definition_tokens`
        /// @brief Extracts all the tokens which are part of the definition
        ///
        /// @return `token_list` Returns the extracted tokens, being part of the definition
        ///
        /// @attention Deletes all tokens which are part of the definition from the given tokens list
        static token_list get_definition_tokens(token_list &tokens);

        /// @function `get_body_tokens`
        /// @brief Extracts all the body tokens based on their indentation
        ///
        /// @param `definition_indentation` The indentation level of the definition. The body of the definition will have at least one
        /// indentation level more
        /// @param `tokens` The tokens from which the body will be extracted from
        /// @return `token_list` The extracted list of tokens forming the whole body
        ///
        /// @attention This function modifies the given `tokens` list, the retured tokens are no longer part of the given list
        static token_list get_body_tokens(unsigned int definition_indentation, token_list &tokens);

        /// @function `extract_from_to`
        /// @brief Extracts the tokens from a given index up to the given index from the given tokens list
        ///
        /// @param `from` The start index from which the extraction starts (inclusive)
        /// @param `to` The end index at which the extraction ends (exclusive)
        /// @param `tokens` The tokens from which the range is extracted
        /// @return `token_list` The extracted token range
        ///
        /// @assert to >= from and to <= tokens.size()
        /// @attention Modifies the given tokens list, deletes all extracted tokens from it
        static token_list extract_from_to(unsigned int from, unsigned int to, token_list &tokens);

        /// @function `clone_from_to`
        /// @brief Clones the tokens from a given index up to the given index from the given tokens list
        ///
        /// @param `from` The start index from which the cloning starts (inclusive)
        /// @param `to` The end index at which the cloning ends (exclusive)
        /// @param `tokens` The tokens from which the range is cloned
        /// @return `token_list` The cloned token range
        ///
        /// @assert to >= from and to <= tokens.size()
        static token_list clone_from_to(unsigned int from, unsigned int to, const token_list &tokens);

        /// @function `create_call_base`
        /// @brief Creates the base node for all calls
        ///
        /// @details This function is part of the `Util` class, as both call statements as well as call expressions use this function
        ///
        /// @param `scope` The scope the call happens in
        /// @param `tokens` The tokens which will be interpreted as call
        /// @return A optional value containing a tuple, where the first value is the function name, the second value is a list of all
        /// expressions (the argument expression) of the call and the third value is the call's return type, encoded as a string
        static std::optional<std::tuple<std::string, std::vector<std::unique_ptr<ExpressionNode>>, std::string>> create_call_base( //
            Scope *scope,                                                                                                          //
            token_list &tokens                                                                                                     //
        );
    }; // subclass Util

    /// @class `Expression`
    /// @brief This class is responsible for parsing everything about expressions
    /// @note This class cannot be initialized and all functions within this class are static
    class Expression {
      public:
        Expression() = delete;

        /// @function `create_variable`
        /// @brief Creates a VariableNode from the given tokens
        ///
        /// @param `scope` The scope in which the variable is defined
        /// @param `tokens` The list of tokens representing the variable
        /// @return `std::optional<VariableNode>` An optional VariableNode if creation is successful, nullopt otherwise
        static std::optional<VariableNode> create_variable(Scope *scope, const token_list &tokens);

        /// @function `create_unary_op`
        /// @brief Creates a UnaryOpNode from the given tokens
        ///
        /// @param `scope` The scope in which the unary operation is defined
        /// @param `tokens` The list of tokens representing the unary operation
        /// @return `std::optional<UnaryOpNode>` An optional UnaryOpNode if creation is successful, nullopt otherwise
        static std::optional<UnaryOpNode> create_unary_op(Scope *scope, const token_list &tokens);

        /// @function `create_literal`
        /// @brief Creates a LiteralNode from the given tokens
        ///
        /// @param `tokens` The list of tokens representing the literal
        /// @return `std::optional<LiteralNode>` An optional LiteralNode if creation is successful, nullopt otherwise
        static std::optional<LiteralNode> create_literal(const token_list &tokens);

        /// @function `create_call_expression`
        /// @brief Creates a CallNodeExpression from the given tokens
        ///
        /// @param `scope` The scope in which the call expression is defined
        /// @param `tokens` The list of tokens representing the call expression
        /// @return `std::unique_ptr<CallNodeExpression>` A unique pointer to the created CallNodeExpression
        static std::unique_ptr<CallNodeExpression> create_call_expression(Scope *scope, token_list &tokens);

        /// @function `create_binary_op`
        /// @brief Creates a BinaryOpNode from the given tokens
        ///
        /// @param `scope` The scope in which the binary operation is defined
        /// @param `tokens` The list of tokens representing the binary operation
        /// @return `std::optional<BinaryOpNode>` An optional BinaryOpNode if creation is successful, nullopt otherwise
        static std::optional<BinaryOpNode> create_binary_op(Scope *scope, token_list &tokens);

        /// @function `create_expression`
        /// @brief Creates an ExpressionNode from the given tokens
        ///
        /// @param `scope` The scope in which the expression is defined
        /// @param `tokens` The list of tokens representing the expression
        /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An optional unique pointer to the created ExpressionNode
        static std::optional<std::unique_ptr<ExpressionNode>> create_expression(Scope *scope, token_list &tokens);
    }; // subclass Expression

    /// @class `Statement`
    /// @brief This class is responsible for parsing everything about statements
    /// @note This class cannot be initialized and all functions within this class are static
    class Statement {
      public:
        Statement() = delete;

        /// @function `create_call_statement`
        /// @brief Creates a CallNodeStatement from the given list of tokens
        ///
        /// @param `scope` The scope in which the call statement is defined
        /// @param `tokens` The list of tokens representing the call statement
        /// @return `std::unique_ptr<CallNodeStatement>` A unique pointer to the created CallNodeStatement
        static std::unique_ptr<CallNodeStatement> create_call_statement(Scope *scope, token_list &tokens);

        /// @function `create_throw`
        /// @brief Creates a ThrowNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the throw statement is defined
        /// @param `tokens` The list of tokens representing the throw statement
        /// @return `std::optional<ThrowNode>` An optional ThrowNode if creation is successful, nullopt otherwise
        static std::optional<ThrowNode> create_throw(Scope *scope, token_list &tokens);

        /// @function `create_return`
        /// @brief Creates a ReturnNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the return statement is defined
        /// @param `tokens` The list of tokens representing the return statement
        /// @return `std::optional<ReturnNode>` An optional ReturnNode if creation is successful, nullopt otherwise
        static std::optional<ReturnNode> create_return(Scope *scope, token_list &tokens);

        /// @function `create_if`
        /// @brief Creates an IfNode from the given if chain
        ///
        /// @param `scope` The scope in which the if statement is defined
        /// @param `if_chain` The list of token pairs representing the if statement chain
        /// @return `std::optional<std::unique_ptr<IfNode>>` An optional unique pointer to the created IfNode
        static std::optional<std::unique_ptr<IfNode>> create_if(Scope *scope, std::vector<std::pair<token_list, token_list>> &if_chain);

        /// @function `create_while_loop`
        /// @brief Creates a WhileNode from the given definition and body tokens inside the given scope
        ///
        /// @param `scope` The scope in which the while loop is defined
        /// @param `definition` The list of tokens representing the while loop definition
        /// @param `body` The list of tokens representing the while loop body
        /// @return `std::optional<std::unique_ptr<WhileNode>>` An optional unique pointer to the created WhileNode
        static std::optional<std::unique_ptr<WhileNode>> create_while_loop(Scope *scope, const token_list &definition, token_list &body);

        /// @function `create_for_loop`
        /// @brief Creates a ForLoopNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the for loop is defined
        /// @param `definition` The list of tokens representing the for loop definition
        /// @param `body` The list of tokens representing the for loop body
        /// @return `std::optional<std::unique_ptr<ForLoopNode>>` An optional unique pointer to the created ForLoopNode
        static std::optional<std::unique_ptr<ForLoopNode>> create_for_loop( //
            Scope *scope,                                                   //
            const token_list &definition,                                   //
            const token_list &body                                          //
        );

        /// @function `create_enh_for_loop`
        /// @brief Creates an enhanced ForLoopNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the enhanced for loop is defined
        /// @param `definition` The list of tokens representing the enhanced for loop definition
        /// @param `body` The list of tokens representing the enhanced for loop body
        /// @return `std::optional<std::unique_ptr<ForLoopNode>>` An optional unique pointer to the created enhanced ForLoopNode
        ///
        /// @attention This function is currently not implemented and will throw an error if called
        static std::optional<std::unique_ptr<ForLoopNode>> create_enh_for_loop( //
            Scope *scope,                                                       //
            const token_list &definition,                                       //
            const token_list &body                                              //
        );

        /// @function `create_catch`
        /// @brief Creates a CatchNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the catch block is defined
        /// @param `definition` The list of tokens representing the catch block definition
        /// @param `body` The list of tokens representing the catch blocks body
        /// @param `statements` The vector of unique pointers to the created statement nodes
        /// @return `std::optional<std::unique_ptr<CatchNode>>` An optional unique pointer to the created CatchNode
        ///
        /// @note This function parses the catch block definition and creates a CatchNode with the parsed statements. It also sets the
        /// 'has_catch' property of the last parsed call node.
        /// @note All other statements to the left of the catch statement are added to the statements list before parsing and adding the
        /// catch node itself. This is why the reference to the statements list has to be provided.
        static std::optional<std::unique_ptr<CatchNode>> create_catch( //
            Scope *scope,                                              //
            const token_list &definition,                              //
            token_list &body,                                          //
            std::vector<std::unique_ptr<StatementNode>> &statements    //
        );

        /// @function `create_assignment`
        /// @brief Creates an AssignmentNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the assignment is defined
        /// @param `tokens` The list of tokens representing the assignment
        /// @return `std::optional<std::unique_ptr<AssignmentNode>>` An optional unique pointer to the created AssignmentNode
        static std::optional<std::unique_ptr<AssignmentNode>> create_assignment(Scope *scope, token_list &tokens);

        /// @function `create_declaration`
        /// @brief Creates a DeclarationNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the declaration is defined
        /// @param `tokens` The list of tokens representing the declaration
        /// @param `is_infered` Determines whether the type of the declared variable is infered
        /// @return `std::optional<std::unique_ptr<DeclarationNode>>` An optional unique pointer to the created DeclarationNode
        static std::optional<DeclarationNode> create_declaration(Scope *scope, token_list &tokens, const bool &is_infered);

        /// @function `create_statement`
        /// @brief Creates a StatementNode from the given list of tokens
        ///
        /// @param `scope` The scope in which the statement is defined
        /// @param `tokens` The list of tokens representing the statement
        /// @return `std::optional<std::unique_ptr<StatementNode>>` An optional unique pointer to the created StatementNode
        ///
        /// @note This function dispatches to other functions to create specific statement nodes based on the signatures. It also handles
        /// parsing errors and returns nullopt if the statement cannot be parsed.
        static std::optional<std::unique_ptr<StatementNode>> create_statement(Scope *scope, token_list &tokens);

        /// @function `create_scoped_statement`
        /// @brief Creates the AST of a scoped statement like if, loops, catch, switch, etc.
        ///
        /// @param `scope` The scope in which the scoped statement is defined
        /// @param `definition` The token list containing all the definition tokens
        /// @param `body` The token list containing all body tokens
        /// @param `statements` A reference to the list of all currently parserd statements
        /// @return `std::optional<std::unique_ptr<StatementNode>>` An optional unique pointer to the created StatementNode
        ///
        /// @note This function creates a new scope and recursively parses the statements within the scoped block. It also handles parsing
        /// errors and returns nullopt if the scoped statement cannot be parsed.
        /// @note If the scoped statement is a catch statement, all other statements left of it are added to the statements list before
        /// parsing and adding the catch node itself. This is why the reference to the statements list has to be provided.
        static std::optional<std::unique_ptr<StatementNode>> create_scoped_statement( //
            Scope *scope,                                                             //
            token_list &definition,                                                   //
            token_list &body,                                                         //
            std::vector<std::unique_ptr<StatementNode>> &statements                   //
        );

        /// @function `create_body`
        /// @brief Creates a body containing of multiple statement nodes from a list of tokens
        ///
        /// @param `scope` The scope of the body to generate. All generated body statements will be added to this scope
        /// @param `body` The token list containing all the body tokens
        /// @return `std::vectro<std::unique_ptr<StatementNode>>` The list of StatementNodes parsed from the body tokens.
        static std::vector<std::unique_ptr<StatementNode>> create_body(Scope *scope, token_list &body);
    }; // subclass Statement

    /// @class `Definition`
    /// @brief This class is responsible for parsing everything about definitions
    /// @note This class cannot be initialized and all functions within this class are static
    class Definition {
      public:
        Definition() = delete;

        /// The return type of the `create_entity` function
        using create_entity_type = std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>>;

        /// @function `create_function`
        /// @brief Creates a FunctionNode from the given definiton tokens of the FunctionNode as well as its body. Will cause additional
        /// creation of AST Nodes for the body
        ///
        /// @param `definition` The list of tokens representing the function definition
        /// @param `body` The list of tokens representing the function body
        /// @return `FunctionNode` The created FunctionNode
        static FunctionNode create_function(const token_list &definition, token_list &body);

        /// @function `create_data`
        /// @brief Creates a DataNode from the given definition and body tokens
        ///
        /// @param `definition` The list of tokens representing the data definition
        /// @param `body` The list of tokens representing the data body
        /// @return `DataNode` The created DataNode
        static DataNode create_data(const token_list &definition, const token_list &body);

        /// @function `create_func`
        /// @brief Creates a FuncNode from the given definition and body tokens
        ///
        /// @param `definition` The list of tokens representing the function definition
        /// @param `body` The list of tokens representing the function body
        /// @return `FuncNode` The created FuncNode
        ///
        /// @note The FuncNode's body is only allowed to house function definitions, and each function has a body respectively
        static FuncNode create_func(const token_list &definition, token_list &body);

        /// @function `create_entity`
        /// @brief Creates an EntityNode from the given definition and body tokens
        ///
        /// @details An Entity can either be monolithic or modular. If its modular, only the EntityNode (result.first) will be returned.
        /// However, if it is monolithic, the data and func content will be returned within the optional pair. The data and func modules
        /// then will be added to the AST too. "Monolithic" entities are no different to modular ones internally.
        ///
        /// @param `definition` The list of tokens representing the entity definition
        /// @param `body` The list of tokens representing the entity body
        /// @return `create_entity_type` A pair containing the created EntityNode and an optional pair of DataNode and FuncNode if the
        /// entity was monolithic
        static create_entity_type create_entity(const token_list &definition, token_list &body);

        /// @function `create_links`
        /// @brief Creates a list of LinkNode's from a given body containing those links
        ///
        /// @param `body` The list of tokens forming the body containing all the link statements
        /// @return `std::vector<std::unique_ptr<LinkNode>>` A vector of created LinkNode
        static std::vector<std::unique_ptr<LinkNode>> create_links(token_list &body);

        /// @function `create_link`
        /// @brief Creates a LinkNode from the given list of tokens
        ///
        /// @param `tokens` The list of tokens representing the link
        /// @return `LinkNode` The created LinkNode
        static LinkNode create_link(const token_list &tokens);

        /// @function `create_enum`
        /// @brief Creates an EnumNode from the given definition and body tokens
        ///
        /// @param `definition` The list of tokens representing the enum definition
        /// @param `body` The list of tokens representing the enum body
        /// @return `EnumNode` The created EnumNode
        static EnumNode create_enum(const token_list &definition, const token_list &body);

        /// @function `create_error`
        /// @brief Creates an ErrorNode from the given definition and body tokens
        ///
        /// @param `definition` The list of tokens representing the error definition
        /// @param `body` The list of tokens representing the error body
        /// @return `ErrorNode` The created ErrorNode
        static ErrorNode create_error(const token_list &definition, const token_list &body);

        /// @function `create_variant`
        /// @brief Creates a VariantNode from the given definition and body tokens
        ///
        /// @param `definition` The list of tokens representing the variant definition
        /// @param `body` The list of tokens representing the variant body
        /// @return `VariantNode` The created VariantNode
        static VariantNode create_variant(const token_list &definition, const token_list &body);

        /// @function `create_import`
        /// @brief Creates an ImportNode from the given list of tokens
        ///
        /// @param `tokens` The list of tokens containing the import node
        /// @return `ImportNode` The created ImportNode
        static ImportNode create_import(const token_list &tokens);
    }; // subclass Definition
};

#endif
