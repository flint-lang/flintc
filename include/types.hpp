#pragma once

#include "lexer/token_context.hpp"

#include <cassert>
#include <utility>
#include <vector>

using token_list = std::vector<TokenContext>;
using token_slice = std::pair<token_list::iterator, token_list::iterator>;
using uint2 = std::pair<unsigned int, unsigned int>;
