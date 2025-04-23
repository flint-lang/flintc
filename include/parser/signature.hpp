#pragma once

#include "lexer/token.hpp"
#include "types.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/// @enum `ESignature`
/// @brief An enum containing all types of signatures
enum class ESignature {
    // Basic Signatures
    ANYTOKEN,
    TYPE_PRIM,
    TYPE_PRIM_MULT,
    LITERAL,
    TYPE,
    ASSIGNMENT_OPERATOR,
    // Binary operations
    OPERATIONAL_BINOP,
    RELATIONAL_BINOP,
    BOOLEAN_BINOP,
    BINARY_OPERATOR,
    // Unary operations
    UNARY_OPERATOR,
    // Other Basic signatures
    REFERENCE,
    ARGS,
    NO_PRIM_ARGS,
    GROUP,

    // Definitions
    USE_STATEMENT,
    FUNCTION_DEFINITION,
    DATA_DEFINITION,
    FUNC_DEFINITION,
    ERROR_DEFINITION,
    ENUM_DEFINITION,
    VARIANT_DEFINITION,
    TEST_DEFINITION,

    // Entity Definitions
    ENTITY_DEFINITION,
    ENTITY_BODY_DATA,
    ENTITY_BODY_FUNC,
    ENTITY_BODY_LINK,
    ENTITY_BODY_LINKS,
    ENTITY_BODY_CONSTRUCTOR,
    ENTITY_BODY,

    // Expressions
    EXPRESSION,
    STRING_INTERPOLATION,
    GROUP_EXPRESSION,
    FUNCTION_CALL,
    TYPE_CAST,
    BIN_OP_EXPR,
    UNARY_OP_EXPR,
    LITERAL_EXPR,
    VARIABLE_EXPR,
    DATA_ACCESS,
    GROUPED_DATA_ACCESS,
    ARRAY_INITIALIZER,

    // Statements
    GROUP_DECLARATION_INFERRED,
    DECLARATION_WITHOUT_INITIALIZER,
    DECLARATION_EXPLICIT,
    DECLARATION_INFERRED,
    ASSIGNMENT,
    ASSIGNMENT_SHORTHAND,
    GROUP_ASSIGNMENT,
    DATA_FIELD_ASSIGNMENT,
    GROUPED_DATA_ASSIGNMENT,
    FOR_LOOP,
    ENHANCED_FOR_LOOP,
    PAR_FOR_LOOP,
    WHILE_LOOP,
    IF_STATEMENT,
    ELSE_IF_STATEMENT,
    ELSE_STATEMENT,
    RETURN_STATEMENT,
    THROW_STATEMENT,

    // Error handling
    CATCH_STATEMENT,
};

/// @class `Signature`
/// @brief Inside this class, all the signatures are defined. A signature can contain multiple other signatures as well.
///
/// @attention This class is not initializable
class Signature {
  public:
    Signature() = delete;

    /// @type `signature`
    /// @brief A signature type is essentially either a token or a string, and a list of that. Signature types only exist at compile-time,
    /// they are meant to be converted to regex strings at compile-time. This makes the regex matching system much faster, as the strings
    /// dont have to be built up every time a signature is used
    using signature = std::vector<std::variant<Token, std::string>>;

    /// @function `stringify`
    /// @brief Makes a vector of TokenContexts to a string that can be checked by regex
    ///
    /// @param `tokens` The list of tokens to stringify
    /// @return `std::string` The stringified tokens
    static std::string stringify(const token_list &tokens);

    /// @function `balanced_range_extraction`
    /// @brief Extracts the range of the given signatures where the 'inc' signature increments the amount of 'dec' signatures needed to
    /// reach the end of the range. This can be used to extract all operations between parenthesis, for example
    ///
    /// @param `tokens` The list of tokens in which to get the balanced range between the increment and decrement signatures
    /// @param `inc` The increment signature regex string
    /// @param `dec` The decrement signature regex string
    /// @return `std::optional<uint2>` The range of the balanced range, nullopt if the tokens do not contain the signatures
    static std::optional<uint2> balanced_range_extraction(const token_list &tokens, const std::string &inc, const std::string &dec);

