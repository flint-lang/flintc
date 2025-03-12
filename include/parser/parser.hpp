#pragma once

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
#include "ast/definitions/test_node.hpp"
#include "ast/definitions/variant_node.hpp"

#include "ast/statements/assignment_node.hpp"
#include "ast/statements/call_node_statement.hpp"
#include "ast/statements/declaration_node.hpp"
#include "ast/statements/for_loop_node.hpp"
#include "ast/statements/group_assignment_node.hpp"
#include "ast/statements/group_declaration_node.hpp"
#include "ast/statements/if_node.hpp"
#include "ast/statements/return_node.hpp"
#include "ast/statements/statement_node.hpp"
#include "ast/statements/while_node.hpp"
#include "parser/ast/statements/unary_op_statement.hpp"

#include "ast/expressions/binary_op_node.hpp"
#include "ast/expressions/call_node_expression.hpp"
#include "ast/expressions/expression_node.hpp"
#include "ast/expressions/group_expression_node.hpp"
#include "ast/expressions/initializer_node.hpp"
#include "ast/expressions/literal_node.hpp"
#include "ast/expressions/type_cast_node.hpp"
#include "ast/expressions/unary_op_expression.hpp"
#include "ast/expressions/variable_node.hpp"
#include "ast/statements/catch_node.hpp"
#include "ast/statements/throw_node.hpp"

#include <cassert>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

/// @class `Parser`
/// @brief The class which is responsible for the AST generation (parsing)
/// @note This class cannot be initialized and all functions within this class are static
class Parser {
  public:
    /// @function `create`
    /// @brief Creates a new instance of the Parser. The parser itself owns all instances of the parser
    static Parser *create(const std::filesystem::path &file);

    /// @function `parse`
    /// @brief Parses the file. It will tokenize it using the Lexer and then create the AST of the file and return the parsed FileNode
    ///
    /// @param `file` The path to the file to tokenize and parse
    /// @return `std::optional<FileNode>` The parsed file, if it succeeded parsing
    ///
    /// @note This function creates a new Lexer class to tokenize the given file, so no further tokenization has to be made, this function
    /// takes care of the tokenization too
    std::optional<FileNode> parse();

    /// @function `get_call_from_id`
    /// @brief Returns the call node from the given id
    ///
    /// @param `call_id` The id from which the call node will be returned
    /// @return `std::optional<CallNodeBase *>` An optional pointer to the CallNodeBase, nullopt if no call with the given call id exists
    static std::optional<CallNodeBase *> get_call_from_id(unsigned int call_id);

    /// @function `parse_all_open_functions`
    /// @brief Parses all still open function bodies
    ///
    /// @param `parse_parallel` Whether to parse the open functions in parallel
    /// @return `bool` Wheter all functions were able to be parsed
    static bool parse_all_open_functions(const bool parse_parallel);

    /// @function `parse_all_open_tests`
    /// @brief Parses all still open test bodies
    ///
    /// @param `parse_parallel` Whether to parse the open tests in parallel
    /// @return `bool` Whether all tests were able to be parsed
    /// @note This will only be called when the developer wants to run the tests, e.g. make a test build. In the normal compilation
    /// pipeline, the parsing and generation of all tests will not be done, to make compilation as fast as possible.
    static bool parse_all_open_tests(const bool parse_parallel);

    /// @function `clear_instances`
    /// @brief Clears all parser instances
    static void clear_instances() {
        instances.clear();
    }

  private:
    // The constructor is private because only the Parser (the instances list) contains the actual Parser
    explicit Parser(const std::filesystem::path &file) :
        file(file),
        file_name(file.filename().string()) {};

    /// @var `instances`
    /// @brief All Parser instances which are present. Used by the two-pass parsing system
    static std::vector<Parser> instances;

    /// @var `token_precedence`
    /// @brief
    ///
    /// @details This map exists to map the token types to their respective precedence values. The higher the precedence the sooner this
    /// token will be evaluated in, for example, a binary operation.
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

