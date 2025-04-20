#pragma once

#include <filesystem>
#include <vector>

/// @class `Linker`
/// @brief The linker is responsible for linking .o files together to libraries or executables
class Linker {
  public:
    Linker() = delete;

    /// @function `link`
    /// @brief Links together the given `obj_file` with all the needed libraries and creates an executable at the `output_file`
    ///
    /// @param `obj_file` The path to the object file to link and create the executable with
    /// @param `output_file` The path to the executable to create
    /// @param `is_static` Whether the executable should be static
    /// @return `bool` Whether linking and creation of the executable was successful
    static bool link(const std::filesystem::path &obj_file, const std::filesystem::path &output_file, const bool is_static);

    /// @function `create_static_library`
    /// @brief Creates a static `.a` library from the given `.o` files
    ///
    /// @param `obj_files` All the object files to statically link to an `.a` library
    /// @param `output_file` The path where to save the statically linked `.a` library at
    /// @return `bool` Whether creation of the static library was successful
    static bool create_static_library(const std::vector<std::filesystem::path> &obj_files, const std::filesystem::path &output_file);
};
