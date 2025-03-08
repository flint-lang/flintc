cmake_minimum_required(VERSION 3.15)

# Project Name
project(FlintCompiler VERSION 0.1.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_STATIC "Build a fully static executable" OFF)
option(DEBUG_MODE "Enable debug build with DEBUG_BUILD definition" OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
        "Choose the type of build, options are:\
Debug \
Release \
RelWithDebInfo \
MinSizeRel" FORCE)
endif()

# Set the possible values of build type for cmake-gui
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug"
    "Release"
    "MinSizeRel"
    "RelWithDebInfo"
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Handle platform specifics
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Compiling for Windows target")
    add_definitions(-D_WIN32)
    set(PLATFORM_LIBS "")
else()
    message(STATUS "Compiling for Unix-like target")
    set(PLATFORM_LIBS dl)
endif()

# Setting the default llvm lib path
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LLVM_LIB_PATH "${CMAKE_SOURCE_DIR}/vendor/llvm-mingw/lib" CACHE PATH "Path to LLVM libraries")
    if(BUILD_STATIC)
        set(LLVM_LIB_PATH "${CMAKE_SOURCE_DIR}/vendor/llvm-mingw-static/lib" CACHE PATH "Path to LLVM libraries")
    endif()
else()
    set(LLVM_LIB_PATH "${CMAKE_SOURCE_DIR}/vendor/llvm-linux/lib" CACHE PATH "Path to LLVM libraries")
    if(BUILD_STATIC)
        set(LLVM_LIB_PATH "${CMAKE_SOURCE_DIR}/vendor/llvm-linux-static/lib" CACHE PATH "Path to LLVM libraries")
    endif()
endif()

# Find the llvm-config tool relative to LLVM_LIB_PATH
get_filename_component(LLVM_BIN_DIR "${LLVM_LIB_PATH}/../bin" ABSOLUTE)
message(STATUS "Looking for llvm-config in: ${LLVM_BIN_DIR}")

# Check if the bin directory exists
if(NOT EXISTS ${LLVM_BIN_DIR})
    message(FATAL_ERROR "LLVM bin directory does not exist: ${LLVM_BIN_DIR}. Please check your LLVM installation.")
endif()

# List files in bin directory for debugging
file(GLOB BIN_DIR_CONTENTS "${LLVM_BIN_DIR}/*")
message(STATUS "Files in ${LLVM_BIN_DIR}:")
foreach(FILE ${BIN_DIR_CONTENTS})
    message(STATUS "  ${FILE}")
endforeach()

# Try to find llvm-config specifically
if(EXISTS "${LLVM_BIN_DIR}/llvm-config")
    set(LLVM_CONFIG "${LLVM_BIN_DIR}/llvm-config")
    message(STATUS "Found llvm-config at: ${LLVM_CONFIG}")
elseif(EXISTS "${LLVM_BIN_DIR}/llvm-config.exe")
    set(LLVM_CONFIG "${LLVM_BIN_DIR}/llvm-config.exe")
    message(STATUS "Found llvm-config.exe at: ${LLVM_CONFIG}")
else()
    # Use find_program as a fallback
    find_program(LLVM_CONFIG llvm-config
            PATHS ${LLVM_BIN_DIR}
            NO_DEFAULT_PATH
        )

    if(NOT LLVM_CONFIG)
        message(FATAL_ERROR "llvm-config not found in ${LLVM_BIN_DIR}. Please ensure it was built and installed.")
    endif()
endif()

# Double-check that we're not using the system llvm-config
if(LLVM_CONFIG MATCHES "^/usr/bin/")
    message(FATAL_ERROR "Found system llvm-config at ${LLVM_CONFIG} but we need to use the version from your LLVM build. Please ensure llvm-config was built and installed to ${LLVM_BIN_DIR}.")
endif()

message(STATUS "Using llvm-config: ${LLVM_CONFIG}")

