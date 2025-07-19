#pragma once

#include "matcher/matcher.hpp"
#include "resolver/resolver.hpp"
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

#include "ast/statements/array_assignment_node.hpp"
#include "ast/statements/assignment_node.hpp"
#include "ast/statements/call_node_statement.hpp"
#include "ast/statements/data_field_assignment_node.hpp"
#include "ast/statements/declaration_node.hpp"
#include "ast/statements/enhanced_for_loop_node.hpp"
#include "ast/statements/for_loop_node.hpp"
#include "ast/statements/group_assignment_node.hpp"
#include "ast/statements/group_declaration_node.hpp"
#include "ast/statements/grouped_data_field_assignment_node.hpp"
#include "ast/statements/if_node.hpp"
#include "ast/statements/return_node.hpp"
#include "ast/statements/statement_node.hpp"
#include "ast/statements/unary_op_statement.hpp"
#include "ast/statements/while_node.hpp"
#include "parser/ast/statements/switch_statement.hpp"

#include "ast/expressions/array_access_node.hpp"
#include "ast/expressions/array_initializer_node.hpp"
#include "ast/expressions/data_access_node.hpp"
#include "ast/expressions/expression_node.hpp"
#include "ast/expressions/group_expression_node.hpp"
#include "ast/expressions/grouped_data_access_node.hpp"
#include "ast/expressions/literal_node.hpp"
#include "ast/expressions/string_interpolation_node.hpp"
#include "ast/expressions/unary_op_expression.hpp"
#include "ast/expressions/variable_node.hpp"
#include "ast/statements/catch_node.hpp"
#include "ast/statements/throw_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"

#include "parser/type/multi_type.hpp"

