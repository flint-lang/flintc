#pragma once

#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"

#include <memory>
#include <optional>
#include <utility>

/// @class `CatchNode`
/// @brief Represents Catch statements
class CatchNode : public StatementNode {
  public:
    explicit CatchNode(std::optional<std::string> &var_name, std::unique_ptr<Scope> &scope, unsigned int call_id) :
        var_name(std::move(var_name)),
        scope(std::move(scope)),
        call_id(call_id) {}

    // constructor
    CatchNode() = default;
    // deconstructor
    ~CatchNode() override = default;
    // copy operations - deleted because of unique_ptr member
    CatchNode(const CatchNode &) = delete;
    CatchNode &operator=(const CatchNode &) = delete;
    // move operations
    CatchNode(CatchNode &&) = default;
    CatchNode &operator=(CatchNode &&) = default;

    /// @var `var_name`
    /// @brief The name of the catch variable. Could be none too, then the catch block behaves differently
    std::optional<std::string> var_name;

    /// @var `scope`
    /// @brief The scope of the catch block
    std::unique_ptr<Scope> scope;

    /// @var `call_id`
    /// @brief The id of the call
    unsigned int call_id{0};
};
