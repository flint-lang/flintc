#ifndef __BASE_ERROR_HPP__
#define __BASE_ERROR_HPP__

#include <sstream>
#include <string>

class BaseError {
  public:
    [[nodiscard]]
    virtual std::string to_string() const {
        std::ostringstream oss;
        oss << "Error: " << message << " at " << file << ":" << line << ":" << column;
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
    BaseError(std::string message, std::string file, int line, int column) :
        message(std::move(message)),
        file(std::move(file)),
        line(line),
        column(column) {}

    std::string message;
    std::string file;
    int line;
    int column;
};

#endif
