#!/usr/bin/env bash
set -e

print_usage() {
    echo "Usage:
    build.sh [OPTIONS]* ...

Options:
    -h, --help              Print this help message
    -a, --all               Build all possible versions (dynamic and static for linux and windows)
    -l, --linux             Build the executable for linux
    -w, --windows           Cross compile the executable for windows
    -s, --static            Build the executable as static
    -d, --dynamic           Build the executable as dynamic (default)
    -D  --debug             Compile the compiler in debug mode
    -r  --rebuild           Forces rebuilding of the compiler
    -R  --rebuild-llvm      Forces rebuilding of llvm
    -t, --test              Run the tests after compilation
    -v, --verbose           Toggle verbosity on
        --llvm <version>    Select the llvm version tag to use (Defaults to 'llvmorg-19.1.7')"
}

# Prints an given error message and returns with the given error code
# $1 - exit code
# $2 - error message
err_exit() {
    echo "Error: $2" >&2
    echo "Use -h or --help to display help" >&2
    exit "$1"
}

# Ensures that the needed directories exist
create_directories() {
    mkdir -p "$root/build"
    mkdir -p "$root/build/out"
    mkdir -p "$root/vendor"

    if [ "$build_windows" = "true" ]; then
        mkdir -p "$root/build/llvm-mingw"
        mkdir -p "$root/vendor/llvm-mingw"
    fi

    if [ "$build_linux" = "true" ]; then
        mkdir -p "$root/build/llvm-linux"
        mkdir -p "$root/vendor/llvm-linux"
    fi
}

# Checks if a given library is present in the /usr/lib path
# $1 - The library to search for
check_lib() {
    lib_file="$(find /usr/lib -name "$1" | head -n 1)"
    [ -n "$lib_file" ] || err_exit 1 "Library '$1' could not be found"
}

# Checks if all needed libraries are present
check_library_requirements() {
    check_lib "libz.a"
    check_lib "libdl.a"
    check_lib "libm.a"
    check_lib "librt.a"
    [ -n "$(which clang)" ] || err_exit 1 "'clang' C compiler not found"
    [ -n "$(which clang++)" ] || err_exit 1 "'clang++' C++ compiler not found"
    [ -n "$(which cmake)" ] || err_exit 1 "'cmake' is not installed"
}

# Builds llvm for windows
build_llvm_windows() {
    # Set build and install directories based on static flag
    llvm_build_dir="$root/build/llvm-mingw"
    llvm_install_dir="$root/vendor/llvm-mingw"
    echo "-- Building static LLVM for Windows..."
    if [ "$force_rebuild_llvm" = "true" ]; then
        rm -r "$root/build/llvm-mingw"
    fi

    # Create directories
    mkdir -p "$llvm_build_dir"
    mkdir -p "$llvm_install_dir"

    llvm_static_flags="-DLLVM_BUILD_STATIC=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DLLVM_ENABLE_PIC=OFF \
        -DCMAKE_FIND_LIBRARY_SUFFIXES='.a'"
    echo "-- Building LLVM with static libraries..."

    cmake -S vendor/llvm-project/llvm -B "$llvm_build_dir" \
        -DCMAKE_INSTALL_PREFIX="$llvm_install_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SKIP_INSTALL_RPATH=ON \
        -DLLVM_ENABLE_PROJECTS="lld" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-w64-mingw32 \
        -DLLVM_TARGET_ARCH=X86 \
        -DLLVM_ENABLE_OPAQUE_POINTERS=ON \
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
        "$llvm_static_flags"

    cmake --build "$llvm_build_dir" -j"$core_count"
    cmake --install "$llvm_build_dir" --prefix="$llvm_install_dir"
}

# Builds llvm for linux
build_llvm_linux() {
    # Set build and install directories based on static flags
    llvm_build_dir="$root/build/llvm-linux"
    llvm_install_dir="$root/vendor/llvm-linux"
    echo "-- Building static LLVM for Linux..."
    if [ "$force_rebuild_llvm" = "true" ]; then
        rm -r "$root/build/llvm-linux"
    fi

    # Create directories
    mkdir -p "$llvm_build_dir"
    mkdir -p "$llvm_install_dir"

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

    # Pre-locate the static zlib to ensure we use it
    STATIC_ZLIB="$(find /usr/lib -name "libz.a" | head -n 1)"
    [ -n "$STATIC_ZLIB" ] || print_err 1 "Static zlib (libz.a) not found. Please install it first."
    echo "Using static zlib: $STATIC_ZLIB"

    cmake -S vendor/llvm-project/llvm -B "$llvm_build_dir" \
        -DCMAKE_INSTALL_PREFIX="$llvm_install_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SKIP_INSTALL_RPATH=ON \
        -DLLVM_ENABLE_PROJECTS="lld" \
        -DCMAKE_C_COMPILER="clang" \
        -DCMAKE_CXX_COMPILER="clang++" \
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
        "$llvm_static_flags"

    cmake --build "$llvm_build_dir" -j"$core_count"
    cmake --install "$llvm_build_dir" --prefix="$llvm_install_dir"
}

