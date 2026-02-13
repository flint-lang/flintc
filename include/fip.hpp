#pragma once

#ifndef DEBUG_BUILD
#define FIP_QUIET
#endif
#define FIP_MASTER
#ifdef __cplusplus
extern "C" {
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "fip.h"
#undef ERROR
#undef OPTIONAL
#pragma GCC diagnostic pop

#ifdef __cplusplus
}
#endif

#include "parser/type/type.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

class FunctionNode;
class ImportNode;

/// @class `FIP`
/// @brief Class to namespace and handle all FIP-related functionality
class FIP {
  public:
    FIP() = delete;

    /// @typedef `fake_fn`
    /// @breif A small structure containing all necessary information about a function to later tell if a given function node is the same as
    /// this function
    /// It also contains information about in which interop module it was found
    typedef struct {
        std::string module_name;
        std::string name;
        std::vector<std::shared_ptr<Type>> ret_types;
        std::vector<std::shared_ptr<Type>> arg_types;
    } fake_function;

    /// @var `is_active`
    /// @brief Whether the FIP is active
    static inline bool is_active = false;

    /// @var `modules`
    /// @brief All the available modules of the FIP which contains the file descriptors of all active and spawned modules
    static inline fip_interop_modules_t modules;

    /// @var `buffer`
    /// @brief The message buffer of FIP
    alignas(alignof(size_t)) static inline char buffer[FIP_MSG_SIZE];

    /// @var `message`
    /// @brief The message which will be re-used for all FIP communications
    static inline fip_msg_t message;

    /// @var `resolved_functions`
    /// @brief A list containing all functions which have been matched by FIP
    static inline std::vector<fake_function> resolved_functions;

    /// @function `get_fip_path`
    /// @brief Returns the path to the '.fip' directory in which all cofigurations and cache of fip is contained
    ///
    /// @return `std::optional<std::filesystem::path>` The path to the .fip directory, nullopt if there was no .fip dir found
    static std::optional<std::filesystem::path> get_fip_path();

    /// @function `init`
    /// @brief Initializes the FIP and does whatever needs to be done at the FIP setup stage
    ///
    /// @param `file_hash` The file in which FIP is being initialized (FIP is initialized on the first extern function found)
    /// @param `line` The line the first extern function is located at
    /// @param `column` The column of the first extern function
    /// @param `length` The length of the first extern function
    /// @return `bool` Whether initialization failed
    static bool init(              //
        const Hash &file_hash,     //
        const unsigned int line,   //
        const unsigned int column, //
        const unsigned int length  //
    );

    /// @function `shutdown`
    /// @brief Shuts down the FIP and sends the kill messages to all interop modules
    static void shutdown();

    /// @function `convert_type`
    /// @brief Converts a given type to a FIP type which other FIP modules can understand
    ///
    /// @param `dest` The destination FIP type
    /// @param `src` The source Flint type
    /// @param `is_mutable` Whether the given type is mutable
    /// @return `bool` Whether conversion was successful
    static bool convert_type(fip_type_t *dest, const std::shared_ptr<Type> &src, const bool is_mutable);

    /// @function `resolve_function`
    /// @brief Resolves a given function definition, changes it's internal name for code generation and returns whether the function was
    /// even found in one of the interop modules at all
    ///
    /// @param `function` The function definition to resolve
    /// @return `bool` Whether the function symbol could be resolved
    static bool resolve_function(FunctionNode *function);

    /// @function `resolve_module_import`
    /// @brief Resolves a given module import of structure `use Fip.module` and checks if the given `module` is present in any of the
    /// modules present in FIP. It then checks whether that module's `.ft` file in the `.fip/bindings` dir has already been generated. If it
    /// has not been generated then the file is being generated and the `import`s path is changed from `Fip.module` to the relative path to
    /// the `.fip/bindings/module.ft` file. This means that the `Fip.module` import also can be aliased like any other "normal" import.
    ///
    /// @param `import` The Fip import to resolve
    /// @return `bool` Whether the Fip import could be resolved
    static bool resolve_module_import(ImportNode *import);

    /// @function `generate_bindings_file`
    /// @brief Generates a bindings file from a list of signatures
    ///
    /// @param `list` The list of signatures from which to generate the bindings file from
    /// @param `module_tag` The module tag from which to generate the bidings file
    /// @return `bool` Whether the bindings file was generated successfully
    ///
    /// @note This function frees the contents of the list as it goes, so the list will be "empty" after the call
    static bool generate_bindings_file(fip_sig_list_t *list, const std::string &module_tag);

    /// @function `generate_fip_type`
    /// @brief Generates the given fip type and appends it to the given file in-line without adding any new lines in it
    ///
    /// @param `type` The type to generate to the file
    /// @param `file` The file to append the generated type to
    ///
    /// @note This function frees the passed-in type by calling `fip_free_type`
    static void generate_fip_type(fip_type_t *type, std::ofstream &file);

    /// @function `send_compile_request`
    /// @brief Sends the compile request to all interop modules, meaning that they now can start compiling their respective source files
    static void send_compile_request();

    /// @function `gather_objects`
    /// @brief Gathers all built objects of all interop modules which need to be linked to the final executable
    ///
    /// @return `std::optional<std::vector<std::array<char, 9>>>` The 8-byte object hashes (+ null terminator) of all objects which need to
    /// be linked to the final executable. `std::nullopt` if one of the external compilation's failed
    static std::optional<std::vector<std::array<char, 9>>> gather_objects();
};
