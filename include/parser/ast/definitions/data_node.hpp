#ifndef __DATA_NODE_HPP__
#define __DATA_NODE_HPP__

#include "../ast_node.hpp"

#include <string>
#include <utility>
#include <vector>

/// DataNode
///     Represents data definitions
class DataNode : public ASTNode {
  public:
    DataNode() = default;
    explicit DataNode(bool &is_shared, bool &is_immutable, bool &is_aligned, std::string &name,
        std::vector<std::pair<std::string, std::string>> &fields, std::vector<std::pair<std::string, std::string>> &default_values,
        std::vector<std::string> &order) :
        is_shared(is_shared),
        is_immutable(is_immutable),
        is_aligned(is_aligned),
        name(name),
        fields(std::move(fields)),
        default_values(std::move(default_values)),
        order(std::move(order)) {}

    /// is_shared
    ///     Determines whether the data is shared between multiple entities
    bool is_shared{false};
    /// is_immutable
    ///     Determines whether the data is immutable. Immutable data is automatically shared too.
    bool is_immutable{false};
    /// is_aligned
    ///     Determines whether the data is aligned to cache-lines
    bool is_aligned{false};
    /// name
    ///     The name of the data module
    std::string name;
    /// fields
    ///     The fields types and names
    std::vector<std::pair<std::string, std::string>> fields;
    /// default_values
    ///     The default values for the variables, where the name of the field is the first element in the pair, and its
    ///     default value the second value
    std::vector<std::pair<std::string, std::string>> default_values;
    /// order
    ///     The order of the dada fields in the constructor
    std::vector<std::string> order;
};

#endif
