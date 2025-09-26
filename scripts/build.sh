#!/usr/bin/env bash
set -e

# Dependencies Commit Hashes
FIP_HASH="d8e53dc86e3d5e02c71673dc7b9ebf037c6e069d"
JSON_MINI_HASH="a32d6e8319d90f5fa75f1651f30798c71464e4c6"

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
    -D  --debug             Compile the compiler in debug mode (default)
    -R  --release           Compile the compiler in release mode
    -f, --fls               Builds all versions of the Flint Language Server
    -r  --rebuild           Forces rebuilding of the Flint compiler
        --rebuild-llvm      Forces rebuilding of the llvm libraries
        --rebuild-config    Forces rebuilding of the llvm-config executable
    -t, --test              Run the tests after compilation
    -v, --verbose           Toggle verbosity on
        --llvm <version>    Select the llvm version tag to use (Defaults to 'llvmorg-19.1.7')
    -j <num>                The number of cores to use for compilation"
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
    mkdir -p "$root/vendor/sources"

    if [ "$build_windows" = "true" ]; then
        mkdir -p "$root/build/llvm-mingw"
    fi

    if [ "$build_linux" = "true" ]; then
        mkdir -p "$root/build/llvm-linux"
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
    check_lib "libdl.a"
    check_lib "libm.a"
    check_lib "librt.a"
    [ -n "$(which clang)" ] || err_exit 1 "'clang' C compiler not found"
    [ -n "$(which clang++)" ] || err_exit 1 "'clang++' C++ compiler not found"
    [ -n "$(which cmake)" ] || err_exit 1 "'cmake' is not installed"
}

fetch_zlib() {
    if [ ! -d "$root/vendor/sources/zlib" ]; then
        echo "-- Fetching zlib version $zlib_version from GitHub..."
        cd "$root/vendor/sources"
        git clone --depth 1 --branch "$zlib_version" "https://github.com/madler/zlib.git"
        cd "$root"
    fi
}

build_zlib() {
    zlib_build_dir="$root/build/zlib"
    zlib_install_dir="$root/vendor/zlib"

    cmake -S "$root/vendor/sources/zlib" -B "$zlib_build_dir" \
        -DCMAKE_INSTALL_PREFIX="$zlib_install_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_C_FLAGS="-fPIC"

    cmake --build "$zlib_build_dir" -j"$core_count"
    cmake --install "$zlib_build_dir" --prefix="$zlib_install_dir"
}

