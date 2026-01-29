#pragma once

#include "globals.hpp"
#include "lexer/builtins.hpp"
#include "parser/ast/call_node_base.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/data_access_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/expressions/grouped_data_access_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/optional_chain_node.hpp"
#include "parser/ast/expressions/optional_unwrap_node.hpp"
#include "parser/ast/expressions/range_expression_node.hpp"
#include "parser/ast/expressions/string_interpolation_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/unary_op_expression.hpp"
#include "parser/ast/expressions/variable_node.hpp"
#include "parser/ast/expressions/variant_extraction_node.hpp"
#include "parser/ast/expressions/variant_unwrap_node.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/instance_call_node_base.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/array_assignment_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/data_field_assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/enhanced_for_loop_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/group_assignment_node.hpp"
#include "parser/ast/statements/group_declaration_node.hpp"
#include "parser/ast/statements/grouped_data_field_assignment_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/stacked_array_assignment.hpp"
#include "parser/ast/statements/stacked_assignment.hpp"
#include "parser/ast/statements/stacked_grouped_assignment.hpp"
#include "parser/ast/statements/switch_statement.hpp"
#include "parser/ast/statements/throw_node.hpp"
#include "parser/ast/statements/unary_op_statement.hpp"
#include "parser/ast/statements/while_node.hpp"
#include "resolver/resolver.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/// @class `Generator`
/// @brief The class which is responsible for the IR code generation
/// @note This class cannot be initialized and all functions within this class are static
class Generator {
  public:
    Generator() = delete;

    /// @function `get_flintc_cache_path`
    /// @brief Returns the path to the `flntc` cache directory in `$HOME/.cache/` or in `C:\\Program Files\\Flint\\Cache\\`
    ///
    /// @return `std::filesystem::path` The path to the flint cache directory
    static std::filesystem::path get_flintc_cache_path();

    /// @function `compile_module`
    /// @brief Compiles a given module and saves the .o / .obj file at the given path
    ///
    /// @param `module` The module to compile
    /// @param `module_path` The path
    /// @return `bool` Whether compilation was successful
    static bool compile_module(llvm::Module *module, const std::filesystem::path &module_path);

    /// @function `verify_module`
    /// @brief Verifies a given module
    ///
    /// @param `module` The module to verify
    /// @return `bool` Whether the module is error-free
    static bool verify_module(const llvm::Module *module);

    /// @function `generate_program_ir`
    /// @brief Generates the llvm IR code for a complete program
    ///
    /// @param `program_name` The name the program (the module) will have
    /// @param `dep_graph` The root DepNode of the dependency graph
    /// @param `is_test` Whether the program is built in test mode
    /// @return `std::optional<std::unique_ptr<llvm::Module>>` A pointer containing the generated program module, nullopt if anything failed
    ///
    /// @attention Do not forget to call `Resolver::clear()` before the module returned from this function goes out of scope! You would get
    /// a segmentation fault if you were to forget that!
    static std::optional<std::unique_ptr<llvm::Module>> generate_program_ir( //
        const std::string &program_name,                                     //
        const std::shared_ptr<DepNode> &dep_graph,                           //
        const bool is_test                                                   //
    );

    /// @function `generate_file_ir`
    /// @brief Generates the llvm IR code for a single file and saves it into a llvm module
    ///
    /// @param `dep_node` The dependency graph node of the file (used for circular dependencies)
    /// @param `file` The file node to generate
    /// @param `is_test` Whether the program is built in test mode
    /// @return `std::unique_ptr<llvm::Module>` A pointer containing the generated file module, nullopt if code generation failed
    static std::optional<std::unique_ptr<llvm::Module>> generate_file_ir( //
        const std::shared_ptr<DepNode> &dep_node,                         //
        FileNode &file,                                                   //
        const bool is_test                                                //
    );

    /// @function `get_module_ir_string`
    /// @brief Generates the IR code of the given Module and returns it as a string
    ///
    /// @param `module` The module whose IR code will be generated
    /// @return `std::string` The string containing the IR code of the module
    static std::string get_module_ir_string(const llvm::Module *module);

    /// @function `resolve_ir_comments`
    /// @brief Resolves all IR metadata within the given IR string and places comments where the metadata is located at
    ///
    /// @param `ir_string` The IR code where metadata will be resolved, and replaced by IR comments
    /// @return `std::string` The modified IR code, where all metadata is resolved into IR comments
    static std::string resolve_ir_comments(const std::string &ir_string);

  private:
    /// @var `context`
    /// @brief The global llvm context used for code generation
    static inline llvm::LLVMContext context;

    /// @alias `group_mapping`
    /// @brief This type represents all values of the group. Everything is considered a group, if one single value is returned, its a group
    /// of size 1. This makes direct-group mappings much easier
    using group_mapping = std::optional<std::vector<llvm::Value *>>;

    /// @var `c_functions`
    /// @brief Map containing references to all external c functions
    ///
    /// @attention The external c functions are nullpointers until explicitely generated by the `Builtin::generate_c_functions` function
    static inline std::unordered_map<CFunction, llvm::Function *> c_functions = {
        {CFunction::PRINTF, nullptr},
        {CFunction::MALLOC, nullptr},
        {CFunction::FREE, nullptr},
        {CFunction::MEMCPY, nullptr},
        {CFunction::REALLOC, nullptr},
        {CFunction::SNPRINTF, nullptr},
        {CFunction::MEMCMP, nullptr},
        {CFunction::EXIT, nullptr},
        {CFunction::ABORT, nullptr},
        {CFunction::FGETC, nullptr},
        {CFunction::MEMMOVE, nullptr},
        {CFunction::STRTOL, nullptr},
        {CFunction::STRTOUL, nullptr},
        {CFunction::STRTOF, nullptr},
        {CFunction::STRTOD, nullptr},
        {CFunction::STRLEN, nullptr},
        {CFunction::FOPEN, nullptr},
        {CFunction::FSEEK, nullptr},
        {CFunction::FCLOSE, nullptr},
        {CFunction::FTELL, nullptr},
        {CFunction::FREAD, nullptr},
        {CFunction::REWIND, nullptr},
        {CFunction::FGETS, nullptr},
        {CFunction::FWRITE, nullptr},
        {CFunction::GETENV, nullptr},
#ifndef __WIN32__
        {CFunction::SETENV, nullptr},
#endif
        {CFunction::POPEN, nullptr},
        {CFunction::PCLOSE, nullptr},
        {CFunction::SIN, nullptr},
        {CFunction::SINF, nullptr},
        {CFunction::COS, nullptr},
        {CFunction::COSF, nullptr},
        {CFunction::SQRT, nullptr},
        {CFunction::SQRTF, nullptr},
        {CFunction::POW, nullptr},
        {CFunction::POWF, nullptr},
        {CFunction::ABS, nullptr},
        {CFunction::LABS, nullptr},
        {CFunction::FABSF, nullptr},
        {CFunction::FABS, nullptr},
    };

    /// @struct `GenerationContext`
    /// @brief The context of the Generation
    struct GenerationContext {
        /// @var `parent`
        /// @brief The function the generation happens in
        llvm::Function *parent;

        /// @var `scope`
        /// @brief The scope the generation happens in
        std::shared_ptr<Scope> scope;

        /// @var `allocations`
        /// @brief The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        std::unordered_map<std::string, llvm::Value *const> allocations;

        /// @var `imported_core_modules`
        /// @brief The list of imported core modules
        const std::unordered_map<std::string, ImportNode *const> imported_core_modules;

        /// @var `short_circuit_block`
        /// @brief The block to branch to within an optional chain if any of the chain operations fails
        std::optional<llvm::BasicBlock *> short_circuit_block = std::nullopt;
    };

    /// @var `type_map`
    /// @brief Map containing all possible struct return types of function calls
    ///
    /// This map exists in order to prevent re-creation of the same StructType for every function declaration and definition. It is used
    /// throughout the generation process to reference already created StructType types
    ///
    /// @details
    /// - **Key** `std::string` - The return type of the function, encoded as a string (for example 'int' or 'str' for single returns)
    /// - **Value** `llvm::StructType *` - The reference to the already existent StructType definition
    static inline std::unordered_map<std::string, llvm::StructType *> type_map;

    /// @var `unresolved_functions`
    /// @brief Stores unresolved function calls within a module.
    ///
    /// This data structure is used during the module generation process to track
    /// all unresolved function calls. These calls will be resolved at the end
    /// of the per-file module generation phase.
    ///
    /// @details
    /// - **Key:** `std::string` - Name of the called function.
    /// - **Value:** `std::vector<llvm::CallInst *>` - List of all calls to this function.
    ///
    /// @note This map is static and persists throughout the module's lifecycle.
    /// @attention Ensure that all entries in this map are processed and resolved
    /// before completing the module generation.
    static inline std::unordered_map<std::string, std::vector<llvm::CallInst *>> unresolved_functions;

    /// @var `file_unresolved_functions`
    /// @brief Stores all unresolved function calls towards a different file
    ///
    /// This data structure is used during the module generation process to track all unresolved
    /// inter-module function calls. These calls will be resolved at the end of the generation
    /// process, when all module's functions have been generated and have been added to the same
    /// main module.
    ///
    /// @details
    /// - **Key** `std::string` - Name of the file the call targets
    /// - **Value** `std::unordered_map<std::string, std::vector<llvm::CallInst *>>` -
    ///         The map of all unresolved function calls towards this file
    /// - - **Key** `std::string` - Name of the called function (including the namespace hash before it like `Asdfb9s5.fn_name`)
    /// - - **Value** `std::vector<llvm::CallInst *>` - The list of all calls targeting this function
    ///
    /// @note This map is static and persists throughout the module's lifecyle
    static inline std::unordered_map<std::string, std::unordered_map<std::string, std::vector<llvm::CallInst *>>> file_unresolved_functions;

    /// @var `function_mangle_ids`
    /// @brief Stores all mangle IDs for all functions in the module currently being generated
    ///
    /// This data structure is used during the module generation process to track the mangle IDs of all functions within the module
    /// currently being generated. Because every function inside the generated module can appear in any order, all function definitions have
    /// to be forward-declared within the generated module, thus _every_ function inside the module does have a mangle ID
    ///
    /// @details
    /// - **Key** `std::string` - Name of the function (including the namespace hash before it like `Asdfb9s5.fn_name`)
    /// - **Value** `unsigned int` - Manlge ID of the function
    ///
    /// @note This map is being cleared at the end of every file module generation pass
    // static inline std::unordered_map<std::string, unsigned int> function_mangle_ids;

    /// @var `file_function_mangle_ids`
    /// @brief Stores all mangle ids for all functions from all files
    ///
    /// This data structure is used during the module generation process to track the mangle IDs of all functions of all generated
    /// files. It is used at the end of the main module generation to resolve all unresolved function calls between files.
    ///
    /// @details
    /// - **Key** `std::string` - Name of the file the functions are defined in
    /// - **Value** `std::unordered_map<std::string, unsigned int>` - The function mangle id map containing all ids of all functions
    ///
    /// @attention This map is never cleared so it is considered unsafe generating multiple programs within one lifetime of the program
    // static inline std::unordered_map<std::string, std::unordered_map<std::string, unsigned int>> file_function_mangle_ids;

    /// @var `function_names`
    /// @brief A vector of all function names within the current file
    ///
    /// This data structure is used during the module generation process to track all names of functions inside the file currently being
    /// generated. This list is essential in checking if a function call is external or not, as currently only the functions names are
    /// important, not their types.
    ///
    /// @note This list of function names is cleared ad the end of every file module generation pass
    /// @todo Implement a more sophisticated system to detect extern calls which combines the functions name as well as its type when
    /// checking if the call is file-internal or external
    static inline std::vector<std::string> function_names;

    /// @var `file_function_names`
    /// @brief Stores all the lists of function names from every file
    ///
    /// This data structure is used during the module generation process to keep track of all function names across all files. It is
    /// important for checking to which file a call goes to when it is an extern call.
    ///
    /// @details
    /// - **Key** `std::string` - The name of the file the functions are located in
    /// - **Value** `std::vector<std::string>` - The list of function names within this file
    ///
    /// @attention This map is never cleared so it is considered unsafe generating multiple programs within one lifetime of the program
    /// @todo Implement a more sophisticated system to detect which extern function the call references by embedding the whole signature of
    /// the function the call references, not only its name
    static inline std::unordered_map<std::string, std::vector<std::string>> file_function_names;

    /// @var `main_call_array`
    /// @brief Holds a reference to the call of the user-defined main function from within the builtin main function
    ///
    /// This array of size 1 exists because a static variable is needed to reference the call instruction. If the pointer to the call
    /// instruction itself would have been made static, very weird things start to happen as the pointer seemingly "changes" its reference
    /// its pointing to. This is the reason the pointer, the value of this static construct, is being wrapped in an array of size 1. It
    /// introduces minimal overhead and only acts as a static "container" in this case, to store the pointer to the call instruction calling
    /// the user-defined main function from within the builtin main function.
    static inline std::array<llvm::CallInst *, 1> main_call_array;

    /// @var `main_module`
    /// @brief Holds a static reference to the main module
    ///
    /// This array of size 1 exists because of the same reasons as why 'main_call_array' exists. It holds the reference to the main
    /// module, which means that every function can reference the main module if needed without the main module needed to explicitely be
    /// passed around through many functions which dont use it annyways. It is actually much more efficient to have this reference inside an
    /// static array than to pass it around unnecessarily.
    static inline std::array<llvm::Module *, 1> main_module;

    /// @var `tests`
    /// @brief A list of all generated test functions across all files:
    ///  - Key: `Hash` The hash of the file the tests are created in
    ///  - Value: Pair
    ///     - 0: The test node itself
    ///     - 1: The exact generated name of the test across modules, to find the test function in the module
    static inline std::unordered_map<Hash, std::vector<std::pair<const TestNode *, std::string>>> tests;

    /// @var `last_looparound_blocks`
    /// @brief The last basic blocks to loop back to, its a list to not need to update them for nested loops
    static inline std::vector<llvm::BasicBlock *> last_looparound_blocks;

    /// @var `last_loop_merge_blocks`
    /// @brief The last basic block to merge to, its a list in order to not needing to update the merge block after every nested loop
    /// statement
    static inline std::vector<llvm::BasicBlock *> last_loop_merge_blocks;

    /// @var `enum_name_arrays_map`
    /// @brief A map containing all references to all enum name arrays which map each enum value to it's string name, the key is the type
    /// name of the enum and the hash from where the enum came from, so `Sb7HsALK.<enum_name>`
    static inline std::unordered_map<std::string, llvm::GlobalVariable *> enum_name_arrays_map;

    /// @var `global_strings`
    /// @brief A map containing all references to all global string variables, each string only exists once
    ///
    /// @note The global string variables are stored by name, this is to keep them as valid as possible
    static inline std::unordered_map<std::string, std::string> global_strings;

    /// @var `generating_builtin_module`
    /// @brief Whether the generator currently is generating a builtin module
    static inline bool generating_builtin_module = false;

    /// @var `extern_functions`
    /// @breif A map of all extern functions where each function exists exactly once in the map. It maps each function name to the
    /// FunctionNode of the external definition, it is used to forward-declare all extern functions at the top of each module to enable
    /// calling extern functions defined in a different included file (file A has a `extern ...` and file B uses file A, this enables that
    /// file B can call the extern-defined functions defined in file A)
    static inline std::unordered_map<std::string, FunctionNode *> extern_functions;

    /// @class `IR`
    /// @brief The class which is responsible for the utility functions for the IR generation
    /// @note This class cannot be initialized and all functions within this class are static
    class IR {
      public:
        // The constructor is deleted to make this class non-initializable
        IR() = delete;

        /// @function `init_builtin_types`
        /// @brief Initializes the builtin types like `type.flint.err`
        static void init_builtin_types();

        /// @function `create_struct_type`
        /// @brief Creates a struct type with the given type name and the given field types
        ///
        /// @param `type_name` The name of the struct type to create
        /// @param `field_types` The field types of the newly created struct
        /// @param `is_packed` Whether the struct is packed
        /// @return `llvm::StructType *` The created struct type
        static llvm::StructType *create_struct_type(      //
            const std::string &type_name,                 //
            const std::vector<llvm::Type *> &field_types, //
            const bool is_packed = false                  //
        );

        /// @function `add_and_or_get_type`
        /// @brief Checks if a given return type of a given types list already exists. If it exists, it returns a reference to it, if it
        /// does not exist it creates it and then returns a reference to the created StructType
        ///
        /// @param `module` The module in which to get the type from
        /// @param `type` The type to get or set the struct type from
        /// @param `is_return_type` Whether the StructType is a return type (if it is, it has one return value more, the error return value)
        /// @param `is_extern` Whether the type to get is extern-targetted
        /// @return `llvm::StructType *` The reference to the StructType, representing the return type of the types map
        static llvm::StructType *add_and_or_get_type( //
            llvm::Module *module,                     //
            const std::shared_ptr<Type> &type,        //
            const bool is_return_type = true,         //
            const bool is_extern = false              //
        );

