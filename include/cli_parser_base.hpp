#pragma once

#include <array>
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

    /// get_command_output
    ///     Executes an command and returns the exit code as well as the output of the command
    static std::tuple<int, std::string> get_command_output(const std::string &command) {
        constexpr std::size_t BUFFER_SIZE = 128;
        std::string output;
        FILE *pipe = popen((command + " 2>&1").c_str(), "r");

        if (pipe == nullptr) {
            throw std::runtime_error("popen() failed!");
        }

        try {
            std::array<char, BUFFER_SIZE> buffer{};
            while (fgets(buffer.data(), BUFFER_SIZE, pipe) != nullptr) {
                output.append(buffer.data());
            }

            int status = pclose(pipe);

            if (status == -1) {
                throw std::runtime_error("pclose() failed!");
            }

            // On both Windows and Unix-like systems, the exit code will be in the lowest 8 bits of the status
            int exit_code = status & 0xFF;

            return {exit_code, output};
        } catch (...) {
            pclose(pipe);
            throw;
        }
    }

  protected:
    // Protected constructor - only derived classes can use it
    CLIParserBase(int argc, char *argv[]) {
        // Convert the char* argv[] to a vector of strings
        args.reserve(argc - 1);
        for (int i = 1; i < argc; ++i) {
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

    bool starts_with(const std::string &str, const std::string &prefix) {
        if (str.length() < prefix.length()) {
            return false;
        }
        return str.compare(0, prefix.length(), prefix) == 0;
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
