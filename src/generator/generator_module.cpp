#include "generator/generator.hpp"

#include "error/error.hpp"
#include "globals.hpp"
#include "linker/linker.hpp"
#include "profiler.hpp"

#include <json/parser.hpp>

void Generator::Module::generate_dima_heads(llvm::Module *module, const std::string &module_name) {
    llvm::StructType *head_type = type_map.at("__flint_type_dima_head");
    llvm::ConstantPointerNull *nullpointer = llvm::ConstantPointerNull::get(head_type->getPointerTo());
    Hash file_hash(module_name);
    for (const auto &data_type_tuple : core_module_data_types.at(module_name)) {
        const std::string data_node_name(std::get<0>(data_type_tuple));
        const std::string head_var_str = file_hash.to_string() + ".dima.head.data." + data_node_name;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
        llvm::GlobalVariable *head_variable = new llvm::GlobalVariable(                                             //
            *module, head_type->getPointerTo(), false, llvm::GlobalValue::WeakODRLinkage, nullpointer, head_var_str //
        );
#pragma GCC diagnostic pop
        const std::string heads_key = file_hash.to_string() + "." + data_node_name;
        DIMA::dima_heads[heads_key] = head_variable;
    }
}

bool Generator::Module::generate_module(     //
    const BuiltinLibrary lib_to_build,       //
    const std::filesystem::path &cache_path, //
    const std::string &module_name           //
) {
    PROFILE_SCOPE("Generating module '" + module_name + "'");
    generating_builtin_module = true;
    auto builder = std::make_unique<llvm::IRBuilder<>>(context);
    auto module = std::make_unique<llvm::Module>(module_name, context);
    IR::init_builtin_types();
    switch (lib_to_build) {
        case BuiltinLibrary::PRINT:
            Builtin::generate_c_functions(module.get());
            Print::generate_print_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::STR:
            Builtin::generate_c_functions(module.get());
            String::generate_string_manip_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::CAST:
            Builtin::generate_c_functions(module.get());
            String::generate_string_manip_functions(builder.get(), module.get(), true);
            TypeCast::generate_typecast_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::ARITHMETIC:
            Builtin::generate_c_functions(module.get());
            Arithmetic::generate_arithmetic_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::ARRAY:
            Builtin::generate_c_functions(module.get());
            Array::generate_array_manip_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::READ:
            Builtin::generate_c_functions(module.get());
            String::generate_string_manip_functions(builder.get(), module.get(), true);
            Read::generate_read_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::ASSERT:
            String::generate_string_manip_functions(builder.get(), module.get(), true);
            Assert::generate_assert_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::FILESYSTEM:
            Builtin::generate_c_functions(module.get());
            String::generate_string_manip_functions(builder.get(), module.get(), true);
            Array::generate_array_manip_functions(builder.get(), module.get(), true);
            FileSystem::generate_filesystem_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::ENV:
            Builtin::generate_c_functions(module.get());
            String::generate_string_manip_functions(builder.get(), module.get(), true);
            Env::generate_env_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::SYSTEM:
            Builtin::generate_c_functions(module.get());
            String::generate_string_manip_functions(builder.get(), module.get(), true);
            System::generate_system_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::MATH:
            Builtin::generate_c_functions(module.get());
            Math::generate_math_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::PARSE:
            Builtin::generate_c_functions(module.get());
            String::generate_string_manip_functions(builder.get(), module.get(), true);
            Parse::generate_parse_functions(builder.get(), module.get(), false);
            break;
        case BuiltinLibrary::TIME: {
            Builtin::generate_c_functions(module.get());
            DIMA::generate_dima_functions(builder.get(), module.get(), true, true);
            generate_dima_heads(module.get(), "time");
            Time::generate_time_functions(builder.get(), module.get(), false);
            DIMA::dima_heads.clear();
            Time::time_data_types.clear();
            break;
        }
        case BuiltinLibrary::DIMA:
            Builtin::generate_c_functions(module.get());
            DIMA::generate_dima_functions(builder.get(), module.get(), true);
            break;
    }

    // Verify the module after generation
    if (!verify_module(module.get())) {
        THROW_BASIC_ERR(ERR_GENERATING);
        return false;
    }
    // Print the module, if requested
    if (DEBUG_MODE && (BUILTIN_LIBS_TO_PRINT & static_cast<unsigned int>(lib_to_build))) {
        std::cout << YELLOW << "[Debug Info] Generated module '" << module_name << "':\n"
                  << DEFAULT << resolve_ir_comments(get_module_ir_string(module.get())) << std::endl;
    }
    // Save the generated module at the module_path
    bool compilation_successful = compile_module(module.get(), cache_path / module_name);
    module.reset();
    builder.reset();
    global_strings.clear();
    generating_builtin_module = false;
    if (!compilation_successful) {
        std::cerr << "Error: Failed to generate builtin module '" << module_name << "'" << std::endl;
    }
    return compilation_successful;
}

