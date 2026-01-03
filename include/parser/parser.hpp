#pragma once

#include "matcher/matcher.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "types.hpp"

#include "ast/annotation_node.hpp"
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
#include "ast/statements/switch_statement.hpp"
#include "ast/statements/unary_op_statement.hpp"
#include "ast/statements/while_node.hpp"

#include "analyzer/analyzer.hpp"
#include "ast/expressions/array_access_node.hpp"
#include "ast/expressions/array_initializer_node.hpp"
#include "ast/expressions/data_access_node.hpp"
#include "ast/expressions/expression_node.hpp"
#include "ast/expressions/group_expression_node.hpp"
#include "ast/expressions/grouped_data_access_node.hpp"
#include "ast/expressions/literal_node.hpp"
#include "ast/expressions/optional_chain_node.hpp"
#include "ast/expressions/switch_expression.hpp"
#include "ast/expressions/unary_op_expression.hpp"
#include "ast/expressions/variable_node.hpp"
#include "ast/statements/catch_node.hpp"
#include "ast/statements/throw_node.hpp"
#include "parser/ast/expressions/variant_extraction_node.hpp"

#include "type/multi_type.hpp"

#include <atomic>
#include <cassert>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>

/// @class `Parser`
/// @brief The class which is responsible for the AST generation (parsing)
/// @note This class cannot be initialized and all functions within this class are static
class Parser {
  public:
    /// @struct `Context`
    /// @brief The context of the parsing
    struct Context {
        /// @var `level`
        /// @brief The context level the parser is currently at
        ContextLevel level;
    };

    /// @struct `CastDirection`
    /// @brief A small helper structure representing the casting direction, it makes the whole system a lot easier and more extensible. I do
    /// not think that it will ever be extended in the future, but it makes everything around castability so much more clear and more
    /// readable that I just prefer this solution annyway
    struct CastDirection {
      public:
        /// @enum `Kind`
        /// @brief The kind of the cast direction, determining the direction in which to cast
        enum class Kind {
            NOT_CASTABLE,        // Types are incpomatible
            SAME_TYPE,           // The two types are actually the exact same type, just present inside different shared pointer containers
            CAST_LHS_TO_RHS,     // Cast left operand to right's type
            CAST_RHS_TO_LHS,     // Cast right operand to left's type
            CAST_BOTH_TO_COMMON, // Cast both operands to the common type below
            CAST_BIDIRECTIONAL,  // Casting both ways is possible without a problem
        };

        /// @var `kind`
        /// @brief The kind of this cast direction
        Kind kind;

        /// @var `common_type`
        /// @brief The common type to cast to
        ///
        /// @attention This variable is **ONLY** meaningful if the Kind is `CAST_BOTH_TO_COMMON`, access to this value is UB in all other
        /// cases
        std::shared_ptr<Type> common_type;

        static CastDirection not_castable() {
            return {Kind::NOT_CASTABLE, nullptr};
        }

        static CastDirection same_type() {
            return {Kind::SAME_TYPE, nullptr};
        }

        static CastDirection lhs_to_rhs() {
            return {Kind::CAST_LHS_TO_RHS, nullptr};
        }

        static CastDirection rhs_to_lhs() {
            return {Kind::CAST_RHS_TO_LHS, nullptr};
        }

        static CastDirection both_to_common(std::shared_ptr<Type> type) {
            return {Kind::CAST_BOTH_TO_COMMON, std::move(type)};
        }

        static CastDirection bidirectional() {
            return {Kind::CAST_BIDIRECTIONAL, nullptr};
        }
    };

    /// @var `_ctx_`
    /// @brief The default context for parsing, globally constructed at compile-time to reduce LoC wastage of context creation
    static constexpr Context _ctx_{.level = ContextLevel::INTERNAL};

    /// @function `init_core_modules`
    /// @brief Initializes all the namespaces of the Core modules to prepare them to be imported
    static void init_core_modules();

    /// @function `create`
    /// @brief Creates a new instance of the Parser. The parser itself owns all instances of the parser
    ///
    /// @param `file` The path to the file to create the parser instance from
    /// @return `Parser *` A pointer to the created parser instance
    static Parser *create(const std::filesystem::path &file);

    /// @function `create`
    /// @brief Creates a new instance of the Parser. The parser itself owns all instances of the parser
    ///
    /// @param `file` The path to the file, for storage reasons
    /// @param `file_content` The already loaded content of the file
    /// @return `Parser *` A pointer to the created parser instance
    static Parser *create(const std::filesystem::path &file, const std::string &file_content);

    /// @function `parse`
    /// @brief Parses the file. It will tokenize it using the Lexer and then create the AST of the file and return the parsed FileNode
    ///
    /// @param `file` The path to the file to tokenize and parse
    /// @return `std::optional<FileNode *>` The parsed file, if it succeeded parsing
    ///
    /// @note This function creates a new Lexer class to tokenize the given file, so no further tokenization has to be made, this function
    /// takes care of the tokenization too
    std::optional<FileNode *> parse();

