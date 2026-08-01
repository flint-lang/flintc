#include "analyzer/analyzer.hpp"

#include "assert.hpp"
#include "lexer/builtins.hpp"
#include "parser/ast/definitions/error_node.hpp"
#include "parser/ast/definitions/func_node.hpp"
#include "parser/ast/definitions/interface_node.hpp"
#include "parser/ast/definitions/object_node.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/parser.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/interface_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/opaque_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/variant_type.hpp"
#include "parser/type/vector_type.hpp"
#include "profiler.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

bool Analyzer::Castability::resolve_comptime_type_of_expr(Parser &parser, std::unique_ptr<ExpressionNode> &expr,
    const std::optional<std::shared_ptr<Type>> &target_type) {
    const std::string &type_str = expr->type->to_string();
    if (type_str == "int") {
        if (target_type.has_value()) {
            expr->type = target_type.value();
        } else {
            expr->type = Type::get_primitive_type("i32");
        }
        return true;
    }
    if (type_str == "float") {
        if (target_type.has_value()) {
            expr->type = target_type.value();
        } else {
            expr->type = Type::get_primitive_type("f32");
        }
        return true;
    }
    if (target_type.has_value()) {
        return false;
    }
    if (type_str == "type.flint.str.lit") {
        expr->type = Type::get_primitive_type("str");
        return true;
    }
    if (expr->type->get_variation() != Type::Variation::GROUP) {
        return false;
    }
    // First check for any comptime types in the group and resolve them to their default types.
    // Then check if the group can turn into a vector, if it cannot turn into a vector then the result type will be a tuple.
    // Copy the type since we may modify it directly
    std::shared_ptr<GroupType> group_type = std::make_shared<GroupType>(*expr->type->as<GroupType>());
    bool contains_literal = false;
    for (auto &type : group_type->types) {
        const std::string element_type_str = type->to_string();
        if (element_type_str == "int") {
            type = Type::get_primitive_type("i32");
            contains_literal = true;
        } else if (element_type_str == "float") {
            type = Type::get_primitive_type("f32");
            contains_literal = true;
        } else if (element_type_str == "type.flint.str.lit") {
            type = Type::get_primitive_type("str");
            contains_literal = true;
        }
    }
    if (!contains_literal || expr->get_variation() != ExpressionNode::Variation::GROUP_EXPRESSION) {
        return false;
    }
    std::shared_ptr<Type> result_type = group_type;
    if (!parser.file_node_ptr->file_namespace->add_type(group_type)) {
        result_type = parser.file_node_ptr->file_namespace->get_type_from_str(group_type->to_string()).value();
    }
    expr->type = result_type;

    // Make sure that all group expression literal types are resolved
    GroupExpressionNode *group_expression = expr->as<GroupExpressionNode>();
    for (auto &group_expr : group_expression->expressions) {
        resolve_comptime_type_of_expr(parser, group_expr, std::nullopt);
    }
    return true;
}

