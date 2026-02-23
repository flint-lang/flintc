#include "parser/parser.hpp"

#include "debug.hpp"
#include "globals.hpp"
#include "lexer/builtins.hpp"
#include "lexer/lexer.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/opaque_type.hpp"
#include "parser/type/unknown_type.hpp"
#include "persistent_thread_pool.hpp"
#include "profiler.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

void Parser::init_core_modules() {
    for (const auto &[module_name_view, overload_list] : core_module_functions) {
        const std::string module_name(module_name_view);
        assert(core_namespaces.find(module_name) == core_namespaces.end());
        core_namespaces[module_name] = std::make_unique<Namespace>(module_name);
        std::unique_ptr<Namespace> &core_namespace = core_namespaces.at(module_name);
        Resolver::namespace_map[core_namespace->namespace_hash] = core_namespace.get();
        auto &types = core_namespace->public_symbols.types;
        // First go through all types the core module defines and add them to the namespace
        // - Add all error set types
        if (core_module_error_sets.find(module_name) != core_module_error_sets.end()) {
            for (const auto &error_set_type : core_module_error_sets.at(module_name)) {
                const std::string error_set_name(std::get<0>(error_set_type));
                assert(types.find(error_set_name) == types.end());
                const std::string error_set_parent_name(std::get<1>(error_set_type));
                std::vector<std::string> values;
                std::vector<std::string> default_messages;
                for (const auto &[value, msg] : std::get<2>(error_set_type)) {
                    values.emplace_back(value);
                    default_messages.emplace_back(msg);
                }
                std::unique_ptr<DefinitionNode> error_set = std::make_unique<ErrorNode>(                                     //
                    core_namespace->namespace_hash, 0, 0, 0, error_set_name, error_set_parent_name, values, default_messages //
                );
                core_namespace->public_symbols.definitions.emplace_back(std::move(error_set));
                ErrorNode *const err_ptr = core_namespace->public_symbols.definitions.back()->as<ErrorNode>();
                types[error_set_name] = std::make_shared<ErrorSetType>(err_ptr);
            }
        }
        // - Add all enum types
        if (core_module_enum_types.find(module_name) != core_module_enum_types.end()) {
            for (const auto &[enum_name_view, enum_values] : core_module_enum_types.at(module_name)) {
                const std::string enum_name(enum_name_view);
                assert(types.find(enum_name) == types.end());
                std::vector<std::pair<std::string, unsigned int>> values;
                for (const auto &[enum_tag, enum_value] : enum_values) {
                    values.emplace_back(enum_tag, enum_value);
                }
                std::unique_ptr<DefinitionNode> enum_node = std::make_unique<EnumNode>( //
                    core_namespace->namespace_hash, 0, 0, 0, enum_name, values          //
                );
                core_namespace->public_symbols.definitions.emplace_back(std::move(enum_node));
                EnumNode *const enum_ptr = core_namespace->public_symbols.definitions.back()->as<EnumNode>();
                types[enum_name] = std::make_shared<EnumType>(enum_ptr);
            }
        }
        // - Add all data types
        if (core_module_data_types.find(module_name) != core_module_data_types.end()) {
            for (const auto &data_type : core_module_data_types.at(module_name)) {
                const std::string data_type_name(std::get<0>(data_type));
                assert(types.find(data_type_name) == types.end());
                const auto &field_pairs = std::get<1>(data_type);
                std::vector<DataNode::Field> fields;
                for (const auto &[field_type_view, field_name_view] : field_pairs) {
                    std::optional<std::shared_ptr<Type>> field_type = core_namespace->get_type_from_str(std::string(field_type_view));
                    assert(field_type.has_value());
                    fields.emplace_back(DataNode::Field{
                        .name = std::string(field_name_view),
                        .type = field_type.value(),
                        .initializer_tokens = std::nullopt,
                        .initializer = std::nullopt,
                    });
                }
                std::unique_ptr<DefinitionNode> data = std::make_unique<DataNode>( //
                    core_namespace->namespace_hash, 0, 0, 0,                       //
                    false, false, data_type_name, fields                           //
                );
                core_namespace->public_symbols.definitions.emplace_back(std::move(data));
                DataNode *const data_ptr = core_namespace->public_symbols.definitions.back()->as<DataNode>();
                types[data_type_name] = std::make_shared<DataType>(data_ptr);
            }
        }

        // Then create and add all function definitions to the public definition list of the namespace
        for (const auto &[function_name_view, overloads] : overload_list) {
            const std::string function_name(function_name_view);
            for (const auto &overload : overloads) {
                const auto &parameters_str = std::get<0>(overload);
                std::vector<std::tuple<std::shared_ptr<Type>, std::string, bool>> parameters;
                for (const auto &[param_type_view, param_name_view] : parameters_str) {
                    const std::string param_type_str(param_type_view);
                    const std::string param_name_str(param_name_view);
                    const std::shared_ptr<Type> param_type = core_namespace->get_type_from_str(param_type_str).value();
                    parameters.emplace_back(param_type, param_name_str, false);
                }

                const auto &return_types_str = std::get<1>(overload);
                std::vector<std::shared_ptr<Type>> return_types;
                for (const auto &return_view : return_types_str) {
                    const std::string return_str(return_view);
                    const std::shared_ptr<Type> return_type = core_namespace->get_type_from_str(return_str).value();
                    return_types.emplace_back(return_type);
                }

                const auto &error_types_str = std::get<2>(overload);
                std::vector<std::shared_ptr<Type>> error_types;
                for (const auto &error_view : error_types_str) {
                    const std::string error_str(error_view);
                    const std::shared_ptr<Type> error_type = core_namespace->get_type_from_str(error_str).value();
                    error_types.emplace_back(error_type);
                }

                std::optional<std::shared_ptr<Scope>> scope = std::nullopt;
                std::unique_ptr<DefinitionNode> function = std::make_unique<FunctionNode>( //
                    core_namespace->namespace_hash, 0, 0, 0,                               //
                    false, false, false, true, function_name,                              //
                    parameters, return_types, error_types, scope, std::nullopt             //
                );
                core_namespace->public_symbols.definitions.emplace_back(std::move(function));
            }
        }
    }
}

std::optional<Parser *> Parser::create(const std::filesystem::path &file) {
    try {
        instances.emplace_back(Parser(file));
        return &instances.back();
    } catch ([[maybe_unused]] const std::exception &e) { return std::nullopt; }
}

Parser *Parser::create(const std::filesystem::path &file, const std::string &file_content) {
    PROFILE_CUMULATIVE("Parser::create");
    instances.emplace_back(Parser(file, file_content));
    return &instances.back();
}

bool Parser::file_exists_and_is_readable(const std::filesystem::path &file_path) {
    PROFILE_CUMULATIVE("Parser::file_exists_and_is_readable");
    std::ifstream file(file_path.string());
    return file.is_open() && !file.fail();
}

