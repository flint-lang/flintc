#ifndef __CLI_PARSER_BASE_HPP__
#define __CLI_PARSER_BASE_HPP__

#include <filesystem>
#include <iostream>
#include <sys/types.h>
#include <vector>

/// CommandLineParser
///     Parses all the command line arguments and saves their values locally, accessible from outside
class CLIParserBase {
  public:
    virtual ~CLIParserBase() = default;

    // Pure virtual function
    virtual int parse() = 0;

  protected:
    // Protected constructor - only derived classes can use it
    CLIParserBase(int argc, char *argv[]) {
        // Convert the char* argv[] to a vector of strings
        args.reserve(argc - 1);
        for (size_t i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
    }

    // Protected member variables accessible to derived classes
    std::vector<std::string> args;
    std::string argument_value;

    // Pure virtual functions
    virtual void print_help() = 0;

    virtual int print_err(const std::string &err) {
        static const std::string RED = "\033[31m";
        static const std::string DEFAULT = "\033[0m";
        std::cerr << "-- " << RED << "Error: " << DEFAULT << err << "\n";
        print_help();
        return 1;
    }

    bool n_args_follow(const unsigned int count, const std::string &arg, const std::string &option) {
        if (args.size() <= count) {
            std::cerr << "Expected " << arg << " after '" << option << "' option!\n";
            print_help();
            return false;
        }
        return true;
    }

    static std::filesystem::path get_absolute(const std::filesystem::path &cwd, const std::string &path) {
        std::filesystem::path file_path;
        if (path[0] == '/') {
            // Absolute path
            file_path = path;
        } else {
            // Relative path
            file_path = cwd / path;
        }
        return file_path;
    }
};

#endif