    /// @var `parsed_calls`
    /// @brief Stores all the calls that have been parsed
    ///
    /// @details This map exists to keep track of all parsed call nodes. This map is not allowed to be changed to an unordered_map, because
    /// the elements of the map are required to perserve their ordering. Most of times only the last element from this map is searched for,
    /// this is the reason to why the ordering is important.
    static std::map<unsigned int, CallNodeBase *> parsed_calls;

    /// @var `parsed_calls_nodes_mutex`
    /// @brief A mutex for the `parsed_calls` variable, which is used to provide thread-safe access to the map
    static std::mutex parsed_calls_mutex;

    /// @var `parsed_functions`
    /// @brief Stores all the functions that have been parsed
    ///
    /// @details This list exists to keep track of all parsed function nodes
    static std::vector<std::pair<FunctionNode *, std::string>> parsed_functions;

    /// @var `parsed_functions_mutex`
    /// @brief A mutex for the `parsed_functions` variable, which is used to provide thread-safe access to the list
    static std::mutex parsed_functions_mutex;

    /// @var `parsed_tests`
    /// @brief Stores all the tests that have been parsed
    ///
    /// @details The list exists to keep track of all parsed test nodes
    static std::vector<std::pair<TestNode *, std::string>> parsed_tests;

    /// @var `parsed_tests_mutex`
    /// @brief A mutex for the `parsed_tests` varible, which is used to provide thread-safe access to the list
    static std::mutex parsed_tests_mutex;

    /// @var `parsed_data`
    /// @brief Stores all the data nodes that have been parsed
    ///
    /// @details The key is the file in which the data definitions are defined in, and the value is a list of all data nodes in that file
    static std::unordered_map<std::string, std::vector<DataNode *>> parsed_data;

    /// @var `parsed_data_mutex`
    /// @brief A mutex for the `parsed_data` variable, which is used to provide thread-safe access to the map
    static std::mutex parsed_data_mutex;

    /// @var `open_functions_list`
    /// @brief The list of all open functions, which will be parsed in the second phase of the parser
    std::vector<std::pair<FunctionNode *, token_list>> open_functions_list{};

    /// @var `open_tests_list`
    /// @brief The lsit of all open tests, which will be parsed in the second phase of the parser
    std::vector<std::pair<TestNode *, token_list>> open_tests_list{};

    /// @var `imported_files`
    /// @brief The list of all files the file that is currently parsed can seee / has imported
    std::vector<ImportNode *> imported_files{};

    /// @var `file`
    /// @brief The path to the file to parse
    const std::filesystem::path file;

    /// @var `file_name`
    /// @brief The name of the file to be parsed
    const std::string file_name;

    /// @function `set_last_parsed_call`
    /// @brief Sets the last parsed call
    ///
    /// @param `call_id` The call ID of the CallNode to add
    /// @param `call` The pointer to the base of the CallNode to add
    static inline void set_last_parsed_call(unsigned int call_id, CallNodeBase *call) {
        // The mutex will unlock by itself when it goes out of scope
        std::lock_guard<std::mutex> lock(parsed_calls_mutex);
        parsed_calls.emplace(call_id, call);
    }

    /// @function `get_last_parsed_call_id`
    /// @brief Returns the ID of the last parsed call
    ///
    /// @return The ID of the last parsed call
    ///
    /// @attention Asserts that the call_nodes map contains at least one element
    static inline unsigned int get_last_parsed_call_id() {
        // The mutex will unlock by itself when it goes out of scope
        std::lock_guard<std::mutex> lock(parsed_calls_mutex);
        assert(!parsed_calls.empty());
        return parsed_calls.rbegin()->first;
    }

    /// @function `add_parsed_function`
    /// @brief Adds a given function combined with the file it is contained in
    ///
    /// @param `function_node` The function which was parsed
    /// @param `file_name` The name of the file the function was parsed in
    static inline void add_parsed_function(FunctionNode *function_node, std::string file_name) {
        std::lock_guard<std::mutex> lock(parsed_functions_mutex);
        parsed_functions.emplace_back(function_node, file_name);
    }

