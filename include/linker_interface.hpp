#pragma once

#include <filesystem>

class LinkerInterface {
  public:
    static bool link(const std::filesystem::path &obj_file, const std::filesystem::path &output_file, const bool is_static);
};
