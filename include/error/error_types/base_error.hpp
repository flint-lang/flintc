#ifndef __BASE_ERROR_HPP__
#define __BASE_ERROR_HPP__

#include "colors.hpp"

#include <sstream>
#include <string>

class BaseError {
  public:
    [[nodiscard]]
    virtual std::string to_string() const {
        std::ostringstream oss;
        oss << RED << "Error" << DEFAULT << ": " << message << " at " << file << ":" << line << ":" << column;
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
    BaseError(std::string file, int line, int column, std::string message) :
        file(std::move(file)),
        line(line),
        column(column),
        message(std::move(message)) {}

    std::string file;
    int line;
    int column;
    std::string message;
};

#endif
