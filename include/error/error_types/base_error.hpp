#ifndef __BASE_ERROR_HPP__
#define __BASE_ERROR_HPP__

#include "colors.hpp"
#include "error/error_type.hpp"
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
    std::string get_token_string(const token_list &tokens, const std::vector<Token> &ignore_tokens) const {
        std::string token_str;
        for (auto it = tokens.begin(); it != tokens.end(); it++) {
            if (std::find(ignore_tokens.begin(), ignore_tokens.end(), it->type) != ignore_tokens.end()) {
                continue;
            }
            switch (it->type) {
                case TOK_STR_VALUE:
                    token_str += "\"" + it->lexme + "\" ";
                    break;
                case TOK_CHAR_VALUE:
                    token_str += "'" + it->lexme + "' ";
                default:
                    token_str += it->lexme + " ";
                    break;
            }
        }
        return trim_right(token_str);
    }
};

#endif
