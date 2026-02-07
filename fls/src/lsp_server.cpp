#include "lsp_server.hpp"
#include "completion_data.hpp"
#include "globals.hpp"
#include "lexer/lexer.hpp"
#include "lsp_protocol.hpp"
#include "parser/parser.hpp"
#include "profiler.hpp"

#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/type.hpp"
#include "parser/type/variant_type.hpp"

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
    FIP::shutdown();
}

std::optional<FileNode *> LspServer::parse_program(const std::string &source_file_path, const std::optional<std::string> &file_content) {
    log_info("Parsing file path: " + source_file_path);
    static std::mutex parsing_mutex;
    std::lock_guard<std::mutex> lock(parsing_mutex);
    std::filesystem::path file_path(source_file_path);
    const bool parse_parallel = false;
    // Clear all internal state before parsing so that the internal state is valid after we are done with this function
    // LSP optimization: only parse aliased imports transitively
    Resolver::minimal_tree = true;
    Resolver::clear();
    Parser::clear_instances();
    Type::clear_types();
    diagnostics.clear();

    Profiler::start_task("ALL");
    Type::init_types();
    static bool core_modules_initialized = false;
    if (!core_modules_initialized) {
        Parser::init_core_modules();
        core_modules_initialized = true;
    }
    // Set the "main" file to the current source file being parsed
    main_file_path = source_file_path;
    std::optional<FileNode *> file;
    if (file_content.has_value()) {
        file = Parser::create(file_path, file_content.value())->parse();
    } else {
        std::optional<Parser *> parser = Parser::create(file_path);
        if (!parser.has_value()) {
            std::cerr << RED << "Error" << DEFAULT << ": The file " << YELLOW << file_path.relative_path().string() << DEFAULT
                      << " does not exist" << std::endl;
            return std::nullopt;
        }
        file = parser.value()->parse();
    }
    if (!file.has_value()) {
        std::cerr << RED << "Error" << DEFAULT << ": Failed to parse file " << YELLOW << file_path.filename().string() << DEFAULT
                  << std::endl;
        parser_cleanup();
        return std::nullopt;
    }
    auto dep_graph = Resolver::create_dependency_graph(file.value(), parse_parallel);
    if (!Parser::resolve_all_imports()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    if (!Parser::resolve_all_unknown_types()) {
        THROW_BASIC_ERR(ERR_PARSING);
        return std::nullopt;
    }
    bool parsed_successful = Parser::parse_all_open_func_modules(parse_parallel);
    if (!parsed_successful) {
        parser_cleanup();
        return std::nullopt;
    }
    parsed_successful = Parser::parse_all_open_entities(parse_parallel);
    if (!parsed_successful) {
        parser_cleanup();
        return std::nullopt;
    }
    parsed_successful = Parser::parse_all_open_functions(parse_parallel);
    if (!parsed_successful) {
        parser_cleanup();
        return std::nullopt;
    }

    parser_cleanup();
    return file.value();
}

void LspServer::process_message(const std::string &content) {
    // Basic JSON-RPC message handling
    if (DEBUG_MODE) {
        log_info("PROCESS_MESSAGE: '" + content + "'\n");
    }
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
    } else if (contains_method(content, LspProtocol::METHOD_TEXT_DOCUMENT_DID_SAVE)) {
        handle_document_save(content);
    } else if (contains_method(content, LspProtocol::METHOD_TEXT_DOCUMENT_COMPLETION)) {
        send_completion_response(content);
    } else if (contains_method(content, LspProtocol::METHOD_TEXT_DOCUMENT_DEFINITION)) {
        send_definition_response(content);
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
      "textDocumentSync": {
        "openClose": true,
        "change": 1,
        "save": {
          "includeText": false
        }
      },
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

void LspServer::send_definition_response(const std::string &content) {
    const std::string request_id = extract_request_id(content);
    const std::string file_uri = extract_file_uri(content);
    const std::string file_path = uri_to_file_path(file_uri);
    const auto position = extract_position(content);

    log_info("Definition request for file: " + file_path + " at line " + std::to_string(position.first) + ", char " +
        std::to_string(position.second));
    log_info("Content of the definition request: " + content);

    // Find the definition
    const auto definition = find_definition_at_position(file_path, position.first, position.second);

    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id
             << R"(,
  "result": )";

    if (definition.has_value() && !std::get<0>(definition.value()).empty()) {
        // Convert file path back to URI
        const Hash &file_hash = std::get<0>(definition.value());
        const std::string def_uri = "file://" + file_hash.path.string();
        response << R"({
    "uri": ")" << def_uri
                 << R"(",
    "range": {
      "start": {"line": )"
                 << std::get<1>(definition.value()) << R"(, "character": )" << std::get<2>(definition.value()) << R"(},
      "end": {"line": )"
                 << std::get<1>(definition.value()) << R"(, "character": )" << std::get<2>(definition.value()) << R"(}
    }
  })";
    } else {
        response << "null";
    }

    response << "\n}";
    log_info("DEFINITION_RESPONSE_BEGIN" + response.str() + " |DEFINTION_RESPONSE_END");
    send_lsp_response(response.str());
}