    /// @function `get_source_code_lines`
    /// @brief Returns the source code lines from this parser's instance
    ///
    /// @return `std::vector<std::string_view>` The source code lines
    std::vector<std::pair<unsigned int, std::string_view>> get_source_code_lines() const {
        return source_code_lines;
    }

    /// @function `check_primitive_castability`
    /// @brief Checks if one of the two types can be implicitely cast to the other type. Returns the directionality of the cast
    ///
    /// @param `lhs` The lhs type to check
    /// @param `rhs` The rhs type to check
    /// @param `is_implicit` Whether the cast is implicit or explicit
    /// @return `CastDirection` The direction in which to cast indicating if/how types can be cast
    static CastDirection check_primitive_castability( //
        const std::shared_ptr<Type> &lhs_type,        //
        const std::shared_ptr<Type> &rhs_type,        //
        const bool is_implicit = true                 //
    );

    /// @function `check_castability`
    /// @brief Checks if one of the two types can be implicitely cast to the other type. Returns the directionality of the cast
    ///
    /// @param `lhs` The lhs type to check
    /// @param `rhs` The rhs type to check
    /// @param `is_implicit` Whether the cast is implicit or explicit
    /// @return `CastDirection` The direction in which to cast indicating if/how types can be cast
    static CastDirection check_castability(    //
        const std::shared_ptr<Type> &lhs_type, //
        const std::shared_ptr<Type> &rhs_type, //
        const bool is_implicit = true          //
    );

    /// @function `is_castable_to`
    /// @brief Checks if the given type is castable 'from' the given type 'to' the given type, and whether it needs to be implicitely
    /// castable or explicitely castable
    ///
    /// @param `from` The type to cast from
    /// @param `to` The type to cast to
    /// @param `is_implicit` Whether the cast needs to be able to be done implicitely
    static bool is_castable_to(const std::shared_ptr<Type> &from, const std::shared_ptr<Type> &to, const bool is_implicit = true);

    /// @function `check_castability`
    /// @brief Checks whether the given expression can be cast to the target type and casts the expression to the type if needed. If the
    /// expression is not castable to the given type the function will return false.
    ///
    /// @param `target_type` The type to cast towards, the target type of the expression
    /// @param `expr` The expression to cast / check
    /// @param `is_implicit` Whether casting is implicit or was explicit
    ///
    /// @note If the expression already is the target type this function will leave the expression unchanged and simply return true
    bool check_castability(                       //
        const std::shared_ptr<Type> &target_type, //
        std::unique_ptr<ExpressionNode> &expr,    //
        const bool is_implicit = true             //
    );

    /// @function `resolve_all_imports`
    /// @brief Resolves all imports and puts all public symbols of imported files into the private symbol list of the file's namespace. This
    /// also checks for multiple definitions of the same symbol in multiple imported files and prints that it has defined at multiple places
    ///
    /// @return `bool` Whether everything went as expected
    static bool resolve_all_imports();

    /// @function `resolve_all_unknown_types`
    /// @brief Resolves all unknown types to point to real types
    ///
    /// @return `bool` Whether all unknown types could be resolved correctly
    ///
    /// @note Also substitutes all unknown types if the unknown type is an aliased type
    static bool resolve_all_unknown_types();

    /// @function `get_open_functions`
    /// @brief Returns all open functions whose bodies have not been parsed yet
    ///
    /// @return `std::vector<FunctionNode *>` A list of all open functions
    std::vector<FunctionNode *> get_open_functions();

    /// @function `get_all_errors`
    /// @brief Collects and returns all error types from all files
    ///
    /// @return `std::vector<const ErrorNode *>` A list of all error nodes of all files
    static std::vector<const ErrorNode *> get_all_errors();

    /// @function `get_all_data_types`
    /// @brief Collects and returns all data types from all files
    ///
    /// @¶eturn `std::vector<std::shared_ptr<Type>>` A list of all data types of all files
    static std::vector<std::shared_ptr<Type>> get_all_data_types();

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
    ///
    /// @note This will only be called when the developer wants to run the tests, e.g. make a test build. In the normal compilation
    /// pipeline, the parsing and generation of all tests will not be done, to make compilation as fast as possible.
    static bool parse_all_open_tests(const bool parse_parallel);