bool Generator::Module::generate_modules() {
    std::filesystem::path cache_path = get_flintc_cache_path();

    // Check if the files need to be rebuilt at all
    const unsigned int which_need_rebuilding = which_modules_to_rebuild();
    // If no files need to be rebuilt, we can directly return true
    if (which_need_rebuilding == 0) {
        return true;
    }

    bool success = true;
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::PRINT)) {
        success = success && generate_module(BuiltinLibrary::PRINT, cache_path, "print");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::STR)) {
        success = success && generate_module(BuiltinLibrary::STR, cache_path, "str");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::CAST)) {
        success = success && generate_module(BuiltinLibrary::CAST, cache_path, "cast");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::ARITHMETIC)) {
        success = success && generate_module(BuiltinLibrary::ARITHMETIC, cache_path, "arithmetic");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::ARRAY)) {
        success = success && generate_module(BuiltinLibrary::ARRAY, cache_path, "array");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::READ)) {
        success = success && generate_module(BuiltinLibrary::READ, cache_path, "read");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::ASSERT)) {
        success = success && generate_module(BuiltinLibrary::ASSERT, cache_path, "assert");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::FILESYSTEM)) {
        success = success && generate_module(BuiltinLibrary::FILESYSTEM, cache_path, "filesystem");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::ENV)) {
        success = success && generate_module(BuiltinLibrary::ENV, cache_path, "env");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::SYSTEM)) {
        success = success && generate_module(BuiltinLibrary::SYSTEM, cache_path, "system");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::MATH)) {
        success = success && generate_module(BuiltinLibrary::MATH, cache_path, "math");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::PARSE)) {
        success = success && generate_module(BuiltinLibrary::PARSE, cache_path, "parse");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::TIME)) {
        success = success && generate_module(BuiltinLibrary::TIME, cache_path, "time");
    }
    if (which_need_rebuilding & static_cast<unsigned int>(BuiltinLibrary::DIMA)) {
        success = success && generate_module(BuiltinLibrary::DIMA, cache_path, "dima");
    }
    if (!success) {
        return false;
    }

    // Then, save the new metadata file
    save_metadata_json_file(static_cast<int>(overflow_mode), static_cast<int>(oob_mode));

    // Now, merge together all object files into one single .o / .obj file
    std::string file_ending = "";
    switch (COMPILATION_TARGET) {
        case Target::NATIVE:
#ifdef __WIN32__
            file_ending = ".obj";
#else
            file_ending = ".o";
#endif
            break;
        case Target::LINUX:
            file_ending = ".o";
            break;
        case Target::WINDOWS:
            file_ending = ".obj";
            break;
    }
    std::vector<std::filesystem::path> libs;
    libs.emplace_back(cache_path / ("print" + file_ending));
    libs.emplace_back(cache_path / ("str" + file_ending));
    libs.emplace_back(cache_path / ("cast" + file_ending));
    libs.emplace_back(cache_path / ("arithmetic" + file_ending));
    libs.emplace_back(cache_path / ("array" + file_ending));
    libs.emplace_back(cache_path / ("read" + file_ending));
    libs.emplace_back(cache_path / ("assert" + file_ending));
    libs.emplace_back(cache_path / ("filesystem" + file_ending));
    libs.emplace_back(cache_path / ("env" + file_ending));
    libs.emplace_back(cache_path / ("system" + file_ending));
    libs.emplace_back(cache_path / ("math" + file_ending));
    libs.emplace_back(cache_path / ("parse" + file_ending));
    libs.emplace_back(cache_path / ("time" + file_ending));
    libs.emplace_back(cache_path / ("dima" + file_ending));

    // Delete the old `builtins.` o / obj file before creating a new one
    std::filesystem::path builtins_path = cache_path / ("builtins" + file_ending);
    if (std::filesystem::exists(builtins_path)) {
        std::filesystem::remove(builtins_path);
    }

    // Create the static .a file from all `.o` files
    Profiler::start_task("Creating static library libbuiltins.a");
    bool merge_success = Linker::create_static_library(libs, cache_path / "libbuiltins");
    Profiler::end_task("Creating static library libbuiltins.a");
    return merge_success;
}

