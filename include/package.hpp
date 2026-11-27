#pragma once

#include "assert.hpp"
#include "io.hpp"
#include "profiler.hpp"
#include "tomlc17.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

/// @struct `PackageRoot`
/// @brief Struct which represents the root of a given Flint package
struct PackageRoot {
    /// @var `root`
    /// @brief The root of the package contianing the `flint.toml` file
    std::filesystem::path root;

    /// @var `deps`
    /// @brief A map of all dependencies declared in the `flint.toml` file
    std::unordered_map<std::string, PackageRoot *> deps;

    /// @var `lib_file`
    /// @brief The 'lib' file declared in the dependency
    std::optional<std::filesystem::path> lib_file;

    /// @var `roots`
    /// @brief A list of all roots of this project. Every parsed file will point to one specific PackageRoot
    static inline std::vector<std::unique_ptr<PackageRoot>> roots;

    /// @function `init`
    /// @brief Initializes all packages by parsing the main `flint.toml` file
    static bool init() {
        PROFILE_SCOPE("PackageRoot::init");
        const std::filesystem::path cwd = std::filesystem::current_path();
        const std::filesystem::path main_toml_path = cwd / "flint.toml";
        if (!std::filesystem::exists(main_toml_path)) {
            // This project does not contain a 'flint.toml' file and thus does not have any dependencies
            return true;
        }

        const std::optional<PackageRoot> &result = parse_toml(main_toml_path);
        if (!result.has_value()) {
            return false;
        }
        roots.insert(roots.begin(), std::make_unique<PackageRoot>(std::move(result.value())));
        return true;
    }

    /// @function `get_from_path`
    /// @brief Returns the package root of the given Flint file path
    ///
    /// @param `file_path` The path to the `.ft` file currently being looked at
    /// @return `std::optional<PackageRoot *>` A pointer to the package root if one was found for this file, nullopt if not found
    static std::optional<PackageRoot *> get_from_path(const std::filesystem::path &file_path) {
        // First check if there exists a `<project_root>/flint/.pkg` directory. If it does not exist we always are at the "main" root
        const std::filesystem::path cwd = std::filesystem::current_path();
        const std::filesystem::path pkg_path = cwd / "flint" / ".pkg";
        if (!std::filesystem::exists(pkg_path)) {
            if (roots.empty()) {
                return std::nullopt;
            } else {
                return roots.front().get();
            }
        }

        // Check if we are inside the `<project_root>/flint/.pkg` directory. If we are not within that directory it means that we are not
        // looking at a file from any dependency but at the main file / main project instead. So, we just return the main PackageRoot, if it
        // exists
        if (!IO::is_parent_of(pkg_path, file_path)) {
            if (roots.empty()) {
                return std::nullopt;
            } else {
                return roots.front().get();
            }
        }

        // Find the correct package root and return it
        ASSERT(!roots.empty());
        for (auto it = roots.begin() + 1; it != roots.end(); ++it) {
            if (IO::is_parent_of((*it)->root, file_path)) {
                return (*it).get();
            }
        }
        return std::nullopt;
    }

    /// @function `get_lib_file`
    /// @brief Returns the path to the 'lib' file of the dependency from the given 'file_path', to simplify library import resolving
    ///
    /// @param `file_path` The Flint file from which to get the path to one of its dependencies
    /// @param `dep_name` The dependency defined in the 'flint.toml' scoped to the 'file_path' Flint file
    /// @return `std::optional<std::filesystem::path>` The path to the 'lib' file defined in the 'flint.toml' file associated with the
    ///                                                'file_path' Flint file, nullopt of anything failed
    static std::optional<std::filesystem::path> get_lib_file(const std::filesystem::path &file_path, const std::string &dep_name) {
        const std::optional<PackageRoot *> root = get_from_path(file_path);
        if (!root.has_value()) {
            // Dependency 'dep_name' cannot be resolved because the package the file is contained in does not have a 'flint.toml' file
            // defined for it
            THROW_BASIC_ERR(ERR_PARSING);
            return std::nullopt;
        }
        for (const auto &[_dep_name, dep_pkg] : root.value()->deps) {
            if (_dep_name != dep_name) {
                continue;
            }
            if (!dep_pkg->lib_file.has_value()) {
                // Cannot import dependency which does not define a 'lib' file
                THROW_BASIC_ERR(ERR_PARSING);
                return std::nullopt;
            }
            return dep_pkg->lib_file.value();
        }
        // Dependency 'dep_name' not found in 'flint.toml' file of the package
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }

