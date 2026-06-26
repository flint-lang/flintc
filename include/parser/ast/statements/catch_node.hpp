#pragma once

#include "parser/ast/call_node_base.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/statement_node.hpp"

#include <memory>
#include <optional>

/// @class `CatchNode`
/// @brief Represents Catch statements
class CatchNode : public StatementNode {
  public:
    explicit CatchNode(                             //
        const Hash &hash,                           //
        const token_slice &tokens,                  //
        const std::optional<std::string> &var_name, //
        const std::shared_ptr<Scope> &scope,        //
        CallNodeBase *const call_node               //
        ) :
        StatementNode(hash, tokens),
        var_name(var_name),
        scope(scope),
        call_node(call_node) {}

    Variation get_variation() const override {
        return Variation::CATCH;
    }

    // constructor
    CatchNode() = delete;
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
    std::shared_ptr<Scope> scope;

    /// @var `call_node`
    /// @brief The call this catch belongs to
    CallNodeBase *call_node;
};
