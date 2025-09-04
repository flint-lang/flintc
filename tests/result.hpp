#pragma once

#include "colors.hpp"
#include "debug.hpp"

#include <string>

class TestResult {
  private:
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

    void print_debug(const std::string &str) {
        append("\t" + str + "\t...");
    }
};

namespace Debug {
    /// print_tree_row
    ///
    static void print_tree_row(const std::vector<TreeType> &types, TestResult *result) {
        std::string addition;
        for (const TreeType &type : types) {
            addition = tree_blocks.at(type);
        }
        if (result != nullptr) {
            result->append(addition);
        } else {
            std::cout << addition;
        }
    }
} // namespace Debug