unsigned int Generator::Module::which_modules_to_rebuild() {
    // First, all modules need to be rebuilt which are requested to be printed
    unsigned int needed_rebuilds = BUILTIN_LIBS_TO_PRINT;

    // Then, we parse the metadata.json file in the cache directory, or if it doesnt exist, create it
    const std::filesystem::path cache_path = get_flintc_cache_path();
    const std::filesystem::path metadata_file = cache_path / "metadata.json";
    if (!std::filesystem::exists(metadata_file)) {
        // If no metadata file existed, we need to re-build everything as we cannot be sure with which settings the .o files were built the
        // last time
        if (DEBUG_MODE) {
            std::cout << YELLOW << "[Debug Info] Rebuilding all library files because no metadata.json file was found\n" << DEFAULT;
            std::cout << "-- overflow_mode: " << static_cast<unsigned int>(overflow_mode) << "\n" << std::endl;
        }
        save_metadata_json_file(static_cast<int>(overflow_mode), static_cast<int>(oob_mode));
        return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
    }

    std::vector<JsonToken> tokens = JsonLexer::scan(metadata_file);
    std::optional<std::unique_ptr<JsonObject>> metadata = JsonParser::parse(tokens);
    if (!metadata.has_value()) {
        // Failed to parse the metadata, so we create the current metadata json file
        save_metadata_json_file(static_cast<int>(overflow_mode), static_cast<int>(oob_mode));
        // We dont know the settings of the old metadata.json file, so we rebuild everything
        return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
    }

    // Read all the values from the metadata
    const auto main_group = dynamic_cast<const JsonGroup *>(metadata.value().get());
    if (main_group == nullptr || main_group->name != "__ROOT__") {
        THROW_BASIC_ERR(ERR_GENERATING);
        // Set all bits to 1, e.g. rebuild everything
        return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
    }
    for (const auto &field : main_group->fields) {
        if (const JsonString *json_string = dynamic_cast<const JsonString *>(field.get())) {
            if (json_string->name == "commit_hash") {
                if (json_string->value != COMMIT_HASH_VALUE) {
                    return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
                }
                continue;
            } else {
                // Unknown bare string field in the metadata.json file
                THROW_BASIC_ERR(ERR_GENERATING);
                return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
            }
        }
        const auto group_value = dynamic_cast<const JsonGroup *>(field.get());
        if (group_value == nullptr) {
            THROW_BASIC_ERR(ERR_GENERATING);
            // Set all bits to 1, e.g. rebuild everything
            return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
        }
        if (group_value->name == "arithmetic") {
            // For now, we can assume that it contains a single value
            const auto metadata_overflow_mode = dynamic_cast<const JsonNumber *>(group_value->fields.at(0).get());
            if (metadata_overflow_mode == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
                // Set all bits to 1, e.g. rebuild everything
                return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
            }
            if (metadata_overflow_mode->number != static_cast<int>(overflow_mode)) {
                // We need to rebuild the arithmetic.o file if the overflow modes dont match up
                needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::ARITHMETIC);
            }
        } else if (group_value->name == "array") {
            const auto metadata_oob_mode = dynamic_cast<const JsonNumber *>(group_value->fields.at(0).get());
            if (metadata_oob_mode == nullptr) {
                THROW_BASIC_ERR(ERR_GENERATING);
                // Set all bits to 1, e.g. rebuild everything
                return static_cast<unsigned int>(0) - static_cast<unsigned int>(1);
            }
            if (metadata_oob_mode->number != static_cast<int>(oob_mode)) {
                // We need to rebuild the arithmetic.o file if the overflow modes dont match up
                needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::ARRAY);
                needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::STR);
            }
        }
    }

    // Check which object files exist. If any does not exist, it needs to be rebuilt
