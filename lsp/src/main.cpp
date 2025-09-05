#include "lsp_protocol.hpp"
#include "lsp_server.hpp"

#include "globals.hpp"

#include <iostream>
#include <string>

void print_help() {
    std::cout << "Usage: fls [OPTIONS]\n\n"                                                       //
              << "Available Options:\n"                                                           //
              << "  --help, -h        Shows this help message\n"                                  //
              << "  --version, -v     Prints the version information\n\n"                         //
              << "The Flint Language Server operates over stdio, so you actually don't need to\n" //
              << "execute it manually, the Language Clients should start it instead."             //
              << std::endl;
}

void print_version() {
    std::cout << "fls v" << MAJOR << "." << MINOR << "." << PATCH << "-" << VERSION << ", LSP v" << LspProtocol::PROTOCOL_VERSION
              << std::endl;
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            print_version();
            return 0;
        } else {
            std::cerr << "Unknown CLI option: " << arg << std::endl;
            return 1;
        }
    }

    // Create and run the LSP server
    LspServer server;
    server.run();

    return 0;
}
