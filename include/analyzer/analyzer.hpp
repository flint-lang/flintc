#pragma once

#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"

/// @class `Analyzer`
/// @brief This class is responsible for semantic analysis of the AST, including type checking,
/// scope validation, and semantic constraint verification.
/// @note This class cannot be initialized and all methods within this class are static
class Analyzer {
  public:
    Analyzer() = delete;

    /// @function `analyze`
    /// @brief Analyzes the given file node for semantic correctness
    ///
    /// @param `file` The file node to analyze
    /// @return `bool` Whether the analysis succeeded (true = success)
    static bool analyze(const FileNode *file);

  private:
    /// @class `Context`
    /// @brief All the context needed for the analyzation stage, it's passed down each function
    struct Context {
        /// @var `is_extern`
        /// @brief Whether the analyzation stage is inside an extern context, in extern calls or definitions
        bool is_extern;
    };

    /// @function `analyze_definition`
    /// @brief Analyzes a top-level definition node (data, function, enum, etc.)
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `ast` The definition node to analyze
    /// @return `bool` Whether the analysis succeeded (true = success)
    static bool analyze_definition(const Context &ctx, const DefinitionNode *ast);

    /// @function `analyze_scope`
    /// @brief Analyzes the given scope for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `scope` The scope to analyze
    /// @return `bool` Whether the analysis succeeded (true = success)
    static bool analyze_scope(const Context &ctx, const Scope *scope);

    /// @function `analyze_statement`
    /// @brief Analyzes the given statement node for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `statement` The statement node to analyze
    /// @return `bool` Whether the analysis succeeded (true = success)
    static bool analyze_statement(const Context &ctx, const StatementNode *statement);

    /// @function `analyze_expression`
    /// @brief Analyzes the given expression node for semantic correctness
    ///
    /// @param `ctx` The context of the analyzation
    /// @param `expression` The expression node to analyze
    /// @return `bool` Whether the analysis succeeded (true = success)
    static bool analyze_expression(const Context &ctx, const ExpressionNode *expression);
};
