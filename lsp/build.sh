#!/usr/bin/env sh

set -e

print_usage() {
    echo "Usage: build.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help      Show this help"
    echo "  -d, --debug     Build in debug mode (default: release)"
    echo "  -c, --clean     Clean before building"
    echo "  -j <num>        Number of cores to use (default: auto)"
    echo ""
    echo "Examples:"
    echo "  ./build.sh           # Release build"
    echo "  ./build.sh -d        # Debug build"
    echo "  ./build.sh -c -j4    # Clean release build with 4 cores"
}

# Get script directory
root="$(cd "$(dirname "$0")" && pwd)"
build_dir="$root/build"

# Defaults
build_type="Release"
clean_build=false
jobs=$(nproc 2>/dev/null || echo 4)

# Parse arguments
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) print_usage; exit 0 ;;
        -d|--debug) build_type="Debug"; shift ;;
        -c|--clean) clean_build=true; shift ;;
        -j) jobs="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Clean if requested
if [ "$clean_build" = true ]; then
    rm -rf "$build_dir"
fi

# Build
mkdir -p "$build_dir"
cd "$build_dir"
cmake -DCMAKE_BUILD_TYPE="$build_type" ..
cmake --build . -j"$jobs"

echo "Build complete! Binary: $build_dir/out/flint-lsp"

