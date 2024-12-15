#include "./linker.hpp"
#include "../error/error.hpp"
#include "../error/error_type.hpp"
#include "../lexer/lexer.hpp"
#include "../types.hpp"

#include <optional>
#include <string>
#include <vector>

/// link
///     Goes through all the imports in the file and scans all the imported files, and links them again, and so on.
///     Replaces all the 'use "..."' tokens inside the tokens list with the content of the imported file recursively
void Linker::link(token_list &tokens, std::string cwd) {
    // check if there is any imports present in the current file
    std::optional<token_list> paths_maybe = get_imports_from(tokens);
    // only continue when the list of tokens contains any imports
    if (!paths_maybe.has_value()) {
        return;
    }
    token_list paths = paths_maybe.value();

    for (const TokenContext &path : paths) {
        link_next_use(tokens, path);
    }
}

/// check_for_imports
///     Goes through a given list of tokens and returns all the paths of included files inside this token list, if there
///     are any inclusions at all. When no inclusion ('use') is present, None is returned.
std::optional<token_list> Linker::get_imports_from(const token_list &tokens) {
    token_list paths;
    bool last_was_use = false;
    for (const TokenContext &token : tokens) {
        if (token.type == TOK_USE) {
            last_was_use = true;
            continue;
        }
        if (last_was_use) {
            if (token.type == TOK_STR_VALUE) {
                last_was_use = false;
                paths.push_back(token);
            } else {
                throw_err(ERR_EXPECTED_PATH_AFTER_USE);
            }
        }
    }

    if (paths.empty()) {
        return std::nullopt;
    }
    return paths;
}

/// link_next_use
///     Goes to the line of the import, removes the import (the provided use) from the token list and inserts all the
///     scanned tokens of the file to insert into the token list
void Linker::link_next_use(token_list &source_tokens, const TokenContext &use_path) {
    // first, lets scan the imported file
    std::string path = use_path.lexme;
    Lexer lexer = Lexer(path);
    token_list file_tokens = lexer.scan();
    // Check if the scanned file has imports, resolve those first
    Linker::link(file_tokens, path);
    // remove the 'use "...";' from the source tokens list, then insert the new file tokens into the source tokens list.
    for (auto token = source_tokens.begin(); token != source_tokens.end();) {
        if (token->line == use_path.line) {
            if (token->lexme == use_path.lexme) {

                // erasing use
                source_tokens.erase(token - 1);
                // erasing "..."
                source_tokens.erase(token - 1);
                // erasing ;
                source_tokens.erase(token - 1);
                // inserting all the new tokens
                source_tokens.insert(token - 1, file_tokens.begin(), file_tokens.end());
                // exiting the loop
                break;
            }
        }
        token += 1;
    }
}
