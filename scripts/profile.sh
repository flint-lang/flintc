#!/usr/bin/env sh

# Print error and exit program with specified error code
err_exit() {
    echo "Error: $2"
    exit "$1"
}

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "profile [Option] <executable> [arguments with which to call the executable]*"
    echo "Options:"
    echo "  valgrind        Uses valgrind for performance analysis"
    echo "  perf            Uses perf for performance analysis"
fi

option="valgrind"
if [ "$1" = "valgrind" ]; then
    shift
elif [ "$1" = "perf" ]; then
    option="perf"
    shift
fi

if [ "$option" = "perf" ]; then
    # Make sure that 'perf' is installed
    [ ! -x "$(command -v perf)" ] && err_exit 1 "'perf' is not installed on your system! You can install it with your package manager (e.g. sudo apt install -y perf)"

    # Make sure that 'flamegraph.pl' is installed
    [ ! -x "$(command -v flamegraph.pl)" ] && err_exit 1 "'flamegraph.pl' is not installed on your system! You can install it with your package manager (e.g. sudo apt install -y flamegraph)"

    # Make sure that 'stackcollapse-perf.pl' is installed
    [ ! -x "$(command -v stackcollapse-perf.pl)" ] && err_exit 1 "'stackcollapse-perf.pl' is not installed on your system! You can install it with your package manager (e.g. sudo apt install -y perf)"
else
    # Make sure that valgrind is installed
    [ ! -x "$(command -v valgrind)" ] && err_exit 1 "'valgrind' is not installed!"

    # Make sure that kcachegrind is installed
    [ ! -x "$(command -v kcachegrind)" ] && err_exit 1 "'kcachegrind' is not installed!"
fi

# Make sure that at least one cli argument (the file to profile) is given
[ "$#" -gt 0 ] || err_exit 1 "No file given thats profilable! ('$1')"
file="$1"
shift

# Make sure that the executable ($1) exists
[ ! -x "$file" ] && err_exit 1 "Executable not found or not executable!"

# Make sure the profiling directory exists but is empty
mkdir -p "/tmp/profiling"
rm -f "/tmp/profiling/"*

echo "-- Starting Profiling"

# Collect performance data with perf
if [ "$option" = "perf" ]; then
    # shellcheck disable=SC2068
    perf record --call-graph fp --no-buildid-cache --no-buildid --all-user -F 10000 -g -o "/tmp/profiling/perf.data" "$file" $@

    # Generate the flamegraph
    echo "-- Generating flamegraph..."
    perf script -i "/tmp/profiling/perf.data" | stackcollapse-perf.pl > "/tmp/profiling/out.folded"
    flamegraph.pl "/tmp/profiling/out.folded" > "/tmp/profiling/out.svg"

    # Open flamegraph
    firefox "/tmp/profiling/out.svg"
else
    # Call valgrind
    # shellcheck disable=SC2068
    valgrind --tool=callgrind --callgrind-out-file=/tmp/profiling/callgrind.out "$file" $@

    # Open the out file with kcachegrind
    kcachegrind /tmp/profiling/callgrind.out &
fi
