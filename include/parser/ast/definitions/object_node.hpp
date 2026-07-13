#pragma once

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/definitions/func_node.hpp"
#include "parser/object_dispatch_graph.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @class `ObjectNode`
/// @brief Represents objects and their func / data relationships
class ObjectNode : public DefinitionNode {
  public:
    struct ParentObject {
        std::shared_ptr<Type> type;
        std::string accessor_name;
        size_t line;
        size_t column;
    };

    explicit ObjectNode(                                //
        const Hash &file_hash,                          //
        const unsigned int line,                        //
        const unsigned int column,                      //
        const unsigned int length,                      //
        const std::string &name,                        //
        const std::vector<FunctionNode *> &functions,   //
        const std::vector<ParentObject> &parent_objects //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        name(name),
        functions(functions),
        parent_objects(parent_objects) {}

    Variation get_variation() const override {
        return Variation::OBJECT;
    }

    // destructor
    ~ObjectNode() override = default;
    // copy operations - disabled due to unique_ptr member
    ObjectNode(const ObjectNode &) = delete;
    ObjectNode &operator=(const ObjectNode &) = delete;
    // move operations
    ObjectNode(ObjectNode &&) = default;
    ObjectNode &operator=(ObjectNode &&) = default;

    /// @var `name`
    /// @brief The name of the object
    std::string name;

    /// @var `data_modules`
    /// @brief The list of data modules defined inside the object and an optional accessor for that data to be used in free-floating
    /// functions or the constructor of the object
    std::vector<std::pair<DataNode *, std::optional<std::string>>> data_modules;

    /// @var `func_modules`
    /// @brief The list of func modules used inside the object
    std::vector<FuncNode *> func_modules;

    /// @var `functions`
    /// @brief A list of all functions defined as free-floating functions within this object definition
    std::vector<FunctionNode *> functions;

    /// @var `parent_objects`
    /// @brief The parent objects, whose data and func modules and link modules will be used.
    std::vector<ParentObject> parent_objects;

    /// @var `constructor_order`
    /// @brief The order of the data modules in which they have to be constructed
    std::vector<size_t> constructor_order;

    /// @var `edg`
    /// @brief The object dispatch graph is a simple graph of function IDs built through all link directives of this object / it's parents
    ObjectDispatchGraph edg;
};
