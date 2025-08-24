#pragma once

#define NO_FIP_LIB
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
#include "fip.h"
#pragma GCC diagnostic pop

#ifdef __cplusplus
}
#endif

#include "parser/ast/definitions/function_node.hpp"

#include <array>
#include <vector>

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

    /// @var `modules`
    /// @brief All the available modules of the FIP which contains the file descriptors of all active and spawned modules
    static inline fip_interop_modules_t modules;

    /// @var `buffer`
    /// @brief The message buffer of FIP
    static inline char buffer[FIP_MSG_SIZE];

    /// @var `socket_fd`
    /// @brief The socket file descriptor of the master
    static inline int socket_fd;

    /// @var `message`
    /// @brief The message which will be re-used for all FIP communications
    static inline fip_msg_t message;

    /// @var `resolved_functions`
    /// @brief A list containing all functions which have been matched by FIP
    static inline std::vector<fake_function> resolved_functions;

    /// @function `init`
    /// @brief Initializes the FIP and does whatever needs to be done at the FIP setup stage
    ///
    /// @return `bool` Whether initialization failed
    static bool init();

    /// @function `shutdown`
    /// @brief Shuts down the FIP and sends the kill messages to all interop modules
    static void shutdown();

    /// @function `resolve_function`
    /// @brief Resolves a given function definition, changes it's internal name for code generation and returns whether the function was
    /// even found in one of the interop modules at all
    ///
    /// @param `function` The function definition to resolve
    /// @return `bool` Whether the function symbol could be resolved
    static bool resolve_function(FunctionNode *function);

    /// @function `send_compile_request`
    /// @brief Sends the compile request to all interop modules, meaning that they now can start compiling their respective source files
    static void send_compile_request();

    /// @function `gather_objects`
    /// @brief Gathers all built objects of all interop modules which need to be linked to the final executable
    ///
    /// @return `std::vector<std::array<char, 9>>` The 8-byte object hashes (+ null terminator) of all objects which need to be linked to
    /// the final executable
    static std::vector<std::array<char, 9>> gather_objects();
};