# Builds llvm for windows
build_llvm_windows() {
    llvm_build_dir="$root/build/llvm-mingw"
    llvm_install_dir="$root/vendor/llvm-mingw"
    echo "-- Building LLVM static libraries for Windows..."
    if [ "$force_rebuild_llvm" = "true" ]; then
        rm -rf "$llvm_build_dir"
        rm -rf "$llvm_install_dir"
    fi

    # Create directories
    mkdir -p "$llvm_build_dir"
    mkdir -p "$llvm_install_dir"

    cmake -S "$root/vendor/sources/llvm-project/llvm" -B "$llvm_build_dir" \
        -DCMAKE_INSTALL_PREFIX="$llvm_install_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SKIP_INSTALL_RPATH=ON \
        -DLLVM_ENABLE_PROJECTS="clang;lld" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-w64-mingw32 \
        -DLLVM_TARGET_ARCH=X86 \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        \
        -DLLVM_ENABLE_THREADS=ON \
        -DLLVM_BUILD_STATIC=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DLLVM_ENABLE_PIC=OFF \
        \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_INCLUDE_DOCS=OFF \
        -DLLVM_BUILD_DOCS=OFF \
        -DLLVM_BUILD_UTILS=OFF \
        -DLLVM_INCLUDE_UTILS=OFF \
        -DLLVM_BUILD_RUNTIME=OFF \
        \
        -DLLVM_BUILD_TOOLS=ON \
        -DLLVM_INCLUDE_TOOLS=ON \
        \
        -DCLANG_BUILD_TOOLS=OFF \
        -DCLANG_INCLUDE_TESTS=OFF \
        -DCLANG_INCLUDE_DOCS=OFF \
        -DCLANG_BUILD_EXAMPLES=OFF \
        -DCLANG_ENABLE_ARCMT=OFF \
        -DCLANG_ENABLE_STATIC_ANALYZER=OFF \
        -DCLANG_ENABLE_OBJC_REWRITER=OFF \
        -DCLANG_PLUGIN_SUPPORT=OFF \
        \
        -DLLVM_ENABLE_ZLIB=OFF \
        -DLLVM_ENABLE_ZSTD=OFF \
        -DLLVM_ENABLE_LIBXML2=OFF \
        -DLLVM_ENABLE_CURL=OFF \
        -DLLVM_ENABLE_HTTPLIB=OFF \
        -DLLVM_ENABLE_FFI=OFF \
        -DLLVM_ENABLE_LIBEDIT=OFF \
        -DLLVM_ENABLE_Z3_SOLVER=OFF \
        -DCMAKE_FIND_LIBRARY_SUFFIXES='.a'

    echo "-- Building LLVM, Clang, and LLD static libraries for Windows..."
    # Build library targets including the C API
    cmake --build "$llvm_build_dir" -j"$core_count" --target \
        llvm-libraries clang-libraries libclang \
        lldCommon lldELF lldCOFF lldMachO lldMinGW lldWasm

    echo "-- Installing all headers to $llvm_install_dir ..."
    cmake --install "$llvm_build_dir" --prefix="$llvm_install_dir" --component llvm-headers
    cmake --install "$llvm_build_dir" --prefix="$llvm_install_dir" --component clang-headers

    echo "-- Manually copying llvm, libclang and the LLD libraries to $llvm_install_dir"
    mkdir -p "$llvm_install_dir/lib"
    read -a llvm_libs <<< "$("$root/vendor/llvm-config/bin/llvm-config" --link-static --libs all | sed "s/-l//g")"
    declare -a to_copy=(
        "clang"
        "lldCommon"
        "lldELF"
        "lldCOFF"
        "lldMachO"
        "lldMinGW"
        "lldWasm"
    )
    for lib in "${llvm_libs[@]}"; do
        echo "Copying lib${lib}.a ..."
        if [ -f "$llvm_build_dir/lib/lib${lib}.a" ]; then
            cp "$llvm_build_dir/lib/lib${lib}.a" "$llvm_install_dir/lib/lib${lib}.a"
        else
            echo "Warning: lib${lib}.a not found, skipping..."
        fi
    done
    for lib in "${to_copy[@]}"; do
        echo "Copying lib${lib}.a ..."
        if [ -f "$llvm_build_dir/lib/lib${lib}.a" ]; then
            cp "$llvm_build_dir/lib/lib${lib}.a" "$llvm_install_dir/lib/lib${lib}.a"
        else
            echo "Warning: lib${lib}.a not found, skipping..."
        fi
    done
}