#ifdef __WIN32__
    const std::string file_ending = ".obj";
#else
    const std::string file_ending = ".o";
#endif
    if (!std::filesystem::exists(cache_path / ("print" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::PRINT);
    }
    if (!std::filesystem::exists(cache_path / ("str" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::STR);
    }
    if (!std::filesystem::exists(cache_path / ("cast" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::CAST);
    }
    if (!std::filesystem::exists(cache_path / ("arithmetic" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::ARITHMETIC);
    }
    if (!std::filesystem::exists(cache_path / ("array" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::ARRAY);
    }
    if (!std::filesystem::exists(cache_path / ("read" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::READ);
    }
    if (!std::filesystem::exists(cache_path / ("assert" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::ASSERT);
    }
    if (!std::filesystem::exists(cache_path / ("filesystem" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::FILESYSTEM);
    }
    if (!std::filesystem::exists(cache_path / ("env" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::ENV);
    }
    if (!std::filesystem::exists(cache_path / ("system" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::SYSTEM);
    }
    if (!std::filesystem::exists(cache_path / ("math" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::MATH);
    }
    if (!std::filesystem::exists(cache_path / ("parse" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::PARSE);
    }
    if (!std::filesystem::exists(cache_path / ("time" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::TIME);
    }
    if (!std::filesystem::exists(cache_path / ("dima" + file_ending))) {
        needed_rebuilds |= static_cast<unsigned int>(BuiltinLibrary::DIMA);
    }
    return needed_rebuilds;
}

void Generator::Module::save_metadata_json_file(int overflow_mode_value, int oob_mode_value) {
    std::unique_ptr<JsonObject> commit_hash_object = std::make_unique<JsonString>("commit_hash", COMMIT_HASH_VALUE);

    std::unique_ptr<JsonObject> overflow_mode_object = std::make_unique<JsonNumber>("overflow_mode", overflow_mode_value);
    std::vector<std::unique_ptr<JsonObject>> arithmetic_group_content;
    arithmetic_group_content.emplace_back(std::move(overflow_mode_object));
    std::unique_ptr<JsonObject> arithmetic_group = std::make_unique<JsonGroup>("arithmetic", arithmetic_group_content);

    std::unique_ptr<JsonObject> oob_mode_object = std::make_unique<JsonNumber>("oob_mode", oob_mode_value);
    std::vector<std::unique_ptr<JsonObject>> array_group_content;
    array_group_content.emplace_back(std::move(oob_mode_object));
    std::unique_ptr<JsonObject> array_group = std::make_unique<JsonGroup>("array", array_group_content);

    std::vector<std::unique_ptr<JsonObject>> main_object_content;
    main_object_content.emplace_back(std::move(commit_hash_object));
    main_object_content.emplace_back(std::move(arithmetic_group));
    main_object_content.emplace_back(std::move(array_group));
    std::unique_ptr<JsonObject> main_object = std::make_unique<JsonGroup>("__ROOT__", main_object_content);

    std::string main_object_string = JsonParser::to_string(main_object.get());
    main_object.reset();

    const std::filesystem::path metadata_file = get_flintc_cache_path() / "metadata.json";
    if (std::filesystem::exists(metadata_file)) {
        std::filesystem::remove(metadata_file);
    }

    // Save the main_object_string to the file
    std::ofstream file_stream(metadata_file.string());
    file_stream << main_object_string;
    file_stream.flush();
    file_stream.close();
}
