#pragma once

#include <filesystem>
#include <optional>
#include <vector>

/// @class `Linker`
/// @brief The linker is responsible for linking .o files together to libraries or executables
class Linker {
  public:
    Linker() = delete;

    /// @function `link`
    /// @brief Links together the given `obj_file` with all the needed libraries and creates an executable at the `output_file`
    ///
    /// @param `obj_files` The paths to the object files to link together and create the executable with
    /// @param `output_file` The path to the executable to create
    /// @param `flags` The flags used during linking
    /// @param `is_static` Whether the executable should be static
    /// @return `bool` Whether linking and creation of the executable was successful
    static bool link(                                        //
        const std::vector<std::filesystem::path> &obj_files, //
        const std::filesystem::path &output_file,            //
        const std::vector<std::string> &flags,               //
        const bool is_static                                 //
    );

    /// @function `create_static_library`
    /// @brief Creates a static `.a` library from the given `.o` files
    ///
    /// @param `obj_files` All the object files to statically link to an `.a` library
    /// @param `output_file` The path where to save the statically linked `.a` library at
    /// @return `bool` Whether creation of the static library was successful
    static bool create_static_library(const std::vector<std::filesystem::path> &obj_files, const std::filesystem::path &output_file);

  private:
    /// @function `fetch_crt_libs`
    /// @brief Fetches the needed crt libs for windows
    static bool fetch_crt_libs();

    /// @function `get_lib_env_win`
    /// @brief Get the content of the 'LIB' environment variable on Windows
    ///
    /// @return `std::string` The content of the 'LIB' environment variable
    static std::string get_lib_env_win();

    /// @function `get_windows_args`
    /// @brief Get all the windows args to call lld with
    ///
    /// @param `obj_files` The paths to the object files to link together to the executable
    /// @param `output_file` The output file of the link
    /// @param `is_static` Whether the output file should be linked as static
    /// @return `std::optional<std::vector<std::string>>` The list of all arguments with which lld will be called, nullopt if something
    /// failed
    static std::optional<std::vector<std::string>> get_windows_args( //
        const std::vector<std::filesystem::path> &obj_files,         //
        const std::filesystem::path &output_file,                    //
        const bool is_static                                         //
    );

    /// @function `link_windows`
    /// @brief Links the built program for windows and outputs an .exe file
    ///
    /// @param `obj_files` The paths to the object files to link together to the executable
    /// @param `output_file` The output file of the link
    /// @param `flags` The flags with which to link the .obj file to the executable
    /// @param `is_static` Whether the output file should be linked as static
    /// @return `bool` Whether linking the executable was succesful
    static bool link_windows(                                //
        const std::vector<std::filesystem::path> &obj_files, //
        const std::filesystem::path &output_file,            //
        const std::vector<std::string> &flags,               //
        const bool is_static                                 //
    );

    /// @function `get_linux_args`
    /// @brief Get all the linux args to call lld with
    ///
    /// @param `obj_files` The paths to the object files to link together to the executable
    /// @param `output_file` The output file of the link
    /// @param `is_static` Whether the output file should be linked as static
    /// @return `std::vector<std::string>` The list of all arguments with which lld will be called
    static std::optional<std::vector<std::string>> get_linux_args( //
        const std::vector<std::filesystem::path> &obj_files,       //
        const std::filesystem::path &output_file,                  //
        const bool is_static                                       //
    );

    /// @function `link_linux`
    /// @brief Links the built program for linux and outputs a binary file
    ///
    /// @param `obj_files` The paths to the object files to link together to the executable
    /// @param `output_file` The output file of the link
    /// @param `flags` The flags with which to link the .o file to the executable
    /// @param `is_static` Whether the output file should be linked as static
    /// @return `bool` Whether linking the executable was succesful
    static bool link_linux(                                  //
        const std::vector<std::filesystem::path> &obj_files, //
        const std::filesystem::path &output_file,            //
        const std::vector<std::string> &flags,               //
        const bool is_static                                 //
    );
};