Analyzer::Castability::CastDirection Analyzer::Castability::check_primitive_castability(const std::shared_ptr<Type> &lhs_type, //
    const std::shared_ptr<Type> &rhs_type,                                                                                     //
    const bool is_implicit                                                                                                     //
) {
    PROFILE_CUMULATIVE("Analyzer::Castability::check_primitive_castability");
    const std::string lhs_str = lhs_type->to_string();
    const std::string rhs_str = rhs_type->to_string();
    ASSERT(lhs_str != rhs_str);

    // Check if both sides are literals, cast int to float in that case
    if (lhs_str == "int" && rhs_str == "float") {
        return CastDirection::lhs_to_rhs();
    } else if (lhs_str == "float" && rhs_str == "int") {
        return CastDirection::rhs_to_lhs();
    } else if (lhs_str == "str" && rhs_type->get_variation() == Type::Variation::ENUM) {
        // Enums are always castable to strings
        return CastDirection::rhs_to_lhs();
    } else if (lhs_str == "str" && rhs_type->get_variation() == Type::Variation::ERROR_SET) {
        // Error sets are always castable to strings
        return CastDirection::rhs_to_lhs();
    } else if (lhs_str == "str" && rhs_str == "anyerror") {
        // The 'anyerror' Error can always be cast to strings
        return CastDirection::rhs_to_lhs();
    } else if (lhs_str == "str" && rhs_type->get_variation() == Type::Variation::OPAQUE) {
        // Any opaque type can be cast to a string, the string will either be `null` or `0x...`
        return CastDirection::rhs_to_lhs();
    }

    // Check if one side is a pointer type and the other is opaque
    if (lhs_type->get_variation() == Type::Variation::OPAQUE && rhs_type->get_variation() == Type::Variation::POINTER) {
        return CastDirection::rhs_to_lhs();
    } else if (rhs_type->get_variation() == Type::Variation::OPAQUE && lhs_type->get_variation() == Type::Variation::POINTER) {
        return CastDirection::lhs_to_rhs();
    }

    // Check if both sides are error variants and whether one error variant is the superset of the other one
    if (lhs_type->get_variation() == Type::Variation::ERROR_SET && rhs_type->get_variation() == Type::Variation::ERROR_SET) {
        ErrorNode *const lhs_err = lhs_type->as<ErrorSetType>()->error_node;
        ErrorNode *const rhs_err = rhs_type->as<ErrorSetType>()->error_node;
        // It is always allowed to cast to more specific errors if the more specific error is the superset of the basic error
        if (lhs_err->is_parent_of(rhs_err)) {
            return CastDirection::lhs_to_rhs();
        }
        if (rhs_err->is_parent_of(lhs_err)) {
            return CastDirection::rhs_to_lhs();
        }
        return CastDirection::not_castable();
    }

    // Check if one side is an object and the other side is a func component and if the func component is contained within the object type
    if (lhs_type->get_variation() == Type::Variation::OBJECT && rhs_type->get_variation() == Type::Variation::FUNC) {
        ObjectNode *const lhs_obj = lhs_type->as<ObjectType>()->object_node;
        FuncNode *const rhs_func = rhs_type->as<FuncType>()->func_node;
        for (const auto &func_component_ptr : lhs_obj->func_components) {
            if (rhs_func == func_component_ptr) {
                return CastDirection::lhs_to_rhs();
            }
        }
    } else if (lhs_type->get_variation() == Type::Variation::FUNC && rhs_type->get_variation() == Type::Variation::OBJECT) {
        FuncNode *const lhs_func = lhs_type->as<FuncType>()->func_node;
        ObjectNode *const rhs_obj = rhs_type->as<ObjectType>()->object_node;
        for (const auto &func_component_ptr : rhs_obj->func_components) {
            if (lhs_func == func_component_ptr) {
                return CastDirection::rhs_to_lhs();
            }
        }
    }

    // Check if one side is an object and the other side is an interface and if the interface is contained within the object type
    if (lhs_type->get_variation() == Type::Variation::OBJECT && rhs_type->get_variation() == Type::Variation::INTERFACE) {
        ObjectNode *const lhs_obj = lhs_type->as<ObjectType>()->object_node;
        InterfaceNode *const rhs_interface = rhs_type->as<InterfaceType>()->interface_node;
        for (const auto &interface : lhs_obj->interfaces) {
            if (rhs_interface == interface.type->as<InterfaceType>()->interface_node) {
                return CastDirection::lhs_to_rhs();
            }
        }
    } else if (lhs_type->get_variation() == Type::Variation::INTERFACE && rhs_type->get_variation() == Type::Variation::OBJECT) {
        InterfaceNode *const lhs_interface = lhs_type->as<InterfaceType>()->interface_node;
        ObjectNode *const rhs_obj = rhs_type->as<ObjectType>()->object_node;
        for (const auto &interface : rhs_obj->interfaces) {
            if (lhs_interface == interface.type->as<InterfaceType>()->interface_node) {
                return CastDirection::rhs_to_lhs();
            }
        }
    }

    bool lhs_to_rhs_allowed = false;
    bool rhs_to_lhs_allowed = false;
    if (is_implicit) {
        // Check if lhs can implicitly cast to rhs
        const auto &lhs_cast_it = primitive_implicit_casting_table.find(lhs_str);
        if (lhs_cast_it != primitive_implicit_casting_table.end()) {
            const auto &lhs_targets = lhs_cast_it->second;
            auto it = std::find(lhs_targets.begin(), lhs_targets.end(), rhs_str);
            if (it != lhs_targets.end()) {
                lhs_to_rhs_allowed = true;
            }
        }
        // Check if rhs can implicitly cast to lhs
        const auto &rhs_cast_it = primitive_implicit_casting_table.find(rhs_str);
        if (rhs_cast_it != primitive_implicit_casting_table.end()) {
            const auto &rhs_targets = rhs_cast_it->second;
            auto it = std::find(rhs_targets.begin(), rhs_targets.end(), lhs_str);
            if (it != rhs_targets.end()) {
                rhs_to_lhs_allowed = true;
            }
        }
    } else {
        // Check if lhs can explicitely cast to rhs
        const auto &lhs_cast_it = primitive_casting_table.find(lhs_str);
        if (lhs_cast_it != primitive_casting_table.end()) {
            const auto &lhs_targets = lhs_cast_it->second;
            auto it = std::find(lhs_targets.begin(), lhs_targets.end(), rhs_str);
            if (it != lhs_targets.end()) {
                lhs_to_rhs_allowed = true;
            }
        }
        // Check if rhs can explicitely cast to lhs
        const auto &rhs_cast_it = primitive_casting_table.find(rhs_str);
        if (rhs_cast_it != primitive_casting_table.end()) {
            const auto &rhs_targets = rhs_cast_it->second;
            auto it = std::find(rhs_targets.begin(), rhs_targets.end(), lhs_str);
            if (it != rhs_targets.end()) {
                rhs_to_lhs_allowed = true;
            }
        }
    }

    // If only one direction is allowed, choose that
    if (lhs_to_rhs_allowed && !rhs_to_lhs_allowed) {
        return CastDirection::lhs_to_rhs();
    }
    if (rhs_to_lhs_allowed && !lhs_to_rhs_allowed) {
        return CastDirection::rhs_to_lhs();
    }
    // If both directions are allowed, it's castable both ways (for example u8 <-> bool8)
    if (rhs_to_lhs_allowed && lhs_to_rhs_allowed) {
        return CastDirection::bidirectional();
    }
    // If neither direction is allowed, types are incompatible
    if (!lhs_to_rhs_allowed && !rhs_to_lhs_allowed) {
        return CastDirection::not_castable();
    }

    // Special case: concrete type + float literal
    // Example: i32 + 3.0 -> both should become f32
    if (rhs_str == "float") {
        // rhs is unresolved float literal, lhs is concrete
        if (lhs_str == "f32" || lhs_str == "f64") {
            return CastDirection::rhs_to_lhs();
        }
        const std::shared_ptr<Type> f32_ty = Type::get_primitive_type("f32");
        return CastDirection::both_to_common(f32_ty);
    }
    if (lhs_str == "float") {
        // lhs is unresolved float literal, rhs is concrete
        if (rhs_str == "f32" || rhs_str == "f64") {
            return CastDirection::lhs_to_rhs();
        }
        const std::shared_ptr<Type> f32_ty = Type::get_primitive_type("f32");
        return CastDirection::both_to_common(f32_ty);
    }

    // Prefer concrete types over int literals
    if (lhs_str == "int") {
        return CastDirection::lhs_to_rhs(); // int literal adopts concrete type
    }
    if (rhs_str == "int") {
        return CastDirection::rhs_to_lhs(); // int literal adopts concrete type
    }

    // If no side is a literal or compile-time type, prefer widening conversions
    // Check if one is strictly wider than the other
    static const std::unordered_map<std::string_view, int> type_size = {
        {"bool", 1},
        {"bool8", 8},
        {"u8", 8},
        {"i8", 8},
        {"u16", 16},
        {"i16", 16},
        {"u32", 32},
        {"i32", 32},
        {"u64", 64},
        {"i64", 64},
        {"f32", 32},
        {"f64", 64},
    };
    const auto &lhs_size_it = type_size.find(lhs_str);
    const auto &rhs_size_it = type_size.find(rhs_str);
    if (lhs_size_it != type_size.end() && rhs_size_it != type_size.end()) {
        const int lhs_bits = lhs_size_it->second;
        const int rhs_bits = rhs_size_it->second;

        if (lhs_bits < rhs_bits) {
            // Cast smaller lhs to larger rhs
            return CastDirection::lhs_to_rhs();
        }
        if (rhs_bits < lhs_bits) {
            // Cast smaller rhs to larger lhs
            return CastDirection::rhs_to_lhs();
        }
    }

    // Default fallback, cast lhs to rhs (left-to-right bias)
    return CastDirection::lhs_to_rhs();
}

