#!/usr/bin/env bash

# This shell builds the flintc compiler
clear

files=($(find . -type f -name "*.cpp"))

echo "Compiling the program..."

clang++ -g -fsanitize=address -fsanitize=undefined -fdebug-info-for-profiling ${files[@]} -o ./main || exit 1




printErr() {
    echo "Error: $2" >&2
    exit "$1"
}


while [ "$#" -gt 0 ]; do
    case "$1" in
        --run)
            [ "$2" != "" ] || printErr 1 "Option '$1' requires an argument"
            echo
            echo "EXECUTING './main --file $2"
            echo
            ./main --file "$2"
            shift 2 ;;
        --test)
            echo
            echo "EXECUTING './main --test'"
            echo
            ./main --test
            shift 1 ;;
        *)
            printErr 1 "Compiled successfully, but no option was provided to run the program!" ;;
    esac
done

