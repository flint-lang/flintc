#ifndef __DATA_NODE_HPP__
#define __DATA_NODE_HPP__

#include "../ast_node.hpp"

#include <string>

/// DataNode
///     Represents data definitions
class DataNode : public ASTNode {
    public:

    private:
        /// name
        ///     The name of the data module
        std::string name;
        /// fields
        ///     The fields types and names
        std::vector<std::pair<std::string, std::string>> fields;
        /// default_values
        ///     The default values for the variables, where the name of the field is the first element in the pair, and its default value the second value
        std::vector<std::pair<std::string, std::string>> default_values;
        /// order
        ///     The order of the dada fields in the constructor
        std::vector<std::string> order;
};

#endif