    /// @function `balanced_range_extraction_vec`
    /// @brief Extracts all balanced ranges of the given inc and dec signatures
    ///
    /// @param `tokens` The list of tokens in which to get the balanced ranges between the increment and decrement signatures
    /// @param `inc` The increment signature regex string
    /// @param `dec` The decrement signature regex string
    /// @return `std::vector<uint2>` A list of all ranges from the balanced ranges
    static std::vector<uint2> balanced_range_extraction_vec(const token_list &tokens, const std::string &inc, const std::string &dec);

    /// @function `balanced_ranges_vec`
    /// @brief Returns a list of all balanced ranges in which the given increment and decrement is present inside the given string
    ///
    /// @param `src` The source string to search through
    /// @param `inc` The increment signature regex string
    /// @param `dec` The decrement signature regex string
    /// @return `std::vector<uint2>` A list of all ranges from the balanced ranges
    static std::vector<uint2> balanced_ranges_vec(const std::string &src, const std::string &inc, const std::string &dec);

    /// @function `match_until_signature`
    /// @brief Creates a new signature where all token matches until inclusive the given signature.
    ///
    /// @details Each "not until" match can be extracted separately. This function will be primarily used for extracting statements
    /// ('[^;]*;') matches
    ///
    /// @param `signature` The signature until to create a match signature
    /// @return `signature` The signature to match until the give signature
    static signature match_until_signature(const signature &signature);

    /// @function `get_regex_string`
    /// @brief Returns the built regex string of a passed in signature
    ///
    /// @param `sig` The signature from which to get the string
    /// @return `std::string` The regex string from the give signature
    static std::string get_regex_string(const signature &sig);

    /// @function `tokens_contain`
    /// @brief Checks if a given vector of TokenContext contains a given signature
    ///
    /// @param `tokens` The tokens to check if they contain the given signature
    /// @param `signature` The signature enum of the signature to check
    /// @return `bool` Whether the given token list contains the given signature
    static bool tokens_contain(const token_list &tokens, const ESignature signature);

    /// @function `tokens_contain`
    /// @brief Checks if a given vector of TokenContext contains a given signature
    ///
    /// @param `tokens` The tokens to check if they contain the given signature
    /// @param `signature` The Token to check if the list of tokens contains it
    /// @return `bool` Whether the given token list contains the given signature
    static bool tokens_contain(const token_list &tokens, const Token signature);

    /// @function `tokens_match`
    /// @brief Checks if a given vector of TokenContext matches a given signature
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `signature` The signature enum to check for
    /// @return `bool` Whether the given token list matches the givn signature
    static bool tokens_match(const token_list &tokens, const ESignature signature);

    /// @function `tokens_contain_in_range`
    /// @brief Checks if a given vector of TokenContext elements matches a given signature within a given range of the vector
    ///
    /// @param `tokens` The tokens to check
    /// @param `signature` The signature enum to search for
    /// @param `range` The range in which to search for the given signature
    /// @return `bool` Whether the given token list contains the given signature in the given range
    static bool tokens_contain_in_range(const token_list &tokens, const ESignature signature, const uint2 &range);

    /// @function `tokens_contain_in_range`
    /// @brief Checks if a given vector of tokens contains a given token (signature) in a given range
    ///
    /// @param `tokens` The tokens to check
    /// @param `signature` The Token to search for
    /// @param `range` The range in which to search for the given signature
    /// @return `bool` Whether the given token list contains the given signature in the given range
    static bool tokens_contain_in_range(const token_list &tokens, const Token signature, const uint2 &range);

