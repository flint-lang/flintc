#ifndef __BASE_ERROR_HPP__
#define __BASE_ERROR_HPP__

#include "colors.hpp"
#include "error/error_type.hpp"

#include <sstream>
#include <string>

class BaseError {
  public:
    [[nodiscard]]
    virtual std::string to_string() const {
        std::ostringstream oss;
        oss << RED << error_type_names.at(error_type) << DEFAULT << ": at " << GREEN << file << ":" << line << ":" << column << DEFAULT
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

    std::string trim_right(const std::string &str) const {
        size_t size = str.length();
        for (auto it = str.rbegin(); it != str.rend() && std::isspace(*it); ++it) {
            --size;
        }
        return str.substr(0, size);
    }
};

#endif
