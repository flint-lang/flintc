#define FIP_IMPLEMENTATION
#include "fip.hpp"

#ifdef DEBUG_BUILD
fip_log_level_t LOG_LEVEL = FIP_DEBUG;
#else
fip_log_level_t LOG_LEVEL = FIP_ERROR;
#endif
fip_master_state_t master_state;

#include "error/error.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "profiler.hpp"

#include <iostream>

bool FIP::init() {
    PROFILE_SCOPE("FIP init");
    if (is_active) {
        // Initializing an active FIP is considered an error case as this should not happen, but it's a me-problem, not a user problem
        assert(false);
        return false;
    }
    is_active = true;

    // Parse the config file (fip.toml)
    fip_master_config_t config_file = fip_master_load_config();
    bool needs_shutdown = true;
    if (config_file.ok) {
        // Next we check whether there are any active modules in the config file, if there are not then we can shut down
        if (config_file.enabled_count > 0) {
            // Now we check if all of the enabled modules even exist in the `.fip/modules/` path. If an enabled module does not exist then
            // we report it and shut down too. If all exist, however, we stay active
            needs_shutdown = false;
            for (uint8_t i = 0; i < config_file.enabled_count; i++) {
                std::filesystem::path module_path = ".fip/modules/";
                module_path = module_path / std::string(config_file.enabled_modules[i]);
                if (!std::filesystem::exists(module_path)) {
                    needs_shutdown = true;
                    break;
                }
            }
        }
    }
    if (needs_shutdown) {
        // If there is no fip.toml file or it is faulty we just do not start FIP at all and just continue with compilation. If a symbol
        // resolve request is done later because there are external symbols a custom error will be thrown for that case, but it's not a
        // mistake in of itself to not have a fip.toml config file at all
        is_active = false;
        return true;
    }

    // Now that the config file has been parsed and we know that we *actually* need the FIP we can initialize the socket
    socket_fd = fip_master_init_socket();
    if (socket_fd == -1) {
        fip_print(0, FIP_ERROR, "Failed to initialize socket, exiting");
        abort();
    }

    // Start all enabled interop modules
    for (uint8_t i = 0; i < config_file.enabled_count; i++) {
        const char *mod = config_file.enabled_modules[i];
        fip_print(0, FIP_INFO, "Starting the %s module...", mod);
        char module_path[13 + FIP_MAX_MODULE_NAME_LEN] = {0};
        snprintf(module_path, sizeof(module_path), ".fip/modules/%s", mod);
        fip_spawn_interop_module(&modules, module_path);
    }

    // Give the Interop Modules time to connect
    fip_print(0, FIP_INFO, "Waiting for interop modules to connect...");
    fip_master_accept_pending_connections(socket_fd);
    // Wait for all connect messages from the IMs
    fip_print(0, FIP_INFO, "Waiting for all connect requests...");
    fip_master_await_responses(       //
        buffer,                       //
        master_state.responses,       //
        &master_state.response_count, //
        FIP_MSG_CONNECT_REQUEST       //
    );
    // Check if each interop module has the correct version
    for (uint8_t i = 0; i < master_state.response_count; i++) {
        const fip_msg_t *response = &master_state.responses[i];
        const fip_msg_connect_request_t *req = &response->u.con_req;
        assert(response->type == FIP_MSG_CONNECT_REQUEST);
        if (req->version.major != FIP_MAJOR    //
            || req->version.minor != FIP_MINOR //
            || req->version.patch != FIP_PATCH //
        ) {
            fip_print(0, FIP_ERROR, "Version mismatch with module '%s'", response->u.con_req.module_name);
            fip_print(                                                                                      //
                0, FIP_ERROR, "  Expected 'v%d.%d.%d' but got 'v%d.%d.%d'",                                 //
                FIP_MAJOR, FIP_MINOR, FIP_PATCH, req->version.major, req->version.minor, req->version.patch //
            );
            return false;
        }
        if (!req->setup_ok) {
            fip_print(0, FIP_ERROR, "Module '%s' failed it's setup", req->module_name);
            return false;
        }
    }
    return true;
}

void FIP::shutdown() {
    if (!is_active) {
        // Shutting down when not being active is actually fine, since the FIP could already be shut down as it never ran in the first place
        // (for example when no fip.toml was provided)
        return;
    }
    PROFILE_SCOPE("FIP shutdown");
    fip_free_msg(&message);
    message.type = FIP_MSG_KILL;
    message.u.kill.reason = FIP_KILL_FINISH;
    auto sleep_100ms = timespec{.tv_sec = 0, .tv_nsec = 100000000};
    nanosleep(&sleep_100ms, NULL);
    fip_master_broadcast_message(buffer, &message);

    // Clean up
    nanosleep(&sleep_100ms, NULL);
    fip_master_cleanup_socket(socket_fd);
    fip_terminate_all_slaves(&modules); // Fallback cleanup

    fip_print(0, FIP_INFO, "Master shutting down");
}

