#include "lsp_server.hpp"
#include "completion_data.hpp"
#include "globals.hpp"
#include "lsp_protocol.hpp"
#include "parser/parser.hpp"
#include "profiler.hpp"

#include <iostream>
#include <sstream>
#include <string>

std::vector<Diagnostic> diagnostics;

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

void parser_cleanup() {
    Profiler::end_task("ALL");
    Profiler::root_nodes.clear();
    while (!Profiler::profile_stack.empty()) {
        Profiler::profile_stack.pop();
    }
    Profiler::active_tasks.clear();
}

std::optional<FileNode *> LspServer::parse_program(const std::string &source_file_path, const std::optional<std::string> &file_content) {
    static std::mutex parsing_mutex;
    std::lock_guard<std::mutex> lock(parsing_mutex);
    std::filesystem::path file_path(source_file_path);
    const bool parse_parallel = false;
    // Clear all internal state before parsing so that the internal state is valid after we are done with this function
    Resolver::max_graph_depth = 1;
    Resolver::clear();
    Parser::clear_instances();
    Type::clear_types();
    diagnostics.clear();

    Profiler::start_task("ALL");
    Type::init_types();
    Resolver::add_path(file_path.filename().string(), file_path.parent_path());
    std::optional<FileNode *> file;
    if (file_content.has_value()) {
        file = Parser::create(file_path, file_content.value())->parse();
    } else {
        file = Parser::create(file_path)->parse();
    }
    if (!file.has_value()) {
        std::cerr << RED << "Error" << DEFAULT << ": Failed to parse file " << YELLOW << file_path.filename() << DEFAULT << std::endl;
        parser_cleanup();
        return std::nullopt;
    }
    auto dep_graph = Resolver::create_dependency_graph(file.value(), file_path.parent_path(), parse_parallel);
    Parser::resolve_all_unknown_types();
    bool parsed_successful = Parser::parse_all_open_functions(parse_parallel);
    if (!parsed_successful) {
        parser_cleanup();
        return std::nullopt;
    }

    parser_cleanup();
    return file.value();
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
             << "v" << MAJOR << "." << MINOR << "." << PATCH << "-" << VERSION << R"("
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
    auto position = extract_position(content);

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

void LspServer::publish_diagnostics(const std::string &file_uri) {
    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "method": "textDocument/publishDiagnostics",
  "params": {
    "uri": ")"
             << file_uri << R"(",
    "diagnostics": )"
             << diagnostics_to_json_array() << R"(
  }
})";

    send_lsp_response(response.str());
    log_info("Published " + std::to_string(diagnostics.size()) + " diagnostics for " + file_uri);
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
    std::string file_uri = extract_file_uri(content);
    std::string file_path = uri_to_file_path(file_uri);

    if (content.find(LspProtocol::FLINT_EXTENSION) != std::string::npos) {
        log_info("Flint document (.ft) opened");

        // Parse the file and publish diagnostics
        LspServer::parse_program(file_path, std::nullopt);
        publish_diagnostics(file_uri);
    } else {
        log_info("Document opened");
    }
}

void LspServer::handle_document_change(const std::string &content) {
    std::string file_uri = extract_file_uri(content);
    std::string file_path = uri_to_file_path(file_uri);

    if (content.find(LspProtocol::FLINT_EXTENSION) != std::string::npos) {
        log_info("Flint document (.ft) changed");

        // Parse the file and publish diagnostics
        std::string file_content = extract_file_content_from_change(content);
        LspServer::parse_program(file_path, file_content);
        publish_diagnostics(file_uri);
    } else {
        log_info("Document changed");
    }
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

std::string LspServer::extract_file_content_from_change(const std::string &content) {
    // Look for "contentChanges":[{"text":"..."}]
    size_t changes_start = content.find("\"contentChanges\":");
    if (changes_start == std::string::npos) {
        return "";
    }

    // Find the first "text" field in the changes array
    size_t text_start = content.find("\"text\":\"", changes_start);
    if (text_start == std::string::npos) {
        return "";
    }

    text_start += 8; // Skip past "text":"

    // Find the closing quote, handling escaped quotes
    size_t text_end = text_start;
    while (text_end < content.length()) {
        if (content[text_end] == '"' && (text_end == text_start || content[text_end - 1] != '\\')) {
            break;
        }
        text_end++;
    }

    if (text_end >= content.length()) {
        return "";
    }

    std::string raw_content = content.substr(text_start, text_end - text_start);

    // Unescape JSON string
    return unescape_json_string(raw_content);
}

// Helper function to unescape JSON strings
std::string LspServer::unescape_json_string(const std::string &escaped) {
    std::string result;
    result.reserve(escaped.length());

    for (size_t i = 0; i < escaped.length(); ++i) {
        if (escaped[i] == '\\' && i + 1 < escaped.length()) {
            switch (escaped[i + 1]) {
                case 'n':
                    result += '\n';
                    i++;
                    break;
                case 't':
                    result += '\t';
                    i++;
                    break;
                case 'r':
                    result += '\r';
                    i++;
                    break;
                case '"':
                    result += '"';
                    i++;
                    break;
                case '\\':
                    result += '\\';
                    i++;
                    break;
                default:
                    result += escaped[i];
                    break;
            }
        } else {
            result += escaped[i];
        }
    }

    return result;
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
