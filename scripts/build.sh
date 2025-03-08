#!/usr/bin/env sh
set -e

print_usage() {
    echo "Usage:
    build.sh [OPTIONS]* ...

Options:
    -h, --help              Print this help message
    -d, --debug             Compile the compiler in debug mode
    -l, --linux             Build the executable for linux
        --llvm <version>    Select the llvm version tag to use (Defaults to 'llvmorg-19.1.7')
    -s, --static            Build the executable as static
    -t, --test              Run the tests after compilation
    -v, --verbose           Toggle verbosity on
    -w, --windows           Cross compile the executable for windows"
}

# Prints an given error message and returns with the given error code
# $1 - exit code
# $2 - error message
err_exit() {
    echo "Error: $2" >&2
    echo "Use -h or --help to display help" >&2
    exit "$1"
}

# If no CLI arguments are given, print the help and exit
if [ "$#" -eq 0 ]; then
    print_usage
    exit 0
fi

# Get the projects root directory (use cd + dirname + pwd to always get absolute path, even when it is called relatively)
root="$(cd "$(dirname "$0")" && cd .. && pwd)"

is_static=false
build_windows=false
build_linux=false
run_tests=false
debug_mode=false
llvm_version="llvmorg-19.1.7"
if [ "$(which bc)" != "" ]; then
    # Dont fully peg the CPU, only use half its cores, this is stil plenty fast
    core_count="$(echo "$(nproc)/2" | bc)"
else
    core_count="$(nproc)"
fi
verbosity_flag="-DCMAKE_VERBOSE_MAKEFILE=OFF"

# Parse all CLI arguments
while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help)
            print_usage
            exit 0 ;;
        -d|--debug)
            debug_mode=true
            shift ;;
        -l|--linux)
            build_linux=true
            shift ;;
        --llvm)
            [ "$2" != "" ] || err_exit 1 "Option '$1' requires an argument"
            llvm_version="$2"
            shift 2 ;;
        -s|--static)
            is_static=true
            shift ;;
        -t|--test)
            run_tests=true
            shift ;;
        -w|--windows)
            build_windows=true
            shift ;;
        -v|--verbose)
            verbosity_flag="-DCMAKE_VERBOSE_MAKEFILE=ON"
            shift ;;
        *)
            err_exit 1 "Unknown cli argument: '$1'"
            ;;
    esac
done

# Default to Linux build if no platform specified
if [ "$build_windows" = "false" ] && [ "$build_linux" = "false" ]; then
    build_linux=true
fi

# First, ensure that the needed directories exist
echo "-- Creating necessary directories..."

mkdir -p "$root/build"
mkdir -p "$root/build/out"
mkdir -p "$root/vendor"

if [ "$build_windows" = "true" ]; then
    # Create directories
    if [ "$is_static" = "true" ]; then
        mkdir -p "$root/build/llvm-mingw-static"
        mkdir -p "$root/vendor/llvm-mingw-static"
    else
        mkdir -p "$root/build/llvm-mingw"
        mkdir -p "$root/vendor/llvm-mingw"
    fi
fi

if [ "$build_linux" = "true" ]; then
    # Create directories
    if [ "$is_static" = "true" ]; then
        mkdir -p "$root/build/llvm-linux-static"
        mkdir -p "$root/vendor/llvm-linux-static"
    else
        mkdir -p "$root/build/llvm-linux"
        mkdir -p "$root/vendor/llvm-linux"
    fi
fi


echo "-- Checking Library requirements..."

check_lib() {
    lib_file="$(find /usr/lib -name "$1" | head -n 1)"
    [ ! -z "$lib_file" ] || err_exit 1 "Library '$1' could not be found"
}

check_lib "libz.a"
check_lib "libdl.a"
check_lib "libm.a"
check_lib "librt.a"


if [ ! -d "$root/vendor/llvm-project" ]; then
    echo "-- Fetching llvm version $llvm_version from GitHub..."
    cd "$root/vendor"
    git clone --depth 1 --branch "$llvm_version" "https://github.com/llvm/llvm-project.git"
    cd "$root"