    /// @function `add_parsed_test`
    /// @brief Adds a given test combined with the file it is contained in
    ///
    /// @param `test_node` The test which was parsed
    /// @param `file_name` The name of the file the test was parsed in
    static inline void add_parsed_test(TestNode *test_node, std::string file_name) {
        std::lock_guard<std::mutex> lock(parsed_tests_mutex);
        parsed_tests.emplace_back(test_node, file_name);
    }

    /// @function `add_parsed_data`
    /// @brief Adds a given data node to the map of all data nodes
    ///
    /// @param `data_node` The data node which was parsed
    /// @param `file_name` The name of the file the data node is contained in
    static inline void add_parsed_data(DataNode *data_node, std::string file_name) {
        std::lock_guard<std::mutex> lock(parsed_data_mutex);
        parsed_data[file_name].emplace_back(data_node);
    }

    /// @function `get_function_from_call`
    /// @brief Looks through all parsed functions for a match for the given function call
    ///
    /// @param `call_node` The call node the search for the corresponding FunctionNode is done for
    /// @return `std::optional<std::pair<FunctionNode *, std::string>>` A pair of the function node and its file, or nullopt if no match
    /// could be found
    ///
    /// @attention Asserts that the parameter `call_node` is not a nullptr
    static std::optional<std::pair<FunctionNode *, std::string>> get_function_from_call( //
        const std::string &call_name,                                                    //
        const std::vector<std::string> &arg_types                                        //
    );

    /// @function `get_data_definition`
    /// @brief Get the DataNode of the given data name and arg types of the data`s constructor
    ///
    /// @param `file_name` The name of the current file
    /// @param `data_name` The name of the data node to search for
    /// @param `imports` All the use statements of the current file
    /// @param `arg_types` The argument types with which said type is initialized with
    /// @param `is_known_data_type` Whether the given data type should be a data type, set this to false if you only want to check _if_ its
    /// a data type but dont want to generate errors if its not
    /// @return `std::optional<DataNode *>` The pointer to the data node, if it exists
    static std::optional<DataNode *> get_data_definition( //
        const std::string &file_name,                     //
        const std::string &data_name,                     //
        const std::vector<ImportNode *> &imports,         //
        const std::vector<std::string> &arg_types,        //
        const bool is_known_data_type = true              //
    );

    /// @function `add_open_function`
    /// @brief Adds a open function to the list of all open functions
    ///
    /// @param `open_function` A rvalue reference to the OpenFunction to add to the list
    ///
    /// @attention This function takes ownership of the `open_function` parameter
    void add_open_function(std::pair<FunctionNode *, token_list> &&open_function) {
        open_functions_list.push_back(std::move(open_function));
    }

    /// @function `get_next_open_function`
    /// @brief Returns the next open function to parse
    ///
    /// @return `std::optional<OpenFunction>` The next Open Function to parse. Returns a nullopt if there are no open functions left
    std::optional<std::pair<FunctionNode *, token_list>> get_next_open_function() {
        if (open_functions_list.empty()) {
            return std::nullopt;
        }
        std::pair<FunctionNode *, token_list> of = std::move(open_functions_list.back());
        open_functions_list.pop_back();
        return of;
    }

    /// @function `add_open_test`
    /// @brief Adds a open test to the list of all open tests
    ///
    /// @param `open_test` A rvalue reference to the OpenTest to add to the list
    ///
    /// @attention This function takes ownership of the `open_test` parameter
    void add_open_test(std::pair<TestNode *, token_list> &&open_test) {
        open_tests_list.push_back(std::move(open_test));
    }

    /// @function `get_next_open_test`
    /// @brief Returns the next open test to parse
    ///
    /// @return `std::optional<OpenTest>` The next Open Test to parse. Returns a nullopt if there are no open tests left
    std::optional<std::pair<TestNode *, token_list>> get_next_open_test() {
        if (open_tests_list.empty()) {
            return std::nullopt;
        }
        std::pair<TestNode *, token_list> ot = std::move(open_tests_list.back());
        open_tests_list.pop_back();
        return ot;
    }