    /// @function `tokens_contain_in_range_outside_group`
    /// @brief Checks if a given token list contains a given signature inside a given range, but outside the balanced ranges inside the list
    ///
    /// @details This function is primarily used to help in the extraction of arguments or types
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `signature` The signature to search for
    /// @param `range` The total range in which to search for the signature
    /// @param `inc` The increase signature of possible balanced ranges inside the list of tokens to skip
    /// @param `dec` The decrease signature of possible balanced ranges inside the list of tokens to skip
    /// @return `bool` Whether the given signature is within the given range but not within any group
    static bool tokens_contain_in_range_outside_group( //
        const token_list &tokens,                      //
        const std::string &signature,                  //
        const uint2 &range,                            //
        const std::string &inc,                        //
        const std::string &dec                         //
    );

    /// @function `get_tokens_line_range`
    /// @brief Returns the range of a given line within the tokens_list
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `line` The line from which to get the token range from
    /// @return `std::optional<uint2>` The range of all tokens in the given line, nullopt if no tokens exist in the given line
    static std::optional<uint2> get_tokens_line_range(const token_list &tokens, unsigned int line);

    /// @function `get_match_ranges`
    /// @brief Returns a list of all match ranges of the given signature in the given token list
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `signature` The regex signature string to search for
    /// @return `std::vector<uint2>` A list of all matches of the given signature
    static std::vector<uint2> get_match_ranges(const token_list &tokens, const std::string &signature);

    /// @function `get_match_ranges`
    /// @brief Returns a list of all match ranges of the given signature in the given token list
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `signature` The signature enum to search for
    /// @return `std::vector<uint2>` A list of all matches of the given signature
    static std::vector<uint2> get_match_ranges(const token_list &tokens, const ESignature signature);

    /// @function `get_match_ranges_in_range`
    /// @brief Returns a list of all match ranges of the given signature in the given token list that also are within a given range
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `signature` The regex string signature to search for
    /// @param `range` The range in which to search for matches
    /// @return `std::vector<uint2>` A list of all matches within the given range of the given signature
    static std::vector<uint2> get_match_ranges_in_range(const token_list &tokens, const std::string &signature, const uint2 &range);

    /// @function `get_match_ranges_in_range`
    /// @brief Returns a list of all match ranges of the given Token signature in the given token list that also are within a given range
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `signature` The Token to search for
    /// @param `range` The range in which to search for matches
    /// @return `std::vector<uint2>` A list of all matches within the given range of the given signature
    static std::vector<uint2> get_match_ranges_in_range(const token_list &tokens, const Token signature, const uint2 &range);

    /// @function `get_match_ranges_in_range_outside_group`
    /// @brief Returns all matches of the given signature within the given range thats not within any group defined by the inc and dec
    /// signatures for the balanced range extraction
    ///
    /// @details This function is primarily used to help in the extraction of arguments or types
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `signature` The signature to search for
    /// @param `range` The total range in which to search for the signature
    /// @param `inc` The increase signature of possible balanced ranges inside the list of tokens to skip
    /// @param `dec` The decrease signature of possible balanced ranges inside the list of tokens to skip
    /// @return `std::vector<uint2>` All the match ranges that are inside the given range but not inside any group
    static std::vector<uint2> get_match_ranges_in_range_outside_group( //
        const token_list &tokens,                                      //
        const std::string &signature,                                  //
        const uint2 &range,                                            //
        const std::string &inc,                                        //
        const std::string &dec                                         //
    );

    /// @function `get_next_match_range`
    /// @brief Returns the next match range, if the token_list contains the specified signature
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `signature` The regex signature string to get the next match for
    /// @return `std::optional<uint2>` The next match range, nullopt if the token list doesnt contain the given signature
    static std::optional<uint2> get_next_match_range(const token_list &tokens, const std::string &signature);

