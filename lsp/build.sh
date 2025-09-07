#!/usr/bin/env bash

set -e

print_usage() {
    echo "Usage: build.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help      Show this help"
    echo "  -d, --debug     Build in debug mode (default: release)"
    echo "  -c, --clean     Clean before building"
    echo "  -l, --linux     Build for Linux (default)"
    echo "  -w, --windows   Cross-compile for Windows using MinGW"
    echo "  -j <num>        Number of cores to use (default: auto)"
    echo ""
    echo "Examples:"
    echo "  ./build.sh           # Release build for Linux"
    echo "  ./build.sh -d        # Debug build for Linux"
    echo "  ./build.sh -w        # Release build for Windows"
    echo "  ./build.sh -w -d     # Debug build for Windows"
    echo "  ./build.sh -c -j4    # Clean release build with 4 cores"
}

# Get script directory
root="$(cd "$(dirname "$0")" && pwd)"

# Defaults
build_type="Release"
clean_build=false
target_windows=false
jobs=$(nproc 2>/dev/null || echo 4)

# Parse arguments
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) print_usage; exit 0 ;;
        -d|--debug) build_type="Debug"; shift ;;
        -c|--clean) clean_build=true; shift ;;
        -l|--linux) target_windows=false; shift ;;
        -w|--windows) target_windows=true; shift ;;
        -j) jobs="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Set build directory based on target
if [ "$target_windows" = true ]; then
    build_dir="$root/build-windows"
    binary_name="fls.exe"
    echo "-- Cross-compiling for Windows using MinGW..."

    # Check if MinGW is available
    if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        echo "Error: MinGW cross-compiler not found. Please install mingw-w64."
        echo "On Ubuntu/Debian: sudo apt install mingw-w64"
        exit 1
    fi
else
    build_dir="$root/build-linux"
    binary_name="fls"
    echo "-- Building for Linux..."
fi

# Append debug to build directory if debug mode
if [ "$build_type" = "Debug" ]; then
    build_dir="${build_dir}-debug"
    echo "-- Building in debug mode..."
fi

# Clean if requested
if [ "$clean_build" = true ]; then
    echo "-- Cleaning build directory..."
    rm -rf "$build_dir"
fi

# Set debug mode flag
debug_flag=""
if [ "$build_type" = "Debug" ]; then
    debug_flag="-DDEBUG_MODE=ON"
fi

# Build
echo "-- Creating build directory: $build_dir"
mkdir -p "$build_dir"
cd "$build_dir"

echo "-- Configuring CMake..."

if [ "$target_windows" = true ]; then
    # Cross-compile for Windows
    cmake -DCMAKE_BUILD_TYPE="$build_type" \
          -DCMAKE_SYSTEM_NAME=Windows \
          -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
          -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
          $debug_flag \
          ..
else
    # Native Linux build
    cmake -DCMAKE_BUILD_TYPE="$build_type" \
          $debug_flag \
          ..
fi

echo "-- Building with $jobs cores..."
cmake --build . -j"$jobs"

# Check if build was successful
if [ -f "out/$binary_name" ]; then
    echo "-- Build complete! Binary: $build_dir/out/$binary_name"

    # Show some info about the binary
    echo "-- Binary info:"
    file "out/$binary_name" 2>/dev/null || echo "   (file command not available)"

    if [ "$target_windows" = false ]; then
        ldd "out/$binary_name" 2>/dev/null || echo "   (statically linked or ldd not available)"
    fi
else
    echo "-- Build failed! Binary not found at: $build_dir/out/$binary_name"
    exit 1
fi
