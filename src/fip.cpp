#define FIP_IMPLEMENTATION
#include "fip.hpp"

#include "profiler.hpp"

bool FIP::init() {
    PROFILE_SCOPE("FIP init");
    socket_fd = fip_master_init_socket();
    if (socket_fd == -1) {
        fip_print(0, "Failed to initialize socket, exiting");
        abort();
    }

    // Parse the config file (fip.toml)
    fip_master_config_t config_file = fip_master_load_config();

    // Start all enabled interop modules
    for (uint8_t i = 0; i < config_file.enabled_count; i++) {
        const char *mod = config_file.enabled_modules[i];
        fip_print(0, "Starting the %s module...", mod);
        char module_path[13 + FIP_MAX_MODULE_NAME_LEN] = {0};
        snprintf(module_path, sizeof(module_path), ".fip/modules/%s", mod);
        fip_spawn_interop_module(&modules, module_path);
    }

    // Give the Interop Modules time to connect
    fip_print(0, "Waiting for interop modules to connect...");
    fip_master_accept_pending_connections(socket_fd);
    // Wait for all connect messages from the IMs
    fip_print(0, "Waiting for all connect requests...");
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
            fip_print(0, "Version mismatch with module '%s'", response->u.con_req.module_name);
            fip_print(0, "  Expected 'v%d.%d.%d' but got 'v%d.%d.%d'", FIP_MAJOR, FIP_MINOR, FIP_PATCH, req->version.major,
                req->version.minor, req->version.patch);
            return false;
        }
        if (!req->setup_ok) {
            fip_print(0, "Module '%s' failed it's setup", req->module_name);
            return false;
        }
    }
    return true;
}

void FIP::shutdown() {
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

    fip_print(0, "Master shutting down");
}

bool FIP::resolve_function(FunctionNode *function) {
    // This function is not concurrency-safe. FIP assumes a strict order for the master's messages so only one thread is allowed to send /
    // recieve messages to and from the FIP at a time
    static std::mutex resolve_mutex;
    std::lock_guard<std::mutex> lock(resolve_mutex);
    fip_msg_t msg = fip_msg_t{};
    msg.type = FIP_MSG_SYMBOL_REQUEST;
    msg.u.sym_req.type = FIP_SYM_FUNCTION;
    strncpy(msg.u.sym_req.sig.fn.name, function->name.c_str(), function->name.size());
    if (!function->return_types.empty()) {
        const uint8_t rets_len = static_cast<uint8_t>(function->return_types.size());
        msg.u.sym_req.sig.fn.rets_len = rets_len;
        msg.u.sym_req.sig.fn.rets = static_cast<fip_sig_type_t *>(malloc(sizeof(fip_sig_type_t) * rets_len));
        for (uint8_t i = 0; i < rets_len; i++) {
            msg.u.sym_req.sig.fn.rets[i].is_mutable = false;
            // Find the type ID
            uint8_t type_id = 0;
            for (; type_id < FIP_TYPE_COUNT; type_id++) {
                const std::string type_str(fip_type_names[type_id]);
                if (type_str == function->return_types.at(i)->to_string()) {
                    break;
                }
            }
            if (type_id == FIP_TYPE_COUNT) {
                // No valid FIP type found
                return false;
            }
            msg.u.sym_req.sig.fn.rets[i].type = static_cast<fip_type_enum_t>(type_id);
        }
    }
    if (!function->parameters.empty()) {
        const uint8_t args_len = static_cast<uint8_t>(function->parameters.size());
        msg.u.sym_req.sig.fn.args_len = args_len;
        msg.u.sym_req.sig.fn.args = static_cast<fip_sig_type_t *>(malloc(sizeof(fip_sig_type_t) * args_len));
        for (uint8_t i = 0; i < args_len; i++) {
            msg.u.sym_req.sig.fn.args[i].is_mutable = std::get<2>(function->parameters.at(i));
            // Find the type ID
            uint8_t type_id = 0;
            for (; type_id < FIP_TYPE_COUNT; type_id++) {
                const std::string type_str(fip_type_names[type_id]);
                if (type_str == std::get<0>(function->parameters.at(i))->to_string()) {
                    break;
                }
            }
            if (type_id == FIP_TYPE_COUNT) {
                // No valid FIP type found
                return false;
            }
            msg.u.sym_req.sig.fn.args[i].type = static_cast<fip_type_enum_t>(type_id);
        }
    }
    fip_print(0, "Checking whether the function '%s' exists", function->name.c_str());
    if (!fip_master_symbol_request(buffer, &msg)) {
        fip_print(0, "The function '%s' does not exist", function->name.c_str());
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
    // Change the function tame to have the prefix `__fip_X_` where `X` is the module name it came from (`fip-X`)
    function->extern_name_alias = "__fip_" + fake_fn.module_name.substr(4) + "_" + function->name;
    function->error_types.clear();
    fake_fn.name = function->name;
    resolved_functions.push_back(fake_fn);
    return true;
}

std::vector<std::array<char, 8>> FIP::gather_objects() {
    return {};
}
