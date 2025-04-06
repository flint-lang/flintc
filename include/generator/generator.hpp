#pragma once

#include "lexer/builtins.hpp"
#include "parser/ast/call_node_base.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/data_access_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/expressions/grouped_data_access_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/unary_op_expression.hpp"
#include "parser/ast/expressions/variable_node.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/data_field_assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/group_assignment_node.hpp"
#include "parser/ast/statements/group_declaration_node.hpp"
#include "parser/ast/statements/grouped_data_field_assignment_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/return_node.hpp"
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

    /// @function `generate_program_ir`
    /// @brief Generates the llvm IR code for a complete program
    ///
    /// @param `program_name` The name the program (the module) will have
    /// @param `context` The context used for generation
    /// @param `dep_graph` The root DepNode of the dependency graph
    /// @param `is_test` Whether the program is built in test mode
    /// @return `std::unique_ptr<llvm::Module>` A pointer containing the generated program module
    ///
    /// @attention Do not forget to call `Resolver::clear()` before the module returned from this function goes out of scope! You would get
    /// a segmentation fault if you were to forget that!
    static std::unique_ptr<llvm::Module> generate_program_ir( //
        const std::string &program_name,                      //
        llvm::LLVMContext &context,                           //
        const std::shared_ptr<DepNode> &dep_graph,            //
        const bool is_test                                    //
    );

    /// @function `generate_file_ir`
    /// @brief Generates the llvm IR code for a single file and saves it into a llvm module
    ///
    /// @param `context` The LLVM context
    /// @param `dep_node` The dependency graph node of the file (used for circular dependencies)
    /// @param `file` The file node to generate
    /// @param `is_test` Whether the program is built in test mode
    /// @return `std::unique_ptr<llvm::Module>` A pointer containing the generated file module
    static std::unique_ptr<llvm::Module> generate_file_ir( //
        llvm::LLVMContext &context,                        //
        const std::shared_ptr<DepNode> &dep_node,          //
        const FileNode &file,                              //
        const bool is_test                                 //
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
    /// @alias `group_mapping`
    /// @brief This type represents all values of the group. Everything is considered a group, if one single value is returned, its a group
    /// of size 1. This makes direct-group mappings much easier
    using group_mapping = std::optional<std::vector<llvm::Value *>>;

    /// @var `builtins`
    /// @brief Map containing references to all builtin functions
    ///
    /// This map exists to track the references to all the builtin functions
    ///
    /// @details
    /// - **Key** `BuiltinFunctions` - The enum value of the builtin function to reference
    /// - **Value** `llvm::Function *` - The reference to the generated builtin function
    ///
    /// @attention The builtin functions are nullpointers until the explcitely are generated by their respecitve generation functions
    /// @attention Currently, no builtin functions are being generated yet, so there does not exist any reason to use this map yet
    /// @todo Remove the PRINT builtin function, as it has its own builtin functions, overloaded for every type (`print_functions`)
    static inline std::unordered_map<BuiltinFunction, llvm::Function *> builtins = {
        {BuiltinFunction::PRINT, nullptr},
        {BuiltinFunction::PRINT_ERR, nullptr},
        // {BuiltinFunction::ASSERT, nullptr},
        {BuiltinFunction::ASSERT_ARG, nullptr},
        {BuiltinFunction::RUN_ON_ALL, nullptr},
        {BuiltinFunction::MAP_ON_ALL, nullptr},
        {BuiltinFunction::FILTER_ON_ALL, nullptr},
        {BuiltinFunction::REDUCE_ON_ALL, nullptr},
        {BuiltinFunction::REDUCE_ON_PAIRS, nullptr},
        {BuiltinFunction::PARTITION_ON_ALL, nullptr},
        {BuiltinFunction::SPLIT_ON_ALL, nullptr},
    };

    /// @var `c_functions`
    /// @brief Map containing references to all external c functions
    ///
    /// @attention The external c functions are nullpointers until explicitely generated by the `Builtin::generate_c_functions` function
    static inline std::unordered_map<CFunction, llvm::Function *> c_functions = {
        {CFunction::MALLOC, nullptr},
        {CFunction::FREE, nullptr},
        {CFunction::MEMCPY, nullptr},
        {CFunction::REALLOC, nullptr},
        {CFunction::SNPRINTF, nullptr},
    };

    /// @var `print_functions`
    /// @brief Map containing references to all print function varaints
    ///
    /// This map exists to track the references to the builtin print functions. They are being created at the beginning of the program
    /// generation phase. Whenever a builtin print function is being refernced this map is used to resolve it.
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
        {"char", nullptr},
        {"str", nullptr},
        {"str_var", nullptr},
        {"bool", nullptr},
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
    static std::unordered_map<std::string, llvm::StructType *> type_map;

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
    static std::unordered_map<std::string, std::vector<llvm::CallInst *>> unresolved_functions;

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
    /// - - **Key** `std::string` - Name of the called function
    /// - - **Value** `std::vector<llvm::CallInst *>` - The list of all calls targeting this function
    ///
    /// @note This map is static and persists throughout the module's lifecyle
    static std::unordered_map<std::string, std::unordered_map<std::string, std::vector<llvm::CallInst *>>> file_unresolved_functions;

    /// @var `function_mangle_ids`
    /// @brief Stores all mangle IDs for all functions in the module currently being generated
    ///
    /// This data structure is used during the module generation process to track the mangle IDs of all functions within the module
    /// currently being generated. Because every function inside the generated module can appear in any order, all function definitions have
    /// to be forward-declared within the generated module, thus _every_ function inside the module does have a mangle ID
    ///
    /// @details
    /// - **Key** `std::string` - Name of the function
    /// - **Value** `unsigned int` - Manlge ID of the function
    ///
    /// @note This map is being cleared at the end of every file module generation pass
    static std::unordered_map<std::string, unsigned int> function_mangle_ids;

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
    static std::unordered_map<std::string, std::unordered_map<std::string, unsigned int>> file_function_mangle_ids;

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
    static std::vector<std::string> function_names;

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
    static std::unordered_map<std::string, std::vector<std::string>> file_function_names;

    /// @var `main_call_array`
    /// @brief Holds a reference to the call of the user-defined main function from within the builtin main function
    ///
    /// This array of size 1 exists because a static variable is needed to reference the call instruction. If the pointer to the call
    /// instruction itself would have been made static, very weird things start to happen as the pointer seemingly "changes" its reference
    /// its pointing to. This is the reason the pointer, the value of this static construct, is being wrapped in an array of size 1. It
    /// introduces minimal overhead and only acts as a static "container" in this case, to store the pointer to the call instruction calling
    /// the user-defined main function from within the builtin main function.
    static std::array<llvm::CallInst *, 1> main_call_array;

    /// @var `main_module`
    /// @brief Holds a static reference to the main module
    ///
    /// This array of size 1 exists because of the same reasons as why 'main_call_array' exists. It holds the reference to the main
    /// module, which means that every function can reference the main module if needed without the main module needed to explicitely be
    /// passed around through many functions which dont use it annyways. It is actually much more efficient to have this reference inside an
    /// static array than to pass it around unnecessarily.
    static std::array<llvm::Module *, 1> main_module;

    /// @var `tests`
    /// @brief A list of all generated test functions across all files together with the file names, and the name of the test and the exact
    /// name of the function, because a `llvm::Function *` type would become invalid after combining multiple llvm modules
    static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> tests;

    /// @var `data_nodes`
    /// @brief A list of all available data nodes across all parsed files. If a file has access to a given set of data is determined in the
    /// parsing steps, so we can assume that all usages of data types are valid when generating the IR code.
    static std::unordered_map<std::string, const DataNode *const> data_nodes;

    /// @function `get_data_nodes`
    /// @brief This function collects all data nodes from the parser and puts them into the `data_nodes` map in the generator
    static void get_data_nodes();

    /// @class `IR`
    /// @brief The class which is responsible for the utility functions for the IR generation
    /// @note This class cannot be initialized and all functions within this class are static
    class IR {
      public:
        // The constructor is deleted to make this class non-initializable
        IR() = delete;

        /// @function `add_and_or_get_type`
        /// @brief Checks if a given return type of a given types list already exists. If it exists, it returns a reference to it, if it
        /// does not exist it creates it and then returns a reference to the created StructType
        ///
        /// @param `context` The LLVM context
        /// @param `types` The list of types to get or set the struct type from
        /// @param `is_return_type` Whether the StructType is a return type (if it is, it has one return value more, the error return value)
        /// @return `llvm::StructType *` The reference to the StructType, representing the return type of the types map
        static llvm::StructType *add_and_or_get_type( //
            llvm::LLVMContext *context,               //
            const std::vector<std::string> &types,    //
            const bool is_return_type = true          //
        );

        /// @function `generate_forward_declarations`
        /// @brief Generates the forward-declarations of all constructs in the given FileNode, except the 'use' constructs to make another
        /// module able to use them. This function is also essential for Flint's support of circular dependency resolution
        ///
        /// @param `module` The module the forward declarations are declared inside
        /// @param `file_node` The FileNode whose construct definitions will be forward-declared in the given module
        static void generate_forward_declarations(llvm::Module *module, const FileNode &file_node);

        /// @function `get_type_from_str`
        /// @brief Returns the llvm Type from a given string value
        ///
        /// @param `context` The LLVM context
        /// @param `str` The string representation of the type which is matched to get the correct type
        /// @return `std::pair<llvm::Type *, bool>` A pair containing a pointer to the correct llvm Type from the given string and a boolean
        /// value to determine if the given data type is a complex type (data, entity etc)
        ///
        /// @throws ErrGenerating when the type could not be parsed from the string representation to a real type
        static std::pair<llvm::Type *, bool> get_type_from_str(llvm::LLVMContext &context, const std::string &str);

        /// @function `get_default_value_of_type`
        /// @brief Returns the default value associated with a given Type
        ///
        /// @param `type` The Type from which the default value has to be returned
        /// @return `llvm::Value *` The default value of the given Type
        ///
        /// @throws ErrGenerating when the given Type has no default value available for it
        /// @todo Add more types to the function, currently it only works with int, flint and pointer types
        static llvm::Value *get_default_value_of_type(llvm::Type *type);

        /// @function `generate_const_string`
        /// @brief Generates a compile-time constant string that will be embedded into the binary itself, this string is not mutable
        ///
        /// @param `builder` The IRBuilder
        /// @param `parent` The function the constant string will be contained in
        /// @param `str` The value of the string
        /// @return `llvm::Value *` The generated static string value
        static llvm::Value *generate_const_string(llvm::IRBuilder<> &builder, llvm::Function *parent, const std::string &str);

        /// @function `generate_pow_instruction`
        /// @brief Generates a pow instruction from the given llvm values
        ///
        /// @param `builder` The IR builder
        /// @param `parent` The function the instruction will be generated in
        /// @param `lhs` The lhs value of the pow instruction (the pow base)
        /// @param `rhs` The rhs value of the pow instruction (the pow exponent)
        /// @return `llvm::Value *` A pointer to the generated result value of the pow instruction
        ///
        /// @attention This function currently just returns a nullptr, dont use it yet!
        /// @todo Actually implement this function
        static llvm::Value *generate_pow_instruction( //
            llvm::IRBuilder<> &builder,               //
            llvm::Function *parent,                   //
            llvm::Value *lhs,                         //
            llvm::Value *rhs                          //
        );

        /// @function `generate_debug_print`
        /// @brief Generates a small call to print which prints the given message using printf
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the debug print will be generated in
        /// @param `message` The message to print
        static void generate_debug_print( //
            llvm::IRBuilder<> *builder,   //
            llvm::Function *parent,       //
            const std::string &message    //
        );
    }; // subclass IR

    /// @class `Builtin`
    /// @brief The class which is responsible for generating all builtin functions
    /// @note This class cannot be initialized and all functions within this class are static
    class Builtin {
      public:
        // The constructor is deleted to make this class non-initializable
        Builtin() = delete;

        /// @function `generate_exit`
        /// @brief Generates a call to the exit function from C
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the exit call will be generated in
        /// @param `exit_value` The value which will be passed to the exit function
        ///
        /// @attention This function does not create a basic block, make sure to set the basic block in which to generate the exit call
        /// before
        /// @attention This function generates an unreachable statement after the exit, so any code after this call will not be generated
        static void generate_exit(llvm::IRBuilder<> *builder, llvm::Module *module, llvm::Value *exit_value);

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

        /// @function `generate_builtin_prints`
        /// @brief Generates the builtin 'print()' function and its overloaded versions to utilize C IO calls of the IO C stdlib
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the print functions definitions will be generated in
        static void generate_builtin_prints(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_builtin_print`
        /// @brief Helper function to generate the builtin print function for the specified type
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the print function definition will be generated in
        /// @param `type` The type of variable this print function expects
        /// @param `format` The C format string for the specified type (%i or %d for example)
        static void generate_builtin_print( //
            llvm::IRBuilder<> *builder,     //
            llvm::Module *module,           //
            const std::string &type,        //
            const std::string &format       //
        );

        /// @function `generate_builtin_print_str_var`
        /// @brief Generates the builtin print_str_var function which prints the value of a string variable
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the print function definition will be generated in
        static void generate_builtin_print_str_var(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_builtin_print_bool`
        /// @brief Generates the builtin print_bool function which prints 'true' or 'false' depending on the bool value
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the print function definition will be generated in
        static void generate_builtin_print_bool(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_builtin_test`
        /// @brief Generates the entry point of the program when compiled with the `--test` flag enabled
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the test entry point will be generated in
        static void generate_builtin_test(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_test_function`
        /// @brief Generates the test function
        ///
        /// @param `module` The LLVM Module in which the test will be generated in
        /// @param `test_node` The TestNode to generate
        /// @return `llvm::Function *` The generated test function
        static llvm::Function *generate_test_function(llvm::Module *module, const TestNode *test_node);
    }; // subclass Builtin

    /// @class `Arithmetic`
    /// @brief The class which is responsible for everything arithmetic-related
    /// @note This class cannot be initialized and all functions within this class are static
    class Arithmetic {
      public:
        // The constructor is deleted to make this class non-initializable
        Arithmetic() = delete;

        /// @function `int_safe_add`
        /// @brief Creates a safe addition of two signed integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe addition
        /// @param `rhs` The rhs value of the safe addition
        /// @return `llvm::Value *` The result of the safe signed addition
        static llvm::Value *int_safe_add(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);

        /// @function `int_safe_sub`
        /// @brief Creates a safe subtraction of two signed integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe subtraction
        /// @param `rhs` The rhs value of the safe subtraction
        /// @return `llvm::Value *` The result of the safe signed subtraction
        static llvm::Value *int_safe_sub(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);

        /// @function `int_safe_mul`
        /// @brief Creates a safe multiplication of two signed integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe multiplication
        /// @param `rhs` The rhs value of the safe multiplication
        /// @return `llvm::Value *` The result of the safe signed multiplication
        static llvm::Value *int_safe_mul(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);

        /// @function `int_safe_div`
        /// @brief Creates a safe division of two signed integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe division
        /// @param `rhs` The rhs value of the safe division
        /// @return `llvm::Value *` The result of the safe signed division
        static llvm::Value *int_safe_div(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);

        /// @function `uint_safe_add`
        /// @brief Creates a safe addition of two unsigned integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe addition
        /// @param `rhs` The rhs value of the safe addition
        /// @return `llvm::Value *` The result of the safe unsigned addition
        static llvm::Value *uint_safe_add(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);

        /// @function `int_safe_sub`
        /// @brief Creates a safe subtraction of two unsigned integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe subtraction
        /// @param `rhs` The rhs value of the safe subtraction
        /// @return `llvm::Value *` The result of the safe unsigned subtraction
        static llvm::Value *uint_safe_sub(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);

        /// @function `int_safe_mul`
        /// @brief Creates a safe multiplication of two unsigned integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe multiplication
        /// @param `rhs` The rhs value of the safe multiplication
        /// @return `llvm::Value *` The result of the safe unsigned multiplication
        static llvm::Value *uint_safe_mul(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);

        /// @function `int_safe_div`
        /// @brief Creates a safe division of two unsigned integer types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value of the safe division
        /// @param `rhs` The rhs value of the safe division
        /// @return `llvm::Value *` The result of the safe unsigned division
        static llvm::Value *uint_safe_div(llvm::IRBuilder<> &builder, llvm::Value *lhs, llvm::Value *rhs);
    }; // subclass Arithmetic

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
        static inline std::unordered_map<std::string_view, llvm::Function *> typecast_functions = {
            {"count_digits", nullptr},
            {"i32_to_str", nullptr},
            {"u32_to_str", nullptr},
            {"i64_to_str", nullptr},
            {"u64_to_str", nullptr},
            {"f32_to_str", nullptr},
            {"f64_to_str", nullptr},
        };

        /// @function `generate_helper_functions`
        /// @brief Function to generate all helper functions used for the type-casting. Currently these helper functions are only used when
        /// casting strings
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the function is generated in
        static void generate_helper_functions(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_count_digits_function`
        /// @brief Function to generate the `count_digits` helper function, used for to-string casting
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the function is generated in
        static void generate_count_digits_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /**************************************************************************************************************************************
         * @region `I32`
         *************************************************************************************************************************************/

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
        static void generate_i32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module);

        /**************************************************************************************************************************************
         * @region `U32`
         *************************************************************************************************************************************/

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
        static void generate_u32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module);

        /**************************************************************************************************************************************
         * @region `I64`
         *************************************************************************************************************************************/

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
        static void generate_i64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module);

        /**************************************************************************************************************************************
         * @region `U64`
         *************************************************************************************************************************************/

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
        static void generate_u64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module);

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
        static void generate_f32_to_str(llvm::IRBuilder<> *builder, llvm::Module *module);

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
        static void generate_f64_to_str(llvm::IRBuilder<> *builder, llvm::Module *module);
    }; // subclass TypeCast

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
        ///
        /// @attention The allocations map will be modified (new entries are added), but it will not be cleared. If you want a clear
        /// allocations map before calling this function, you need to clear it yourself.
        static void generate_allocations(                                    //
            llvm::IRBuilder<> &builder,                                      //
            llvm::Function *parent,                                          //
            const Scope *scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations //
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
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `call_node` The CallNode used to generate the allocations from
        ///
        /// @attention The allocations map will be modified
        static void generate_call_allocations(                                //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const CallNodeBase *call_node                                     //
        );

        /// @funnction `generate_if_allcoations`
        /// @brief Generates the allocations for if chains
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `if_node` The IfNode used to generate the allocations from
        ///
        /// @attention The allocations map will be modified
        static void generate_if_allocations(                                  //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const IfNode *if_node                                             //
        );

        /// @funnction `generate_declaration_allcoations`
        /// @brief Generates the allocations for a normal declaration
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `scope` The scope the allocation would take place in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `declaration_node` The DeclarationNode used to generate the allocations from
        ///
        /// @attention The allocations map will be modified
        static void generate_declaration_allocations(                         //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const DeclarationNode *declaration_node                           //
        );

        /// @funnction `generate_group_declaration_allcoations`
        /// @brief Generates the allocations for grouped declarations
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the allocations are generated in
        /// @param `scope` The scope the allocation would take place in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc is encoded
        /// @param `group_declaration_node` The GroupDeclarationNode used to generate the allocations from
        ///
        /// @attention The allocations map will be modified
        static void generate_group_declaration_allocations(                   //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const GroupDeclarationNode *group_declaration_node                //
        );

        /// @function `generate_expression_allocations`
        /// @brief Goes throught all expressions and searches for all calls to allocate them
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `scope` The scope the allocation would take place in
        /// @param `allocations` The map of allocations, where in the key all information like scope ID, call ID, name, etc in encoded
        /// @param `expression` The expression to search for calls for
        ///
        /// @attention The allocations map will be modified
        static void generate_expression_allocations(                          //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const ExpressionNode *expression                                  //
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
        /// @param `context` The LLVM context the type is generated in
        /// @param `function_node` The FunctionNode used to generate the type
        /// @return `llvm::FunctionType *` A pointer to the generated FuntionType
        static llvm::FunctionType *generate_function_type(llvm::LLVMContext &context, FunctionNode *function_node);

        /// @function `generate_function`
        /// @brief Generates a function from a given FunctionNode
        ///
        /// @param `module` The LLVM Module the function will be generated in
        /// @param `function_node` The FunctionNode used to generate the function
        /// @return `llvm::Function *` A pointer to the generated Function
        static llvm::Function *generate_function(llvm::Module *module, FunctionNode *function_node);

        /// @function `get_function_definition`
        /// @brief Returns the function definition from the given CallNode or other values based on a few conditions
        ///
        /// In the "normal" execution case, this function returns a pointer to the definition from a given CallNode's call. But, there are
        /// some conditions. If the call is towards a builtin function, this function explicitely returns a nullptr on purpose. If the
        /// definition could not be found, this function returns a `std::nullopt`. The second variable determines whether the call targets a
        /// function inside the current module (false) or if it targets a function from another module (true).
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
        /// @param `parent` The function the statement will be generated in
        /// @param `scope` The scope the statement is being generated in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `statement` The statement which will be generated
        static void generate_statement(                                       //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const std::unique_ptr<StatementNode> &statement                   //
        );

        /// @function `generate_body`
        /// @brief Generates a whole body
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the body will be generated in
        /// @param `scope` The scope containing the body which will be generated
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        static void generate_body(                                           //
            llvm::IRBuilder<> &builder,                                      //
            llvm::Function *parent,                                          //
            const Scope *scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations //
        );

        /// @function `generate_end_of_scope`
        /// @brief Generates the instructions that need to be applied at the end of a scope (variables going out of scope, for example)
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `scope` The scope that ends
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        static void generate_end_of_scope(                                   //
            llvm::IRBuilder<> &builder,                                      //
            const Scope *scope,                                              //
            std::unordered_map<std::string, llvm::Value *const> &allocations //
        );

        /// @function `generate_return_statement`
        /// @brief Generates the return statement from the given ReturnNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the return statement will be generated in
        /// @param `scope` The scope the return statement will be generated in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `return_node` The return node to generated
        static void generate_return_statement(                                //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const ReturnNode *return_node                                     //
        );

        /// @function `generate_throw_statement`
        /// @brief Generates the throw statement from the given ThrowNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the return statement will be generated in
        /// @param `scope` The scope the throw statement will be generated in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `throw_node` The throw node to generate
        static void generate_throw_statement(                                 //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const ThrowNode *throw_node                                       //
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
        /// @param `parent` The function the if chain will be generated in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `blocks` The list of all basic blocks the if bodies are contained in
        /// @param `nesting_level` The nesting level determines how "deep" one is inside the if-chain
        /// @param `if_node` The if node to generate
        static void generate_if_statement(                                    //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            std::vector<llvm::BasicBlock *> &blocks,                          //
            unsigned int nesting_level,                                       //
            const IfNode *if_node                                             //
        );

        /// @function `generate_while_loop`
        /// @brief Generates the while loop from the given WhileNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the while loop will be generated in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `while_node` The while node to generate
        static void generate_while_loop(                                      //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const WhileNode *while_node                                       //
        );

        /// @function `generate_for_loop`
        /// @brief Generates the for loop from the given ForLoopNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the for loop will be generated in
        /// @param `allocations` The map of all allocations (from the preallcation system) to track the AllocaInst instructions
        /// @param `for_node` The for loop node to generate
        static void generate_for_loop(                                        //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const ForLoopNode *for_node                                       //
        );

        /// @function `generate_catch_statement`
        /// @brief Generates the catch statement from the given CatchNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the catch statement will be generated in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `catch_node` The catch node to generate
        static void generate_catch_statement(                                 //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const CatchNode *catch_node                                       //
        );

        /// @function `generate_group_declaration`
        /// @brief Generates the group declaration from the given GroupDeclarationNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the group declaration will be generated in
        /// @param `scope` The scope the group declaration is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `declaration_node` The group declaration node to generate
        static void generate_group_declaration(                               //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const GroupDeclarationNode *declaration_node                      //
        );

        /// @function `generate_declaration`
        /// @brief Generates the declaration from the given DeclarationNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the declaration will be generated in
        /// @param `scope` The scope the declaration is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `declaration_node` The declaration node to generate
        static void generate_declaration(                                     //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const DeclarationNode *declaration_node                           //
        );

        /// @function `generate_assignment`
        /// @brief Generates the assignment from the given AssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the assignment will be generated in
        /// @param `scope` The scope the assignment is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `assignment_node` The assignment node to generate
        static void generate_assignment(                                      //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const AssignmentNode *assignment_node                             //
        );

        /// @function `generate_group_assignment`
        /// @brief Generates the group assignment from the given GroupAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the group assignment will be generated in
        /// @param `scope` The scope the group assignment is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `group_assignment` The group assignemnt node to generate
        static void generate_group_assignment(                                //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const GroupAssignmentNode *group_assignment                       //
        );

        /// @function `generate_data_field_assignment`
        /// @brief Generates the data field assignment from the given DataFieldAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the data field assignemnt will be generated in
        /// @param `scope` The scope the data field assignment is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `data_field_assignment` The data field assignment to generate
        static void generate_data_field_assignment(                           //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const DataFieldAssignmentNode *data_field_assignment              //
        );

        /// @function `generate_grouped_data_field_assignment`
        /// @brief Generates the grouped field assignment from the given GroupedDataFieldAssignmentNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the grouped data field assignment will be generated in
        /// @param `scope` The scope the grouped data field assignment is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `grouped_field_assignment` The grouped data field assignment to generate
        static void generate_grouped_data_field_assignment(                   //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const GroupedDataFieldAssignmentNode *grouped_field_assignment    //
        );

        /// @function `generate_unary_op_statement`
        /// @brief Generates the unary operation value from the given UnaryOpStatement
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the unary operation is generated in
        /// @param `scope` The scope the binary operation is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `unary_op` The unary operation to generate
        static void generate_unary_op_statement(                              //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const UnaryOpStatement *unary_op                                  //
        );
    }; // subclass Statement

    /// @class `Expression`
    /// @brief The class which is responsible for generating everything related to expressions
    /// @note This class cannot be initialized and all functions within this class are static
    class Expression {
      public:
        // The constructor is deleted to make this class non-initializable
        Expression() = delete;

        /// @function `generate_expression`
        /// @brief Generates an expression from the given ExpressionNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the expression will be generated in
        /// @param `scope` The scope the expression is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `expression_node` The expression node to generate
        /// @param `is_reference` Whether the result of the expression should be a reference. This is only possible for certain expressions
        /// like variables for example, defaults to false
        /// @return `group_mapping` The value(s) containing the result of the expression
        static group_mapping generate_expression(                             //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const ExpressionNode *expression_node,                            //
            const bool is_reference = false                                   //
        );

        /// @function `generate_literal`
        /// @brief Generates the literal value from the given LiteralNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The scope the literal is contained in
        /// @param `scope` The scope the expression is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `literal_node` The literal node to generate
        /// @return `llvm::Value *` The value containing the result of the literal
        static llvm::Value *generate_literal(                                 //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const LiteralNode *literal_node                                   //
        );

        /// @function `generate_variable`
        /// @brief Generates the variable from the given VariableNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the variable is generated in
        /// @param `scope` The scope the variable is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `variable_node` The variable node to generate
        /// @param `is_reference` Whether to return the value or the AllocaInst of the variable
        /// @return `llvm::Value *` The value containing the result of the variable
        static llvm::Value *generate_variable(                                //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const VariableNode *variable_node,                                //
            const bool is_reference = false                                   //
        );

        /// @function `generate_call`
        /// @brief Generates the call from the given CallNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the call is generated in
        /// @param `scope` The scope the call is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `call_node` The call node to generate
        /// @return `group_mapping` The value(s) containing the result of the call
        static group_mapping generate_call(                                   //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const CallNodeBase *call_node                                     //
        );

        /// @function `generate_rethrow`
        /// @brief Generates a catch block which re-throws the error of the call, if the call had an error
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the rethrow is generated in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `call_node` The call node which is used to generate the rethrow from
        static void generate_rethrow(                                         //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const CallNodeBase *call_node                                     //
        );

        /// @function `generate_group_expression`
        /// @brief Generates a group expression from the given GroupExpressionNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the group expression is generated in
        /// @param `scope` The scope the group expression is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `group_node` The group operation to generate
        /// @return `group_mapping` The value(s) containing the result of the group expression
        static group_mapping generate_group_expression(                       //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const GroupExpressionNode *group_node                             //
        );

        /// @function `generate_initializer`
        /// @brief Generates an initializer from the given InitializerNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the initializer is generated in
        /// @param `scope` The scope the initializer is contained in
        /// @param `allocations´`The map of all alloccations (from the preallocation system) to track the AllocaInst instructions
        /// @param `initializer` The initializer to generate
        /// @return `group_mapping` The loaded value(s) of the initializer, representing every field of the loaded data
        static group_mapping generate_initializer(                            //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const InitializerNode *initializer                                //
        );

        /// @function `generate_data_access`
        /// @brief Generates a data access from a given DataAccessNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `scope` The scope the data access is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `data_access` The data access node to generate
        /// @return `llvm::Value *` The value containing the result of the data access
        static llvm::Value *generate_data_access(                             //
            llvm::IRBuilder<> &builder,                                       //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const DataAccessNode *data_access                                 //
        );

        /// @function `generate_grouped_data_access`
        /// @brief Generates a grouped data access from a given GroupedDataAccessNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `scope` The scope the grouped data access is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `grouped_data_access` The grouped data access node to generate
        /// @return `group_mapping` The value(s) containing the result of the grouped data access
        static group_mapping generate_grouped_data_access(                    //
            llvm::IRBuilder<> &builder,                                       //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const GroupedDataAccessNode *grouped_data_access                  //
        );

        /// @function `generate_type_cast`
        /// @brief Generates a type cast from a TypeCastNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the type cast is generated in
        /// @param `scope` The scope the type cast is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `type_cast_node` The type cast to generate
        /// @return `group_mapping` The value(s) containing the result of the type cast
        static group_mapping generate_type_cast(                              //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const TypeCastNode *type_cast_node                                //
        );

        /// @function `generate_type_cast`
        /// @brief Generates a type cast from the given expression depending on the from and to types
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `expr` The llvm value which will be cast
        /// @param `from_type` The string representation of the type to cast from
        /// @param `to_type` The string representation of the type to cast to
        /// @return `llvm::Value *` The value containing the result of the type cast
        static llvm::Value *generate_type_cast( //
            llvm::IRBuilder<> &builder,         //
            llvm::Value *expr,                  //
            const std::string &from_type,       //
            const std::string &to_type          //
        );

        /// @function `generate_unary_op_expression`
        /// @brief Generates the unary operation value from the given UnaryOpNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the unary operation is generated in
        /// @param `scope` The scope the binary operation is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `unary_op` The unary operation to generate
        /// @return `group_mapping` The value containing the result of the unary operation
        static group_mapping generate_unary_op_expression(                    //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const UnaryOpExpression *unary_op                                 //
        );

        /// @function `generate_binary_op`
        /// @brief Generates a binary operation from the given BinaryOpNode
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `parent` The function the binary operation is generated in
        /// @param `scope` The scope the binary operation is contained in
        /// @param `allocations` The map of all allocations (from the preallocation system) to track the AllocaInst instructions
        /// @param `bin_op_node` The binary operation to generate
        /// @return `group_mapping` The value(s) containing the result of the binop
        static group_mapping generate_binary_op(                              //
            llvm::IRBuilder<> &builder,                                       //
            llvm::Function *parent,                                           //
            const Scope *scope,                                               //
            std::unordered_map<std::string, llvm::Value *const> &allocations, //
            const BinaryOpNode *bin_op_node                                   //
        );
    }; // subclass Expression

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
        /// @attention The functions are nullpointers until the `generate_builtin_prints` function is called
        /// @attention The map is not being cleared after the program module has been generated
        static inline std::unordered_map<std::string_view, llvm::Function *> string_manip_functions = {
            {"create_str", nullptr},
            {"init_str", nullptr},
            {"assign_str", nullptr},
            {"assign_lit", nullptr},
            {"add_str_str", nullptr},
            {"add_str_lit", nullptr},
            {"add_lit_str", nullptr},
        };

        /// @function `generate_create_str_function`
        /// @brief Generates the builtin hidden `create_str` function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the `create_str` function will be generated in
        static void generate_create_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_init_str_function`
        /// @brief Generates the builtin hidden `init_str` function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the `init_str` function will be generated in
        static void generate_init_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_assign_str_function`
        /// @brief Generates the builtin hidden `assign_str` function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the `assign_str` function will be generated in
        static void generate_assign_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_assign_lit_function`
        /// @brief Generates the builtin hidden `assign_lit` function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the `assign_lit` function will be generated in
        static void generate_assign_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_add_str_str_functiion`
        /// @brief Generates the builtin hidden `add_str_str` function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the `add_str_str` function will be generated in
        static void generate_add_str_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_add_str_lit_function`
        /// @brief Generates the builtin hidden `add_str_lit` function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the `add_str_lit` function will be generated in
        static void generate_add_str_lit_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_add_lit_str_function`
        /// @brief Generates the builtin hidden `add_lit_str` function
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the `add_lit_str` function will be generated in
        static void generate_add_lit_str_function(llvm::IRBuilder<> *builder, llvm::Module *module);

        /// @function `generate_string_manip_functions`
        /// @brief Generates all the builtin hidden string manipulation functions
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `module` The LLVM Module the string manipulation functions will be generated in
        static void generate_string_manip_functions(llvm::IRBuilder<> *builder, llvm::Module *module);

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
        /// @param `assignment_node` The assignment node, used to check if the rhs is a literal or not
        /// @param `expression` The generated value of the expression of the rhs
        static void generate_string_assignment(    //
            llvm::IRBuilder<> &builder,            //
            llvm::Value *const lhs,                //
            const AssignmentNode *assignment_node, //
            llvm::Value *expression                //
        );

        /// @function `generate_string_addition`
        /// @brief Generates the addition instruction of two strings and returns the result of the addition
        ///
        /// @param `builder` The LLVM IRBuilder
        /// @param `lhs` The lhs value from llvm
        /// @param `lhs_expr` The lhs expression, to check if it is / was a literal
        /// @param `rhs` The rhs value from llvm
        /// @param `rhs_expr` The rhs expression, to check if it is / was a literal
        /// @return `llvm::Value *` The result of the string addition
        static llvm::Value *generate_string_addition( //
            llvm::IRBuilder<> &builder,               //
            llvm::Value *lhs,                         //
            const ExpressionNode *lhs_expr,           //
            llvm::Value *rhs,                         //
            const ExpressionNode *rhs_expr            //
        );
    }; // subclass String
};
