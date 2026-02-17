#define FIP_IMPLEMENTATION
#include "fip.hpp"

#ifdef DEBUG_BUILD
fip_log_level_e LOG_LEVEL = FIP_DEBUG;
#else
fip_log_level_e LOG_LEVEL = FIP_ERROR;
#endif
fip_master_state_t master_state;

#include "error/error.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/pointer_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "profiler.hpp"

#include <filesystem>
#include <fstream>

// Helper: Split string by delimiter (':' on Unix, ';' on Windows)
std::vector<std::string> split_path(const std::string &path, const char delimiter) {
    std::vector<std::string> parts;
    auto start = path.begin();
    auto it = path.begin();
    for (; it != path.end(); ++it) {
        if (*it == delimiter) {
            parts.emplace_back(start, it);
            start = std::next(it);
        }
    }
    if (start != it) {
        parts.emplace_back(start, it);
    }
    return parts;
}

bool is_parent_of(const std::filesystem::path &parent, const std::filesystem::path &path) {
    const std::filesystem::path &c_parent = std::filesystem::weakly_canonical(parent);
    const std::filesystem::path &c_path = std::filesystem::weakly_canonical(path);
    const std::string &parent_str = c_parent.generic_string();
    const std::string &path_str = c_path.generic_string();
    if (path_str.size() < parent_str.size()) {
        return false;
    }
    return parent_str == path_str.substr(0, parent_str.size());
}

