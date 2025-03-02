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

  private:
    /// test_names
    ///     Static map containing all test names of all files
    static std::unordered_map<std::string, std::vector<std::string>> test_names;

    /// test_names_mutex
    ///     The mutex for the access of the test_names map, because parsing is multithreaded
    static std::mutex test_names_mutex;
};

#endif
