#!/usr/bin/env sh

# --- THIS PROGRAM WILL BUILLD ALL TARGETS ---

# Print error and exit program with specified error code
err_exit() {
    echo "Error: $2"
    exit $1
}

# Get the projects root directory (use cd + dirname + pwd to always get absolute path, even when it is called relatively)
root="$(cd "$(dirname "$0")" && cd .. && pwd)"

# First, ensure that the build/ directory exists!
mkdir -p "$root/build"

echo "-- Checking Library requirements"

# Then, ensure that the GLIBCPATH and ZLIBPATH environment variables are set and correct
[ "$GLIBCPATH" = "" ] && err_exit 1 "'GLIBCPATH' environment variable not set!"
[ "$ZLIBPATH" = "" ] && err_exit 1 "'ZLIBPATH' environment variable not set!"

# Then, check if the correct content is present in these directories (the .a files)
glibc_files=$(ls "$GLIBCPATH")
zlib_files=$(ls "$ZLIBPATH")

if [ "$(echo "$glibc_files" | grep "librt.a")" = "" ]; then
    err_exit 1 "Library 'librt.a' not present in GLIBCPATH: '$GLIBCPATH'"
fi
if [ "$(echo "$glibc_files" | grep "libdl.a")" = "" ]; then
    err_exit 1 "Library 'libdl.a' not present in GLIBCPATH: '$GLIBCPATH'"
fi
if [ "$(echo "$glibc_files" | grep "libm.a")" = "" ]; then
    err_exit 1 "Library 'libm.a' not present in GLIBCPATH: '$GLIBCPATH'"
fi

if [ "$(echo "$zlib_files" | grep "libz.a")" = "" ]; then
    err_exit 1 "Library 'libz.a' not present in ZLIBPATH: '$ZLIBPATH'"
fi

# Finally, check if xml2 has been built and if libxml2.a is available
if [ ! -d "$root/xml2/xml2" ] || [ ! -d "$root/xml2/xml2/lib" ]; then
    err_exit 1 "XML2 has not been built manually in the xml2 directory! Check the README.md in the xml2 directory for further information!"
fi
if [ "$(ls "$root/xml2/xml2/lib" | grep "libxml2.a")" = "" ]; then
    err_exit 1 "Required static library 'libxml2.a' not found in the xml2/xml2/lib directory!"
fi

echo "-- Starting CMake build process"

# Then, start the configuring phase of CMake
cmake -S . -B "$root/build"

# And finally, build the cmake targets
cmake --build "$root/build" --target static

echo "-- Build finished! Look at 'build/out' to see the built binaries"