Analyzer::Castability::CastDirection Analyzer::Castability::check_castability(const std::shared_ptr<Type> &lhs_type, //
    const std::shared_ptr<Type> &rhs_type,                                                                           //
    const bool is_implicit                                                                                           //
) {
    PROFILE_CUMULATIVE("Analyzer::Castability::check_castability_type");
    if (lhs_type->get_variation() == Type::Variation::ALIAS) {
        const auto *lhs_alias = lhs_type->as<AliasType>();
        return check_castability(lhs_alias->type, rhs_type, is_implicit);
    }
    if (rhs_type->get_variation() == Type::Variation::ALIAS) {
        const auto *rhs_alias = rhs_type->as<AliasType>();
        return check_castability(lhs_type, rhs_alias->type, is_implicit);
    }
    if (lhs_type->equals(rhs_type)) {
        return CastDirection::same_type();
    }
    const GroupType *lhs_group = dynamic_cast<const GroupType *>(lhs_type.get());
    const GroupType *rhs_group = dynamic_cast<const GroupType *>(rhs_type.get());
    if (lhs_group == nullptr && rhs_group == nullptr) {
        // Both single type
        // If one of them is a vector-type, the other one has to be a single value with the same type as the base type of the mutli-type
        const VectorType *lhs_mult = dynamic_cast<const VectorType *>(lhs_type.get());
        const VectorType *rhs_mult = dynamic_cast<const VectorType *>(rhs_type.get());
        if (lhs_mult != nullptr && rhs_mult == nullptr && is_castable_to(lhs_mult->base_type, rhs_type)) {
            return CastDirection::rhs_to_lhs();
        } else if (lhs_mult == nullptr && rhs_mult != nullptr && is_castable_to(rhs_mult->base_type, lhs_type)) {
            return CastDirection::lhs_to_rhs();
        }
        if (lhs_type->to_string() == "type.flint.str.lit" && rhs_type->to_string() == "str") {
            return CastDirection::lhs_to_rhs();
        } else if (lhs_type->to_string() == "str" && rhs_type->to_string() == "type.flint.str.lit") {
            return CastDirection::rhs_to_lhs();
        }
        // Check one or both of the sides are optional types
        const OptionalType *lhs_opt = dynamic_cast<const OptionalType *>(lhs_type.get());
        const OptionalType *rhs_opt = dynamic_cast<const OptionalType *>(rhs_type.get());
        if (lhs_opt != nullptr || rhs_opt != nullptr) {
            // One of the sides is an optional, but first check if the lhs or rhs is a literal
            const std::string lhs_str = lhs_type->to_string();
            const std::string rhs_str = rhs_type->to_string();
            if (lhs_str == "void?" || rhs_str == "void?") {
                // If rhs is void? then rhs -> lhs, otherwise lhs -> rhs
                return rhs_str == "void?" ? CastDirection::rhs_to_lhs() : CastDirection::lhs_to_rhs();
            }
            // None of the sides is a optional literal, so we need to check if one of the sides is the same type as the optional's base type
            if (lhs_opt != nullptr) {
                if (lhs_opt->base_type == rhs_type) {
                    return CastDirection::rhs_to_lhs();
                } else if ((rhs_str == "int" || rhs_str == "float")                                                           //
                    && check_primitive_castability(lhs_opt->base_type, rhs_type).kind == CastDirection::Kind::CAST_RHS_TO_LHS //
                ) {
                    return CastDirection::rhs_to_lhs();
                }
                const CastDirection cast = check_castability(lhs_opt->base_type, rhs_type, is_implicit);
                switch (cast.kind) {
                    case CastDirection::Kind::SAME_TYPE:
                        return CastDirection::same_type();
                    case CastDirection::Kind::CAST_BIDIRECTIONAL:
                    case CastDirection::Kind::CAST_RHS_TO_LHS:
                        return CastDirection::rhs_to_lhs();
                    default:
                        return CastDirection::not_castable();
                }
            } else if (rhs_opt != nullptr) {
                if (rhs_opt->base_type == lhs_type) {
                    return CastDirection::lhs_to_rhs();
                } else if ((lhs_str == "int" || lhs_str == "float")                                                           //
                    && check_primitive_castability(rhs_opt->base_type, lhs_type).kind == CastDirection::Kind::CAST_RHS_TO_LHS //
                ) {
                    return CastDirection::lhs_to_rhs();
                }
                const CastDirection cast = check_castability(rhs_opt->base_type, lhs_type, is_implicit);
                switch (cast.kind) {
                    case CastDirection::Kind::SAME_TYPE:
                        return CastDirection::same_type();
                    case CastDirection::Kind::CAST_BIDIRECTIONAL:
                    case CastDirection::Kind::CAST_LHS_TO_RHS:
                        return CastDirection::lhs_to_rhs();
                    default:
                        break;
                }
            }
        }
        // Check if one or both of the sides are variant types, the other side then needs to be one of the possible variant types
        const VariantType *lhs_var = dynamic_cast<const VariantType *>(lhs_type.get());
        const VariantType *rhs_var = dynamic_cast<const VariantType *>(rhs_type.get());
        if (lhs_var != nullptr && rhs_var != nullptr) {
            return CastDirection::not_castable();
        } else if (lhs_var != nullptr) {
            const std::string lhs_type_str = lhs_type->to_string();
            const std::string rhs_type_str = rhs_type->to_string();
            for (const auto &[_, type] : lhs_var->get_possible_types()) {
                const std::string type_str = type->to_string();
                if (type == rhs_type) {
                    return CastDirection::rhs_to_lhs();
                }
            }
            return CastDirection::not_castable();
        } else if (rhs_var != nullptr) {
            const std::string lhs_type_str = lhs_type->to_string();
            const std::string rhs_type_str = rhs_type->to_string();
            for (const auto &[_, type] : rhs_var->get_possible_types()) {
                const std::string type_str = type->to_string();
                if (type == lhs_type) {
                    return CastDirection::lhs_to_rhs();
                }
            }
            return CastDirection::not_castable();
        }
        // Check if one or both of the sides are array types, the only allowed cast direction is fixed array -> dynamic array
        const ArrayType *lhs_arr = dynamic_cast<const ArrayType *>(lhs_type.get());
        const ArrayType *rhs_arr = dynamic_cast<const ArrayType *>(rhs_type.get());
        if (lhs_arr != nullptr && rhs_arr != nullptr) {
            if (!lhs_arr->type->equals(rhs_arr->type)) {
                return CastDirection::not_castable();
            }
            if (lhs_arr->dimensionality != rhs_arr->dimensionality) {
                return CastDirection::not_castable();
            }
            if (!lhs_arr->sizes.has_value() && rhs_arr->sizes.has_value()) {
                return CastDirection::rhs_to_lhs();
            }
            if (lhs_arr->sizes.has_value() && !rhs_arr->sizes.has_value()) {
                return CastDirection::lhs_to_rhs();
            }
            return CastDirection::not_castable();
        }
        // If none of the above apply, check if its a primitive castability situation
        return check_primitive_castability(lhs_type, rhs_type, is_implicit);
    } else if (lhs_group == nullptr && rhs_group != nullptr) {
        // Left is no group, right is group
        switch (lhs_type->get_variation()) {
            default:
                return CastDirection::not_castable();
            case Type::Variation::VECTOR: {
                const auto *lhs_mult = lhs_type->as<VectorType>();
                // If left is a vector-type, then the right is castable to the left and the left to the right
                // The group must have the same size as the vector-type
                if (lhs_mult->width != rhs_group->types.size()) {
                    return CastDirection::not_castable();
                }
                // All elements in the group must have the same type as the vector-type or be implicitely castable to it (for example
                // literals)
                for (size_t i = 0; i < lhs_mult->width; i++) {
                    if (lhs_mult->base_type->equals(rhs_group->types.at(i))) {
                        continue;
                    }
                    switch (check_primitive_castability(lhs_mult->base_type, rhs_group->types.at(i)).kind) {
                        case CastDirection::Kind::NOT_CASTABLE:
                        case CastDirection::Kind::CAST_BOTH_TO_COMMON:
                        case CastDirection::Kind::CAST_LHS_TO_RHS:
                            return CastDirection::not_castable();
                        case CastDirection::Kind::SAME_TYPE:
                        case CastDirection::Kind::CAST_RHS_TO_LHS:
                        case CastDirection::Kind::CAST_BIDIRECTIONAL:
                            break;
                    }
                }
                return CastDirection::bidirectional();
            }
            case Type::Variation::TUPLE: {
                const auto *lhs_tup = lhs_type->as<TupleType>();
                // If left is a tuple type then the right is castable to the left
                if (lhs_tup->types.size() != rhs_group->types.size()) {
                    return CastDirection::not_castable();
                }
                // All elements in the group must be castable or equal to the element of the tuple
                for (size_t i = 0; i < lhs_tup->types.size(); i++) {
                    const std::shared_ptr<Type> lhs_elem_type = lhs_tup->types[i];
                    const std::shared_ptr<Type> rhs_elem_type = rhs_group->types[i];
                    if (lhs_elem_type == rhs_elem_type) {
                        continue;
                    }
                    const std::string rhs_elem_type_str = rhs_elem_type->to_string();
                    const CastDirection elem_castability = check_castability(lhs_elem_type, rhs_elem_type, is_implicit);
                    switch (elem_castability.kind) {
                        default:
                            return CastDirection::not_castable();
                        case CastDirection::Kind::SAME_TYPE:
                            return CastDirection::same_type();
                        case CastDirection::Kind::CAST_RHS_TO_LHS:
                            if (rhs_elem_type_str == "int" || rhs_elem_type_str == "float") {
                                // TODO: We somehow need to change the type of the rhs expression directly, but that's not possible in this
                                // function
                            } else {
                                // TODO: We somehow need to wrap the rhs in a TypeCastNode, but this is not possible in this function
                            }
                    }
                }
                return CastDirection::bidirectional();
            }
            case Type::Variation::PRIMITIVE: {
                // Check for single element <-> homogeneous group castability, for example for `x == (1, 2)` or `(1, 2, 3) + 4`. In such
                // cases the single type must cast to the group, not the other way around
                for (size_t i = 0; i < rhs_group->types.size(); i++) {
                    const std::shared_ptr<Type> &rhs_elem_type = rhs_group->types[i];
                    if (lhs_type->equals(rhs_elem_type)) {
                        continue;
                    }
                    const CastDirection elem_castability = check_castability(lhs_type, rhs_elem_type, is_implicit);
                    switch (elem_castability.kind) {
                        case CastDirection::Kind::NOT_CASTABLE:
                        case CastDirection::Kind::CAST_BOTH_TO_COMMON:
                        case CastDirection::Kind::CAST_LHS_TO_RHS:
                            return CastDirection::not_castable();
                        case CastDirection::Kind::SAME_TYPE:
                        case CastDirection::Kind::CAST_RHS_TO_LHS:
                        case CastDirection::Kind::CAST_BIDIRECTIONAL:
                            break;
                    }
                }
                return CastDirection::rhs_to_lhs();
            }
        }
    } else if (lhs_group != nullptr && rhs_group == nullptr) {
        // Left is group, right is no group, so we call the function itself and just swap the sides, this effectively executes the branch
        // above, and then we just need to flip the result for the directional casts afterwards
        CastDirection cast_dir = check_castability(rhs_type, lhs_type, is_implicit);
        switch (cast_dir.kind) {
            case CastDirection::Kind::CAST_LHS_TO_RHS:
                cast_dir.kind = CastDirection::Kind::CAST_RHS_TO_LHS;
                break;
            case CastDirection::Kind::CAST_RHS_TO_LHS:
                cast_dir.kind = CastDirection::Kind::CAST_LHS_TO_RHS;
                break;
            default:
                break;
        }
        return cast_dir;
    } else {
        // Both group, this means that either all types on the left must be castable to the right or all on the right must be castable to
        // the left. Mixing and matching is not allowed, so either all of the right need to be cast to the left or alll of the left to the
        // right
        if (lhs_group->types.size() != rhs_group->types.size()) {
            return CastDirection::not_castable();
        }
        std::vector<CastDirection> castability;
        for (size_t i = 0; i < lhs_group->types.size(); i++) {
            castability.emplace_back(check_castability(lhs_group->types.at(i), rhs_group->types.at(i), is_implicit));
        }
        enum class Direction { UNCERTAIN, LTR, RTL };
        Direction dir = Direction::UNCERTAIN;
        for (const auto &cast : castability) {
            switch (cast.kind) {
                case CastDirection::Kind::SAME_TYPE:
                case CastDirection::Kind::CAST_BIDIRECTIONAL:
                    continue;
                case CastDirection::Kind::CAST_BOTH_TO_COMMON:
                case CastDirection::Kind::NOT_CASTABLE:
                    return CastDirection::not_castable();
                case CastDirection::Kind::CAST_LHS_TO_RHS:
                    if (dir == Direction::RTL) {
                        return CastDirection::not_castable();
                    }
                    dir = Direction::LTR;
                    break;
                case CastDirection::Kind::CAST_RHS_TO_LHS:
                    if (dir == Direction::LTR) {
                        return CastDirection::not_castable();
                    }
                    dir = Direction::RTL;
                    break;
            }
        }
        switch (dir) {
            case Direction::UNCERTAIN:
                return CastDirection::not_castable();
            case Direction::LTR:
                return CastDirection::lhs_to_rhs();
            case Direction::RTL:
                return CastDirection::rhs_to_lhs();
        }
    }
    return CastDirection::rhs_to_lhs();
}