    /// @function `remove_leading_garbage`
    /// @brief Removes all leading garbage, such as indentation or eol characters from the list of tokens
    ///
    /// @param `tokens` The tokens in which to remove all leading garbage
    static void remove_leading_garbage(token_list &tokens) {
        for (auto it = tokens.begin(); it != tokens.end();) {
            if (it->type == TOK_INDENT || it->type == TOK_EOL) {
                tokens.erase(it);
            } else {
                break;
            }
        }
    }

    /// @function `remove_trailing_garbage`
    /// @brief Removes all trailing garbage, such as indentation, eol characters or semicolons from the list of tokens
    ///
    /// @param `tokens` The tokens in which to remove all trailing garbage
    static void remove_trailing_garbage(token_list &tokens) {
        for (auto it = tokens.rbegin(); it != tokens.rend();) {
            if (it->type == TOK_INDENT || it->type == TOK_EOL || it->type == TOK_SEMICOLON) {
                ++it;
                tokens.erase(std::prev(it).base());
            } else {
                break;
            }
        }
    }

    /// @function `remove_surrounding_paren`
    /// @brief Removes surrounding parenthesis of the given token list if they are at the begin and the end
    ///
    /// @param `tokens` The tokens in which to remove the surrounding parens, if they exist
    static void remove_surrounding_paren(token_list &tokens) {
        if (tokens.begin()->type == TOK_LEFT_PAREN && std::prev(tokens.end())->type == TOK_RIGHT_PAREN) {
            tokens.erase(tokens.begin());
            tokens.pop_back();
        }
    }

    /**************************************************************************************************************************************
     * @region `Util`
     * @brief This region is responsible for priving common functionality
     *************************************************************************************************************************************/

    /// @function `add_next_main_node`
    /// @brief Creates the next main node from the list of tokens and adds it to the file node
    ///
    /// @details A main node is every node which is able to be at the top-level of a file (functions, modules, file imports etc.)
    ///
    /// @param `file_node` The file node the next created main node will be added to
    /// @param `tokens` The list of tokens from which the next main node will be created from
    /// @return `bool` Whether the next main node was added correctly. Returns false if there was an error
    bool add_next_main_node(FileNode &file_node, token_list &tokens);

    /// @function `get_definition_tokens`
    /// @brief Extracts all the tokens which are part of the definition
    ///
    /// @return `token_list` Returns the extracted tokens, being part of the definition
    ///
    /// @attention Deletes all tokens which are part of the definition from the given tokens list
    token_list get_definition_tokens(token_list &tokens);

    /// @function `get_body_tokens`
    /// @brief Extracts all the body tokens based on their indentation
    ///
    /// @param `definition_indentation` The indentation level of the definition. The body of the definition will have at least one
    /// indentation level more
    /// @param `tokens` The tokens from which the body will be extracted from
    /// @return `token_list` The extracted list of tokens forming the whole body
    ///
    /// @attention This function modifies the given `tokens` list, the retured tokens are no longer part of the given list
    token_list get_body_tokens(unsigned int definition_indentation, token_list &tokens);

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
    token_list extract_from_to(unsigned int from, unsigned int to, token_list &tokens);

    /// @function `clone_from_to`
    /// @brief Clones the tokens from a given index up to the given index from the given tokens list
    ///
    /// @param `from` The start index from which the cloning starts (inclusive)
    /// @param `to` The end index at which the cloning ends (exclusive)
    /// @param `tokens` The tokens from which the range is cloned
    /// @return `token_list` The cloned token range
    ///
    /// @assert to >= from and to <= tokens.size()
    token_list clone_from_to(unsigned int from, unsigned int to, const token_list &tokens);

