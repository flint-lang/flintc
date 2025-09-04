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
        send_completion_response(content);
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

void LspServer::send_completion_response(const std::string &content) {
    // DEBUG: Log the actual request content
    log_info("Full completion request content (first 500 chars): " + content.substr(0, std::min(content.length(), size_t(500))));

    // Extract file context from the request
    std::string request_id = extract_request_id(content);
    std::string file_uri = extract_file_uri(content);
    std::string file_path = uri_to_file_path(file_uri);
    auto position = extract_position(request_id);

    log_info("Completion request for file: " + file_path + " at line " + std::to_string(position.first) + ", char " +
        std::to_string(position.second));

    // Get context-aware completions
    std::vector<CompletionItem> completions;

    if (!file_path.empty() && file_path.size() > 3 && file_path.substr(file_path.size() - 3) == LspProtocol::FLINT_EXTENSION) {
        // Parse the file to get context-specific completions
        completions = get_context_aware_completions(file_path, position.first, position.second);
    } else {
        // Fallback to static completions
        completions = CompletionData::get_all_completions();
    }

    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id
             << R"(,
  "result": {
    "isIncomplete": false,
    "items": )"
             << completion_items_to_json_array(completions) << R"(
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

std::string LspServer::extract_file_uri(const std::string &content) {
    // Look for "textDocument":{"uri":"file://..."
    size_t uri_start = content.find("\"uri\":\"");
    if (uri_start != std::string::npos) {
        uri_start += 7; // Skip past "uri":"
        size_t uri_end = content.find("\"", uri_start);
        if (uri_end != std::string::npos) {
            return content.substr(uri_start, uri_end - uri_start);
        }
    }
    return "";
}

std::pair<int, int> LspServer::extract_position(const std::string &content) {
    size_t pos_start = content.find("\"position\":");
    if (pos_start == std::string::npos) {
        return {-1, -1};
    }

    // Find the opening brace after "position":
    size_t brace_start = content.find("{", pos_start);
    if (brace_start == std::string::npos) {
        return {-1, -1};
    }

    // Extract line
    size_t line_pos = content.find("\"line\":", brace_start);
    if (line_pos == std::string::npos) {
        return {-1, -1};
    }

    line_pos += 7; // Skip "line":
    size_t line_end = content.find_first_of(",}", line_pos);
    if (line_end == std::string::npos) {
        return {-1, -1};
    }

    // Extract character
    size_t char_pos = content.find("\"character\":", line_end);
    if (char_pos == std::string::npos) {
        return {-1, -1};
    }

    char_pos += 12; // Skip "character":
    size_t char_end = content.find_first_of(",}", char_pos);
    if (char_end == std::string::npos) {
        return {-1, -1};
    }

    try {
        int line = std::stoi(content.substr(line_pos, line_end - line_pos));
        int character = std::stoi(content.substr(char_pos, char_end - char_pos));
        return {line, character};
    } catch (...) { return {-1, -1}; }
}

std::string LspServer::uri_to_file_path(const std::string &uri) {
    // Convert file://path to path
    if (uri.substr(0, 7) == "file://") {
        return uri.substr(7); // Remove "file://" prefix
    }
    return uri;
}