    /// @function `get_leading_indents`
    /// @brief Returns the number of leading indents in the given line in the given list of tokens
    ///
    /// @param `tokens` The list of tokens to scan
    /// @param `line` The line in which to check for all indents
    /// @return `std::optional<unsigned int>` The number of indents in the given line, nullopt if the line doesnt exist
    static std::optional<unsigned int> get_leading_indents(const token_list &tokens, unsigned int line);

  private:
    /// @function `combine`
    /// @brief Combines all the initializers of the signature type into a single signature type
    ///
    /// @details This is possible because the signature type is of type vector, so each signature, being a vector, can be inserted into a
    /// coherent vector of variants. This Makes creating nested signatures **much** easier
    ///
    /// @param `signatures` The initializer list of all signatures to combine to a new signature
    /// @return `signature` The combined signature
    static signature combine(std::initializer_list<signature> signatures);

    /// @function `get`
    /// @brief Returns the regex string from the map of the matching signature enum
    ///
    /// @param `signature` The signature enum from which to get the regex string
    /// @return `std::string` The regex string signature of the given signature enum
    static std::string get(const ESignature signature);

    /// @function `tokens_contain`
    /// @brief Checks if a given vector of TokenContext contains a given signature
    ///
    /// @param `tokens` The tokens to check if they contain the given signature
    /// @param `signature` The regex string of the signature to search for
    /// @return `bool` Whether the given token list contains the given signature
    static bool tokens_contain(const token_list &tokens, const std::string &signature);

    /// @function `tokens_match`
    /// @brief Checks if a given vector of TokenContext matches a given signature
    ///
    /// @param `tokens` The list of tokens to check
    /// @param `signature` The signature to check for
    /// @return `bool` Whether the given token list matches the givn signature
    static bool tokens_match(const token_list &tokens, const std::string &signature);

    /// @function `tokens_contain_in_range`
    /// @brief Checks if a given vector of TokenContext elements matches a given signature within a given range of the vector
    ///
    /// @param `tokens` The tokens to check
    /// @param `signature` The regex signature string to search for
    /// @param `range` The range in which to search for the given signature
    /// @return `bool` Whether the given token list contains the given signature in the given range
    static bool tokens_contain_in_range(const token_list &tokens, const std::string &signature, const uint2 &range);

