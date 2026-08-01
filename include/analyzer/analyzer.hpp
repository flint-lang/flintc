#pragma once

#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>

class Parser;

/// @enum `ContextLevel`
/// @brief Provides context for the current analyzation / parsing level, whether it's internal, external or unknown
enum class ContextLevel {
    INTERNAL,
    EXTERNAL,
    CONST_DATA,
    UNKNOWN,
};

/// @class `Analyzer`
/// @brief This class is responsible for semantic analysis of the AST, including type checking,
/// scope validation, and semantic constraint verification.
/// @note This class cannot be initialized and all methods within this class are static
class Analyzer {
  public:
    Analyzer() = delete;

    /// @class `Context`
    /// @brief All the context needed for the analyzation stage, it's passed down each function
    struct Context {
        /// @var `level`
        /// @brief The context level the analyzation stage is currently at
        ContextLevel level;

        /// @var `is_function_definition_context`
        /// @brief Whether the type currently being analyzed is part of a function's own signature (a parameter or return type).
        ///        This selects which error class pointer types in non-extern contexts are reported with
        bool is_function_definition_context;

        /// @var `file_name`
        /// @brief The file name which is being analyzed
        std::string file_name;

        /// @var `line`
        /// @brief The line the analyzer is currently at in the file
        unsigned int line;

        /// @var `column`
        /// @brief The column the analyzer is currently at in file
        unsigned int column;

        /// @var `length`
        /// @brief The length of the current element being analyzed (for error reporting)
        unsigned int length;

        /// @var `parser`
        /// @brief The parser instance of the file being analyzed, used to access the file's tokens and to mutate the AST
        Parser &parser;

        /// @var `return_type`
        /// @brief The return type of the function currently being analyzed
        std::optional<std::shared_ptr<Type>> return_type;
    };

    /// @class `Castability`
    /// @brief Contains the whole castability engine, previously located in the Parser. The pure functions only depend on the types,
    /// the expression taking ones additionally need access to the `Parser` for the file hash and type storage
    class Castability {
      public:
        Castability() = delete;

        /// @struct `CastDirection`
        /// @brief A small helper structure representing the casting direction, it makes the whole system a lot easier and more extensible.
        /// I do not think that it will ever be extended in the future, but it makes everything around castability so much more clear and so
        /// much more readable that I just prefer this solution anyway
        struct CastDirection {
          public:
            /// @enum `Kind`
            /// @brief The kind of the cast direction, determining the direction in which to cast
            enum class Kind {
                NOT_CASTABLE,    // Types are incpomatible
                SAME_TYPE,       // The two types are actually the exact same type, just present inside different shared pointer containers
                CAST_LHS_TO_RHS, // Cast left operand to right's type
                CAST_RHS_TO_LHS, // Cast right operand to left's type
                CAST_BOTH_TO_COMMON, // Cast both operands to the common type below
                CAST_BIDIRECTIONAL,  // Casting both ways is possible without a problem
            };

            /// @var `kind`
            /// @brief The kind of this cast direction
            Kind kind;

            /// @var `common_type`
            /// @brief The common type to cast to
            ///
            /// @attention This variable is **ONLY** meaningful if the Kind is `CAST_BOTH_TO_COMMON`, access to this value is UB in all
            /// other cases
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