fi


if [ "$build_windows" = "true" ]; then
    # Set build and install directories based on static flag
    if [ "$is_static" = "true" ]; then
        llvm_build_dir="$root/build/llvm-mingw-static"
        llvm_install_dir="$root/vendor/llvm-mingw-static"
        echo "-- Building static LLVM for Windows..."
    else
        llvm_build_dir="build/llvm-mingw"
        llvm_install_dir="$root/vendor/llvm-mingw"
        echo "-- Building LLVM for Windows..."
    fi

    # Create directories
    mkdir -p "$llvm_build_dir"
    mkdir -p "$llvm_install_dir"

    llvm_static_flags=""
    if [ "$is_static" = "true" ]; then
        llvm_static_flags="-DLLVM_BUILD_STATIC=ON \
            -DBUILD_SHARED_LIBS=OFF \
            -DLLVM_ENABLE_PIC=OFF \
            -DCMAKE_FIND_LIBRARY_SUFFIXES='.a'"
        echo "-- Building LLVM with static libraries..."
    fi

    cmake -S vendor/llvm-project/llvm -B "$llvm_build_dir" \
        -DCMAKE_INSTALL_PREFIX=$llvm_install_dir \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_ENABLE_PROJECTS="" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-w64-mingw32 \
        -DLLVM_TARGET_ARCH=X86 \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        -DLLVM_ENABLE_THREADS=ON \
        -DLLVM_ENABLE_ZLIB=OFF \
        -DLLVM_ENABLE_LIBXML2=OFF \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_BUILD_TOOLS=ON \
        -DLLVM_INCLUDE_TOOLS=ON \
        -DLLVM_BUILD_UTILS=OFF \
        -DLLVM_BUILD_RUNTIME=OFF \
        -DLLVM_INCLUDE_UTILS=OFF \
        -DLLVM_INCLUDE_RUNTIME=OFF \
        $llvm_static_flags

    cmake --build "$llvm_build_dir" -j$core_count
    cmake --install "$llvm_build_dir" --prefix=$llvm_install_dir
fi


if [ "$build_linux" = "true" ]; then
    # Set build and install directories based on static flagx
    if [ "$is_static" = "true" ]; then
        llvm_build_dir="$root/build/llvm-linux-static"
        llvm_install_dir="$root/vendor/llvm-linux-static"
        echo "-- Building static LLVM for Linux..."
    else
        llvm_build_dir="$root/build/llvm-linux"
        llvm_install_dir="$root/vendor/llvm-linux"
        echo "-- Building LLVM for Linux..."
    fi

    # Create directories
    mkdir -p "$llvm_build_dir"
    mkdir -p "$llvm_install_dir"

    llvm_static_flags=""
    if [ "$is_static" = "true" ]; then
        llvm_static_flags="-DLLVM_BUILD_STATIC=ON \
            -DBUILD_SHARED_LIBS=OFF \
            -DLLVM_ENABLE_ZSTD=OFF \
            -DLLVM_ENABLE_ZLIB=FORCE_ON \
            -DZLIB_USE_STATIC_LIBS=ON \
            -DLLVM_ENABLE_LIBXML2=OFF \
            -DCMAKE_EXE_LINKER_FLAGS='-static' \
            -DCMAKE_FIND_LIBRARY_SUFFIXES='.a' \
            -DLLVM_ENABLE_PIC=OFF"
        echo "-- Building LLVM with static libraries..."
    fi

    # Pre-locate the static zlib to ensure we use it
    STATIC_ZLIB="$(find /usr/lib -name "libz.a" | head -n 1)"
    [ ! -z "$STATIC_ZLIB" ] || print_err 1 "Static zlib (libz.a) not found. Please install it first."
    echo "Using static zlib: $STATIC_ZLIB"

    cmake -S vendor/llvm-project/llvm -B "$llvm_build_dir" \
        -DCMAKE_INSTALL_PREFIX=$llvm_install_dir \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_ENABLE_PROJECTS="" \
        -DLLVM_TARGET_ARCH=X86 \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        -DLLVM_ENABLE_THREADS=ON \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_BUILD_TOOLS=ON \
        -DLLVM_INCLUDE_TOOLS=ON \
        -DLLVM_TOOL_LLVM_CONFIG_BUILD=ON \
        -DLLVM_BUILD_UTILS=OFF \
        -DLLVM_BUILD_RUNTIME=OFF \
        -DLLVM_INCLUDE_UTILS=OFF \
        -DLLVM_INCLUDE_RUNTIME=OFF \
        -DZLIB_LIBRARY="$STATIC_ZLIB" \
        -DZLIB_INCLUDE_DIR="/usr/include" \
        $llvm_static_flags

    cmake --build "$llvm_build_dir" -j$core_count
    cmake --install "$llvm_build_dir" --prefix=$llvm_install_dir
