#include "error/error_types/base_error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/lexer_utils.hpp"
#include "resolver/resolver.hpp"

#include <algorithm>
#include <filesystem>

std::string BaseError::to_string() const {
    std::ostringstream oss;
    oss << RED << error_type_names.at(error_type) << DEFAULT << " at " << GREEN
        << std::filesystem::relative(Resolver::get_path(file) / file, std::filesystem::current_path()).string() << ":" << line << ":"
        << column << DEFAULT << "\n -- ";
    return oss.str();
}

std::string BaseError::trim_right(const std::string &str) {
    size_t size = str.length();
    for (auto it = str.rbegin(); it != str.rend() && std::isspace(*it); ++it) {
        --size;
    }
    return str.substr(0, size);
}

std::string BaseError::get_token_string(const std::vector<Token> &tokens) {
    std::ostringstream oss;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it != tokens.begin()) {
            oss << " ";
        }
        oss << "'" << get_token_name(*it) << "'";
    }
    return oss.str();
}

[[nodiscard]] std::string BaseError::get_token_string(const token_slice &tokens, const std::vector<Token> &ignore_tokens) {
    std::stringstream token_str;
    for (auto it = tokens.first; it != tokens.second; it++) {
        if (std::find(ignore_tokens.begin(), ignore_tokens.end(), it->token) != ignore_tokens.end()) {
            continue;
        }
        switch (it->token) {
            case TOK_EOF:
                continue;
            case TOK_TYPE:
                token_str << it->type->to_string();
                if (space_needed(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
            case TOK_STR_VALUE:
                token_str << "\"" + it->lexme + "\"";
                if (space_needed(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
            case TOK_CHAR_VALUE:
                token_str << "'" + it->lexme + "' ";
                break;
            case TOK_IDENTIFIER:
                token_str << it->lexme;
                if (space_needed(tokens, it, {TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
            case TOK_LEFT_PAREN:
                token_str << it->lexme;
                break;
            case TOK_INDENT:
                token_str << std::string(Lexer::TAB_SIZE, ' ');
                break;
            default:
                token_str << it->lexme;
                if (space_needed(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
        }
    }
    return trim_right(token_str.str());
}

std::string BaseError::get_function_signature_string(   //
    const std::string &function_name,                   //
    const std::vector<std::shared_ptr<Type>> &arg_types //
) {
    std::stringstream oss;
    oss << function_name << "(";
    for (auto arg = arg_types.begin(); arg != arg_types.end(); ++arg) {
        oss << (*arg)->to_string() << (arg != arg_types.begin() ? ", " : "");
    }
    oss << ")";
    return oss.str();
}

bool BaseError::space_needed(                  //
    const token_slice &tokens,                 //
    const token_list::const_iterator iterator, //
    const std::vector<Token> &ignores          //
) {
    return iterator != std::prev(tokens.second) && std::find(ignores.begin(), ignores.end(), std::next(iterator)->token) == ignores.end();
}
