#ifndef __RESULT_HPP__
#define __RESULT_HPP__

#include "parser/signature.hpp"

#include <codecvt>
#include <locale>
#include <string>

static const std::string RED = "\033[31m";
static const std::string GREEN = "\033[32m";
static const std::string YELLOW = "\033[33m";
static const std::string BLUE = "\033[34m";
static const std::string WHITE = "\033[37m";
static const std::string DEFAULT = "\033[0m";

class TestResult {
  private:
    static std::wstring convert(const std::string &str) {
        static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        return converter.from_bytes(str);
    }

    std::string message{WHITE};
    unsigned int count = 0;

  public:
    [[nodiscard]]
    std::string get_message() const {
        return message;
    }

    [[nodiscard]]
    unsigned int get_count() const {
        return count;
    }

    void increment() {
        count++;
    }

    void add_result(TestResult &result) {
        append(result.get_message());
        count += result.get_count();
    }

    void add_result_if(TestResult &result) {
        if (result.get_count() > 0) {
            add_result(result);
        }
    }

    void append(const std::string &text) {
        message.append(text);
    }

    void append_test_name(const std::string &name, const bool is_section_header) {
        if (is_section_header) {
            append(name + "\n");
        } else {
            append(name + " ");
        }
    }

    void ok_or_not(bool was_ok) {
        if (was_ok) {
            append(GREEN + "OK" + WHITE + "\n");
        } else {
            append(RED + "FAILED" + WHITE + "\n");
        }
    }

    void print_token_stringified(const std::vector<TokenContext> &tokens) {
        append(Signature::stringify(tokens) + "\n");
    }

    void print_regex_string(const Signature::signature &signature) {
        append(Signature::get_regex_string(signature) + "\n");
    }

    void print_debug(const std::string &str) {
        append("\t" + str + "\t...");
    }
};

#endif
