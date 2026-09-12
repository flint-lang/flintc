#include "specializer/specializer.hpp"

#include "parser/ast/definitions/func_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/definitions/interface_node.hpp"
#include "parser/ast/definitions/object_node.hpp"
#include "parser/ast/definitions/variant_node.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/namespace.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/array_type.hpp"
#include "parser/type/comptime_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/fn_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/generic_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/interface_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/optional_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/tuple_type.hpp"
#include "parser/type/unknown_type.hpp"
#include "parser/type/variant_type.hpp"

std::optional<std::shared_ptr<Type>> Specializer::specialize( //
    Namespace *const ns,                                      //
    std::shared_ptr<Type> type,                               //
    std::vector<std::shared_ptr<Type>> cvl                    //
) {
    const DefinitionNode *definition = nullptr;
    if (type->get_variation() == Type::Variation::UNKNOWN) {
        auto *const unknown = type->as<UnknownType>();
        // Try to resolve type first, if it fails then we just return a generic type with the unknown as the base type
        if (const auto &resolved = ns->get_type_from_str(unknown->type_str)) {
            type = resolved.value();
        } else {
            std::shared_ptr<Type> new_type = std::make_shared<GenericType>(type, cvl);
            if (!ns->add_type(new_type)) {
                new_type = ns->get_type_from_str(new_type->to_string()).value();
            }
            return new_type;
        }
    }
    // Apply already present comptimme values if the type is generic, and set the type to the type of the generic type to further resolve it
    if (type->get_variation() == Type::Variation::GENERIC) {
        auto *const generic = type->as<GenericType>();
        for (auto it = generic->cvl.rbegin(); it != generic->cvl.rend(); ++it) {
            cvl.begin() = cvl.emplace(cvl.begin(), *it);
        }
        type = generic->base;
    }
    switch (type->get_variation()) {
        default:
            UNREACHABLE();
        case Type::Variation::DATA:
            definition = type->as<DataType>()->data_node;
            break;
        case Type::Variation::FUNC:
            definition = type->as<FuncType>()->func_node;
            break;
        case Type::Variation::INTERFACE:
            definition = type->as<InterfaceType>()->interface_node;
            break;
        case Type::Variation::OBJECT:
            definition = type->as<ObjectType>()->object_node;
            break;
        case Type::Variation::VARIANT:
            definition = std::get<VariantNode *const>(type->as<VariantType>()->var_or_list);
            break;
    }

    if (cvl.size() != definition->cpl.size()) {
        return std::nullopt;
    }
    for (const auto &param : definition->cpl) {
        if (param.type->get_variation() != Type::Variation::TYPE) {
            return std::nullopt;
        }
    }
    if (!std::all_of(cvl.begin(), cvl.end(), &is_specializable)) {
        std::shared_ptr<Type> new_type = std::make_shared<GenericType>(type, cvl);
        if (!ns->add_type(new_type)) {
            new_type = ns->get_type_from_str(new_type->to_string()).value();
        }
        return new_type;
    }

    // Check if specialization already exists, return that one if present
    const std::string definition_key = get_definition_string(definition, cvl, false);
    specialization_map &map = specializations[definition_key];
    const std::string specialization_key = get_definition_string(definition, cvl, true);
    if (map.find(specialization_key) != map.end()) {
        return ns->get_type_from_ptr(map.at(specialization_key));
    }

    // Pre-create fake specialized node as support for recursive types
    std::optional<DefinitionNode *const> dest = pre_create(definition, specialization_key);
    if (!dest.has_value()) {
        return std::nullopt;
    }
    DefinitionNode *const result = dest.value();
    map[specialization_key] = result;

    // Now do the actual specialization
    switch (definition->get_variation()) {
        default:
            UNREACHABLE();
        case DefinitionNode::Variation::DATA: {
            const auto *const src = definition->as<DataNode>();
            if (!specialize_data(static_cast<DataNode *const>(result), src, cvl)) {
                return std::nullopt;
            }
            break;
        }
        case DefinitionNode::Variation::FUNC: {
            [[maybe_unused]] const auto *const node = definition->as<FuncNode>();
            UNREACHABLE();
        }
        case DefinitionNode::Variation::FUNCTION: {
            [[maybe_unused]] const auto *const node = definition->as<FunctionNode>();
            UNREACHABLE();
        }
        case DefinitionNode::Variation::INTERFACE: {
            [[maybe_unused]] const auto *const node = definition->as<InterfaceNode>();
            UNREACHABLE();
        }
        case DefinitionNode::Variation::OBJECT: {
            [[maybe_unused]] const auto *const node = definition->as<ObjectNode>();
            UNREACHABLE();
        }
        case DefinitionNode::Variation::VARIANT: {
            const auto *const node = definition->as<VariantNode>();
            if (!specialize_variant(static_cast<VariantNode *const>(result), node, cvl)) {
                return std::nullopt;
            }
            break;
        }
    }
    return ns->get_type_from_ptr(result);
}