# Builds llvm for linux
build_llvm_linux() {
    llvm_build_dir="$root/build/llvm-linux"
    llvm_install_dir="$root/vendor/llvm-linux"
    echo "-- Building LLVM static libraries for Linux..."
    if [ "$force_rebuild_llvm" = "true" ]; then
        rm -rf "$llvm_build_dir"
        rm -rf "$llvm_install_dir"
    fi

    # Create directories
    mkdir -p "$llvm_build_dir"
    mkdir -p "$llvm_install_dir"

    cmake -S "$root/vendor/sources/llvm-project/llvm" -B "$llvm_build_dir" \
        -DCMAKE_INSTALL_PREFIX="$llvm_install_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SKIP_INSTALL_RPATH=ON \
        -DLLVM_ENABLE_PROJECTS="clang;lld" \
        -DCMAKE_C_COMPILER="clang" \
        -DCMAKE_CXX_COMPILER="clang++" \
        -DLLVM_TARGET_ARCH=X86 \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        \
        -DLLVM_ENABLE_THREADS=ON \
        -DLLVM_BUILD_STATIC=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DLLVM_ENABLE_PIC=OFF \
        \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_INCLUDE_DOCS=OFF \
        -DLLVM_BUILD_DOCS=OFF \
        -DLLVM_BUILD_UTILS=OFF \
        -DLLVM_INCLUDE_UTILS=OFF \
        -DLLVM_BUILD_RUNTIME=OFF \
        \
        -DLLVM_BUILD_TOOLS=ON \
        -DLLVM_INCLUDE_TOOLS=ON \
        \
        -DCLANG_BUILD_TOOLS=OFF \
        -DCLANG_INCLUDE_TESTS=OFF \
        -DCLANG_INCLUDE_DOCS=OFF \
        -DCLANG_BUILD_EXAMPLES=OFF \
        -DCLANG_ENABLE_ARCMT=OFF \
        -DCLANG_ENABLE_STATIC_ANALYZER=OFF \
        -DCLANG_ENABLE_OBJC_REWRITER=OFF \
        -DCLANG_PLUGIN_SUPPORT=OFF \
        \
        -DZLIB_LIBRARY="$root/vendor/zlib/lib/libz.a" \
        -DLLVM_ENABLE_ZSTD=OFF \
        -DLLVM_ENABLE_ZLIB=FORCE_ON \
        -DZLIB_USE_STATIC_LIBS=ON \
        -DLLVM_ENABLE_LIBXML2=OFF \
        -DLLVM_ENABLE_CURL=OFF \
        -DLLVM_ENABLE_HTTPLIB=OFF \
        -DLLVM_ENABLE_FFI=OFF \
        -DLLVM_ENABLE_LIBEDIT=OFF \
        -DLLVM_ENABLE_Z3_SOLVER=OFF

    echo "-- Building LLVM, Clang, and LLD static libraries..."
    # Build library targets including the C API
    cmake --build "$llvm_build_dir" -j"$core_count" --target \
        llvm-libraries clang-libraries libclang \
        lldCommon lldELF lldCOFF lldMachO lldMinGW lldWasm

    echo "-- Installing all headers to $llvm_install_dir ..."
    cmake --install "$llvm_build_dir" --prefix="$llvm_install_dir" --component llvm-headers
    cmake --install "$llvm_build_dir" --prefix="$llvm_install_dir" --component clang-headers

    echo "-- Manually copying llvm, libclang and the LLD libraries to $llvm_install_dir"
    mkdir -p "$llvm_install_dir/lib"
    read -a llvm_libs <<< "$("$root/vendor/llvm-config/bin/llvm-config" --link-static --libs all | sed "s/-l//g")"
    declare -a to_copy=(
        "clang"
        "lldCommon"
        "lldELF"
        "lldCOFF"
        "lldMachO"
        "lldMinGW"
        "lldWasm"
    )
    for lib in "${llvm_libs[@]}"; do
        echo "Copying lib${lib}.a ..."
        if [ -f "$llvm_build_dir/lib/lib${lib}.a" ]; then
            cp "$llvm_build_dir/lib/lib${lib}.a" "$llvm_install_dir/lib/lib${lib}.a"
        else
            echo "Warning: lib${lib}.a not found, skipping..."
        fi
    done
    for lib in "${to_copy[@]}"; do
        echo "Copying lib${lib}.a ..."
        if [ -f "$llvm_build_dir/lib/lib${lib}.a" ]; then
            cp "$llvm_build_dir/lib/lib${lib}.a" "$llvm_install_dir/lib/lib${lib}.a"
        else
            echo "Warning: lib${lib}.a not found, skipping..."
        fi
    done
}

