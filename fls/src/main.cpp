#include "lsp_protocol.hpp"
#include "lsp_server.hpp"

#include "fip.hpp"
#include "globals.hpp"

#include <iostream>
#include <string>

#ifdef _WIN32
#include <fcntl.h> // _O_BINARY
#include <io.h>    // _setmode, _fileno
#endif

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
    std::cout << "fls " << MAJOR << "." << MINOR << "." << PATCH << "-" << VERSION << " (" << COMMIT_HASH_VALUE << ", " << BUILD_DATE
              << ")";
    if (DEBUG_MODE) {
        std::cout << " [debug]";
    }
    std::cout << " LSP v" << LspProtocol::PROTOCOL_VERSION << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
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

#ifdef _WIN32
    // Disable CRLF <-> LF translations so LSP headers are sent raw bytes.
    // Must be done before any LSP stdio I/O happens.
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    // Start FIP
    if (!FIP::init()) {
        return 1;
    }
    // Create and run the LSP server
    LspServer::run();

    FIP::shutdown();
    return 0;
}
