set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the compilers (use clang as in your original config)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Set architecture-specific flags for x86_64
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=x86-64 -mtune=generic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64 -mtune=generic")

# This is for standard system paths on Arch Linux
set(CMAKE_FIND_ROOT_PATH /usr)

# Include standard system paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Set library directory
set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)

# Common Arch Linux paths
list(APPEND CMAKE_PREFIX_PATH /usr/lib/cmake)