Analyzer::Castability::BinopMatchResult Analyzer::Castability::match_binop_operands(Parser &parser, const Token &pivot_token,
    std::unique_ptr<ExpressionNode> &lhs, std::unique_ptr<ExpressionNode> &rhs) {
    PROFILE_CUMULATIVE("Analyzer::Castability::match_binop_operands");

    // Coercion helper mirroring the former `Parser::check_castability(lhs, rhs)` wrapper
    auto check_and_coerce = [&parser](std::unique_ptr<ExpressionNode> &lhs_expr, std::unique_ptr<ExpressionNode> &rhs_expr) -> bool {
        if (lhs_expr->type->equals(rhs_expr->type)) {
            return true;
        }
        const CastDirection castability = check_castability(lhs_expr->type, rhs_expr->type);
        switch (castability.kind) {
            case CastDirection::Kind::NOT_CASTABLE:
                return false;
            case CastDirection::Kind::SAME_TYPE:
                return true;
            case CastDirection::Kind::CAST_LHS_TO_RHS:
                return check_castability(parser, rhs_expr->type, lhs_expr);
            case CastDirection::Kind::CAST_BIDIRECTIONAL:
            case CastDirection::Kind::CAST_RHS_TO_LHS:
                return check_castability(parser, lhs_expr->type, rhs_expr);
            case CastDirection::Kind::CAST_BOTH_TO_COMMON:
                if (!check_castability(parser, castability.common_type, lhs_expr, false)) {
                    return false;
                }
                if (!check_castability(parser, castability.common_type, rhs_expr, false)) {
                    return false;
                }
                return true;
        }
        UNREACHABLE();
    };

    if (lhs->type->equals(rhs->type)) {
        return BinopMatchResult::OK;
    }

    // Check if the operator is a optional default, in this case we need to check whether the lhs is an optional and whether the rhs is the
    // base type of the optional, otherwise it is considered an error
    if (pivot_token == TOK_OPT_DEFAULT) {
        if (lhs->type->get_variation() != Type::Variation::OPTIONAL) {
            // ?? operator not possible on non-optional type
            return BinopMatchResult::TYPE_MISMATCH;
        }
        const auto *lhs_opt = lhs->type->as<OptionalType>();
        if (!check_castability(parser, lhs_opt->base_type, rhs, true)) {
            return BinopMatchResult::OPT_DEFAULT_MISMATCH;
        }
        return BinopMatchResult::OK;
    }

    // Check if one of the sides is a homogeneous group variation of the other side
    // This only works if the *other side*s type is comparable at all. Only primitive types and enums are comparable
    bool is_castable = true;
    const std::shared_ptr<Type> &lhs_type = lhs->type;
    const std::shared_ptr<Type> &rhs_type = rhs->type;

    const auto lhs_variation = lhs_type->get_variation();
    const auto rhs_variation = rhs_type->get_variation();

    const bool lhs_is_group = lhs_variation == Type::Variation::GROUP;
    const bool rhs_is_group = rhs_variation == Type::Variation::GROUP;
    const bool lhs_is_comparable = lhs_variation == Type::Variation::ENUM || lhs_variation == Type::Variation::PRIMITIVE;
    const bool rhs_is_comparable = rhs_variation == Type::Variation::ENUM || rhs_variation == Type::Variation::PRIMITIVE;

    if (lhs_is_group && rhs_is_comparable) {
        // All elements of the lhs group must match the rhs type, otherwise it's not a homogenous group
        const GroupType *lhs_group_type = lhs_type->as<GroupType>();
        GroupExpressionNode *lhs_group_expr = dynamic_cast<GroupExpressionNode *>(rhs.get());
        const bool rhs_is_literal = rhs_type->to_string() == "int" || rhs_type->to_string() == "float";
        const std::shared_ptr<Type> cmp_type = rhs_is_literal ? lhs_group_type->types.front() : rhs_type;
        for (size_t i = 0; i < lhs_group_type->types.size(); i++) {
            const auto &type = lhs_group_type->types.at(i);
            if (lhs_group_expr != nullptr) {
                if (!check_castability(parser, rhs_type, lhs_group_expr->expressions.at(i))) {
                    is_castable = false;
                    break;
                }
                continue;
            }
            if (!type->equals(cmp_type)) {
                is_castable = false;
                break;
            }
        }
        if (is_castable && rhs_is_literal) {
            // Set the type of the rhs literal to mark it as "resolved"
            rhs->type = cmp_type;
        }
    } else if (rhs_is_group && lhs_is_comparable) {
        // All elements of the rhs group must match the lhs type or be castable to it, otherwise it's not a homogenous group
        const GroupType *rhs_group_type = rhs_type->as<GroupType>();
        GroupExpressionNode *rhs_group_expr = dynamic_cast<GroupExpressionNode *>(rhs.get());
        const bool lhs_is_literal = lhs_type->to_string() == "int" || lhs_type->to_string() == "float";
        const std::shared_ptr<Type> cmp_type = lhs_is_literal ? rhs_group_type->types.front() : lhs_type;
        for (size_t i = 0; i < rhs_group_type->types.size(); i++) {
            const auto &type = rhs_group_type->types.at(i);
            if (rhs_group_expr != nullptr) {
                if (!check_castability(parser, lhs_type, rhs_group_expr->expressions.at(i))) {
                    is_castable = false;
                    break;
                }
                continue;
            }
            if (!type->equals(lhs_type)) {
                const std::string type_str = type->to_string();
                if (type_str == "int" || type_str == "float")
                    is_castable = false;
                break;
            }
        }
        if (is_castable && lhs_is_literal) {
            // Set the type of the lhs literal to mark it as "resolved"
            lhs->type = cmp_type;
        }
    } else if (lhs_is_group && rhs_is_group) {
        // For example the groups (int, i32) and (i64, int) should result in both sides being of type (i64, i32)
        //
        // Non-group expressions could also have a group type as their result. Only GroupExpressionNodes can be cast to other group
        // types, for example if we do a function call which returns `(u32, i32)` then we cannot cast it's expressions directly. For this
        // case the whole group needs to be cast. from `(u32, i32) -> (u64, i64)` for example. This means that we have three four distinct
        // possibilities to account for:
        // - both sides are group expressions
        // - left group expression, right other expression returning a group
        // - left some expression returning a group, right group expression
        // - none of the sides are group expressions
        const GroupType *lhs_group_type = lhs_type->as<GroupType>();
        const GroupType *rhs_group_type = rhs_type->as<GroupType>();
        GroupExpressionNode *lhs_group_expr = dynamic_cast<GroupExpressionNode *>(lhs.get());
        GroupExpressionNode *rhs_group_expr = dynamic_cast<GroupExpressionNode *>(rhs.get());
        if (lhs_group_type->types.size() == rhs_group_type->types.size()) {
            if (lhs_group_expr != nullptr && rhs_group_expr != nullptr) {
                // Both sides are group expressions
                for (size_t i = 0; i < lhs_group_type->types.size(); i++) {
                    if (!check_and_coerce(lhs_group_expr->expressions.at(i), rhs_group_expr->expressions.at(i))) {
                        is_castable = false;
                        break;
                    }
                }
            } else if (lhs_group_expr != nullptr && rhs_group_expr == nullptr) {
                // Rhs is no group expr, lhs is a group expr
                is_castable = check_castability(parser, rhs_type, lhs);
            } else if (lhs_group_expr == nullptr && rhs_group_expr != nullptr) {
                // Lhs is no group expr, rhs is a group expr
                is_castable = check_castability(parser, lhs_type, rhs);
            } else {
                // TODO: Both sides are non-group expressions
                THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                is_castable = false;
            }
        }
    } else {
        is_castable = check_and_coerce(lhs, rhs);
    }
    if (!is_castable) {
        return BinopMatchResult::TYPE_MISMATCH;
    }
    return BinopMatchResult::OK;
}

