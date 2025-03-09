#pragma once

#include "colors.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer.hpp"
#include "lexer/lexer_utils.hpp"
#include "lexer/token.hpp"
#include "types.hpp"

#include <algorithm>
#include <sstream>
#include <string>

class BaseError {
  public:
    [[nodiscard]]
    virtual std::string to_string() const {
        std::ostringstream oss;
        oss << RED << error_type_names.at(error_type) << DEFAULT << " at " << GREEN << file << ":" << line << ":" << column << DEFAULT
            << "\n -- ";
        return oss.str();
    }

    // destructor
    virtual ~BaseError() = default;
    // copy operators
    BaseError(const BaseError &) = default;
    BaseError &operator=(const BaseError &) = default;
    // move operators
    BaseError(BaseError &&) = default;
    BaseError &operator=(BaseError &&) = default;

  protected:
    BaseError(const ErrorType error_type, std::string file, int line, int column) :
        error_type(error_type),
        file(std::move(file)),
        line(line),
        column(column) {}

    ErrorType error_type;
    std::string file;
    int line;
    int column;

    [[nodiscard]]
    std::string trim_right(const std::string &str) const {
        size_t size = str.length();
        for (auto it = str.rbegin(); it != str.rend() && std::isspace(*it); ++it) {
            --size;
        }
        return str.substr(0, size);
    }

    [[nodiscard]]
    std::string get_token_string(const std::vector<Token> &tokens) const {
        std::ostringstream oss;
        for (auto it = tokens.begin(); it != tokens.end(); ++it) {
            if (it != tokens.begin()) {
                oss << " ";
            }
            oss << "'" << get_token_name(*it) << "'";
        }
        return oss.str();
    }

    [[nodiscard]]
    std::string get_token_string(const token_list &tokens, const std::vector<Token> &ignore_tokens) const {
        std::string token_str;
        for (auto it = tokens.begin(); it != tokens.end(); it++) {
            if (std::find(ignore_tokens.begin(), ignore_tokens.end(), it->type) != ignore_tokens.end()) {
                continue;
            }
            switch (it->type) {
                case TOK_STR_VALUE:
                    token_str += "\"" + it->lexme + "\"";
                    token_str += get_possible_space(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON});
                    break;
                case TOK_CHAR_VALUE:
                    token_str += "'" + it->lexme + "' ";
                    break;
                case TOK_IDENTIFIER:
                    token_str += it->lexme;
                    token_str += get_possible_space(tokens, it, {TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON});
                    break;
                case TOK_LEFT_PAREN:
                    token_str += it->lexme;
                    break;
                case TOK_INDENT:
                    token_str += std::string(Lexer::TAB_SIZE, ' ');
                    break;
                default:
                    token_str += it->lexme + get_possible_space(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON});
                    break;
            }
        }
        return trim_right(token_str);
    }

    [[nodiscard]]
    std::string get_function_signature_string(const std::string &function_name, const std::vector<std::string> &arg_types) const {
        std::stringstream oss;
        oss << function_name << "(";
        for (auto arg = arg_types.begin(); arg != arg_types.end(); ++arg) {
            oss << *arg << (arg != arg_types.begin() ? ", " : "");
        }
        oss << ")";
        return oss.str();
    }

    [[nodiscard]]
    std::string get_possible_space(                //
        const token_list &tokens,                  //
        const token_list::const_iterator iterator, //
        const std::vector<Token> &ignores          //
    ) const {
        if (iterator != std::prev(tokens.end()) && std::find(ignores.begin(), ignores.end(), std::next(iterator)->type) == ignores.end()) {
            return " ";
        }
        return "";
    }
};