std::string Specializer::get_definition_string(    //
    const DefinitionNode *const definition,        //
    const std::vector<std::shared_ptr<Type>> &cvl, //
    const bool include_cpl                         //
) {
    std::stringstream ss;
    if (!include_cpl) {
        ss << definition->file_hash.to_string() << ".";
    } else {
        // Add "S." to mark "S"pecialized types
        ss << "S.";
    }
    switch (definition->get_variation()) {
        default:
            UNREACHABLE();
        case DefinitionNode::Variation::DATA: {
            const auto *const node = definition->as<DataNode>();
            ss << node->name;
            break;
        }
        case DefinitionNode::Variation::FUNC: {
            const auto *const node = definition->as<FuncNode>();
            ss << node->name;
            break;
        }
        case DefinitionNode::Variation::FUNCTION: {
            const auto *const node = definition->as<FunctionNode>();
            ss << node->name;
            break;
        }
        case DefinitionNode::Variation::INTERFACE: {
            const auto *const node = definition->as<InterfaceNode>();
            ss << node->name;
            break;
        }
        case DefinitionNode::Variation::OBJECT: {
            const auto *const node = definition->as<ObjectNode>();
            ss << node->name;
            break;
        }
        case DefinitionNode::Variation::VARIANT: {
            const auto *const node = definition->as<VariantNode>();
            ss << node->name;
            break;
        }
    }
    if (!include_cpl) {
        return ss.str();
    }
    ss << "[";
    for (size_t i = 0; i < cvl.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << cvl.at(i)->to_string();
    }
    ss << "]";
    return ss.str();
}

