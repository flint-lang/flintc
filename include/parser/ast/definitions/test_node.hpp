#ifndef __TEST_NODE_HPP__
#define __TEST_NODE_HPP__

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

/// TestNode
///     Represents function definitions
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

    /// file_name
    ///     The name of the file the test is contained in
    std::string file_name;
    /// name
    ///     The name of the test
    std::string name;
    /// body
    ///     The body of the function containing all statements
    std::unique_ptr<Scope> scope;
    /// test_id
    ///     The ID of the test
    unsigned int test_id;

  private:
    /// check_test_name
    ///     Checks if a given test name already exists in the given file
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

    /// get_next_test_id
    ///     Returns the next test id. Ensures that each test gets its own id for the lifetime of the program
    static unsigned inline int get_next_test_id() {
        static unsigned int test_id = 0;
        return test_id++;
    }
};

#endif