    /// @function `create_call_or_initializer_base`
    /// @brief Creates the base node for all calls or initializers
    ///
    /// @details This function is part of the `Util` class, as both call statements as well as call expressions use this function
    ///
    /// @param `scope` The scope the call happens in
    /// @param `tokens` The tokens which will be interpreted as call
    /// @return A optional value containing a tuple, where:
    ///         - the first value is the function or initializers name
    ///         - the second value is a list of all expressions (the argument expression) of the call / initializier
    ///         - the third value is the call's return type, or the initializers type encoded as a string
    ///         - the forth value is: true if the expression is a Data initializer, false if an entity initializer, nullopt if its a call
    std::optional<std::tuple<std::string, std::vector<std::unique_ptr<ExpressionNode>>, std::string, std::optional<bool>>>
    create_call_or_initializer_base(Scope *scope, token_list &tokens);

    /// @function `create_unary_op_base`
    /// @brief Creates a UnaryOpBase from the given tokens
    ///
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return A optional value containing a tuple, where the first value is the operator token, the second value is the expression on
    /// which the unary operation is applied on and the third value is whether the unary operator is left to the expression
    std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> create_unary_op_base(Scope *scope, token_list &tokens);

    /**************************************************************************************************************************************
     * @region `Util` END
     *************************************************************************************************************************************/

    /**************************************************************************************************************************************
     * @region `Expression`
     * @brief This region is responsible for parsing everything about expressions
     *************************************************************************************************************************************/

    /// @function `create_variable`
    /// @brief Creates a VariableNode from the given tokens
    ///
    /// @param `scope` The scope in which the variable is defined
    /// @param `tokens` The list of tokens representing the variable
    /// @return `std::optional<VariableNode>` An optional VariableNode if creation is successful, nullopt otherwise
    std::optional<VariableNode> create_variable(Scope *scope, const token_list &tokens);

    /// @function `create_unary_op_expression`
    /// @brief Creates a UnaryOpExpression from the given tokens
    ///
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return `std::optional<UnaryOpExpression>` An optional UnaryOpExpression if creation is successful, nullopt otherwise
    std::optional<UnaryOpExpression> create_unary_op_expression(Scope *scope, token_list &tokens);

    /// @function `create_literal`
    /// @brief Creates a LiteralNode from the given tokens
    ///
    /// @param `tokens` The list of tokens representing the literal
    /// @return `std::optional<LiteralNode>` An optional LiteralNode if creation is successful, nullopt otherwise
    std::optional<LiteralNode> create_literal(const token_list &tokens);

    /// @function `create_call_or_initializer_expression`
    /// @brief Creates a CallNodeExpression from the given tokens
    ///
    /// @param `scope` The scope in which the call expression is defined
    /// @param `tokens` The list of tokens representing the call expression
    /// @return `std::optional<std::variant<std::unique_ptr<CallNodeExpression>, std::unique_ptr<InitializerNode>>` A unique pointer to the
    /// created CallNodeExpression or InitializerNode, depending on if it was an call or initialization
    std::optional<std::variant<std::unique_ptr<CallNodeExpression>, std::unique_ptr<InitializerNode>>>
    create_call_or_initializer_expression(Scope *scope, token_list &tokens);

    /// @function `create_binary_op`
    /// @brief Creates a BinaryOpNode from the given tokens
    ///
    /// @param `scope` The scope in which the binary operation is defined
    /// @param `tokens` The list of tokens representing the binary operation
    /// @return `std::optional<BinaryOpNode>` An optional BinaryOpNode if creation is successful, nullopt otherwise
    std::optional<BinaryOpNode> create_binary_op(Scope *scope, token_list &tokens);

    /// @function `create_type_cast`
    /// @brief Creates a TypeCastNode from the given tokens
    ///
    /// @param `scope` The scope in which the type cast is defined
    /// @param `tokens` The list of tokens representing the type cast
    /// @return `std::optional<TypeCastNode>` An optional TypeCastNode if creation is successful, nullopt otherwise
    std::optional<TypeCastNode> create_type_cast(Scope *scope, token_list &tokens);

