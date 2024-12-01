#ifndef __LINKER_HPP__
#define __LINKER_HPP__

#include "../types.hpp"
#include "../lexer/token_context.hpp"

#include <string>
#include <optional>

class Linker {
public:
    static void link(token_list &tokens, std::string cwd);

private:
    static std::optional<token_list> get_imports_from(const token_list &tokens);

    static void link_next_use(token_list &source_tokens, const TokenContext &use_path);
};

#endif