# Checks if llvm has been cloned, if not does clone it
fetch_llvm() {
    if [ ! -d "$root/vendor/llvm-project" ]; then
        echo "-- Fetching llvm version $llvm_version from GitHub..."
        cd "$root/vendor"
        git clone --depth 1 --branch "$llvm_version" "https://github.com/llvm/llvm-project.git"
        cd "$root"
    fi
}

# Compiles the needed versions of llvm
build_llvm() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            build_llvm_windows "true"
        fi
        if [ "$build_dynamic" = "true" ]; then
            build_llvm_windows "false"
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            build_llvm_linux "true"
        fi
        if [ "$build_dynamic" = "true" ]; then
            build_llvm_linux "false"
        fi
    fi
}

fetch_json_mini() {
    if [ ! -d "$root/vendor/json-mini" ]; then
        echo "-- Fetching json-mini from GitHub..."
        cd "$root/vendor"
        git clone "https://github.com/flint-lang/json-mini.git"
        cd "$root"
    else
        echo "-- Updating json-mini repository..."
        cd "$root/vendor/json-mini"
        git fetch
        git pull
        cd "$root"
    fi
}

# Sets up the cmake build directory for windows
# $1 - is_static - Whether to build the dynamic ("false") or the static ("true") version of llvm
setup_build_windows() {
    echo "-- Configuring compiler for Windows..."

    static_flag=""
    build_dir="$root/build/windows"
    llvm_lib_path="$root/vendor/llvm-mingw/lib"
    if [ "$1" = "true" ]; then
        static_flag="-DBUILD_STATIC=ON"
        build_dir="${build_dir}-static"
        echo "-- Building fully static executable..."
    fi

    debug_flag="-DDEBUG_MODE=OFF"
    if [ "$debug_mode" = "true" ]; then
        debug_flag="-DDEBUG_MODE=ON"
        echo "-- Building executable in debug mode..."
    fi

    if [ "$force_rebuild" = "true" ]; then
        rm -r "$build_dir"
    fi

    mkdir -p "$build_dir"
    cmake -S "$root" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchain-mingw64.cmake" \
        -DLLVM_LIB_PATH="$llvm_lib_path" \
        "$static_flag" \
        "$debug_flag" \
        "$verbosity_flag"
}

# Sets up the cmake build directory for linux
# $1 - is_static - Whether to build the dynamic ("false") or the static ("true") version of llvm
setup_build_linux() {
    echo "-- Configuring compiler for Linux..."

    static_flag=""
    build_dir="$root/build/linux"
    llvm_lib_path="$root/vendor/llvm-linux/lib"
    if [ "$1" = "true" ]; then
        static_flag="-DBUILD_STATIC=ON"
        build_dir="${build_dir}-static"
        echo "-- Building fully static executable..."
    fi

    debug_flag="-DDEBUG_MODE=OFF"
    if [ "$debug_mode" = "true" ]; then
        debug_flag="-DDEBUG_MODE=ON"
        echo "-- Building executable in debug mode..."
    fi

    if [ "$force_rebuild" = "true" ]; then
        rm -r "$build_dir"
    fi

    mkdir -p "$build_dir"
    cmake -S "$root" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchain-linux.cmake" \
        -DLLVM_LIB_PATH="$llvm_lib_path" \
        "$static_flag" \
        "$debug_flag" \
        "$verbosity_flag"
}

# Sets up all build directories for all compiler builds
setup_builds() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            setup_build_windows "true"
        fi
        if [ "$build_dynamic" = "true" ]; then
            setup_build_windows "false"
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            setup_build_linux "true"
        fi
        if [ "$build_dynamic" = "true" ]; then
            setup_build_linux "false"
        fi
    fi
}

# Builds all requested compilers
build_compilers() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            echo "-- Building static Windows targets..."
            cmake --build "$root/build/windows-static" -j"$core_count" --target static
        fi
        if [ "$build_dynamic" = "true" ]; then
            echo "-- Building Windows targets..."
            cmake --build "$root/build/windows" -j"$core_count" --target dynamic
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            echo "-- Building static Linux targets..."
            cmake --build "$root/build/linux-static" -j"$core_count" --target static
        fi
        if [ "$build_dynamic" = "true" ]; then
            echo "-- Building Linux targets..."
            cmake --build "$root/build/linux" -j"$core_count" --target dynamic
        fi
    fi
}

# Copies all executables from the nested out directories into one out directory, where all outs are saved
copy_executables() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            cp "$root/build/windows-static/out/flintc.exe" "$root/build/out/"
            cp "$root/build/windows-static/out/tests.exe" "$root/build/out/"
        fi
        if [ "$build_dynamic" = "true" ]; then
            cp "$root/build/windows/out/dynamic-flintc.exe" "$root/build/out/"
            cp "$root/build/windows/out/dynamic-tests.exe" "$root/build/out/"
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            cp "$root/build/linux-static/out/flintc" "$root/build/out/"
            cp "$root/build/linux-static/out/tests" "$root/build/out/"
        fi
        if [ "$build_dynamic" = "true" ]; then
            cp "$root/build/linux/out/dynamic-flintc" "$root/build/out/"
            cp "$root/build/linux/out/dynamic-tests" "$root/build/out/"
        fi
    fi
}

