#include "linearizer/linearizer.hpp"
#include "lexer/token.hpp"
#include "profiler.hpp"
#include "types.hpp"

#include <cstddef>

std::optional<std::vector<Line>> Linearizer::linearize(const Hash &file_hash, token_list &source) {
    PROFILE_CUMULATIVE("Linearizer::linearize");
    std::vector<Line> physical_lines;
    auto current_line_start = source.begin();
    unsigned int current_indent_lvl = 0;

    // A logical line is everything from the start of a line up to a semicolon. Line-continuation is only possible if the next line has a
    // higher indentation level than the "last" line. We start by regularly collecting physical lines.
    bool ends_with_colon = false;
    for (auto it = source.begin(); it != source.end();) {
        if (it == current_line_start) {
            // Skip all the \t tokens but remember the indentation depth
            if (current_indent_lvl == 0) {
                while (it != source.end()) {
                    if (it->token != TOK_INDENT) {
                        break;
                    }
                    current_indent_lvl++;
                    ++it;
                }
            }
            ends_with_colon = current_indent_lvl == 0 //
                && (it->token == TOK_DEF              //
                       || it->token == TOK_FUNC       //
                       || it->token == TOK_TEST       //
                       || it->token == TOK_DATA       //
                       || it->token == TOK_ENUM       //
                       || it->token == TOK_VARIANT    //
                       || it->token == TOK_ERROR      //
                       || it->token == TOK_INTERFACE  //
                       || it->token == TOK_OBJECT     //
                   );
        }
        if (current_indent_lvl == 0 && ends_with_colon) {
            if (it->token == TOK_COLON) {
                ++it;
                token_slice current_line = {current_line_start, it};
                physical_lines.emplace_back(current_indent_lvl, current_line);
                if (it->token == TOK_EOL) [[likely]] {
                    ++it;
                }
                current_line_start = it;
                current_indent_lvl = std::prev(it)->line == it->line;
                continue;
            }
            ++it;
            continue;
        }
        if (it->token == TOK_EOL) {
            token_slice current_line = {current_line_start, it};
            physical_lines.emplace_back(current_indent_lvl, current_line);
            ++it;
            current_line_start = it;
            current_indent_lvl = 0;
            continue;
        } else if (it->token == TOK_EOF) {
            if (current_line_start == it) {
                break;
            }
            token_slice current_line = {current_line_start, it};
            physical_lines.emplace_back(current_indent_lvl, current_line);
            break;
        }
        ++it;
    }
    if (physical_lines.empty()) {
        return physical_lines;
    }

    /// @enum `BodyMode`
    /// @brief A small enum to control the linearization mode we are currently in
    enum class BodyMode {
        // : terminated headers with nested bodies
        DEFINITION,
        // Until ; or : and applies regular unterminated-line indentation-checks
        STATEMENT,
        // ;-terminated with no unterminated-line indentation-checks, also skips over : tokens
        UNTIL_SEMICOLON,
    };

    // Now we can iterate all indented lines and create their respective logical lines from it
    bool is_object = false;
    BodyMode mode = BodyMode::DEFINITION;
    auto line = physical_lines.begin();
    token_list::iterator line_start = line->tokens.first;
    token_list::iterator line_end = line->tokens.first;
    std::vector<Line> logical_lines;
    while (line != physical_lines.end()) {
        // Remove all leading or trailing EOL and TOK_INDENT tokens
        while (std::prev(line->tokens.second)->token == TOK_INDENT || std::prev(line->tokens.second)->token == TOK_EOL) {
            --(line->tokens.second);
        }
        while (line->tokens.first->token == TOK_INDENT || line->tokens.first->token == TOK_EOL) {
            ++(line->tokens.first);
        }

        // Check if we are at a definition line, skip all one-liner definitions and set `is_object` and `mode` accordingly + skip the line
        if (line->indent_lvl == 0) {
            if (line->tokens.first->token == TOK_USE             //
                || line->tokens.first->token == TOK_TYPE_KEYWORD //
                || line->tokens.first->token == TOK_ANNOTATION   //
                || line->tokens.first->token == TOK_OPAQUE       //
                || line->tokens.first->token == TOK_EXTERN       //
            ) {
                logical_lines.emplace_back(*line);
                ++line;
                line_start = line->tokens.first;
                line_end = line->tokens.first;
                continue;
            }
            Token definition_kind = line->tokens.first->token;
            if (definition_kind == TOK_CONST || definition_kind == TOK_SHARED) {
                definition_kind = (line->tokens.first + 1)->token;
            }
            is_object = false;
            switch (definition_kind) {
                default:
                    mode = BodyMode::STATEMENT;
                    break;
                case TOK_OBJECT:
                    is_object = true;
                    [[fallthrough]];
                case TOK_DATA:
                case TOK_ENUM:
                case TOK_VARIANT:
                case TOK_ERROR:
                case TOK_INTERFACE:
                    mode = BodyMode::UNTIL_SEMICOLON;
                    break;
                case TOK_DEF:
                case TOK_TEST:
                    mode = BodyMode::STATEMENT;
                    break;
                case TOK_FUNC:
                    mode = BodyMode::DEFINITION;
                    break;
            }
            logical_lines.emplace_back(*line);
            ++line;
            line_start = line->tokens.first;
            line_end = line->tokens.first;
            continue;
        }

        if (mode == BodyMode::STATEMENT || line->indent_lvl > 1) {
            // We iterate through all tokens within the logical line
            size_t indentation_offset = 0;
            while (line_end != line->tokens.second) {
                const bool is_colon = line_end->token == TOK_COLON;
                if (line_end->token == TOK_SEMICOLON || is_colon) {
                    ++line_end;
                    const token_slice logical_line = {line_start, line_end};
                    logical_lines.emplace_back(line->indent_lvl + indentation_offset, logical_line);
                    line_start = line_end;
                    if (is_colon) {
                        indentation_offset++;
                    }
                    continue;
                }
                ++line_end;
            }
            if (line_start != line_end) {
                // Line not finished, needs to continue in the next line. We now check indentation level of the next line to check
                // whether we are allowed to continue on in the next line
                if (line + 1 == physical_lines.end() || (line + 1)->indent_lvl <= line->indent_lvl) {
                    THROW_ERR(ErrMissingSemicolon, ERR_PARSING, file_hash, token_slice{line_start, line_end});
                    return std::nullopt;
                }
                line->tokens.second = (line + 1)->tokens.second;
                line = std::prev(physical_lines.erase(line + 1));
                continue;
            }
        } else {
            if (is_object && line->tokens.first->token == TOK_DEF) {
                mode = BodyMode::DEFINITION;
                is_object = false;
            }
            switch (mode) {
                case BodyMode::STATEMENT:
                    break;
                case BodyMode::UNTIL_SEMICOLON: {
                    while (line_end->token != TOK_SEMICOLON) {
                        if (line_end == line->tokens.second) {
                            if (line + 1 == physical_lines.end()) {
                                THROW_ERR(ErrMissingSemicolon, ERR_PARSING, file_hash, line->tokens);
                                return std::nullopt;
                            }
                            line->tokens.second = (line + 1)->tokens.second;
                            line = std::prev(physical_lines.erase(line + 1));
                        }
                        ++line_end;
                    }
                    ++line_end;
                    logical_lines.emplace_back(line->indent_lvl, token_slice{line_start, line_end});
                    if (line_end->token == TOK_EOL) {
                        ++line;
                        line_end = line->tokens.first;
                    }
                    line_start = line_end;
                    continue;
                }
                case BodyMode::DEFINITION: {
                    // Merge all tokens across multiple lines until we find a colon symbol
                    while (line_end->token != TOK_COLON) {
                        if (line_end == line->tokens.second) {
                            if (line + 1 == physical_lines.end()) {
                                THROW_ERR(ErrMissingColon, ERR_PARSING, file_hash, line->tokens);
                                return std::nullopt;
                            }
                            line->tokens.second = (line + 1)->tokens.second;
                            line = std::prev(physical_lines.erase(line + 1));
                        }
                        ++line_end;
                    }
                    ++line_end;
                    logical_lines.emplace_back(line->indent_lvl, token_slice{line_start, line_end});
                    if (line_end->token == TOK_EOL) {
                        ++line;
                        line_end = line->tokens.first;
                    }
                    line_start = line_end;
                    continue;
                }
            }
        }
        if (line_start->token == TOK_EOL) {
            line_start++;
        }
        ++line;
        line_start = line->tokens.first;
        line_end = line->tokens.first;
    }

    // Now remove all TOK_INDENT and TOK_EOL tokens which are contained within the logical lines
    remove_indent_eol_tokens(source, logical_lines);

    // Special-case to merge 3 logical lines into one line for c-style for loops
    line = logical_lines.begin();
    while (line != logical_lines.end()) {
        if (line->tokens.first->token == TOK_FOR && std::prev(line->tokens.second)->token == TOK_SEMICOLON) {
            ASSERT(line + 2 < logical_lines.end());
            line->tokens.second = (line + 2)->tokens.second;
            logical_lines.erase(line + 1);
            logical_lines.erase(line + 1);
        }
        ++line;
    }

    // Because of the new line parser, something like
    //     for i64 i = 0;
    //     i < 3;
    //     i++: print($"i = {i}\n");
    // is totally valid code, as these three logical lines are merged into one. This means that we need to do the deletion of
    // TOK_INDENT and TOK_EOL again after the lines have been merged or otherwise these tokens would stay end up within the line
    remove_indent_eol_tokens(source, logical_lines);
    return logical_lines;
}

