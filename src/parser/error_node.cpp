#include "parser/ast/definitions/error_node.hpp"
#include "parser/type/error_set_type.hpp"

#include <algorithm>

std::optional<const ErrorNode *> ErrorNode::get_parent_node() const {
    if (parent_error == "anyerror") {
        return std::nullopt;
    }
    std::optional<std::shared_ptr<Type>> parent_type = Type::get_type_from_str(parent_error);
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