# Builds llvm-config executable and static libraries for Linux
build_llvm_config() {
    llvm_build_dir="$root/build/llvm-config"
    llvm_install_dir="$root/vendor/llvm-config"
    echo "-- Building llvm-config executable and static libraries for Linux..."
    if [ "$force_rebuild_llvm_config" = "true" ]; then
        rm -rf "$llvm_build_dir"
        rm -rf "$llvm_install_dir"
    fi

    # Create directories
    mkdir -p "$llvm_build_dir"
    mkdir -p "$llvm_install_dir"

    cmake -S vendor/sources/llvm-project/llvm -B "$llvm_build_dir" \
        -DCMAKE_INSTALL_PREFIX="$llvm_install_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SKIP_INSTALL_RPATH=ON \
        -DLLVM_ENABLE_PROJECTS="" \
        -DCMAKE_C_COMPILER="clang" \
        -DCMAKE_CXX_COMPILER="clang++" \
        -DLLVM_TARGET_ARCH=X86 \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        \
        -DLLVM_ENABLE_THREADS=ON \
        -DLLVM_BUILD_STATIC=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DLLVM_ENABLE_PIC=OFF \
        \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_INCLUDE_DOCS=OFF \
        -DLLVM_BUILD_DOCS=OFF \
        -DLLVM_BUILD_UTILS=OFF \
        -DLLVM_INCLUDE_UTILS=OFF \
        -DLLVM_BUILD_RUNTIME=OFF \
        \
        -DLLVM_BUILD_TOOLS=ON \
        -DLLVM_INCLUDE_TOOLS=ON \
        \
        -DZLIB_LIBRARY="$root/vendor/zlib/lib/libz.a" \
        -DLLVM_ENABLE_ZSTD=OFF \
        -DLLVM_ENABLE_ZLIB=FORCE_ON \
        -DZLIB_USE_STATIC_LIBS=ON \
        -DLLVM_ENABLE_LIBXML2=OFF \
        -DLLVM_ENABLE_CURL=OFF \
        -DLLVM_ENABLE_HTTPLIB=OFF \
        -DLLVM_ENABLE_FFI=OFF \
        -DLLVM_ENABLE_LIBEDIT=OFF \
        -DLLVM_ENABLE_Z3_SOLVER=OFF

    echo "-- Building all LLVM static libraries and llvm-config..."
    cmake --build "$llvm_build_dir" -j"$core_count"

    echo "-- Installing llvm-config and libraries to $llvm_install_dir"
    # Install just the llvm-config binary and static libraries manually
    mkdir -p "$llvm_install_dir/bin"
    mkdir -p "$llvm_install_dir/lib"
    cp "$llvm_build_dir/bin/llvm-config" "$llvm_install_dir/bin/"
    cp "$llvm_build_dir/lib/libLLVM"*.a "$llvm_install_dir/lib/" 2>/dev/null || true
}

# Checks if llvm has been cloned, if not does clone it
fetch_llvm() {
    if [ ! -d "$root/vendor/sources/llvm-project" ]; then
        echo "-- Fetching llvm version $llvm_version from GitHub..."
        cd "$root/vendor/sources"
        git clone --depth 1 --branch "$llvm_version" "https://github.com/llvm/llvm-project.git"
        cd "$root"
    fi
}

# Compiles the needed versions of llvm
build_llvm() {
    if ! [ -d "$root/vendor/llvm-config" ] || [ "$force_rebuild_llvm_config" = "true" ]; then
        build_llvm_config
    else
        echo "-- Skip building llvm-config..."
    fi

    if [ "$build_linux" = "true" ]; then
        if ! [ -d "$root/vendor/llvm-linux" ] || [ "$force_rebuild_llvm" = "true" ]; then
            build_llvm_linux
        else
            echo "-- Skip building LLVM for Linux..."
        fi
    fi

    if [ "$build_windows" = "true" ]; then
        if ! [ -d "$root/vendor/llvm-mingw" ] || [ "$force_rebuild_llvm" = "true" ]; then
            build_llvm_windows
        else
            echo "-- Skip building LLVM for Windows..."
        fi
    fi
}

