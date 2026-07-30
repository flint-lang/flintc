#pragma once

#include "line.hpp"
#include "parser/hash.hpp"
#include "types.hpp"

#include <optional>

class Linearizer {
  public:
    Linearizer() = delete;

    /// @function `linearize`
    /// @brief Takes a given token list as the source and linearizes it to a list of logical lines
    ///
    /// @param `file_hash` The hash of the file whose token stream is linearized
    /// @param `source` The source tokens which will be turned into logical lines
    /// @return `std::optional<std::vector<Line>>` A list of logical lines, nullopt if its creation failed
    static std::optional<std::vector<Line>> linearize(const Hash &file_hash, token_list &source);
};