    /// @function `create_group_expression`
    /// @brief Creates a GroupExpressionNode from the given tokens
    ///
    /// @param `scope` The scope in which the grouped expression is defined
    /// @param `tokens` The list of tokens representing the type cast
    /// @return `std::optional<GroupExpressionNode>` An optiional grouped expression, nullopt otherwise
    std::optional<GroupExpressionNode> create_group_expression(Scope *scope, token_list &tokens);

    /// @function `create_expression`
    /// @brief Creates an ExpressionNode from the given tokens
    ///
    /// @param `scope` The scope in which the expression is defined
    /// @param `tokens` The list of tokens representing the expression
    /// @param `expected_type` The expected type of the expression. If possible, applies implicit type conversion to get this type
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An optional unique pointer to the created ExpressionNode
    std::optional<std::unique_ptr<ExpressionNode>> create_expression( //
        Scope *scope,                                                 //
        const token_list &tokens,                                     //
        const std::optional<std::string> expected_type = std::nullopt //
    );

    /**************************************************************************************************************************************
     * @region `Expression` END
     *************************************************************************************************************************************/

    /**************************************************************************************************************************************
     * @region `Statement`
     * @brief This region is responsible for parsing everything about statements
     *************************************************************************************************************************************/

    /// @function `create_call_statement`
    /// @brief Creates a CallNodeStatement from the given list of tokens
    ///
    /// @param `scope` The scope in which the call statement is defined
    /// @param `tokens` The list of tokens representing the call statement
    /// @return `std::optional<std::unique_ptr<CallNodeStatement>>` A unique pointer to the created CallNodeStatement
    std::optional<std::unique_ptr<CallNodeStatement>> create_call_statement(Scope *scope, token_list &tokens);

    /// @function `create_throw`
    /// @brief Creates a ThrowNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the throw statement is defined
    /// @param `tokens` The list of tokens representing the throw statement
    /// @return `std::optional<ThrowNode>` An optional ThrowNode if creation is successful, nullopt otherwise
    std::optional<ThrowNode> create_throw(Scope *scope, token_list &tokens);

    /// @function `create_return`
    /// @brief Creates a ReturnNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the return statement is defined
    /// @param `tokens` The list of tokens representing the return statement
    /// @return `std::optional<ReturnNode>` An optional ReturnNode if creation is successful, nullopt otherwise
    std::optional<ReturnNode> create_return(Scope *scope, token_list &tokens);

    /// @function `create_if`
    /// @brief Creates an IfNode from the given if chain
    ///
    /// @param `scope` The scope in which the if statement is defined
    /// @param `if_chain` The list of token pairs representing the if statement chain
    /// @return `std::optional<std::unique_ptr<IfNode>>` An optional unique pointer to the created IfNode
    std::optional<std::unique_ptr<IfNode>> create_if(Scope *scope, std::vector<std::pair<token_list, token_list>> &if_chain);

    /// @function `create_while_loop`
    /// @brief Creates a WhileNode from the given definition and body tokens inside the given scope
    ///
    /// @param `scope` The scope in which the while loop is defined
    /// @param `definition` The list of tokens representing the while loop definition
    /// @param `body` The list of tokens representing the while loop body
    /// @return `std::optional<std::unique_ptr<WhileNode>>` An optional unique pointer to the created WhileNode
    std::optional<std::unique_ptr<WhileNode>> create_while_loop(Scope *scope, const token_list &definition, token_list &body);

    /// @function `create_for_loop`
    /// @brief Creates a ForLoopNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the for loop is defined
    /// @param `definition` The list of tokens representing the for loop definition
    /// @param `body` The list of tokens representing the for loop body
    /// @return `std::optional<std::unique_ptr<ForLoopNode>>` An optional unique pointer to the created ForLoopNode
    std::optional<std::unique_ptr<ForLoopNode>> create_for_loop(Scope *scope, const token_list &definition, token_list &body);