std::string Parser::load_file(const std::filesystem::path &file_path) {
    PROFILE_CUMULATIVE("Parser::load_file");
    std::ifstream file(file_path.string());
    if (!file) {
        throw std::runtime_error("Failed to load file " + file_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::optional<FileNode *> Parser::parse() {
    PROFILE_SCOPE("Parse file '" + file.filename().string() + "'");
    file_node_ptr = std::make_unique<FileNode>(file);
    {
        std::lock_guard<std::mutex> lock(Resolver::namespace_map_mutex);
        assert(Resolver::namespace_map.find(file_node_ptr->file_namespace->namespace_hash) == Resolver::namespace_map.end());
        Resolver::namespace_map.emplace(file_node_ptr->file_namespace->namespace_hash, file_node_ptr->file_namespace.get());
    }
    file_node_ptr->file_namespace->file_node = file_node_ptr.get();
    Lexer lexer(file, *source_code.get());
    file_node_ptr->tokens = lexer.scan();
    if (file_node_ptr->tokens.empty()) {
        return std::nullopt;
    }
    source_code_lines = lexer.lines;
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Lexer lines of file '" << file_name << "'" << DEFAULT << std::endl;
        unsigned int line_idx = 1;
        const size_t last_line_width = std::to_string(source_code_lines.size()).size();
        for (const auto &line : source_code_lines) {
            std::cout << std::left << std::setw(last_line_width) << std::to_string(line_idx) << " | " << std::string(line.second);
            line_idx++;
        }
    }
    token_slice token_slice = {file_node_ptr->tokens.begin(), file_node_ptr->tokens.end()};
    if (PRINT_TOK_STREAM) {
        Debug::print_token_context_vector(token_slice, file_name);
    }
    // Consume all tokens and convert them to nodes
    bool had_failure = false;
    while (token_slice.first != token_slice.second && token_slice.first->token != TOK_EOF) {
        if (!add_next_main_node(*(file_node_ptr.get()), token_slice)) {
            had_failure = true;
        }
    }
    if (had_failure) {
        return std::nullopt;
    }
    return file_node_ptr.get();
}

Parser::CastDirection Parser::check_primitive_castability( //
    const std::shared_ptr<Type> &lhs_type,                 //
    const std::shared_ptr<Type> &rhs_type,                 //
    const bool is_implicit                                 //
) {
    PROFILE_CUMULATIVE("Parser::check_primitive_castability");
    const std::string lhs_str = lhs_type->to_string();
    const std::string rhs_str = rhs_type->to_string();
    assert(lhs_str != rhs_str);

    // Check if both sides are literals, cast int to float in that case
    if (lhs_str == "int" && rhs_str == "float") {
        // Cast lhs (int) to rhs (float)
        return CastDirection::lhs_to_rhs();
    } else if (lhs_str == "float" && rhs_str == "int") {
        // Cast rhs (int) to lhs (float)
        return CastDirection::rhs_to_lhs();
    } else if (lhs_str == "str" && rhs_type->get_variation() == Type::Variation::ENUM) {
        // Enums are always castable to strings
        return CastDirection::rhs_to_lhs();
    } else if (lhs_str == "str" && rhs_type->get_variation() == Type::Variation::ERROR_SET) {
        // Error sets are always castable to strings
        return CastDirection::rhs_to_lhs();
    } else if (lhs_str == "str" && rhs_str == "anyerror") {
        // The 'anyerror' Error can be cast to an string too obviously
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
        ErrorNode *lhs_err = lhs_type->as<ErrorSetType>()->error_node;
        ErrorNode *rhs_err = rhs_type->as<ErrorSetType>()->error_node;
        // It is always allowed to cast to more specific errors if the more specific error is the superset of the basic error
        if (lhs_err->is_parent_of(rhs_err)) {
            return CastDirection::lhs_to_rhs();
        }
        if (rhs_err->is_parent_of(lhs_err)) {
            return CastDirection::rhs_to_lhs();
        }
        return CastDirection::not_castable();
    }

    // Check if one side is an entity and the other side is a func module and if the func module is contained within the entity type
    if (lhs_type->get_variation() == Type::Variation::ENTITY && rhs_type->get_variation() == Type::Variation::FUNC) {
        EntityNode *lhs_ent = lhs_type->as<EntityType>()->entity_node;
        FuncNode *rhs_func = rhs_type->as<FuncType>()->func_node;
        for (const auto &func_module_ptr : lhs_ent->func_modules) {
            if (rhs_func == func_module_ptr) {
                return CastDirection::lhs_to_rhs();
            }
        }
    } else if (lhs_type->get_variation() == Type::Variation::FUNC && rhs_type->get_variation() == Type::Variation::ENTITY) {
        FuncNode *lhs_func = lhs_type->as<FuncType>()->func_node;
        EntityNode *rhs_ent = rhs_type->as<EntityType>()->entity_node;
        for (const auto &func_module_ptr : rhs_ent->func_modules) {
            if (lhs_func == func_module_ptr) {
                return CastDirection::rhs_to_lhs();
            }
        }
    }

    bool lhs_to_rhs_allowed = false;
    bool rhs_to_lhs_allowed = false;
    if (is_implicit) {
        // Check if lhs can implicitly cast to rhs
        auto lhs_cast_it = primitive_implicit_casting_table.find(lhs_str);
        if (lhs_cast_it != primitive_implicit_casting_table.end()) {
            const auto &lhs_targets = lhs_cast_it->second;
            auto it = std::find(lhs_targets.begin(), lhs_targets.end(), rhs_str);
            if (it != lhs_targets.end()) {
                lhs_to_rhs_allowed = true;
            }
        }
        // Check if rhs can implicitly cast to lhs
        auto rhs_cast_it = primitive_implicit_casting_table.find(rhs_str);
        if (rhs_cast_it != primitive_implicit_casting_table.end()) {
            const auto &rhs_targets = rhs_cast_it->second;
            auto it = std::find(rhs_targets.begin(), rhs_targets.end(), lhs_str);
            if (it != rhs_targets.end()) {
                rhs_to_lhs_allowed = true;
            }
        }
    } else {
        // Check if lhs can explicitely cast to rhs
        auto lhs_cast_it = primitive_casting_table.find(lhs_str);
        if (lhs_cast_it != primitive_casting_table.end()) {
            const auto &lhs_targets = lhs_cast_it->second;
            auto it = std::find(lhs_targets.begin(), lhs_targets.end(), rhs_str);
            if (it != lhs_targets.end()) {
                lhs_to_rhs_allowed = true;
            }
        }
        // Check if rhs can explicitely cast to lhs
        auto rhs_cast_it = primitive_casting_table.find(rhs_str);
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
    auto lhs_size_it = type_size.find(lhs_str);
    auto rhs_size_it = type_size.find(rhs_str);
    if (lhs_size_it != type_size.end() && rhs_size_it != type_size.end()) {
        int lhs_bits = lhs_size_it->second;
        int rhs_bits = rhs_size_it->second;

        if (lhs_bits < rhs_bits) {
            // Cast smaller lhs to larger rhs
            return CastDirection::lhs_to_rhs();
        }
        if (rhs_bits < lhs_bits) {
            // Cast smaller rhs to larger lhs
            return CastDirection::rhs_to_lhs();
        }
    }

    // Default fallback - cast lhs to rhs (left-to-right bias)
    return CastDirection::lhs_to_rhs();
}

Parser::CastDirection Parser::check_castability( //
    const std::shared_ptr<Type> &lhs_type,       //
    const std::shared_ptr<Type> &rhs_type,       //
    const bool is_implicit                       //
) {
    PROFILE_CUMULATIVE("Parser::check_castability_type");
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
        // If one of them is a multi-type, the other one has to be a single value with the same type as the base type of the mutli-type
        const MultiType *lhs_mult = dynamic_cast<const MultiType *>(lhs_type.get());
        const MultiType *rhs_mult = dynamic_cast<const MultiType *>(rhs_type.get());
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
        // Check if one or both of the sides are variant types, the other side then needs to be one of the possible variant types
        const VariantType *lhs_var = dynamic_cast<const VariantType *>(lhs_type.get());
        const VariantType *rhs_var = dynamic_cast<const VariantType *>(rhs_type.get());
        if (lhs_var != nullptr && rhs_var != nullptr) {
            // Variants of different types cannot be cast to one another in any way
            THROW_BASIC_ERR(ERR_PARSING);
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
        return check_primitive_castability(lhs_type, rhs_type, is_implicit);
    } else if (lhs_group == nullptr && rhs_group != nullptr) {
        // Left is no group, right is group
        switch (lhs_type->get_variation()) {
            default:
                return CastDirection::not_castable();
            case Type::Variation::MULTI: {
                const auto *lhs_mult = lhs_type->as<MultiType>();
                // If left is a multi-type, then the right is castable to the left and the left to the right
                // The group must have the same size as the multi-type
                if (lhs_mult->width != rhs_group->types.size()) {
                    return CastDirection::not_castable();
                }
                // All elements in the group must have the same type as the multi-type or be implicitely castable to it (for example
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

bool Parser::is_castable_to(const std::shared_ptr<Type> &from, const std::shared_ptr<Type> &to, const bool is_implicit) {
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

bool Parser::check_castability(const std::shared_ptr<Type> &target_type, std::unique_ptr<ExpressionNode> &expr, const bool is_implicit) {
    PROFILE_CUMULATIVE("Parser::check_castability_expr_inplace");
    if (target_type->get_variation() == Type::Variation::ALIAS) {
        const auto *alias_type = target_type->as<AliasType>();
        return check_castability(alias_type->type, expr, is_implicit);
    }
    if (expr->type->equals(target_type)) {
        return true;
    }
    const std::string expr_type_str = expr->type->to_string();
    const std::string target_type_str = target_type->to_string();
    if (expr_type_str == "type.flint.str.lit" && target_type_str == "str") {
        expr = std::make_unique<TypeCastNode>(target_type, expr);
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
                            if (elem_type_str == "int" || elem_type_str == "float") {
                                elem_expr->type = target_elem_type;
                            } else {
                                elem_expr = std::make_unique<TypeCastNode>(target_elem_type, elem_expr);
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
                if (!file_node_ptr->file_namespace->add_type(new_group_type)) {
                    new_group_type = file_node_ptr->file_namespace->get_type_from_str(new_group_type->to_string()).value();
                }
                expr->type = new_group_type;
            }
            if (expr->type->equals(target_type)) {
                return true;
            }
            expr = std::make_unique<TypeCastNode>(target_type, expr);
            return true;
        }
    }

    switch (target_type->get_variation()) {
        case Type::Variation::ALIAS:
            assert(false);
            break;
        case Type::Variation::ARRAY:
        case Type::Variation::DATA:
        case Type::Variation::ENTITY:
        case Type::Variation::ENUM:
        case Type::Variation::ERROR_SET:
        case Type::Variation::FUNC:
        case Type::Variation::POINTER:
        case Type::Variation::RANGE:
            expr = std::make_unique<TypeCastNode>(target_type, expr);
            return true;
        case Type::Variation::GROUP: {
            if (expr->type->get_variation() == Type::Variation::MULTI) {
                const auto *expr_multi = expr->type->as<MultiType>();
                const auto *target_group = target_type->as<GroupType>();
                if (expr_multi->width != target_group->types.size()) {
                    return false;
                }
                for (size_t i = 0; i < target_group->types.size(); i++) {
                    if (!expr_multi->base_type->equals(target_group->types[i])) {
                        return false;
                    }
                }
                expr = std::make_unique<TypeCastNode>(target_type, expr);
                return true;
            }
            expr = std::make_unique<TypeCastNode>(target_type, expr);
            return true;
        }
        case Type::Variation::MULTI: {
            const auto *multi_type = target_type->as<MultiType>();
            if (multi_type->to_string() == "bool8" && expr->type->to_string() == "u8") {
                expr = std::make_unique<TypeCastNode>(target_type, expr);
                return true;
            }
            if (expr->get_variation() != ExpressionNode::Variation::GROUP_EXPRESSION) {
                // If rhs type is a single value of type 'int' or 'float' then it can be splatted to the lhs type
                CastDirection::Kind primitive_castability = CastDirection::Kind::SAME_TYPE;
                if (!multi_type->base_type->equals(expr->type)) {
                    primitive_castability = check_primitive_castability(multi_type->base_type, expr->type).kind;
                }
                const bool rhs_is_able_to_swizzle =                                     //
                    primitive_castability == CastDirection::Kind::CAST_RHS_TO_LHS       //
                    || primitive_castability == CastDirection::Kind::CAST_BIDIRECTIONAL //
                    || primitive_castability == CastDirection::Kind::SAME_TYPE;
                if (rhs_is_able_to_swizzle) {
                    if (expr->type->to_string() == "int" || expr->type->to_string() == "float") {
                        expr->type = multi_type->base_type;
                    }
                    expr = std::make_unique<TypeCastNode>(target_type, expr);
                    return true;
                }
                return false;
            }
            auto *group_expr = expr->as<GroupExpressionNode>();
            if (group_expr->expressions.size() != multi_type->width) {
                return false;
            }

            bool any_element_changed = false;
            for (auto &elem_expr : group_expr->expressions) {
                if (!elem_expr->type->equals(multi_type->base_type)) {
                    const std::string elem_type_str = elem_expr->type->to_string();
                    if (elem_type_str == "int" || elem_type_str == "float") {
                        elem_expr->type = multi_type->base_type;
                        any_element_changed = true;
                    } else {
                        if (elem_expr->type->equals(multi_type->base_type)) {
                            continue;
                        }
                        const CastDirection elem_cast = check_primitive_castability(multi_type->base_type, elem_expr->type, is_implicit);
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
                        elem_expr = std::make_unique<TypeCastNode>(multi_type->base_type, elem_expr);
                        any_element_changed = true;
                    }
                }
            }
            if (any_element_changed) {
                std::vector<std::shared_ptr<Type>> new_element_types(multi_type->width, multi_type->base_type);
                std::shared_ptr<Type> new_group_type = std::make_shared<GroupType>(new_element_types);
                if (!file_node_ptr->file_namespace->add_type(new_group_type)) {
                    new_group_type = file_node_ptr->file_namespace->get_type_from_str(new_group_type->to_string()).value();
                }
                expr->type = new_group_type;
            }
            expr = std::make_unique<TypeCastNode>(target_type, expr);
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
                    expr = std::make_unique<TypeCastNode>(target_type, expr);
                    return true;
                case Type::Variation::POINTER:
                    expr = std::make_unique<TypeCastNode>(target_type, expr);
                    return true;
            }
        }
        case Type::Variation::OPTIONAL: {
            const auto *optional_type = target_type->as<OptionalType>();
            if (expr->type->equals(optional_type->base_type)) {
                expr = std::make_unique<TypeCastNode>(target_type, expr);
                return true;
            }
            if (expr->type->get_variation() == Type::Variation::OPTIONAL) {
                const auto *expr_opt = expr->type->as<OptionalType>();
                if (expr_opt->base_type->equals(Type::get_primitive_type("void"))) {
                    expr = std::make_unique<TypeCastNode>(target_type, expr);
                    return true;
                }
                return false;
            }

            const CastDirection base_cast = check_castability(optional_type->base_type, expr->type, is_implicit);
            switch (base_cast.kind) {
                case CastDirection::Kind::SAME_TYPE:
                case CastDirection::Kind::CAST_BIDIRECTIONAL:
                case CastDirection::Kind::CAST_RHS_TO_LHS:
                    if (expr_type_str == "int" || expr_type_str == "float") {
                        expr->type = optional_type->base_type;
                        expr = std::make_unique<TypeCastNode>(target_type, expr);
                    } else {
                        expr = std::make_unique<TypeCastNode>(optional_type->base_type, expr);
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
                    expr = std::make_unique<LiteralNode>(new_value, Type::get_primitive_type("type.flint.str.lit"), literal->is_folded);
                    expr = std::make_unique<TypeCastNode>(Type::get_primitive_type("str"), expr);
                    expr->line = line;
                    expr->column = column;
                    expr->length = length;
                    return true;
                }
            }
            if (expr_type_str == "int" || expr_type_str == "float") {
                expr->type = target_type;
            } else {
                expr = std::make_unique<TypeCastNode>(target_type, expr);
            }
            return true;
        }
        case Type::Variation::TUPLE:
            expr = std::make_unique<TypeCastNode>(target_type, expr);
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
            expr = std::make_unique<TypeCastNode>(target_type, expr);
            return true;
        }
    }
    return true;
}

bool Parser::resolve_all_imports() {
    PROFILE_CUMULATIVE("Parser::resolve_all_imports");
    for (const auto &instance : instances) {
        const auto &file_namespace = instance.file_node_ptr->file_namespace;
#ifdef FLINT_LSP
        // Skip every file that's not the main file
        if (file_namespace->file_path != main_file_path) {
            continue;
        }
#endif
        const auto &imports = file_namespace->public_symbols.imports;
        for (const auto &import : imports) {
            Namespace *imported_namespace = nullptr;
            if (std::holds_alternative<Hash>(import->path)) {
                const Hash &import_hash = std::get<Hash>(import->path);
                imported_namespace = Resolver::get_namespace_from_hash(import_hash);
            } else {
                // Check if it's a core module, continue if it's not
                const auto &import_segments = std::get<std::vector<std::string>>(import->path);
                if (import_segments.size() > 2 || import_segments.front() != "Core") {
                    continue;
                }
                // Not-available core modules should have been caught some time earlier
                assert(Parser::core_namespaces.find(import_segments.back()) != Parser::core_namespaces.end());
                imported_namespace = Parser::core_namespaces.at(import_segments.back()).get();
            }
            // Only update the alias map if the import is aliased but do not add any symbols from the namespace in here
            if (import->alias.has_value()) {
                auto &aliased_imports = file_namespace->public_symbols.aliased_imports;
                aliased_imports[import->alias.value()] = imported_namespace;
                continue;
            }
            // Place all symbols of non-aliased imports to the private symbol list
            // Place all defined types in the private types map and all functions in the function map
            const Hash import_hash = imported_namespace->namespace_hash;
            auto &private_symbols = file_namespace->private_symbols;
            for (const auto &definition : imported_namespace->public_symbols.definitions) {
                switch (definition->get_variation()) {
                    default:
                        break;
                    case DefinitionNode::Variation::DATA: {
                        auto *node = definition->as<DataNode>();
                        if (!file_namespace->get_type_from_str(node->name).has_value()) {
                            std::shared_ptr<Type> data_type = std::make_shared<DataType>(node);
                            private_symbols.types[data_type->to_string()] = data_type;
                        }
                        break;
                    }
                    case DefinitionNode::Variation::ENUM: {
                        auto *node = definition->as<EnumNode>();
                        if (!file_namespace->get_type_from_str(node->name).has_value()) {
                            std::shared_ptr<Type> enum_type = std::make_shared<EnumType>(node);
                            private_symbols.types[enum_type->to_string()] = enum_type;
                        }
                        break;
                    }
                    case DefinitionNode::Variation::ERROR: {
                        auto *node = definition->as<ErrorNode>();
                        if (!file_namespace->get_type_from_str(node->name).has_value()) {
                            std::shared_ptr<Type> error_type = std::make_shared<ErrorSetType>(node);
                            private_symbols.types[error_type->to_string()] = error_type;
                        }
                        break;
                    }
                    case DefinitionNode::Variation::FUNCTION: {
                        auto *function = definition->as<FunctionNode>();
                        if (private_symbols.functions.find(import_hash) == private_symbols.functions.end()) {
                            private_symbols.functions[import_hash].emplace_back(function);
                        } else {
                            private_symbols.functions.at(import_hash).emplace_back(function);
                        }
                        break;
                    }
                    case DefinitionNode::Variation::VARIANT: {
                        auto *node = definition->as<VariantNode>();
                        if (!file_namespace->get_type_from_str(node->name).has_value()) {
                            // const std::variant<VariantNode *const, std::vector<std::shared_ptr<Type>>> var_or_list = node;
                            std::shared_ptr<Type> variant_type = std::make_shared<VariantType>(node, false);
                            private_symbols.types[variant_type->to_string()] = variant_type;
                        }
                        break;
                    }
                }
            }
        }
    }
    return true;
}

bool Parser::resolve_all_unknown_types() {
    PROFILE_CUMULATIVE("Parser::resolve_all_unknown_types");
    // First go through all the parameters of the function and resolve their type if they are of unknown type
    for (auto &parser : instances) {
        Namespace *file_namespace = parser.file_node_ptr->file_namespace.get();
        for (auto &definition : file_namespace->public_symbols.definitions) {
            // Resolve all types of the definitions first
            switch (definition->get_variation()) {
                default:
                    break;
                case DefinitionNode::Variation::FUNCTION: {
                    auto *function_node = definition->as<FunctionNode>();
                    if (!function_node->is_extern) {
                        break;
                    }
                    // Resolve all argument and return type aliases
                    for (auto &param : function_node->parameters) {
                        if (!file_namespace->resolve_type(std::get<0>(param))) {
                            return false;
                        }
                    }
                    // Resolve all return types of the function
                    for (auto &ret : function_node->return_types) {
                        if (!file_namespace->resolve_type(ret)) {
                            return false;
                        }
                    }
                    break;
                }
                case DefinitionNode::Variation::VARIANT: {
                    auto *variant_node = definition->as<VariantNode>();
                    for (auto &type : variant_node->possible_types) {
                        if (!file_namespace->resolve_type(type.second)) {
                            return false;
                        }
                    }
                    break;
                }
                case DefinitionNode::Variation::DATA: {
                    auto *data_node = definition->as<DataNode>();
                    for (auto &field : data_node->fields) {
                        if (!file_namespace->resolve_type(field.type)) {
                            return false;
                        }
                    }
                    break;
                }
            }
        }
        // Resolve the parameter and return types of all functions
        for (FunctionNode *function : parser.get_open_functions()) {
            for (auto &param : function->parameters) {
                if (!file_namespace->resolve_type(std::get<0>(param))) {
                    return false;
                }
            }
            // Resolve all return types of the function
            for (auto &ret : function->return_types) {
                if (!file_namespace->resolve_type(ret)) {
                    return false;
                }
            }
            // Don't check local variables since extern functions don't have a body
            if (function->is_extern) {
                continue;
            }
            // The parameters are added to the list of variables to the functions scope, so we need to change the types there too
            for (auto &[variable_name, variable] : function->scope.value()->variables) {
                if (!file_namespace->resolve_type(variable.type)) {
                    return false;
                }
            }
        }
    }
    return true;
}

std::vector<FunctionNode *> Parser::get_open_functions() {
    PROFILE_CUMULATIVE("Parser::get_open_functions");
    std::vector<FunctionNode *> open_function_list;
    for (auto &open_function : open_functions_list) {
        open_function_list.emplace_back(std::get<0>(open_function));
    }
    return open_function_list;
}

std::vector<const ErrorNode *> Parser::get_all_errors() {
    std::vector<const ErrorNode *> errors;
    // Go through all core Modules and collect all errors they provide
    for (const auto &[module_name, module_namespace] : core_namespaces) {
        for (const auto &definition : module_namespace->public_symbols.definitions) {
            if (definition->get_variation() == DefinitionNode::Variation::ERROR) {
                const auto *error_node = definition->as<ErrorNode>();
                errors.emplace_back(error_node);
            }
        }
    }
    // Go through all instances of the parser and collect all errors from all instances
    for (const auto &instance : Parser::instances) {
        for (const auto &definition : instance.file_node_ptr->file_namespace->public_symbols.definitions) {
            if (definition->get_variation() == DefinitionNode::Variation::ERROR) {
                const auto *error_node = definition->as<ErrorNode>();
                errors.emplace_back(error_node);
            }
        }
    }
    return errors;
}

std::vector<std::shared_ptr<Type>> Parser::get_all_data_types() {
    std::vector<std::shared_ptr<Type>> data_types;
    // Go through all core Modules and collect all data nodes they provide
    for (const auto &[module_name, module_namespace] : core_namespaces) {
        for (const auto &definition : module_namespace->public_symbols.definitions) {
            if (definition->get_variation() == DefinitionNode::Variation::DATA) {
                const auto *data_node = definition->as<DataNode>();
                const auto data_type = module_namespace->get_type_from_str(data_node->name).value();
                data_types.emplace_back(data_type);
            }
        }
    }
    // Go through all instances of the parser and collect all data nodes from all instances
    for (const auto &instance : Parser::instances) {
        for (const auto &definition : instance.file_node_ptr->file_namespace->public_symbols.definitions) {
            if (definition->get_variation() == DefinitionNode::Variation::DATA) {
                const auto *data_node = definition->as<DataNode>();
                const auto data_type = instance.file_node_ptr->file_namespace->get_type_from_str(data_node->name).value();
                data_types.emplace_back(data_type);
            }
        }
    }
    return data_types;
}

std::vector<std::shared_ptr<Type>> Parser::get_all_freeable_types() {
    std::vector<std::shared_ptr<Type>> freeable_types;
    std::vector<std::string> collected_types;

    // Go through all core Modules and collect all freeable types they provide
    for (const auto &[module_name, module_namespace] : core_namespaces) {
        for (const auto &[type_string, type] : module_namespace->public_symbols.types) {
            if (!type->is_freeable()) {
                continue;
            }
            if (std::find(collected_types.begin(), collected_types.end(), type->to_string()) == collected_types.end()) {
                freeable_types.emplace_back(type);
                collected_types.emplace_back(type->to_string());
            }
        }
    }
    // Go through all instances of the parser and collect all freeable types from all instances
    for (const auto &instance : Parser::instances) {
        for (const auto &[type_string, type] : instance.file_node_ptr->file_namespace->public_symbols.types) {
            if (!type->is_freeable()) {
                continue;
            }
            if (std::find(collected_types.begin(), collected_types.end(), type->to_string()) == collected_types.end()) {
                freeable_types.emplace_back(type);
                collected_types.emplace_back(type->to_string());
            }
        }
    }
    // Go through all public types and collect all freable types
    for (const auto &[type_string, type] : Type::types) {
        if (!type->is_freeable()) {
            continue;
        }
        if (std::find(collected_types.begin(), collected_types.end(), type->to_string()) == collected_types.end()) {
            freeable_types.emplace_back(type);
            collected_types.emplace_back(type->to_string());
        }
    }
    return freeable_types;
}

bool Parser::parse_all_open_data_modules(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Data Modules");

    // Define a task to process a single data module
    auto process_function = [](Parser &parser, DataNode *data) -> bool {
        PROFILE_SCOPE("Process data module '" + data->name + "'");
        // Go through al default values of the data's fields and parse them
        std::shared_ptr<Scope> data_scope = std::make_shared<Scope>();
        const Context data_context = Context{
            .level = data->is_const ? ContextLevel::CONST_DATA : ContextLevel::INTERNAL,
        };
        for (auto &field : data->fields) {
            if (!field.initializer_tokens.has_value()) {
                continue;
            }
            parser.collapse_types_in_slice(field.initializer_tokens.value(), parser.file_node_ptr->tokens);
            field.initializer = parser.create_expression(data_context, data_scope, field.initializer_tokens.value());
            if (!field.initializer.has_value()) {
                return false;
            }
        }
        return true;
    };

    // Collect all open data modules from all parsers and store them in two lists, the const ones and the non-const ones. We parse the const
    // ones first
    std::vector<std::pair<Parser &, DataNode *>> const_data;
    std::vector<std::pair<Parser &, DataNode *>> nonconst_data;
    for (auto &parser : instances) {
        while (auto next = parser.get_next_open_data()) {
            if (next.value()->is_const) {
                const_data.emplace_back(parser, next.value());
            } else {
                nonconst_data.emplace_back(parser, next.value());
            }
        }
    }

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Parse all const data first, then parse the non-const data
        for (auto &[parser, data] : const_data) {
            futures.emplace_back(thread_pool.enqueue(process_function, std::ref(parser), data));
        }
        // Collect results from all const tasks
        for (auto &future : futures) {
            result = result && future.get(); // Combine results using logical AND
        }
        futures.clear();
        for (auto &[parser, data] : nonconst_data) {
            futures.emplace_back(thread_pool.enqueue(process_function, std::ref(parser), data));
        }
        // Collect results from all nonconst tasks
        for (auto &future : futures) {
            result = result && future.get(); // Combine results using logical AND
        }
    } else {
        // Process data sequentially, first all const data then all nonconst dat
        for (auto &[parser, data] : const_data) {
            result = result && process_function(parser, data);
        }
        for (auto &[parser, data] : nonconst_data) {
            result = result && process_function(parser, data);
        }
    }
    return result;
}

bool Parser::parse_all_open_func_modules(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Func Modules");

    // Define a task to process a single func module
    auto process_function = [](Parser &parser, FuncNode *func, std::vector<Line> &body) -> bool {
        PROFILE_SCOPE("Process func module '" + func->name + "'");
        // First, refine all the body lines
        parser.collapse_types_in_lines(body, parser.file_node_ptr->tokens);
        // Go through all required data of the func module and resolve them if they are unknown and throw an error if they are not data
        for (auto &required_data : func->required_data) {
            switch (required_data.first->get_variation()) {
                case Type::Variation::DATA:
                    // All ok
                    break;
                case Type::Variation::UNKNOWN: {
                    const UnknownType *unknown_type = required_data.first->as<UnknownType>();
                    auto type = parser.file_node_ptr->file_namespace->get_type_from_str(unknown_type->type_str);
                    if (!type.has_value()) {
                        // Unknown type in requires clausel of func module
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    if (type.value()->get_variation() == Type::Variation::DATA) {
                        required_data.first = type.value();
                        break;
                    }
                    [[fallthrough]];
                }
                default:
                    // The required type is not a data type
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
            }
        }

        std::vector<Line> body_mut = body;
        while (!body_mut.empty()) {
            const Line function_definition_line = body_mut.front();
            body_mut.erase(body_mut.begin());
            if (body_mut.empty()) {
                // Function has no body
                // TODO: When "virtual" (linked) functions are implemented this case could be allowed if the function is only a definition
                // and not a declaration
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            std::vector<Line> function_body_lines;
            for (auto it = body_mut.begin(); it != body_mut.end();) {
                if (it->indent_lvl > function_definition_line.indent_lvl) {
                    function_body_lines.emplace_back(*it);
                    body_mut.erase(it);
                    continue;
                }
                break;
            }
            if (function_body_lines.empty()) {
                THROW_BASIC_ERR(ERR_PARSING);
                return false;
            }
            std::pair<std::string, required_data_t> required_data_pair{func->name, func->required_data};
            std::optional<FunctionNode> fn = parser.create_function(function_definition_line.tokens, required_data_pair);
            if (!fn.has_value()) {
                return false;
            }
            std::optional<FunctionNode *> added_function = parser.file_node_ptr->add_function(fn.value(), core_namespaces);
            if (!added_function.has_value()) {
                return false;
            }
            parser.add_open_function({added_function.value(), function_body_lines});
            func->functions.emplace_back(added_function.value());
        }
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Iterate through all parsers and their open func modules
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_func()) {
                auto &[func, body] = next.value();
                // Enqueue a task for each function
                futures.emplace_back(thread_pool.enqueue(process_function, std::ref(parser), func, std::ref(body)));
            }
        }
        // Collect results from all tasks
        for (auto &future : futures) {
            result = result && future.get(); // Combine results using logical AND
        }
    } else {
        // Process functions sequentially
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_func()) {
                auto &[func, body] = next.value();
                result = result && process_function(parser, func, std::ref(body));
            }
        }
    }
    return result;
}

bool Parser::parse_all_open_entities(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Entities");

    // Define a task to process a single entity
    auto process_function = [](Parser &parser, EntityNode *entity, std::vector<Line> &body) -> bool {
        PROFILE_SCOPE("Process entity '" + entity->name + "'");
        // Go through all extended entities and resolve all unknown types and ensure that the extended entities are entities at all
        for (auto &parent_entity : entity->parent_entities) {
            switch (parent_entity.first->get_variation()) {
                case Type::Variation::ENTITY:
                    // All ok
                    break;
                case Type::Variation::UNKNOWN: {
                    const UnknownType *unknown_type = parent_entity.first->as<UnknownType>();
                    auto type = parser.file_node_ptr->file_namespace->get_type_from_str(unknown_type->type_str);
                    if (!type.has_value()) {
                        // Unknown type in extends clausel of entity
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    if (type.value()->get_variation() == Type::Variation::ENTITY) {
                        parent_entity.first = type.value();
                        break;
                    }
                    [[fallthrough]];
                }
                default:
                    // The required type is not a data type
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
            }
        }

        auto &data_modules = entity->data_modules;
        auto &func_modules = entity->func_modules;
        auto &parent_entities = entity->parent_entities;

        // Add all data modules and func modules of all parent entities to the data modules list in the order defined in the `extends`
        // definition
        for (const auto &parent_entity_type : parent_entities) {
            const EntityNode *parent_entity = parent_entity_type.first->as<EntityType>()->entity_node;
            for (DataNode *data_node : parent_entity->data_modules) {
                if (std::find(data_modules.begin(), data_modules.end(), data_node) == data_modules.end()) {
                    data_modules.emplace_back(data_node);
                }
            }
            for (FuncNode *func_node : parent_entity->func_modules) {
                if (std::find(func_modules.begin(), func_modules.end(), func_node) == func_modules.end()) {
                    func_modules.emplace_back(func_node);
                }
            }
        }

        // TODO: Make the order of definition for the data and func modules order-independant. This means that one could then also first
        // define all func modules before defining all data modules. For now, to keep it simple, we will have a fixed order of data first
        // and then func modules

        // Add all data modules defined in the `data` section of the entity
        auto line_it = body.begin();
        auto tok_it = line_it->tokens.first;
        assert(tok_it->token == TOK_DATA);
        tok_it++;
        bool semicolon_found = false;
        if (tok_it->token == TOK_COLON) {
            // This entity provides some data
            tok_it++;
            while (line_it != body.end()) {
                while (tok_it != line_it->tokens.second) {
                    switch (tok_it->token) {
                        default:
                            THROW_BASIC_ERR(ERR_PARSING);
                            return false;
                        case TOK_COMMA:
                            break;
                        case TOK_SEMICOLON:
                            semicolon_found = true;
                            break;
                        case TOK_IDENTIFIER: {
                            auto type = parser.file_node_ptr->file_namespace->get_type_from_str(std::string(tok_it->lexme));
                            if (!type.has_value()) {
                                THROW_BASIC_ERR(ERR_PARSING);
                                return false;
                            }
                            *tok_it = TokenContext(TOK_TYPE, tok_it->line, tok_it->column, tok_it->file_id, type.value());
                            [[fallthrough]];
                        }
                        case TOK_TYPE: {
                            if (tok_it->type->get_variation() != Type::Variation::DATA) {
                                THROW_BASIC_ERR(ERR_PARSING);
                                return false;
                            }
                            auto *data_node = tok_it->type->as<DataType>()->data_node;
                            if (std::find(data_modules.begin(), data_modules.end(), data_node) != data_modules.end()) {
                                THROW_ERR(                                                    //
                                    ErrDefEntityDuplicateData, ERR_PARSING, parser.file_hash, //
                                    tok_it->line, tok_it->column, tok_it->type->to_string()   //
                                );
                                return false;
                            }
                            data_modules.emplace_back(data_node);
                            break;
                        }
                    }
                    if (semicolon_found) {
                        break;
                    }
                    tok_it++;
                }
                line_it++;
                tok_it = line_it->tokens.first;
                if (semicolon_found) {
                    break;
                }
            }
        } else {
            // This entity does not provide any data on it's own. This is fine if it extends another entity, otherwise this would be an
            // error
            assert(tok_it->token == TOK_SEMICOLON);
            assert(!parent_entities.empty());
            assert(!data_modules.empty());
            line_it++;
            tok_it = line_it->tokens.first;
        }
        if (line_it == body.end()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }

        tok_it = line_it->tokens.first;
        assert(tok_it->token == TOK_FUNC);
        tok_it++;
        assert(tok_it->token == TOK_COLON);
        tok_it++;
        semicolon_found = false;
        while (line_it != body.end()) {
            while (tok_it != line_it->tokens.second) {
                switch (tok_it->token) {
                    default:
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    case TOK_COMMA:
                        break;
                    case TOK_SEMICOLON:
                        semicolon_found = true;
                        break;
                    case TOK_IDENTIFIER: {
                        auto type = parser.file_node_ptr->file_namespace->get_type_from_str(std::string(tok_it->lexme));
                        if (!type.has_value()) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return false;
                        }
                        *tok_it = TokenContext(TOK_TYPE, tok_it->line, tok_it->column, tok_it->file_id, type.value());
                        [[fallthrough]];
                    }
                    case TOK_TYPE: {
                        if (tok_it->type->get_variation() != Type::Variation::FUNC) {
                            THROW_BASIC_ERR(ERR_PARSING);
                            return false;
                        }
                        auto *func_node = tok_it->type->as<FuncType>()->func_node;
                        if (std::find(func_modules.begin(), func_modules.end(), func_node) != func_modules.end()) {
                            THROW_ERR(                                                    //
                                ErrDefEntityDuplicateFunc, ERR_PARSING, parser.file_hash, //
                                tok_it->line, tok_it->column, tok_it->type->to_string()   //
                            );
                            return false;
                        }
                        func_modules.emplace_back(func_node);
                        break;
                    }
                }
                if (semicolon_found) {
                    break;
                }
                tok_it++;
            }
            line_it++;
            tok_it = line_it->tokens.first;
            if (semicolon_found) {
                break;
            }
        }
        if (line_it == body.end()) {
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }

        // Make sure that all required data from all func modules is present in the entity
        for (const auto &func : func_modules) {
            for (const auto &required_data : func->required_data) {
                bool contains_required_data = false;
                const auto *required_data_node = required_data.first->as<DataType>()->data_node;
                for (const auto &provided_data_node : data_modules) {
                    if (provided_data_node == required_data_node) {
                        contains_required_data = true;
                        break;
                    }
                }
                if (!contains_required_data) {
                    THROW_ERR(                                                                             //
                        ErrDefEntityMissingData, ERR_PARSING, parser.file_hash,                            //
                        entity->line, entity->column, entity->length, required_data_node->name, func->name //
                    );
                    return false;
                }
            }
        }

        std::vector<std::unique_ptr<LinkNode>> link_nodes;
        if (tok_it->token == TOK_LINK) {
            // TODO: Parse all links once links are implemented
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return false;
        }

        assert(tok_it->token == TOK_IDENTIFIER);
        if (tok_it->lexme != entity->name) {
            // Incorrect constructor name
            THROW_BASIC_ERR(ERR_PARSING);
            return false;
        }
        tok_it++;
        assert(tok_it->token == TOK_LEFT_PAREN);
        tok_it++;
        std::vector<DataNode *> constructed_data;
        while (tok_it != line_it->tokens.second && tok_it->token != TOK_RIGHT_PAREN) {
            switch (tok_it->token) {
                default:
                    // Not allowed token in constructor
                    THROW_BASIC_ERR(ERR_PARSING);
                    return false;
                case TOK_IDENTIFIER: {
                    const std::string identifier(tok_it->lexme);
                    if (!parent_entities.empty()) {
                        // Search if the identifier is one of the parent entities' accessors
                        bool parent_added = false;
                        for (const auto &[parent_entity_type, accessor] : parent_entities) {
                            const EntityNode *parent_entity = parent_entity_type->as<EntityType>()->entity_node;
                            if (identifier == accessor) {
                                // For now we simply add the parent's data modules to the data constructor list and to the data modules of
                                // this entity. If any data module is defined duplicately afterwards (explicit, below) then an error will be
                                // thrown. If duplicates arise from extended entities then it is simply skipped to the next token iteration
                                // TODO: Update change the constructors of extended entities to be required to pass in an entity at the
                                // position of the extended entity accessor. For now the simpler approach is chosen.
                                for (DataNode *data_node : parent_entity->data_modules) {
                                    if (std::find(constructed_data.begin(), constructed_data.end(), data_node) != constructed_data.end()) {
                                        // Duplicate data constructor
                                        THROW_BASIC_ERR(ERR_PARSING);
                                        return false;
                                    }
                                    constructed_data.emplace_back(data_node);
                                    auto idx = std::find(data_modules.begin(), data_modules.end(), data_node);
                                    entity->constructor_order.emplace_back(std::distance(data_modules.begin(), idx));
                                }
                                parent_added = true;
                            }
                        }
                        if (parent_added) {
                            tok_it++;
                            break;
                        }
                    }
                    auto type = parser.file_node_ptr->file_namespace->get_type_from_str(identifier);
                    if (!type.has_value()) {
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    *tok_it = TokenContext(TOK_TYPE, tok_it->line, tok_it->column, tok_it->file_id, type.value());
                    [[fallthrough]];
                }
                case TOK_TYPE: {
                    if (tok_it->type->get_variation() != Type::Variation::DATA) {
                        // Only data types allowed in constructor
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    DataNode *data_node = tok_it->type->as<DataType>()->data_node;
                    auto idx = std::find(data_modules.begin(), data_modules.end(), data_node);
                    if (idx == data_modules.end()) {
                        // Unknown data module
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    // Check if the module has already been added to the constructed data
                    if (std::find(constructed_data.begin(), constructed_data.end(), data_node) != constructed_data.end()) {
                        // Duplicate data type
                        THROW_BASIC_ERR(ERR_PARSING);
                        return false;
                    }
                    constructed_data.emplace_back(data_node);
                    entity->constructor_order.emplace_back(std::distance(data_modules.begin(), idx));
                    [[fallthrough]];
                }
                case TOK_COMMA:
                    tok_it++;
                    break;
            }
        }

        return true;
    };

    // Parse all entities from tips to roots since dependant entities need internal information of the base entities
    struct DepNode {
        Parser *parser;
        EntityNode *entity;
        std::vector<Line> body;
        std::vector<std::string> parents;
        std::vector<std::string> children;
        bool processed = false;
    };
    std::unordered_map<std::string, DepNode> nodes;

    // Helper to create node placeholder if missing
    auto get_or_create_node = [&nodes](const std::string &key) -> DepNode & {
        if (nodes.find(key) == nodes.end()) {
            auto res = nodes.emplace(key, DepNode());
            return res.first->second;
        }
        return nodes.at(key);
    };

    // Collect all open entities from all parsers
    for (auto &parser : instances) {
        while (auto next = parser.get_next_open_entity()) {
            auto &[entity, body] = next.value();
            const std::string key = entity->file_hash.to_string() + "." + entity->name;
            DepNode &node = get_or_create_node(key);
            node.parser = &parser;
            node.entity = entity;
            node.body = std::move(body);
            for (auto &[parent_type, accessor_name] : entity->parent_entities) {
                if (parent_type->get_variation() == Type::Variation::UNKNOWN) {
                    const std::string &unknown_type_str = parent_type->as<UnknownType>()->type_str;
                    parent_type = parser.file_node_ptr->file_namespace->get_type_from_str(unknown_type_str).value();
                }
                const EntityNode *parent_entity = parent_type->as<EntityType>()->entity_node;
                const std::string parent_key = parent_entity->file_hash.to_string() + "." + parent_entity->name;
                node.parents.emplace_back(parent_key);
                DepNode &parent_node = get_or_create_node(parent_key);
                parent_node.children.emplace_back(key);
            }
        }
    }
    // Get a list of all entities which do not have any parents, these are the tips of our entity dependency trees
    // We also check in this phase that every single node's entity pointer has been set. If this is not the case something went wrong
    std::vector<std::string> tip_keys;
    for (const auto &[key, node] : nodes) {
        assert(node.entity != nullptr);
        if (node.parents.empty()) {
            tip_keys.emplace_back(key);
        }
    }

    // We now parse all entities at our tips and collect the next stage of tips which need to be parsed, all current tips can be processed
    // in parallel as well
    std::vector<std::string> next_tip_keys;
    bool result = true;
    if (parse_parallel) {
        while (!tip_keys.empty()) {
            // Enqueue tasks in the global thread pool
            std::vector<std::future<bool>> futures;
            // Iterate through all current tip keys and enqueue the processing of them
            for (const auto &tip_key : tip_keys) {
                auto &node = nodes.at(tip_key);
                // Enqueue a task for each node
                futures.emplace_back(thread_pool.enqueue(process_function, std::ref(*node.parser), node.entity, std::ref(node.body)));
                // We set the processed state in here for correctness of later code
                node.processed = true;
                // Collect all the keys for the next iteration. We only mark those entities whose parents have fully been parsed, if any
                // parent is still missing we do not add the key
                for (const auto &child_key : node.children) {
                    auto &child_node = nodes.at(child_key);
                    bool all_parents_processed = true;
                    for (const auto &parent_key : child_node.parents) {
                        auto &parent_node = nodes.at(parent_key);
                        all_parents_processed &= parent_node.processed;
                    }
                    if (!all_parents_processed) {
                        continue;
                    }
                    // If all parents have been processed then we add the child to the keys to parse next
                    if (std::find(next_tip_keys.begin(), next_tip_keys.end(), child_key) == next_tip_keys.end()) {
                        next_tip_keys.emplace_back(child_key);
                    }
                }
            }
            // Collect results from all entities
            for (auto &future : futures) {
                result = result && future.get(); // Combine results using logical AND
            }
            // Cancel if one of the entities failed since now other entities depend on them and potentially would fail too
            if (!result) {
                return result;
            }
            tip_keys = next_tip_keys;
            next_tip_keys.clear();
        }
    } else {
        // Process entities sequentially
        while (!tip_keys.empty()) {
            // Iterate through all current tip keys and enqueue the processing of them
            for (const auto &tip_key : tip_keys) {
                auto &node = nodes.at(tip_key);
                // Enqueue a task for each node
                result = result && process_function(*node.parser, node.entity, node.body);
                // We set the processed state in here for correctness of later code
                node.processed = true;
                // Collect all the keys for the next iteration
                for (const auto &child_key : node.children) {
                    auto &child_node = nodes.at(child_key);
                    bool all_parents_processed = true;
                    for (const auto &parent_key : child_node.parents) {
                        auto &parent_node = nodes.at(parent_key);
                        all_parents_processed &= parent_node.processed;
                    }
                    if (!all_parents_processed) {
                        continue;
                    }
                    // If all parents have been processed then we add the child to the keys to parse next
                    if (std::find(next_tip_keys.begin(), next_tip_keys.end(), child_key) == next_tip_keys.end()) {
                        next_tip_keys.emplace_back(child_key);
                    }
                }
            }
            // Cancel if one of the entities failed since now other entities depend on them and potentially would fail too
            if (!result) {
                return result;
            }
            tip_keys = next_tip_keys;
            next_tip_keys.clear();
        }
    }
    return result;
}

bool Parser::parse_all_open_functions(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Functions");

    // Define a task to process a single function
    auto process_function = [](Parser &parser, FunctionNode *function, std::vector<Line> &body) -> bool {
        PROFILE_SCOPE("Process function '" + function->name + "'");
        if (function->is_extern) {
            // Check whether the FIP provides the searched for function in any of it's modules. We only print the error that the function
            // was unable to be resolved if the FIP is active, if the FIP is not active then the problem is not that the problem has not
            // been found but that FIP has not been able to be started / initialized properly
            if (!FIP::resolve_function(function)) {
                if (FIP::is_active) {
                    THROW_ERR(ErrExternFnNotFound, ERR_PARSING, function);
                }
                return false;
            }
            return true;
        }
        // First, refine all the body lines
        parser.collapse_types_in_lines(body, parser.file_node_ptr->tokens);
        if (DEBUG_MODE) {
            // Debug print the definition line as well as the body lines vector as one continuous print, like when printing the token lists
            std::cout << "\n"
                      << YELLOW << "[Debug Info] Printing refined body tokens of function: " << DEFAULT << function->name << std::endl;
            Debug::print_token_context_vector({body.front().tokens.first, body.back().tokens.second}, "DEFINITION");
        }
        // Create the body and add the body statements to the created scope
        auto body_statements = parser.create_body(function->scope.value(), body);
        if (!body_statements.has_value()) {
            return false;
        }
        function->scope.value()->body = std::move(body_statements.value());
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Iterate through all parsers and their open functions
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_function()) {
                auto &[function, body] = next.value();
                // Enqueue a task for each function
                futures.emplace_back(thread_pool.enqueue(process_function, std::ref(parser), function, std::ref(body)));
            }
        }
        // Collect results from all tasks
        for (auto &future : futures) {
            result = result && future.get(); // Combine results using logical AND
        }
    } else {
        // Process functions sequentially
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_function()) {
                auto &[function, body] = next.value();
                result = result && process_function(parser, function, std::ref(body));
            }
        }
    }
    return result;
}

bool Parser::parse_all_open_tests(const bool parse_parallel) {
    PROFILE_SCOPE("Parse Open Tests");

    // Define a task to process a single test
    auto process_test = [](Parser &parser, TestNode *test, std::vector<Line> &body) -> bool {
        PROFILE_SCOPE("Process test '" + test->name + "'");
        // First, refine all the body lines
        parser.collapse_types_in_lines(body, parser.file_node_ptr->tokens);
        if (DEBUG_MODE) {
            // Debug print the definition line as well as the body lines vector as one continuous print, like when printing the token lists
            std::cout << "\n" << YELLOW << "[Debug Info] Printing refined body tokens of test: " << DEFAULT << test->name << std::endl;
            Debug::print_token_context_vector({body.front().tokens.first, body.back().tokens.second}, "DEFINITION");
        }
        // Create the body and add the body statements to the created scope
        auto body_statements = parser.create_body(test->scope, body);
        if (!body_statements.has_value()) {
            return false;
        }
        test->scope->body = std::move(body_statements.value());
        return true;
    };

    bool result = true;
    if (parse_parallel) {
        // Enqueue tasks in the global thread pool
        std::vector<std::future<bool>> futures;
        // Iterate through all parsers and their open functions
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_test()) {
                auto &[test, body] = next.value();
                // Enqueue a task for each function
                futures.emplace_back(thread_pool.enqueue(process_test, std::ref(parser), test, std::ref(body)));
            }
        }
        // Collect results from all tasks
        for (auto &future : futures) {
            result = result && future.get(); // Combine results using logical AND
        }
    } else {
        // Process functions sequentially
        for (auto &parser : Parser::instances) {
            while (auto next = parser.get_next_open_test()) {
                auto &[test, body] = next.value();
                result = result && process_test(parser, test, std::ref(body));
            }
        }
    }
    return result;
}