std::optional<std::filesystem::path> get_full_path_of_module(const std::string &command) {
    const char *path_env = std::getenv("PATH");
    if (!path_env) {
        return std::nullopt;
    }

    const std::string path_str(path_env);
#ifdef __WIN32__
    const std::vector<std::string> dirs = split_path(path_str, ';');
#else
    const std::vector<std::string> dirs = split_path(path_str, ':');
#endif

    for (const std::string &dir : dirs) {
        std::filesystem::path full_path = dir;
        full_path = full_path / command;

        // On Windows, append .exe if not already present
#ifdef __WIN32__
        if (command.substr(command.size() - 4) != ".exe") {
            full_path += ".exe";
        }
#endif

        if (access(full_path.string().data(), X_OK) == 0) {
            return full_path;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> FIP::get_fip_path() {
    // The fip path can be found be going through the main file's directories up further and further until we reach the current working
    // directory. The first directory up from the `main_file_path` which contains a `.fip` directory will be the fip path (including the
    // `.fip` directory of course)
    const std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path dir_to_search = std::filesystem::is_directory(main_file_path) ? main_file_path : main_file_path.parent_path();
    while (dir_to_search != cwd.parent_path()) {
        if (std::filesystem::exists(dir_to_search / ".fip")) {
            return dir_to_search / ".fip";
        } else {
            dir_to_search = dir_to_search.parent_path();
        }
    }
    return std::nullopt;
}

bool FIP::init(                //
    const Hash &file_hash,     //
    const unsigned int line,   //
    const unsigned int column, //
    const unsigned int length  //
) {
    PROFILE_SCOPE("FIP init");
    if (is_active) {
        // Initializing an active FIP is considered an error case as this should not happen, but it's a me-problem, not a user problem
        assert(false);
        return false;
    }
    is_active = true;

    // Parse the config file (fip.toml)
    const std::optional<std::filesystem::path> fip_path = get_fip_path();
    if (!fip_path.has_value()) {
        is_active = false;
        THROW_ERR(ErrNoFipDirectoryFound, ERR_PARSING, file_hash, line, column, length);
        return false;
    }
    const std::string fip_config_path_string = (fip_path.value() / "config" / "fip.toml").string();
    fip_master_config_t config_file = fip_master_load_config(fip_config_path_string.data());
    bool needs_shutdown = true;
    if (config_file.ok) {
        // Next we check whether there are any active modules in the config file, if there are not then we can shut down
        if (config_file.enabled_count > 0) {
            // Now we check if all of the enabled modules even exist in the PATH. If an enabled module does not  exist then we report it and
            // shut down too. If all exist, however, we stay active
            needs_shutdown = false;
            for (uint8_t i = 0; i < config_file.enabled_count; i++) {
                std::string module = std::string(config_file.enabled_modules[i]);
#ifdef __WIN32__
                module += ".exe";
#endif
                if (!get_full_path_of_module(module).has_value()) {
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

    // Start all enabled interop modules
    for (uint8_t i = 0; i < config_file.enabled_count; i++) {
        const char *mod = config_file.enabled_modules[i];
        fip_print(0, FIP_INFO, "Starting the %s module...", mod);
        const std::filesystem::path module_path = get_full_path_of_module(std::string(mod)).value();
        fip_spawn_interop_module(&modules, fip_path.value().parent_path().string().data(), module_path.string().data());
    }

    // Initialize the master
    if (!fip_master_init(&modules)) {
        fip_print(0, FIP_ERROR, "Failed to initialize master, exiting");
        abort();
    }

    // Give the Interop Modules time to connect
    fip_print(0, FIP_INFO, "Waiting for interop modules to connect...");
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
        // (for example when no fip.toml was provided or when no extern definitions were found)
        return;
    }
    PROFILE_SCOPE("FIP shutdown");
    fip_free_msg(&message);
    message.type = FIP_MSG_KILL;
    message.u.kill.reason = FIP_KILL_FINISH;
#ifndef __WIN32__
    auto sleep_100ms = timespec{.tv_sec = 0, .tv_nsec = 100000000};
    nanosleep(&sleep_100ms, NULL);
#endif
    fip_master_broadcast_message(buffer, &message);

    // Clean up
#ifndef __WIN32__
    nanosleep(&sleep_100ms, NULL);
#endif
    fip_master_cleanup();
    fip_terminate_all_slaves(&modules); // Fallback cleanup

    fip_print(0, FIP_INFO, "Master shutting down");
    is_active = false;
}

bool FIP::convert_type(fip_type_t *dest, const std::shared_ptr<Type> &src, const bool is_mutable) {
    dest->is_mutable = is_mutable;
    switch (src->get_variation()) {
        default:
            // Unsupported type, return false
            return false;
        case Type::Variation::PRIMITIVE: {
            const auto *type = src->as<PrimitiveType>();
            dest->type = FIP_TYPE_PRIMITIVE;

            // Map primitive type string to enum
            const std::string type_str = type->to_string();
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
        }
        case Type::Variation::MULTI: {
            const auto *type = src->as<MultiType>();
            // A multi-type will be handled just as a struct type and nothing more
            dest->type = FIP_TYPE_STRUCT;
            dest->u.struct_t.field_count = static_cast<uint8_t>(type->width);
            dest->u.struct_t.fields = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t) * type->width));
            for (uint8_t i = 0; i < dest->u.struct_t.field_count; i++) {
                if (!convert_type(&dest->u.struct_t.fields[i], type->base_type, true)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::DATA: {
            const auto *type = src->as<DataType>();
            // A data-type is just a struct and will be handled as such
            const DataNode *data_node = type->data_node;
            dest->type = FIP_TYPE_STRUCT;
            dest->u.struct_t.field_count = static_cast<uint8_t>(data_node->fields.size());
            dest->u.struct_t.fields = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t) * data_node->fields.size()));
            for (uint8_t i = 0; i < dest->u.struct_t.field_count; i++) {
                if (!convert_type(&dest->u.struct_t.fields[i], data_node->fields.at(i).second, true)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::TUPLE: {
            const auto *type = src->as<TupleType>();
            // A tuple-type is just a struct under the hood annyway
            dest->type = FIP_TYPE_STRUCT;
            dest->u.struct_t.field_count = static_cast<uint8_t>(type->types.size());
            dest->u.struct_t.fields = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t) * type->types.size()));
            for (uint8_t i = 0; i < dest->u.struct_t.field_count; i++) {
                if (!convert_type(&dest->u.struct_t.fields[i], type->types.at(i), true)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::POINTER: {
            const auto *type = src->as<PointerType>();
            // A pointer type is essentially just the base type encoded but with a pointer type tag.
            dest->type = FIP_TYPE_PTR;
            dest->u.ptr.base_type = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t)));
            if (!convert_type(dest->u.ptr.base_type, type->base_type, is_mutable)) {
                return false;
            }
            return true;
        }
        case Type::Variation::ENUM: {
            const auto *type = src->as<EnumType>();
            const EnumNode *enum_node = type->enum_node;
            dest->type = FIP_TYPE_ENUM;
            // TODO: For now all enums in Flint are i32
            dest->u.enum_t.bit_width = 32;
            dest->u.enum_t.is_signed = true;
            assert(enum_node->values.size() <= 255);
            dest->u.enum_t.value_count = type->enum_node->values.size();
            dest->u.enum_t.values = static_cast<size_t *>(malloc(sizeof(size_t) * dest->u.enum_t.value_count));
            for (size_t i = 0; i < enum_node->values.size(); i++) {
                dest->u.enum_t.values[i] = enum_node->values.at(i).second;
            }
            return true;
        }
    }
}

bool FIP::resolve_function(FunctionNode *function) {
    PROFILE_SCOPE("FIP resolve '" + function->name + "'");
    PROFILE_CUMULATIVE("FIP::resolve_function");
    if (!is_active) {
        // Try to initialize the FIP if it's not running yet
        if (!init(function->file_hash, function->line, function->column, function->length)) {
            // Initialization failed for some reason
            return false;
        }
        if (!is_active) {
            // We need an error here specifically because we try to resolve an external function without the FIP running, which is not
            // possible. This could happen because no module is active, for example
            THROW_ERR(ErrExternWithoutFip, ERR_PARSING, function->file_hash, function->line, function->column, function->length);
            return false;
        }
    }
    // Don't resolve functions which are defined inside the `.fip` path, as all files in there are generated by the compiler by fip and they
    // are generated from module tag requests, so all extern-defined functions inside these directories *definitely* do exist
    const std::filesystem::path rel = std::filesystem::relative(function->file_hash.path, get_fip_path().value());
    if (is_parent_of(get_fip_path().value(), function->file_hash.path)) {
        return true;
    }
    // This function is not concurrency-safe. FIP assumes a strict order for the master's messages so only one thread is allowed to send /
    // recieve messages to and from the FIP at a time, that's why we have a mutex here
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

    std::shared_ptr<Type> ret_type;
    if (!function->return_types.empty()) {
        if (function->return_types.size() > 1) {
            ret_type = std::make_shared<TupleType>(function->return_types);
            Namespace *file_namespace = Resolver::get_namespace_from_hash(function->file_hash);
            if (!file_namespace->add_type(ret_type)) {
                ret_type = file_namespace->get_type_from_str(ret_type->to_string()).value();
            }
        } else {
            ret_type = function->return_types.front();
        }
        msg.u.sym_req.sig.fn.rets_len = 1; // We *always* have only one return type, being either the value directly or a struct
        msg.u.sym_req.sig.fn.rets = static_cast<fip_type_t *>(malloc(sizeof(fip_type_t)));
        if (!convert_type(&msg.u.sym_req.sig.fn.rets[0], ret_type, true)) {
            const std::string type_str = ret_type->to_string();
            fip_print(0, FIP_ERROR, "Type '%s' not compatible with FIP", type_str.c_str());
            return false;
        }
    }

    if (!function->parameters.empty()) {
        const uint8_t args_len = static_cast<uint8_t>(function->parameters.size());
        msg.u.sym_req.sig.fn.args_len = args_len;
        msg.u.sym_req.sig.fn.args = static_cast<fip_sig_fn_arg_t *>(malloc(sizeof(fip_sig_fn_arg_t) * args_len));
        for (uint8_t i = 0; i < args_len; i++) {
            const auto &param = function->parameters.at(i);
            if (!convert_type(&msg.u.sym_req.sig.fn.args[i].type, std::get<0>(param), std::get<2>(param))) {
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

bool FIP::resolve_module_import(ImportNode *import) {
    assert(!std::holds_alternative<Hash>(import->path));
    const std::vector<std::string> &import_path = std::get<std::vector<std::string>>(import->path);
    assert(import_path.size() == 2);
    const std::string module_tag = import_path.back();
    PROFILE_SCOPE("FIP resolve import 'Fip." + module_tag + "'");
    PROFILE_CUMULATIVE("FIP::resolve_module_import");
    if (!is_active) {
        // Try to initialize the FIP if it's not running yet
        if (!init(import->file_hash, import->line, import->column, import->length)) {
            // Initialization failed for some reason
            return false;
        }
        if (!is_active) {
            // We need an error here specifically because we try to resolve an external function without the FIP running, which is not
            // possible. This could happen because no module is active, for example
            THROW_ERR(ErrUseWithoutFip, ERR_PARSING, import->file_hash, import->line, import->column, import->length, module_tag);
            return false;
        }
    }
    // Generate the hash from the module file path and set the path of the import node accordingly
    const std::filesystem::path module_file_path = get_fip_path().value() / "generated" / (module_tag + ".ft");
    const Hash module_hash(module_file_path);
    import->path = module_hash;
    // Get the signature list from the tag request
    fip_msg_t msg{};
    msg.type = FIP_MSG_TAG_REQUEST;
    memcpy(msg.u.tag_req.tag, module_tag.data(), module_tag.size());
    fip_tag_request_result_t result = fip_master_tag_request(buffer, &msg);
    switch (result.status) {
        case FIP_TAG_REQUEST_STATUS_OK:
            return generate_bindings_file(result.list, module_tag);
        case FIP_TAG_REQUEST_STATUS_ERR_FAULTY:
            THROW_BASIC_ERR(ERR_PARSING);
            fip_free_sig_list(result.list);
            return false;
        case FIP_TAG_REQUEST_STATUS_ERR_UNKNOWN_TAG:
            THROW_ERR(                                                                                                        //
                ErrUnknownModuleTag, ERR_PARSING, import->file_hash, import->line, import->column, import->length, module_tag //
            );
            fip_free_sig_list(result.list);
            return false;
        case FIP_TAG_REQUEST_STATUS_ERR_AMBIGUOUS_TAG:
            THROW_ERR(                                                                                                          //
                ErrAmbiguousModuleTag, ERR_PARSING, import->file_hash, import->line, import->column, import->length, module_tag //
            );
            fip_free_sig_list(result.list);
            return false;
        case FIP_TAG_REQUEST_STATUS_ERR_WRITE:
            THROW_BASIC_ERR(ERR_PARSING);
            fip_free_sig_list(result.list);
            return false;
    }
}

bool FIP::generate_bindings_file(fip_sig_list_t *list, const std::string &module_tag) {
    const std::filesystem::path fip_path = get_fip_path().value();
    const std::filesystem::path generated_dir = fip_path / "generated";
    if (!std::filesystem::exists(generated_dir)) {
        std::filesystem::create_directory(generated_dir);
    }
    const std::filesystem::path generated_file = generated_dir / (module_tag + ".ft");
    std::ofstream file(generated_file);
    if (!file.is_open()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return false;
    }
    for (size_t i = 0; i < list->count; i++) {
        switch (list->sigs[i].type) {
            case FIP_SYM_UNKNOWN:
                THROW_BASIC_ERR(ERR_PARSING);
                break;
            case FIP_SYM_FUNCTION: {
                const fip_sig_fn_t *const f = &list->sigs[i].sig.fn;
                file << "extern def " << f->name << "(";
                for (size_t j = 0; j < f->args_len; j++) {
                    if (j > 0) {
                        file << ", ";
                    }
                    if (f->args[j].type.is_mutable) {
                        file << "mut ";
                    } else {
                        file << "const ";
                    }
                    generate_fip_type(&f->args[j].type, file);
                    file << " " << f->args[j].name;
                }
                file << ")";
                if (f->rets_len > 0) {
                    file << " -> ";
                }
                if (f->rets_len > 1) {
                    file << "(";
                }
                for (size_t j = 0; j < f->rets_len; j++) {
                    if (j > 0) {
                        file << ", ";
                    }
                    generate_fip_type(&f->rets[j], file);
                }
                if (f->rets_len > 1) {
                    file << ")";
                }
                file << ";\n\n";
                break;
            }
            case FIP_SYM_DATA: {
                const fip_sig_data_t *const d = &list->sigs[i].sig.data;
                file << "data " << d->name << ":\n";
                for (size_t j = 0; j < d->value_count; j++) {
                    file << "\t";
                    generate_fip_type(&d->value_types[j], file);
                    file << " " << d->value_names[j] << ";\n";
                    fip_free_type(&d->value_types[j]);
                }
                file << "\t" << d->name << "(";
                for (size_t j = 0; j < d->value_count; j++) {
                    if (j > 0) {
                        file << ", ";
                    }
                    file << d->value_names[j];
                    free(d->value_names[j]);
                }
                file << ");\n";
                file << "\n";
                free(d->value_types);
                free(d->value_names);
                break;
            }
            case FIP_SYM_ENUM: {
                const fip_sig_enum_t *const e = &list->sigs[i].sig.enum_t;
                file << "enum " << e->name << ":\n";
                for (size_t j = 0; j < e->value_count; j++) {
                    file << "\t" << e->tags[j] << " = " << e->values[j];
                    if (j + 1 == e->value_count) {
                        file << ";\n";
                    } else {
                        file << ",\n";
                    }
                    free(e->tags[j]);
                }
                file << "\n";
                free(e->tags);
                free(e->values);
                break;
            }
        }
    }
    file.close();
    free(list);
    return true;
}

void FIP::generate_fip_type(fip_type_t *type, std::ofstream &file) {
    switch (type->type) {
        case FIP_TYPE_PRIMITIVE:
            switch (type->u.prim) {
                case FIP_VOID:
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    break;
                case FIP_U8:
                    file << "u8";
                    break;
                case FIP_U16:
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    break;
                case FIP_U32:
                    file << "u32";
                    break;
                case FIP_U64:
                    file << "u64";
                    break;
                case FIP_I8:
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    break;
                case FIP_I16:
                    THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                    break;
                case FIP_I32:
                    file << "i32";
                    break;
                case FIP_I64:
                    file << "i64";
                    break;
                case FIP_F32:
                    file << "f32";
                    break;
                case FIP_F64:
                    file << "f64";
                    break;
                case FIP_BOOL:
                    file << "bool";
                    break;
                case FIP_STR:
                    file << "str";
                    break;
            }
            break;
        case FIP_TYPE_PTR:
            generate_fip_type(type->u.ptr.base_type, file);
            file << "*";
            break;
        case FIP_TYPE_STRUCT: {
            const fip_type_struct_t *s = &type->u.struct_t;
            file << "data<";
            for (size_t i = 0; i < s->field_count; i++) {
                if (i > 0) {
                    file << ", ";
                }
                generate_fip_type(&s->fields[i], file);
            }
            file << ">";
            break;
        }
        case FIP_TYPE_RECURSIVE:
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            break;
        case FIP_TYPE_ENUM: {
            const fip_type_enum_t *e = &type->u.enum_t;
            const size_t type_len = strlen(e->name);
            if (type_len > 0) {
                file << e->name;
            } else {
                file << "enum(";
                file << (e->is_signed ? "i" : "u");
                file << std::to_string(e->bit_width);
                file << "){";
                for (size_t i = 0; i < e->value_count; i++) {
                    if (i > 0) {
                        file << ", ";
                    }
                    file << e->values[i];
                }
                file << "}";
            }
            break;
        }
    }
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

std::optional<std::vector<std::array<char, 9>>> FIP::gather_objects() {
    if (!is_active) {
        // No error, just return an empty list
        return std::vector<std::array<char, 9>>{};
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
            if (response->u.obj_res.compilation_failed) {
                THROW_ERR(ErrExternCompilationFailed, ERR_GENERATING);
                return std::nullopt;
            }
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
