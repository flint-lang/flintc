#pragma once

#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/hash.hpp"
#include "parser/type/type.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/// @class `FunctionNode`
/// @brief Represents function definitions
class FunctionNode : public DefinitionNode {
  public:
    enum class Visibility {
        /// @brief The function is defined regulary as a plain old regular function internal to Flint
        INTERN,
        /// @brief The function was defined using the 'extern' keyword and thus comes somewhere from a FIP IM
        EXTERN,
        /// @brief The function was defined using the 'export' keyword and will have a second wrapper function generated as the exported
        /// signature to make it callable from languages outside of Flint
        EXPORT,
        /// @brief The function comes from a Core module and thus does not obey the rules of the TS
        CORE,
    };

    struct Parameter {
        /// @var `type`
        /// @brief The type of the function parameter
        std::shared_ptr<Type> type;

        /// @var `name`
        /// @brief The name of the function parameter
        std::string name;

        /// @var `is_mutable`
        /// @brief Whether the function parameter is mutable
        bool is_mutable;
    };

    explicit FunctionNode(                                //
        const Hash &file_hash,                            //
        const unsigned int line,                          //
        const unsigned int column,                        //
        const unsigned int length,                        //
        const std::vector<AnnotationNode> &annotations,   //
        const bool is_const,                              //
        const Visibility visibility,                      //
        const std::string &name,                          //
        std::vector<Parameter> &parameters,               //
        std::vector<std::shared_ptr<Type>> &return_types, //
        std::vector<std::shared_ptr<Type>> &error_types,  //
        std::optional<std::shared_ptr<Scope>> &scope,     //
        const std::optional<size_t> &mangle_id            //
        ) :
        DefinitionNode(file_hash, line, column, length, annotations),
        is_const(is_const),
        visibility(visibility),
        name(name),
        parameters(std::move(parameters)),
        return_types(std::move(return_types)),
        error_types(std::move(error_types)),
        scope(std::move(scope)),
        mangle_id(mangle_id) {}

    /// @var `consumable_extern_annotations`
    /// @brief The annotations consumable by this definition node if the function is extern
    static const inline std::unordered_set<AnnotationKind> consumable_extern_annotations = {
        AnnotationKind::FIP_DISABLE,
    };

    std::unordered_set<AnnotationKind> get_possible_annotations() const override {
        if (visibility == Visibility::EXTERN) {
            return consumable_extern_annotations;
        } else {
            return {};
        }
    }

    Variation get_variation() const override {
        return Variation::FUNCTION;
    }

    size_t get_id() const {
        const std::string hash_str = file_hash.to_string() + "." + get_signature_string(0, true);
        static std::unordered_map<std::string, size_t> ids;
        if (ids.find(hash_str) != ids.end()) {
            return ids.at(hash_str);
        }

        // FNV-1a hash algorithm constants
        constexpr uint64_t FNV_PRIME = 1099511628211ull;
        constexpr uint64_t FNV_OFFSET_BASIS = 5472609002491880229ull; // 14695981039346656037 truncated to 63 bits

        // 63-bit hash container
        struct {
            uint64_t hash : 63;
            uint64_t unused : 1;
        } container = {
            .hash = FNV_OFFSET_BASIS,
            .unused = 1,
        };

        for (char c : hash_str) {
            container.hash ^= static_cast<unsigned char>(c);
            container.hash *= FNV_PRIME;
        }

        // Shift left and handle zero case
        ASSERT(ids.find(hash_str) == ids.end());
        ids[hash_str] = *reinterpret_cast<uint64_t *>(&container);
        return ids.at(hash_str);
    }

    std::string get_signature_string(                 //
        const size_t implicit_parameters_to_skip = 0, //
        const bool name_dot_as_ref = false,           //
        const bool include_modifiers = true,          //
        const bool include_param_names = true,        //
        const bool include_return_types = true,       //
        const bool include_error_types = true         //
    ) const {
        std::ostringstream oss;
        const auto dot_idx = std::find(name.begin(), name.end(), '.');
        if (dot_idx == name.end()) {
            oss << name;
        } else {
            const auto &dot_dist = std::distance(name.begin(), dot_idx);
            if (name_dot_as_ref) {
                oss << name.substr(0, dot_dist) << "::";
            }
            oss << name.substr(dot_dist + 1);
        }
        oss << "(";
        for (size_t j = implicit_parameters_to_skip; j < parameters.size(); j++) {
            if (j > implicit_parameters_to_skip) {
                oss << ", ";
            }
            const auto &[param_type, param_name, is_mut] = parameters.at(j);
            if (include_modifiers) {
                if (is_mut) {
                    oss << "mut ";
                } else {
                    oss << "const ";
                }
            }
            oss << param_type->to_string();
            if (include_param_names) {
                oss << " " << param_name;
            }
        }
        oss << ")";
        if (include_return_types) {
            oss << " -> ";
            switch (return_types.size()) {
                case 0:
                    oss << "void";
                    break;
                case 1:
                    oss << return_types.front()->to_string();
                    break;
                default:
                    oss << "(";
                    for (size_t j = 0; j < return_types.size(); j++) {
                        if (j > 0) {
                            oss << ", ";
                        }
                        oss << return_types.at(j)->to_string();
                    }
                    oss << ")";
                    break;
            }
        }
        if (include_error_types) {
            oss << " ";
            switch (error_types.size()) {
                case 0:
                    break;
                default:
                    oss << "{";
                    for (size_t j = 0; j < error_types.size(); j++) {
                        if (j > 0) {
                            oss << ", ";
                        }
                        oss << error_types.at(j)->to_string();
                    }
                    oss << "}";
                    break;
            }
        }
        return oss.str();
    }

    // empty constructor
    FunctionNode() = delete;
    // deconstructor
    ~FunctionNode() override = default;
    // copy operations - disabled due to unique_ptr member
    FunctionNode(const FunctionNode &) = delete;
    FunctionNode &operator=(const FunctionNode &) = delete;
    // move operations
    FunctionNode(FunctionNode &&) = default;
    FunctionNode &operator=(FunctionNode &&) = default;

    /// @var `is_const`
    /// @brief Determines whether the function is const, e.g. it cannot access data outise of its arguments
    bool is_const;

    /// @var `visibility`
    /// @brief The visibility of the function
    Visibility visibility;

    /// @var `name`
    /// @brief The name of the function
    std::string name;

    /// @var `parameters`
    /// @brief List of all the function parameters
    std::vector<Parameter> parameters;

    /// @var `return_types`
    /// @brief The types of all return values
    std::vector<std::shared_ptr<Type>> return_types;

    /// @var `error_types`
    /// @brief The types of errors this function can throw
    std::vector<std::shared_ptr<Type>> error_types;

    /// @var `scope`
    /// @brief The scope of the function containing all statements or nullopt if the function is just a declaration
    std::optional<std::shared_ptr<Scope>> scope;

    /// @var `mangle_id`
    /// @brief The mangle id of this function, or other said the Nth function defined in the file
    std::optional<size_t> mangle_id;

    /// @var `persistent_count`
    /// @brief How many persistent locals are defined inside this function's body
    size_t persistent_count{0};
};
