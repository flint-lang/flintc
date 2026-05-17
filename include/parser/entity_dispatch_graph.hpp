#pragma once

#include "error/error.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>

/// @struct `EntityDispatchGraph`
/// @brief An entity dispatch graph is a rather simple function-id mapper and resolver for entity link and hook chains
struct EntityDispatchGraph {
    /// @struct `Node`
    /// @brief A single node of the entity dispatch graph. Every single node, being a function in the EDG, can only point to *one* other
    /// function (the link), so `F1::function -> F2::function`. A single function can *never* point to more than just one other function,
    /// but every function can be pointed to as often as wanted. The EDG stars with N "start" nodes, N functions defined in the entity
    /// through it's included func modules. These N "start" nodes map to M "end nodes" through atmost N graphs. Every
    struct Node {
        /// @var `fn_id`
        /// @brief The function ID of this node
        size_t fn_id;

        // @var `next`
        // @brief The next ID in this graph, nullopt if this function ID does not point to any other ID
        std::optional<size_t> next;
    };

    EntityDispatchGraph() = default;
    EntityDispatchGraph(std::unordered_map<size_t, Node> nodes) :
        nodes(nodes) {}

    /// @var `nodes`
    /// @brief The node_mapping is a simple map which maps the function ID to it's respective node. This is the owner of all nodes of this
    /// EDG.
    std::unordered_map<size_t, Node> nodes;

    /// @function `add_id`
    /// @brief Adds a function ID, e.g. a new node to the EDG. Does nothing if the ID is already present in the EDG
    ///
    /// @param `id` The function ID to add to the EDG
    void add_id(const size_t id) {
        if (nodes.find(id) == nodes.end()) {
            nodes[id] = Node{.fn_id = id, .next = std::nullopt};
        }
    }

    /// @function `add_mapping`
    /// @brief Adds a mapping of a source function ID to a dest function ID and checks whether the graph would form circles
    ///
    /// @param `src` The source function ID to map to the dest function ID
    /// @param `dest` The dest function ID being mapped to from the src function ID
    /// @return `bool` Whether adding the mapping failed
    [[nodiscard]] bool add_mapping(const size_t src, const size_t dest) {
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
            // Src ID is already mapped to other function
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
    /// @brief Returns the function ID of the function the given source function ID ultimately maps to. Returns nullopt if the given source
    /// function ID is not part of the EDG.
    ///
    /// @param `src` The source function ID to get the mapped-to function ID from
    /// @return `std::optional<size_t>` The mapped-to function ID, nullopt of `src` is not part of the EDG
    std::optional<size_t> get_mapped_fn(const size_t src) const {
        if (nodes.find(src) == nodes.end()) {
            return std::nullopt;
        }
        Node node = nodes.at(src);
        while (node.next.has_value()) {
            node = nodes.at(node.next.value());
        }
        return node.fn_id;
    }

    /// @function `get_all_mappings`
    /// @brief Returns all src->dest function ID mappings of this EDG
    ///
    /// @return `std::unordered_map<size_t, size_t>` A mapping of all function IDs this EDG maps from->to
    std::unordered_map<size_t, size_t> get_all_mappings() const {
        std::unordered_map<size_t, size_t> mapping;
        for (const auto &[id, node] : nodes) {
            if (!node.next.has_value()) {
                continue;
            }
            Node next = nodes.at(node.next.value());
            while (next.next.has_value()) {
                next = nodes.at(next.next.value());
            }
            mapping[id] = next.fn_id;
        }
        return mapping;
    }
};
