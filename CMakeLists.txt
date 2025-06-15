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

# Add the Commit hash definition
add_compile_definitions(
    COMMIT_HASH=${COMMIT_HASH}
    BUILD_DATE="${BUILD_DATE}"
)

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Compiling for Windows target")
    add_definitions(-D_WIN32)
    set(PLATFORM_LIBS "")
    # Windows-specific compiler flags for MinGW
    set(BASE_CXX_FLAGS
        -g
        -O1
        -Wall         # Keep basic warnings
        -Wextra       # Extra warnings
        # Remove -Werror to allow warnings in LLVM headers
        -Wno-unused-parameter  # Suppress specific warnings common in LLVM
        -Wno-unused-variable
        -Wno-deprecated-declarations
        -fno-omit-frame-pointer
        -funwind-tables
        # No -fstandalone-debug for MinGW
        -D_GNU_SOURCE
        -D__STDC_CONSTANT_MACROS
        -D__STDC_FORMAT_MACROS
        -D__STDC_LIMIT_MACROS
    )
else()
    message(STATUS "Compiling for Linux target")
    set(PLATFORM_LIBS dl)
    # Linux/Unix compiler flags including Clang-specific options
    set(BASE_CXX_FLAGS
        -g
        -O0
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
        -Wno-deprecated-declarations  # Ignore deprecation warnings
        -Wno-deprecated  # Ignore general deprecation warnings
        -fno-omit-frame-pointer
        -funwind-tables
        -fstandalone-debug # Only for Clang on Linux
        -D_GNU_SOURCE
        -D__STDC_CONSTANT_MACROS
        -D__STDC_FORMAT_MACROS
        -D__STDC_LIMIT_MACROS
    )
endif()

# Setting the default LLVM lib path based on platform and build type
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LLVM_LIB_PATH "${CMAKE_SOURCE_DIR}/vendor/llvm-mingw/lib" CACHE PATH "Path to LLVM libraries")
else()
    set(LLVM_LIB_PATH "${CMAKE_SOURCE_DIR}/vendor/llvm-linux/lib" CACHE PATH "Path to LLVM libraries")
endif()

# Get the LLVM include directory
get_filename_component(LLVM_INCLUDE_DIR "${LLVM_LIB_PATH}/../include" ABSOLUTE)

# Check if the include directory exists
if(NOT EXISTS ${LLVM_INCLUDE_DIR})
    message(FATAL_ERROR "LLVM include directory does not exist: ${LLVM_INCLUDE_DIR}. Please check your LLVM installation.")
endif()

message(STATUS "Using LLVM include directory: ${LLVM_INCLUDE_DIR}")

# Include the vendor stuff (json-mini)
include_directories(${CMAKE_SOURCE_DIR}/vendor/json-mini/include)

# Always add this definition to prevent LLVM from using zstd
add_definitions(-DLLVM_DISABLE_ZLIB_SYMBOLS -DLLVM_ENABLE_ZSTD=0)

# Find the Linux static llvm-config for use in both Linux and Windows builds
get_filename_component(LINUX_STATIC_LLVM_BIN_DIR "${CMAKE_SOURCE_DIR}/vendor/llvm-linux/bin" ABSOLUTE)
find_program(LINUX_LLVM_CONFIG llvm-config
    PATHS ${LINUX_STATIC_LLVM_BIN_DIR}
    NO_DEFAULT_PATH
)

if(NOT LINUX_LLVM_CONFIG)
    message(FATAL_ERROR "Linux static llvm-config not found at ${LINUX_STATIC_LLVM_BIN_DIR}. This is needed for builds.")
endif()

message(STATUS "Using Linux static llvm-config: ${LINUX_LLVM_CONFIG}")

# Get the complete library list from Linux llvm-config
execute_process(
    COMMAND ${LINUX_LLVM_CONFIG} --link-static --libs all
    OUTPUT_VARIABLE LINUX_LLVM_LIBS
    RESULT_VARIABLE LLVM_CONFIG_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE LLVM_CONFIG_ERROR
)

