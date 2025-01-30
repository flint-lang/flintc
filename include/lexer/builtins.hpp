#ifndef __BUILTINS_HPP__
#define __BUILTINS_HPP__

#include <string_view>
#include <unordered_map>

enum BuiltinFunctions { //
    PRINT,
    PRINT_ERR,
    ASSERT,
    ASSERT_ARG,
    RUN_ON_ALL,
    MAP_ON_ALL,
    FILTER_ON_ALL,
    REDUCE_ON_ALL,
    REDUCE_ON_PAIRS,
    PARTITION_ON_ALL,
    SPLIT_ON_ALL,
};

static const std::unordered_map<std::string_view, BuiltinFunctions> builtin_functions = {
    // printing
    {"print", PRINT},        //
    {"printerr", PRINT_ERR}, //
    // assertions
    {"assert", ASSERT},         //
    {"assert_arg", ASSERT_ARG}, //
    // concurrency
    {"run_on_all", RUN_ON_ALL},            //
    {"map_on_all", MAP_ON_ALL},            //
    {"filter_on_all", FILTER_ON_ALL},      //
    {"reduce_on_all", REDUCE_ON_ALL},      //
    {"reduce_on_pairs", REDUCE_ON_PAIRS},  //
    {"partition_on_all", PARTITION_ON_ALL} //
};

#endif