  private:
    static std::optional<PackageRoot> parse_toml(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path)) {
            std::cerr << "Error: Unable to find file: " << path.string() << std::endl;
            return std::nullopt;
        }
        const std::ifstream toml_ifstream(path, std::ios::binary);
        if (!toml_ifstream.is_open()) {
            std::cerr << "Error: Unable to open file: " << path.string() << std::endl;
            return std::nullopt;
        }

        std::stringstream ss;
        ss << toml_ifstream.rdbuf();
        const std::string toml_content = ss.str();
        const toml_result_t result = toml_parse(toml_content.data(), toml_content.length());
        if (!result.ok) {
            std::cerr << "Error: Failed to parse file: " << path.string() << std::endl;
            return std::nullopt;
        }
        const toml_datum_t *root = &result.toptab;
        if (root->type != TOML_TABLE) {
            std::cerr << "Error: Expected toptab to be a table in file: " << path.string() << std::endl;
            return std::nullopt;
        }

        PackageRoot package = {
            .root = path.parent_path(),
            .deps = {},
            .lib_file = std::nullopt,
        };
        const auto *entries = &root->u.tab;
        if (entries->size == 0) {
            return package;
        }
        if (entries->key == NULL or entries->value == NULL) {
            std::cerr << "Error: Cannot parse malformed toml file: " << path.string() << std::endl;
            return std::nullopt;
        }
        for (size_t i = 0; i < static_cast<size_t>(entries->size); i++) {
            const std::string name(entries->key[i]);
            const toml_datum_t *value = &entries->value[i];

            if (value->type != TOML_TABLE) {
                std::cerr << "Expected section '[" << name << "]' to be a table, but got '" << type_name(value->type) << "'" << std::endl;
                return std::nullopt;
            }
            const auto *fields = &value->u.tab;
            if (fields->size == 0) {
                continue;
            }

            if (name == "config") {
                for (size_t j = 0; j < static_cast<size_t>(fields->size); j++) {
                    const std::string field_name(fields->key[j]);
                    const toml_datum_t *field_value = &fields->value[j];

                    if (field_name == "lib") {
                        package.lib_file = package.root / std::string(field_value->u.str.ptr, field_value->u.str.len);
                    }
                }
                continue;
            } else if (name == "Core") {
                std::cerr << "Error: The 'flint.toml' file is not allowed to contain a '[Core]' table" << std::endl;
                return std::nullopt;
            }

            for (size_t j = 0; j < static_cast<size_t>(fields->size); j++) {
                const std::string field_name(fields->key[j]);
                const toml_datum_t *field_value = &fields->value[j];

                if (field_name == "kind") {
                    ASSERT(field_value->type == TOML_STRING);
                    const std::string kind = std::string(field_value->u.str.ptr, field_value->u.str.len);
                    if (kind != "flint") {
                        THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
                        return std::nullopt;
                    }
                } else if (field_name == "sha256") {
                    ASSERT(field_value->type == TOML_STRING);
                    const std::string sha256 = std::string(field_value->u.str.ptr, field_value->u.str.len);
                    ASSERT(sha256.size() == 64);
                    const std::filesystem::path dep_root_dir = std::filesystem::current_path() / "flint" / ".pkg" / sha256;
                    if (!std::filesystem::exists(dep_root_dir)) {
                        std::cerr << "Error: Dependency '" << name << "' defined in '" << path.string() << "' not found.\n";
                        std::cerr << "       Have you executed 'flint fetch' yet?" << std::endl;
                        return std::nullopt;
                    }
                    const std::filesystem::path dep_toml_dir = dep_root_dir / "flint.toml";
                    if (!std::filesystem::exists(dep_toml_dir)) {
                        break;
                    }

                    const std::optional<PackageRoot> dep = parse_toml(dep_toml_dir);
                    if (!dep.has_value()) {
                        return std::nullopt;
                    }
                    roots.emplace_back(std::make_unique<PackageRoot>(std::move(dep.value())));
                    package.deps.emplace(name, roots.back().get());
                }
            }
        }
        return package;
    }

    static std::string_view type_name(const toml_type_t &toml_type) {
        switch (toml_type) {
            case TOML_STRING:
                return "string";
            case TOML_INT64:
                return "number";
            case TOML_FP64:
                return "number";
            case TOML_BOOLEAN:
                return "bool";
            case TOML_ARRAY:
                return "array";
            case TOML_TABLE:
                return "table";
            default:
                return "unknown";
        };
    }
};