    /// @function `create_enh_for_loop`
    /// @brief Creates an enhanced ForLoopNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the enhanced for loop is defined
    /// @param `definition` The list of tokens representing the enhanced for loop definition
    /// @param `body` The list of tokens representing the enhanced for loop body
    /// @return `std::optional<std::unique_ptr<ForLoopNode>>` An optional unique pointer to the created enhanced ForLoopNode
    ///
    /// @attention This function is currently not implemented and will throw an error if called
    std::optional<std::unique_ptr<ForLoopNode>> create_enh_for_loop( //
        Scope *scope,                                                //
        const token_list &definition,                                //
        const token_list &body                                       //
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
    std::optional<std::unique_ptr<CatchNode>> create_catch(     //
        Scope *scope,                                           //
        const token_list &definition,                           //
        token_list &body,                                       //
        std::vector<std::unique_ptr<StatementNode>> &statements //
    );

    /// @function `create_group_assignment`
    /// @brief Creates an GroupAssignmentNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the assignment is defined
    /// @param `tokens` The list of tokens representing the assignment
    /// @return `std::optional<std::unique_ptr<GroupAssignmentNode>>` An optional unique pointer to the created GroupAssignmentNode
    std::optional<std::unique_ptr<GroupAssignmentNode>> create_group_assignment(Scope *scope, token_list &tokens);

    /// @function `create_assignment`
    /// @brief Creates an AssignmentNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the assignment is defined
    /// @param `tokens` The list of tokens representing the assignment
    /// @return `std::optional<std::unique_ptr<AssignmentNode>>` An optional unique pointer to the created AssignmentNode
    std::optional<std::unique_ptr<AssignmentNode>> create_assignment(Scope *scope, token_list &tokens);

    /// @function `create_group_declaration`
    /// @brief Creates a GroupDeclarationNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the group declaration is defined
    /// @param `tokens` The list of tokens representing the group declaration
    /// @return `std::optional<GroupDeclarationNode>` An optional GroupDeclarationNode, if creation was sucessfull
    ///
    /// @note A group declaration is _always_ inferred and cannot be not inferred
    std::optional<GroupDeclarationNode> create_group_declaration(Scope *scope, token_list &tokens);

    /// @function `create_declaration`
    /// @brief Creates a DeclarationNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the declaration is defined
    /// @param `tokens` The list of tokens representing the declaration
    /// @param `is_inferred` Determines whether the type of the declared variable is inferred
    /// @param `has_rhs` Determines whether the declaration even has a rhs
    /// @return `std::optional<DeclarationNode>` An optional DeclarationNode, if creation was sucessfull
    std::optional<DeclarationNode> create_declaration(Scope *scope, token_list &tokens, const bool is_inferred, const bool has_rhs);

    /// @function `create_unary_op_statement`
    /// @brief Creates a UnaryOpStatement from the given tokens
    ///
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return `std::optional<UnaryOpStatement>` An optional UnaryOpStatement if creation is successful, nullopt otherwise
    std::optional<UnaryOpStatement> create_unary_op_statement(Scope *scope, token_list &tokens);

    /// @function `create_statement`
    /// @brief Creates a StatementNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the statement is defined
    /// @param `tokens` The list of tokens representing the statement
    /// @return `std::optional<std::unique_ptr<StatementNode>>` An optional unique pointer to the created StatementNode
    ///
    /// @note This function dispatches to other functions to create specific statement nodes based on the signatures. It also handles
    /// parsing errors and returns nullopt if the statement cannot be parsed.
    std::optional<std::unique_ptr<StatementNode>> create_statement(Scope *scope, token_list &tokens);

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
    std::optional<std::unique_ptr<StatementNode>> create_scoped_statement( //
        Scope *scope,                                                      //
        token_list &definition,                                            //
        token_list &body,                                                  //
        std::vector<std::unique_ptr<StatementNode>> &statements            //
    );

    /// @function `create_body`
    /// @brief Creates a body containing of multiple statement nodes from a list of tokens
    ///
    /// @param `scope` The scope of the body to generate. All generated body statements will be added to this scope
    /// @param `body` The token list containing all the body tokens
    /// @return `std::optional<std::vectro<std::unique_ptr<StatementNode>>>` The list of StatementNodes parsed from the body tokens.
    std::optional<std::vector<std::unique_ptr<StatementNode>>> create_body(Scope *scope, token_list &body);

    /**************************************************************************************************************************************
     * @region `Statement` END
     *************************************************************************************************************************************/

    /**************************************************************************************************************************************
     * @region `Definition`
     * @brief This region is responsible for parsing everything about definitions
     *************************************************************************************************************************************/

    /// The return type of the `create_entity` function
    using create_entity_type = std::pair<EntityNode, std::optional<std::pair<std::unique_ptr<DataNode>, std::unique_ptr<FuncNode>>>>;

    /// @function `create_function`
    /// @brief Creates a FunctionNode from the given definiton tokens of the FunctionNode as well as its body. Will cause additional
    /// creation of AST Nodes for the body
    ///
    /// @param `definition` The list of tokens representing the function definition
    /// @return `std::optional<FunctionNode>` The created FunctionNode
    std::optional<FunctionNode> create_function(const token_list &definition);

    /// @function `create_data`
    /// @brief Creates a DataNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the data definition
    /// @param `body` The list of tokens representing the data body
    /// @return `std::optional<DataNode>` The created DataNode, nullopt if creation failed
    std::optional<DataNode> create_data(const token_list &definition, const token_list &body);

    /// @function `create_func`
    /// @brief Creates a FuncNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the function definition
    /// @param `body` The list of tokens representing the function body
    /// @return `std::optional<FuncNode>` The created FuncNode or nullopt if creation failed
    ///
    /// @note The FuncNode's body is only allowed to house function definitions, and each function has a body respectively
    std::optional<FuncNode> create_func(const token_list &definition, token_list &body);

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
    create_entity_type create_entity(const token_list &definition, token_list &body);

    /// @function `create_links`
    /// @brief Creates a list of LinkNode's from a given body containing those links
    ///
    /// @param `body` The list of tokens forming the body containing all the link statements
    /// @return `std::vector<std::unique_ptr<LinkNode>>` A vector of created LinkNode
    std::vector<std::unique_ptr<LinkNode>> create_links(token_list &body);

    /// @function `create_link`
    /// @brief Creates a LinkNode from the given list of tokens
    ///
    /// @param `tokens` The list of tokens representing the link
    /// @return `LinkNode` The created LinkNode
    LinkNode create_link(const token_list &tokens);

    /// @function `create_enum`
    /// @brief Creates an EnumNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the enum definition
    /// @param `body` The list of tokens representing the enum body
    /// @return `EnumNode` The created EnumNode
    EnumNode create_enum(const token_list &definition, const token_list &body);

    /// @function `create_error`
    /// @brief Creates an ErrorNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the error definition
    /// @param `body` The list of tokens representing the error body
    /// @return `ErrorNode` The created ErrorNode
    ErrorNode create_error(const token_list &definition, const token_list &body);

    /// @function `create_variant`
    /// @brief Creates a VariantNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the variant definition
    /// @param `body` The list of tokens representing the variant body
    /// @return `VariantNode` The created VariantNode
    VariantNode create_variant(const token_list &definition, const token_list &body);

    /// @function `create_test`
    /// @brief Creates a TestNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the test definition
    /// @return `std::optional<TestNode>` The created TestNode, if creation was successful, nullopt otherwise
    std::optional<TestNode> create_test(const token_list &definition);

    /// @function `create_import`
    /// @brief Creates an ImportNode from the given list of tokens
    ///
    /// @param `tokens` The list of tokens containing the import node
    /// @return `ImportNode` The created ImportNode
    ImportNode create_import(const token_list &tokens);

    /**************************************************************************************************************************************
     * @region `Definition` END
     *************************************************************************************************************************************/
};
