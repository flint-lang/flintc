#pragma once

#include "parser/ast/annotation_node.hpp"
#include "parser/ast/ast_node.hpp"

/// @class `DefinitionNode`
/// @brief Base class for all top-level definitions
class DefinitionNode : public ASTNode {
  public:
    /// @struct `ComptimeParameter`
    /// @brief Small collection of all values needed for a comptime parameter
    struct ComptimeParameter {
        /// @var `type`
        /// @brief The type of the comptime parameter, for now it is only allowed to be a `TypeType`
        std::shared_ptr<Type> type;

        /// @var `name`
        /// @brief The name of the comptime parameter
        std::string name;

        /// @var `applied_value`
        /// @brief The applied value makes specializing functions a lot easier, is not necessarily needed to represent a comptime parameter
        /// itself
        std::optional<std::shared_ptr<Type>> applied_value = std::nullopt;
    };

    /// @struct `Specialization`
    /// @brief Small collection of things only present in the definition if it was specialized from a template
    struct Specialization {
        /// @var `origin`
        /// @brief The original template definition this definition was specialized from
        DefinitionNode *origin;

        /// @var `applied_cvl`
        /// @brief The comptime values which were applied to specialize this definition
        std::vector<std::shared_ptr<Type>> applied_cvl;
    };

  protected:
    DefinitionNode(                                     //
        const Hash &file_hash,                          //
        const unsigned int line,                        //
        const unsigned int column,                      //
        const unsigned int length,                      //
        const std::vector<AnnotationNode> &annotations, //
        const std::vector<ComptimeParameter> &cpl       //
        ) :
        ASTNode(file_hash, line, column, length),
        annotations(annotations),
        cpl(cpl) {}

  public:
    // destructor
    ~DefinitionNode() override = default;
    // copy operations - disabled by default
    DefinitionNode(const DefinitionNode &) = delete;
    DefinitionNode &operator=(const DefinitionNode &) = delete;
    // move operations
    DefinitionNode(DefinitionNode &&) = default;
    DefinitionNode &operator=(DefinitionNode &&) = default;

    /// @var `annotations`
    /// @brief The annotations defined for this definition
    std::vector<AnnotationNode> annotations;

    /// @var `cpl`
    /// @brief The comptime parameter list of this definition node, for example `[type T, int N]`
    std::vector<ComptimeParameter> cpl;

    /// @var `specialization`
    /// @brief If this definition is a specialization then this value contains all information about that, nullopt if not specialized
    std::optional<Specialization> specialization;

    /// @function `is_generic_template`
    /// @brief Whether this definition is a generic template which has not been specialized yet. A template does not have a single applied
    /// value set in its CPL, so it is never parsed/analyzed/code-generated on its own
    ///
    /// @return `bool` Whether this definition is an un-specialized generic template
    bool is_generic_template() const {
        if (cpl.empty()) {
            return false;
        }
        for (const auto &param : cpl) {
            if (!param.applied_value.has_value()) {
                return true;
            }
        }
        return false;
    }

    /// @var `tokens`
    /// @brief The tokens of this definition's body which the definition owns. Each definition copies the token span of its body out of
    /// the file's token list into this list, so type-collapsing can mutate them freely without invalidating any other definition slices
    token_list tokens;

    /// @function `contains_annotation`
    /// @brief Checks whether this definition contains the given annotation kind
    ///
    /// @param `kind` The annotation kind to search for
    /// @return `bool` Whether this definition contains the given annotation
    bool contains_annotation(const AnnotationKind kind) const {
        for (const auto &annotation : annotations) {
            if (annotation.kind == kind) {
                return true;
            }
        }
        return false;
    }

    /// @function `get_possible_annotations`
    /// @brief Function to get all annotations this definition supports, as it is virtual extended definitions may overwrite it, if not
    /// overwritten no annotations are returned
    ///
    /// @return `std::unordered_set<AnnotationKind>` All annotations this definition supports
    virtual std::unordered_set<AnnotationKind> get_possible_annotations() const {
        return {};
    }

    /// @enum `Variation`
    /// @brief A enum describing which definition variations exist
    enum class Variation {
        DATA,
        ENUM,
        ERROR,
        FUNC,
        FUNCTION,
        IMPORT,
        INTERFACE,
        OBJECT,
        TEST,
        VARIANT,
    };

    /// @function `get_variation`
    /// @brief Function to return which variation this definition node is
    ///
    /// @return `Variation` The variation of this definition node
    virtual Variation get_variation() const = 0;

    /// @function `as`
    /// @brief Casts this definition node to the requested type, but the requested type must be a child type of this class
    template <typename T>
    std::enable_if_t<std::is_base_of_v<DefinitionNode, T> && !std::is_same_v<DefinitionNode, T>, const T *> inline as() const {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(const_cast<DefinitionNode *>(this));
        ASSERT(result, "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<const T *>(this);
#endif
    }

    /// @function `as`
    /// @brief Casts this definition node to the requested type, but the requested type must be a child type of this class
    template <typename T> std::enable_if_t<std::is_base_of_v<DefinitionNode, T> && !std::is_same_v<DefinitionNode, T>, T *> inline as() {
#ifdef DEBUG_BUILD
        T *result = dynamic_cast<T *>(this);
        ASSERT(result, "as<T>() type mismatch - check your switch case!");
        return result;
#else
        return static_cast<T *>(this);
#endif
    }
};