    // --- BASIC SIGNATURES ---
    static const inline signature anytoken = {"#-?..#"};
    static const inline signature type_prim = {"(", TOK_I32, "|", TOK_I64, "|", TOK_U32, "|", TOK_U64, "|", TOK_F32, "|", TOK_F64, "|",
        TOK_FLINT, "|", TOK_STR, "|", TOK_CHAR, "|", TOK_BOOL, ")"};
    static const inline signature type_prim_mult = {"(", TOK_I32X2, "|", TOK_I32X3, "|", TOK_I32X4, "|", TOK_I32X8, "|", TOK_I64X2, "|",
        TOK_I64X3, "|", TOK_I64X4, "|", TOK_F32X2, "|", TOK_F32X3, "|", TOK_F32X4, "|", TOK_F32X8, "|", TOK_F64X2, "|", TOK_F64X3, "|",
        TOK_F64X4, ")"};
    static const inline signature literal = {"(", TOK_STR_VALUE, "|", TOK_INT_VALUE, "|", TOK_FLINT_VALUE, "|", TOK_CHAR_VALUE, "|",
        TOK_TRUE, "|", TOK_FALSE, ")"};
    static const inline signature type = combine({//
        {"("}, type_prim, {"|", TOK_IDENTIFIER, "|", TOK_IDENTIFIER, TOK_LEFT_BRACKET, TOK_RIGHT_BRACKET, "|"}, type_prim,
        {TOK_LEFT_BRACKET, TOK_RIGHT_BRACKET, "|"}, type_prim_mult, {")"}});
    static const inline signature assignment_operator = {"(", TOK_PLUS_EQUALS, "|", TOK_MINUS_EQUALS, "|", TOK_MULT_EQUALS, "|",
        TOK_DIV_EQUALS, ")"};
    static const inline signature operational_binop = {"(", TOK_PLUS, "|", TOK_MINUS, "|", TOK_MULT, "|", TOK_DIV, "|", TOK_POW, ")"};
    static const inline signature relational_binop = {"(", TOK_EQUAL_EQUAL, "|", TOK_NOT_EQUAL, "|", TOK_LESS, "|", TOK_LESS_EQUAL, "|",
        TOK_GREATER, "|", TOK_GREATER_EQUAL, ")"};
    static const inline signature boolean_binop = {"(", TOK_AND, "|", TOK_OR, ")"};
    static const inline signature binary_operator = combine({//
        {"("}, operational_binop, {"|"}, relational_binop, {"|"}, boolean_binop, {")"}});
    static const inline signature unary_operator = {"(", TOK_INCREMENT, "|", TOK_DECREMENT, "|", TOK_NOT, "|", TOK_MINUS, ")"};
    static const inline signature reference = {TOK_IDENTIFIER, "(", TOK_COLON, TOK_COLON, TOK_IDENTIFIER, ")+"};
    static const inline signature args = combine({//
        type, {TOK_IDENTIFIER, "(", TOK_COMMA}, type, {TOK_IDENTIFIER, ")*"}});
    static const inline signature params = combine({//
        {"(", TOK_MUT, "|", TOK_CONST, ")?"}, type, {TOK_IDENTIFIER, "(", TOK_COMMA, "(", TOK_MUT, "|", TOK_CONST, ")?"}, type,
        {TOK_IDENTIFIER, ")*"}});
    static const inline signature no_prim_args = {TOK_IDENTIFIER, TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, TOK_IDENTIFIER, ")*"};
    static const inline signature group = combine({//
        {TOK_LEFT_PAREN}, type, {"(", TOK_COMMA}, type, {")*", TOK_RIGHT_PAREN}});

    // --- DEFINITIONS ---
    static const inline signature use_statement = {TOK_USE, "(", TOK_STR_VALUE, "|((", TOK_IDENTIFIER, "|", TOK_FLINT, ")(", TOK_DOT,
        TOK_IDENTIFIER, ")*))"};

    static const inline signature function_definition = combine({//
        {"(", TOK_ALIGNED, ")?", "(", TOK_CONST, ")?", TOK_DEF, TOK_IDENTIFIER, TOK_LEFT_PAREN, "("}, params,
        {")?", TOK_RIGHT_PAREN, "(", TOK_ARROW}, group, {TOK_COLON, "|", TOK_ARROW}, type, {TOK_COLON, "|", TOK_COLON, ")"}});
    static const inline signature data_definition = {"(", TOK_SHARED, "|", TOK_IMMUTABLE, ")?(", TOK_ALIGNED, ")?", TOK_DATA,
        TOK_IDENTIFIER, TOK_COLON};
    static const inline signature func_definition = combine({//
        {TOK_FUNC, TOK_IDENTIFIER, "(", TOK_REQUIRES, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}});
    static const inline signature error_definition = {TOK_ERROR, TOK_IDENTIFIER, "(", TOK_LEFT_PAREN, TOK_IDENTIFIER, TOK_RIGHT_PAREN, ")?",
        TOK_COLON};
    static const inline signature enum_definition = {TOK_ENUM, TOK_IDENTIFIER, TOK_COLON};
    static const inline signature variant_definition = {TOK_VARIANT, TOK_IDENTIFIER, TOK_COLON};
    static const inline signature test_definition = {TOK_TEST, TOK_STR_VALUE, TOK_COLON};