fetch_json_mini() {
    if [ ! -d "$root/vendor/sources/json-mini" ]; then
        echo "-- Fetching json-mini from GitHub..."
        cd "$root/vendor/sources"
        git clone "https://github.com/flint-lang/json-mini.git" "$JSON_MINI_HASH"
        cd "$root"
    else
        # Checking for internet connection
        echo "-- Checking for internet connection..."
        if ping -w 5 -c 2 google.com >/dev/null 2>&1; then
            echo "-- Updating json-mini repository..."
            cd "$root/vendor/sources/json-mini"
            if [ "$(git rev-parse HEAD)" = "$JSON_MINI_HASH" ]; then
                return
            fi
            echo -n "-- "
            git checkout -f main
            git clean -fd
            echo -n "-- "
            git reset
            git fetch
            git pull
            echo "-- Set json-mini to hash '$JSON_MINI_HASH'"
            git checkout -f "$JSON_MINI_HASH" >/dev/null 2>&1
            cd "$root"
        else
            echo "-- No internet connection, skipping updating the json-mini repository..."
        fi
    fi
}

fetch_fip() {
    if [ ! -d "$root/vendor/sources/fip" ]; then
        echo "-- Fetching fip from GitHub..."
        cd "$root/vendor/sources"
        git clone "https://github.com/flint-lang/fip.git" "$FIP_HASH"
        cd "$root"
    else
        # Checking for internet connection
        echo "-- Checking for internet connection..."
        if ping -w 5 -c 2 google.com >/dev/null 2>&1; then
            echo "-- Updating fip repository..."
            cd "$root/vendor/sources/fip"
            if [ "$(git rev-parse HEAD)" = "$FIP_HASH" ]; then
                return
            fi
            echo -n "-- "
            git checkout -f main
            git clean -fd
            echo -n "-- "
            git reset
            git fetch
            git pull
            echo "-- Set fip to hash '$FIP_HASH'"
            git checkout -f "$FIP_HASH" >/dev/null 2>&1
            cd "$root"
        else
            echo "-- No internet connection, skipping updating the fip repository..."
        fi
    fi
}

# Sets up the cmake build directory for windows
# $1 - is_static - Whether to build the dynamic ("false") or the static ("true") version of the compiler
# $2 - is_debug - Whether to build the release ("false") or the debug ("true") build
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
    if [ "$2" = "true" ]; then
        debug_flag="-DDEBUG_MODE=ON"
        build_dir="${build_dir}-debug"
        echo "-- Building executable in debug mode..."
    fi

    if [ -d "$build_dir" ] && [ "$force_rebuild" = "true" ]; then
        rm -r "$build_dir"
    fi

    mkdir -p "$build_dir"
    COMMIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
    BUILD_DATE="$(date +%Y-%m-%d)"
    cmake -S "$root" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchain-mingw64.cmake" \
        -DLLVM_LIB_PATH="$llvm_lib_path" \
        -DCOMMIT_HASH="$COMMIT_HASH" \
        -DBUILD_DATE="$BUILD_DATE" \
        "$static_flag" \
        "$debug_flag" \
        "$verbosity_flag"
}

# Sets up the cmake build directory for linux
# $1 - is_static - Whether to build the dynamic ("false") or the static ("true") version of llvm
# $2 - is_debug - Whether to build the release ("false") or the debug ("true") build
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
    if [ "$2" = "true" ]; then
        debug_flag="-DDEBUG_MODE=ON"
        build_dir="${build_dir}-debug"
        echo "-- Building executable in debug mode..."
    fi

    if [ -d "$build_dir" ] && [ "$force_rebuild" = "true" ]; then
        rm -r "$build_dir"
    fi

    mkdir -p "$build_dir"
    COMMIT_HASH="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
    BUILD_DATE="$(date +%Y-%m-%d)"
    cmake -S "$root" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchain-linux.cmake" \
        -DLLVM_LIB_PATH="$llvm_lib_path" \
        -DCOMMIT_HASH="$COMMIT_HASH" \
        -DBUILD_DATE="$BUILD_DATE" \
        "$static_flag" \
        "$debug_flag" \
        "$verbosity_flag"
}