        /// @function `generate_bitwidth_change`
        /// @brief Generates a cast between two bitwidths
        ///
        /// @param `builder` The IRBuilder to use
        /// @param `value` The value to cast
        /// @param `from_bitwidth` The bitwidth of the value
        /// @param `to_bitwidth` The bitwidth to cast to
        /// @param `to_type` The type to cast to
        /// @return `llvm::Value *` The casted value
        static llvm::Value *generate_bitwidth_change( //
            llvm::IRBuilder<> &builder,               //
            llvm::Value *value,                       //
            const unsigned int from_bitwidth,         //
            const unsigned int to_bitwidth,           //
            llvm::Type *to_type                       //
        );

        /// @function `generate_weights`
        /// @brief Generates the weights to be used for branch prediction
        ///
        /// @param `true_weight` The weight of the true branch
        /// @param `false_weight` The weight of the false branch
        /// @return `llvm::MDNode *` The created branch weights
        static llvm::MDNode *generate_weights(unsigned int true_weight, unsigned int false_weight);

        /// @function `generate_forward_declarations`
        /// @brief Generates the forward-declarations of all constructs in the given FileNode, except the 'use' constructs to make another
        /// module able to use them. This function is also essential for Flint's support of circular dependency resolution
        ///
        /// @param `module` The module the forward declarations are declared inside
        /// @param `file_node` The FileNode whose construct definitions will be forward-declared in the given module
        static void generate_forward_declarations(llvm::Module *module, const FileNode &file_node);

        /// @function `get_extern_type`
        /// @brief Returns the llvm Type from a given Type for the use with FIP
        ///
        /// @param `module` The module from which to get the type from
        /// @param `type` The type from which to get the llvm type from
        /// @return `std::optional<llvm::Type *>` The correct extern type representation of the given type. If nullopt is returned then no
        /// special handling needs to be done for this type and the normal type resolving of the `get_type` function can continue
        static std::optional<llvm::Type *> get_extern_type( //
            llvm::Module *module,                           //
            const std::shared_ptr<Type> &type               //
        );

        /// @function `get_type`
        /// @brief Returns the llvm Type from a given Type
        ///
        /// @param `module` The module from which to get the type from
        /// @param `type` The type from which to get the llvm type from
        /// @param `is_extern` Whether the type is for an external function
        /// @return `std::pair<llvm::Type *, std::pair<bool, bool>>` A pair containing:
        ///     - a pointer to the correct llvm Type from the given string
        ///     - a pair of boolean values determining whether the given type is:
        ///         - A complex heap-allocated type (data, entity, etc)
        ///         - Passed by reference (data, entity, tuple, optional, variant, etc)
        static std::pair<llvm::Type *, std::pair<bool, bool>> get_type( //
            llvm::Module *module,                                       //
            const std::shared_ptr<Type> &type,                          //
            const bool is_extern = false                                //
        );

        /// @function `get_default_value_of_type`
        /// @brief Returns the default value associated with the given type
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The module in which to get the type from
        /// @param `type` The type from which the default value has to be returned
        /// @return `llvm::Value *` The default value of the given type
        static llvm::Value *get_default_value_of_type(llvm::IRBuilder<> &builder, llvm::Module *module, const std::shared_ptr<Type> &type);

        /// @function `get_default_value_of_type`
        /// @brief Returns the default value associated with a given Type
        ///
        /// @param `type` The Type from which the default value has to be returned
        /// @return `llvm::Value *` The default value of the given Type
        static llvm::Value *get_default_value_of_type(llvm::Type *type);

        /// @function `generate_const_string`
        /// @brief Generates a compile-time constant string that will be embedded into the binary itself, this string is not mutable
        ///
        /// @param `module` The module in which to generate the constant string in
        /// @param `str` The value of the string
        /// @return `llvm::Value *` The generated static string value
        static llvm::Value *generate_const_string(llvm::Module *module, const std::string &str);

        /// @function `generate_enum_value_strings`
        /// @brief Generates all the global strings of the enum within the given module, necessary for type-casting of enum values to
        /// strings
        ///
        /// @param `module` The module in which to generate the enum strings in
        /// @param `hash` The hash of the file the enum is defined in
        /// @param `enum_name` The name of the enum (e.g. it's type)
        /// @param `enum_values` The values of the enum to generate the global strings from
        /// @return `bool` Whether generating the global strings was successful
        static bool generate_enum_value_strings(        //
            llvm::Module *module,                       //
            const std::string &hash,                    //
            const std::string &enum_name,               //
            const std::vector<std::string> &enum_values //
        );

        /// @function `generate_err_value`
        /// @brief Generates an error value from the given error components
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which to generate the error value in
        /// @param `err_id` The type ID of the error
        /// @param `err_value` The value ID of the error
        /// @param `err_message` The message from the error
        /// @return `llvm::Value *` The constructed error value, ready to be stored somewhere
        static llvm::Value *generate_err_value( //
            llvm::IRBuilder<> &builder,         //
            llvm::Module *module,               //
            const unsigned int err_id,          //
            const unsigned int err_value,       //
            const std::string &err_message      //
        );

        /// @function `generate_debug_print`
        /// @brief Generates a small call to print which prints the given message using printf
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The module in which to generate the debug print
        /// @param `format` The format string for the debug print call
        /// @param `values` The actual llvm values to print in the debug print
        static void generate_debug_print(            //
            llvm::IRBuilder<> *builder,              //
            llvm::Module *module,                    //
            const std::string &format,               //
            const std::vector<llvm::Value *> &values //
        );

        /// @function `aligned_load`
        /// @brief Generates the code for a correctly type-aligned load instruction
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `type` The type of the value to load
        /// @param `ptr` The pointer to load the value from
        /// @param `name` The name of the loaded value
        /// @return `llvm::LoadInst *` The loaded value
        static llvm::LoadInst *aligned_load(llvm::IRBuilder<> &builder, llvm::Type *type, llvm::Value *ptr, const std::string &name = "");

        /// @function `aligned_store`
        /// @brief Genrates the code for a correctly type-aligned store instruction
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `value` The value to store at the pointer
        /// @param `ptr` The pointer to store the value at
        /// @return `llvm::StoreInst *` The store instruction
        static llvm::StoreInst *aligned_store(llvm::IRBuilder<> &builder, llvm::Value *value, llvm::Value *ptr);
    }; // subclass IR

    /// @class `Builtin`
    /// @brief The class which is responsible for generating all builtin functions
    /// @note This class cannot be initialized and all functions within this class are static
    class Builtin {
      public:
        // The constructor is deleted to make this class non-initializable
        Builtin() = delete;

