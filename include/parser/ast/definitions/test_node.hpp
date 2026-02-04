#pragma once

#include "parser/ast/definitions/definition_node.hpp"
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
class TestNode : public DefinitionNode {
  public:
    explicit TestNode(                                  //
        const Hash &file_hash,                          //
        const unsigned int line,                        //
        const unsigned int column,                      //
        const unsigned int length,                      //
        const std::vector<AnnotationNode> &annotations, //
        const std::string &name,                        //
        std::shared_ptr<Scope> &scope                   //
        ) :
        DefinitionNode(file_hash, line, column, length, annotations),
        name(name),
        scope(std::move(scope)) {}

    /// @var `consumable_annotations`
    /// @brief The annotations consumable by this statement node
    static const inline std::unordered_set<AnnotationKind> consumable_annotations = {
        AnnotationKind::TEST_OUTPUT_ALWAYS,
        AnnotationKind::TEST_PERFORMANCE,
        AnnotationKind::TEST_SHOULD_FAIL,
    };

    Variation get_variation() const override {
        return Variation::TEST;
    }

    // deconstructor
    ~TestNode() override = default;
    // copy operations - disabled due to unique_ptr member
    TestNode(const TestNode &) = delete;
    TestNode &operator=(const TestNode &) = delete;
    // move operations
    TestNode(TestNode &&) = default;
    TestNode &operator=(TestNode &&) = default;

    /// @var `name`
    /// @brief The name of the test
    std::string name;

    /// @var `body`
    /// @brief The body of the function containing all statements
    std::shared_ptr<Scope> scope;

    /// @var `test_id`
    /// @brief The ID of the test
    unsigned int test_id = get_next_test_id();

    /// @function `check_test_name`
    /// @brief Checks if a given test name already exists in the given file, if it does, this function throws up
    ///
    /// @param `file_name` The name of the file to check for the test names
    /// @param `name` The name of the test to check for
    /// @return `bool` Whether the test name was okay
    [[nodiscard]] static bool check_test_name(const std::string &file_name, const std::string &name) {
        std::lock_guard<std::mutex> lock(test_names_mutex);
        if (test_names.find(file_name) == test_names.end()) {
            test_names[file_name] = {name};
            return true;
        }
        std::vector<std::string> &tests = test_names.at(file_name);
        if (std::find(tests.begin(), tests.end(), name) != tests.end()) {
            return false;
        }
        tests.emplace_back(name);
        return true;
    }

    /// @function `clear_test_namees`
    /// @brief Clears all the test names
    static void clear_test_names() {
        std::lock_guard<std::mutex> lock(test_names_mutex);
        test_names.clear();
    }

  private:
    /// @var `test_names`
    /// @brief A map which maps each file name to a list of tests defined inside the file
    static inline std::unordered_map<std::string, std::vector<std::string>> test_names;

    /// @var `test_names_mutex`
    /// @brief Mutex for the test names map variable
    static inline std::mutex test_names_mutex;

    /// @function `get_next_test_id`
    /// @brief Returns the next test id. Ensures that each test gets its own id for the lifetime of the program
    ///
    /// @return `unsigned int` The next unique test id
    static inline unsigned int get_next_test_id() {
        static unsigned int test_id = 0;
        static std::mutex test_id_mutex;
        std::lock_guard<std::mutex> lock(test_id_mutex);
        return test_id++;
    }
};