bool Analyzer::Castability::is_castable_to(const std::shared_ptr<Type> &from, const std::shared_ptr<Type> &to, const bool is_implicit) {
    const CastDirection cast_direction = check_castability(from, to, is_implicit);
    switch (cast_direction.kind) {
        case CastDirection::Kind::NOT_CASTABLE:
        case CastDirection::Kind::CAST_LHS_TO_RHS:
        case CastDirection::Kind::CAST_BOTH_TO_COMMON:
            return false;
        case CastDirection::Kind::SAME_TYPE:
        case CastDirection::Kind::CAST_RHS_TO_LHS:
        case CastDirection::Kind::CAST_BIDIRECTIONAL:
            return true;
    }
}

bool Analyzer::Castability::check_castability(Parser &parser, const std::shared_ptr<Type> &target_type,
    std::unique_ptr<ExpressionNode> &expr, const bool is_implicit) {
    PROFILE_CUMULATIVE("Analyzer::Castability::check_castability_expr_inplace");
    if (target_type->get_variation() == Type::Variation::ALIAS) {
        const auto *alias_type = target_type->as<AliasType>();
        return check_castability(parser, alias_type->type, expr, is_implicit);
    }
    if (expr->type->equals(target_type)) {
        return true;
    }
    const std::string expr_type_str = expr->type->to_string();
    const std::string target_type_str = target_type->to_string();
    if (expr_type_str == "type.flint.str.lit" && target_type_str == "str") {
        expr = std::make_unique<TypeCastNode>(                                                              //
            parser.file_hash, ASTNode::PosTriple{expr->line, expr->column, expr->length}, target_type, expr //
        );
        return true;
    }

    const CastDirection cast_direction = check_castability(target_type, expr->type, is_implicit);
    switch (cast_direction.kind) {
        case CastDirection::Kind::SAME_TYPE:
            return true;
        case CastDirection::Kind::NOT_CASTABLE:
        case CastDirection::Kind::CAST_LHS_TO_RHS:
        case CastDirection::Kind::CAST_BOTH_TO_COMMON:
            return false;
        case CastDirection::Kind::CAST_RHS_TO_LHS:
        case CastDirection::Kind::CAST_BIDIRECTIONAL:
            break;
    }

    if (expr->get_variation() == ExpressionNode::Variation::GROUP_EXPRESSION) {
        auto *group_expr = expr->as<GroupExpressionNode>();
        const auto *group_expr_type = expr->type->as<GroupType>();
        const auto target_variation = target_type->get_variation();
        if (target_variation == Type::Variation::GROUP || target_variation == Type::Variation::TUPLE) {
            std::vector<std::shared_ptr<Type>> target_types;
            if (target_variation == Type::Variation::GROUP) {
                const auto *target_group = target_type->as<GroupType>();
                target_types = target_group->types;
            } else {
                const auto *target_tuple = target_type->as<TupleType>();
                target_types = target_tuple->types;
            }
            if (cast_direction.kind != CastDirection::Kind::CAST_RHS_TO_LHS       //
                && cast_direction.kind != CastDirection::Kind::CAST_BIDIRECTIONAL //
            ) {
                return false;
            }
            if (target_types.size() != group_expr_type->types.size()) {
                return false;
            }

            bool any_element_changed = false;
            std::vector<std::shared_ptr<Type>> new_element_types;
            for (size_t i = 0; i < target_types.size(); i++) {
                const std::shared_ptr<Type> &target_elem_type = target_types[i];
                const std::shared_ptr<Type> &expr_elem_type = group_expr_type->types[i];
                auto &elem_expr = group_expr->expressions[i];
                if (!expr_elem_type->equals(target_elem_type)) {
                    const CastDirection elem_cast = check_castability(target_elem_type, expr_elem_type, is_implicit);
                    switch (elem_cast.kind) {
                        case CastDirection::Kind::CAST_BOTH_TO_COMMON:
                        case CastDirection::Kind::NOT_CASTABLE:
                        case CastDirection::Kind::CAST_LHS_TO_RHS:
                            return false;
                        case CastDirection::Kind::CAST_BIDIRECTIONAL:
                        case CastDirection::Kind::SAME_TYPE:
                            new_element_types.push_back(target_elem_type);
                            break;
                        case CastDirection::Kind::CAST_RHS_TO_LHS: {
                            const std::string &elem_type_str = expr_elem_type->to_string();
                            if (!resolve_comptime_type_of_expr(parser, elem_expr, target_elem_type)) {
                                const auto cast_pos = ASTNode::PosTriple{elem_expr->line, elem_expr->column, elem_expr->length};
                                elem_expr = std::make_unique<TypeCastNode>(parser.file_hash, cast_pos, target_elem_type, elem_expr);
                            }
                            new_element_types.push_back(target_elem_type);
                            any_element_changed = true;
                            break;
                        }
                    }
                } else {
                    new_element_types.push_back(expr_elem_type);
                }
            }
            if (any_element_changed) {
                std::shared_ptr<Type> new_group_type = std::make_shared<GroupType>(new_element_types);
                if (!parser.file_node_ptr->file_namespace->add_type(new_group_type)) {
                    new_group_type = parser.file_node_ptr->file_namespace->get_type_from_str(new_group_type->to_string()).value();
                }
                expr->type = new_group_type;
            }
            if (expr->type->equals(target_type)) {
                return true;
            }
            expr = std::make_unique<TypeCastNode>(                                                              //
                parser.file_hash, ASTNode::PosTriple{expr->line, expr->column, expr->length}, target_type, expr //
            );
            return true;
        }
    }

    const ASTNode::PosTriple &expr_pos = {expr->line, expr->column, expr->length};
    switch (target_type->get_variation()) {
        case Type::Variation::ALIAS:
            UNREACHABLE();
            break;
        case Type::Variation::FN:
            UNREACHABLE();
            break;
        case Type::Variation::ARRAY:
        case Type::Variation::DATA:
        case Type::Variation::OBJECT:
        case Type::Variation::ENUM:
        case Type::Variation::ERROR_SET:
        case Type::Variation::FUNC:
        case Type::Variation::INTERFACE:
        case Type::Variation::POINTER:
        case Type::Variation::RANGE:
            expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
            return true;
        case Type::Variation::GROUP: {
            if (expr->type->get_variation() == Type::Variation::VECTOR) {
                const auto *expr_vector = expr->type->as<VectorType>();
                const auto *target_group = target_type->as<GroupType>();
                if (expr_vector->width != target_group->types.size()) {
                    return false;
                }
                for (size_t i = 0; i < target_group->types.size(); i++) {
                    if (!expr_vector->base_type->equals(target_group->types[i])) {
                        return false;
                    }
                }
                expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                return true;
            }
            expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
            return true;
        }
        case Type::Variation::VECTOR: {
            const auto *vector_type = target_type->as<VectorType>();
            if (vector_type->to_string() == "bool8" && expr->type->to_string() == "u8") {
                expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                return true;
            }
            if (expr->get_variation() != ExpressionNode::Variation::GROUP_EXPRESSION) {
                // If rhs type is a single value of type 'int' or 'float' then it can be splatted to the lhs type
                CastDirection::Kind primitive_castability = CastDirection::Kind::SAME_TYPE;
                if (!vector_type->base_type->equals(expr->type)) {
                    primitive_castability = check_primitive_castability(vector_type->base_type, expr->type).kind;
                }
                const bool rhs_is_able_to_swizzle =                                     //
                    primitive_castability == CastDirection::Kind::CAST_RHS_TO_LHS       //
                    || primitive_castability == CastDirection::Kind::CAST_BIDIRECTIONAL //
                    || primitive_castability == CastDirection::Kind::SAME_TYPE;
                if (rhs_is_able_to_swizzle) {
                    resolve_comptime_type_of_expr(parser, expr, vector_type->base_type);
                    expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                    return true;
                }
                return false;
            }
            auto *group_expr = expr->as<GroupExpressionNode>();
            if (group_expr->expressions.size() != vector_type->width) {
                return false;
            }

            bool any_element_changed = false;
            for (auto &elem_expr : group_expr->expressions) {
                if (elem_expr->type->equals(vector_type->base_type)) {
                    continue;
                }
                if (resolve_comptime_type_of_expr(parser, elem_expr, vector_type->base_type)) {
                    any_element_changed = true;
                    continue;
                }
                if (elem_expr->type->equals(vector_type->base_type)) {
                    continue;
                }
                const CastDirection elem_cast = check_primitive_castability(vector_type->base_type, elem_expr->type, is_implicit);
                switch (elem_cast.kind) {
                    case CastDirection::Kind::NOT_CASTABLE:
                    case CastDirection::Kind::CAST_BOTH_TO_COMMON:
                    case CastDirection::Kind::CAST_LHS_TO_RHS:
                        return false;
                    case CastDirection::Kind::SAME_TYPE:
                    case CastDirection::Kind::CAST_RHS_TO_LHS:
                    case CastDirection::Kind::CAST_BIDIRECTIONAL:
                        break;
                }
                expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                any_element_changed = true;
            }
            if (any_element_changed) {
                std::vector<std::shared_ptr<Type>> new_element_types(vector_type->width, vector_type->base_type);
                std::shared_ptr<Type> new_group_type = std::make_shared<GroupType>(new_element_types);
                if (!parser.file_node_ptr->file_namespace->add_type(new_group_type)) {
                    new_group_type = parser.file_node_ptr->file_namespace->get_type_from_str(new_group_type->to_string()).value();
                }
                expr->type = new_group_type;
            }
            expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
            return true;
        }
        case Type::Variation::OPAQUE: {
            // Any named opaque can be cast to any unnamed opaque
            // Any pointer can be cast to any opaque
            const auto *opaque_type = target_type->as<OpaqueType>();
            switch (expr->type->get_variation()) {
                default:
                    return false;
                case Type::Variation::OPAQUE:
                    if (opaque_type->name.has_value() || !expr->type->as<OpaqueType>()->name.has_value()) {
                        return false;
                    }
                    expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                    return true;
                case Type::Variation::POINTER:
                    expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                    return true;
            }
        }
        case Type::Variation::OPTIONAL: {
            const auto *optional_type = target_type->as<OptionalType>();
            if (expr->type->equals(optional_type->base_type)) {
                expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                return true;
            }
            if (expr->type->get_variation() == Type::Variation::OPTIONAL) {
                const auto *expr_opt = expr->type->as<OptionalType>();
                if (expr_opt->base_type->equals(Type::get_primitive_type("void"))) {
                    expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                    return true;
                }
                return false;
            }

            const CastDirection base_cast = check_castability(optional_type->base_type, expr->type, is_implicit);
            switch (base_cast.kind) {
                case CastDirection::Kind::SAME_TYPE:
                case CastDirection::Kind::CAST_BIDIRECTIONAL:
                case CastDirection::Kind::CAST_RHS_TO_LHS:
                    if (resolve_comptime_type_of_expr(parser, expr, optional_type->base_type)) {
                        expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                    } else {
                        expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, optional_type->base_type, expr);
                        expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
                    }
                    return true;
                default:
                    return false;
            }
        }
        case Type::Variation::PRIMITIVE: {
            if (target_type_str == "str" && (expr_type_str == "int" || expr_type_str == "float")) {
                if (expr->get_variation() == ExpressionNode::Variation::LITERAL) {
                    auto *literal = expr->as<LiteralNode>();
                    std::string str_value;
                    if (expr_type_str == "int") {
                        const auto &lit_int = std::get<LitInt>(literal->value);
                        str_value = lit_int.value.to_string();
                    } else {
                        const auto &lit_float = std::get<LitFloat>(literal->value);
                        str_value = lit_float.value.to_string();
                    }

                    LitValue new_value = LitStr{str_value};
                    const unsigned int line = literal->line;
                    const unsigned int column = literal->column;
                    const unsigned int length = literal->length;
                    const ASTNode::PosTriple literal_pos = {
                        .line = literal->line,
                        .column = literal->column,
                        .length = literal->length,
                    };
                    expr = std::make_unique<LiteralNode>(                                                                            //
                        parser.file_hash, literal_pos, new_value, Type::get_primitive_type("type.flint.str.lit"), literal->is_folded //
                    );
                    expr = std::make_unique<TypeCastNode>(parser.file_hash, literal_pos, Type::get_primitive_type("str"), expr);
                    expr->line = line;
                    expr->column = column;
                    expr->length = length;
                    return true;
                }
            }
            if (!resolve_comptime_type_of_expr(parser, expr, target_type)) {
                expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
            }
            return true;
        }
        case Type::Variation::TUPLE:
            expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
            return true;
        case Type::Variation::UNKNOWN:
            return false;
        case Type::Variation::VARIANT: {
            const auto *variant_type = target_type->as<VariantType>();
            bool is_valid_variant_type = false;
            for (const auto &[_, type] : variant_type->get_possible_types()) {
                if (type->equals(expr->type)) {
                    is_valid_variant_type = true;
                    break;
                }
            }
            if (!is_valid_variant_type) {
                return false;
            }
            expr = std::make_unique<TypeCastNode>(parser.file_hash, expr_pos, target_type, expr);
            return true;
        }
    }
    return true;
}