fi


echo "-- Starting CMake configuration phase..."

if [ "$build_windows" = "true" ]; then
    echo "-- Configuring compiler for Windows..."

    static_flag=""
    build_dir="$root/build/windows"
    if [ "$is_static" = "true" ]; then
        static_flag="-DBUILD_STATIC=ON"
        build_dir="${build_dir}-static"
        llvm_lib_path="$root/vendor/llvm-mingw-static/lib"
        echo "-- Building fully static executable..."
    else
        llvm_lib_path="$root/vendor/llvm-mingw/lib"
    fi

    debug_flag="-DDEBUG_MODE=OFF"
    if [ "$debug_mode" = "true" ]; then
        debug_flag="-DDEBUG_MODE=ON"
        echo "-- Building executable in debug mode..."
    fi

    mkdir -p "$build_dir"
    cmake -S "$root" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchain-mingw64.cmake" \
        -DLLVM_LIB_PATH="$llvm_lib_path" \
        "$static_flag" \
        "$debug_flag" \
        "$verbosity_flag"
fi

if [ "$build_linux" = "true" ]; then
    echo "-- Configuring compiler for Linux..."

    static_flag=""
    build_dir="$root/build/linux"
    if [ "$is_static" = "true" ]; then
        static_flag="-DBUILD_STATIC=ON"
        build_dir="${build_dir}-static"
        llvm_lib_path="$root/vendor/llvm-linux-static/lib"
        echo "-- Building fully static executable..."
    else
        llvm_lib_path="$root/vendor/llvm-linux/lib"
    fi

    debug_flag="-DDEBUG_MODE=OFF"
    if [ "$debug_mode" = "true" ]; then
        debug_flag="-DDEBUG_MODE=ON"
        echo "-- Building executable in debug mode..."
    fi

    mkdir -p "$build_dir"
    cmake -S "$root" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchain-linux.cmake" \
        -DLLVM_LIB_PATH="$llvm_lib_path" \
        "$static_flag" \
        "$debug_flag" \
        "$verbosity_flag"
fi


echo "-- Starting CMake build phase..."

if [ "$build_windows" = "true" ]; then
    if [ "$is_static" = "true" ]; then
        echo "-- Building static Windows targets..."
        cmake --build "$root/build/windows-static" -j$core_count --target static
    else
        echo "-- Building Windows targets..."
        cmake --build "$root/build/windows" -j$core_count --target dynamic
    fi
fi

if [ "$build_linux" = "true" ]; then
    if [ "$is_static" = "true" ]; then
        echo "-- Building static Linux targets..."
        cmake --build "$root/build/linux-static" -j$core_count --target static
    else
        echo "-- Building Linux targets..."
        cmake --build "$root/build/linux" -j$core_count --target dynamic
    fi
fi


echo "-- Build finished! Look at 'build/out' to see the built binaries"

# Execute all tests to ensure the compiler is in a working state
if [ "$run_tests" = "true" ]; then
    if [ "$build_windows" = "true" ]; then
        echo "-- Running tests for the Windows compiler"
        ./build/out/tests.exe
    fi

    if [ "build_linux" = "true" ]; then
        echo "-- Running tests for the Linux compiler"
        ./build/out/tests
    fi
fi
