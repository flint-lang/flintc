#pragma once

#include <filesystem>
#include <vector>

class LinkerInterface {
  public:
    static bool link(const std::filesystem::path &obj_file, const std::filesystem::path &output_file, const bool is_static);
    static bool create_static_library(const std::vector<std::filesystem::path> &obj_files, const std::filesystem::path &output_file);
};
