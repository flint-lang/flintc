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

    // TODO: Change it to dynamically discovering the interop modules from the `.fip/modules` path

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

bool FIP::resolve_function([[maybe_unused]] FunctionNode *function) {
    return false;
}

std::vector<std::array<char, 8>> FIP::gather_objects() {
    return {};
}
