#include "lsp_server.hpp"
#include "completion_data.hpp"
#include "lsp_protocol.hpp"

#include <iostream>
#include <sstream>
#include <string>

void LspServer::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        // Basic LSP message handling
        if (line.find("Content-Length:") == 0) {
            // Extract content length
            size_t length = std::stoul(line.substr(16));

            // Skip empty line
            std::getline(std::cin, line);

            // Read message content
            std::string content(length, '\0');
            std::cin.read(&content[0], length);

            // Process the message
            process_message(content);
        }
    }
}

void LspServer::process_message(const std::string &content) {
    // Basic JSON-RPC message handling
    if (contains_method(content, LspProtocol::METHOD_INITIALIZE)) {
        std::string request_id = extract_request_id(content);
        send_initialize_response(request_id);
    } else if (contains_method(content, LspProtocol::METHOD_INITIALIZED)) {
        log_info("LSP Server initialized");
    } else if (contains_method(content, LspProtocol::METHOD_SHUTDOWN)) {
        std::string request_id = extract_request_id(content);
        send_shutdown_response(request_id);
    } else if (contains_method(content, LspProtocol::METHOD_EXIT)) {
        exit(0);
    } else if (contains_method(content, LspProtocol::METHOD_TEXT_DOCUMENT_DID_OPEN)) {
        handle_document_open(content);
    } else if (contains_method(content, LspProtocol::METHOD_TEXT_DOCUMENT_DID_CHANGE)) {
        handle_document_change(content);
    } else if (contains_method(content, LspProtocol::METHOD_TEXT_DOCUMENT_COMPLETION)) {
        std::string request_id = extract_request_id(content);
        send_completion_response(request_id);
    } else if (contains_method(content, LspProtocol::METHOD_TEXT_DOCUMENT_HOVER)) {
        std::string request_id = extract_request_id(content);
        send_hover_response(request_id);
    }
}

void LspServer::send_initialize_response(const std::string &request_id) {
    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id
             << R"(,
  "result": {
    "capabilities": {
      "textDocumentSync": 1,
      "completionProvider": {
        "triggerCharacters": ["."]
      },
      "hoverProvider": true,
      "definitionProvider": true,
      "documentSymbolProvider": true
    },
    "serverInfo": {
      "name": ")"
             << LspProtocol::SERVER_NAME << R"(",
      "version": ")"
             << LspProtocol::SERVER_VERSION << R"("
    }
  }
})";
    send_lsp_response(response.str());
}

void LspServer::send_shutdown_response(const std::string &request_id) {
    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id
             << R"(,
  "result": null
})";
    send_lsp_response(response.str());
}

void LspServer::send_completion_response(const std::string &request_id) {
    // Get all completion items from the data store
    auto all_completions = CompletionData::get_all_completions();

    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id
             << R"(,
  "result": {
    "isIncomplete": false,
    "items": )"
             << completion_items_to_json_array(all_completions) << R"(
  }
})";
    send_lsp_response(response.str());
}

void LspServer::send_hover_response(const std::string &request_id) {
    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id
             << R"(,
  "result": {
    "contents": {
      "kind": "markdown",
      "value": "**Flint Language**\n\nHover information for Flint language constructs.\n\nFlint uses:\n- `def` for functions\n- `data` for structures\n- `test` for test blocks\n- `)"
             << LspProtocol::FLINT_EXTENSION << R"(` file extension"
    }
  }
})";
    send_lsp_response(response.str());
}

void LspServer::handle_document_open(const std::string &content) {
    if (content.find(LspProtocol::FLINT_EXTENSION) != std::string::npos) {
        log_info("Flint document (.ft) opened");
    } else {
        log_info("Document opened");
    }
}

void LspServer::handle_document_change(const std::string &content) {
    log_info("Document changed");
}

void LspServer::log_info(const std::string &message) {
    std::cerr << "[INFO] " << message << std::endl;
}