    /// @function `clear_instances`
    /// @brief Clears all parser instances
    static void clear_instances() {
        instances.clear();
        main_function_parsed = false;
        main_function_has_args = false;
        TestNode::clear_test_names();

        // Clear all the other static containers that hold pointers to parser data
        {
            std::lock_guard<std::mutex> lock(parsed_tests_mutex);
            parsed_tests.clear();
        }
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

    /// @var `main_file_hash`
    /// @brief The hash of the file contianing the main function
    static inline Hash main_file_hash{std::string("")};

    /// @var `open_functions_list`
    /// @brief The list of all open functions, which will be parsed in the second phase of the parser
    std::vector<std::pair<FunctionNode *, std::vector<Line>>> open_functions_list{};

    /// @var `file_node_ptr`
    /// @brief The file node this parser is currently parsing
    std::unique_ptr<FileNode> file_node_ptr;

    /// @var `annotation_queue`
    /// @brief A list of all annotations which are queued up before parsing the next definition or statement
    std::vector<AnnotationNode> annotation_queue;

    /// @var `core_namespaces`
    /// @brief A map mapping the names of each core module to it's namespace containing all definitions of that namespace
    static inline std::unordered_map<std::string, std::unique_ptr<Namespace>> core_namespaces;

    /// @var `instances`
    /// @brief All Parser instances which are present. Used by the two-pass parsing system
    static inline std::vector<Parser> instances;

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

    /// @function `get_instance_from_hash`
    /// @brief Returns the instance of the given file hash or nullopt if no instance with that hash exists
    ///
    /// @param `hash` The hash of the file to search the correct instance for
    /// @return `std::optional<const Parser &>` A reference to the parser instance
    static std::optional<const Parser *> get_instance_from_hash(const Hash &hash);

  private:
    // The constructor is private because only the Parser (the instances list) contains the actual Parser
    explicit Parser(const std::filesystem::path &file) :
        file(file),
        file_name(file.filename().string()),
        file_hash(Hash(std::filesystem::absolute(file))) {
        if (!file_exists_and_is_readable(file)) {
            throw std::runtime_error("The passed file '" + file.string() + "' could not be opened!");
        }
        source_code = std::make_unique<const std::string>(load_file(file));
    };
    explicit Parser(const std::filesystem::path &file, const std::string &file_content) :
        source_code(std::make_unique<std::string>(file_content)),
        file(file),
        file_name(file.filename().string()),
        file_hash(Hash(std::filesystem::absolute(file))) {}

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

    /// @var `last_parsed_call`
    /// @brief Stores a pointer to the last parsed call of this parser
    std::optional<CallNodeBase *> last_parsed_call;

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

    /// @var `source_code`
    /// @brief The source code of the file this parser instance handles
    std::unique_ptr<const std::string> source_code;

    /// @var `source_code_lines`
    /// @brief A list of all the lines of the source code where each line is a slice into the file content
    std::vector<std::pair<unsigned int, std::string_view>> source_code_lines;

    /// @var `file`
    /// @brief The path to the file to parse
    const std::filesystem::path file;

    /// @var `file_name`
    /// @brief The name of the file to be parsed
    const std::string file_name;

    /// @var `file_hash`
    /// @brief The hash of the file to be parsed
    const Hash file_hash;

    /// @var `next_mangle_id`
    /// @brief The next mangle id for the next function definition to be found in the current file being parsed, this simplifies the code
    /// generation phase a lot
    size_t next_mangle_id{0};

    /// @function `file_exists_and_is_readable`
    /// @brief Checks if the given file does exist and is readable
    ///
    /// @param `file_path` The file path to check
    /// @return `bool` Whether the file exists and is readable
    [[nodiscard]] static bool file_exists_and_is_readable(const std::filesystem::path &file_path);

    /// @function `load_file`
    /// @brief Loads a given file from a file path and returns the files content
    ///
    /// @param `path` The path to the file
    /// @return `std::string` The loaded file
    static std::string load_file(const std::filesystem::path &path);

    /// @function `add_parsed_test`
    /// @brief Adds a given test combined with the file it is contained in
    ///
    /// @param `test_node` The test which was parsed
    /// @param `file_name` The name of the file the test was parsed in
    static inline void add_parsed_test(TestNode *test_node, std::string file_name) {
        std::lock_guard<std::mutex> lock(parsed_tests_mutex);
        parsed_tests.emplace_back(test_node, file_name);
    }

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
    bool add_next_main_node(FileNode &file_node, token_slice &tokens);

    /// @function `get_definition_tokens`
    /// @brief Extracts all the tokens which are part of the definition
    ///
    /// @return `token_list` Returns the extracted tokens, being part of the definition
    ///
    /// @attention Deletes all tokens which are part of the definition from the given tokens list
    token_slice get_definition_tokens(const token_slice &tokens);

    /// @function `get_body_lines`
    /// @brief Extracts all the body lines based on their indentation. Returns a list of lines, where each line is a slice view into the
    /// token list
    ///
    /// @param `definition_indentation` The indentation level of the definition. The body of the definition will have at least one
    /// indentation level more
    /// @param `tokens` The tokens from which the body will be extracted from
    /// @return `std::vector<Line>` The extracted next body lines
    std::vector<Line> get_body_lines(unsigned int definition_indentation, token_slice &tokens);

    /// @function `collapse_types_in_lines`
    /// @brief Refines all given lines. Refinement means that all tabs within a line are removed and that all type tokens are collapsed to a
    /// single type token instead. When deleting tokens from the source, all other Lines' ranges are updated automatically
    ///
    /// @param `lines` The lines to refine
    /// @param `source` A reference to the source token vector directly to enable direct modification
    ///
    /// @note Also replaces all `identifier` tokens with an `TOK_ALIAS` if the identifier matches the import alias
    /// @note Also replaces all type aliases with their aliased types
    void collapse_types_in_lines(std::vector<Line> &lines, token_list &source);

    /// @function `substitute_type_aliases`
    /// @brief Recursively substitutes all type aliases within the type to resolve
    ///
    /// @param `type_to_resolve` The type to resolve
    void substitute_type_aliases(std::shared_ptr<Type> &type_to_resolve);

    /// @struct `CreateCallOrInitializerBaseRet`
    /// @brief The return type of the `create_call_or_initializer_base` function. It's return type got very complex and that's why this
    /// struct was needed, to make it just much easier to use the returned value instead of a big ass tuple
    struct CreateCallOrInitializerBaseRet {
        /// @var `args`
        /// @brief The arguments with which the initializer or call are "called" + whether the argument is expected to be a reference
        std::vector<std::pair<std::unique_ptr<ExpressionNode>, bool>> args;

        /// @var `type`
        /// @brief The call's return type or the intializer's type
        std::shared_ptr<Type> type;

        /// @var `is_initializer`
        /// @brief Whether it's an initializer or a call
        bool is_initializer;

        /// @var `function`
        /// @brief A pointer to the function the call targets. If it's an initializer then this value is a nullptr
        FunctionNode *function{nullptr};
    };

    /// @function `create_call_or_initializer_base`
    /// @brief Creates the base node for all calls or initializers
    ///
    /// @details This function is part of the `Util` class, as both call statements as well as call expressions use this function
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope the call happens in
    /// @param `tokens` The tokens which will be interpreted as call
    /// @param `call_namespace` The namespace the called function comes from, for example when called via an alias the namespace is the
    /// alias namepsace, when called directly it's the namespace of this file
    /// @return `...` The return values are stored in a dedicated struct for this function. For more information look there
    std::optional<CreateCallOrInitializerBaseRet> create_call_or_initializer_base( //
        const Context &ctx,                                                        //
        std::shared_ptr<Scope> &scope,                                             //
        const token_slice &tokens,                                                 //
        const Namespace *call_namespace                                            //
    );

    /// @function `create_unary_op_base`
    /// @brief Creates a UnaryOpBase from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return A optional value containing a tuple, where the first value is the operator token, the second value is the expression on
    /// which the unary operation is applied on and the third value is whether the unary operator is left to the expression
    std::optional<std::tuple<Token, std::unique_ptr<ExpressionNode>, bool>> create_unary_op_base( //
        const Context &ctx,                                                                       //
        std::shared_ptr<Scope> &scope,                                                            //
        const token_slice &tokens                                                                 //
    );

    /// @function `create_field_access_base`
    /// @brief Creates a tuple of all field access variables extracted from a field access
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the field access is defined
    /// @param `tokens` The list of tokens representing the field access
    /// @param `has_inbetween_operator` Whether the field access has an in-between operator like a `?` for example
    /// @return A optional value containing a tuple, where the
    ///     - first value is the base expression of the access
    ///     - second value is the name of the accessed field, nullopt if tis a $N access
    ///     - third value is the id of the field
    ///     - fourth value is the type of the field
    std::optional<std::tuple<std::unique_ptr<ExpressionNode>, std::optional<std::string>, unsigned int, std::shared_ptr<Type>>>
    create_field_access_base(                     //
        const Context &ctx,                       //
        std::shared_ptr<Scope> &scope,            //
        const token_slice &tokens,                //
        const bool has_inbetween_operator = false //
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
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the grouped access is defined
    /// @param `tokens` The list of tokens representing the grouped access
    /// @param `has_inbetween_operator` Whether the grouped field access has an in-between operator like a `?` for example
    /// @return A optional value containing a tuple, where the
    ///     - first value is the base expression of the access
    ///     - second value is the list of accessed field names
    ///     - third value is the list of accessed field ids
    ///     - fourth value is the list of accessed field types
    std::optional<std::tuple<std::unique_ptr<ExpressionNode>, std::vector<std::string>, std::vector<unsigned int>,
        std::vector<std::shared_ptr<Type>>>>
    create_grouped_access_base(                   //
        const Context &ctx,                       //
        std::shared_ptr<Scope> &scope,            //
        const token_slice &tokens,                //
        const bool has_inbetween_operator = false //
    );

    /// @function `ensure_castability_multiple`
    /// @brief Ensures that all expressions in the expression vector are castable to the given type respectively
    ///
    /// @param `to_type` The type to cast all expressions to if they are not that type already
    /// @param `expressions` The expressions to ensure to be compatible
    /// @param `tokens` The tokens forming the expressions
    /// @return `bool` Whether all expressions are compatible with the given type
    bool ensure_castability_multiple(                              //
        const std::shared_ptr<Type> &to_type,                      //
        std::vector<std::unique_ptr<ExpressionNode>> &expressions, //
        const token_slice &tokens                                  //
    );

    /// @function `add_annotation`
    /// @brief Adds the annotation contained in the given tokens to the annotation queue to be consumed by definitions or statements alike
    ///
    /// @param `tokens` The tokens containing the annotation to be added
    /// @return `bool` Whether the annotation was able to be added to the queue
    bool add_annotation(const token_slice &tokens);

    /// @function `ensure_no_allotation_leftovers`
    /// @brief Checks whether the annotation queue is empty, throws an error if the queue still contains annotations. This would mean that
    /// an annotation was defined which was unable to be consumed by the following definition / statement
    ///
    /// @return `bool` True if all is fine, false if there were leftovers
    bool enusure_no_annotation_leftovers();

    /**************************************************************************************************************************************
     * @region `Util` END
     *************************************************************************************************************************************/

    /**************************************************************************************************************************************
     * @region `Expression`
     * @brief This region is responsible for parsing everything about expressions
     *************************************************************************************************************************************/

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
    std::optional<VariableNode> create_variable(std::shared_ptr<Scope> &scope, const token_slice &tokens);

    /// @function `create_unary_op_expression`
    /// @brief Creates a UnaryOpExpression from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the unary operation is defined
    /// @param `tokens` The list of tokens representing the unary operation
    /// @return `std::optional<UnaryOpExpression>` An optional UnaryOpExpression if creation is successful, nullopt otherwise
    std::optional<UnaryOpExpression> create_unary_op_expression( //
        const Context &ctx,                                      //
        std::shared_ptr<Scope> &scope,                           //
        const token_slice &tokens                                //
    );

    /// @function `create_literal`
    /// @brief Creates a LiteralNode from the given tokens
    ///
    /// @param `tokens` The list of tokens representing the literal
    /// @return `std::optional<LiteralNode>` An optional LiteralNode if creation is successful, nullopt otherwise
    std::optional<LiteralNode> create_literal(const token_slice &tokens);

    /// @functon `create_string_interpolation`
    /// @brief Creates a StringInterpolationNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope the string interpolation takes place in
    /// @param `interpol_string` The string from which the interpolation is created
    /// @param `tokens` The tokens which contain the string interpolation (needed for correct error output)
    /// @retun `std::optional<std::unique_ptr<ExpressionNode>>` The string interpolation if creation was successful, nullopt otherwise
    std::optional<std::unique_ptr<ExpressionNode>> create_string_interpolation( //
        const Context &ctx,                                                     //
        std::shared_ptr<Scope> &scope,                                          //
        const std::string &interpol_string,                                     //
        const token_slice &tokens                                               //
    );

    /// @function `create_call_expression`
    /// @brief Creates a CallNodeExpression from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the call expression is defined
    /// @param `tokens` The list of tokens representing the call expression
    /// @param `alias` The potential alias base on which the call is done
    /// @return `std::optional<std::unique_ptr<CallNodeExpression>>` A unique pointer to the created CallNodeExpression
    std::optional<std::unique_ptr<ExpressionNode>> create_call_expression( //
        const Context &ctx,                                                //
        std::shared_ptr<Scope> &scope,                                     //
        const token_slice &tokens,                                         //
        const std::optional<Namespace *> &alias                            //
    );

    /// @function `create_initializer`
    /// @brief Creates a InitializerNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the initializer is defined
    /// @param `tokens` The list of tokens representing the initializer
    /// @return `std::optional<std::unique_ptr<InitializerNode>>` A unique pointer to the created InitializerNod3
    std::optional<std::unique_ptr<ExpressionNode>> create_initializer( //
        const Context &ctx,                                            //
        std::shared_ptr<Scope> &scope,                                 //
        const token_slice &tokens                                      //
    );

    /// @function `create_type_cast`
    /// @brief Creates a TypeCastNode from the given tokens, if the expression already is the wanted type it returns the expression directly
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the type cast is defined
    /// @param `tokens` The list of tokens representing the type cast
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An ExpressionNode if creation is successful, nullopt otherwise
    std::optional<std::unique_ptr<ExpressionNode>> create_type_cast( //
        const Context &ctx,                                          //
        std::shared_ptr<Scope> &scope,                               //
        const token_slice &tokens                                    //
    );

    /// @function `create_group_expression`
    /// @brief Creates a GroupExpressionNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the grouped expression is defined
    /// @param `tokens` The list of tokens representing the type cast
    /// @return `std::optional<GroupExpressionNode>` An optional grouped expression, nullopt if its creation failed
    std::optional<GroupExpressionNode> create_group_expression( //
        const Context &ctx,                                     //
        std::shared_ptr<Scope> &scope,                          //
        const token_slice &tokens                               //
    );

    /// @function `create_group_expressions`
    /// @brief Creates a bunch of comma-separated group expressions
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the expressions are defined
    /// @param `tokens` The list of tokens representing all expressions separated by commas
    /// @return `std::optional<std::vector<std::unique_ptr<ExpressionNode>>>` The list of expressions in the group
    std::optional<std::vector<std::unique_ptr<ExpressionNode>>> create_group_expressions( //
        const Context &ctx,                                                               //
        std::shared_ptr<Scope> &scope,                                                    //
        const token_slice &tokens                                                         //
    );

    /// @function `create_range_expression`
    /// @brief Creates a range expression from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the range expression is defined in
    /// @param `tokens` The list of tokens representing the range expression
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An optional range expression, nullopt if its creation failed
    std::optional<std::unique_ptr<ExpressionNode>> create_range_expression( //
        const Context &ctx,                                                 //
        std::shared_ptr<Scope> &scope,                                      //
        const token_slice &tokens                                           //
    );

    /// @function `create_data_access`
    /// @brief Creates a DataAccessNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the data access is defined
    /// @param `tokens` The list of tokens representing the data access
    /// @return `std::optional<DataAccessNode>` An optional data access node, nullopt if its creation failed
    std::optional<DataAccessNode> create_data_access( //
        const Context &ctx,                           //
        std::shared_ptr<Scope> &scope,                //
        const token_slice &tokens                     //
    );

    /// @function `create_grouped_data_access`
    /// @brief Creates a GroupedDataAccessNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the data access is defined
    /// @param `tokens` The list of tokens representing the data access
    /// @return `std::optional<GroupedDataAccessNode>` A grouped data access node, nullopt if its creation failed
    std::optional<GroupedDataAccessNode> create_grouped_data_access( //
        const Context &ctx,                                          //
        std::shared_ptr<Scope> &scope,                               //
        const token_slice &tokens                                    //
    );

    /// @function `create_array_initializer`
    /// @brief Creates an ArrayInitializerNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the array initializer is defined
    /// @param `tokens` The list of tokens representing the array initializer
    /// @return `std::optional<ArrayInitializerNode>` An array initializer node, nullopt if its creation failed
    std::optional<ArrayInitializerNode> create_array_initializer( //
        const Context &ctx,                                       //
        std::shared_ptr<Scope> &scope,                            //
        const token_slice &tokens                                 //
    );

    /// @function `create_array_access`
    /// @brief Creates an ArrayAccessNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the array initializer is defined
    /// @param `tokens` The list of tokens representing the array access
    /// @return `std::optional<ArrayAccessNode>` An array access node, nullopt if its creation failed
    std::optional<ArrayAccessNode> create_array_access( //
        const Context &ctx,                             //
        std::shared_ptr<Scope> &scope,                  //
        const token_slice &tokens                       //
    );

    /// @function `create_optional_chain`
    /// @brief Creates an OptionalChainNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the optional chain is defined
    /// @param `tokens` The list of tokens representing the optional chain
    /// @return `std::optional<OptionalChainNode>` An optional chain, nullopt if its creation failed
    std::optional<OptionalChainNode> create_optional_chain( //
        const Context &ctx,                                 //
        std::shared_ptr<Scope> &scope,                      //
        const token_slice &tokens                           //
    );

    /// @function `create_optional_unwrap`
    /// @brief Creates an OptionalUnwrapNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the optional unwrap is defined
    /// @param `tokens` The list of tokens representing the optional unwrap
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` The expression containing the optional unwrap, nullopt if creation failed
    std::optional<std::unique_ptr<ExpressionNode>> create_optional_unwrap( //
        const Context &ctx,                                                //
        std::shared_ptr<Scope> &scope,                                     //
        const token_slice &tokens                                          //
    );

    /// @function `create_variant_extraction`
    /// @brief Creates an VariantExtractionNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the variant extraction is defined
    /// @param `tokens` The list of tokens representing the variant extraction
    /// @return `std::optional<VariantExtractionNode>` A variant extraction, nullopt if its creation failed
    std::optional<VariantExtractionNode> create_variant_extraction( //
        const Context &ctx,                                         //
        std::shared_ptr<Scope> &scope,                              //
        const token_slice &tokens                                   //
    );

    /// @function `create_variant_unwrap`
    /// @brief Creates a VariantUnwrapNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the variant unwrap is defined
    /// @param `tokens` The list of tokens representing the variant unwrap
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` The expression containing the variant unwrap, nullopt if creation failed
    std::optional<std::unique_ptr<ExpressionNode>> create_variant_unwrap( //
        const Context &ctx,                                               //
        std::shared_ptr<Scope> &scope,                                    //
        const token_slice &tokens                                         //
    );

    /// @function `create_stacked_expression`
    /// @brief Creates a stacked expression from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the scoped expression is defined
    /// @param `tokens` The list of tokens representing the scoped expression
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` The stacked expression, nullopt if its creation failed
    ///
    /// @details Stacked expressions are unwrapped from the right to the left: `instance.field.field.field` becomes
    /// `(instance.field.field).field` and then it becomes `((instance.field).field).field`, so they are evalueated balanced from the right
    std::optional<std::unique_ptr<ExpressionNode>> create_stacked_expression( //
        const Context &ctx,                                                   //
        std::shared_ptr<Scope> &scope,                                        //
        const token_slice &tokens                                             //
    );

    /// @function `create_pivot_expression`
    /// @brief Creates a expression based on token precedences, where the token with the highest precedence is the "pivot point" of the
    /// epxression creation
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the expression is defined
    /// @param `tokens` The list of tokens representing the expression
    /// @param `expected_type` The expected type of the expression. Needed for the cration of default nodes
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An optional unique pointer to the created ExpressionNode
    std::optional<std::unique_ptr<ExpressionNode>> create_pivot_expression(      //
        const Context &ctx,                                                      //
        std::shared_ptr<Scope> &scope,                                           //
        const token_slice &tokens,                                               //
        const std::optional<std::shared_ptr<Type>> &expected_type = std::nullopt //
    );

    /// @function `create_expression`
    /// @brief Creates an ExpressionNode from the given tokens
    ///
    /// @param `ctx` The parsing context
    /// @param `scope` The scope in which the expression is defined
    /// @param `tokens` The list of tokens representing the expression
    /// @param `expected_type` The expected type of the expression. If possible, applies implicit type conversion to get this type
    /// @return `std::optional<std::unique_ptr<ExpressionNode>>` An optional unique pointer to the created ExpressionNode
    std::optional<std::unique_ptr<ExpressionNode>> create_expression(            //
        const Context &ctx,                                                      //
        std::shared_ptr<Scope> &scope,                                           //
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
    /// @param `alias` The potential alias base of the call
    /// @return `std::optional<std::unique_ptr<CallNodeStatement>>` A unique pointer to the created CallNodeStatement
    std::optional<std::unique_ptr<CallNodeStatement>> create_call_statement( //
        std::shared_ptr<Scope> &scope,                                       //
        const token_slice &tokens,                                           //
        const std::optional<Namespace *> &alias                              //
    );

    /// @function `create_throw`
    /// @brief Creates a ThrowNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the throw statement is defined
    /// @param `tokens` The list of tokens representing the throw statement
    /// @return `std::optional<ThrowNode>` An optional ThrowNode if creation is successful, nullopt otherwise
    std::optional<ThrowNode> create_throw(std::shared_ptr<Scope> &scope, const token_slice &tokens);

    /// @function `create_return`
    /// @brief Creates a ReturnNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the return statement is defined
    /// @param `tokens` The list of tokens representing the return statement
    /// @return `std::optional<ReturnNode>` An optional ReturnNode if creation is successful, nullopt otherwise
    std::optional<ReturnNode> create_return(std::shared_ptr<Scope> &scope, const token_slice &tokens);

    /// @function `create_if`
    /// @brief Creates an IfNode from the given if chain
    ///
    /// @param `scope` The scope in which the if statement is defined
    /// @param `if_chain` The list of token pairs representing the if statement chain
    /// @return `std::optional<std::unique_ptr<IfNode>>` An optional unique pointer to the created IfNode
    std::optional<std::unique_ptr<IfNode>> create_if(std::shared_ptr<Scope> &scope,
        std::vector<std::pair<token_slice, std::vector<Line>>> &if_chain);

    /// @function `create_do_while_loop`
    /// @brief Creates a DoWhileNode from the given definition and body tokens inside the given scope
    ///
    /// @param `scope` The scope in which the do-while loop is defined
    /// @param `condition_line` The list of tokens representing the end of the scope and the condition
    /// @param `body` The list of tokens representing the loop body
    /// @return `std::optional<std::unique_ptr<DoWhileNode>>` An optional unique pointer to the created DoWhileNode
    std::optional<std::unique_ptr<DoWhileNode>> create_do_while_loop( //
        std::shared_ptr<Scope> &scope,                                //
        const token_slice &condition_line,                            //
        const std::vector<Line> &body                                 //
    );

    /// @function `create_while_loop`
    /// @brief Creates a WhileNode from the given definition and body tokens inside the given scope
    ///
    /// @param `scope` The scope in which the while loop is defined
    /// @param `definition` The list of tokens representing the while loop definition
    /// @param `body` The list of tokens representing the while loop body
    /// @return `std::optional<std::unique_ptr<WhileNode>>` An optional unique pointer to the created WhileNode
    std::optional<std::unique_ptr<WhileNode>> create_while_loop( //
        std::shared_ptr<Scope> &scope,                           //
        const token_slice &definition,                           //
        const std::vector<Line> &body                            //
    );

    /// @function `create_for_loop`
    /// @brief Creates a ForLoopNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the for loop is defined
    /// @param `definition` The list of tokens representing the for loop definition
    /// @param `body` The list of tokens representing the for loop body
    /// @return `std::optional<std::unique_ptr<ForLoopNode>>` An optional unique pointer to the created ForLoopNode
    std::optional<std::unique_ptr<ForLoopNode>> create_for_loop(std::shared_ptr<Scope> &scope, const token_slice &definition,
        const std::vector<Line> &body);

    /// @function `create_enh_for_loop`
    /// @brief Creates an enhanced ForLoopNode from the given list of tokens
    ///
    /// @param `scope` The scope in which the enhanced for loop is defined
    /// @param `definition` The list of tokens representing the enhanced for loop definition
    /// @param `body` The list of tokens representing the enhanced for loop body
    /// @return `std::optional<std::unique_ptr<EnhForLoopNode>>` An optional unique pointer to the created enhanced ForLoopNode
    std::optional<std::unique_ptr<EnhForLoopNode>> create_enh_for_loop( //
        std::shared_ptr<Scope> &scope,                                  //
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
        std::shared_ptr<Scope> &scope,                                   //
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
        std::shared_ptr<Scope> &scope,              //
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
        std::shared_ptr<Scope> &scope,              //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const EnumNode *enum_node,                  //
        const bool is_statement                     //
    );

    /// @function `create_error_switch_branches`
    /// @brief Creates the branches for the error switch and adds them to the `s_branches` or `e_branches`, depending on whether it's a
    /// statement or an expression
    ///
    /// @param `scope` The scope in which the switch statement / expression switching on the enum value is defined
    /// @param `s_branches` The list of all statement branches
    /// @param `e_branches` The list of all expression branches
    /// @param `body` The body of the whole switch
    /// @param `switcher_type` The type of the expression which is switched upon
    /// @param `error_node` The pointer to the error node definition which is switched on
    /// @param `is_statement` Whether the created switch is an expression or a statement
    /// @return `bool` Whether the creation of the error switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_error_switch_branches(              //
        std::shared_ptr<Scope> &scope,              //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const ErrorNode *error_node,                //
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
    /// @param `is_mutable` Whether the switched on variable is mutable
    /// @return `bool` Whether the creation of the enum switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_optional_switch_branches(           //
        std::shared_ptr<Scope> &scope,              //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const bool is_statement,                    //
        const bool is_mutable                       //
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
    /// @param `is_mutable` Whether the switched on variable is mutable
    /// @return `bool` Whether the creation of the variant switch branches was successfull
    ///
    /// @attention The `s_branches` vector will be modified and filled with the branches of the switch statement
    /// @attention The `e_branches` vector will be modified and filled with the branches of the switch expression
    bool create_variant_switch_branches(            //
        std::shared_ptr<Scope> &scope,              //
        std::vector<SSwitchBranch> &s_branches,     //
        std::vector<ESwitchBranch> &e_branches,     //
        const std::vector<Line> &body,              //
        const std::shared_ptr<Type> &switcher_type, //
        const bool is_statement,                    //
        const bool is_mutable                       //
    );

    /// @function `create_switch_statement`
    /// @brief Creates an switch statement from the given list of tokens
    ///
    /// @param `scope` The scope in which the switch statement is defined
    /// @param `definition` The list of tokens representing the switch statements definition
    /// @param `body` The list of lines representing the switch statements entire body
    /// @return `std::optional<std::unique_ptr<StatementNode>>` An optional unique pointer to the created statement
    std::optional<std::unique_ptr<StatementNode>> create_switch_statement( //
        std::shared_ptr<Scope> &scope,                                     //
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
        std::shared_ptr<Scope> &scope,                          //
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
        std::shared_ptr<Scope> &scope,                          //
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
        std::shared_ptr<Scope> &scope,                                    //
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
        std::shared_ptr<Scope> &scope,                      //
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
        std::shared_ptr<Scope> &scope,                         //
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
        std::shared_ptr<Scope> &scope,                            //
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
        std::shared_ptr<Scope> &scope,                      //
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
    std::optional<UnaryOpStatement> create_unary_op_statement(std::shared_ptr<Scope> &scope, const token_slice &tokens);

    /// @function `create_data_field_assignment`
    /// @brief Creates a DataFieldAssignmentNode from the given tokens
    ///
    /// @param `scope` The scope in which the data field assignment is defined
    /// @param `tokens` The list of tokens representing the data field assignment node
    /// @param `rhs` The rhs of the assignment, which possibly is already parsed
    /// @return `std::optional<DataFieldAssignmentNode>` The created DataFieldAssignmentNode, nullopt if its creation failed
    std::optional<DataFieldAssignmentNode> create_data_field_assignment( //
        std::shared_ptr<Scope> &scope,                                   //
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
        std::shared_ptr<Scope> &scope,                                                  //
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
        std::shared_ptr<Scope> &scope,                                                            //
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
        std::shared_ptr<Scope> &scope,                          //
        const token_slice &tokens,                              //
        std::optional<std::unique_ptr<ExpressionNode>> &rhs     //
    );

    /// @function `create_stacked_statement`
    /// @brief Creates a stacked statement, like `a.b.c = sdf` for example
    ///
    /// @param `scope` The scope in which the stacked statement is defined
    /// @param `tokens` The list of tokens representing the stacked statement
    /// @return `std::optional<std::unique_ptr<StatementNode>>` The created stacked statement
    std::optional<std::unique_ptr<StatementNode>> create_stacked_statement(std::shared_ptr<Scope> &scope, const token_slice &tokens);

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
        std::shared_ptr<Scope> &scope,                                    //
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
        std::shared_ptr<Scope> &scope,                                     //
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
    std::optional<std::vector<std::unique_ptr<StatementNode>>> create_body(std::shared_ptr<Scope> &scope, const std::vector<Line> &body);

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

    /// @function `create_extern_function`
    /// @brief Creates an extern function definition from the given definition tokens. Communicates with the FIP to find out where the
    /// function comes from and whether any external language provides the function being searched for
    ///
    /// @param `definition` The list of tokens containing the function definition
    /// @return `std::optional<FunctionNode>` The created FunctionNode
    std::optional<FunctionNode> create_extern_function(const token_slice &definition);

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
    /// @return `std::optional<ErrorNode>` The created ErrorNode, nullopt if creation failed
    std::optional<ErrorNode> create_error(const token_slice &definition, const std::vector<Line> &body);

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
