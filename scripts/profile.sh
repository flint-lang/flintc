#!/usr/bin/env sh

# Print error and exit program with specified error code
err_exit() {
    echo "Error: $2"
    exit "$1"
}

# Get the projects root directory (use cd + dirname + pwd to always get absolute path, even when it is called relatively)
root="$(cd "$(dirname "$0")" && cd .. && pwd)"

# Make sure that 'perf' is installed
[ ! -x "$(command -v perf)" ] && err_exit 1 "'perf' is not installed on your system! You can install it with your package manager (e.g. sudo apt install -y perf)"

# Make sure that 'flamegraph.pl' is installed
[ ! -x "$(command -v flamegraph.pl)" ] && err_exit 1 "'flamegraph.pl' is not installed on your system! You can install it with your package manager (e.g. sudo apt install -y flamegraph)"

# Make sure that 'stackcollapse-perf.pl' is installed
[ ! -x "$(command -v stackcollapse-perf.pl)" ] && err_exit 1 "'stackcollapse-perf.pl' is not installed on your system! You can install it with your package manager (e.g. sudo apt install -y perf)"

# Make sure that the executable (flintc) exists in the "$root/build/out/" directory
[ ! -x "$root/build/out/flintc" ] && err_exit 1 "Executable 'flintc' not found in '$root/build/out/' directory!"

# Make sure that cli arguments are given
[ "$#" -eq 0 ] && err_exit 1 "No cli arguments given!"

# Make sure the profiling directory exists but is empty
mkdir -p "$root/scripts/profiling"
rm -f "$root/scripts/profiling/"*

echo "-- Starting Profiling"

# Collect performance data with perf and forward all cli arguments of this script
# shellcheck disable=SC2068 # We want to forward all arguments
sudo perf record --call-graph dwarf --all-user -F 9999 -g -o "$root/scripts/profiling/perf.data" "$root/build/out/flintc" $@

# Generate the svg file
sudo perf script -i "$root/scripts/profiling/perf.data" | stackcollapse-perf.pl | flamegraph.pl > "$root/scripts/profiling/out.svg"

# Visualize the flamegraph
firefox "$root/scripts/profiling/out.svg"