std::optional<std::tuple<std::string, overloads, std::optional<std::string>>> Parser::get_builtin_function( //
    const std::string &function_name,                                                                       //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules                         //
) {
    for (const auto &[module_name, import_node] : imported_core_modules) {
        const auto &module_functions = core_module_functions.at(module_name);
        if (module_functions.find(function_name) != module_functions.end()) {
            const auto &function_variants = module_functions.at(function_name);
            return std::make_tuple(module_name, function_variants, import_node->alias);
        }
    }
    return std::nullopt;
}

token_list Parser::extract_from_to(unsigned int from, unsigned int to, token_list &tokens) {
    PROFILE_CUMULATIVE("Parser::extract_from_to");
    token_list extraction = clone_from_to(from, to, tokens);
    tokens.erase(tokens.begin() + from, tokens.begin() + to);
    return extraction;
}

token_list Parser::clone_from_to(unsigned int from, unsigned int to, const token_list &tokens) {
    PROFILE_CUMULATIVE("Parser::clone_from_to");
    assert(to >= from);
    assert(to <= tokens.size());
    token_list extraction;
    if (to == from) {
        return extraction;
    }
    extraction.reserve(to - from);
    std::copy(tokens.begin() + from, tokens.begin() + to, std::back_inserter(extraction));
    return extraction;
}

token_list Parser::clone_from_slice(const token_slice &slice) {
    PROFILE_CUMULATIVE("Parser::clone_from_slice");
    assert(slice.second - slice.first > 0);
    assert(slice.first != slice.second);
    token_list extraction;
    extraction.reserve(std::distance(slice.first, slice.second));
    std::copy(slice.first, slice.second, std::back_inserter(extraction));
    return extraction;
}

std::optional<const Parser *> Parser::get_instance_from_hash(const Hash &hash) {
    PROFILE_CUMULATIVE("Parser::get_instance_from_hash");
    for (const auto &instance : instances) {
        if (instance.file_node_ptr->file_namespace->namespace_hash == hash) {
            return &instance;
        }
    }
    return std::nullopt;
}