bool FIP::convert_type(fip_type_t *dest, const std::shared_ptr<Type> &src, const bool is_mutable) {
    dest->is_mutable = is_mutable;
    if (const PrimitiveType *prim_type = dynamic_cast<const PrimitiveType *>(src.get())) {
        dest->type = FIP_TYPE_PRIMITIVE;

        // Map primitive type string to enum
        const std::string type_str = prim_type->to_string();
        if (type_str == "u8") {
            dest->u.prim = FIP_U8;
        } else if (type_str == "u32") {
            dest->u.prim = FIP_U32;
        } else if (type_str == "u64") {
            dest->u.prim = FIP_U64;
        } else if (type_str == "i32") {
            dest->u.prim = FIP_I32;
        } else if (type_str == "i64") {
            dest->u.prim = FIP_I64;
        } else if (type_str == "f32") {
            dest->u.prim = FIP_F32;
        } else if (type_str == "f64") {
            dest->u.prim = FIP_F64;
        } else if (type_str == "bool") {
            dest->u.prim = FIP_BOOL;
        } else if (type_str == "str") {
            dest->u.prim = FIP_STR;
        } else {
            // Unknown primitive type
            return false;
        }
        return true;
    } else if (const MultiType *multi_type = dynamic_cast<const MultiType *>(src.get())) {
        // A multi-type will be handled just as a struct type and nothing more
        dest->type = FIP_TYPE_STRUCT;
        dest->u.struct_t.field_count = static_cast<uint8_t>(multi_type->width);
        dest->u.struct_t.fields = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t) * multi_type->width));
        for (uint8_t i = 0; i < dest->u.struct_t.field_count; i++) {
            if (!convert_type(&dest->u.struct_t.fields[i], multi_type->base_type, true)) {
                return false;
            }
        }
        return true;
    } else if (const DataType *data_type = dynamic_cast<const DataType *>(src.get())) {
        // A data-type is just a struct and will be handled as such
        const DataNode *data_node = data_type->data_node;
        dest->type = FIP_TYPE_STRUCT;
        dest->u.struct_t.field_count = static_cast<uint8_t>(data_node->fields.size());
        dest->u.struct_t.fields = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t) * data_node->fields.size()));
        for (uint8_t i = 0; i < dest->u.struct_t.field_count; i++) {
            if (!convert_type(&dest->u.struct_t.fields[i], data_node->fields.at(i).second, true)) {
                return false;
            }
        }
        return true;
    } else {
        // Handle pointer types or other complex types
        dest->type = FIP_TYPE_PTR;
        // TODO: Implement pointer type handling based on your type system
        // For now, return false for unsupported types
        return false;
    }
}