        /// @function `generate_builtin_main`
        /// @brief Generates the builtin main function which calls the user defined main function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the main function definition will be generated in
        static void generate_builtin_main(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_c_functions`
        /// @brief Generates all references to the external c functions
        ///
        /// @param `module` The LLVM Module the c functions will be defined in
        static void generate_c_functions(llvm::Module *module);

        /// @function `refresh_c_functions`
        /// @brief Refreshes all references to the external c functions which have already been generated in this module before
        ///
        /// @param `module` The LLVM Module the c functions are already defined in
        /// @return `bool` Whether all functions were able to be refreshed
        static bool refresh_c_functions(llvm::Module *module);

        /// @function `generate_builtin_test`
        /// @brief Generates the entry point of the program when compiled with the `--test` flag enabled
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the test entry point will be generated in
        static void generate_builtin_test(llvm::IRBuilder<> *builder, llvm::Module *module);
    }; // subclass Builtin

    /// @class `Logical`
    /// @brief The class which is responsible for everything logical-related
    /// @note This class cannot be initialized and all functions within this class are static
    class Logical {
      public:
        // The constructor is deleted to make this class non-initializable
        Logical() = delete;

        /// @function `generate_not`
        /// @brief Inverts the given value. If the given value is a boolean, it creates a 'not' expression, otherwise it inverts all bits
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `value_to_negate` The value which is inverted
        /// @return `llvm::Value *` The inverted value
        static llvm::Value *generate_not(llvm::IRBuilder<> &builder, llvm::Value *value_to_negate);

        /// @function `generate_string_cmp_lt`
        /// @brief Generates the less than comparison between two strings
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The LLVM value of the left hand side to generate the string comparison from
        /// @param `lhs_expr` The lhs expression to check if its a literal
        /// @param `rhs` The LLVM value of the right hand side to generate the stirng comparison from
        /// @param `rhs_expr` The rhs expression to check if its a literal
        /// @return `llvm::Value *` The result of the string comparison
        static llvm::Value *generate_string_cmp_lt( //
            llvm::IRBuilder<> &builder,             //
            llvm::Value *lhs,                       //
            const ExpressionNode *lhs_expr,         //
            llvm::Value *rhs,                       //
            const ExpressionNode *rhs_expr          //
        );

        /// @function `generate_string_cmp_gt`
        /// @brief Generates the greater than comparison between two strings
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The LLVM value of the left hand side to generate the string comparison from
        /// @param `lhs_expr` The lhs expression to check if its a literal
        /// @param `rhs` The LLVM value of the right hand side to generate the stirng comparison from
        /// @param `rhs_expr` The rhs expression to check if its a literal
        /// @return `llvm::Value *` The result of the string comparison
        static llvm::Value *generate_string_cmp_gt( //
            llvm::IRBuilder<> &builder,             //
            llvm::Value *lhs,                       //
            const ExpressionNode *lhs_expr,         //
            llvm::Value *rhs,                       //
            const ExpressionNode *rhs_expr          //
        );

        /// @function `generate_string_cmp_le`
        /// @brief Generates the less equal comparison between two strings
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The LLVM value of the left hand side to generate the string comparison from
        /// @param `lhs_expr` The lhs expression to check if its a literal
        /// @param `rhs` The LLVM value of the right hand side to generate the stirng comparison from
        /// @param `rhs_expr` The rhs expression to check if its a literal
        /// @return `llvm::Value *` The result of the string comparison
        static llvm::Value *generate_string_cmp_le( //
            llvm::IRBuilder<> &builder,             //
            llvm::Value *lhs,                       //
            const ExpressionNode *lhs_expr,         //
            llvm::Value *rhs,                       //
            const ExpressionNode *rhs_expr          //
        );

        /// @function `generate_string_cmp_ge`
        /// @brief Generates the greater equal comparison between two strings
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The LLVM value of the left hand side to generate the string comparison from
        /// @param `lhs_expr` The lhs expression to check if its a literal
        /// @param `rhs` The LLVM value of the right hand side to generate the stirng comparison from
        /// @param `rhs_expr` The rhs expression to check if its a literal
        /// @return `llvm::Value *` The result of the string comparison
        static llvm::Value *generate_string_cmp_ge( //
            llvm::IRBuilder<> &builder,             //
            llvm::Value *lhs,                       //
            const ExpressionNode *lhs_expr,         //
            llvm::Value *rhs,                       //
            const ExpressionNode *rhs_expr          //
        );

        /// @function `generate_string_cmp_eq`
        /// @brief Generates the equal comparison between two strings
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The LLVM value of the left hand side to generate the string comparison from
        /// @param `lhs_expr` The lhs expression to check if its a literal
        /// @param `rhs` The LLVM value of the right hand side to generate the stirng comparison from
        /// @param `rhs_expr` The rhs expression to check if its a literal
        /// @return `llvm::Value *` The result of the string comparison
        static llvm::Value *generate_string_cmp_eq( //
            llvm::IRBuilder<> &builder,             //
            llvm::Value *lhs,                       //
            const ExpressionNode *lhs_expr,         //
            llvm::Value *rhs,                       //
            const ExpressionNode *rhs_expr          //
        );

        /// @function `generate_string_cmp_neq`
        /// @brief Generates the not equal comparison between two strings
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The LLVM value of the left hand side to generate the string comparison from
        /// @param `lhs_expr` The lhs expression to check if its a literal
        /// @param `rhs` The LLVM value of the right hand side to generate the stirng comparison from
        /// @param `rhs_expr` The rhs expression to check if its a literal
        /// @return `llvm::Value *` The result of the string comparison
        static llvm::Value *generate_string_cmp_neq( //
            llvm::IRBuilder<> &builder,              //
            llvm::Value *lhs,                        //
            const ExpressionNode *lhs_expr,          //
            llvm::Value *rhs,                        //
            const ExpressionNode *rhs_expr           //
        );
    };

    /// @class `Allocation`
    /// @brief The class which is responsible for everything allocation-related, like varaible preallocation
    /// @note This class cannot be initialized and all functions within this class are static
    class Allocation {
      public:
        // The constructor is deleted to make this class non-initializable
        Allocation() = delete;

        /// @function `generate_allocations`
        /// @brief Generates all allocations of the given scope recursively. Adds all AllocaInst pointer to the allocations map
        ///
        /// This function is meant to be called at the start of the generate_function function. This function goes through all
        /// statements and expressions recursively down the scope and enters every sub-scope too and generates all allocations of all
        /// function variables at the start of the function. This is done to make StackOverflows nearly impossible. Before this
        /// pre-allocation system for all variables was implemented, StackOverflows were common (when calling functions inside loops),
        /// caused by the creation of a return struct for every function call.
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The Function the allocations are generated in
        /// @param `scope` The Scope from which all allocations are collected and allocated at the start of the scope
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @return `bool` Whether all allocations were successful
        ///
        /// @attention The allocations map will be modified (new entries are added), but it will not be cleared. If you want a clear
        /// allocations map before calling this function, you need to clear it yourself.
        [[nodiscard]] static bool generate_allocations(                                     //
            llvm::IRBuilder<> &builder,                                                     //
            llvm::Function *parent,                                                         //
            const std::shared_ptr<Scope> scope,                                             //
            std::unordered_map<std::string, llvm::Value *const> &allocations,               //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
        );

        /// @function `generate_function_allocations`
        /// @brief Maps the argument values to fake allocations in the allocations map to make them accessible in the functions body
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The Function from which the arguments are mapped to allocations (if arguments are of non-primitive type)
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name etc is encoded
        /// @param `function` The function node from which to map the argument allocations
        static void generate_function_allocations(                            //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const FunctionNode *function                                      //
        );

        /// @funnction `generate_call_allcoations`
        /// @brief Generates the allocations for calls (The return struct, the error value and all return values)
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `scope` The scope the allocation would take place in
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `call_node` The CallNode used to generate the allocations from
        /// @return `bool` Whether the call allocations were all successful
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_call_allocations(                                 //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            const std::shared_ptr<Scope> scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const CallNodeBase *call_node                                                    //
        );

        /// @funnction `generate_if_allcoations`
        /// @brief Generates the allocations for if chains
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `if_node` The IfNode used to generate the allocations from
        /// @return `bool` Whether the if allocations were all successful
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_if_allocations(                                   //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const IfNode *if_node                                                            //
        );

        /// @function `generate_enh_for_allocations`
        /// @brief Generates the allocations for enhanced for loops
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `allocations` The map of allocations, where tin the key all information like scope ID, call ID, name, etc is encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `for_node` The enhanced for loop node to generate allocations for
        /// @return `bool` Whether the allocations were all successfull
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_enh_for_allocations(                              //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const EnhForLoopNode *for_node                                                   //
        );

        /// @function `generate_switch_statement_allocations`
        /// @brief Generates the allocations for switch statements
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `scope` The scope in which the switch statement is defined in
        /// @param `allocations` The map of allocations, where tin the key all information like scope ID, call ID, name, etc is encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `switch_statement` The switch statement from which to generate all allocations
        /// @return `bool` Whether the allocations were all successfull
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_switch_statement_allocations(                     //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            const std::shared_ptr<Scope> scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const SwitchStatement *switch_statement                                          //
        );

        /// @function `generate_switch_expression_allocations`
        /// @brief Generates the allocations for switch expressions
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `scope` The scope in which the switch statement is defined in
        /// @param `allocations` The map of allocations, where tin the key all information like scope ID, call ID, name, etc is encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `switch_expression` The switch expression from which to generate all allocations
        /// @return `bool` Whether the allocations were all successfull
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_switch_expression_allocations(                    //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            const std::shared_ptr<Scope> scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const SwitchExpression *switch_expression                                        //
        );

        /// @funnction `generate_declaration_allcoations`
        /// @brief Generates the allocations for a normal declaration
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `scope` The scope the allocation would take place in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `declaration_node` The DeclarationNode used to generate the allocations from
        /// @return `bool` Whether all declaration allocations succeeded
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_declaration_allocations(                          //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            const std::shared_ptr<Scope> scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const DeclarationNode *declaration_node                                          //
        );

        /// @funnction `generate_group_declaration_allcoations`
        /// @brief Generates the allocations for grouped declarations
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `scope` The scope the allocation would take place in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `group_declaration_node` The GroupDeclarationNode used to generate the allocations from
        /// @return `bool` Whether all group declaration allocations were successful
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_group_declaration_allocations(                    //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            const std::shared_ptr<Scope> scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const GroupDeclarationNode *group_declaration_node                               //
        );

        /// @function `generate_array_indexing_allocation`
        /// @brief Generates the allocation for a simple i64 array of size `dimensionality` which is re-used for all array indexing of that
        /// dimensionality within this function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `indexing_expressions` The indexing expressions of the array
        static void generate_array_indexing_allocation(                              //
            llvm::IRBuilder<> &builder,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations,        //
            const std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
        );

        /// @function `generate_expression_allocations`
        /// @brief Goes throught all expressions and searches for all calls to allocate them
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `scope` The scope the allocation would take place in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc in encoded
        /// @param `imported_core_modules` The list of imported core modules
        /// @param `expression` The expression to search for calls for
        /// @param `bool` Whether all expression allocations were successful
        ///
        /// @attention The allocations map will be modified
        [[nodiscard]] static bool generate_expression_allocations(                           //
            llvm::IRBuilder<> &builder,                                                      //
            llvm::Function *parent,                                                          //
            const std::shared_ptr<Scope> scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations,                //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules, //
            const ExpressionNode *expression                                                 //
        );

        /// @function `generate_allocation`
        /// @brief Generates a custom allocation call. This is a helper function to make allocations easier
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `alloca_name` The name of the allocation (its name in the allocations map)
        /// @param `type` The type of the allocation
        /// @param `ir_name` The name of the allocation, only important for the IR Code output
        /// @param `ir_comment` The comment the allocation gets, only important for the IR Code output
        static void generate_allocation(                                      //
            llvm::IRBuilder<> &builder,                                       //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const std::string &alloca_name,                                   //
            llvm::Type *type,                                                 //
            const std::string &ir_name,                                       //
            const std::string &ir_comment                                     //
        );

        /// @function `calculate_type_alignment`
        /// @brief Calculates the alignment for the given type
        ///
        /// @param `type` The type to calculate the alignment for
        /// @return `unsigned int` The calculated alignment
        static unsigned int calculate_type_alignment(llvm::Type *type);

        /// @function `get_type_size`
        /// @brief Calculates the size of the given type
        ///
        /// @param `type` The type to get the size of
        /// @return `size_t` The size of the type in bytes
        static size_t get_type_size(llvm::Module *module, llvm::Type *type);

        /// @function `generate_default_struct`
        /// @brief Allocates a struct and adds default values to every element of the struct
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `type` The type of the struct
        /// @param `name` The name of the allocation
        /// @param `ignore_first` If to skip setting any value of the first element of the struct (the error value)
        /// @return `llvm::AllocaInst *` A pointer to the allocated struct allocation, where (all) the elements contain default values
        ///
        /// @attention If 'ignore_first' is set, the first struct element (the error value) wont be set to its default value. This is
        ///            important for the cases where you want a default struct but set the error value yourself. For example, when throwing
        ///            an error you would want to generate a default struct return and then set the error value. By setting `ignore_first`,
        ///            you save on a few instructions, as the first value (error value) wont be set to anything by default.
        static llvm::AllocaInst *generate_default_struct( //
            llvm::IRBuilder<> &builder,                   //
            llvm::StructType *type,                       //
            const std::string &name,                      //
            bool ignore_first = false                     //
        );
    }; // subclass Allocation

    /// @class `Function`
    /// @brief The class which is responsible for generating everything related to functions
    /// @note This class cannot be initialized and all functions within this class are static
    class Function {
      public:
        // The constructor is deleted to make this class non-initializable
        Function() = delete;

        /// @function `generate_function_type`
        /// @brief Generates the type information of a given FunctionNode
        ///
        /// @param `module` The module in which the function type is generated in
        /// @param `function_node` The FunctionNode used to generate the type
        /// @return `llvm::FunctionType *` A pointer to the generated FuntionType
        ///
        /// @note This function handles a lot of work to get external function definitions right. It splits struct argument types or refines
        /// return types etc. This function, like the `Expression::generate_extern_call` function are the boundary between Flint and the
        /// outside world.
        static llvm::FunctionType *generate_function_type(llvm::Module *module, FunctionNode *function_node);

        /// @function `generate_function`
        /// @brief Generates a function from a given FunctionNode
        ///
        /// @param `module` The LLVM Module the function will be generated in
        /// @param `function_node` The FunctionNode used to generate the function
        /// @param `imported_core_modules` The list of imported core modules
        /// @return `bool` Whether the code generation of the function was successful
        [[nodiscard]] static bool generate_function(                                        //
            llvm::Module *module,                                                           //
            FunctionNode *function_node,                                                    //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
        );

        /// @function `generate_test_function`
        /// @brief Generates the test function
        ///
        /// @param `module` The LLVM Module in which the test will be generated in
        /// @param `test_node` The TestNode to generate
        /// @param `imported_core_modules` The list of imported core modules
        /// @return `std::optional<llvm::Function *>` The generated test function, nullopt if generation failed
        [[nodiscard]] static std::optional<llvm::Function *> generate_test_function(        //
            llvm::Module *module,                                                           //
            const TestNode *test_node,                                                      //
            const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
        );

        /// @function `get_function_definition`
        /// @brief Returns the function definition from the given CallNode or other values based on a few conditions
        ///
        /// If the definition could not be found, this function returns a `std::nullopt`. The second variable determines whether the call
        /// targets a function inside the current module (false) or if it targets a function from another module (true).
        ///
        /// @param `parent` The Function the call happens in
        /// @param `call_node` The CallNode from which the actual function definition is tried to be found
        /// @return `std::pair<nullptr, false>` If the call targets a builtin function
        /// @return `std::pair<std::nullopt, false>` If the function definition could not be found
        /// @return `std::pair<std::optional<Function *>, bool>` In all other cases. The second variable always determines whether the call
        /// targets a function inside the current module (false) or if it targets a function in another module (true).
        static std::pair<std::optional<llvm::Function *>, bool> get_function_definition( //
            llvm::Function *parent,                                                      //
            const CallNodeBase *call_node                                                //
        );

        /// @function `function_has_return`
        /// @brief Checks if a given function has a return statement within its bodies instructions
        ///
        /// @param `function` The function to check
        /// @param `bool` True if the function has a return statement, false if not
        ///
        /// @todo Currently, this function does return true if a single block has a return statement, but does not check if the last, or all
        ///       blocks, have return statements. This needs to be changed to check if all blocks have a return statement, and possibly
        ///       return a list of all blocks not containing a termination instruction, like `br` or `ret`
        static bool function_has_return(llvm::Function *function);
    }; // subclass Function

    /// @class `Statement`
    /// @brief The class which is responsible for generating everything related to statements
    /// @note This class cannot be initialized and all functions within this class are static
    class Statement {
      public:
        // The constructor is deleted to make this class non-initializable
        Statement() = delete;

        /// @function `generate_statement`
        /// @brief Generates a single statement
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `statement` The statement which will be generated
        /// @return `bool` Whether the generation of the statment was successful
        [[nodiscard]] static bool generate_statement(       //
            llvm::IRBuilder<> &builder,                     //
            GenerationContext &ctx,                         //
            const std::unique_ptr<StatementNode> &statement //
        );

        /// @function `clear_garbage`
        /// @brief Generates all calls for all the garbage to clean (like temporary string variables)
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @return `bool` Whether the code generation of the garbage cleanup was successful
        [[nodiscard]] static bool clear_garbage(                                                                         //
            llvm::IRBuilder<> &builder,                                                                                  //
            std::unordered_map<unsigned int, std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>>> &garbage //
        );

        /// @function `generate_body`
        /// @brief Generates a whole body
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @return `bool` Whether the generation of the whole body was successful
        [[nodiscard]] static bool generate_body( //
            llvm::IRBuilder<> &builder,          //
            GenerationContext &ctx               //
        );

        /// @function `generate_end_of_scope`
        /// @brief Generates the instructions that need to be applied at the end of a scope (variables going out of scope, for example)
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @return `bool` Whether the code generation of the end of scope was successful
        [[nodiscard]] static bool generate_end_of_scope( //
            llvm::IRBuilder<> &builder,                  //
            GenerationContext &ctx                       //
        );

        /// @function `generate_return_statement`
        /// @brief Generates the return statement from the given ReturnNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `return_node` The return node to generated
        /// @return `bool` Whether the code generation of the return statement was successful
        [[nodiscard]] static bool generate_return_statement( //
            llvm::IRBuilder<> &builder,                      //
            GenerationContext &ctx,                          //
            const ReturnNode *return_node                    //
        );

        /// @function `generate_throw_statement`
        /// @brief Generates the throw statement from the given ThrowNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `throw_node` The throw node to generate
        /// @return `bool` Whether the code generation of the throw statement was successful
        [[nodiscard]] static bool generate_throw_statement( //
            llvm::IRBuilder<> &builder,                     //
            GenerationContext &ctx,                         //
            const ThrowNode *throw_node                     //
        );

        /// @function `generate_if_blocks`
        /// @brief Generates all blocks of the if-chain
        ///
        /// @param `parent` The function the basic blocks will be created in
        /// @param `blocks` The list of all basic blocks which will be created
        /// @param `if_node` The if node containin ght whole if-chain to generate the BasicBlocks from
        static void generate_if_blocks(              //
            llvm::Function *parent,                  //
            std::vector<llvm::BasicBlock *> &blocks, //
            const IfNode *if_node                    //
        );

        /// @function `generate_if_statement`
        /// @brief Generates the if statement from the given IfNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `blocks` The list of all basic blocks the if bodies are contained in
        /// @param `nesting_level` The nesting level determines how "deep" one is inside the if-chain
        /// @param `if_node` The if node to generate
        /// @return `bool` Whether the code generation was successful
        [[nodiscard]] static bool generate_if_statement( //
            llvm::IRBuilder<> &builder,                  //
            GenerationContext &ctx,                      //
            std::vector<llvm::BasicBlock *> &blocks,     //
            unsigned int nesting_level,                  //
            const IfNode *if_node                        //
        );

        /// @function `generate_do_while_loop`
        /// @brief Generates the do-while loop from the given WhileNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `do_while_node` The do-while node to generate
        /// @return `bool` Whether the code generation of the do-while loop was successful
        [[nodiscard]] static bool generate_do_while_loop( //
            llvm::IRBuilder<> &builder,                   //
            GenerationContext &ctx,                       //
            const DoWhileNode *do_while_node              //
        );

        /// @function `generate_while_loop`
        /// @brief Generates the while loop from the given WhileNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `while_node` The while node to generate
        /// @return `bool` Whether the code generation of the while loop was successful
        [[nodiscard]] static bool generate_while_loop( //
            llvm::IRBuilder<> &builder,                //
            GenerationContext &ctx,                    //
            const WhileNode *while_node                //
        );

        /// @function `generate_for_loop`
        /// @brief Generates the for loop from the given ForLoopNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `for_node` The for loop node to generate
        /// @return `bool` Whether the code generation for the for loop was successful
        [[nodiscard]] static bool generate_for_loop( //
            llvm::IRBuilder<> &builder,              //
            GenerationContext &ctx,                  //
            const ForLoopNode *for_node              //
        );

        /// @function `generate_enh_for_loop`
        /// @brief Generates the enhanced for loop from the given EnhForloopNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `for_node` The enhanced for loop node to generate
        /// @return `bool` Whether the code generation of the enhanced for loop node was successful
        [[nodiscard]] static bool generate_enh_for_loop( //
            llvm::IRBuilder<> &builder,                  //
            GenerationContext &ctx,                      //
            const EnhForLoopNode *for_node               //
        );

        /// @function `generate_optional_switch_statement`
        /// @brief Generates the optional switch statement from the given SwitchStatement node
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `switch_statement` The switch statement to generate
        /// @param `switch_value` The optional value being switched on
        /// @return `bool` Whether the code generation of the switch statement was successful
        [[nodiscard]] static bool generate_optional_switch_statement( //
            llvm::IRBuilder<> &builder,                               //
            GenerationContext &ctx,                                   //
            const SwitchStatement *switch_statement,                  //
            llvm::Value *switch_value                                 //
        );

        /// @function `generate_switch_statement`
        /// @brief Generates the switch statement from the given SwitchStatement node
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `switch_statement` The switch statement to generate
        /// @param `switch_value` The optional value being switched on
        /// @return `bool` Whether the code generation of the switch statement was successful
        [[nodiscard]] static bool generate_variant_switch_statement( //
            llvm::IRBuilder<> &builder,                              //
            GenerationContext &ctx,                                  //
            const SwitchStatement *switch_statement,                 //
            llvm::Value *switch_value                                //
        );

        /// @function `generate_switch_statement`
        /// @brief Generates the switch statement from the given SwitchStatement node
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `switch_statement` The switch statement to generate
        /// @return `bool` Whether the code generation of the switch statement was successful
        [[nodiscard]] static bool generate_switch_statement( //
            llvm::IRBuilder<> &builder,                      //
            GenerationContext &ctx,                          //
            const SwitchStatement *switch_statement          //
        );

        /// @function `generate_catch_statement`
        /// @brief Generates the catch statement from the given CatchNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `catch_node` The catch node to generate
        /// @return `bool` Whether the code generation of the catch statement was successful
        [[nodiscard]] static bool generate_catch_statement( //
            llvm::IRBuilder<> &builder,                     //
            GenerationContext &ctx,                         //
            const CatchNode *catch_node                     //
        );

        /// @function `generate_group_declaration`
        /// @brief Generates the group declaration from the given GroupDeclarationNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `declaration_node` The group declaration node to generate
        /// @return `bool` Whether the code generation of the group declaration was successful
        [[nodiscard]] static bool generate_group_declaration( //
            llvm::IRBuilder<> &builder,                       //
            GenerationContext &ctx,                           //
            const GroupDeclarationNode *declaration_node      //
        );

        /// @function `generate_declaration`
        /// @brief Generates the declaration from the given DeclarationNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `declaration_node` The declaration node to generate
        /// @return `bool` Whether the code generation of the declaration was successful
        [[nodiscard]] static bool generate_declaration( //
            llvm::IRBuilder<> &builder,                 //
            GenerationContext &ctx,                     //
            const DeclarationNode *declaration_node     //
        );

        /// @function `generate_assignment`
        /// @brief Generates the assignment from the given AssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `assignment_node` The assignment node to generate
        /// @return `bool` Whether the code generation of the assignment was successful
        [[nodiscard]] static bool generate_assignment( //
            llvm::IRBuilder<> &builder,                //
            GenerationContext &ctx,                    //
            const AssignmentNode *assignment_node      //
        );

        /// @function `generate_group_assignment`
        /// @brief Generates the group assignment from the given GroupAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `group_assignment` The group assignemnt node to generate
        /// @return `bool` Whether the code generation for the group assignment was successful
        [[nodiscard]] static bool generate_group_assignment( //
            llvm::IRBuilder<> &builder,                      //
            GenerationContext &ctx,                          //
            const GroupAssignmentNode *group_assignment      //
        );

        /// @function `generate_data_field_assignment`
        /// @brief Generates the data field assignment from the given DataFieldAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `data_field_assignment` The data field assignment to generate
        /// @return `bool` Whether the code generation of the data field assignment was successful
        [[nodiscard]] static bool generate_data_field_assignment( //
            llvm::IRBuilder<> &builder,                           //
            GenerationContext &ctx,                               //
            const DataFieldAssignmentNode *data_field_assignment  //
        );

        /// @function `generate_grouped_data_field_assignment`
        /// @brief Generates the grouped field assignment from the given GroupedDataFieldAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `grouped_field_assignment` The grouped data field assignment to generate
        /// @return `bool` Whether the code generation of the grouped data field assingment was successful
        [[nodiscard]] static bool generate_grouped_data_field_assignment(  //
            llvm::IRBuilder<> &builder,                                    //
            GenerationContext &ctx,                                        //
            const GroupedDataFieldAssignmentNode *grouped_field_assignment //
        );

        /// @function `generate_array_assignment`
        /// @brief Generates the array assignment from the given ArrayAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `array_assignment` The array assignment to generate
        /// @return `bool` Whether the code genration of the array assignment was successful
        [[nodiscard]] static bool generate_array_assignment( //
            llvm::IRBuilder<> &builder,                      //
            GenerationContext &ctx,                          //
            const ArrayAssignmentNode *array_assignment      //
        );

        /// @function `generate_stacked_assignment`
        /// @brief Generates the stacked assignment from the given StackedAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `stacked_assignment` The stacked assignment to generate
        /// @return `bool` Whether the code generation of the stacked assignment was successful
        [[nodiscard]] static bool generate_stacked_assignment( //
            llvm::IRBuilder<> &builder,                        //
            GenerationContext &ctx,                            //
            const StackedAssignmentNode *stacked_assignment    //
        );

        /// @function `generate_stacked_array_assignment`
        /// @brief Generates the stacked array assignment from the given StackedArrayAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `stacked_assignment` The stacked array assignment to generate
        /// @return `bool` Whether the code generation of the stacked array assignment was successful
        [[nodiscard]] static bool generate_stacked_array_assignment( //
            llvm::IRBuilder<> &builder,                              //
            GenerationContext &ctx,                                  //
            const StackedArrayAssignmentNode *stacked_assignment     //
        );

        /// @function `generate_stacked_grouped_assignment`
        /// @brief Generates the stacked grouped assignment from the given StackedGroupedAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `stacked_assignment` The stacked grouped assignment to generate
        /// @return `bool` Whether the code generation of the stacked grouped assignment was successful
        [[nodiscard]] static bool generate_stacked_grouped_assignment( //
            llvm::IRBuilder<> &builder,                                //
            GenerationContext &ctx,                                    //
            const StackedGroupedAssignmentNode *stacked_assignment     //
        );

        /// @function `generate_unary_op_statement`
        /// @brief Generates the unary operation value from the given UnaryOpStatement
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the statement generation
        /// @param `unary_op` The unary operation to generate
        /// @return `bool` Whether the code generation of the unary operator statement was successful
        [[nodiscard]] static bool generate_unary_op_statement( //
            llvm::IRBuilder<> &builder,                        //
            GenerationContext &ctx,                            //
            const UnaryOpStatement *unary_op                   //
        );
    }; // subclass Statement

    /// @class `Expression`
    /// @brief The class which is responsible for generating everything related to expressions
    /// @note This class cannot be initialized and all functions within this class are static
    class Expression {
      public:
        // The constructor is deleted to make this class non-initializable
        Expression() = delete;

        /// @typedef `garbage_type`
        /// @brief The type of the collected garbage, which is a map where the 'expr_depth' is the key and its value is a list of all values
        /// that need to be cleaned up
        using garbage_type = std::unordered_map<unsigned int, std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>>>;

        /// @function `generate_expression`
        /// @brief Generates an expression from the given ExpressionNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `expression_node` The expression node to generate
        /// @param `is_reference` Whether the result of the expression should be a reference. This is only possible for certain
        /// expressions like variables for example, defaults to false
        /// @return `group_mapping` The value(s) containing the result of the expression
        static group_mapping generate_expression(  //
            llvm::IRBuilder<> &builder,            //
            GenerationContext &ctx,                //
            garbage_type &garbage,                 //
            const unsigned int expr_depth,         //
            const ExpressionNode *expression_node, //
            const bool is_reference = false        //
        );

        /// @function `generate_literal`
        /// @brief Generates the literal value from the given LiteralNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `literal_node` The literal node to generate
        /// @return `group_mapping` The value(s) containing the result of the literal
        static group_mapping generate_literal( //
            llvm::IRBuilder<> &builder,        //
            GenerationContext &ctx,            //
            garbage_type &garbage,             //
            const unsigned int expr_depth,     //
            const LiteralNode *literal_node    //
        );

        /// @function `generate_variable`
        /// @brief Generates the variable from the given VariableNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `variable_node` The variable node to generate
        /// @param `is_reference` Whether to return the value or the AllocaInst of the variable
        /// @return `llvm::Value *` The value containing the result of the variable
        static llvm::Value *generate_variable( //
            llvm::IRBuilder<> &builder,        //
            GenerationContext &ctx,            //
            const VariableNode *variable_node, //
            const bool is_reference = false    //
        );

        /// @function `generate_string_interpolation`
        /// @brief Generates the string interpolation expression from the given StringInterpolationNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `interpol_node` The string interpolation node to generate
        /// @retrn `llvm::Value *` The result of the string interpolation expression
        static llvm::Value *generate_string_interpolation( //
            llvm::IRBuilder<> &builder,                    //
            GenerationContext &ctx,                        //
            garbage_type &garbage,                         //
            const unsigned int expr_depth,                 //
            const StringInterpolationNode *interpol_node   //
        );

        /// @function `convert_type_to_ext`
        /// @brief Converts the given type to be compatible with external types, e.g. the SystemV ABI
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The contet of the expression generation
        /// @param `type` The type to convert
        /// @param `value` The value to convert
        /// @param `args` The output arguments in which to place the converted value(s)
        static void convert_type_to_ext(       //
            llvm::IRBuilder<> &builder,        //
            GenerationContext &ctx,            //
            const std::shared_ptr<Type> &type, //
            llvm::Value *const value,          //
            std::vector<llvm::Value *> &args   //
        );

        /// @function `convert_data_type_to_ext`
        /// @brief Converts the given data type to be compatible with external types, e.g. the SystemV ABI
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The contet of the expression generation
        /// @param `type` The data type to convert
        /// @param `value` The value to convert
        /// @param `args` The output arguments in which to place the converted value(s)
        static void convert_data_type_to_ext(  //
            llvm::IRBuilder<> &builder,        //
            GenerationContext &ctx,            //
            const std::shared_ptr<Type> &type, //
            llvm::Value *const value,          //
            std::vector<llvm::Value *> &args   //
        );

        /// @function `convert_type_from_ext`
        /// @brief Converts the returned value from the external type to the expected return type
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The contet of the expression generation
        /// @param `type` The Flint type we want the value to be converted to
        /// @param `value` The (return) value which needs to be converted from the external to the internal type. It's a reference to a
        /// pointer because it needs to be overwritten with a new pointer location to point at
        static void convert_type_from_ext(     //
            llvm::IRBuilder<> &builder,        //
            GenerationContext &ctx,            //
            const std::shared_ptr<Type> &type, //
            llvm::Value *&value                //
        );

        /// @function `convert_data_type_from_ext`
        /// @brief Converts the returned value from the external type to the expected return type
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The contet of the expression generation
        /// @param `type` The data type we want the value to be converted to
        /// @param `value` The (return) value which needs to be converted from the external to the internal type. It's a reference to a
        /// pointer because it needs to be overwritten with a new pointer location to point at
        static void convert_data_type_from_ext( //
            llvm::IRBuilder<> &builder,         //
            GenerationContext &ctx,             //
            const std::shared_ptr<Type> &type,  //
            llvm::Value *&value                 //
        );

        /// @function `generate_extern_call`
        /// @brief Generates a call to an external function defined in one of the FIP modules
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `call_node` The call node to generate
        /// @param `args` The already generated arguments, which will be refined and reordered to be ABI-compliant
        /// @return `group_mapping` The value(s) containing the result of the call of the extern function
        ///
        /// @note A lot of argument and return value changes happen in this function because of the 8-byte segmentation or 16 byte rule of
        /// x86_64. All edge cases regarding calls to external functions defined in other languages are contained and resolved within this
        /// function. Everything before the function and after this function is considered Flint-internal and does not comply to any ABI.
        /// This function is the "connection point" between Flint and the outside world, so to speak.
        static group_mapping generate_extern_call( //
            llvm::IRBuilder<> &builder,            //
            GenerationContext &ctx,                //
            const CallNodeBase *call_node,         //
            std::vector<llvm::Value *> &args       //
        );

        /// @function `generate_call`
        /// @brief Generates the call from the given CallNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `call_node` The call node to generate
        /// @return `group_mapping` The value(s) containing the result of the call
        static group_mapping generate_call( //
            llvm::IRBuilder<> &builder,     //
            GenerationContext &ctx,         //
            const CallNodeBase *call_node   //
        );

        /// @function `generate_instance_call`
        /// @brief Generates the instance call from the given InstanceCallNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `call_node` The call node to generate
        /// @return `group_mapping` The value(s) containing the result of the instance call
        static group_mapping generate_instance_call( //
            llvm::IRBuilder<> &builder,              //
            GenerationContext &ctx,                  //
            const InstanceCallNodeBase *call_node    //
        );

        /// @function `generate_rethrow`
        /// @brief Generates a catch block which re-throws the error of the call, if the call had an error
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `call_node` The call node which is used to generate the rethrow from
        static void generate_rethrow(     //
            llvm::IRBuilder<> &builder,   //
            GenerationContext &ctx,       //
            const CallNodeBase *call_node //
        );

        /// @function `generate_group_expression`
        /// @brief Generates a group expression from the given GroupExpressionNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `group_node` The group operation to generate
        /// @return `group_mapping` The value(s) containing the result of the group expression
        static group_mapping generate_group_expression( //
            llvm::IRBuilder<> &builder,                 //
            GenerationContext &ctx,                     //
            garbage_type &garbage,                      //
            const unsigned int expr_depth,              //
            const GroupExpressionNode *group_node       //
        );

        /// @function `generate_range_expression`
        /// @brief Generates a range expression from the given RangeExpressionNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `range_node` The range expression to generate
        /// @return `group_mapping` The values containing the result of the range expression
        ///
        /// @note The range expression always consists of two values being the lower and the upper bound
        static group_mapping generate_range_expression( //
            llvm::IRBuilder<> &builder,                 //
            GenerationContext &ctx,                     //
            garbage_type &garbage,                      //
            const unsigned int expr_depth,              //
            const RangeExpressionNode *range_node       //
        );

        /// @function `generate_initializer`
        /// @brief Generates an initializer from the given InitializerNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `initializer` The initializer to generate
        /// @return `group_mapping` The loaded value(s) of the initializer, representing every field of the loaded data
        static group_mapping generate_initializer( //
            llvm::IRBuilder<> &builder,            //
            GenerationContext &ctx,                //
            garbage_type &garbage,                 //
            const unsigned int expr_depth,         //
            const InitializerNode *initializer     //
        );

        /// @function `generate_optional_switch_expression`
        /// @brief Generates the optional switch expression from the given SwitchStatement node
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `switch_expression` The switch expression to generate
        /// @param `switch_value` The optional value being switched on
        /// @return `group_mapping` The result of the switch expression
        [[nodiscard]] static Generator::group_mapping generate_optional_switch_expression( //
            llvm::IRBuilder<> &builder,                                                    //
            GenerationContext &ctx,                                                        //
            garbage_type &garbage,                                                         //
            const unsigned int expr_depth,                                                 //
            const SwitchExpression *switch_expression,                                     //
            llvm::Value *switch_value                                                      //
        );

        /// @function `generate_variant_switch_expression`
        /// @brief Generates the variant switch expression from the given SwitchStatement node
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `switch_expression` The switch expression to generate
        /// @param `switch_value` The optional value being switched on
        /// @return `group_mapping` The result of the switch expression
        [[nodiscard]] static Generator::group_mapping generate_variant_switch_expression( //
            llvm::IRBuilder<> &builder,                                                   //
            GenerationContext &ctx,                                                       //
            garbage_type &garbage,                                                        //
            const unsigned int expr_depth,                                                //
            const SwitchExpression *switch_expression,                                    //
            llvm::Value *switch_value                                                     //
        );

        /// @function `generate_switch_expression`
        /// @brief Generates a switch expression from the given SwitchExpression node
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `switch_expression` The switch expression to generate
        /// @return `group_mapping` The result of the switch expression
        static group_mapping generate_switch_expression( //
            llvm::IRBuilder<> &builder,                  //
            GenerationContext &ctx,                      //
            garbage_type &garbage,                       //
            const unsigned int expr_depth,               //
            const SwitchExpression *switch_expression    //
        );

        /// @function `generate_array_initializer`
        /// @brief Generates an array initialization from a given ArrayInitializerNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `initializer` The array initializer to generate
        /// @return `llvm::Value *` The initialized array
        static llvm::Value *generate_array_initializer( //
            llvm::IRBuilder<> &builder,                 //
            GenerationContext &ctx,                     //
            garbage_type &garbage,                      //
            const unsigned int expr_depth,              //
            const ArrayInitializerNode *initializer     //
        );

        /// @function `generate_array_access`
        /// @brief Generates an array access from a given ArrayAccessNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `access` The array access to generate
        /// @return `llvm::Value *` The accessed element
        static llvm::Value *generate_array_access( //
            llvm::IRBuilder<> &builder,            //
            GenerationContext &ctx,                //
            garbage_type &garbage,                 //
            const unsigned int expr_depth,         //
            const ArrayAccessNode *access          //
        );

        /// @function `generate_array_access`
        /// @brief Generates an array access from all the needed information for the array access
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `base_expr_value` The (potentailly) already-generated base expression of the array access
        /// @param `result_type` The result type of the array access (being the `base_type` of the array type itself most of times)
        /// @param `base_expr` The base expression to generate, if no `base_expr_value` is provided
        /// @param `indexing_expressions` The indexing expressions to generate, whose results are the indices of the array access
        /// @return `llvm::Value *` The accessed element
        static llvm::Value *generate_array_access(                                   //
            llvm::IRBuilder<> &builder,                                              //
            GenerationContext &ctx,                                                  //
            garbage_type &garbage,                                                   //
            const unsigned int expr_depth,                                           //
            std::optional<llvm::Value *> base_expr_value,                            //
            const std::shared_ptr<Type> result_type,                                 //
            const std::unique_ptr<ExpressionNode> &base_expr,                        //
            const std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
        );

        /// @function `generate_array_slice`
        /// @brief Generates an array slice from all the needed information for the array slice access
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `base_expr_value` The (potentailly) already-generated base expression of the array slice access
        /// @param `result_type` The result type of the array access (being the `base_type` of the array type itself most of times)
        /// @param `base_expr` The base expression to generate, if no `base_expr_value` is provided
        /// @param `indexing_expressions` The indexing expressions to generate, whose results are the indices of the array access
        /// @return `llvm::Value *` The accessed element
        static llvm::Value *generate_array_slice(                                    //
            llvm::IRBuilder<> &builder,                                              //
            GenerationContext &ctx,                                                  //
            garbage_type &garbage,                                                   //
            const unsigned int expr_depth,                                           //
            std::optional<llvm::Value *> base_expr_value,                            //
            const std::unique_ptr<ExpressionNode> &base_expr,                        //
            const std::vector<std::unique_ptr<ExpressionNode>> &indexing_expressions //
        );

        /// @function `get_bool8_element_at`
        /// @brief Generates all the IR code to access a single element of an bool8 variable and returns the read value
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `b8_val` The loaded bool8 (u8) value
        /// @param `elem_idx` The index to access from the b8_val
        /// @return `llvm::Value *` The loaded element of the bool8 value at the given index
        static llvm::Value *get_bool8_element_at(llvm::IRBuilder<> &builder, llvm::Value *b8_val, unsigned int elem_idx);

        /// @function `set_bool8_element_at`
        /// @brief Sets the bit at the given index inside the given bool8 variable
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `b8_val` The loaded bool8 (u8) value to modify
        /// @param `bit` The value the bit gets set to
        /// @param `elem_idx` The index of the bit to set
        /// @return `llvm::Value *` The modified b8_val which needs to be stored back on the original variable
        static llvm::Value *set_bool8_element_at(llvm::IRBuilder<> &builder, llvm::Value *b8_val, llvm::Value *bit, unsigned int elem_idx);

        /// @function `generate_data_access`
        /// @brief Generates a data access from a given DataAccessNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `data_access` The data access node to generate
        /// @return `group_mapping` The value containing the result of the data access, nullopt if generation failed
        static group_mapping generate_data_access( //
            llvm::IRBuilder<> &builder,            //
            GenerationContext &ctx,                //
            garbage_type &garbage,                 //
            const unsigned int expr_depth,         //
            const DataAccessNode *data_access      //
        );

        /// @function `generate_grouped_data_access`
        /// @brief Generates a grouped data access from a given GroupedDataAccessNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `grouped_data_access` The grouped data access node to generate
        /// @return `group_mapping` The value(s) containing the result of the grouped data access
        static group_mapping generate_grouped_data_access(   //
            llvm::IRBuilder<> &builder,                      //
            GenerationContext &ctx,                          //
            garbage_type &garbage,                           //
            const unsigned int expr_depth,                   //
            const GroupedDataAccessNode *grouped_data_access //
        );

        /// @function `generate_optional_chain`
        /// @brief Generates an optional chain from a given OptionalChainNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `chain` The optonal chain node to generate
        /// @return `group_mapping` The value(s) containing the result of the optional chain
        static group_mapping generate_optional_chain( //
            llvm::IRBuilder<> &builder,               //
            GenerationContext &ctx,                   //
            garbage_type &garbage,                    //
            const unsigned int expr_depth,            //
            const OptionalChainNode *chain            //
        );

        /// @function `generate_optional_unwrap`
        /// @brief Generates an optional unwrap from a given OptionalUnwrapNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `unwrap` The optional unwrap node node to generate
        /// @return `group_mapping` The value(s) containing the result of the optional unwrap
        static group_mapping generate_optional_unwrap( //
            llvm::IRBuilder<> &builder,                //
            GenerationContext &ctx,                    //
            garbage_type &garbage,                     //
            const unsigned int expr_depth,             //
            const OptionalUnwrapNode *unwrap           //
        );

        /// @function `generate_variant_extraction`
        /// @brief Generates a variant extraction from a given VariantExtractionNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `extraction` The variant extraction node to generate
        /// @return `group_mapping` The value containing the result of the variant extraction
        static group_mapping generate_variant_extraction( //
            llvm::IRBuilder<> &builder,                   //
            GenerationContext &ctx,                       //
            const VariantExtractionNode *extraction       //
        );

        /// @function `generate_variant_unwrap`
        /// @brief Generates an variant unwrap from a given VariantUnwrapNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `unwrap` The variant unwrap node node to generate
        /// @return `group_mapping` The value(s) containing the result of the variant unwrap
        static group_mapping generate_variant_unwrap( //
            llvm::IRBuilder<> &builder,               //
            GenerationContext &ctx,                   //
            const VariantUnwrapNode *unwrap           //
        );

        /// @function `generate_type_cast`
        /// @brief Generates a type cast from a TypeCastNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `type_cast_node` The type cast to generate
        /// @param `is_reference` Whether the result of the expression should be a reference. This is only possible for certain
        /// @return `group_mapping` The value(s) containing the result of the type cast
        static group_mapping generate_type_cast( //
            llvm::IRBuilder<> &builder,          //
            GenerationContext &ctx,              //
            garbage_type &garbage,               //
            const unsigned int expr_depth,       //
            const TypeCastNode *type_cast_node,  //
            const bool is_reference = false      //
        );

        /// @function `generate_type_cast`
        /// @brief Generates a type cast from the given expression depending on the from and to types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `expr` The llvm value which will be cast
        /// @param `from_type` The type to cast from
        /// @param `to_type` The type to cast to
        /// @return `llvm::Value *` The value containing the result of the type cast
        static llvm::Value *generate_type_cast(     //
            llvm::IRBuilder<> &builder,             //
            GenerationContext &ctx,                 //
            llvm::Value *expr,                      //
            const std::shared_ptr<Type> &from_type, //
            const std::shared_ptr<Type> &to_type    //
        );

        /// @function `generate_unary_op_expression`
        /// @brief Generates the unary operation value from the given UnaryOpNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `unary_op` The unary operation to generate
        /// @return `group_mapping` The value containing the result of the unary operation
        static group_mapping generate_unary_op_expression( //
            llvm::IRBuilder<> &builder,                    //
            GenerationContext &ctx,                        //
            garbage_type &garbage,                         //
            const unsigned int expr_depth,                 //
            const UnaryOpExpression *unary_op              //
        );

        /// @function `generate_binary_op`
        /// @brief Generates a binary operation from the given BinaryOpNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
        /// @param `bin_op_node` The binary operation to generate
        /// @return `group_mapping` The value(s) containing the result of the binop
        static group_mapping generate_binary_op( //
            llvm::IRBuilder<> &builder,          //
            GenerationContext &ctx,              //
            garbage_type &garbage,               //
            const unsigned int expr_depth,       //
            const BinaryOpNode *bin_op_node      //
        );

        /// @function `generate_binary_op_set_cmp`
        /// @brief Generates a scalar vs group binary operator comparison for set-like comparisons
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 every layer)
        /// @param `bin_op_node` The binary operation to generate the set comparisons from
        /// @param `lhs` The list of all lhs values, 1 for scalar, n for group
        /// @param `rhs` The list of all rhs values, 1 for scalar, n for group
        /// @return `std::optional<llvm::Value *>` The result of the binary op comparison
        static std::optional<llvm::Value *> generate_binary_op_set_cmp( //
            llvm::IRBuilder<> &builder,                                 //
            GenerationContext &ctx,                                     //
            garbage_type &garbage,                                      //
            const unsigned int expr_depth,                              //
            const BinaryOpNode *bin_op_node,                            //
            std::vector<llvm::Value *> lhs,                             //
            std::vector<llvm::Value *> rhs                              //
        );

        /// @function `generate_binary_op_scalar`
        /// @brief Generates the binary operation for scalar binary ops
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 every layer)
        /// @param `bin_op_node` The binary operation to generate
        /// @param `type_str` The string representation of the type
        /// @param `lhs` The left hand side llvm instruction
        /// @param `rhs` The right hand side llvm instruction
        /// @return `std::optional<llvm::Value *>` The result of the binary operation
        static std::optional<llvm::Value *> generate_binary_op_scalar( //
            llvm::IRBuilder<> &builder,                                //
            GenerationContext &ctx,                                    //
            garbage_type &garbage,                                     //
            const unsigned int expr_depth,                             //
            const BinaryOpNode *bin_op_node,                           //
            const std::string &type_str,                               //
            llvm::Value *lhs,                                          //
            llvm::Value *rhs                                           //
        );

        /// @struct `FakeBinaryOpNode`
        /// @brief A "Fake" Binary op node used for the *actual* generation of the binary operation, which has *all* the fields of a
        /// "normal" BinaryOpNode
        struct FakeBinaryOpNode {
            const Token operator_token;
            const ExpressionNode *left;
            const ExpressionNode *right;
            const std::shared_ptr<Type> &type;
            const bool is_shorthand;
        };

        /// @function `generate_binary_op_scalar`
        /// @brief Generates the binary operation for scalar binary ops
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 every layer)
        /// @param `bin_op_node` The fake binary operation to generate
        /// @param `type_str` The string representation of the type
        /// @param `lhs` The left hand side llvm instruction
        /// @param `rhs` The right hand side llvm instruction
        /// @return `std::optional<llvm::Value *>` The result of the binary operation
        static std::optional<llvm::Value *> generate_binary_op_scalar( //
            llvm::IRBuilder<> &builder,                                //
            GenerationContext &ctx,                                    //
            garbage_type &garbage,                                     //
            const unsigned int expr_depth,                             //
            const FakeBinaryOpNode *bin_op_node,                       //
            const std::string &type_str,                               //
            llvm::Value *lhs,                                          //
            llvm::Value *rhs                                           //
        );

        /// @function `generate_optional_cmp`
        /// @brief Generates the instructions for the optional comparison of the lhs and rhs
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `garbage` A list of all accumulated temporary variables that need cleanup
        /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 every layer)
        /// @param `lhs` The left hand side llvm instruction of the comparison
        /// @param `lhs_expr` The left hand side expression node of the comparison
        /// @param `rhs` The right hand side llvm instruction of the comparison
        /// @param `hhs_expr` The right hand side expression node of the comparison
        /// @param `eq` Whether to compare for equality (true) or inequality (false)
        /// @return `std::optional<llvm::Value *>` The result of the optional comparison
        static std::optional<llvm::Value *> generate_optional_cmp( //
            llvm::IRBuilder<> &builder,                            //
            GenerationContext &ctx,                                //
            garbage_type &garbage,                                 //
            const unsigned int expr_depth,                         //
            llvm::Value *lhs,                                      //
            const ExpressionNode *lhs_expr,                        //
            llvm::Value *rhs,                                      //
            const ExpressionNode *rhs_expr,                        //
            const bool eq                                          //
        );

        /// @function `generate_variant_cmp`
        /// @brief Generates the instructions for the variant comparison of the lhs and rhs
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `ctx` The context of the expression generation
        /// @param `lhs` The left hand side llvm instruction of the comparison
        /// @param `lhs_expr` The left hand side expression node of the comparison
        /// @param `rhs` The right hand side llvm instruction of the comparison
        /// @param `hhs_expr` The right hand side expression node of the comparison
        /// @param `eq` Whether to compare for equality (true) or inequality (false)
        /// @return `std::optional<llvm::Value *>` The result of the variant comparison
        static std::optional<llvm::Value *> generate_variant_cmp( //
            llvm::IRBuilder<> &builder,                           //
            GenerationContext &ctx,                               //
            llvm::Value *lhs,                                     //
            const ExpressionNode *lhs_expr,                       //
            llvm::Value *rhs,                                     //
            const ExpressionNode *rhs_expr,                       //
            const bool eq                                         //
        );

        /// @function `generate_binary_op_vector`
        /// @brief Generates the binary operation for vector binary ops
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `bin_op_node` The binary operation to generate
        /// @param `type_str` The string representation of the type
        /// @param `lhs` The left hand side llvm instruction
        /// @param `rhs` The right hand side llvm instruction
        /// @return `std::optional<llvm::Value *>` The result of the binary operation
        static std::optional<llvm::Value *> generate_binary_op_vector( //
            llvm::IRBuilder<> &builder,                                //
            const BinaryOpNode *bin_op_node,                           //
            const std::string &type_str,                               //
            llvm::Value *lhs,                                          //
            llvm::Value *rhs                                           //
        );
    }; // subclass Expression

    /// @class `Error`
    /// @brief The class which contains the generation functions and pointer to the functions for resolving all error to string requests
    /// @note This class cannot be initialized and all functions within this class are static
    class Error {
      public:
        // The constructor is deleted to make this class non-initializable
        Error() = delete;

        /// @var `error_functions`
        /// @brief Map containing references to all error functions
        ///
        /// @details
        /// - **Key** `std::string_view` - The name of the function
        /// - **Value** `llvm::Function *` - The reference to the genereated function
        ///
        /// @attention The functions are nullpointers until the `generate_error_functions` function is called
        /// @attention The map is not being cleared after the program module has been generated
        static inline std::unordered_map<std::string_view, llvm::Function *> error_functions = {
            {"get_err_type_str", nullptr},
            {"get_err_val_str", nullptr},
            {"get_err_str", nullptr},
        };

        /// @function `generate_error_functions`
        /// @brief Generates all the error functions used to resolve error types and get the error type strings from them
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which the functions are generated in
        static void generate_error_functions(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_get_err_type_str_function`
        /// @brief Generates the `get_err_type_str` function used to resolve error types
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which the functions are generated in
        static void generate_get_err_type_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_get_err_val_str_function`
        /// @brief Generates the `get_err_val_str` function used to resolve error values
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which the functions are generated in
        static void generate_get_err_val_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_get_err_str_function`
        /// @brief Generates the `get_err_str` function used to cast whole errors to strings (resulting in a `ErrType.ErrVal` string)
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which the functions are generated in
        static void generate_get_err_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);
    };