    // --- ENTITY DEFINITION ---
    static const inline signature entity_definition = combine({//
        {TOK_ENTITY, TOK_IDENTIFIER, "(", TOK_EXTENDS, TOK_LEFT_PAREN}, no_prim_args, {TOK_RIGHT_PAREN, ")?", TOK_COLON}});
    static const inline signature entity_body_data = combine({//
        {TOK_DATA, TOK_COLON, "("}, anytoken, {")*", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*", TOK_SEMICOLON}});
    static const inline signature entity_body_func = combine({//
        {TOK_FUNC, TOK_COLON, "("}, anytoken, {")*", TOK_IDENTIFIER, "(", TOK_COMMA, TOK_IDENTIFIER, ")*", TOK_SEMICOLON}});
    static const inline signature entity_body_link = combine({reference, {TOK_ARROW}, reference, {TOK_SEMICOLON}});
    static const inline signature entity_body_links = combine({//
        {TOK_LINK, TOK_COLON, "("}, anytoken, {")*("}, entity_body_link, {"("}, anytoken, {")*)+"}});
    static const inline signature entity_body_constructor = {TOK_IDENTIFIER, TOK_LEFT_PAREN, "(", TOK_IDENTIFIER, "(", TOK_COMMA,
        TOK_IDENTIFIER, ")*)?", TOK_RIGHT_PAREN, TOK_SEMICOLON};
    static const inline signature entity_body = combine({//
        {"("}, entity_body_data, {")?("}, anytoken, {")*("}, entity_body_func, {")?("}, anytoken, {")*("}, entity_body_links, {")?("},
        anytoken, {")*"}, entity_body_constructor});

    // --- EXPRESSIONS ---
    static const inline signature expression = combine({{"("}, anytoken, {")*"}});
    static const inline signature string_interpolation = {TOK_DOLLAR, TOK_STR_VALUE};
    static const inline signature group_expression = combine({{TOK_LEFT_PAREN}, expression, {TOK_COMMA}, expression, {TOK_RIGHT_PAREN}});
    static const inline signature function_call = combine({{TOK_IDENTIFIER, TOK_LEFT_PAREN, "("}, expression, {")?", TOK_RIGHT_PAREN}});
    static const inline signature type_cast = combine({type_prim, {TOK_LEFT_PAREN, "("}, expression, {")", TOK_RIGHT_PAREN}});
    static const inline signature bin_op_expr = combine({expression, binary_operator, expression});
    static const inline signature unary_op_expr =
        combine({{"(("}, expression, unary_operator, {")|("}, unary_operator, expression, {"))"}});
    static const inline signature literal_expr = combine({//
        {"("}, literal, {"("}, binary_operator, literal, {")*|"}, unary_operator, literal, {"|"}, literal, unary_operator, {")"}});
    static const inline signature variable_expr = {TOK_IDENTIFIER, "(?!", TOK_LEFT_PAREN, ")"};
    static const inline signature data_access = {TOK_IDENTIFIER, TOK_DOT, TOK_IDENTIFIER, "(?!", TOK_LEFT_PAREN, ")"};
    static const inline signature grouped_data_access = combine({{TOK_IDENTIFIER, TOK_DOT}, group_expression});
    static const inline signature array_initializer = combine({//
        type, {TOK_LEFT_BRACKET, TOK_INT_VALUE, TOK_RIGHT_BRACKET, TOK_LEFT_PAREN}, expression, {TOK_RIGHT_PAREN}});

    // --- STATEMENTS ---
    static const inline signature group_declaration_inferred = combine({//
        {TOK_LEFT_PAREN}, expression, {TOK_COMMA}, expression, {TOK_RIGHT_PAREN, TOK_COLON_EQUAL}});
    static const inline signature declaration_without_initializer = combine({type, {TOK_IDENTIFIER, TOK_SEMICOLON}});
    static const inline signature declaration_explicit = combine({type, {TOK_IDENTIFIER, TOK_EQUAL}});
    static const inline signature declaration_inferred = {TOK_IDENTIFIER, TOK_COLON_EQUAL};
    static const inline signature assignment = {TOK_IDENTIFIER, TOK_EQUAL};
    static const inline signature assignment_shorthand = combine({{TOK_IDENTIFIER}, assignment_operator});
    static const inline signature group_assignment = combine({{TOK_LEFT_PAREN}, match_until_signature({TOK_RIGHT_PAREN}), {TOK_EQUAL}});
    static const inline signature data_field_assignment = combine({data_access, {TOK_EQUAL}});
    static const inline signature grouped_data_assignment = combine({grouped_data_access, {TOK_EQUAL}});
    static const inline signature for_loop = combine({//
        {TOK_FOR}, match_until_signature({TOK_SEMICOLON}), match_until_signature({TOK_SEMICOLON}), match_until_signature({TOK_COLON})});
    static const inline signature enhanced_for_loop = combine({//
        {TOK_FOR, "(", TOK_UNDERSCORE, "|", TOK_IDENTIFIER, ")", TOK_COMMA, "(", TOK_UNDERSCORE, "|", TOK_IDENTIFIER, ")", TOK_IN},
        match_until_signature({TOK_COLON})});
    static const inline signature par_for_loop = combine({{TOK_PARALLEL}, enhanced_for_loop});
    static const inline signature while_loop = combine({{TOK_WHILE}, match_until_signature({TOK_COLON})});
    static const inline signature if_statement = combine({{TOK_IF}, match_until_signature({TOK_COLON})});
    static const inline signature else_if_statement = combine({{TOK_ELSE, TOK_IF}, match_until_signature({TOK_COLON})});
    static const inline signature else_statement = combine({{TOK_ELSE}, match_until_signature({TOK_COLON})});
    static const inline signature return_statement = combine({{TOK_RETURN}, match_until_signature({TOK_SEMICOLON})});
    static const inline signature throw_statement = combine({{TOK_THROW}, match_until_signature({TOK_SEMICOLON})});

    // --- ERROR HANDLING --- (requires function_call to be defined)
    static const inline signature catch_statement = combine({function_call, {TOK_CATCH, "(", TOK_IDENTIFIER, ")?", TOK_COLON}});

    /// @var `regex_string`
    /// @brief A constant map to generate the signature strings at compile-time to make actual execution (compilation) faster
    static const inline std::unordered_map<ESignature, const std::string> regex_strings = {
        // Basic Signatures
        {ESignature::ANYTOKEN, get_regex_string(anytoken)},
        {ESignature::TYPE_PRIM, get_regex_string(type_prim)},

        {ESignature::TYPE_PRIM_MULT, get_regex_string(type_prim_mult)},
        {ESignature::LITERAL, get_regex_string(literal)},
        {ESignature::TYPE, get_regex_string(type)},
        {ESignature::ASSIGNMENT_OPERATOR, get_regex_string(assignment_operator)},
        // Binary operations
        {ESignature::OPERATIONAL_BINOP, get_regex_string(operational_binop)},
        {ESignature::RELATIONAL_BINOP, get_regex_string(relational_binop)},
        {ESignature::BOOLEAN_BINOP, get_regex_string(boolean_binop)},
        {ESignature::BINARY_OPERATOR, get_regex_string(binary_operator)},
        // Unary operations
        {ESignature::UNARY_OPERATOR, get_regex_string(unary_operator)},
        // Other Basic signatures
        {ESignature::REFERENCE, get_regex_string(reference)},
        {ESignature::ARGS, get_regex_string(args)},
        {ESignature::NO_PRIM_ARGS, get_regex_string(no_prim_args)},
        {ESignature::GROUP, get_regex_string(group)},

        // Definitions
        {ESignature::USE_STATEMENT, get_regex_string(use_statement)},
        {ESignature::FUNCTION_DEFINITION, get_regex_string(function_definition)},
        {ESignature::DATA_DEFINITION, get_regex_string(data_definition)},
        {ESignature::FUNC_DEFINITION, get_regex_string(func_definition)},
        {ESignature::ERROR_DEFINITION, get_regex_string(error_definition)},
        {ESignature::ENUM_DEFINITION, get_regex_string(enum_definition)},
        {ESignature::VARIANT_DEFINITION, get_regex_string(variant_definition)},
        {ESignature::TEST_DEFINITION, get_regex_string(test_definition)},

        // Entity Definitions
        {ESignature::ENTITY_DEFINITION, get_regex_string(entity_definition)},
        {ESignature::ENTITY_BODY_DATA, get_regex_string(entity_body_data)},
        {ESignature::ENTITY_BODY_FUNC, get_regex_string(entity_body_func)},
        {ESignature::ENTITY_BODY_LINK, get_regex_string(entity_body_link)},
        {ESignature::ENTITY_BODY_LINKS, get_regex_string(entity_body_links)},
        {ESignature::ENTITY_BODY_CONSTRUCTOR, get_regex_string(entity_body_constructor)},
        {ESignature::ENTITY_BODY, get_regex_string(entity_body)},

        // Expressions
        {ESignature::EXPRESSION, get_regex_string(expression)},
        {ESignature::STRING_INTERPOLATION, get_regex_string(string_interpolation)},
        {ESignature::GROUP_EXPRESSION, get_regex_string(group_expression)},
        {ESignature::FUNCTION_CALL, get_regex_string(function_call)},
        {ESignature::TYPE_CAST, get_regex_string(type_cast)},
        {ESignature::BIN_OP_EXPR, get_regex_string(bin_op_expr)},
        {ESignature::UNARY_OP_EXPR, get_regex_string(unary_op_expr)},
        {ESignature::LITERAL_EXPR, get_regex_string(literal_expr)},
        {ESignature::VARIABLE_EXPR, get_regex_string(variable_expr)},
        {ESignature::DATA_ACCESS, get_regex_string(data_access)},
        {ESignature::GROUPED_DATA_ACCESS, get_regex_string(grouped_data_access)},
        {ESignature::ARRAY_INITIALIZER, get_regex_string(array_initializer)},

        // Statements
        {ESignature::GROUP_DECLARATION_INFERRED, get_regex_string(group_declaration_inferred)},
        {ESignature::DECLARATION_WITHOUT_INITIALIZER, get_regex_string(declaration_without_initializer)},
        {ESignature::DECLARATION_EXPLICIT, get_regex_string(declaration_explicit)},
        {ESignature::DECLARATION_INFERRED, get_regex_string(declaration_inferred)},
        {ESignature::ASSIGNMENT, get_regex_string(assignment)},
        {ESignature::ASSIGNMENT_SHORTHAND, get_regex_string(assignment_shorthand)},
        {ESignature::GROUP_ASSIGNMENT, get_regex_string(group_assignment)},
        {ESignature::DATA_FIELD_ASSIGNMENT, get_regex_string(data_field_assignment)},
        {ESignature::GROUPED_DATA_ASSIGNMENT, get_regex_string(grouped_data_assignment)},
        {ESignature::FOR_LOOP, get_regex_string(for_loop)},
        {ESignature::ENHANCED_FOR_LOOP, get_regex_string(enhanced_for_loop)},
        {ESignature::PAR_FOR_LOOP, get_regex_string(par_for_loop)},
        {ESignature::WHILE_LOOP, get_regex_string(while_loop)},
        {ESignature::IF_STATEMENT, get_regex_string(if_statement)},
        {ESignature::ELSE_IF_STATEMENT, get_regex_string(else_if_statement)},
        {ESignature::ELSE_STATEMENT, get_regex_string(else_statement)},
        {ESignature::RETURN_STATEMENT, get_regex_string(return_statement)},
        {ESignature::THROW_STATEMENT, get_regex_string(throw_statement)},

        // Error handling
        {ESignature::CATCH_STATEMENT, get_regex_string(catch_statement)},
    };
}; // class Signature
