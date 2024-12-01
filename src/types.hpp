#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include "lexer/token.hpp"
#include "lexer/token_context.hpp"

#include <vector>
#include <variant>
#include <string>

namespace Signature {
    using signature = std::vector<std::variant<Token, std::string>>;
}
using token_list = std::vector<TokenContext>;

#endif