# Check for errors
if(NOT LLVM_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Error running Linux llvm-config --link-static --libs all: ${LLVM_CONFIG_ERROR}")
endif()

# Get system libraries needed
execute_process(
    COMMAND ${LINUX_LLVM_CONFIG} --link-static --system-libs
    OUTPUT_VARIABLE LINUX_SYSTEM_LIBS
    RESULT_VARIABLE LLVM_CONFIG_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE LLVM_CONFIG_ERROR
)

# Check for errors
if(NOT LLVM_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Error running Linux llvm-config --link-static --system-libs: ${LLVM_CONFIG_ERROR}")
endif()

# Get LLVM version
execute_process(
    COMMAND ${LINUX_LLVM_CONFIG} --version
    OUTPUT_VARIABLE LLVM_CONFIG_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "LLVM version: ${LLVM_CONFIG_VERSION}")

# Platform-specific LLVM library handling
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Transform the -l... format to component names for Windows
    string(REGEX REPLACE "-l(LLVM[^ \t\r\n]+)" "\\1" LLVM_COMPONENTS "${LINUX_LLVM_LIBS}")
    # Split into a list
    string(REGEX REPLACE "[ \t\r\n]+" ";" LLVM_COMPONENTS_LIST "${LLVM_COMPONENTS}")

    message(STATUS "Components from Linux llvm-config: ${LLVM_COMPONENTS_LIST}")

    # Check which components actually exist for Windows
    set(WINDOWS_COMPONENTS_LIST "")
    foreach(COMPONENT ${LLVM_COMPONENTS_LIST})
        # Check if the library exists in the Windows LLVM path
        if(EXISTS "${LLVM_LIB_PATH}/lib${COMPONENT}.a")
            list(APPEND WINDOWS_COMPONENTS_LIST ${COMPONENT})
        else()
            message(STATUS "Skipping component not available in Windows build: ${COMPONENT}")
        endif()
    endforeach()
    list(APPEND WINDOWS_COMPONENTS_LIST "lldCOFF")
    list(APPEND WINDOWS_COMPONENTS_LIST "lldELF")
    list(APPEND WINDOWS_COMPONENTS_LIST "lldCommon")

    # Use the filtered list for Windows
    set(LLVM_STATIC_LIBS_LIST ${WINDOWS_COMPONENTS_LIST})
    set(LLVM_SYSTEM_LIBS_LIST "")  # Windows doesn't use the same system libs

    message(STATUS "Final Windows LLVM components: ${LLVM_STATIC_LIBS_LIST}")
else()
    # For Linux, use the output directly
    string(REPLACE " " ";" LLVM_STATIC_LIBS_LIST "${LINUX_LLVM_LIBS}")
    list(APPEND LLVM_STATIC_LIBS_LIST "-llldCommon")
    list(APPEND LLVM_STATIC_LIBS_LIST "-llldELF")
    list(APPEND LLVM_STATIC_LIBS_LIST "lldCOFF")
    string(REPLACE " " ";" LLVM_SYSTEM_LIBS_LIST "${LINUX_SYSTEM_LIBS}")

    message(STATUS "Linux LLVM libraries: ${LLVM_STATIC_LIBS_LIST}")
    message(STATUS "Linux system libraries: ${LLVM_SYSTEM_LIBS_LIST}")
endif()

# Setup static build configuration
if(BUILD_STATIC)
    message(STATUS "Building static executable")

    # Platform-specific static build settings
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # For Windows, we use .a with MinGW
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.lib")

        # Add the static flag to the linker
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static" CACHE STRING "static link flag" FORCE)

        # Find static zlib for Windows (optional)
        find_library(ZLIB_LIBRARY
            NAMES libz.a z.a zlib.a
            PATHS "${CMAKE_SOURCE_DIR}/vendor/zlib/lib" /mingw64/lib /usr/x86_64-w64-mingw32/lib
        )

        if(NOT ZLIB_LIBRARY)
            message(STATUS "Static zlib library not found for Windows, continuing without it")
            set(ZLIB_LIBRARY "")
            add_definitions(-DLLVM_ENABLE_ZLIB=OFF)
        else()
            message(STATUS "Using static zlib for Windows: ${ZLIB_LIBRARY}")
        endif()

        # Windows-specific static libraries
        set(WIN_STATIC_LIBS
            ws2_32    # Windows sockets
            psapi     # Process status API
            imagehlp  # Image Help Library
            advapi32  # Windows advanced services
            shell32   # Shell functions
            ole32     # OLE functions
            uuid      # UUID functions
            ntdll     # Native API functions
        )

        # Create the final link libraries list for Windows
        set(FINAL_LINK_LIBS
            -Wl,--start-group
            ${LLVM_STATIC_LIBS_LIST}
            ${ZLIB_LIBRARY}
            ${WIN_STATIC_LIBS}
            -Wl,--end-group
        )
    else()
        # Linux static build settings
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

        # Force using static linking for Linux
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
        set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

        # Find static zlib for Linux
        find_library(ZLIB_LIBRARY NAMES libz.a)
        if(NOT ZLIB_LIBRARY)
            message(FATAL_ERROR "Static zlib library not found for Linux")
        else()
            message(STATUS "Using static zlib for Linux: ${ZLIB_LIBRARY}")
        endif()

        # Create the final link libraries list for Linux
        set(FINAL_LINK_LIBS
            -Wl,--start-group
            ${LLVM_STATIC_LIBS_LIST}
            ${ZLIB_LIBRARY}
            -Wl,--end-group
            ${LLVM_SYSTEM_LIBS_LIST}
        )
    endif()
else()
    # Dynamic build settings
    message(STATUS "Building dynamic executable")

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # Find dynamic zlib for Windows
        find_library(ZLIB_LIBRARY
            NAMES zlib1.dll libzlib.dll.a zlib.dll.a
            PATHS "${CMAKE_SOURCE_DIR}/vendor/zlib/bin" /mingw64/bin /usr/x86_64-w64-mingw32/bin
        )

        if(NOT ZLIB_LIBRARY)
            message(STATUS "Dynamic zlib library not found for Windows, continuing without it")
            set(ZLIB_LIBRARY "")
            add_definitions(-DLLVM_ENABLE_ZLIB=OFF)
        else()
            message(STATUS "Using dynamic zlib for Windows: ${ZLIB_LIBRARY}")
        endif()

        # Windows-specific dynamic libraries
        set(WIN_DYNAMIC_LIBS
            ws2_32    # Windows sockets
            psapi     # Process status API
            imagehlp  # Image Help Library
            advapi32  # Windows advanced services
            shell32   # Shell functions
            ole32     # OLE functions
            uuid      # UUID functions
            ntdll     # Native API functions
        )

        # Set final link libraries for dynamic Windows builds
        set(FINAL_LINK_LIBS
            -Wl,--start-group
            ${LLVM_STATIC_LIBS_LIST}  # We're still statically linking LLVM
            ${ZLIB_LIBRARY}
            ${WIN_DYNAMIC_LIBS}
            -Wl,--end-group
        )
    else()
        # For Linux dynamic builds
        find_package(ZLIB REQUIRED)
        set(ZLIB_LIBRARY ${ZLIB_LIBRARIES})

        # Find Threads package for dynamic builds
        find_package(Threads REQUIRED)

        # Find zstd library for dynamic builds
        find_library(ZSTD_LIBRARY NAMES zstd libzstd libzstd.so.1)
        if(ZSTD_LIBRARY)
            message(STATUS "Found zstd library: ${ZSTD_LIBRARY}")
        else()
            message(STATUS "Zstd library not found, will try to continue without it")
            set(ZSTD_LIBRARY "")
        endif()

        # Set final link libraries for dynamic Linux builds
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
endif()

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

    # Link libraries with platform-specific settings
    target_link_libraries(${target_name} PRIVATE ${FINAL_LINK_LIBS})

    # Add the LLVM include directory to the include paths
    target_include_directories(${target_name} SYSTEM PRIVATE ${LLVM_INCLUDE_DIR})

    # For Windows, set the required properties
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # Set output name with .exe extension explicitly
        set_target_properties(${target_name} PROPERTIES SUFFIX ".exe")
    endif()
endfunction()

# Create binaries
create_executable(flintc "${MAIN_FILE};${SRC_FILES}" TRUE)
create_executable(dynamic-flintc "${MAIN_FILE};${SRC_FILES}" FALSE)
create_executable(tests "${TEST_FILES};${SRC_FILES}" TRUE)
create_executable(dynamic-tests "${TEST_FILES};${SRC_FILES}" FALSE)

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
        dynamic-flintc
        tests
        dynamic-tests
)

add_custom_target(static
    DEPENDS
        flintc
        tests
)

add_custom_target(dynamic
    DEPENDS
        dynamic-flintc
        dynamic-tests
)
