cmake_minimum_required(VERSION 3.16)

project(FlintLanguageServer VERSION 0.2.2 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(DEBUG_MODE "Enable debug build with DEBUG_BUILD definition" OFF)

# Include directories
include_directories(include)
include_directories(../include)
include_directories(../vendor/sources/fip)
include_directories(../vendor/sources/fip/toml)

# Source files for LSP server
set(LSP_SOURCES
    # Main LSP sources
    src/main.cpp
    src/lsp_server.cpp
    src/lsp_protocol.cpp
    src/completion_data.cpp
    src/completion.cpp

    # Sources from the main project
    ../src/error/base_error.cpp
    ../src/error/err_expr_call_of_undefined_function.cpp
    ../src/parser/error_node.cpp
    ../src/parser/parser.cpp
    ../src/parser/parser_definition.cpp
    ../src/parser/parser_expression.cpp
    ../src/parser/parser_statement.cpp
    ../src/parser/parser_util.cpp
    ../src/parser/type.cpp
    ../src/debug.cpp
    ../src/fip.cpp
    ../src/lexer.cpp
    ../src/matcher.cpp
    ../src/profiler.cpp
    ../src/resolver.cpp
)

# Platform-specific compiler flags
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Compiling LSP for Windows target")
    add_definitions(-D_WIN32)
    # Windows-specific compiler flags for MinGW
    if(DEBUG_MODE)
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
            -ffunction-sections
            -fdata-sections
            # No -fstandalone-debug for MinGW
            -D_GNU_SOURCE
            -D__STDC_CONSTANT_MACROS
            -D__STDC_FORMAT_MACROS
            -D__STDC_LIMIT_MACROS
            -DFLINT_LSP
        )
    else()
        # Release-ish flags for Windows
        set(BASE_CXX_FLAGS
            -O3
            -march=native
            -pipe
            -fno-omit-frame-pointer
            -Wall
            -Wextra
            -Wno-deprecated-declarations
            -funwind-tables
            -ffunction-sections
            -fdata-sections
            -D_GNU_SOURCE
            -D__STDC_CONSTANT_MACROS
            -D__STDC_FORMAT_MACROS
            -D__STDC_LIMIT_MACROS
            -DFLINT_LSP
        )
    endif()
    # Windows static linking flags
    set(STATIC_LINK_FLAGS
        -static
        -static-libgcc
        -static-libstdc++
    )
else()
    message(STATUS "Compiling LSP for Linux target")
    # Linux/Unix compiler flags including Clang-specific options
    if(DEBUG_MODE)
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
            -ffunction-sections
            -fdata-sections
            -fstandalone-debug # Only for Clang on Linux
            -D_GNU_SOURCE
            -D__STDC_CONSTANT_MACROS
            -D__STDC_FORMAT_MACROS
            -D__STDC_LIMIT_MACROS
            -DFLINT_LSP
        )
    else()
        # Release flags for Linux
        set(BASE_CXX_FLAGS
            -O3
            -march=native         # enable CPU tuning
            -ffunction-sections
            -fdata-sections
            -Wall
            -Wextra
            -Wno-deprecated-declarations
            -funwind-tables
            -ffunction-sections
            -fdata-sections
            -D_GNU_SOURCE
            -D__STDC_CONSTANT_MACROS
            -D__STDC_FORMAT_MACROS
            -D__STDC_LIMIT_MACROS
            -DFLINT_LSP
        )
    endif()
    # Linux static linking flags
    set(STATIC_LINK_FLAGS
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()

# Add debug definition if enabled
if(DEBUG_MODE)
    add_definitions(-DDEBUG_BUILD)
endif()

# Header files (for IDE organization)
set(LSP_HEADERS
    include/lsp_server.hpp
    include/lsp_protocol.hpp
    include/completion_data.hpp
)

# Create executable
add_executable(fls ${LSP_SOURCES} ${LSP_HEADERS})

# Set output directory
set_target_properties(fls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out
)

target_link_libraries(fls PRIVATE external_deps)
add_library(external_deps STATIC ../vendor/sources/fip/toml/tomlc17.c)

# For Windows, set the required properties
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Set output name with .exe extension explicitly
    set_target_properties(fls PROPERTIES SUFFIX ".exe")
endif()

# Compiler flags
target_compile_options(fls PRIVATE ${BASE_CXX_FLAGS})

# Static linking flags
target_link_options(fls PRIVATE ${STATIC_LINK_FLAGS})

# Install target
install(TARGETS fls
    RUNTIME DESTINATION bin
)