bool FIP::resolve_function(FunctionNode *function) {
    if (!is_active) {
#ifdef FLINT_LSP
        function->is_extern = true;
        function->error_types.clear();
        return true;
#else
        // We need an error here specifically because we try to resolve an external function without the FIP, which is not possible
        THROW_ERR(ErrExternWithoutFip, ERR_PARSING, function->file_name, function->line, function->column, function->length);
        return false;
#endif
    }
    // This function is not concurrency-safe. FIP assumes a strict order for the master's messages so only one thread is allowed to send /
    // recieve messages to and from the FIP at a time
    static std::mutex resolve_mutex;
    std::lock_guard<std::mutex> lock(resolve_mutex);
    fip_msg_t msg = fip_msg_t{};
    msg.type = FIP_MSG_SYMBOL_REQUEST;
    msg.u.sym_req.type = FIP_SYM_FUNCTION;
    strncpy(msg.u.sym_req.sig.fn.name, function->name.c_str(), function->name.size());
    std::string fn_str = function->name + "(";
    for (size_t i = 0; i < function->parameters.size(); i++) {
        if (std::get<2>(function->parameters.at(i))) {
            fn_str.append("mut ");
        } else {
            fn_str.append("const ");
        }
        fn_str.append(std::get<0>(function->parameters.at(i))->to_string());
        if (i + 1 < function->parameters.size()) {
            fn_str.append(", ");
        }
    }
    fn_str.append(")");
    if (function->return_types.size() == 1) {
        fn_str.append("->");
        fn_str.append(function->return_types.front()->to_string());
    } else if (function->return_types.size() > 1) {
        fn_str.append("->(");
        for (size_t i = 0; i < function->return_types.size(); i++) {
            fn_str.append(function->return_types.at(i)->to_string());
            if (i + 1 < function->return_types.size()) {
                fn_str.append(", ");
            }
        }
        fn_str.append(")");
    }
    fip_print(0, FIP_INFO, "Trying to resolve function: '%s'", fn_str.c_str());

    if (!function->return_types.empty()) {
        const uint8_t rets_len = static_cast<uint8_t>(function->return_types.size());
        msg.u.sym_req.sig.fn.rets_len = rets_len;
        msg.u.sym_req.sig.fn.rets = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t) * rets_len));
        for (uint8_t i = 0; i < rets_len; i++) {
            if (!convert_type(&msg.u.sym_req.sig.fn.rets[i], function->return_types.at(i), true)) {
                const std::string type_str = function->return_types.at(i)->to_string();
                fip_print(0, FIP_ERROR, "Type '%s' not compatible with FIP", type_str.c_str());
                return false;
            }
        }
    }

    if (!function->parameters.empty()) {
        const uint8_t args_len = static_cast<uint8_t>(function->parameters.size());
        msg.u.sym_req.sig.fn.args_len = args_len;
        msg.u.sym_req.sig.fn.args = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t) * args_len));
        for (uint8_t i = 0; i < args_len; i++) {
            const auto &param = function->parameters.at(i);
            if (!convert_type(&msg.u.sym_req.sig.fn.args[i], std::get<0>(param), std::get<2>(param))) {
                const std::string type_str = std::get<0>(function->parameters.at(i))->to_string();
                fip_print(0, FIP_ERROR, "Type '%s' not compatible with FIP", type_str.c_str());
                return false;
            }
        }
    }

    fip_print(0, FIP_INFO, "Checking whether the function '%s' exists", function->name.c_str());
    if (!fip_master_symbol_request(buffer, &msg)) {
        fip_print(0, FIP_ERROR, "The function '%s' could not be resolved", function->name.c_str());
        return false;
    }

    fake_function fake_fn{};
    fake_fn.ret_types = function->return_types;
    for (const auto &param : function->parameters) {
        fake_fn.arg_types.emplace_back(std::get<0>(param));
    }
    for (uint8_t i = 0; i < master_state.response_count; i++) {
        if (master_state.responses[i].type == FIP_MSG_SYMBOL_RESPONSE //
            && master_state.responses[i].u.sym_res.found              //
        ) {
            fake_fn.module_name = std::string(master_state.responses[i].u.sym_res.module_name);
            break;
        }
    }

    function->is_extern = true;
    function->error_types.clear();
    fake_fn.name = function->name;
    resolved_functions.push_back(fake_fn);
    return true;
}

void FIP::send_compile_request() {
    if (!is_active) {
        // No need to error out, the FIP is not running so there is nothing to compile
        return;
    }
    fip_msg_t msg{};
    msg.type = FIP_MSG_COMPILE_REQUEST;
    // TODO: Change the target of the compile request
    fip_master_broadcast_message(buffer, &msg);
}

std::vector<std::array<char, 9>> FIP::gather_objects() {
    if (!is_active) {
        // No error, just return an empty list
        return {};
    }
    std::vector<std::array<char, 9>> objects;
    uint8_t wrong_msg_count = fip_master_await_responses( //
        buffer,                                           //
        master_state.responses,                           //
        &master_state.response_count,                     //
        FIP_MSG_OBJECT_RESPONSE                           //
    );
    if (wrong_msg_count > 0) {
        fip_print(0, FIP_WARN, "Recieved %u faulty messages", wrong_msg_count);
        return {};
    }
    // Now we can go through all responses and print all the .o files we
    // recieved
    for (uint8_t i = 0; i < master_state.response_count; i++) {
        const fip_msg_t *response = &master_state.responses[i];
        assert(response->type == FIP_MSG_OBJECT_RESPONSE);
        if (response->u.obj_res.has_obj) {
            fip_print(0, FIP_INFO, "Object response from module: %s", response->u.obj_res.module_name);
            fip_print(0, FIP_DEBUG, "Paths: %s", response->u.obj_res.paths);
            char const *paths = response->u.obj_res.paths;
            while (*paths != '\0') {
                std::array<char, 9> obj;
                std::copy_n(paths, 8, obj.begin());
                obj.at(8) = '\0';
                objects.emplace_back(obj);
                paths += 8;
            }
        } else {
            fip_print(0, FIP_INFO, "Object response carries no objects");
        }
    }
    if (DEBUG_MODE) {
        std::cout << YELLOW << "[Debug Info] Gathered FIP Objects:" << DEFAULT << std::endl;
        for (const auto &obj : objects) {
            std::cout << "-- " << obj.data() << "\n";
        }
        std::cout << std::endl;
    }
    return objects;
}
