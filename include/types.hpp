#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include "lexer/token_context.hpp"
#include "parser/ast/statements/statement_node.hpp"

#include <memory>
#include <vector>

using token_list = std::vector<TokenContext>;
using body_statement = std::unique_ptr<StatementNode>;
using uint2 = std::pair<unsigned int, unsigned int>;

#endif
