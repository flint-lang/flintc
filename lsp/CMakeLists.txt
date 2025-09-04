cmake_minimum_required(VERSION 3.16)

project(FlintLanguageServer VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Source files for LSP server
set(LSP_SOURCES
    src/main.cpp
    src/lsp_server.cpp
    src/lsp_protocol.cpp
    src/completion_data.cpp
)

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

# Compiler flags
if(MSVC)
    target_compile_options(fls PRIVATE /W4)
else()
    target_compile_options(fls PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS fls
    RUNTIME DESTINATION bin
)

