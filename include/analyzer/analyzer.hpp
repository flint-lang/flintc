#pragma once

#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"

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

    /// @enum `Result`
    /// @brief The result of the analyzation
    enum class Result {
        OK,
        ERR_HANDLED,
        ERR_PTR_NOT_ALLOWED_IN_NON_EXTERN_CONTEXT,
    };

    /// @class `Context`
    /// @brief All the context needed for the analyzation stage, it's passed down each function
    struct Context {
        /// @var `level`
        /// @brief The context level the analyzation stage is currently at
        ContextLevel level;

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
    };

    /// @function `analyze`
    /// @brief Analyzes the given file node for semantic correctness
    ///
    /// @param `file` The file node to analyze
    /// @return `Result` The result of the analyzation
    static Result analyze_file(const FileNode *file);

    /// @function `analyze_definition`
    /// @brief Analyzes a top-level definition node (data, function, enum, etc.)
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `ast` The definition node to analyze
    /// @return `Result` The result of the analyzation
    static Result analyze_definition(const Context &ctx, const DefinitionNode *ast);

    /// @function `analyze_scope`
    /// @brief Analyzes the given scope for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `scope` The scope to analyze
    /// @return `Result` The result of the analyzation
    static Result analyze_scope(const Context &ctx, const Scope *scope);

    /// @function `analyze_statement`
    /// @brief Analyzes the given statement node for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `statement` The statement node to analyze
    /// @return `Result` The result of the analyzation
    static Result analyze_statement(const Context &ctx, const StatementNode *statement);

    /// @function `analyze_expression`
    /// @brief Analyzes the given expression node for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `expression` The expression node to analyze
    /// @return `Result` The result of the analyzation
    static Result analyze_expression(const Context &ctx, const ExpressionNode *expression);

    /// @function `analyze_type`
    /// @brief Analyzes the given type for correctness (for example using a pointer type in a non-extern context)
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `type` The type to analyze
    /// @return `Result` The result of the analyzation
    static Result analyze_type(const Context &ctx, const std::shared_ptr<Type> &type);
};
