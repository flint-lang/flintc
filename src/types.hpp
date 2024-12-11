#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include "lexer/token_context.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/expressions/call_node.hpp"

#include <vector>
#include <variant>
#include <memory>

using token_list = std::vector<TokenContext>;
using body_statements = std::vector<std::variant<std::unique_ptr<StatementNode>, std::unique_ptr<CallNode>>>;

#endif