std::optional<std::tuple<Hash, int, int>> LspServer::find_definition_at_position( //
    const std::string &file_path,                                                 //
    int line,                                                                     //
    int character                                                                 //
) {
    if (line == -1) {
        log_info("DEFINITION: LINE -1");
        return std::nullopt;
    }
    if (character == -1) {
        log_info("DEFINITION: CHARACTER -1");
        return std::nullopt;
    }
    log_info("DEFINITION: Begin");

    // Parse the program to get the AST
    std::optional<FileNode *> file = parse_program(file_path, std::nullopt);
    if (!file.has_value()) {
        return std::nullopt;
    }
    log_info("DEFINITION: After Parsing");

    // First we need to find which token contains the requested position. If the identfiier `MyData` is clicked and it has character
    // positions like these:
    // M  y  D  a  t  a
    // 10 11 12 13 14 15
    // then it is possible for the definition request to have any position from 10-15. This means we first need to look at tokens from the
    // requested line, check the indentation level and calculate the column offset, as the compiler uses column internal, not character, and
    // then we can get the actual clicked identifier. After we have the identifier we can first try to get a type of the same identifier,
    // and if that does not work then we can see if a function of that identifier exists.
    std::filesystem::path source_file_path(file_path);
    log_info("[DEFINITION] file_name = " + source_file_path.filename().string());
    const Parser *parser = Parser::get_instance_from_hash(Hash(source_file_path)).value();
    const auto &lines = parser->get_source_code_lines();
    log_info("[DEFINITION] lines.size() = " + std::to_string(lines.size()));
    log_info("[DEFINITION] line = " + std::to_string(line));
    const auto &[indent_lvl, line_slice] = lines.at(line);
    int identifier_start = character - indent_lvl;
    log_info("[DEFINITION] identifier_start = " + std::to_string(identifier_start));
    std::string_view identifier;
    while (Lexer::is_alpha_num(line_slice[identifier_start])) {
        identifier_start--;
    }
    identifier_start++;
    int identifier_end = identifier_start;
    while (Lexer::is_alpha_num(line_slice[identifier_end])) {
        identifier_end++;
    }
    const int str_len = identifier_end - identifier_start;
    identifier = line_slice.substr(identifier_start, str_len);

    // Now that we have the identifier we can actually directly check if any type of that identifier already exists
    const std::string identifier_str(identifier);
    log_info("[DEFINITION] identifier: '" + identifier_str + "'");
    const Namespace *file_namespace = parser->file_node_ptr->file_namespace.get();
    std::optional<std::shared_ptr<Type>> type = file_namespace->get_type_from_str(identifier_str);
    if (type.has_value()) {
        log_info("[DEFINITION] is type");
        // The identifier is a type, so we can now check which type it is and then get where it was defined at
        switch (type.value()->get_variation()) {
            default:
                // All other types are not defined at the definition level directly
                return std::nullopt;
            case Type::Variation::DATA: {
                const auto *data_type = type.value()->as<DataType>();
                return std::make_tuple(                                //
                    data_type->data_node->file_hash,                   //
                    static_cast<int>(data_type->data_node->line - 1),  //
                    static_cast<int>(data_type->data_node->column - 1) //
                );
            }
            case Type::Variation::ENUM: {
                const auto *enum_type = type.value()->as<EnumType>();
                return std::make_tuple(                                //
                    enum_type->enum_node->file_hash,                   //
                    static_cast<int>(enum_type->enum_node->line - 1),  //
                    static_cast<int>(enum_type->enum_node->column - 1) //
                );
            }
            case Type::Variation::ERROR_SET: {
                const auto *error_type = type.value()->as<ErrorSetType>();
                if (error_type->error_node->file_hash.empty()) {
                    // This is an error defined inside a core module
                    return std::nullopt;
                }
                return std::make_tuple(                                  //
                    error_type->error_node->file_hash,                   //
                    static_cast<int>(error_type->error_node->line - 1),  //
                    static_cast<int>(error_type->error_node->column - 1) //
                );
            }
            case Type::Variation::VARIANT: {
                const auto *variant_type = type.value()->as<VariantType>();
                if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                    const VariantNode *const variant_node = std::get<VariantNode *const>(variant_type->var_or_list);
                    return std::make_tuple(                        //
                        variant_node->file_hash,                   //
                        static_cast<int>(variant_node->line - 1),  //
                        static_cast<int>(variant_node->column - 1) //
                    );
                }
            }
        }
    }

    // It's not a type, so it can only be a function otherwise
    {
        log_info("[DEFINITION] is function");
        // for (const auto &[fn, _] : Parser::parsed_functions) {
        //     if (!fn->is_extern) {
        //         return std::make_tuple(fn->file_hash, fn->line - 1, fn->column - 1);
        //     }
        // }
    }

    // It's not resolvable for now if it's not a function either
    return std::nullopt;
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
        // TODO: The parser crashes way too often on invalid input while typing, the change event stuff will only be re-activated when the
        // parser has been made much more robust
        return;

        // Parse the file and publish diagnostics
        // std::string file_content = extract_file_content_from_change(content);
        // LspServer::parse_program(file_path, file_content);
        // publish_diagnostics(file_uri);
    } else {
        log_info("Document changed");
    }
}

