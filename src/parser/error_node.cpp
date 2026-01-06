#include "parser/ast/definitions/error_node.hpp"
#include "parser/ast/namespace.hpp"
#include "parser/type/error_set_type.hpp"
#include "resolver/resolver.hpp"

#include <algorithm>

std::optional<const ErrorNode *> ErrorNode::get_parent_node() const {
    if (parent_error == "anyerror") {
        return std::nullopt;
    }
    Namespace *file_namespace = Resolver::get_namespace_from_hash(file_hash);
    std::optional<std::shared_ptr<Type>> parent_type = file_namespace->get_type_from_str(parent_error);
    assert(parent_type.has_value());
    const auto *parent_set_type = parent_type.value()->as<ErrorSetType>();
    return parent_set_type->error_node;
}

unsigned int ErrorNode::get_value_count() const {
    std::optional<const ErrorNode *> parent_node = get_parent_node();
    if (parent_node.has_value()) {
        return values.size() + parent_node.value()->get_value_count();
    } else {
        return values.size();
    }
}

bool ErrorNode::is_parent_of(const ErrorNode *other) const {
    std::optional<const ErrorNode *> other_parent = other->get_parent_node();
    while (other_parent.has_value()) {
        if (this == other_parent.value()) {
            return true;
        }
        other_parent = other_parent.value()->get_parent_node();
    }
    return false;
}

std::optional<std::pair<unsigned int, std::string>> ErrorNode::get_id_msg_pair_of_value(const std::string &value) const {
    unsigned int offset = get_value_count() - values.size();
    const auto it = std::find(values.begin(), values.end(), value);
    if (it != values.end()) {
        unsigned int idx = std::distance(values.begin(), it);
        return std::make_pair(offset + idx, default_messages.at(idx));
    }
    std::optional<const ErrorNode *> parent_node = get_parent_node();
    if (!parent_node.has_value()) {
        // Value not part of any set
        return std::nullopt;
    }
    return parent_node.value()->get_id_msg_pair_of_value(value);
}