    /// @class `Memory`
    /// @brief The class which contains the generation functions and pointer to the functions for memory-related operations in Flint
    /// @note This class cannot be initialized and all functions within this class are static
    class Memory {
      public:
        // The constructor is deleted to make this class non-initializable
        Memory() = delete;

        /// @var `memory_functions`
        /// @brief Map containing references to all memory functions
        ///
        /// @details
        /// - **Key** `std::string_view` - The name of the function
        /// - **Value** `llvm::Function *` - The reference to the genereated function
        ///
        /// @attention The functions are nullpointers until the `generate_memory_functions` function is called
        /// @attention The map is not being cleared after the program module has been generated
        static inline std::unordered_map<std::string_view, llvm::Function *> memory_functions = {
            {"free", nullptr},
            {"clone", nullptr},
        };

        /// @function `generate_memory_functions`
        /// @brief Generates all the error functions used for memory-related operations in Flint
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which the functions are generated in
        /// @param `only_declarations` Whether to only generate the declarations for the functions
        static void generate_memory_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = false);

        /// @function `generate_free_value`
        /// @brief Generates the IR code to free the given type
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which to generate the freeing of the given value
        /// @param `value` The value to free properly and orderly-correct
        /// @param `type` The type of the value to free
        static void generate_free_value(      //
            llvm::IRBuilder<> *const builder, //
            llvm::Module *const module,       //
            llvm::Value *const value,         //
            const std::shared_ptr<Type> &type //
        );

