#pragma once

#include "error/error.hpp"

#include <optional>
#include <unordered_map>

/// @struct `ObjectDispatchGraph`
/// @brief An object dispatch graph is a rather simple function-id mapper and resolver for object link and hook chains
struct ObjectDispatchGraph {
    /// @struct `Node`
    /// @brief A single node of the object dispatch graph. Every single node, being a function in the EDG, can only point to *one* other
    /// function (the link), so `F1::function -> F2::function`. A single function can *never* point to more than just one other function,
    /// but every function can be pointed to as often as wanted. The EDG stars with N "start" nodes, N functions defined in the object
    /// through it's included func modules. These N "start" nodes map to M "end nodes" through atmost N graphs. Every
    struct Node {
        /// @var `fn_id`
        /// @brief The function pointer of this node
        const FunctionNode *fn;

        // @var `next`
        // @brief The next function in this graph, nullopt if this function does not point to any other function
        std::optional<const FunctionNode *> next;
    };

    ObjectDispatchGraph() = default;
    ObjectDispatchGraph(std::unordered_map<const FunctionNode *, Node> nodes) :
        nodes(nodes) {}

    /// @var `nodes`
    /// @brief The node_mapping is a simple map which maps the function ID to it's respective node. This is the owner of all nodes of this
    /// EDG.
    std::unordered_map<const FunctionNode *, Node> nodes;

    /// @function `add_fn`
    /// @brief Adds a function, e.g. a new node, to the EDG. Does nothing if the function is already present in the EDG
    ///
    /// @param `fn` The function to add to the EDG
    void add_fn(const FunctionNode *fn) {
        if (nodes.find(fn) == nodes.end()) {
            nodes[fn] = Node{.fn = fn, .next = std::nullopt};
        }
    }

    /// @function `add_mapping`
    /// @brief Adds a mapping of a source function to a dest function and checks whether the graph would form circles
    ///
    /// @param `src` The source function to map to the dest function
    /// @param `dest` The dest function being mapped to from the src function
    /// @return `bool` Whether adding the mapping failed
    [[nodiscard]] bool add_mapping(const FunctionNode *src, const FunctionNode *dest) {
        if (src == dest) {
            // Cannot map src to itself
            THROW_BASIC_ERR(ERR_PARSING);
            return true;
        }
        if (nodes.find(src) == nodes.end()) {
            // Src ID not present in the EDG
            THROW_BASIC_ERR(ERR_PARSING);
            return true;
        }
        if (nodes.find(dest) == nodes.end()) {
            // Dest ID not present in the EDG
            THROW_BASIC_ERR(ERR_PARSING);
            return true;
        }
        Node &src_node = nodes.at(src);
        Node dest_node = nodes.at(dest);
        if (src_node.next.has_value()) {
            if (get_mapped_fn(src) == dest) {
                // Nothing to do, mapping already exists
                return false;
            }
            // Src ID is already mapped to other function than 'dest'. Mapping one 'src' to multiple 'dest' is not allowed
            THROW_BASIC_ERR(ERR_PARSING);
            return true;
        }
        while (dest_node.next.has_value()) {
            if (dest_node.next.value() == src) {
                // Graph would form a loop back to the src
                THROW_BASIC_ERR(ERR_PARSING);
                return true;
            }
            dest_node = nodes.at(dest_node.next.value());
        }
        // Everything is okay, we can add the src->dest mapping
        src_node.next = dest;
        return false;
    }

    /// @function `get_mapped_fn`
    /// @brief Returns the function of the function the given source function ultimately maps to. Returns nullopt if the given source
    /// function is not part of the EDG.
    ///
    /// @param `src` The source function to get the mapped-to function from
    /// @return `std::optional<const FunctionNode *>` The mapped-to function, nullopt of `src` is not part of the EDG
    std::optional<const FunctionNode *> get_mapped_fn(const FunctionNode *src) const {
        if (nodes.find(src) == nodes.end()) {
            return std::nullopt;
        }
        Node node = nodes.at(src);
        while (node.next.has_value()) {
            node = nodes.at(node.next.value());
        }
        return node.fn;
    }

    /// @function `get_all_mappings`
    /// @brief Returns all src->dest function mappings of this EDG
    ///
    /// @return `std::unordered_map<const FunctionNode *, const FunctionNode *>` A mapping of all functions this EDG maps from->to
    std::unordered_map<const FunctionNode *, const FunctionNode *> get_all_mappings() const {
        std::unordered_map<const FunctionNode *, const FunctionNode *> mapping;
        for (const auto &[id, node] : nodes) {
            if (!node.next.has_value()) {
                continue;
            }
            Node next = nodes.at(node.next.value());
            while (next.next.has_value()) {
                next = nodes.at(next.next.value());
            }
            mapping[id] = next.fn;
        }
        return mapping;
    }
};
