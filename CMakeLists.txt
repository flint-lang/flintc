cmake_minimum_required(VERSION 3.15)

# Project Name
project(FlintCompiler LANGUAGES CXX)

# Option to force Windows build (useful for cross-compilation setups)
option(FORCE_WINDOWS "Force Windows build configuration" OFF)

# Detect platform
if(WIN32 OR FORCE_WINDOWS)
    set(IS_WINDOWS TRUE)
else()
    set(IS_WINDOWS FALSE)
endif()

# Include the appropriate platform-specific build file
if(IS_WINDOWS)
    message(STATUS "Using Windows build configuration")
    include(${CMAKE_SOURCE_DIR}/cmake/CMakeLists.windows.txt)
else()
    message(STATUS "Using Linux build configuration")
    include(${CMAKE_SOURCE_DIR}/cmake/CMakeLists.linux.txt)
endif()