void Linearizer::remove_indent_eol_tokens(token_list &source, std::vector<Line> &lines) {
    PROFILE_CUMULATIVE("Linearizer::remove_indent_eol_tokens");
    if (lines.empty() || source.empty()) {
        return;
    }

    // Mark every INDENT/EOL token which lies inside of one of the logical lines
    std::vector<bool> to_delete(source.size(), false);
    for (const auto &line : lines) {
        for (auto it = line.tokens.first; it != line.tokens.second; ++it) {
            if (it->token == TOK_INDENT || it->token == TOK_EOL) {
                to_delete[static_cast<std::size_t>(it - source.begin())] = true;
            }
        }
    }

    // Compact the source token list in a single pass while recording the old -> new index mapping. For deleted positions the mapping
    // points at the insertion point of the next surviving token, which yields the correct new boundary for exclusive end iterators
    // that happen to point at a removed token.
    std::vector<std::size_t> new_index(source.size() + 1, 0);
    std::size_t write_idx = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        new_index[i] = write_idx;
        if (to_delete[i]) {
            continue;
        }
        if (write_idx != i) {
            source[write_idx] = std::move(source[i]);
        }
        ++write_idx;
    }
    new_index[source.size()] = write_idx;

    // Adjust every logical line range to its new position
    for (auto &line : lines) {
        line.tokens.first = source.begin() + new_index[static_cast<std::size_t>(line.tokens.first - source.begin())];
        line.tokens.second = source.begin() + new_index[static_cast<std::size_t>(line.tokens.second - source.begin())];
    }

    // Drop the moved-from tail. `TokenContext` is not default-constructible, so `resize` cannot be used here.
    source.erase(source.begin() + static_cast<std::ptrdiff_t>(write_idx), source.end());
}
