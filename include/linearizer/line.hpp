#pragma once

#include "types.hpp"

/// @class `Line`
/// @brief A single logical line of tokens. A line is meant to be placed within a list of lines. Because the underlying token list may be
/// mutated, positions of the lines are relative from line to line, so later lines remain valid even if tokens are removed from earlier
/// lines.
struct Line {
    /// @var `indent_lvl`
    /// @brief The indentation level of the line
    unsigned int indent_lvl = 0;

    /// @var `offset`
    /// @brief The number of tokens between the end of the previous line and the start of this line, inside the owning token list
    std::size_t offset = 0;

    /// @var `len`
    /// @brief The number of tokens this line currently contains in the owning token list. Shrinks when tokens are deleted during
    /// type-collapsing
    std::size_t len = 0;

    /// @var `tokens`
    /// @brief The materialized slice of this line into the owning token list. The tokens will materialize after collapsing took place in
    /// this line, making lines easier to handle in the parser etc and to make it still compatible with the old systems
    token_slice tokens = {};

    /// @function `update_tokens`
    /// @brief Updates all token slices in the passed-in lines where `start` is the iterator at whichthe first line starts at
    ///
    /// @param `lines` The lines whose slices to update
    /// @param `start` The start iterator where the lines will start at
    static void update_tokens(std::vector<Line> &lines, const token_list::iterator start) {
        size_t pos = 0;
        for (auto &line : lines) {
            pos += line.offset;
            line.tokens = {start + pos, start + pos + line.len};
            pos += line.len;
        }
    }
};