# Sets up all build directories for all compiler builds
setup_builds() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                setup_build_windows "true" "true"
            fi
            if [ "$build_release" = "true" ]; then
                setup_build_windows "true" "false"
            fi
        fi
        if [ "$build_dynamic" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                setup_build_windows "false" "true"
            fi
            if [ "$build_release" = "true" ]; then
                setup_build_windows "false" "false"
            fi
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                setup_build_linux "true" "true"
            fi
            if [ "$build_release" = "true" ]; then
                setup_build_linux "true" "false"
            fi
        fi
        if [ "$build_dynamic" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                setup_build_linux "false" "true"
            fi
            if [ "$build_release" = "true" ]; then
                setup_build_linux "false" "false"
            fi
        fi
    fi
}

# Builds all requested compilers
build_compilers() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                echo "-- Building static Windows targets in debug mode..."
                cmake --build "$root/build/windows-static-debug" -j"$core_count" --target static
            fi
            if [ "$build_release" = "true" ]; then
                echo "-- Building static Windows targets in release mode..."
                cmake --build "$root/build/windows-static" -j"$core_count" --target static
            fi
        fi
        if [ "$build_dynamic" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                echo "-- Building Windows targets in debug mode..."
                cmake --build "$root/build/windows-debug" -j"$core_count" --target dynamic
            fi
            if [ "$build_release" = "true" ]; then
                echo "-- Building Windows targets in release mode..."
                cmake --build "$root/build/windows" -j"$core_count" --target dynamic
            fi
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                echo "-- Building static Linux targets in debug mode..."
                cmake --build "$root/build/linux-static-debug" -j"$core_count" --target static
            fi
            if [ "$build_release" = "true" ]; then
                echo "-- Building static Linux targets in release mode..."
                cmake --build "$root/build/linux-static" -j"$core_count" --target static
            fi
        fi
        if [ "$build_dynamic" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                echo "-- Building Linux targets in debug mode..."
                cmake --build "$root/build/linux-debug" -j"$core_count" --target dynamic
            fi
            if [ "$build_release" = "true" ]; then
                echo "-- Building Linux targets in release mode..."
                cmake --build "$root/build/linux" -j"$core_count" --target dynamic
            fi
        fi
    fi
}

# Build the Flint Language Server
build_fls() {
    if [ "$build_fls" = "false" ]; then
        return
    fi
    cd "$root/lsp"
    if [ "$build_windows" = "true" ]; then
        echo "-- Building the Flint Language Server for Windows"
        "./build.sh" --windows --clean
    fi
    if [ "$build_linux" = "true" ]; then
        echo "-- Building the Flint Language Server for Linux"
        "./build.sh" --linux --clean
    fi
    cd "$root"
}

# Copies the given file to the given directory, prints a message and fails if copying failed
checked_copy() {
    if ! cp "$1" "$2" &> /dev/null; then
        echo -e "\033[31m-- ERROR: Could not copy '$(basename "$1")' failed because it is in active use!\033[0m"
        exit 1
    fi
}

# Copies all executables from the nested out directories into one out directory, where all outs are saved
copy_executables() {
    if [ "$build_windows" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                checked_copy "$root/build/windows-static-debug/out/flintc.exe" "$root/build/out/flintc.exe"
                checked_copy "$root/build/windows-static-debug/out/tests.exe" "$root/build/out/tests.exe"
            fi
            if [ "$build_release" = "true" ]; then
                checked_copy "$root/build/windows-static/out/flintc.exe" "$root/build/out/flintc-release.exe"
                checked_copy "$root/build/windows-static/out/tests.exe" "$root/build/out/tests-release.exe"
            fi
        fi
        if [ "$build_dynamic" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                checked_copy "$root/build/windows-debug/out/dynamic-flintc.exe" "$root/build/out/dynamic-flintc.exe"
                checked_copy "$root/build/windows-debug/out/dynamic-tests.exe" "$root/build/out/dynamic-tests.exe"
            fi
            if [ "$build_release" = "true" ]; then
                checked_copy "$root/build/windows/out/dynamic-flintc.exe" "$root/build/out/dynamic-flintc-release.exe"
                checked_copy "$root/build/windows/out/dynamic-tests.exe" "$root/build/out/dynamic-tests-release.exe"
            fi
        fi
        if [ "$build_fls" = "true" ]; then
            checked_copy "$root/lsp/build-windows/out/fls.exe" "$root/build/out/fls.exe"
        fi
    fi

    if [ "$build_linux" = "true" ]; then
        if [ "$build_static" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                checked_copy "$root/build/linux-static-debug/out/flintc" "$root/build/out/flintc"
                checked_copy "$root/build/linux-static-debug/out/tests" "$root/build/out/tests"
            fi
            if [ "$build_release" = "true" ]; then
                checked_copy "$root/build/linux-static/out/flintc" "$root/build/out/flintc-release"
                checked_copy "$root/build/linux-static/out/tests" "$root/build/out/tests-release"
            fi
        fi
        if [ "$build_dynamic" = "true" ]; then
            if [ "$build_debug" = "true" ]; then
                checked_copy "$root/build/linux-debug/out/dynamic-flintc" "$root/build/out/dynamic-flintc"
                checked_copy "$root/build/linux-debug/out/dynamic-tests" "$root/build/out/dynamic-tests"
            fi
            if [ "$build_release" = "true" ]; then
                checked_copy "$root/build/linux/out/dynamic-flintc" "$root/build/out/dynamic-flintc-release"
                checked_copy "$root/build/linux/out/dynamic-tests" "$root/build/out/dynamic-tests-release"
            fi
        fi
        if [ "$build_fls" = "true" ]; then
            checked_copy "$root/lsp/build-linux/out/fls" "$root/build/out/fls"
        fi
    fi
}

# Executes all tests to ensure the compiler is in a working state
run_tests() {
    if [ "$build_linux" = "true" ]; then
        if [ "$build_release" = "true" ]; then
            cd "$root/test_files"
            flintc --file wiki_tests.ft --test --run
            cd "$root"
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
build_fls=false
force_rebuild=false
force_rebuild_llvm=false
force_rebuild_llvm_config=false
run_tests=false
build_debug=false
build_release=false
llvm_version="llvmorg-19.1.7"
zlib_version="v1.3.1"
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
    --all)
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
        build_debug=true
        shift
        ;;
    --release)
        build_release=true
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
    --fls)
        build_fls=true
        shift
        ;;
    --rebuild)
        force_rebuild=true
        shift
        ;;
    --rebuild-llvm)
        force_rebuild_llvm=true
        shift
        ;;
    --rebuild-config)
        force_rebuild_llvm_config=true
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
    -j)
        [ "$2" != "" ] || err_exit 1 "Option '$1' requires an argument"
        core_count="$2"
        shift 2
        ;;
    --*) # Handle long options
        err_exit 1 "Unknown cli argument: '$1'"
        ;;
    -*) # Handle short options, including combined ones
        while getopts "adDflrRstwv" arg; do
            case "$arg" in
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
                build_debug=true
                ;;
            f)
                build_fls=true
                ;;
            l)
                build_linux=true
                ;;
            r)
                force_rebuild=true
                ;;
            R)
                build_release=true
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
                err_exit 1 "Unknown option: -$arg"
                ;;
            esac
        done
        shift
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

# Default to debug build if no flag is specified
if [ "$build_debug" = "false" ] && [ "$build_release" = "false" ]; then
    build_debug=true
fi

echo "-- Creating necessary directories..."
create_directories

echo "-- Checking Library requirements..."
check_library_requirements

echo "-- Checking if zlib has been fetched yet..."
fetch_zlib

echo "-- Building zlib..."
build_zlib

echo "-- Checking if LLVM has been fetched yet..."
fetch_llvm

echo "-- Building all required llvm versions..."
build_llvm

echo "-- Making sure json-mini is up to date..."
fetch_json_mini

echo "-- Making sure fip is up to date..."
fetch_fip

echo "-- Starting CMake configuration phase..."
setup_builds

echo "-- Starting CMake build phase..."
build_compilers
build_fls

echo "-- Putting the built binaries into the 'build/out' directory..."
copy_executables

if [ "$run_tests" = "true" ]; then
    echo "-- Running all tests..."
    run_tests
fi

echo "-- Build finished! Look at 'build/out' to see the built binaries!"
