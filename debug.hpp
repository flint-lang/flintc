#ifndef __DEBUG_HPP__
#define __DEBUG_HPP__

#include "./lexer/token_context.hpp"
#include "./lexer/lexer.hpp"

#include <vector>
#include <iostream>

namespace Debug {
    static void print_token_context_vector(const std::vector<TokenContext> &tokens) {
        for(const TokenContext &tc : tokens) {
            std::string name = get_token_name(tc.type);

            std::string type_container(30, ' ');
            std::string type = " | Type: '" + name + "' => " + std::to_string(static_cast<int>(tc.type));
            type_container.replace(0, type.length(), type);

            std::cout << "Line: " << tc.line << type_container << " | Lexme: " << tc.lexme << "\n";
        }
    }
}

#endif