        /// @function `resolve_comptime_type_of_expr`
        /// @brief Resolves the comptime type a given expression has. Resolves the types of group expressions as well as literal
        /// expressions, for example changing `int` to `i32` (the default type for the given comptime type)
        ///
        /// @param `parser` The parser instance the expression belongs to, used for the file hash and type storage
        /// @param `expr` The expression to resolve all comptime types in
        /// @param `target_type` The target type of the literal, if it is known. This type will only be applied in the `int` or `float`
        /// literal type cases, if it is set
        /// @return Whether the type of the expression was changed
        static bool resolve_comptime_type_of_expr(                  //
            Parser &parser,                                         //
            std::unique_ptr<ExpressionNode> &expr,                  //
            const std::optional<std::shared_ptr<Type>> &target_type //
        );

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
        /// @param `parser` The parser instance the expression belongs to, used for the file hash and type storage
        /// @param `target_type` The type to cast towards, the target type of the expression
        /// @param `expr` The expression to cast / check
        /// @param `is_implicit` Whether casting is implicit or was explicit
        ///
        /// @note If the expression already is the target type this function will leave the expression unchanged and simply return true
        static bool check_castability(                //
            Parser &parser,                           //
            const std::shared_ptr<Type> &target_type, //
            std::unique_ptr<ExpressionNode> &expr,    //
            const bool is_implicit = true             //
        );

        /// @enum `BinopMatchResult`
        /// @brief The result of matching the two operands of a binary operation against each other
        enum class BinopMatchResult {
            OK,                   // The operands matched and were coerced to matching types if needed
            TYPE_MISMATCH,        // The operands are not castable to each other
            OPT_DEFAULT_MISMATCH, // The `??` operator's rhs is not castable to the optional's base type
        };

        /// @function `match_binop_operands`
        /// @brief Checks whether the two operands of a binary operation can be coerced to matching types, coercing them in place. This
        /// mirrors the castability check the parser used to perform before creating the `BinaryOpNode`, and is shared between the parser
        /// and the analyzer so that mismatches are reported at the correct stage
        ///
        /// @param `parser` The parser instance the expressions belong to, used for the file hash and type storage
        /// @param `pivot_token` The operator token of the binary operation
        /// @param `lhs` The left operand of the binary operation, coerced in place
        /// @param `rhs` The right operand of the binary operation, coerced in place
        /// @return `BinopMatchResult` The result of the match
        static BinopMatchResult match_binop_operands( //
            Parser &parser,                           //
            const Token &pivot_token,                 //
            std::unique_ptr<ExpressionNode> &lhs,     //
            std::unique_ptr<ExpressionNode> &rhs      //
        );
    };

    /// @function `analyze_file`
    /// @brief Analyzes the given parser instance's file node for semantic correctness. Errors are printed inside the analyzer
    ///
    /// @param `parser` The parser instance whose file to analyze
    /// @return `bool` Whether the file was analyzed successfully
    static bool analyze_file(Parser &parser);

    /// @function `analyze_definition`
    /// @brief Analyzes a top-level definition node (data, function, enum, etc.)
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `ast` The definition node to analyze
    /// @return `bool` Whether the definition was analyzed successfully
    static bool analyze_definition(const Context &ctx, std::unique_ptr<DefinitionNode> &ast);

    /// @function `analyze_scope`
    /// @brief Analyzes the given scope for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `scope` The scope to analyze
    /// @return `bool` Whether the scope was analyzed successfully
    static bool analyze_scope(const Context &ctx, Scope &scope);

    /// @function `analyze_statement`
    /// @brief Analyzes the given statement node for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `statement` The statement node to analyze
    /// @return `bool` Whether the statement was analyzed successfully
    static bool analyze_statement(const Context &ctx, StatementNode &statement);

    /// @function `analyze_expression`
    /// @brief Analyzes the given expression node for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `expression` The expression node to analyze
    /// @return `bool` Whether the expression was analyzed successfully
    static bool analyze_expression(const Context &ctx, std::unique_ptr<ExpressionNode> &expression);

    /// @function `analyze_type`
    /// @brief Analyzes the given type for correctness (for example using a pointer type in a non-extern context)
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `type` The type to analyze
    /// @param `is_empty_fixed_array_allowed` Whether fixed empty arrays are allowed. They are only allowed inside *expressions*, everywhere
    ///                                       else they are disallowed
    /// @return `bool` Whether the type was analyzed successfully
    static bool analyze_type(const Context &ctx, const std::shared_ptr<Type> &type, const bool is_empty_fixed_array_allowed = false);
};