# Get llvm-config version for debugging
execute_process(
        COMMAND ${LLVM_CONFIG} --version
        OUTPUT_VARIABLE LLVM_CONFIG_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
message(STATUS "llvm-config version: ${LLVM_CONFIG_VERSION}")

# Check llvm-config prefix
execute_process(
        COMMAND ${LLVM_CONFIG} --prefix
        OUTPUT_VARIABLE LLVM_CONFIG_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
message(STATUS "llvm-config prefix: ${LLVM_CONFIG_PREFIX}")

# Verify the prefix matches our expected installation
get_filename_component(EXPECTED_PREFIX "${LLVM_LIB_PATH}/.." ABSOLUTE)
if(NOT LLVM_CONFIG_PREFIX STREQUAL EXPECTED_PREFIX)
    message(WARNING "llvm-config prefix (${LLVM_CONFIG_PREFIX}) doesn't match expected prefix (${EXPECTED_PREFIX})")
endif()

# Use llvm-config to get proper static libraries
execute_process(
        COMMAND ${LLVM_CONFIG} --link-static --libs all
        OUTPUT_VARIABLE LLVM_STATIC_LIBS
        RESULT_VARIABLE LLVM_CONFIG_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE LLVM_CONFIG_ERROR
    )

# Check for errors
if(NOT LLVM_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Error running llvm-config --link-static --libs all: ${LLVM_CONFIG_ERROR}")
endif()

# Get system libraries needed
execute_process(
        COMMAND ${LLVM_CONFIG} --link-static --system-libs
        OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
        RESULT_VARIABLE LLVM_CONFIG_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE LLVM_CONFIG_ERROR
    )

# Check for errors
if(NOT LLVM_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Error running llvm-config --link-static --system-libs: ${LLVM_CONFIG_ERROR}")
endif()

# Convert space-separated lists to CMake lists
string(REPLACE " " ";" LLVM_STATIC_LIBS_LIST "${LLVM_STATIC_LIBS}")
string(REPLACE " " ";" LLVM_SYSTEM_LIBS_LIST "${LLVM_SYSTEM_LIBS}")

# Setup static build configuration
if(BUILD_STATIC)
    message(STATUS "Building static executable")
    # Force using static libraries only
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

    # For Linux-specific settings
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # These flags help ensure truly static builds on Linux
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        # Optional: disable position independent executable for fully static builds
        set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
    endif()

    # Add a definition to prevent LLVM from using dynamic zlib symbols
    add_definitions(-DLLVM_DISABLE_ZLIB_SYMBOLS)

    # Manually find and specify the static zlib library
    find_library(ZLIB_LIBRARY NAMES libz.a)
    if(NOT ZLIB_LIBRARY)
        message(FATAL_ERROR "Static zlib library not found")
    else()
        message(STATUS "Using static zlib: ${ZLIB_LIBRARY}")
    endif()

    # Create the final link libraries list
    set(FINAL_LINK_LIBS
        -Wl,--start-group
        ${LLVM_STATIC_LIBS_LIST}
        ${ZLIB_LIBRARY}
        -Wl,--end-group
        ${LLVM_SYSTEM_LIBS_LIST}
    )

    message(STATUS "Static build: using llvm-config libraries:")
    message(STATUS "  LLVM static libs: ${LLVM_STATIC_LIBS}")
    message(STATUS "  LLVM system libs: ${LLVM_SYSTEM_LIBS}")
else()
    # For dynamic builds, use the regular zlib
    find_package(ZLIB REQUIRED)
    set(ZLIB_LIBRARY ${ZLIB_LIBRARIES})

    # Find Threads package for dynamic builds
    find_package(Threads REQUIRED)

    # Find zstd library for dynamic builds
    find_library(ZSTD_LIBRARY NAMES zestd libzstd libzstd.so.1)
    if(ZSTD_LIBRARY)
        message(STATUS "Found zstd library: ${ZSTD_LIBRARY}")
    else()
        message(STATUS "Zstd library not found, will try to continue without it")
        set(ZSTD_LIBRARY "")
    endif()

    # Set final link libraries for dynamic builds
    set(FINAL_LINK_LIBS
        ${PLATFORM_LIBS}
        -Wl,--start-group
        ${LLVM_STATIC_LIBS_LIST}
        ${ZLIB_LIBRARY}
        ${ZSTD_LIBRARY}
        -Wl,--end-group
        Threads::Threads
    )
endif()

# Common compiler flags, preserving your existing ones
set(BASE_CXX_FLAGS
    -g
    -O1
    -Werror
    -Wall         # Enable most warnings
    -Wextra       # Enable extra warnings
    -Wshadow      # Warn about shadow variables
    -Wcast-align  # Warn about pointer casts that increase alignment requirement
    -Wcast-qual   # Warn about casts that remove const qualifier
    -Wunused      # Warn about unused variables
    -Wold-style-cast  # Warn about C-style casts
    -Wdouble-promotion # Warn about float being implicitly promoted to double
    -Wformat=2    # Warn about printf/scanf/strftime/strfmon format string issue
    -Wundef       # Warn if an undefined identifier is evaluated in an #if
    -Wpointer-arith # Warn about sizeof(void) and add/sub with void*
    -Wunreachable-code # Warn about unreachable code
    -fno-omit-frame-pointer
    -funwind-tables
    -fstandalone-debug
    -D_GNU_SOURCE
    -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS
    -D__STDC_LIMIT_MACROS
)

# Add debug definition if enabled
if(DEBUG_MODE)
    add_definitions(-DDEBUG_BUILD)
endif()

# Source Files
set(MAIN_FILE src/main.cpp)
set(TEST_FILES tests/tests.cpp)
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
list(REMOVE_ITEM SRC_FILES ${CMAKE_SOURCE_DIR}/${MAIN_FILE})

# Include both directories where the headers are located
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)

# Add the library path to the search directories
link_directories(${LLVM_LIB_PATH})

# Function to create executable targets with proper linking
function(create_executable target_name sources is_static)
    add_executable(${target_name} ${sources})

    # Apply compiler flags
    target_compile_options(${target_name} PRIVATE ${BASE_CXX_FLAGS})

    # Set appropriate link libraries
    if(${is_static})
        target_link_libraries(${target_name} PRIVATE ${FINAL_LINK_LIBS})

        # Set Windows-specific link flags for static builds
        if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set_target_properties(${target_name} PROPERTIES LINK_FLAGS "-static")
        endif()
    else()
        target_link_libraries(${target_name} PRIVATE ${FINAL_LINK_LIBS})
    endif()
endfunction()

# Create binaries
create_executable(flintc "${MAIN_FILE};${SRC_FILES}" TRUE)
create_executable(flintc-dynamic "${MAIN_FILE};${SRC_FILES}" FALSE)
create_executable(tests "${TEST_FILES};${SRC_FILES}" TRUE)
create_executable(tests-dynamic "${TEST_FILES};${SRC_FILES}" FALSE)

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/out)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/obj)

# Custom targets
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning build directories"
)

add_custom_target(build-all
    DEPENDS
        flintc
        flintc-dynamic
        tests
        tests-dynamic
)

add_custom_target(static
    DEPENDS
        flintc
        tests
)

add_custom_target(dynamic
    DEPENDS
        flintc-dynamic
        tests-dynamic
)
