#pragma once

#include "error/error.hpp"
#include "parser/ast/ast_node.hpp"
#include "parser/ast/scope.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/// @class `TestNode`
/// @brief Represents function definitions
class TestNode : public ASTNode {
  public:
    explicit TestNode(const std::string &file_name, const std::string &name, std::unique_ptr<Scope> &scope) :
        file_name(file_name),
        name(name),
        scope(std::move(scope)) {
        check_test_name(this->file_name, this->name);
        this->test_id = get_next_test_id();
    }

    // empty constructor
    TestNode() = default;
    // deconstructor
    ~TestNode() override = default;
    // copy operations - disabled due to unique_ptr member
    TestNode(const TestNode &) = delete;
    TestNode &operator=(const TestNode &) = delete;
    // move operations
    TestNode(TestNode &&) = default;
    TestNode &operator=(TestNode &&) = default;

    /// @var `file_name`
    /// @brief The name of the file the test is contained in
    std::string file_name;

    /// @var `name`
    /// @brief The name of the test
    std::string name;

    /// @var `body`
    /// @brief The body of the function containing all statements
    std::unique_ptr<Scope> scope;

    /// @var `test_id`
    /// @brief The ID of the test
    unsigned int test_id;

  private:
    /// @function `check_test_name`
    /// @brief Checks if a given test name already exists in the given file, if it does, this function throws up
    ///
    /// @param `file_name` The name of the file to check for the test names
    /// @param `name` The name of the test to check for
    static void check_test_name(const std::string &file_name, const std::string &name) {
        // This map is only used to check whether the given file already contains a given test name
        static std::unordered_map<std::string, std::vector<std::string>> test_names;
        static std::mutex test_names_mutex;
        std::lock_guard<std::mutex> lock(test_names_mutex);
        if (test_names.find(file_name) == test_names.end()) {
            test_names[file_name] = {name};
            return;
        }
        std::vector<std::string> &tests = test_names.at(file_name);
        if (std::find(tests.begin(), tests.end(), name) != tests.end()) {
            // Test already exists in this file
            THROW_BASIC_ERR(ERR_PARSING);
            return;
        }
        tests.emplace_back(name);
    }

    /// @function `get_next_test_id`
    /// @brief Returns the next test id. Ensures that each test gets its own id for the lifetime of the program
    ///
    /// @return `unsigned int` The next unique test id
    static inline unsigned int get_next_test_id() {
        static unsigned int test_id = 0;
        return test_id++;
    }
};