bool Specializer::is_specializable(const std::shared_ptr<Type> &type) {
    switch (type->get_variation()) {
        case Type::Variation::ALIAS:
            // Aliases should be resolved by now, but resolve them anyway to be safe
            return is_specializable(type->as<AliasType>()->type);
        case Type::Variation::ARRAY:
            return is_specializable(type->as<ArrayType>()->type);
        case Type::Variation::FN:
            for (const auto &[param_type, _] : type->as<FnType>()->params) {
                if (!is_specializable(param_type)) {
                    return false;
                }
            }
            for (const auto &return_type : type->as<FnType>()->return_types) {
                if (!is_specializable(return_type)) {
                    return false;
                }
            }
            for (const auto &error_type : type->as<FnType>()->error_types) {
                if (!is_specializable(error_type)) {
                    return false;
                }
            }
            return true;
        case Type::Variation::GROUP: {
            const auto &types = type->as<GroupType>()->types;
            for (const auto &elem_type : types) {
                if (!is_specializable(elem_type)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::TUPLE: {
            const auto &types = type->as<TupleType>()->types;
            for (const auto &elem_type : types) {
                if (!is_specializable(elem_type)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::OPTIONAL:
            return is_specializable(type->as<OptionalType>()->base_type);
        case Type::Variation::POINTER:
            return is_specializable(type->as<PointerType>()->base_type);
        case Type::Variation::COMPTIME:
        case Type::Variation::GENERIC:
        case Type::Variation::UNKNOWN:
            // Need a CVL to resolve these, so they cannot be applied/specialized on their own
            return false;
        case Type::Variation::DATA:
            return type->as<DataType>()->data_node->cpl.empty();
        case Type::Variation::FUNC:
            return type->as<FuncType>()->func_node->cpl.empty();
        case Type::Variation::INTERFACE:
            return type->as<InterfaceType>()->interface_node->cpl.empty();
        case Type::Variation::OBJECT:
            return type->as<ObjectType>()->object_node->cpl.empty();
        case Type::Variation::VARIANT: {
            const auto *const variant = type->as<VariantType>();
            if (std::holds_alternative<VariantNode *const>(variant->var_or_list)) {
                // Named variants are only specializable if the variant itself is not generic
                return std::get<VariantNode *const>(variant->var_or_list)->cpl.empty();
            }
            for (const auto &possible_type : std::get<std::vector<std::shared_ptr<Type>>>(variant->var_or_list)) {
                if (!is_specializable(possible_type)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::ENUM:
        case Type::Variation::ERROR_SET:
        case Type::Variation::OPAQUE:
        case Type::Variation::PRIMITIVE:
        case Type::Variation::RANGE:
        case Type::Variation::TYPE:
        case Type::Variation::VECTOR:
            // These types are never generic so they are always "specializable"
            return true;
    }
}

std::optional<DefinitionNode *> Specializer::pre_create(const DefinitionNode *const definition, const std::string &name) {
    Namespace *const src_ns = definition->file_hash.get_namespace();
    std::optional<DefinitionNode *> added_node = std::nullopt;
    switch (definition->get_variation()) {
        default:
            UNREACHABLE();
        case DefinitionNode::Variation::DATA: {
            const auto *const node = definition->as<DataNode>();
            std::vector<DataNode::Field> fields;
            DataNode new_node = DataNode( //
                node->file_hash,          //
                node->line,               //
                node->column,             //
                node->length,             //
                node->is_const,           //
                node->is_shared,          //
                name,                     //
                {},                       //
                fields                    //
            );
            const std::optional<DataNode *> added_data = src_ns->file_node->add_data(new_node);
            if (!added_data.has_value()) {
                return std::nullopt;
            }
            added_node = added_data;
            break;
        }
        case DefinitionNode::Variation::FUNC: {
            [[maybe_unused]] const auto *const node = definition->as<FuncNode>();
            UNREACHABLE();
            break;
        }
        case DefinitionNode::Variation::FUNCTION: {
            [[maybe_unused]] const auto *const node = definition->as<FunctionNode>();
            UNREACHABLE();
            break;
        }
        case DefinitionNode::Variation::INTERFACE: {
            [[maybe_unused]] const auto *const node = definition->as<InterfaceNode>();
            UNREACHABLE();
            break;
        }
        case DefinitionNode::Variation::OBJECT: {
            [[maybe_unused]] const auto *const node = definition->as<ObjectNode>();
            UNREACHABLE();
            break;
        }
        case DefinitionNode::Variation::VARIANT: {
            const auto *const node = definition->as<VariantNode>();
            std::vector<std::pair<std::optional<std::string>, std::shared_ptr<Type>>> possible_types;
            VariantNode new_node = VariantNode(node->file_hash, //
                node->line,                                     //
                node->column,                                   //
                node->length,                                   //
                name,                                           //
                {},                                             //
                possible_types                                  //
            );
            const std::optional<VariantNode *> added_variant = src_ns->file_node->add_variant(new_node);
            if (!added_variant.has_value()) {
                return std::nullopt;
            }
            added_node = added_variant;
            break;
        }
    }
    return added_node;
}

bool Specializer::specialize_data(                //
    DataNode *const node,                         //
    const DataNode *const definition,             //
    const std::vector<std::shared_ptr<Type>> &cvl //
) {
    ASSERT(definition->cpl.size() == cvl.size());
    for (const auto &field : definition->fields) {
        DataNode::Field new_field = {
            .name = field.name,
            .type = field.type,
            .initializer_tokens = field.initializer_tokens,
            .initializer = std::nullopt,
        };
        const auto &new_type = specialize_type(definition->file_hash.get_namespace(), new_field.type, definition->cpl, cvl);
        if (!new_type.has_value()) {
            return false;
        }
        if (new_type.value().first) {
            new_field.type = new_type.value().second;
        }
        if (field.initializer.has_value()) {
            new_field.initializer = field.initializer.value()->clone(0);
        }
        node->fields.emplace_back(std::move(new_field));
    }
    return true;
}

bool Specializer::specialize_variant(             //
    VariantNode *const node,                      //
    const VariantNode *const definition,          //
    const std::vector<std::shared_ptr<Type>> &cvl //
) {
    ASSERT(definition->cpl.size() == cvl.size());
    for (const auto &[type_tag, type] : definition->possible_types) {
        const auto &new_type = specialize_type(definition->file_hash.get_namespace(), type, definition->cpl, cvl);
        if (!new_type.has_value()) {
            return false;
        }
        node->possible_types.emplace_back(type_tag, new_type.value().second);
    }
    return true;
}

std::optional<std::pair<bool, std::shared_ptr<Type>>> Specializer::specialize_type( //
    Namespace *const ns,                                                            //
    const std::shared_ptr<Type> &type,                                              //
    const std::vector<DefinitionNode::ComptimeParameter> &cpl,                      //
    const std::vector<std::shared_ptr<Type>> &cvl                                   //
) {
    switch (type->get_variation()) {
        case Type::Variation::ALIAS: {
            // Always resolve the alias no matter if specialization took place
            const auto *const alias = type->as<AliasType>();
            const auto &child = specialize_type(ns, alias->type, cpl, cvl);
            if (!child.has_value() || child.value().first) {
                return child;
            }
            return make_pair(false, alias->type);
        }
        case Type::Variation::ARRAY: {
            const auto *const array = type->as<ArrayType>();
            const auto &child = specialize_type(ns, array->type, cpl, cvl);
            if (!child.has_value()) {
                return child;
            }
            if (child.value().first) {
                std::shared_ptr<Type> new_type = std::make_shared<ArrayType>(array->dimensionality, child.value().second, array->sizes);
                if (!ns->add_type(new_type)) {
                    new_type = ns->get_type_from_str(new_type->to_string()).value();
                }
                return make_pair(true, new_type);
            }
            break;
        }
        case Type::Variation::COMPTIME: {
            const auto *const comptime = type->as<ComptimeType>();
            for (size_t i = 0; i < cpl.size(); i++) {
                if (cpl.at(i).name == comptime->name) {
                    return make_pair(true, cvl.at(i));
                }
            }
            // This case is unreachable because of how comptime types are created. If no comptime parameter with the same identifier as
            // the comptime type exists, no comptime type is created but instead an unknown type is created. This means that all
            // comptime types are always guaranteed to be present in the CPL. If we reach this code something went wrong.
            UNREACHABLE();
            break;
        }
        case Type::Variation::DATA: {
            const auto *const data = type->as<DataType>();
            if (!data->data_node->cpl.empty()) {
                return std::nullopt;
            }
            break;
        }
        case Type::Variation::FUNC: {
            const auto *const func = type->as<FuncType>();
            if (!func->func_node->cpl.empty()) {
                return std::nullopt;
            }
            break;
        }
        case Type::Variation::FN: {
            const auto *const fn = type->as<FnType>();
            bool any_changed = false;
            std::vector<std::shared_ptr<Type>> param_types;
            for (const auto &[param_type, is_mutable] : fn->params) {
                param_types.emplace_back(param_type);
            }
            auto was_specialized = specialize_type_list(ns, param_types, cpl, cvl);
            if (!was_specialized.has_value()) {
                return std::nullopt;
            }
            any_changed |= was_specialized.value();

            std::vector<std::shared_ptr<Type>> return_types = fn->return_types;
            was_specialized = specialize_type_list(ns, return_types, cpl, cvl);
            if (!was_specialized.has_value()) {
                return std::nullopt;
            }
            any_changed |= was_specialized.value();
            if (any_changed) {
                std::vector<std::pair<std::shared_ptr<Type>, bool>> params;
                for (size_t i = 0; i < fn->params.size(); i++) {
                    params.emplace_back(param_types.at(i), fn->params.at(i).second);
                }
                std::shared_ptr<Type> new_fn = std::make_shared<FnType>(params, return_types, fn->error_types);
                if (!ns->add_type(new_fn)) {
                    new_fn = ns->get_type_from_str(new_fn->to_string()).value();
                }
                return make_pair(true, new_fn);
            }
            break;
        }
        case Type::Variation::GENERIC: {
            const auto *const generic = type->as<GenericType>();
            std::vector<std::shared_ptr<Type>> new_cvl;
            new_cvl.reserve(generic->cvl.size());
            for (const auto &arg : generic->cvl) {
                const auto &child = specialize_type(ns, arg, cpl, cvl);
                if (!child.has_value()) {
                    return std::nullopt;
                }
                new_cvl.emplace_back(child.value().second);
            }
            const auto &specialized = specialize(ns, generic->base, new_cvl);
            if (specialized.has_value()) {
                return make_pair(true, specialized.value());
            }
            // Residual generics/comptimes cannot be made concrete here
            return std::nullopt;
        }
        case Type::Variation::GROUP: {
            const auto *const group = type->as<GroupType>();
            auto types = group->types;
            const auto was_specialized = specialize_type_list(ns, types, cpl, cvl);
            if (!was_specialized.has_value()) {
                return std::nullopt;
            }
            if (was_specialized.value()) {
                std::shared_ptr<Type> new_tuple = std::make_shared<GroupType>(types);
                if (!ns->add_type(new_tuple)) {
                    new_tuple = ns->get_type_from_str(new_tuple->to_string()).value();
                }
                return std::make_pair(true, new_tuple);
            }
            break;
        }
        case Type::Variation::INTERFACE: {
            const auto *const interface = type->as<InterfaceType>();
            if (!interface->interface_node->cpl.empty()) {
                return std::nullopt;
            }
            break;
        }
        case Type::Variation::OBJECT: {
            const auto *const object = type->as<ObjectType>();
            if (!object->object_node->cpl.empty()) {
                return std::nullopt;
            }
            break;
        }
        case Type::Variation::OPTIONAL: {
            const auto *const optional = type->as<OptionalType>();
            const auto &child = specialize_type(ns, optional->base_type, cpl, cvl);
            if (!child.has_value()) {
                return child;
            }
            if (child.value().first) {
                std::shared_ptr<Type> new_type = std::make_shared<OptionalType>(child.value().second);
                if (!ns->add_type(new_type)) {
                    new_type = ns->get_type_from_str(new_type->to_string()).value();
                }
                return make_pair(true, new_type);
            }
            break;
        }
        case Type::Variation::POINTER: {
            const auto *const pointer = type->as<PointerType>();
            const auto &child = specialize_type(ns, pointer->base_type, cpl, cvl);
            if (!child.has_value()) {
                return child;
            }
            if (child.value().first) {
                std::shared_ptr<Type> new_type = std::make_shared<PointerType>(child.value().second);
                if (!ns->add_type(new_type)) {
                    new_type = ns->get_type_from_str(new_type->to_string()).value();
                }
                return make_pair(true, new_type);
            }
            break;
        }
        case Type::Variation::TUPLE: {
            const auto *const tuple = type->as<TupleType>();
            auto types = tuple->types;
            const auto &was_specialized = specialize_type_list(ns, types, cpl, cvl);
            if (!was_specialized.has_value()) {
                return std::nullopt;
            }
            if (was_specialized.value()) {
                std::shared_ptr<Type> new_type = std::make_shared<TupleType>(types);
                if (!ns->add_type(new_type)) {
                    new_type = ns->get_type_from_str(new_type->to_string()).value();
                }
                return std::make_pair(true, new_type);
            }
            break;
        }
        case Type::Variation::VARIANT: {
            const auto *const variant = type->as<VariantType>();
            if (variant->is_err_variant) {
                break;
            }
            if (std::holds_alternative<VariantNode *const>(variant->var_or_list)) {
                if (!std::get<VariantNode *const>(variant->var_or_list)->cpl.empty()) {
                    return std::nullopt;
                }
            } else {
                auto types = std::get<std::vector<std::shared_ptr<Type>>>(variant->var_or_list);
                const auto &was_specialized = specialize_type_list(ns, types, cpl, cvl);
                if (!was_specialized.has_value()) {
                    return std::nullopt;
                }
                if (was_specialized.value()) {
                    std::shared_ptr<Type> new_variant = std::make_shared<VariantType>(types, false);
                    if (!ns->add_type(new_variant)) {
                        new_variant = ns->get_type_from_str(new_variant->to_string()).value();
                    }
                    return make_pair(true, new_variant);
                }
            }
            break;
        }
        case Type::Variation::ENUM:
        case Type::Variation::ERROR_SET:
        case Type::Variation::OPAQUE:
        case Type::Variation::PRIMITIVE:
        case Type::Variation::RANGE:
        case Type::Variation::UNKNOWN:
        case Type::Variation::TYPE:
        case Type::Variation::VECTOR:
            // These types are always guaranteed to be non-generic and cannot contain any other types which may be generic either
            break;
    }
    // No specialization applied, return pair contining the type + info that nothing was specialized
    return make_pair(false, type);
}

std::optional<bool> Specializer::specialize_type_list(         //
    Namespace *const ns,                                       //
    std::vector<std::shared_ptr<Type>> &types,                 //
    const std::vector<DefinitionNode::ComptimeParameter> &cpl, //
    const std::vector<std::shared_ptr<Type>> &cvl              //
) {
    bool changed = false;
    for (auto &type : types) {
        auto new_type = specialize_type(ns, type, cpl, cvl);
        if (!new_type.has_value()) {
            return std::nullopt;
        }
        if (new_type.value().first) {
            type = new_type.value().second;
            changed = true;
        }
    }
    return changed;
}