#include <atomic>
#include <cassert>
#include <filesystem>
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
    /// @return `std::optional<FileNode *>` The parsed file, if it succeeded parsing
    ///
    /// @note This function creates a new Lexer class to tokenize the given file, so no further tokenization has to be made, this function
    /// takes care of the tokenization too
    std::optional<FileNode *> parse();

    /// @function `resolve_all_unknown_types`
    /// @brief Resolves all unknown types to point to real types
    ///
    /// @return `bool` Whether all unknown types could be resolved correctly
    static bool resolve_all_unknown_types();

    /// @function `get_open_functions`
    /// @brief Returns all open functions whose bodies have not been parsed yet
    ///
    /// @return `std::vector<FunctionNode *>` A list of all open functions
    std::vector<FunctionNode *> get_open_functions();

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

    /// @function `get_builtin_function`
    /// @brief Returns the module the builtin function is contained in, as well as the function overloads and possible aliases for it
    ///
    /// @param `function_name` The name of the function to check for
    /// @param `imported_core_modules` The core modules that have been imported in this file
    /// @return `std::optional<std::tuple<std::string, overloads, std::optional<std::string>>>` The builtin function match, nullopt if not
    ///     - The first element is the name of the core module the function comes from
    ///     - The second element are the overloads of the function from the module
    ///     - The third element is the alias of the module, if there is any, nullopt if there is none
    static std::optional<std::tuple<std::string, overloads, std::optional<std::string>>> get_builtin_function( //
        const std::string &function_name,                                                                      //
        const std::unordered_map<std::string, ImportNode *const> &imported_core_modules                        //
    );

    /// @var `main_function_has_args`
    /// @brief Whether the main function has an args list (str[] type)
    static inline std::atomic_bool main_function_has_args{false};

    /// @var `main_function_parsed`
    /// @brief Determines whether the main function has been parsed already. Atomic for thread-safe access
    static inline std::atomic_bool main_function_parsed{false};

    /// @var `parsed_data`
    /// @brief Stores all the data nodes that have been parsed
    ///
    /// @details The key is the file in which the data definitions are defined in, and the value is a list of all data nodes in that file
    static inline std::unordered_map<std::string, std::vector<DataNode *>> parsed_data;

    /// @var `parsed_data_mutex`
    /// @brief A mutex for the `parsed_data` variable, which is used to provide thread-safe access to the map
    static inline std::mutex parsed_data_mutex;

    /// @var `open_functions_list`
    /// @brief The list of all open functions, which will be parsed in the second phase of the parser
    std::vector<std::pair<FunctionNode *, std::vector<Line>>> open_functions_list{};

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

    /// @function `clone_form_slice`
    /// @brief Clines a token_list vector from a given slice. This creates a new vector and returns it. This function is absolutely needed
    /// by the dual phase parsing system, as all bodies of all functions are saved and then parsed one by one in the second parsing phase.
    ///
    /// @param `slice` The token slice to clone
    /// @return `token_list` The real cloned vector from the slice
    static token_list clone_from_slice(const token_slice &slice);

  private:
    // The constructor is private because only the Parser (the instances list) contains the actual Parser
    explicit Parser(const std::filesystem::path &file) :
        file(file),
        file_name(file.filename().string()) {};

    /// @var `instances`
    /// @brief All Parser instances which are present. Used by the two-pass parsing system
    static inline std::vector<Parser> instances;

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
        {TOK_OPT_DEFAULT, 8},
        {TOK_POW, 7},
        {TOK_MOD, 7},
        {TOK_MULT, 6},
        {TOK_DIV, 6},
        {TOK_PLUS, 5},
        {TOK_MINUS, 5},
        {TOK_LESS, 4},
        {TOK_GREATER, 4},
        {TOK_LESS_EQUAL, 4},
        {TOK_GREATER_EQUAL, 4},
        {TOK_EQUAL_EQUAL, 4},
        {TOK_NOT_EQUAL, 4},
        {TOK_NOT, 3},
        {TOK_AND, 2},
        {TOK_OR, 1},
        {TOK_EQUAL, 0},
    };

    /// @enum `Associativity`
    /// @brief The associativity any operational token could have, its either left or right associative
    enum Associativity { LEFT, RIGHT };

    /// @var `token_associativity`
    /// @brief The associativity of every token, `false` if left-associative, `true` if right-associative
    static const inline std::unordered_map<Token, Associativity> token_associativity = {
        {TOK_OPT_DEFAULT, Associativity::LEFT},
        {TOK_POW, Associativity::RIGHT},
        {TOK_MOD, Associativity::LEFT},
        {TOK_MULT, Associativity::LEFT},
        {TOK_DIV, Associativity::LEFT},
        {TOK_PLUS, Associativity::LEFT},
        {TOK_MINUS, Associativity::LEFT},
        {TOK_LESS, Associativity::LEFT},
        {TOK_GREATER, Associativity::LEFT},
        {TOK_LESS_EQUAL, Associativity::LEFT},
        {TOK_GREATER_EQUAL, Associativity::LEFT},
        {TOK_EQUAL_EQUAL, Associativity::LEFT},
        {TOK_NOT_EQUAL, Associativity::LEFT},
        {TOK_NOT, Associativity::RIGHT},
        {TOK_OR, Associativity::LEFT},
        {TOK_AND, Associativity::LEFT},
        {TOK_EQUAL, Associativity::RIGHT},
    };

    /// @var `type_precedence`
    /// @brief Map containing the precedences of types. Lower types will always be cast to higher types, if possible.
    ///
    /// This map exists to ensure that in the expression (4 / 2.4) the left side will be implicitely cast to a float, and not the right side
    /// to an int, for example
    static const inline std::unordered_map<std::string_view, unsigned int> type_precedence = {
        {"str", 7},
        {"f64", 6},
        {"f32", 5},
        {"i64", 4},
        {"i32", 3},
        {"u64", 2},
        {"u32", 1},
        {"bool", 0},
    };

    /// @var `last_parsed_call`
    /// @brief Stores a pointer to the last parsed call of this parser
    std::optional<CallNodeBase *> last_parsed_call;

    /// @var `parsed_functions`
    /// @brief Stores all the functions that have been parsed
    ///
    /// @details This list exists to keep track of all parsed function nodes
    static inline std::vector<std::pair<FunctionNode *, std::string>> parsed_functions;

    /// @var `parsed_functions_mutex`
    /// @brief A mutex for the `parsed_functions` variable, which is used to provide thread-safe access to the list
    static inline std::mutex parsed_functions_mutex;

    /// @var `parsed_tests`
    /// @brief Stores all the tests that have been parsed
    ///
    /// @details The list exists to keep track of all parsed test nodes
    static inline std::vector<std::pair<TestNode *, std::string>> parsed_tests;

    /// @var `parsed_tests_mutex`
    /// @brief A mutex for the `parsed_tests` varible, which is used to provide thread-safe access to the list
    static inline std::mutex parsed_tests_mutex;

    /// @var `open_tests_list`
    /// @brief The lsit of all open tests, which will be parsed in the second phase of the parser
    std::vector<std::pair<TestNode *, std::vector<Line>>> open_tests_list{};

    /// @var `imported_files`
    /// @brief The list of all files the file that is currently parsed can seee / has imported
    std::vector<ImportNode *> imported_files{};

    /// @var `file_node_ptr`
    /// @brief The file node this parser is currently parsing
    std::unique_ptr<FileNode> file_node_ptr;

    /// @var `aliases`
    /// @brief All the aliases within this file
    std::unordered_map<std::string, ImportNode *> aliases;

    /// @var `file`
    /// @brief The path to the file to parse
    const std::filesystem::path file;

    /// @var `file_name`
    /// @brief The name of the file to be parsed
    const std::string file_name;

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
    /// @param `call_name` The name of the called function
    /// @param `arg_types` A list of the argument types of the function call
    /// @return `std::optional<std::pair<FunctionNode *, std::string>>` A pair of the function node and its file, or nullopt if no match
    /// could be found
    ///
    /// @attention Asserts that the parameter `call_node` is not a nullptr
    static std::optional<std::pair<FunctionNode *, std::string>> get_function_from_call( //
        const std::string &call_name,                                                    //
        const std::vector<std::shared_ptr<Type>> &arg_types                              //
    );

    /// @function `add_open_function`
    /// @brief Adds a open function to the list of all open functions
    ///
    /// @param `open_function` A rvalue reference to the OpenFunction to add to the list
    ///
    /// @attention This function takes ownership of the `open_function` parameter
    void add_open_function(std::pair<FunctionNode *, std::vector<Line>> &&open_function) {
        open_functions_list.push_back(std::move(open_function));
    }

    /// @function `get_next_open_function`
    /// @brief Returns the next open function to parse
    ///
    /// @return `std::optional<std::pair<FunctionNode *, std::vector<Line>>>` The next Open Function to parse. Returns a nullopt if there
    /// are no open functions left
    std::optional<std::pair<FunctionNode *, std::vector<Line>>> get_next_open_function() {
        if (open_functions_list.empty()) {
            return std::nullopt;
        }
        std::pair<FunctionNode *, std::vector<Line>> of = std::move(open_functions_list.back());
        open_functions_list.pop_back();
        return of;
    }

    /// @function `add_open_test`
    /// @brief Adds a open test to the list of all open tests
    ///
    /// @param `open_test` A rvalue reference to the OpenTest to add to the list
    ///
    /// @attention This function takes ownership of the `open_test` parameter
    void add_open_test(std::pair<TestNode *, std::vector<Line>> &&open_test) {
        open_tests_list.push_back(std::move(open_test));
    }

    /// @function `get_next_open_test`
    /// @brief Returns the next open test to parse
    ///
    /// @return `std::optional<std::pair<TestNode *, std::vector<Line>>>` The next Open Test to parse. Returns a nullopt if there are no
    /// open tests left
    std::optional<std::pair<TestNode *, std::vector<Line>>> get_next_open_test() {
        if (open_tests_list.empty()) {
            return std::nullopt;
        }
        std::pair<TestNode *, std::vector<Line>> ot = std::move(open_tests_list.back());
        open_tests_list.pop_back();
        return ot;
    }

    /// @function `remove_trailing_garbage`
    /// @brief Removes all trailing garbage, such as indentation, eol characters or semicolons from the token slice
    ///
    /// @param `tokens` The tokens in which to remove all trailing garbage
    static void remove_trailing_garbage(token_slice &tokens) {
        for (; tokens.second != tokens.first;) {
            // The tokens.second is the non-inclusive iterator, so we dont check it directly but only what comes directly infront of it
            Token tp = std::prev(tokens.second)->token;
            if (tp == TOK_INDENT || tp == TOK_EOL || tp == TOK_SEMICOLON || tp == TOK_COLON) {
                tokens.second--;
            } else {
                break;
            }
        }
    }

    /// @function `remove_surrounding_paren`
    /// @brief Removes surrounding parenthesis of the given token list if they are at the begin and the end
    ///
    /// @param `tokens` The tokens in which to remove the surrounding parens, if they exist
    static void remove_surrounding_paren(token_slice &tokens) {
        // Us the Matcher to check if the first and last paren are not parens by accident, for example in '(1 + 2) * (3 + 4)', because if we
        // would not check for that the expression would become '1 + 2) * (3 + 4' and that is definitely wrong
        if (tokens.first->token == TOK_LEFT_PAREN && Matcher::tokens_match({tokens.first + 1, tokens.second}, Matcher::until_right_paren)) {
            tokens.first++;
            tokens.second--;
        }
    }

    /// @function `get_slice_size`
    /// @brief Returns the size of the given token slice
    ///
    /// @param `tokens` The token slice to get the length for
    /// @return `size_t` The size of the slice
    static inline size_t get_slice_size(const token_slice &tokens) {
        return std::distance(tokens.first, tokens.second);
    }

    /// @function `check_type_aliasing`
    /// @brief Checks if the type is aliased, if it is it checks if in the given file the given type does even exist
    ///
    /// @param `tokens` The type token slice to check
    /// @return `bool` Whether something failed
    bool check_type_aliasing(token_slice &tokens) {
        if (std::distance(tokens.first, tokens.second) > 2 && tokens.first->token == TOK_IDENTIFIER &&
            std::next(tokens.first)->token == TOK_DOT) {
            const std::string alias = tokens.first->lexme;
            // Check if this file even has the given alias
            if (aliases.find(alias) == aliases.end()) {
                THROW_ERR(ErrAliasNotFound, ERR_PARSING, file_name, tokens.first->line, tokens.first->column, alias);
                return false;
            }
            const std::string type_name = (tokens.first + 2)->lexme;
            for (const auto &import : imported_files) {
                if (import->alias.has_value() && import->alias.value() == alias &&                           //
                    std::holds_alternative<std::pair<std::optional<std::string>, std::string>>(import->path) //
                ) {
                    const auto &path_pair = std::get<std::pair<std::optional<std::string>, std::string>>(import->path);
                    const FileNode *file_node = Resolver::file_map.at(path_pair.second);
                    for (const auto &definition : file_node->definitions) {
                        if (const DataNode *data_node = dynamic_cast<const DataNode *>(definition.get())) {
                            if (data_node->name == type_name) {
                                tokens.first += 2;
                                return true;
                            }
                        } else if (const EnumNode *enum_node = dynamic_cast<const EnumNode *>(definition.get())) {
                            if (enum_node->name == type_name) {
                                tokens.first += 2;
                                return true;
                            }
                        }
                    }
                    // The given type definition should have been in this file import from the alias, but it has not been found here
                    THROW_ERR(                                                                         //
                        ErrDefNotFoundInAliasedFile, ERR_PARSING, file_name, (tokens.first + 2)->line, //
                        (tokens.first + 2)->column, alias, path_pair.second, type_name                 //
                    );
                    return false;
                }
            }
            // It should never come here, it must have returned earlier, otherwise there is a bug somewhere
            assert(false);
        }
        return true;
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
    /// @param `souce` A reference to the source token vector directly to enable direct modification
    /// @return `bool` Whether the next main node was added correctly. Returns false if there was an error
    bool add_next_main_node(FileNode &file_node, token_slice &tokens, token_list &source);

    /// @function `get_definition_tokens`
    /// @brief Extracts all the tokens which are part of the definition
    ///
    /// @return `token_list` Returns the extracted tokens, being part of the definition
    ///
    /// @attention Deletes all tokens which are part of the definition from the given tokens list
    token_slice get_definition_tokens(const token_slice &tokens);

    /// @function `get_body_lines`
    /// @brief Extracts all the body lines based on their indentation. Returns a list of lines, where the tokens have been refined already,
    /// meaning that all tokens forming a type have been mergeed into a type token, and all tabs within a given line have been removed,
    /// leaving only the pure line tokens themselves.
    ///
    /// @param `definition_indentation` The indentation level of the definition. The body of the definition will have at least one
    /// indentation level more
    /// @param `tokens` The tokens from which the body will be extracted from
    /// @return `std::vector<Line>` The extracted next body lines
    ///
    /// @attention This function modifies the given `tokens` list, the retured tokens are no longer part of the given list
    std::vector<Line> get_body_lines(                     //
        unsigned int definition_indentation,              //
        token_slice &tokens,                              //
        std::optional<token_list *> source = std::nullopt //
    );

    /// @function `create_call_or_initializer_base`
    /// @brief Creates the base node for all calls or initializers
    ///
    /// @details This function is part of the `Util` class, as both call statements as well as call expressions use this function
    ///
    /// @param `scope` The scope the call happens in
    /// @param `tokens` The tokens which will be interpreted as call
    /// @param `alias_base` The alias base of the call, if there is any
    /// @return A optional value containing a tuple, where:
    ///     - the first value is the function or initializers name
    ///     - the second value is a list of all expressions (the argument expression) of the call / initializier and if the arg is expected
    ///     to be a reference
    ///     - the third value is the call's return type, or the initializers type
    ///     - the forth value is: true if the expression is a Data initializer, false if an entity initializer, nullopt if its a call
    ///     - the fifth value tells whether the call can throw an error
    std::optional<std::tuple<                                          //
        std::string,                                                   // name
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>>, // args
        std::shared_ptr<Type>,                                         // type
        std::optional<bool>,                                           // is data (true), entity (false) or call (nullopt)
        bool                                                           // can_throw
        >>
    create_call_or_initializer_base(std::shared_ptr<Scope> scope, const token_slice &tokens, const std::optional<std::string> &alias_base);

    /// @function `create_unary_op_base`
    /// @brief Creates a UnaryOpBase from the given tokens
    ///
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return A optional value containing a tuple, where the first value is the operator token, the second value is the expression on
    /// which the unary operation is applied on and the third value is whether the unary operator is left to the expression
    std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> create_unary_op_base(std::shared_ptr<Scope> scope,
        const token_slice &tokens);

    /// @function `create_field_access_base`
    /// @brief Creates a tuple of all field access variables extracted from a field access
    ///
    /// @param `scope` The scope in which the field access is defined
    /// @param `tokens` The list of tokens representing the field access
    /// @return A optional value containing a tuple, where the
    ///     - first value is the type of the accessed data variable
    ///     - second value is the name of the accessed data variable
    ///     - third value is the name of the accessed field, nullopt if tis a $N access
    ///     - fourth value is the id of the field
    ///     - fifth value is the type of the field
    std::optional<std::tuple<std::shared_ptr<Type>, std::string, std::optional<std::string>, unsigned int, std::shared_ptr<Type>>>
    create_field_access_base(         //
        std::shared_ptr<Scope> scope, //
        token_slice &tokens,          //
        const bool is_type_access     //
    );

    /// @function `create_multi_type_access`
    /// @brief Checks the multi-typed acccess and returns the index as well as the field name, where the returned field name is always in
    /// the form of $N instead of the x, y, z or r, g, b, a form thats possible
    ///
    /// @param `multi_type` The multi-type the field access is based on
    /// @param `field_name` The field name of the access
    /// @return `std::optional<std::tuple<std::string, unsigned int>>` A tuple of the "new" access name and the access id, nullopt if failed
    std::optional<std::tuple<std::string, unsigned int>> create_multi_type_access( //
        const MultiType *multi_type,                                               //
        const std::string &field_name                                              //
    );

    /// @function `create_grouped_access_base`
    /// @brief Creates a tuple for all group access variables extracted from a grouped variable access
    ///
    /// @param `scope` The scope in which the grouped access is defined
    /// @param `tokens` The list of tokens representing the grouped access
    /// @return A optional value containing a tuple, where the
    ///     - first value is the type of the accessed data variable
    ///     - second value is the name of the accessed data variable
    ///     - third value is the list of accessed field names
    ///     - fourth value is the list of accessed field ids
    ///     - fifth value is the list of accessed field types
    std::optional<std::tuple<std::shared_ptr<Type>, std::string, std::vector<std::string>, std::vector<unsigned int>,
        std::vector<std::shared_ptr<Type>>>>
    create_grouped_access_base(       //
        std::shared_ptr<Scope> scope, //
        token_slice &tokens           //
    );

    /**************************************************************************************************************************************
     * @region `Util` END
     *************************************************************************************************************************************/

    /**************************************************************************************************************************************
     * @region `Expression`
     * @brief This region is responsible for parsing everything about expressions
     *************************************************************************************************************************************/

    /// @function `check_castability`
    /// @brief Checks if one of the two types can be implicitely cast to the other type. Returns the directionality of the cast
    ///
    /// @param `lhs` The lhs type to check
    /// @param `rhs` The rhs type to check
    /// @return `std::optional<bool>` nullopt if not castable, 'true' if rhs -> lhs, 'false' if lhs -> rhs
    std::optional<bool> check_castability(const std::shared_ptr<Type> &lhs_type, const std::shared_ptr<Type> &rhs_type);

    /// @function `check_castability`
    /// @brief Checks if one of the two expression can be implicitely cast to the other expression. If yes, it wraps the expression in a
    /// type cast
    ///
    /// @param `lhs` The lhs of which to check the type and cast if needed
    /// @param `rhs` The rhs of which to check the type and cast if needed
    /// @return `bool` Whether the casting was sucessful
    ///
    /// @attention Modifies the `lhs` or `rhs` expressions, depending on castablity, or throws an error if its not castable
    bool check_castability(std::unique_ptr<ExpressionNode> &lhs, std::unique_ptr<ExpressionNode> &rhs);

    /// @function `check_const_folding`
    /// @brief Checks if the lhs and rhs of a binary operation are able to be constant folded, if they can it returns the result of the
    /// folding
    ///
    /// @param `lhs` The lhs of the binary expression
    /// @param `operation` The operation which is applied to the lhs and rhs
    /// @param `rhs` The rhs of the binary expression
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` The result of te const folding, nullopt if const folding is not applicable
    static std::optional<std::unique_ptr<ExpressionNode>> check_const_folding( //
        std::unique_ptr<ExpressionNode> &lhs,                                  //
        const Token operation,                                                 //
        std::unique_ptr<ExpressionNode> &rhs                                   //
    );

    /// @function `add_literals`
    /// @brief Adds two literal nodes together and returns an the result wrapped in an optional. If the literals could not be added, nullopt
    /// is returned
    ///
    /// @param `lhs` The left hand side literal
    /// @param `operation` The operation to apply
    /// @param `rhs` The right hand side literal
    /// @return `std::optional<std::unique_ptr<LiteralNode>>` The resulting literal, nullopt if the literals could not be added up
    static std::optional<std::unique_ptr<LiteralNode>> add_literals( //
        const LiteralNode *lhs,                                      //
        const Token operation,                                       //
        const LiteralNode *rhs                                       //
    );

    /// @function `create_variable`
    /// @brief Creates a VariableNode from the given tokens
    ///
    /// @param `scope` The scope in which the variable is defined
    /// @param `tokens` The list of tokens representing the variable
    /// @return `std::optional<VariableNode>` An optional VariableNode if creation is successful, nullopt otherwise
    std::optional<VariableNode> create_variable(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_unary_op_expression`
    /// @brief Creates a UnaryOpExpression from the given tokens
    ///
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return `std::optional<UnaryOpExpression>` An optional UnaryOpExpression if creation is successful, nullopt otherwise
    std::optional<UnaryOpExpression> create_unary_op_expression(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_literal`
    /// @brief Creates a LiteralNode from the given tokens
    ///
    /// @param `tokens` The list of tokens representing the literal
    /// @return `std::optional<LiteralNode>` An optional LiteralNode if creation is successful, nullopt otherwise
    std::optional<LiteralNode> create_literal(const token_slice &tokens);

    /// @functon `create_string_interpolation`
    /// @brief Creates a StringInterpolationNode from the given tokens
    ///
    /// @param `scope` The scope the string interpolation takes place in
    /// @param `interpol_string` The string from which the interpolation is created
    /// @retun `std::optional<StringInterpolationNode>` An optional StringInterpolationNode if creation was successful, nullopt otherwise
    std::optional<StringInterpolationNode> create_string_interpolation(std::shared_ptr<Scope> scope, const std::string &interpol_string);

    /// @function `create_call_expression`
    /// @brief Creates a CallNodeExpression from the given tokens
    ///
    /// @param `scope` The scope in which the call expression is defined
    /// @param `tokens` The list of tokens representing the call expression
    /// @return `std::optional<std::unique_ptr<CallNodeExpression>>` A unique pointer to the created CallNodeExpression
    std::optional<std::unique_ptr<ExpressionNode>> create_call_expression( //
        std::shared_ptr<Scope> scope,                                      //
        const token_slice &tokens,                                         //
        const std::optional<std::string> &alias_base                       //
    );

    /// @function `create_initializer`
    /// @brief Creates a InitializerNode from the given tokens
    ///
    /// @param `scope` The scope in which the initializer is defined
    /// @param `tokens` The list of tokens representing the initializer
    /// @return `std::optional<std::unique_ptr<InitializerNode>>` A unique pointer to the created InitializerNod3
    std::optional<std::unique_ptr<ExpressionNode>> create_initializer( //
        std::shared_ptr<Scope> scope,                                  //
        const token_slice &tokens,                                     //
        const std::optional<std::string> &alias_base                   //
    );

    /// @function `create_type_cast`
    /// @brief Creates a TypeCastNode from the given tokens, if the expression already is the wanted type it returns the expression directly
    ///
    /// @param `scope` The scope in which the type cast is defined
    /// @param `tokens` The list of tokens representing the type cast
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An ExpressionNode if creation is successful, nullopt otherwise
    std::optional<std::unique_ptr<ExpressionNode>> create_type_cast(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_group_expression`
    /// @brief Creates a GroupExpressionNode from the given tokens
    ///
    /// @param `scope` The scope in which the grouped expression is defined
    /// @param `tokens` The list of tokens representing the type cast
    /// @return `std::optional<GroupExpressionNode>` An optional grouped expression, nullopt if its creation failed
    std::optional<GroupExpressionNode> create_group_expression(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_group_expressions`
    /// @brief Creates a bunch of comma-separated group expressions
    ///
    /// @param `scope` The scope in which the expressions are defined
    /// @param `tokens` The list of tokens representing all expressions separated by commas
    /// @return `std::optional<std::vector<std::unique_ptr<ExpressionNode>>>` The list of expressions in the group
    std::optional<std::vector<std::unique_ptr<ExpressionNode>>> create_group_expressions(std::shared_ptr<Scope> scope, token_slice &tokens);

    /// @function `create_data_access`
    /// @brief Creates a DataAccessNode from the given tokens
    ///
    /// @param `scope` The scope in which the data access is defined
    /// @param `tokens` The list of tokens representing the data access
    /// @param `is_type_access` Whether the "data" access is actually a type access, like an enum value access
    /// @return `std::optional<DataAccessNode>` An optional data access node, nullopt if its creation failed
    std::optional<DataAccessNode> create_data_access(std::shared_ptr<Scope> scope, const token_slice &tokens, const bool is_type_access);

    /// @function `create_grouped_data_access`
    /// @brief Creates a GroupedDataAccessNode from the given tokens
    ///
    /// @param `scope` The scope in which the data access is defined
    /// @param `tokens` The list of tokens representing the data access
    /// @return `std::optional<GroupedDataAccessNode>` A grouped data access node, nullopt if its creation failed
    std::optional<GroupedDataAccessNode> create_grouped_data_access(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_array_initializer`
    /// @brief Creates an ArrayInitializerNode from the given tokens
    ///
    /// @param `scope` The scope in which the array initializer is defined
    /// @param `tokens` The list of tokens representing the array initializer
    /// @return `std::optional<ArrayInitializerNode>` An array initializer node, nullopt if its creation failed
    std::optional<ArrayInitializerNode> create_array_initializer(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_array_access`
    /// @brief Creates an ArrayAccessNode from the given tokens
    ///
    /// @param `scope` The scope in which the array initializer is defined
    /// @param `tokens` The list of tokens representing the array access
    /// @return `std::optional<ArrayAccessNode>` An array access node, nullopt if its creation failed
    std::optional<ArrayAccessNode> create_array_access(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_stacked_expression`
    /// @brief Creates a stacked expression from the given tokens
    ///
    /// @param `scope` The scope in which the scoped expression is defined
    /// @param `tokens` The list of tokens representing the scoped expression
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` The stacked expression, nullopt if its creation failed
    ///
    /// @details Stacked expressions are unwrapped from the right to the left: `instance.field.field.field` becomes
    /// `(instance.field.field).field` and then it becomes `((instance.field).field).field`, so they are evalueated balanced from the right
    std::optional<std::unique_ptr<ExpressionNode>> create_stacked_expression(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_pivot_expression`
    /// @brief Creates a expression based on token precedences, where the token with the highest precedence is the "pivot point" of the
    /// epxression creation
    ///
    /// @param `scope` The scope in which the expression is defined
    /// @param `tokens` The list of tokens representing the expression
    /// @param `expected_type` The expected type of the expression. Needed for the cration of default nodes
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An optional unique pointer to the created ExpressionNode
    std::optional<std::unique_ptr<ExpressionNode>> create_pivot_expression(      //
        std::shared_ptr<Scope> scope,                                            //
        const token_slice &tokens,                                               //
        const std::optional<std::shared_ptr<Type>> &expected_type = std::nullopt //
    );

    /// @function `create_expression`
    /// @brief Creates an ExpressionNode from the given tokens
    ///
    /// @param `scope` The scope in which the expression is defined
    /// @param `tokens` The list of tokens representing the expression
    /// @param `expected_type` The expected type of the expression. If possible, applies implicit type conversion to get this type
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An optional unique pointer to the created ExpressionNode
    std::optional<std::unique_ptr<ExpressionNode>> create_expression(            //
        std::shared_ptr<Scope> scope,                                            //
        const token_slice &tokens,                                               //
        const std::optional<std::shared_ptr<Type>> &expected_type = std::nullopt //
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
    /// @param `alias_base` The alias base of the call, if any
    /// @return `std::optional<std::unique_ptr<CallNodeStatement>>` A unique pointer to the created CallNodeStatement
    std::optional<std::unique_ptr<CallNodeStatement>> create_call_statement( //
        std::shared_ptr<Scope> scope,                                        //
        const token_slice &tokens,                                           //
        const std::optional<std::string> &alias_base                         //
    );

    /// @function `create_throw`
    /// @brief Creates a ThrowNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the throw statement is defined
    /// @param `tokens` The list of tokens representing the throw statement
    /// @return `std::optional<ThrowNode>` An optional ThrowNode if creation is successful, nullopt otherwise
    std::optional<ThrowNode> create_throw(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_return`
    /// @brief Creates a ReturnNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the return statement is defined
    /// @param `tokens` The list of tokens representing the return statement
    /// @return `std::optional<ReturnNode>` An optional ReturnNode if creation is successful, nullopt otherwise
    std::optional<ReturnNode> create_return(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_if`
    /// @brief Creates an IfNode from the given if chain
    ///
    /// @param `scope` The scope in which the if statement is defined
    /// @param `if_chain` The list of token pairs representing the if statement chain
    /// @return `std::optional<std::unique_ptr<IfNode>>` An optional unique pointer to the created IfNode
    std::optional<std::unique_ptr<IfNode>> create_if(std::shared_ptr<Scope> scope,
        std::vector<std::pair<token_slice, std::vector<Line>>> &if_chain);

    /// @function `create_while_loop`
    /// @brief Creates a WhileNode from the given definition and body tokens inside the given scope
    ///
    /// @param `scope` The scope in which the while loop is defined
    /// @param `definition` The list of tokens representing the while loop definition
    /// @param `body` The list of tokens representing the while loop body
    /// @return `std::optional<std::unique_ptr<WhileNode>>` An optional unique pointer to the created WhileNode
    std::optional<std::unique_ptr<WhileNode>> create_while_loop(std::shared_ptr<Scope> scope, const token_slice &definition,
        const std::vector<Line> &body);

    /// @function `create_for_loop`
    /// @brief Creates a ForLoopNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the for loop is defined
    /// @param `definition` The list of tokens representing the for loop definition
    /// @param `body` The list of tokens representing the for loop body
    /// @return `std::optional<std::unique_ptr<ForLoopNode>>` An optional unique pointer to the created ForLoopNode
    std::optional<std::unique_ptr<ForLoopNode>> create_for_loop(std::shared_ptr<Scope> scope, const token_slice &definition,
        const std::vector<Line> &body);

    /// @function `create_enh_for_loop`
    /// @brief Creates an enhanced ForLoopNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the enhanced for loop is defined
    /// @param `definition` The list of tokens representing the enhanced for loop definition
    /// @param `body` The list of tokens representing the enhanced for loop body
    /// @return `std::optional<std::unique_ptr<EnhForLoopNode>>` An optional unique pointer to the created enhanced ForLoopNode
    std::optional<std::unique_ptr<EnhForLoopNode>> create_enh_for_loop( //
        std::shared_ptr<Scope> scope,                                   //
        const token_slice &definition,                                  //
        const std::vector<Line> &body                                   //
    );

    /// @function `create_switch_branch_body`
    /// @brief Creates the body of a single switch branch and then creates the whole branch and adds it to the list of s or e branches
    ///
    /// @param `scope` The scope in which the switch statement / expression is defined
    /// @param `match_expressions` The list of the match expressions which, when matched, this branch will be executed
    /// @param `s_branches` The list of all statement branches
    /// @param `e_branches` The list of all expression branches
    /// @param `line_it` The current iterator of the line inside the switch's body
    /// @param `body` The body of the whole switch
    /// @param `tokens` The token slice representing the whole "definition" of this branch (the line which contains the : or -> symbol)
    /// @param `match_range` The range the matching expression(s) range [from, to) within the `tokens` slice
    /// @param `is_statement` Whether the created switch is an expression or a statement
    /// @return `bool` Whether the creation of the switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_switch_branch_body(                                      //
        std::shared_ptr<Scope> scope,                                    //
        std::vector<std::unique_ptr<ExpressionNode>> &match_expressions, //
        std::vector<SSwitchBranch> &s_branches,                          //
        std::vector<ESwitchBranch> &e_branches,                          //
        std::vector<Line>::const_iterator &line_it,                      //
        const std::vector<Line> &body,                                   //
        const token_slice &tokens,                                       //
        const uint2 &match_range,                                        //
        const bool is_statement                                          //
    );

    /// @function `create_switch_branches`
    /// @brief Creates the branches of a general switch, e.g. a switch where the switched-on values can be expressions (like integer types)
    ///
    /// @param `scope` The scope in which the switch statement / expression is defined
    /// @param `s_branches` The list of all statement branches
    /// @param `e_branches` The list of all expression branches
    /// @param `body` The body of the whole switch
    /// @param `switcher_type` The type of the expression which is switched on
    /// @param `is_statement` Whether the created switch is an expression or a statement
    /// @return `bool` Whether the creation of the switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_switch_branches(                    //
        std::shared_ptr<Scope> scope,               //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const bool is_statement                     //
    );

    /// @function `create_enum_switch_branches`
    /// @brief Creates the branches for the enum switch and adds them to the `s_branches` or `e_branches`, depending on whether it's a
    /// statement or an expression
    ///
    /// @param `scope` The scope in which the switch statement / expression switching on the enum value is defined
    /// @param `s_branches` The list of all statement branches
    /// @param `e_branches` The list of all expression branches
    /// @param `body` The body of the whole switch
    /// @param `switcher_type` The type of the expression which is switched upon
    /// @param `enum_node` The pointer to the enum node definition which is switched on
    /// @param `is_statement` Whether the created switch is an expression or a statement
    /// @return `bool` Whether the creation of the enum switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_enum_switch_branches(               //
        std::shared_ptr<Scope> scope,               //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const EnumNode *enum_node,                  //
        const bool is_statement                     //
    );

    /// @function `create_optional_switch_branches`
    /// @brief Creates the branches for the optional switch and adds them to the `s_branches` or `e_branches`, depending on whether it's a
    /// statement or an expression
    ///
    /// @param `scope` The scope in which the switch statement / expression switching on the optional value is defined
    /// @param `s_branches` The list of all statement branches
    /// @param `e_branches` The list of all expression branches
    /// @param `body` The body of the whole switch
    /// @param `switcher_type` The type of the expression which is switched upon
    /// @param `is_statement` Whether the created switch is an expression or a statement
    /// @return `bool` Whether the creation of the enum switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_optional_switch_branches(           //
        std::shared_ptr<Scope> scope,               //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const bool is_statement                     //
    );

    /// @function `create_variant_switch_branches`
    /// @brief Creates the branches for the variant switch and adds them to the `s_branches` or `e_branches`, depending on whether it's a
    /// statement or an expression
    ///
    /// @param `scope` The scope in which the switch statement / expression switching on the variant value is defined
    /// @param `s_branches` The list of all statement branches
    /// @param `e_branches` The list of all expression branches
    /// @param `body` The body of the whole switch
    /// @param `switcher_type` The type of the expression which is switched upon
    /// @param `is_statement` Whether the created switch is an expression or a statement
    /// @return `bool` Whether the creation of the variant switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_variant_switch_branches(            //
        std::shared_ptr<Scope> scope,               //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const bool is_statement                     //
    );

    /// @function `create_switch_statement`
    /// @brief Creates an switch statement from the given list of tokens
    ///
    /// @param `scope` The scope in which the switch statement is defined
    /// @param `definition` The list of tokens representing the switch statements definition
    /// @param `body` The list of lines representing the switch statements entire body
    /// @return `std::optional<std::unique_ptr<StatementNode>>` An optional unique pointer to the created statement
    std::optional<std::unique_ptr<StatementNode>> create_switch_statement( //
        std::shared_ptr<Scope> scope,                                      //
        const token_slice &definition,                                     //
        const std::vector<Line> &body                                      //
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
        std::shared_ptr<Scope> scope,                           //
        const token_slice &definition,                          //
        const std::vector<Line> &body,                          //
        std::vector<std::unique_ptr<StatementNode>> &statements //
    );

    /// @function `create_group_assignment`
    /// @brief Creates an GroupAssignmentNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the assignment is defined
    /// @param `tokens` The list of tokens representing the assignment
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<std::unique_ptr<GroupAssignmentNode>>` An optional unique pointer to the created GroupAssignmentNode
    std::optional<GroupAssignmentNode> create_group_assignment( //
        std::shared_ptr<Scope> scope,                           //
        const token_slice &tokens,                              //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs     //
    );

    /// @function `create_group_assignment_shorthand`
    /// @brief Creates an GroupAssignmentNode from the given list of tokens where the group assignment itself is a shorthand
    ///
    /// @param `scope` The scope in which the assignment is defined
    /// @param `tokens` The list of tokens representing the assignment
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<std::unique_ptr<GroupAssignmentNode>>` An optional unique pointer to the created GroupAssignmentNode
    std::optional<GroupAssignmentNode> create_group_assignment_shorthand( //
        std::shared_ptr<Scope> scope,                                     //
        const token_slice &tokens,                                        //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs               //
    );

    /// @function `create_assignment`
    /// @brief Creates an AssignmentNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the assignment is defined
    /// @param `tokens` The list of tokens representing the assignment
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<std::unique_ptr<AssignmentNode>>` An optional unique pointer to the created AssignmentNode
    std::optional<AssignmentNode> create_assignment(        //
        std::shared_ptr<Scope> scope,                       //
        const token_slice &tokens,                          //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs //
    );

    /// @function `create_assignment_shorthand`
    /// @brief Creates an AssignmentShorthandNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the assignment shorthand is defined
    /// @param `tokens` The list of tokens contiaining the assignment shorthand
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<AssignmentNode>` The created AssignmentNode, nullopt if creation failed
    std::optional<AssignmentNode> create_assignment_shorthand( //
        std::shared_ptr<Scope> scope,                          //
        const token_slice &tokens,                             //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs    //
    );

    /// @function `create_group_declaration`
    /// @brief Creates a GroupDeclarationNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the group declaration is defined
    /// @param `tokens` The list of tokens representing the group declaration
    /// @param `rhs` The rhs of the declaration, which possibly is already parsed
    /// @return `std::optional<GroupDeclarationNode>` An optional GroupDeclarationNode, if creation was sucessfull
    ///
    /// @note A group declaration is _always_ inferred and cannot be not inferred
    std::optional<GroupDeclarationNode> create_group_declaration( //
        std::shared_ptr<Scope> scope,                             //
        const token_slice &tokens,                                //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs       //
    );

    /// @function `create_declaration`
    /// @brief Creates a DeclarationNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the declaration is defined
    /// @param `tokens` The list of tokens representing the declaration
    /// @param `is_inferred` Determines whether the type of the declared variable is inferred
    /// @param `has_rhs` Determines whether the declaration even has a rhs
    /// @param `rhs` The rhs of the declaration, which possibly is already parsed
    /// @return `std::optional<DeclarationNode>` An optional DeclarationNode, if creation was sucessfull
    std::optional<DeclarationNode> create_declaration(      //
        std::shared_ptr<Scope> scope,                       //
        const token_slice &tokens,                          //
        const bool is_inferred,                             //
        const bool has_rhs,                                 //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs //
    );

    /// @function `create_unary_op_statement`
    /// @brief Creates a UnaryOpStatement from the given tokens
    ///
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return `std::optional<UnaryOpStatement>` An optional UnaryOpStatement if creation is successful, nullopt otherwise
    std::optional<UnaryOpStatement> create_unary_op_statement(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_data_field_assignment`
    /// @brief Creates a DataFieldAssignmentNode from the given tokens
    ///
    /// @param `scope` The scope in which the data field assignment is defined
    /// @param `tokens` The list of tokens representing the data field assignment node
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<DataFieldAssignmentNode>` The created DataFieldAssignmentNode, nullopt if its creation failed
    std::optional<DataFieldAssignmentNode> create_data_field_assignment( //
        std::shared_ptr<Scope> scope,                                    //
        const token_slice &tokens,                                       //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs              //
    );

    /// @function `create_grouped_data_field_assignment`
    /// @brief Creates a GroupedDataFieldAssignmentNode from the given tokens
    ///
    /// @param `scope` The scope in which the grouped data field assignment is defined
    /// @param `tokens` The list of tokens representing the grouped data field assignment
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<GroupedDataFieldAssignmentNode>` The created GroupedDataFieldAssignmentNode, nullopt if its creation failed
    std::optional<GroupedDataFieldAssignmentNode> create_grouped_data_field_assignment( //
        std::shared_ptr<Scope> scope,                                                   //
        const token_slice &tokens,                                                      //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs                             //
    );

    /// @function `create_grouped_data_field_assignment_shorthand`
    /// @brief Creates a GroupedDataFieldAssignmentNode from the given tokens
    ///
    /// @param `scope` The scope in which the grouped data field assignment is defined
    /// @param `tokens` The list of tokens representing the grouped data field assignment
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<GroupedDataFieldAssignmentNode>` The created GroupedDataFieldAssignmentNode, nullopt if its creation failed
    std::optional<GroupedDataFieldAssignmentNode> create_grouped_data_field_assignment_shorthand( //
        std::shared_ptr<Scope> scope,                                                             //
        const token_slice &tokens,                                                                //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs                                       //
    );

    /// @function `create_array_assignment`
    /// @brief Creates an ArrayAssignmentNode from the given tokens
    ///
    /// @param `scope` The scope in which the grouped array assignment is defined
    /// @param `tokens` The list of tokens representing the array assignment
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<ArrayAssignmentNode>` The created ArrayAssignmentNode, nullopt if its creation failed
    std::optional<ArrayAssignmentNode> create_array_assignment( //
        std::shared_ptr<Scope> scope,                           //
        const token_slice &tokens,                              //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs     //
    );

    /// @function `create_stacked_statement`
    /// @brief Creates a stacked statement, like `a.b.c = sdf` for example
    ///
    /// @param `scope` The scope in which the stacked statement is defined
    /// @param `tokens` The list of tokens representing the stacked statement
    /// @return `std::optional<std::unique_ptr<StatementNode>>` The created stacked statement
    std::optional<std::unique_ptr<StatementNode>> create_stacked_statement(std::shared_ptr<Scope> scope, const token_slice &tokens);

    /// @function `create_statement`
    /// @brief Creates a StatementNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the statement is defined
    /// @param `tokens` The list of tokens representing the statement
    /// @param `rhs` The rhs of the statement, which possibly is already parsed
    /// @return `std::optional<std::unique_ptr<StatementNode>>` An optional unique pointer to the created StatementNode
    ///
    /// @note This function dispatches to other functions to create specific statement nodes based on the signatures. It also handles
    /// parsing errors and returns nullopt if the statement cannot be parsed.
    std::optional<std::unique_ptr<StatementNode>> create_statement(       //
        std::shared_ptr<Scope> scope,                                     //
        const token_slice &tokens,                                        //
        std::optional<std::unique_ptr<ExpressionNode>> rhs = std::nullopt //
    );

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
        std::shared_ptr<Scope> scope,                                      //
        std::vector<Line>::const_iterator &line_it,                        //
        const std::vector<Line> &body,                                     //
        std::vector<std::unique_ptr<StatementNode>> &statements            //
    );

    /// @function `create_body`
    /// @brief Creates a body containing of multiple statement nodes from a list of tokens
    ///
    /// @param `scope` The scope of the body to generate. All generated body statements will be added to this scope
    /// @param `body` The token list containing all the body tokens
    /// @return `std::optional<std::vectro<std::unique_ptr<StatementNode>>>` The list of StatementNodes parsed from the body tokens.
    std::optional<std::vector<std::unique_ptr<StatementNode>>> create_body(std::shared_ptr<Scope> scope, const std::vector<Line> &body);

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
    std::optional<FunctionNode> create_function(const token_slice &definition);

    /// @function `create_data`
    /// @brief Creates a DataNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the data definition
    /// @param `body` The list of tokens representing the data body
    /// @return `std::optional<DataNode>` The created DataNode, nullopt if creation failed
    std::optional<DataNode> create_data(const token_slice &definition, const std::vector<Line> &body);

    /// @function `create_func`
    /// @brief Creates a FuncNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the function definition
    /// @param `body` The list of tokens representing the function body
    /// @return `std::optional<FuncNode>` The created FuncNode or nullopt if creation failed
    ///
    /// @note The FuncNode's body is only allowed to house function definitions, and each function has a body respectively
    std::optional<FuncNode> create_func(const token_slice &definition, const std::vector<Line> &body);

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
    create_entity_type create_entity(const token_slice &definition, const std::vector<Line> &body);

    /// @function `create_links`
    /// @brief Creates a list of LinkNode's from a given body containing those links
    ///
    /// @param `body` The list of tokens forming the body containing all the link statements
    /// @return `std::vector<std::unique_ptr<LinkNode>>` A vector of created LinkNode
    std::vector<std::unique_ptr<LinkNode>> create_links(const std::vector<Line> &body);

    /// @function `create_link`
    /// @brief Creates a LinkNode from the given list of tokens
    ///
    /// @param `tokens` The list of tokens representing the link
    /// @return `LinkNode` The created LinkNode
    LinkNode create_link(const token_slice &tokens);

    /// @function `create_enum`
    /// @brief Creates an EnumNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the enum definition
    /// @param `body` The list of tokens representing the enum body
    /// @return `std::optional<EnumNode>` The created EnumNode, nullopt if creation failed
    std::optional<EnumNode> create_enum(const token_slice &definition, const std::vector<Line> &body);

    /// @function `create_error`
    /// @brief Creates an ErrorNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the error definition
    /// @param `body` The list of tokens representing the error body
    /// @return `ErrorNode` The created ErrorNode
    ErrorNode create_error(const token_slice &definition, const std::vector<Line> &body);

    /// @function `create_variant`
    /// @brief Creates a VariantNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the variant definition
    /// @param `body` The list of tokens representing the variant body
    /// @return `std::optional<VariantNode>` The created VariantNode, nullopt if it's creation failed
    std::optional<VariantNode> create_variant(const token_slice &definition, const std::vector<Line> &body);

    /// @function `create_test`
    /// @brief Creates a TestNode from the given definition and body tokens
    ///
    /// @param `definition` The list of tokens representing the test definition
    /// @return `std::optional<TestNode>` The created TestNode, if creation was successful, nullopt otherwise
    std::optional<TestNode> create_test(const token_slice &definition);

    /// @function `create_import`
    /// @brief Creates an ImportNode from the given list of tokens
    ///
    /// @param `tokens` The list of tokens containing the import node
    /// @return `std::optional<ImportNode>` The created ImportNode, nullopt if its creation failed
    std::optional<ImportNode> create_import(const token_slice &tokens);

    /**************************************************************************************************************************************
     * @region `Definition` END
     *************************************************************************************************************************************/
};