# Executes all tests to ensure the compiler is in a working state
run_tests() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            echo "-- Running tests for the static Windows compiler..."
            "$root"/build/out/tests.exe
            echo
        fi
        if [ "$build_dynamic" = "true" ]; then
            echo "-- Running tests for the dynamic Windows compiler is not suppoerted!"
            # "$root"/build/out/dynamic-tests.exe
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            echo "-- Running tests for the static Linux compiler..."
            "$root"/build/out/tests
        fi
        if [ "$build_dynamic" = "true" ]; then
            echo "-- Running tests for the dynamic Linux compiler..."
            "$root"/build/out/dynamic-tests
        fi
    fi
}

# If no CLI arguments are given, print the help and exit
if [ "$#" -eq 0 ]; then
    print_usage
    exit 0
fi

# Get the projects root directory (use cd + dirname + pwd to always get absolute path, even when it is called relatively)
root="$(cd "$(dirname "$0")" && cd .. && pwd)"

build_dynamic=false
build_static=false
build_windows=false
build_linux=false
force_rebuild=false
force_rebuild_llvm=false
run_tests=false
debug_mode=false
llvm_version="llvmorg-19.1.7"
# Dont fully peg the CPU, use one core less
core_count="$(($(nproc) - 1))"
verbosity_flag="-DCMAKE_VERBOSE_MAKEFILE=OFF"

# Parse all CLI arguments
while [ "$#" -gt 0 ]; do
    case "$1" in
    -h | --help)
        print_usage
        exit 0
        ;;
    -a | --all)
        build_linux=true
        build_windows=true
        build_static=true
        build_dynamic=true
        shift
        ;;
    --dynamic)
        build_dynamic=true
        shift
        ;;
    --debug)
        debug_mode=true
        shift
        ;;
    --linux)
        build_linux=true
        shift
        ;;
    --llvm)
        [ "$2" != "" ] || err_exit 1 "Option '$1' requires an argument"
        llvm_version="$2"
        shift 2
        ;;
    --rebuild)
        force_rebuild=true
        shift
        ;;
    --rebuild-llvm)
        force_rebuild_llvm=true
        shift
        ;;
    --static)
        build_static=true
        shift
        ;;
    --test)
        run_tests=true
        shift
        ;;
    --windows)
        build_windows=true
        shift
        ;;
    --verbose)
        verbosity_flag="-DCMAKE_VERBOSE_MAKEFILE=ON"
        shift
        ;;
    --*) # Handle long options
        err_exit 1 "Unknown cli argument: '$1'"
        ;;
    -*) # Handle short options, including combined ones
        arg="${1#-}"
        shift

        while [ -n "$arg" ]; do
            opt="${arg:0:1}" # Extract first character
            arg="${arg:1}"   # Remove first character

            case "$opt" in
            a)
                build_linux=true
                build_windows=true
                build_static=true
                build_dynamic=true
                ;;
            d)
                build_dynamic=true
                ;;
            D)
                debug_mode=true
                ;;
            l)
                build_linux=true
                ;;
            r)
                force_rebuild=true
                ;;
            R)
                force_rebuild_llvm=true
                ;;
            s)
                build_static=true
                ;;
            t)
                run_tests=true
                ;;
            w)
                build_windows=true
                ;;
            v)
                verbosity_flag="-DCMAKE_VERBOSE_MAKEFILE=ON"
                ;;
            *)
                err_exit 1 "Unknown option: -$opt"
                ;;
            esac
        done
        ;;
    *)
        err_exit 1 "Unknown cli argument: '$1'"
        ;;
    esac
done

# Default to Linux build if no platform specified
if [ "$build_windows" = "false" ] && [ "$build_linux" = "false" ]; then
    build_linux=true
fi

# Default to dynamic build if no flag is specified
if [ "$build_static" = "false" ] && [ "$build_dynamic" = "false" ]; then
    build_dynamic=true
fi

echo "-- Creating necessary directories..."
create_directories

echo "-- Checking Library requirements..."
check_library_requirements

echo "-- Checking if LLVM has been fetched yet..."
fetch_llvm

echo "-- Building all required llvm versions..."
build_llvm

echo "-- Making sure json-mini is up to date..."
fetch_json_mini

echo "-- Starting CMake configuration phase..."
setup_builds

echo "-- Starting CMake build phase..."
build_compilers

echo "-- Putting the built binaries into the 'build/out' directory..."
copy_executables

if [ "$run_tests" = "true" ]; then
    echo "-- Running all tests..."
    run_tests
fi

echo "-- Build finished! Look at 'build/out' to see the built binaries!"
