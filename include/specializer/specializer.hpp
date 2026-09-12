#pragma once

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/definitions/variant_node.hpp"

/// @class `Specializer`
/// @brief The class which is responsible for generic application of compile-time parameters (specialization)
/// @note This class cannot be initialized and all functions within this class are static
class Specializer {
  public:
    Specializer() = delete;

    /// @function `specialize`
    /// @brief Function to specialize the given definition if it is a generic definition at all. The applied comptime parameter list must
    /// have the same same size as the CPL of the definition. If specialization fails this function returns a nullopt. If it succeeds it
    /// means that the specialized definition has been added to the file node etc
    ///
    /// @param `ns` The namespace in which we try to specialize the type
    /// @param `type` The type to specialize with the given CVL
    /// @param `cvl` All comptime values applied to the definition to specialize it
    /// @return `std::optional<std::shared_ptr<Type>>` The specialized type, nullopt if specialization failed
    [[nodiscard]] static std::optional<std::shared_ptr<Type>> specialize( //
        Namespace *const ns,                                              //
        std::shared_ptr<Type> type,                                       //
        std::vector<std::shared_ptr<Type>> cvl                            //
    );

    /// @function `clear`
    /// @brief Clears all internal state of the specializer, needed for the LSP to work more than just once
    static void clear() {
        specializations.clear();
    }

  private:
    using specialization_map = std::unordered_map<std::string, DefinitionNode *>;

    /// @var `specializations`
    /// @brief A list of all specializations for every definition node
    static inline std::unordered_map<std::string, specialization_map> specializations{};

    /// @function `get_definition_string`
    /// @brief Returns the definition string in the form of `<name>[CP0, CP1, ...]` if `include_cpl` is true, `<HASH>.<name>` if not
    ///
    /// @param `definition` The definition to get the unified string of
    /// @param `cvl` All comptime parameters applied to the definition to specialize it
    /// @param `include_cpl` Whether to include the CPL in the definition string
    /// @return `std::string` The unified definition string used as the key for the specializations
    [[nodiscard]] static std::string get_definition_string( //
        const DefinitionNode *const definition,             //
        const std::vector<std::shared_ptr<Type>> &cvl,      //
        const bool include_cpl                              //
    );

    /// function `is_specializable`
    /// @brief Check whether the given type is specializable. For example within a definition a nested generic depending on the CPL of the
    /// definition is not specializable direclty when encountering it. It can only be specialized if the CVL is provided. This function
    /// checks whether the passed-in type can be **used** when specializing a generic type.
    ///
    /// @param `type` The type to check if it is specializable
    /// @return `bool` Whether the type is able to be specialized
    [[nodiscard]] static bool is_specializable(const std::shared_ptr<Type> &type);

    /// @function `pre_create`
    /// @brief Pre-creates a new node in the file hash of the definition with the given name
    ///
    /// @param `definition` The definition to pre-create
    /// @param `name` The new name the pre-created definition will have
    /// @return `std::optional<DefinitionNode *>` A pointer to the pre-created definition. It is entirely "empty" and will be populated by
    /// the deeper specialize functions, nullopt of pre-creation failed
    [[nodiscard]] static std::optional<DefinitionNode *> pre_create(const DefinitionNode *const definition, const std::string &name);

    /// @function `specialize_data`
    /// @brief Specializes the given data definition into the given node and returns whether specialization was successful
    ///
    /// @param `node` The destination data definition to put the specialized data definition into
    /// @param `definition` The source data definition to specialize
    /// @param `cvl` All comptime parameters applied to the definition to specialize it
    /// @return `bool` Whether specialization was successful
    [[nodiscard]] static bool specialize_data(        //
        DataNode *const node,                         //
        const DataNode *const definition,             //
        const std::vector<std::shared_ptr<Type>> &cvl //
    );

    /// @function `specialize_variant`
    /// @brief Specializes the given variant definition into the given node and returns whether specialization was successful
    ///
    /// @param `node` The destination variant node to put the specialized variant definition into
    /// @param `definition` The source variant definition to specialize
    /// @param `cvl` All comptime parameters applied to the definition to specialize it
    /// @return `bool` Whether specialization was successful
    [[nodiscard]] static bool specialize_variant(     //
        VariantNode *const node,                      //
        const VariantNode *const definition,          //
        const std::vector<std::shared_ptr<Type>> &cvl //
    );

    /// @function `specialize_type`
    /// @brief Specializes the given type with the given CPL and applied CVL
    ///
    /// @param `ns` The namespace in which to specialized the type
    /// @param `type` The type to specialize
    /// @param `cpl` The comptime parameter list of the definition in which to specailize the type
    /// @param `cvl` The comptime value list which was applied to the definition
    /// @return `std::optional<std::pair<bool, std::shared_ptr<Type>>>` Whether the type was specialized + the specialized type, nullopt if
    /// specialization failed
    [[nodiscard]] static std::optional<std::pair<bool, std::shared_ptr<Type>>> specialize_type( //
        Namespace *const ns,                                                                    //
        const std::shared_ptr<Type> &type,                                                      //
        const std::vector<DefinitionNode::ComptimeParameter> &cpl,                              //
        const std::vector<std::shared_ptr<Type>> &cvl                                           //
    );

    /// @function `specialize_type_list`
    /// @brief Specializes the given type list with the given CPL and applied CVL
    ///
    /// @param `ns` The namespace in which to specialized the type
    /// @param `types` The type list to specialize, this specializes the type-list directly in-place
    /// @param `cpl` The comptime parameter list of the definition in which to specailize the type
    /// @param `cvl` The comptime value list which was applied to the definition
    /// @return `std::optional<bool>` Whether the type list was specialized (any type in the list changed), nullopt if specialization failed
    [[nodiscard]] static std::optional<bool> specialize_type_list( //
        Namespace *const ns,                                       //
        std::vector<std::shared_ptr<Type>> &types,                 //
        const std::vector<DefinitionNode::ComptimeParameter> &cpl, //
        const std::vector<std::shared_ptr<Type>> &cvl              //
    );
};