void LspServer::handle_document_save(const std::string &content) {
    std::string file_uri = extract_file_uri(content);
    std::string file_path = uri_to_file_path(file_uri);

    if (content.find(LspProtocol::FLINT_EXTENSION) != std::string::npos) {
        log_info("Flint document (.ft) saved");

        // Parse the file and publish diagnostics
        parse_program(file_path, std::nullopt);
        publish_diagnostics(file_uri);
    } else {
        log_info("Document saved");
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
        log_info("extract_position: NOPOS");
        return {-1, -1};
    }

    // Find the opening brace after "position":
    size_t brace_start = content.find("{", pos_start);
    if (brace_start == std::string::npos) {
        log_info("extract_position: NOOPEN");
        return {-1, -1};
    }

    // Extract character
    size_t char_pos = content.find("\"character\":", brace_start);
    if (char_pos == std::string::npos) {
        log_info("extract_position: NOCHAR");
        return {-1, -1};
    }
    char_pos += 12; // Skip "character":

    size_t first_end = content.find_first_of(",", brace_start);
    if (first_end == std::string::npos) {
        log_info("extract_position: NOCOMMA");
        return {-1, -1};
    }

    // Extract line
    size_t line_pos = content.find("\"line\":", brace_start);
    if (line_pos == std::string::npos) {
        log_info("extract_position: NOLINE");
        return {-1, -1};
    }
    line_pos += 7; // Skip "line":

    size_t second_end = content.find_first_of(",}", brace_start);
    if (second_end == std::string::npos) {
        log_info("extract_position: NOCLOSE");
        return {-1, -1};
    }

    try {
        if (char_pos < line_pos) {
            int line = std::stoi(content.substr(line_pos, second_end - line_pos));
            int character = std::stoi(content.substr(char_pos, first_end - char_pos));
            return {line, character};
        } else {
            int line = std::stoi(content.substr(line_pos, first_end - line_pos));
            int character = std::stoi(content.substr(char_pos, second_end - char_pos));
            return {line, character};
        }
    } catch (const std::exception &e) {
        log_info("extract_position: EXCEPTION(" + std::string(e.what()) + ")");
        return {-1, -1};
    }
}

static std::string url_decode(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = std::stoi(s.substr(i + 1, 2), nullptr, 16);
            out.push_back(static_cast<char>(hi));
            i += 2;
        } else if (s[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string LspServer::uri_to_file_path(const std::string &uri) {
    // Expect things like:
    //  file:///C:/path/to/file or file:///o%3A/Users/...
    std::string decoded = url_decode(uri);

    const std::string file_prefix1 = "file:///";
    const std::string file_prefix2 = "file://";

    if (decoded.rfind(file_prefix1, 0) == 0) {
#ifdef _WIN32
        // file:///C:/path -> C:/path
        std::string candidate = decoded.substr(file_prefix1.size());
        // Some clients use lowercase drive letters or other quirks; normalize
        if (candidate.size() >= 2 && candidate[1] == ':') {
            candidate[0] = static_cast<char>(std::toupper(candidate[0]));
        }
        // Convert forward slashes to backslashes for native windows paths if you prefer
        // std::replace(candidate.begin(), candidate.end(), '/', '\\');
        return candidate;
#else
        // Unix: keep leading slash
        return decoded.substr(file_prefix1.size() - 1); // keep leading '/'
#endif
    } else if (decoded.rfind(file_prefix2, 0) == 0) {
        return decoded.substr(file_prefix2.size());
    }
    // If the string is already a path-like thing, just return decoded
    return decoded;
}