        /// @function `generate_free_function`
        /// @brief Generates the `free` function used to free values at runtime
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which the function is generated in
        /// @param `only_declatations` Whether to only generate the declaration for the `free` function
        static void generate_free_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

        /// @function `generate_clone_value`
        /// @brief Generates the IR code to clone the given type
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which to generate the cloning of the given value
        /// @param `src` The value to clone properly and orderly-correct
        /// @param `dest` The place where to store the cloned value after cloning it. This is needed because of things like variants or
        /// tuples, which potentially need to be cloned as well but are not heap-allocated, so there needs to already exist memory for them
        /// to be placed at. Because of this we need to pass a pointer to the clone function instead of recieving a generic pointer from it
        /// @param `type` The type of the value to clone
        /// @return `llvm::Value *` A pointer to the cloned value
        static void generate_clone_value(     //
            llvm::IRBuilder<> *const builder, //
            llvm::Module *const module,       //
            llvm::Value *const src,           //
            llvm::Value *const dest,          //
            const std::shared_ptr<Type> &type //
        );

        /// @function `generate_clone_function`
        /// @brief Generates the `clone` function used to clone values at runtime
        ///
        /// @param `builder` The IRBuilder
        /// @param `module` The module in which the function is generated in
        /// @param `only_declatations` Whether to only generate the declaration for the `clone` function
        static void generate_clone_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);
    };

    /// @class 'Module'
    /// @brief The class which holds all other module sub-classes
    /// @note This class cannot be initialized and all functions within this class are static
    class Module {
      public:
        // The constructor is deleted to make this class non-initializable
        Module() = delete;

        /// @function `generate_dima_heads`
        /// @brief Generates the DIMA heads for the given module's name
        ///
        /// @param `module` The LLVM Module in which to generate the dima heads
        /// @param `module_name` The name of the module to generate the dima heads from
        static void generate_dima_heads(llvm::Module *module, const std::string &module_name);

        /// @function `generate_module`
        /// @brief Generates a single module and compiles it to its .o file
        ///
        /// @param `lib_to_build` The library to build
        /// @param `cache_path` The path to the cache directory
        /// @param `module_name` The name of the generated module
        /// @return `bool` Whether the module was able to be generated
        static bool generate_module(                 //
            const BuiltinLibrary lib_to_build,       //
            const std::filesystem::path &cache_path, //
            const std::string &module_name           //
        );

        /// @function `generate_modules`
        /// @brief This function generates all modules and compiles them to their .o files
        ///
        /// @return `bool` Whether everything worked out as expected, false if any errors occurred
        static bool generate_modules();

        /// @function `which_modules_to_rebuild`
        /// @brief Checks which modules to rebuild
        ///
        /// @return `unsigned int` The bitfield of which modules to rebuild
        static unsigned int which_modules_to_rebuild();

        /// @function `save_metadata_json_file`
        /// @brief Saves the metadata json file from the given arguments
        ///
        /// @param `overflow_mode_value` The overflow mode value to save
        /// @param `oob_mode_value` The out of bounds mode value to save
        static void save_metadata_json_file(int overflow_mode_value, int oob_mode_value);

        /// @class `Arithmetic`
        /// @brief The class which is responsible for everything arithmetic-related
        /// @note This class cannot be initialized and all functions within this class are static
        class Arithmetic {
          public:
            // The constructor is deleted to make this class non-initializable
            Arithmetic() = delete;

            /// @var `arithmetic_functions`
            /// @brief Map containing references to all safe arithmetic functions
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_arithmetic_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string, llvm::Function *> arithmetic_functions = {
                // Signed Integer Types
                {"i32_safe_add", nullptr},
                {"i32_safe_sub", nullptr},
                {"i32_safe_mul", nullptr},
                {"i32_safe_div", nullptr},
                {"i32_pow", nullptr},
                {"i64_safe_add", nullptr},
                {"i64_safe_sub", nullptr},
                {"i64_safe_mul", nullptr},
                {"i64_safe_div", nullptr},
                {"i64_pow", nullptr},
                // Unsigned Integer Types
                {"u8_safe_add", nullptr},
                {"u8_safe_sub", nullptr},
                {"u8_safe_mul", nullptr},
                {"u8_safe_div", nullptr},
                {"u8_pow", nullptr},
                {"u32_safe_add", nullptr},
                {"u32_safe_sub", nullptr},
                {"u32_safe_mul", nullptr},
                {"u32_safe_div", nullptr},
                {"u32_pow", nullptr},
                {"u64_safe_add", nullptr},
                {"u64_safe_sub", nullptr},
                {"u64_safe_mul", nullptr},
                {"u64_safe_div", nullptr},
                {"u64_pow", nullptr},
                // Signed Multi Types of length 2
                {"i32x2_safe_add", nullptr},
                {"i32x2_safe_sub", nullptr},
                {"i32x2_safe_mul", nullptr},
                {"i32x2_safe_div", nullptr},
                {"i64x2_safe_add", nullptr},
                {"i64x2_safe_sub", nullptr},
                {"i64x2_safe_mul", nullptr},
                {"i64x2_safe_div", nullptr},
                // Signed Multi Types of length 3
                {"i32x3_safe_add", nullptr},
                {"i32x3_safe_sub", nullptr},
                {"i32x3_safe_mul", nullptr},
                {"i32x3_safe_div", nullptr},
                {"i64x3_safe_add", nullptr},
                {"i64x3_safe_sub", nullptr},
                {"i64x3_safe_mul", nullptr},
                {"i64x3_safe_div", nullptr},
                // Signed Multi Types of length 4
                {"i32x4_safe_add", nullptr},
                {"i32x4_safe_sub", nullptr},
                {"i32x4_safe_mul", nullptr},
                {"i32x4_safe_div", nullptr},
                {"i64x4_safe_add", nullptr},
                {"i64x4_safe_sub", nullptr},
                {"i64x4_safe_mul", nullptr},
                {"i64x4_safe_div", nullptr},
                // Signed Multi Types of length 8
                {"i32x8_safe_add", nullptr},
                {"i32x8_safe_sub", nullptr},
                {"i32x8_safe_mul", nullptr},
                {"i32x8_safe_div", nullptr},
            };

            /// @function `generate_arithmetic_functions`
            /// @brief Generates all arithmetic functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the arithmetic functions will be generated in (or in which the declarations are added)
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_arithmetic_functions( //
                llvm::IRBuilder<> *builder,            //
                llvm::Module *module,                  //
                const bool only_declarations = true    //
            );

            /// @function `refresh_arithmetic_functions`
            /// @brief Refreshes the pointers to the arithmetic functions
            ///
            /// @param `module` The LLVM Module the arithmetic functions should already be defined in
            /// @return `bool` Whether all refreshs were successful
            static bool refresh_arithmetic_functions(llvm::Module *module);

            /// @function `generate_pow_function`
            /// @brief Generates the pow function for the given integer type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'i/uX_pow' function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `is_signed` Whether the integer type is signed
            static void generate_pow_function( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::IntegerType *int_type,   //
                const bool is_signed           //
            );

            /// @function `generate_int_safe_add`
            /// @brief Creates a safe addition of two int types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iX_safe_add` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `i32` or `i64` at the moment
            static void generate_int_safe_add( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::IntegerType *int_type,   //
                const std::string &name        //
            );

            /// @function `generate_int_safe_sub`
            /// @brief Creates a safe subtraction of two signed integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iX_safe_sub` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `i32` or `i64` at the moment
            static void generate_int_safe_sub( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::IntegerType *int_type,   //
                const std::string &name        //
            );

            /// @function `generate_int_safe_mul`
            /// @brief Creates a safe multiplication of two signed integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iX_safe_mul` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `i32` or `i64` at the moment
            static void generate_int_safe_mul( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::IntegerType *int_type,   //
                const std::string &name        //
            );

            /// @function `generate_int_safe_mul_small`
            /// @brief Creates a safe multiplication of two signed integer types for all types <= 32 bit width
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iX_safe_mul_small` function will be generated in
            /// @param `int_safe_mul_fn` The to-be-created int safe mul function
            /// @param `int_type` The type of the integer arguments the safe mul function will be created with
            /// @param `name` The name of the integer type
            /// @param `arg_lhs` The lhs argument of the function to create
            /// @param `arg_rhs` The rhs argument of the function to create
            static void generate_int_safe_mul_small( //
                llvm::IRBuilder<> *builder,          //
                llvm::Module *module,                //
                llvm::Function *int_safe_mul_fn,     //
                llvm::IntegerType *int_type,         //
                const std::string &name,             //
                llvm::Argument *arg_lhs,             //
                llvm::Argument *arg_rhs              //
            );

            /// @function `generate_int_safe_div`
            /// @brief Creates a safe division of two signed integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iX_safe_div` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `i32` or `i64` at the moment
            static void generate_int_safe_div( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::IntegerType *int_type,   //
                const std::string &name        //
            );

            /// @function `generate_int_safe_mod`
            /// @brief Creates a safe modulo of two integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iX_safe_mod` / `uX_safe_mod` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `i32`, `i64`, `u32` or `u64` at the moment
            /// @param `is_signed` Whether the given int type is signed
            static void generate_int_safe_mod( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::IntegerType *int_type,   //
                const std::string &name,       //
                const bool is_signed           //
            );

            /// @function `generate_uint_safe_add`
            /// @brief Creates a safe addition of two unsigned integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `uX_safe_add` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `u32` or `u64` at the moment
            static void generate_uint_safe_add( //
                llvm::IRBuilder<> *builder,     //
                llvm::Module *module,           //
                const bool only_declarations,   //
                llvm::IntegerType *int_type,    //
                const std::string &name         //
            );

            /// @function `generate_uint_safe_sub`
            /// @brief Creates a safe subtraction of two unsigned integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `uX_safe_sub` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `u32` or `u64` at the moment
            static void generate_uint_safe_sub( //
                llvm::IRBuilder<> *builder,     //
                llvm::Module *module,           //
                const bool only_declarations,   //
                llvm::IntegerType *int_type,    //
                const std::string &name         //
            );

            /// @function `generate_uint_safe_mul`
            /// @brief Creates a safe multiplication of two unsigned integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `uX_safe_mul` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `u32` or `u64` at the moment
            static void generate_uint_safe_mul( //
                llvm::IRBuilder<> *builder,     //
                llvm::Module *module,           //
                const bool only_declarations,   //
                llvm::IntegerType *int_type,    //
                const std::string &name         //
            );

            /// @function `generate_uint_safe_div`
            /// @brief Creates a safe division of two unsigned integer types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `uX_safe_mul` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `int_type` The integer type to generate the function for
            /// @param `name` The name of the generated function, is `u32` or `u64` at the moment
            static void generate_uint_safe_div( //
                llvm::IRBuilder<> *builder,     //
                llvm::Module *module,           //
                const bool only_declarations,   //
                llvm::IntegerType *int_type,    //
                const std::string &name         //
            );

            /// @function `generate_int_vector_safe_add`
            /// @brief Creates a safe addition of two signed integer vector types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iMxN_safe_add` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `vector_int_type` The vector type to generate the function for
            /// @param `vector_width` The width of the vector
            /// @param `name` The name of the generated function (The name of the multi-type, e.g. 'i32x3' or 'i64x2' for example)
            static void generate_int_vector_safe_add( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations,         //
                llvm::VectorType *vector_int_type,    //
                const unsigned int vector_width,      //
                const std::string &name               //
            );

            /// @function `generate_int_vector_safe_sub`
            /// @brief Creates a safe subtraction of two signed integer vector types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iMxN_safe_sub` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `vector_int_type` The vector type to generate the function for
            /// @param `vector_width` The width of the vector
            /// @param `name` The name of the generated function (The name of the multi-type, e.g. 'i32x3' or 'i64x2' for example)
            static void generate_int_vector_safe_sub( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations,         //
                llvm::VectorType *vector_int_type,    //
                const unsigned int vector_width,      //
                const std::string &name               //
            );

            /// @function `generate_int_vector_safe_mul`
            /// @brief Creates a safe multiplication of two signed integer vector types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iMxN_safe_mul` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `vector_int_type` The vector type to generate the function for
            /// @param `vector_width` The width of the vector
            /// @param `name` The name of the generated function (The name of the multi-type, e.g. 'i32x3' or 'i64x2' for example)
            static void generate_int_vector_safe_mul( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations,         //
                llvm::VectorType *vector_int_type,    //
                const unsigned int vector_width,      //
                const std::string &name               //
            );

            /// @function `generate_int_vector_safe_div`
            /// @brief Creates a safe division of two signed integer vector types
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `iMxN_safe_div` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `vector_int_type` The vector type to generate the function for
            /// @param `vector_width` The width of the vector
            /// @param `name` The name of the generated function (The name of the multi-type, e.g. 'i32x3' or 'i64x2' for example)
            static void generate_int_vector_safe_div( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations,         //
                llvm::VectorType *vector_int_type,    //
                const unsigned int vector_width,      //
                const std::string &name               //
            );
        }; // subclass Arithmetic

        /// @class `Array`
        /// @brief The class which is responsible for generating everything related to arrays
        /// @note This class cannot be initlaized and all functions within this class are static
        class Array {
          public:
            // The constructor is deleted to make this class non-initializable
            Array() = delete;

            /// @var `array_manip_functions`
            /// @brief Map containing references to all array manipulation functions, to make array handling easier
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_array_manip_functions` function is called
            static inline std::unordered_map<std::string_view, llvm::Function *> array_manip_functions = {
                {"get_arr_len", nullptr},
                {"create_arr", nullptr},
                {"fill_arr_inline", nullptr},
                {"fill_arr_deep", nullptr},
                {"fill_arr_val", nullptr},
                {"access_arr", nullptr},
                {"access_arr_val", nullptr},
                {"assign_arr_at", nullptr},
                {"assign_arr_val_at", nullptr},
                {"get_arr_slice_1d", nullptr},
                {"get_arr_slice", nullptr},
            };

            /// @function `generate_get_arr_len_function`
            /// @brief Generates the builtin hidded `get_arr_len` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `get_arr_len` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_get_arr_len_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_create_arr_function`
            /// @brief Generates the builtin hidden `create_arr` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `create_arr` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_create_arr_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_fill_arr_inline_function`
            /// @brief Generates the builtin hidden `fill_arr_inline` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `fill_arr_inline` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_fill_arr_inline_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_fill_arr_deep_function`
            /// @brief Generates the builtin hidden `fill_arr_deep` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `fill_arr_deep` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_fill_arr_deep_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_fill_arr_val_function`
            /// @brief Generates the builtin hidden `fill_arr_val` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `fill_arr_val` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the generation for it
            static void generate_fill_arr_val_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_access_arr_function`
            /// @brief Generates the builtin hidden `access_arr` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `access_arr` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_access_arr_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_access_arr_val_function`
            /// @brief Generates the builtin hidden `access_arr_val` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `access_arr_val` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_access_arr_val_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_assign_arr_at_function`
            /// @brief Generates the builtin hidden `assign_arr_at` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `assign_arr_at` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_assign_arr_at_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_assign_arr_val_at_function`
            /// @brief Generates the builtin hidden `assign_arr_val_at` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `assign_arr_val_at` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_assign_arr_val_at_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_get_arr_slice_1d_function`
            /// @brief Generates the builtin hidden `get_arr_slice_1d` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `get_arr_slice_1d` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_get_arr_slice_1d_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_get_arr_slice_function`
            /// @brief Generates the builtin hidden `get_arr_slice` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `get_arr_slice` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_get_arr_slice_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_array_manip_functions`
            /// @brief Generates all the builtin hidden array manipulation functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the array manipulation functions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_array_manip_functions( //
                llvm::IRBuilder<> *builder,             //
                llvm::Module *module,                   //
                const bool only_declarations = true     //
            );
        }; // subclass Array

        /// @class `Assert`
        /// @brief The class which is responsible for generating everything related to assert
        /// @note This class cannot be initialized and all functions within this class are static
        class Assert {
          public:
            // The constructor is deleted to make this class non-initializable
            Assert() = delete;

            /// @var `assert_functions`
            /// @brief Map containing references to all assert functions
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_assert_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> assert_functions = {
                {"assert", nullptr},
            };

            /// @function `generate_assert_functions`
            /// @brief Generates the builtin assert functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the assert functions definitions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_assert_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_assert_function`
            /// @brief Helper function to generate the builtin assert
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the assert function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_assert_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);
        };

        /// @class `DIMA`
        /// @brief The class which is reponsible for generating everything related to Flint's Deterministic Incremental Memory Architecture
        /// @note This class cannot be initialized and all functions within this class are static
        class DIMA {
          public:
            // The constructor is deleted to make this class non-initializable
            DIMA() = delete;

            /// @var `BASE_CAPACITY`
            /// @brief The base capacity of the smallest block. The size of each block is calculated with the formula of
            ///        (BASE_CAPACITY * ((GROWTH_FACTOR / 10) ** BLOCK_ID))
            static const inline constexpr size_t BASE_CAPACITY = 16;

            /// @var `GROWTH_FACTOR`
            /// @brief The growth factor with which dima blocks grow. The actual growth factor is calculated by dividing the factor by 10.
            /// So when writing 15 we have a growth factor of 1.5. But with a growth factor of 20 it will be 2.0.
            static const inline constexpr size_t GROWTH_FACTOR = 11;

            /// @enum `Flags`
            /// @brief The possible flags stored in each slot of DIMA
            enum class Flags : uint16_t {
                UNUSED = 0,
                OCCUPIED = 1,
                ARRAY_START = 2,
                ARRAY_END = 4,
            };

            /// @var `dima_functions`
            /// @brief Map containing references to all dima functions
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_dima_functions` function is called
            static inline std::unordered_map<std::string_view, llvm::Function *> dima_functions = {
                {"get_block_capacity", nullptr},
                {"init_heads", nullptr},
                {"create_block", nullptr},
                {"allocate_in_block", nullptr},
                {"allocate", nullptr},
                {"retain", nullptr},
                {"release", nullptr},
            };

            /// @var `dima_heads`
            /// @brief Map contianing references to all global variables which then point to the allocated heads
            ///
            /// @details
            /// - **Key** `std::string` - The key has the format <hash>.<name> where the hash is the file hash of the type the dima type
            ///                           comes from and the <name> is the type name of the dima type
            /// - **Value** `llvm::GlobalVariable *` - The reference to the global variable pointing at the head
            static inline std::unordered_map<std::string, llvm::GlobalVariable *> dima_heads;

            /// @function `get_head`
            /// @brief Returns the dima head of the given type
            ///
            /// @param `type` The type to get the dima head from
            /// @return `llvm::GlobalVariable *` The dima head of the given type
            static llvm::GlobalVariable *get_head(const std::shared_ptr<Type> &type);

            /// @function `generate_heads`
            /// @brief Generates all the global definitions for the heads, since they are linked together anyways we need to define them in
            /// every single module, so at the beginning of the module generation for each module we simply call this function to refresh
            /// all global heads and add them to every single module to make the verifyer happy
            ///
            /// @param `module` The module to which to add the global head variables
            static void generate_heads(llvm::Module *module);

            /// @function `generate_dima_functions`
            /// @brief Generates all the builtin hidden dima functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the dima functions will be generated in
            /// @param `is_core_generation` Whether we generate all runtime-independant functions of DIMA in the `dima.o` file which is
            ///         linked to the `libbuiltins.a` or not. If it's not only the core generation then only the program-dependent functions
            ///         will be generated
            /// @param `only_declarations` Whether to actually generate the functions or to only generate their declarations
            static void generate_dima_functions(      //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool is_core_generation = true, //
                const bool only_declarations = false  //
            );

            /// @function `generate_types`
            /// @brief Generates all the slot, block and head types of all data types of all files
            static void generate_types();

            /// @function `generate_init_heads_function`
            /// @brief Generates the `init_heads` function to initialize all dima heads at program startup
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `init_heads` function will be generated in
            /// @param `only_declarations` Whether to actually generate the `init_heads` function or to only generate it's declaration
            static void generate_init_heads_function( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations = true   //
            );

            /// @function `generate_get_block_capacity_function`
            /// @brief Generates the `get_block_capacity` function to initialize all dima heads at program startup
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `get_block_capacity` function will be generated in
            /// @param `only_declarations` Whether to actually generate the `get_block_capacity` function or to only generate it's
            /// declaration
            static void generate_get_block_capacity_function( //
                llvm::IRBuilder<> *builder,                   //
                llvm::Module *module,                         //
                const bool only_declarations = true           //
            );

            /// @function `generate_create_block_function`
            /// @brief Gnerates the `create_block` function to create a new block of a given size of the given type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `create_block` function will be generated in
            /// @param `only_declarations` Whether to actually generate the `create_block` function or to only generate it's
            /// declaration
            static void generate_create_block_function( //
                llvm::IRBuilder<> *builder,             //
                llvm::Module *module,                   //
                const bool only_declarations = true     //
            );

            /// @function `generate_allocate_in_block_function`
            /// @brief Gnerates the `allocate_in_block` function to create a new block of a given size of the given type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `allocate_in_block` function will be generated in
            /// @param `only_declarations` Whether to actually generate the `allocate_in_block` function or to only generate it's
            /// declaration
            static void generate_allocate_in_block_function( //
                llvm::IRBuilder<> *builder,                  //
                llvm::Module *module,                        //
                const bool only_declarations = true          //
            );

            /// @function `generate_allocate_function`
            /// @brief Gnerates the `allocate` function to create a new block of a given size of the given type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `allocate` function will be generated in
            /// @param `only_declarations` Whether to actually generate the `allocate` function or to only generate it's declaration
            static void generate_allocate_function( //
                llvm::IRBuilder<> *builder,         //
                llvm::Module *module,               //
                const bool only_declarations = true //
            );

            /// @function `generate_retain_function`
            /// @brief Gnerates the `retain` function to create a new block of a given size of the given type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `retain` function will be generated in
            /// @param `only_declarations` Whether to actually generate the `retain` function or to only generate it's declaration
            static void generate_retain_function(   //
                llvm::IRBuilder<> *builder,         //
                llvm::Module *module,               //
                const bool only_declarations = true //
            );

            /// @function `generate_release_function`
            /// @brief Gnerates the `release` function to create a new block of a given size of the given type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `release` function will be generated in
            /// @param `only_declarations` Whether to actually generate the `release` function or to only generate it's declaration
            static void generate_release_function(  //
                llvm::IRBuilder<> *builder,         //
                llvm::Module *module,               //
                const bool only_declarations = true //
            );
        };

        /// @class `Env`
        /// @brief The class which is responsible for generating everything related to environment variables
        /// @note This class cannot be initialized and all functions within this class are static
        class Env {
          public:
            // The constructor is deleted to make this class non-initializable
            Env() = delete;

            /// @var `env_functions`
            /// @brief Map containing references to all env functions
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_env_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> env_functions = {
                {"get_env", nullptr},
#ifdef __WIN32__
                {"setenv", nullptr},
#endif
                {"set_env", nullptr},
            };

            /// @function `generate_env_functions`
            /// @brief Generates the builtin env functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the env functions definitions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_env_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_get_env_function`
            /// @brief Helper function to generate the builtin 'get_env' function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'get_env' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_get_env_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

#ifdef __WIN32__
            /// @function `generate_setenv_function`
            /// @brief Helper function specifically for Windows because the 'setenv' function does not exist on Windows (with it's third
            /// flag to disable overwriting already existent environment variables)
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'get_env' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_setenv_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);
#endif

            /// @function `generate_set_env_function`
            /// @brief Helper function to generate the builtin 'set_env' function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'set_env' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_set_env_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);
        };

        /// @class `FileSystem`
        /// @brief The class which is responsible for generating everything related to filesystem
        /// @note This class cannot be initialized and all functions within this class are static
        class FileSystem {
          public:
            // The constructor is deleted to make this class non-initializable
            FileSystem() = delete;

            /// @var `fs_functions`
            /// @brief Map containing references to all filesystem functions
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_filesystem_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> fs_functions = {
                {"read_file", nullptr},
                {"read_lines", nullptr},
                {"file_exists", nullptr},
                {"write_file", nullptr},
                {"append_file", nullptr},
                {"is_file", nullptr},
            };

            /// @function `generate_filesystem_functions`
            /// @brief Generates the builtin filesystem functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the filesystem functions definitions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_filesystem_functions( //
                llvm::IRBuilder<> *builder,            //
                llvm::Module *module,                  //
                const bool only_declarations = true    //
            );

            /// @function `generate_read_file_function`
            /// @brief Function to generate the builtin 'read_file' function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'read_file' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_read_file_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_read_lines_function`
            /// @brief Function to generate the builtin 'read_lines' function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'read_lines' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_read_lines_function( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations = true   //
            );

            /// @function `generate_file_exists_function`
            /// @brief Function to generate the builtin `file_exists` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `file_exists` function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_file_exists_function( //
                llvm::IRBuilder<> *builder,            //
                llvm::Module *module,                  //
                const bool only_declarations = true    //
            );

            /// @function `generate_write_file_function`
            /// @brief Function to generate the builtin `write_file` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `write_file` function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_write_file_function( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations = true   //
            );

            /// @function `generate_append_file_function`
            /// @brief Function to generate the builtin `append_file` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `append_file` function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_append_file_function( //
                llvm::IRBuilder<> *builder,            //
                llvm::Module *module,                  //
                const bool only_declarations = true    //
            );

            /// @function `generate_is_file_function`
            /// @brief Function to generate the builtin `is_file` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `is_file` function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_is_file_function( //
                llvm::IRBuilder<> *builder,        //
                llvm::Module *module,              //
                const bool only_declarations       //
            );
        };

        /// @class `Math`
        /// @brief The class which is responsible for generating everything related to the `math` core module
        /// @note This class cannot be initialized and all functions within this class are static
        // The constructor is deleted to make this class non-initializable
        class Math {
          public:
            Math() = delete;

            /// @var `math_functions`
            /// @brief Map containing references to all math functions
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_math_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> math_functions = {
                {"sin_f32", nullptr},
                {"sin_f64", nullptr},
                {"cos_f32", nullptr},
                {"cos_f64", nullptr},
                {"sqrt_f32", nullptr},
                {"sqrt_f64", nullptr},
                {"abs_i32", nullptr},
                {"abs_i64", nullptr},
                {"abs_f32", nullptr},
                {"abs_f64", nullptr},
                {"min_u32", nullptr},
                {"min_i32", nullptr},
                {"min_f32", nullptr},
                {"min_u64", nullptr},
                {"min_i64", nullptr},
                {"min_f64", nullptr},
                {"max_u32", nullptr},
                {"max_i32", nullptr},
                {"max_f32", nullptr},
                {"max_u64", nullptr},
                {"max_i64", nullptr},
                {"max_f64", nullptr},
            };

            /// @function `generate_math_functions`
            /// @brief Generates the builtin math functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the math function definitions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_math_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_abs_int_function`
            /// @brief Generates the 'abs' function for the given integer type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'abs' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `type` The integer type for which to generate the 'abs' function
            /// @param `name` The name of the type for which to generate the 'abs' function for
            static void generate_abs_int_function( //
                llvm::IRBuilder<> *builder,        //
                llvm::Module *module,              //
                const bool only_declarations,      //
                llvm::IntegerType *type,           //
                const std::string &name            //
            );

            /// @function `generate_min_function`
            /// @brief Generates the 'min' function for the given type type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'min' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `type` The type for which to generate the 'min' function
            /// @param `name` The name of the type for which to generate the 'min' function for
            static void generate_min_function( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::Type *type,              //
                const std::string &name        //
            );

            /// @function `generate_fmin_function`
            /// @brief Generates the 'min' function for the given floating point type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'min' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `type` The type for which to generate the 'min' function
            /// @param `name` The name of the type for which to generate the 'min' function for
            static void generate_fmin_function( //
                llvm::IRBuilder<> *builder,     //
                llvm::Module *module,           //
                const bool only_declarations,   //
                llvm::Type *type,               //
                const std::string &name         //
            );

            /// @function `generate_max_function`
            /// @brief Generates the 'max' function for the given type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'max' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `type` The type for which to generate the 'max' function
            /// @param `name` The name of the type for which to generate the 'max' function for
            static void generate_max_function( //
                llvm::IRBuilder<> *builder,    //
                llvm::Module *module,          //
                const bool only_declarations,  //
                llvm::Type *type,              //
                const std::string &name        //
            );

            /// @function `generate_fmax_function`
            /// @brief Generates the 'max' function for the given floating point type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the 'max' function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `type` The type for which to generate the 'max' function
            /// @param `name` The name of the type for which to generate the 'max' function for
            static void generate_fmax_function( //
                llvm::IRBuilder<> *builder,     //
                llvm::Module *module,           //
                const bool only_declarations,   //
                llvm::Type *type,               //
                const std::string &name         //
            );
        };

        /// @class `Parse`
        /// @brief The class which is responsible for generating all functions related to the `Core.parse` module
        /// @note This class cannot be initialized and all functions within this class are static
        class Parse {
          public:
            // The constructor is deleted to make this class non-initializable
            Parse() = delete;

            /// @var `parse_functions`
            /// @brief Map containing references to all parse functions
            ///
            /// This map exists to track the references to the builtin parse functions. They are being created at the beginning of the
            /// program generation phase. Whenever a builtin parse function is being refernced this map is used to resolve it.
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the parse function
            /// - **Value** `llvm::Function *` - The reference to the genereated print function
            ///
            /// @attention The parse functions are nullpointers until the `generate_parse_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> parse_functions = {
                {"parse_u8", nullptr},
                {"parse_i32", nullptr},
                {"parse_u32", nullptr},
                {"parse_i64", nullptr},
                {"parse_u64", nullptr},
                {"parse_f32", nullptr},
                {"parse_f64", nullptr},
            };

            /// @function `generate_parse_functions`
            /// @brief Generates all the functions of the Core.parse module
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the parse functions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_parse_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_parse_int_function`
            /// @brief Helper function to generate the parse_iX functions for the specified bit width
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the parse function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `bit_width` The width of the parsed signed integer
            static void generate_parse_int_function( //
                llvm::IRBuilder<> *builder,          //
                llvm::Module *module,                //
                const bool only_declarations,        //
                const size_t bit_width               //
            );

            /// @function `generate_parse_uint_function`
            /// @brief Helper function to generate the parse_uX functions for the specified bit width
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the parse function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `bit_width` The width of the parsed unsigned integer
            static void generate_parse_uint_function( //
                llvm::IRBuilder<> *builder,           //
                llvm::Module *module,                 //
                const bool only_declarations,         //
                const size_t bit_width                //
            );

            /// @function `generate_parse_f32_function`
            /// @brief Helper function to generate the parse_f32 function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the parse function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_parse_f32_function( //
                llvm::IRBuilder<> *builder,          //
                llvm::Module *module,                //
                const bool only_declarations         //
            );

            /// @function `generate_parse_f64_function`
            /// @brief Helper function to generate the parse_f32 function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the parse function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_parse_f64_function( //
                llvm::IRBuilder<> *builder,          //
                llvm::Module *module,                //
                const bool only_declarations         //
            );
        }; // subclass Parse

        /// @class `Print`
        /// @brief The class which is responsible for generating everything related to print
        /// @note This class cannot be initialized and all functions within this class are static
        class Print {
          public:
            // The constructor is deleted to make this class non-initializable
            Print() = delete;

            /// @var `print_functions`
            /// @brief Map containing references to all print function varaints
            ///
            /// This map exists to track the references to the builtin print functions. They are being created at the beginning of the
            /// program generation phase. Whenever a builtin print function is being refernced this map is used to resolve it.
            ///
            /// @details
            /// - **Key** `std::string_view` - The type of the print function
            /// - **Value** `llvm::Function *` - The reference to the genereated print function
            ///
            /// @attention The print functions are nullpointers until the `generate_builtin_prints` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> print_functions = {
                {"i32", nullptr},
                {"i64", nullptr},
                {"u32", nullptr},
                {"u64", nullptr},
                {"f32", nullptr},
                {"f64", nullptr},
                {"flint", nullptr},
                {"u8", nullptr},
                {"str", nullptr},
                {"type.flint.str.lit", nullptr},
                {"bool", nullptr},
            };

            /// @function `generate_print_functions`
            /// @brief Generates the builtin 'print()' function and its overloaded versions to utilize C IO calls of the IO C stdlib
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the print functions definitions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_print_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_print_function`
            /// @brief Helper function to generate the builtin print function for the specified type
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the print function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `type` The type of variable this print function expects
            /// @param `format` The C format string for the specified type (%i or %d for example)
            static void generate_print_function( //
                llvm::IRBuilder<> *builder,      //
                llvm::Module *module,            //
                const bool only_declarations,    //
                const std::string &type,         //
                const std::string &format        //
            );

            /// @function `generate_print_str_lit_function`
            /// @brief Generates the builtin print_str_lit function which prints the value of a string literal (null-terminated)
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the print function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_print_str_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_print_str_var_function`
            /// @brief Generates the builtin print_str_var function which prints the value of a string variable
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the print function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_print_str_var_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_print_bool_function`
            /// @brief Generates the builtin print_bool function which prints 'true' or 'false' depending on the bool value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the print function definition will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_print_bool_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);
        }; // subclass Print

        /// @class `Read`
        /// @brief The class which is responsible for generating everything related to reading
        /// @note This class cannot be initialized and all functions within this class are static
        class Read {
          public:
            // The constructor is deleted to make this class non-initializable
            Read() = delete;

            /// @var `getline_function`
            /// @brief The builtin getline function to provide a platform-independant way of reading a line from stdio
            static inline llvm::Function *getline_function{nullptr};

            /// @var `read_functions`
            /// @brief Map containing references to all read function varaints
            ///
            /// This map exists to track the references to the builtin read functions. They are being created at the beginning of the
            /// program generation phase. Whenever a builtin read function is being refernced this map is used to resolve it.
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the read function
            /// - **Value** `llvm::Function *` - The reference to the genereated read function
            ///
            /// @attention The print functions are nullpointers until the `generate_read_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string, llvm::Function *> read_functions = {
                {"read_str", nullptr},
                {"read_i32", nullptr},
                {"read_i64", nullptr},
                {"read_u32", nullptr},
                {"read_u64", nullptr},
                {"read_f32", nullptr},
                {"read_f64", nullptr},
            };

            /// @function `generate_getline_function`
            /// @brief Generates the builtin hidden `getline` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `getline` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_getline_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_read_str_function`
            /// @brief Generates the builtin hidden `read_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `read_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_read_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_read_int_function`
            /// @brief Generates the builtin hidden `read_iX` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `read_iX` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `result_type_ptr` The type of the result type
            static void generate_read_int_function(          //
                llvm::IRBuilder<> *builder,                  //
                llvm::Module *module,                        //
                const bool only_declarations,                //
                const std::shared_ptr<Type> &result_type_ptr //
            );

            /// @function `generate_read_uint_function`
            /// @brief Generates the builtin hidden `read_uX` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `read_uX` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `result_type_ptr` The type of the result type
            static void generate_read_uint_function(         //
                llvm::IRBuilder<> *builder,                  //
                llvm::Module *module,                        //
                const bool only_declarations,                //
                const std::shared_ptr<Type> &result_type_ptr //
            );

            /// @function `generate_read_f32_function`
            /// @brief Generates the builtin hidden `read_f32` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `read_f32` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_read_f32_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_read_f64_function`
            /// @brief Generates the builtin hidden `read_f64` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `read_f64` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_read_f64_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_read_functions`
            /// @brief Generates all the builtin hidden read functions to read from stdin
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the read functions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_read_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);
        }; // subclass Read

        /// @class `String`
        /// @brief The class which is responsible for generating everything related to strings
        /// @note This class cannot be initialized and all functions within this class are static
        class String {
          public:
            // The constructor is deleted to make this class non-initializable
            String() = delete;

            /// @var `string_manip_functions`
            /// @brief Map containing references to all string manipulation functions, to make str handling easier
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_string_manip_functions` function is called
            static inline std::unordered_map<std::string_view, llvm::Function *> string_manip_functions = {
                {"access_str_at", nullptr},
                {"assign_str_at", nullptr},
                {"create_str", nullptr},
                {"init_str", nullptr},
                {"assign_str", nullptr},
                {"assign_lit", nullptr},
                {"append_str", nullptr},
                {"append_lit", nullptr},
                {"add_str_str", nullptr},
                {"add_str_lit", nullptr},
                {"add_lit_str", nullptr},
                {"get_str_slice", nullptr},
            };

            /// @function `generate_access_str_at_function`
            /// @brief Generates the builtin hidden `access_str_at` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `access_str_at` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_access_str_at_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_assign_str_at_function`
            /// @brief Generates the builtin hidden `assign_str_at` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `assign_str_at` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_assign_str_at_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_create_str_function`
            /// @brief Generates the builtin hidden `create_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `create_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_create_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_init_str_function`
            /// @brief Generates the builtin hidden `init_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `init_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_init_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_compare_str_function`
            /// @brief Generates the builtin `compare_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `compare_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_compare_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_assign_str_function`
            /// @brief Generates the builtin hidden `assign_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `assign_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_assign_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_assign_lit_function`
            /// @brief Generates the builtin hidden `assign_lit` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `assign_lit` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_assign_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_append_str_function`
            /// @brief Generates the builtin hidden `append_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `append_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_append_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_append_lit_function`
            /// @brief Generates the builtin hidden `append_lit` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `append_lit` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_append_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_add_str_str_functiion`
            /// @brief Generates the builtin hidden `add_str_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `add_str_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_add_str_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_add_str_lit_function`
            /// @brief Generates the builtin hidden `add_str_lit` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `add_str_lit` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_add_str_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_add_lit_str_function`
            /// @brief Generates the builtin hidden `add_lit_str` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the `add_lit_str` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_add_lit_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_get_str_slice_function`
            /// @brief Gneerates the builtin hidden `get_str_slice` function
            ///
            /// @brief `builder` The LLVM IRBuilder
            /// @brief `module` The LLVM Module the `get_str_slice` function will be generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_get_str_slice_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_string_manip_functions`
            /// @brief Generates all the builtin hidden string manipulation functions
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the string manipulation functions will be generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_string_manip_functions( //
                llvm::IRBuilder<> *builder,              //
                llvm::Module *module,                    //
                const bool only_declarations = true      //
            );

            /// @function `generate_string_declaration`
            /// @brief Generates the declaration of a string variable
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `rhs` The rhs value of the declaration
            /// @param `rhs_expr` The rhs expression, if there exists any (to check if its a literal expression)
            /// @return `llvm::Value *` The resulting declaration value that will be saved on the actual variable
            static llvm::Value *generate_string_declaration(   //
                llvm::IRBuilder<> &builder,                    //
                llvm::Value *rhs,                              //
                std::optional<const ExpressionNode *> rhs_expr //
            );

            /// @function `generate_string_assignment`
            /// @brief Generates an assignment of a string variable
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `lhs` The pointer to the allocation instruction of the variable to assign to
            /// @param `expression_node` The expression to assign
            /// @param `expression` The generated value of the expression of the rhs
            static void generate_string_assignment(    //
                llvm::IRBuilder<> &builder,            //
                llvm::Value *const lhs,                //
                const ExpressionNode *expression_node, //
                llvm::Value *expression                //
            );

            /// @function `generate_string_addition`
            /// @brief Generates the addition instruction of two strings and returns the result of the addition
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `scope` The scope the string addition is placed in
            /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
            /// @param `garbage` A list of all accumulated temporary variables that need cleanup
            /// @param `expr_depth` The depth of expressions (starts at 0, increases by 1 by every layer)
            /// @param `lhs` The lhs value from llvm
            /// @param `lhs_expr` The lhs expression, to check if it is / was a literal
            /// @param `rhs` The rhs value from llvm
            /// @param `rhs_expr` The rhs expression, to check if it is / was a literal
            /// @param `is_append` Whether to append the rhs to the lhs
            /// @return `std::optional<llvm::Value *>` The result of the string addition, nullopt if the addition failed
            static std::optional<llvm::Value *> generate_string_addition(                                                     //
                llvm::IRBuilder<> &builder,                                                                                   //
                const std::shared_ptr<Scope> scope,                                                                           //
                const std::unordered_map<std::string, llvm::Value *const> &allocations,                                       //
                std::unordered_map<unsigned int, std::vector<std::pair<std::shared_ptr<Type>, llvm::Value *const>>> &garbage, //
                const unsigned int expr_depth,                                                                                //
                llvm::Value *lhs,                                                                                             //
                const ExpressionNode *lhs_expr,                                                                               //
                llvm::Value *rhs,                                                                                             //
                const ExpressionNode *rhs_expr,                                                                               //
                const bool is_append                                                                                          //
            );
        }; // subclass String

        /// @class `System`
        /// @brief The class which is responsible for generating everything related to system
        /// @note This class cannot be initialized and all functions within this class are static
        class System {
          public:
            // The constructor is deleted to make this class non-initializable
            System() = delete;

            /// @var `system_functions`
            /// @brief Map containing references to all system functions, to make calling them easier
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_system_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> system_functions = {
                {"system_command", nullptr},
                {"get_cwd", nullptr},
                {"get_path", nullptr},
            };

            /// @function `generate_system_functions`
            /// @brief Function to generate all functions from the system Core module
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_system_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_system_command_function`
            /// @brief Function to generate the `system_command` system function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_system_command_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_get_cwd_function`
            /// @brief Function to generate the `get_cwd` system function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_get_cwd_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_get_path_function`
            /// @brief Function to generate the `get_path` system function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_get_path_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);
        };

        /// @class `Time`
        /// @brief The class which is responsible for generating everything related to time
        /// @note This class cannot be initialized and all functions within this class are static
        class Time {
          public:
            // The constructor is deleted to make this class non-initializable
            Time() = delete;

            /// @var `time_functions`
            /// @brief Map containing references to all time functions, to make calling them easier
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_time_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string_view, llvm::Function *> time_functions = {
                {"now", nullptr},
            };

            /// @var `time_data_types`
            /// @brief Map containing references to all time data types, to make referencing them easier
            ///
            /// @details
            /// - **Key** `std::string` - The name of the data type
            /// - **Value** `llvm::StructType *` - The reference to the generated type
            ///
            /// @attention This map will be empty until the `generate_types` function is called
            static inline std::unordered_map<std::string, llvm::StructType *> time_data_types;

            /// @var `time_platform_functions`
            /// @brief Maps the names of platform-specific functions needed inside the module to the function declaration
            ///
            /// @details
            /// - **Key** `std::string` - The name of the platform-specific function
            /// - **Value** `llvm::Function *` - The reference to the function
            ///
            /// @attention This map will be empty until the `generate_platform_functions` function is called
            static inline std::unordered_map<std::string, llvm::Function *> time_platform_functions;

            /// @function `generate_time_functions`
            /// @brief Function to generate all functions from the time Core module
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_time_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_types`
            /// @brief Generates all the types this module provides
            ///
            /// @param `module` The module in which to generate this module's types in
            static void generate_types(llvm::Module *module);

            /// @function `generate_platform_functions`
            /// @brief Generates all platform-specific functions needed by this module
            ///
            /// @param `module` The module in which to generate the platform-specific function declarations
            static void generate_platform_functions(llvm::Module *module);

            /// @function `generate_time_init_function`
            /// @brief Function to generate the `time_init` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declaration for it
            static void generate_time_init_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_now_function`
            /// @brief Function to generate the `now` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declaration for it
            static void generate_now_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_duration_function`
            /// @brief Function to generate the `duration` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declaration for it
            static void generate_duration_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_sleep_duration_function`
            /// @brief Function to generate the `sleep_duration` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declaration for it
            static void generate_sleep_duration_function( //
                llvm::IRBuilder<> *builder,               //
                llvm::Module *module,                     //
                const bool only_declarations = true       //
            );

            /// @function `generate_sleep_time_function`
            /// @brief Function to generate the `sleep_time` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declaration for it
            static void generate_sleep_time_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_as_unit_function`
            /// @brief Funtion to generate the `as_unit` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declaration for it
            static void generate_as_unit_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_from_function`
            /// @brief Funtion to generate the `from` function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the functions are generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declaration for it
            static void generate_from_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);
        };

        /// @class `TypeCast`
        /// @brief The class which is responsilbe for everything type-casting related
        /// @note This class cannot be initialized and all functions within this class are static
        class TypeCast {
          public:
            // The constructor is deleted to make this class non-initializable
            TypeCast() = delete;

            /// @var `typecast_functions`
            /// @brief Map containing references to all typecast functions, to make type casting easier
            ///
            /// @details
            /// - **Key** `std::string_view` - The name of the function
            /// - **Value** `llvm::Function *` - The reference to the genereated function
            ///
            /// @attention The functions are nullpointers until the `generate_helper_functions` function is called
            /// @attention The map is not being cleared after the program module has been generated
            static inline std::unordered_map<std::string, llvm::Function *> typecast_functions = {
                {"count_digits", nullptr},
                {"u8_to_str", nullptr},
                {"i32_to_str", nullptr},
                {"u32_to_str", nullptr},
                {"i64_to_str", nullptr},
                {"u64_to_str", nullptr},
                {"f32_to_str", nullptr},
                {"f64_to_str", nullptr},
                {"bool_to_str", nullptr},
                {"bool8_to_str", nullptr},
                {"u8x2_to_str", nullptr},
                {"u8x3_to_str", nullptr},
                {"u8x4_to_str", nullptr},
                {"u8x8_to_str", nullptr},
                {"i32x2_to_str", nullptr},
                {"i32x3_to_str", nullptr},
                {"i32x4_to_str", nullptr},
                {"i32x8_to_str", nullptr},
                {"i64x2_to_str", nullptr},
                {"i64x3_to_str", nullptr},
                {"i64x4_to_str", nullptr},
                {"f32x2_to_str", nullptr},
                {"f32x3_to_str", nullptr},
                {"f32x4_to_str", nullptr},
                {"f32x8_to_str", nullptr},
                {"f64x2_to_str", nullptr},
                {"f64x3_to_str", nullptr},
                {"f64x4_to_str", nullptr},
            };

            /// @function `generate_typecast_functions`
            /// @brief Function to generate all helper functions used for the type-casting. Currently these helper functions are only used
            /// when casting strings
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the function is generated in
            /// @param `only_declarations` Whether to actually generate the functions or to only generate the declarations for them
            static void generate_typecast_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations = true);

            /// @function `generate_count_digits_function`
            /// @brief Function to generate the `count_digits` helper function, used for to-string casting
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the function is generated in
            static void generate_count_digits_function(llvm::IRBuilder<> *builder, llvm::Module *module);

            /// @function `generate_bool_to_str`
            /// @brief Function to generate the `bool_to_str` typecast function
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_bool_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /// @function `generate_multitype_to_str`
            /// @brief Generates the `typeXwidth_to_str` function which is used to convert multitype values to str values
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            /// @param `type_str` The string of the base type, for example `i32` or `f32`
            /// @param `width` The width of the multi-type, for example `2` or `4`
            static void generate_multitype_to_str( //
                llvm::IRBuilder<> *builder,        //
                llvm::Module *module,              //
                const bool only_declarations,      //
                const std::string &type_str,       //
                const size_t width                 //
            );

            /**************************************************************************************************************************************
             * @region `MultiTypes`
             *************************************************************************************************************************************/

            /// @function `generate_bool8_to_str_function`
            /// @brief Generates the function with which conversions of the bool8 to string variables take place
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i32 value to convert
            /// @return `llvm::Value *` The converted u8 value
            static void generate_bool8_to_str_function(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /**************************************************************************************************************************************
             * @region `U8`
             *************************************************************************************************************************************/

            /// @function `generate_u8_to_str`
            /// @brief Generates the `u8_to_str` function which is used to convert u8 values to str values (the u8 number, not the u8 char)
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_u8_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /**************************************************************************************************************************************
             * @region `I32`
             *************************************************************************************************************************************/

            /// @function `i32_to_u8`
            /// @brief Converts a i32 value to an u8 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i32 value to convert
            /// @return `llvm::Value *` The converted u8 value
            static llvm::Value *i32_to_u8(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i32_to_u32`
            /// @brief Converts an i32 value to a u32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i32 value to convert
            /// @return `llvm::Value *` The converted u32 value
            static llvm::Value *i32_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i32_to_i64`
            /// @brief Converts an i32 value to an i64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i32 value to convert
            /// @return `llvm::Value *` The converted i64 value
            static llvm::Value *i32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i32_to_u64`
            /// @brief Converts an i32 value to a u64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i32 value to convert
            /// @return `llvm::Value *` The converted u64 value
            static llvm::Value *i32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i32_to_f32`
            /// @brief Converts a i32 value to a f32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The int value to convert
            /// @return `llvm::Value *` The converted f32 value
            static llvm::Value *i32_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i32_to_f64`
            /// @brief Converts an i32 value to a f64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i32 value to convert
            /// @return `llvm::Value *` The converted f64 value
            static llvm::Value *i32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `generate_i32_to_str`
            /// @brief Generates the `i32_to_str` function which is used to convert i32 values to str values
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_i32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /**************************************************************************************************************************************
             * @region `U32`
             *************************************************************************************************************************************/

            /// @function `u32_to_u8`
            /// @brief Converts a u32 value to an u8 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u32 value to convert
            /// @return `llvm::Value *` The converted u8 value
            static llvm::Value *u32_to_u8(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u32_to_i32`
            /// @brief Converts a u32 value to an i32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u32 value to convert
            /// @return `llvm::Value *` The converted i32 value
            static llvm::Value *u32_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u32_to_i64`
            /// @brief Converts a u32 value to an i64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u32 value to convert
            /// @return `llvm::Value *` The converted i64 value
            static llvm::Value *u32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u32_to_u64`
            /// @brief Converts a u32 value to a u64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u32 value to convert
            /// @return `llvm::Value *` The converted u64 value
            static llvm::Value *u32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u32_to_f32`
            /// @brief Converts a u32 value to a f32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u32 value to convert
            /// @return `llvm::Value *` The converted f32 value
            static llvm::Value *u32_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u32_to_f64`
            /// @brief Converts a u32 value to a f64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u32 value to convert
            /// @return `llvm::Value *` The converted f64 value
            static llvm::Value *u32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `generate_u32_to_str`
            /// @brief Generates the `u32_to_str` function which is used to convert u32 values to str values
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_u32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /**************************************************************************************************************************************
             * @region `I64`
             *************************************************************************************************************************************/

            /// @function `i64_to_u8`
            /// @brief Converts an i64 value to an u8 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i64 value to convert
            /// @return `llvm::Value *` The converted u8 value
            static llvm::Value *i64_to_u8(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i64_to_i32`
            /// @brief Converts an i64 value to an i32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i64 value to convert
            /// @return `llvm::Value *` The converted i32 value
            static llvm::Value *i64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i64_to_u32`
            /// @brief Converts an i64 value to a u32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i64 value to convert
            /// @return `llvm::Value *` The converted u32 value
            static llvm::Value *i64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i64_to_u64`
            /// @brief Converts an i64 value to a u64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i64 value to convert
            /// @return `llvm::Value *` The converted u64 value
            static llvm::Value *i64_to_u64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i64_to_f32`
            /// @brief Converts an i64 value to a f32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i64 value to convert
            /// @return `llvm::Value *` The converted f32 value
            static llvm::Value *i64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `i64_to_f64`
            /// @brief Converts an i64 value to a f64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The i64 value to convert
            /// @return `llvm::Value *` The converted f64 value
            static llvm::Value *i64_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `generate_i64_to_str`
            /// @brief Generates the `i64_to_str` function which is used to convert i64 values to str values
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_i64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /**************************************************************************************************************************************
             * @region `U64`
             *************************************************************************************************************************************/

            /// @function `u64_to_u8`
            /// @brief Converts a u64 value to an u8 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u64 value to convert
            /// @return `llvm::Value *` The converted u8 value
            static llvm::Value *u64_to_u8(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u64_to_i32`
            /// @brief Converts a u64 value to an i32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u64 value to convert
            /// @return `llvm::Value *` The converted i32 value
            static llvm::Value *u64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u64_to_u32`
            /// @brief Converts a u64 value to a u32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u64 value to convert
            /// @return `llvm::Value *` The converted u32 value
            static llvm::Value *u64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u64_to_i64`
            /// @brief Converts a u64 value to an i64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u64 value to convert
            /// @return `llvm::Value *` The converted i64 value
            static llvm::Value *u64_to_i64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u64_to_f32`
            /// @brief Converts a u64 value to a f32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u64 value to convert
            /// @return `llvm::Value *` The converted f32 value
            static llvm::Value *u64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `u64_to_f64`
            /// @brief Converts a u64 value to a f64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `int_value` The u64 value to convert
            /// @return `llvm::Value *` The converted f64 value
            static llvm::Value *u64_to_f64(llvm::IRBuilder<> &builder, llvm::Value *int_value);

            /// @function `generate_u64_to_str`
            /// @brief Generates the `u64_to_str` function which is used to convert u64 values to str values
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_u64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /**************************************************************************************************************************************
             * @region `F32`
             *************************************************************************************************************************************/

            /// @function `f32_to_i32`
            /// @brief Converts a f32 value to an i32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `float_value` The f32 value to convert
            /// @return `llvm::Value *` The converted i32 value
            static llvm::Value *f32_to_i32(llvm::IRBuilder<> &builder, llvm::Value *float_value);

            /// @function `f32_to_u32`
            /// @brief Converts a f32 value to a u32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `float_value` The f32 value to convert
            /// @return `llvm::Value *` The converted u32 value
            static llvm::Value *f32_to_u32(llvm::IRBuilder<> &builder, llvm::Value *float_value);

            /// @function `f32_to_i64`
            /// @brief Converts a f32 value to an i64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `float_value` The f32 value to convert
            /// @return `llvm::Value *` The converted i64 value
            static llvm::Value *f32_to_i64(llvm::IRBuilder<> &builder, llvm::Value *float_value);

            /// @function `f32_to_u64`
            /// @brief Converts a f32 value to a u64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `float_value` The f32 value to convert
            /// @return `llvm::Value *` The converted u64 value
            static llvm::Value *f32_to_u64(llvm::IRBuilder<> &builder, llvm::Value *float_value);

            /// @function `f32_to_f64`
            /// @brief Converts a f32 value to a f64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `float_value` The f32 value to convert
            /// @return `llvm::Value *` The converted f64 value
            static llvm::Value *f32_to_f64(llvm::IRBuilder<> &builder, llvm::Value *float_value);

            /// @function `generate_f32_to_str`
            /// @brief Generates the `f32_to_str` function which is used to convert f32 values to str values
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_f32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);

            /**************************************************************************************************************************************
             * @region `F64`
             *************************************************************************************************************************************/

            /// @function `f64_to_i32`
            /// @brief Converts a f64 value to an i32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `double_value` The f64 value to convert
            /// @return `llvm::Value *` The converted i32 value
            static llvm::Value *f64_to_i32(llvm::IRBuilder<> &builder, llvm::Value *double_value);

            /// @function `f64_to_u32`
            /// @brief Converts a f64 value to a u32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `double_value` The f64 value to convert
            /// @return `llvm::Value *` The converted u32 value
            static llvm::Value *f64_to_u32(llvm::IRBuilder<> &builder, llvm::Value *double_value);

            /// @function `f64_to_i64`
            /// @brief Converts a f64 value to an i64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `double_value` The f64 value to convert
            /// @return `llvm::Value *` The converted i64 value
            static llvm::Value *f64_to_i64(llvm::IRBuilder<> &builder, llvm::Value *double_value);

            /// @function `f64_to_u64`
            /// @brief Converts a f64 value to a u64 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `double_value` The f64 value to convert
            /// @return `llvm::Value *` The converted u64 value
            static llvm::Value *f64_to_u64(llvm::IRBuilder<> &builder, llvm::Value *double_value);

            /// @function `f64_to_f32`
            /// @brief Converts a f64 value to a f32 value
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `double_value` The f64 value to convert
            /// @return `llvm::Value *` The converted f32 value
            static llvm::Value *f64_to_f32(llvm::IRBuilder<> &builder, llvm::Value *double_value);

            /// @function `generate_f64_to_str`
            /// @brief Generates the `f64_to_str` function which is used to convert f64 values to str values
            ///
            /// @param `builder` The LLVM IRBuilder
            /// @param `module` The LLVM Module in which the function is generated in
            /// @param `only_declarations` Whether to actually generate the function or to only generate the declaration for it
            static void generate_f64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations);
        }; // subclass TypeCast
    }; // subclass Module
};
