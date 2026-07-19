#pragma once

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/definitions/func_node.hpp"

#include <string>
#include <utility>
#include <vector>

/// @class `ObjectNode`
/// @brief Represents objects and their func / data relationships
class ObjectNode : public DefinitionNode {
  public:
    struct ImplementedInterface {
        /// @var `type`
        /// @brief The type of the implemented interface
        std::shared_ptr<Type> type;

        /// @var `pos`
        /// @brief The source location of the implemented interface within the implements clausel
        PosTriple pos;

        /// @var `mapping`
        /// @brief The mapping of this interface, where the "key" is the virtual interface function and the "value" is the function it got
        /// linked to in the object
        std::unordered_map<FunctionNode *, FunctionNode *> mapping = {};
    };

    explicit ObjectNode(                              //
        const Hash &file_hash,                        //
        const unsigned int line,                      //
        const unsigned int column,                    //
        const unsigned int length,                    //
        const std::string &name,                      //
        const std::vector<FunctionNode *> &functions, //
        std::vector<ImplementedInterface> interfaces  //
        ) :
        DefinitionNode(file_hash, line, column, length, {}),
        name(name),
        functions(functions),
        interfaces(interfaces) {}

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

    /// @var `interfaces`
    /// @brief The list of interfaces implemented by the object
    std::vector<ImplementedInterface> interfaces;

    /// @var `constructor_order`
    /// @brief The order of the data modules in which they have to be constructed
    std::vector<size_t> constructor_order;
};
