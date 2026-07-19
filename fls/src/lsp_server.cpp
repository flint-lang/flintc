#include "lsp_server.hpp"
#include "assert.hpp"
#include "completion_data.hpp"
#include "globals.hpp"
#include "lexer/lexer.hpp"
#include "lsp_protocol.hpp"
#include "parser/parser.hpp"
#include "parser/type/interface_type.hpp"
#include "profiler.hpp"

#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/callable_call_node_expression.hpp"
#include "parser/ast/expressions/data_access_node.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/function_reference_node.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/expressions/grouped_array_access_node.hpp"
#include "parser/ast/expressions/grouped_data_access_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/instance_call_node_expression.hpp"
#include "parser/ast/expressions/optional_chain_node.hpp"
#include "parser/ast/expressions/optional_unwrap_node.hpp"
#include "parser/ast/expressions/range_expression_node.hpp"
#include "parser/ast/expressions/string_interpolation_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/type_node.hpp"
#include "parser/ast/expressions/unary_op_expression.hpp"
#include "parser/ast/expressions/variant_extraction_node.hpp"
#include "parser/ast/expressions/variant_unwrap_node.hpp"
#include "parser/ast/statements/array_assignment_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/callable_call_node_statement.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/data_field_assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/enhanced_for_loop_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/group_assignment_node.hpp"
#include "parser/ast/statements/group_declaration_node.hpp"
#include "parser/ast/statements/grouped_array_assignment_node.hpp"
#include "parser/ast/statements/grouped_data_field_assignment_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/instance_call_node_statement.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/switch_statement.hpp"
#include "parser/ast/statements/throw_node.hpp"
#include "parser/ast/statements/unary_op_statement.hpp"
#include "parser/ast/statements/while_node.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/error_set_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/type.hpp"
#include "parser/type/variant_type.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <variant>

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
    bool parsed_successful = Parser::parse_all_open_data_modules(parse_parallel);
    if (!parsed_successful) {
        parser_cleanup();
        return std::nullopt;
    }
    parsed_successful = Parser::parse_all_open_objects(parse_parallel);
    if (!parsed_successful) {
        parser_cleanup();
        return std::nullopt;
    }
    parsed_successful = Parser::parse_all_open_functions(parse_parallel, Hash(std::filesystem::path(source_file_path)));
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
        send_hover_response(content);
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
             << "v" << VERSION << R"("
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

    if (definition.has_value() && !definition.value().target_hash.empty()) {
        const DefinitionResult &def = definition.value();
        const Hash &file_hash = def.target_hash;
        const std::string def_uri = "file://" + file_hash.path.string();

        if (def.source_start != -1 && def.source_end != -1) {
            // LocationLink with originSelectionRange tells VSCode exactly what to underline
            response << R"([{
    "originSelectionRange": {
      "start": {"line": )"
                     << position.first << R"(, "character": )" << def.source_start << R"(},
      "end": {"line": )"
                     << position.first << R"(, "character": )" << def.source_end << R"(}
    },
    "targetUri": ")" << def_uri
                     << R"(",
    "targetRange": {
      "start": {"line": )"
                     << def.target_line << R"(, "character": )" << def.target_col << R"(},
      "end": {"line": )"
                     << def.target_line << R"(, "character": )" << def.target_col << R"(}
    },
    "targetSelectionRange": {
      "start": {"line": )"
                     << def.target_line << R"(, "character": )" << def.target_col << R"(},
      "end": {"line": )"
                     << def.target_line << R"(, "character": )" << def.target_col << R"(}
    }
  }])";
        } else {
            // Fallback: plain Location (no source highlight)
            response << R"({
    "uri": ")" << def_uri
                     << R"(",
    "range": {
      "start": {"line": )"
                     << def.target_line << R"(, "character": )" << def.target_col << R"(},
      "end": {"line": )"
                     << def.target_line << R"(, "character": )" << def.target_col << R"(}
    }
  })";
        }
    } else {
        response << "null";
    }

    response << "\n}";
    log_info("DEFINITION_RESPONSE_BEGIN" + response.str() + " |DEFINTION_RESPONSE_END");
    send_lsp_response(response.str());
}

std::optional<LspServer::DefinitionResult> LspServer::find_definition_at_position( //
    const std::string &file_path,                                                  //
    int line,                                                                      //
    int character                                                                  //
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

    std::filesystem::path source_file_path(file_path);
    log_info("[DEFINITION] file_name = " + source_file_path.filename().string());
    const Parser *parser = Parser::get_instance_from_hash(Hash(source_file_path)).value();
    const Namespace *file_namespace = parser->file_node_ptr->file_namespace.get();
    const auto &lines = parser->get_source_code_lines();

    // Unified AST node lookup (add 1 to convert from VSCode 0-indexed to parser 1-indexed lines)
    const unsigned int uline = static_cast<unsigned int>(line + 1);
    const unsigned int ucol = (line >= 0 && static_cast<size_t>(line) < lines.size()) ? vscode_char_to_column(lines[line].second, character)
                                                                                      : static_cast<unsigned int>(character);
    log_info("[DEFINITION] find_node_at(" + std::to_string(uline) + ", " + std::to_string(ucol) + ")");
    auto node = find_node_at(file_namespace, uline, ucol);
    if (!node.has_value()) {
        log_info("[DEFINITION] find_node_at returned nullopt");
        return std::nullopt;
    }

    // Look up the token at this position for the source highlight range
    DefinitionResult result;
    int source_start = -1;
    int source_end = -1;
    const auto &tok = get_token_at_pos(file_namespace, uline, ucol);
    if (tok.has_value()) {
        int start_col = static_cast<int>(tok.value().column);
        int end_col = start_col + static_cast<int>(tok.value().lexme.size());
        // TOK_STR_VALUE lexeme excludes the surrounding quotes; add them back
        if (tok.value().token == TOK_STR_VALUE) {
            end_col += 2;
        }
        source_start = static_cast<int>(column_to_vscode_char(lines[line].second, start_col));
        source_end = static_cast<int>(column_to_vscode_char(lines[line].second, end_col));
    }

    auto &pos = node.value();
    if (const auto *var = std::get_if<LocalVariable>(&pos)) {
        log_info("[DEFINITION] found variable: " + var->name);
        return DefinitionResult{
            var->file_hash,
            static_cast<int>(var->line - 1),
            static_cast<int>(var->column - 1),
            source_start,
            source_end,
        };
    } else if (const auto *imported_file = std::get_if<ImportedFile>(&pos)) {
        return DefinitionResult{
            imported_file->file_hash,
            0,
            0,
            source_start,
            source_end,
        };
    } else if (const auto *type = std::get_if<std::shared_ptr<Type>>(&pos)) {
        log_info("[DEFINITION] found Type: " + (*type)->to_string());
        auto loc = get_type_definition_location(*type);
        if (loc.has_value()) {
            return DefinitionResult{
                std::get<0>(loc.value()),
                std::get<1>(loc.value()),
                std::get<2>(loc.value()),
                source_start,
                source_end,
            };
        }
        return std::nullopt;
    } else if (const auto **fn = std::get_if<const FunctionNode *>(&pos)) {
        log_info("[DEFINITION] found function: " + (*fn)->name);
        return DefinitionResult{
            (*fn)->file_hash,
            static_cast<int>((*fn)->line - 1),
            static_cast<int>((*fn)->column - 1),
            source_start,
            source_end,
        };
    }
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

void LspServer::send_hover_response(const std::string &content) {
    const std::string file_uri = extract_file_uri(content);
    const std::string file_path = uri_to_file_path(file_uri);
    const auto position = extract_position(content);
    const std::string request_id = extract_request_id(content);

    const auto send_null = [&]() {
        std::stringstream response;
        response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id
                 << R"(,
  "result": null
})";
        send_lsp_response(response.str());
    };

    if (position.first == -1 || position.second == -1 || file_path.empty()) {
        send_null();
        return;
    }

    log_info("Hover request for file: " + file_path + " at line " + std::to_string(position.first) + ", char " +
        std::to_string(position.second));

    std::optional<FileNode *> file = parse_program(file_path, std::nullopt);
    if (!file.has_value()) {
        send_null();
        return;
    }

    const int line = position.first;
    const int character = position.second;

    // Parse the source
    std::filesystem::path source_file_path(file_path);
    auto parser_opt = Parser::get_instance_from_hash(Hash(source_file_path));
    if (!parser_opt.has_value()) {
        send_null();
        return;
    }
    const Parser *parser = parser_opt.value();
    const Namespace *file_namespace = parser->file_node_ptr->file_namespace.get();
    const auto &lines = parser->get_source_code_lines();
    if (line < 0 || static_cast<size_t>(line) >= lines.size()) {
        send_null();
        return;
    }

    // Unified AST node lookup (add 1 to convert from VSCode 0-indexed to parser 1-indexed lines)
    const unsigned int uline = static_cast<unsigned int>(line + 1);
    const unsigned int ucol = (line >= 0 && static_cast<size_t>(line) < lines.size()) ? vscode_char_to_column(lines[line].second, character)
                                                                                      : static_cast<unsigned int>(character);
    log_info("[HOVER] find_node_at(" + std::to_string(uline) + ", " + std::to_string(ucol) + ")");
    auto hover_node = find_node_at(file_namespace, uline, ucol);
    if (!hover_node.has_value()) {
        log_info("[HOVER] find_node_at returned nullopt");
        send_null();
        return;
    }

    std::string hover_value;
    auto &pos = hover_node.value();
    if (const auto *var = std::get_if<LocalVariable>(&pos)) {
        log_info("[HOVER] found variable: " + var->name);
        if (var->type) {
            std::stringstream ss;
            ss << "**variable** **`" << var->name << "`**\n" << build_type_hover_info(var->type);
            hover_value = ss.str();
        }
    } else if (const auto *imported_file = std::get_if<ImportedFile>(&pos)) {
        log_info("[HOVER] found file import: " + imported_file->file_hash.to_string());
    } else if (const auto *type = std::get_if<std::shared_ptr<Type>>(&pos)) {
        log_info("[HOVER] found Type: " + (*type)->to_string());
        hover_value = build_type_hover_info(*type);
    } else if (const auto *fn = std::get_if<const FunctionNode *>(&pos)) {
        log_info("[HOVER] found function: " + (*fn)->name);
        hover_value = build_function_hover_info(*fn);
    } else {
        log_info("[HOVER] unknown node type in variant");
    }

    // Build and send response
    std::stringstream response;
    response << R"({
  "jsonrpc": "2.0",
  "id": )" << request_id;

    if (!hover_value.empty()) {
        log_info("[HOVER] sending hover_value: " + hover_value);
        response << R"(,
  "result": {
    "contents": {
      "kind": "markdown",
      "value": ")"
                 << json_escape(hover_value) << R"("
    }
  }
})";
    } else {
        log_info("[HOVER] hover_value empty, sending null");
        response << R"(,
  "result": null
})";
    }

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

std::optional<TokenContext> LspServer::get_token_at_pos(const Namespace *ns, unsigned int line, unsigned int col) {
    for (const auto &tok : ns->file_node->tokens) {
        if (tok.line != line) {
            continue;
        }
        if (col < tok.column) {
            continue;
        }
        if (tok.token == TOK_TYPE && col < tok.column + tok.type->to_string().size()) {
            return tok;
        }
        if (col < tok.column + tok.lexme.size()) {
            return tok;
        }
    }
    return std::nullopt;
}

/// @brief Recursively finds what's at (line, col) in an expression tree — a variable, a type, or a function.
std::optional<LspServer::PositionInfo> LspServer::find_node_in_expr( //
    const ExpressionNode *expr,                                      //
    const Scope *scope,                                              //
    unsigned int line,                                               //
    unsigned int col                                                 //
) {
    if (!expr->contains_pos(line, col)) {
        return std::nullopt;
    }
    LspServer::log_info("[TRAVERSAL] expr_variation=" + std::to_string(static_cast<int>(expr->get_variation())));
    switch (expr->get_variation()) {
        case ExpressionNode::Variation::ARRAY_ACCESS: {
            const auto *node = expr->as<ArrayAccessNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            for (const auto &indexing_expr : node->indexing_expressions) {
                if (indexing_expr->contains_pos(line, col)) {
                    return find_node_in_expr(indexing_expr.get(), scope, line, col);
                }
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::ARRAY_INITIALIZER: {
            const auto *node = expr->as<ArrayInitializerNode>();
            if (node->initializer_value->contains_pos(line, col)) {
                return find_node_in_expr(node->initializer_value.get(), scope, line, col);
            }
            for (const auto &length_expr : node->length_expressions) {
                if (length_expr->contains_pos(line, col)) {
                    return find_node_in_expr(length_expr.get(), scope, line, col);
                }
            }
            return node->element_type;
        }
        case ExpressionNode::Variation::BINARY_OP: {
            const auto *node = expr->as<BinaryOpNode>();
            if (!node->is_shorthand && node->left->contains_pos(line, col)) {
                return find_node_in_expr(node->left.get(), scope, line, col);
            }
            if (node->right->contains_pos(line, col)) {
                return find_node_in_expr(node->right.get(), scope, line, col);
            }
            // When hovering over the operator
            return std::nullopt;
        }
        case ExpressionNode::Variation::CALL: {
            const auto *node = expr->as<CallNodeExpression>();
            for (const auto &[arg, is_reference] : node->arguments) {
                if (arg->contains_pos(line, col)) {
                    return find_node_in_expr(arg.get(), scope, line, col);
                }
            }
            if (col >= node->column && col <= node->column + node->function->name.length()) {
                return node->function;
            }
            // When hovering over a comma, or the left and right parens for example
            return std::nullopt;
        }
        case ExpressionNode::Variation::CALLABLE_CALL: {
            const auto *node = expr->as<CallableCallNodeExpression>();
            if (scope->variables.find(node->callable_variable) == scope->variables.end()) {
                return std::nullopt;
            }
            const auto &var = scope->variables.at(node->callable_variable);
            return LocalVariable{node->callable_variable, var.type, var.file_hash, var.line, var.column};
        }
        case ExpressionNode::Variation::DATA_ACCESS: {
            const auto *node = expr->as<DataAccessNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            if (col > node->column + node->base_expr->length) {
                // TODO: Implement hover info for hovering over a data field
                return std::nullopt;
            }
            // It's hovering over the dot
            return std::nullopt;
        }
        case ExpressionNode::Variation::DEFAULT: {
            const auto *node = expr->as<DefaultNode>();
            return node->type;
        }
        case ExpressionNode::Variation::GROUP_EXPRESSION: {
            const auto *node = expr->as<GroupExpressionNode>();
            for (const auto &g_expr : node->expressions) {
                if (g_expr->contains_pos(line, col)) {
                    return find_node_in_expr(g_expr.get(), scope, line, col);
                }
            }
            // Hovering over the comma or parenthesis
            return std::nullopt;
        }
        case ExpressionNode::Variation::GROUPED_ARRAY_ACCESS: {
            const auto *node = expr->as<GroupedArrayAccessNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            for (const auto &idx_expr : node->indexing_expressions) {
                if (idx_expr->contains_pos(line, col)) {
                    return find_node_in_expr(idx_expr.get(), scope, line, col);
                }
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
            const auto *node = expr->as<GroupedDataAccessNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            unsigned int start = node->column + node->base_expr->length + 1;
            for (const auto &field : node->field_names) {
                if (col > start && col < start + field.size()) {
                    // TODO: Implement hover info for hovering over a data field
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::FUNCTION_REFERENCE: {
            const auto *node = expr->as<FunctionReferenceNode>();
            return node->referenced_function;
        }
        case ExpressionNode::Variation::INITIALIZER: {
            const auto *node = expr->as<InitializerNode>();
            for (const auto &arg : node->args) {
                if (arg->contains_pos(line, col)) {
                    return find_node_in_expr(arg.get(), scope, line, col);
                }
            }
            return node->type;
        }
        case ExpressionNode::Variation::INLINE_ARRAY_INITIALIZER: {
            const auto *node = expr->as<InlineArrayInitializerNode>();
            for (const auto &length_expr : node->length_expressions) {
                if (length_expr->contains_pos(line, col)) {
                    return find_node_in_expr(length_expr.get(), scope, line, col);
                }
            }
            for (const auto &init_expr : node->initializer_values) {
                if (init_expr->contains_pos(line, col)) {
                    return find_node_in_expr(init_expr.get(), scope, line, col);
                }
            }
            return node->element_type;
        }
        case ExpressionNode::Variation::INSTANCE_CALL: {
            const auto *node = expr->as<InstanceCallNodeExpression>();
            if (node->instance_variable->contains_pos(line, col)) {
                return find_node_in_expr(node->instance_variable.get(), scope, line, col);
            }
            for (const auto &[arg, is_reference] : node->arguments) {
                if (arg->contains_pos(line, col)) {
                    return find_node_in_expr(arg.get(), scope, line, col);
                }
            }
            const unsigned int call_start = node->column + node->instance_variable->length;
            const unsigned int fn_name_len = node->function->name.size();
            const unsigned int base_type_len = node->instance_variable->type->to_string().size();
            const unsigned int call_end = call_start + fn_name_len - base_type_len - 1;
            if (col >= call_start && col < call_end) {
                return node->function;
            }
            // Hovering over the dot
            return std::nullopt;
        }
        case ExpressionNode::Variation::LITERAL: {
            const auto *node = expr->as<LiteralNode>();
            return node->type;
        }
        case ExpressionNode::Variation::OPTIONAL_CHAIN: {
            const auto *node = expr->as<OptionalChainNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            if (std::holds_alternative<ChainFieldAccess>(node->operation)) {
                [[maybe_unused]] const auto &field_access = std::get<ChainFieldAccess>(node->operation);
                // TODO: Implement hover info for hovering over a data field
                return std::nullopt;
            } else {
                const auto &array_access = std::get<ChainArrayAccess>(node->operation);
                for (const auto &idx_expr : array_access.indexing_expressions) {
                    if (idx_expr->contains_pos(line, col)) {
                        return find_node_in_expr(idx_expr.get(), scope, line, col);
                    }
                }
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::OPTIONAL_UNWRAP: {
            const auto *node = expr->as<OptionalUnwrapNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::RANGE_EXPRESSION: {
            const auto *node = expr->as<RangeExpressionNode>();
            if (node->lower_bound->contains_pos(line, col)) {
                return find_node_in_expr(node->lower_bound.get(), scope, line, col);
            }
            if (node->upper_bound->contains_pos(line, col)) {
                return find_node_in_expr(node->upper_bound.get(), scope, line, col);
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::STRING_INTERPOLATION: {
            const auto *node = expr->as<StringInterpolationNode>();
            for (const auto &elem : node->string_content) {
                if (!std::holds_alternative<std::unique_ptr<ExpressionNode>>(elem)) {
                    continue;
                }
                const auto &elem_expr = std::get<std::unique_ptr<ExpressionNode>>(elem);
                LspServer::log_info("[TRAVERSE]   String Interpolation contains Expr, line= " + std::to_string(elem_expr->line) +
                    ", col=" + std::to_string(elem_expr->column) + ", length=" + std::to_string(elem_expr->length) +
                    ", end_line=" + std::to_string(elem_expr->end_line));
                const bool contains = elem_expr->contains_pos(line, col);
                LspServer::log_info("[TRAVERSE]    Contains=" + std::to_string(contains) + ", line=" + std::to_string(line) +
                    ", col=" + std::to_string(col));
                if (contains) {
                    return find_node_in_expr(elem_expr.get(), scope, line, col);
                }
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::SWITCH_EXPRESSION: {
            const auto *node = expr->as<SwitchExpression>();
            if (node->switcher->contains_pos(line, col)) {
                return find_node_in_expr(node->switcher.get(), scope, line, col);
            }
            for (const auto &branch : node->branches) {
                for (const auto &match : branch.matches) {
                    if (match->contains_pos(line, col)) {
                        return find_node_in_expr(match.get(), scope, line, col);
                    }
                }
                if (branch.expr->contains_pos(line, col)) {
                    return find_node_in_expr(branch.expr.get(), scope, line, col);
                }
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::SWITCH_MATCH: {
            [[maybe_unused]] const auto *node = expr->as<SwitchMatchNode>();
            // TODO: What to do here?
            return std::nullopt;
        }
        case ExpressionNode::Variation::TYPE_CAST: {
            const auto *node = expr->as<TypeCastNode>();
            LspServer::log_info("[TRAVERSAL]    TypeCast to " + node->type->to_string());
            if (node->expr->contains_pos(line, col)) {
                return find_node_in_expr(node->expr.get(), scope, line, col);
            }
            return node->type;
        }
        case ExpressionNode::Variation::TYPE: {
            const auto *node = expr->as<TypeNode>();
            return node->type;
        }
        case ExpressionNode::Variation::UNARY_OP: {
            const auto *node = expr->as<UnaryOpExpression>();
            if (node->operand->contains_pos(line, col)) {
                return find_node_in_expr(node->operand.get(), scope, line, col);
            }
            return std::nullopt;
        }
        case ExpressionNode::Variation::VARIABLE: {
            const auto *node = expr->as<VariableNode>();
            LspServer::log_info("[TRAVERSAL]     Variable: " + node->name);
            if (scope->variables.find(node->name) == scope->variables.end()) {
                return std::nullopt;
            }
            const auto &var = scope->variables.at(node->name);
            return LocalVariable{node->name, var.type, var.file_hash, var.line, var.column};
        }
        case ExpressionNode::Variation::VARIANT_EXTRACTION: {
            const auto *node = expr->as<VariantExtractionNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            return node->extracted_type;
        }
        case ExpressionNode::Variation::VARIANT_UNWRAP: {
            const auto *node = expr->as<VariantUnwrapNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            return node->type;
        }
    }
    UNREACHABLE();
}

std::optional<LspServer::PositionInfo> LspServer::find_node_in_stmt( //
    const Namespace *ns,                                             //
    const StatementNode *stmt,                                       //
    const Scope *scope,                                              //
    const unsigned int line,                                         //
    const unsigned int col                                           //
) {
    switch (stmt->get_variation()) {
        case StatementNode::Variation::ARRAY_ASSIGNMENT: {
            const auto *node = stmt->as<ArrayAssignmentNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            for (const auto &index : node->indexing_expressions) {
                if (index->contains_pos(line, col)) {
                    return find_node_in_expr(index.get(), scope, line, col);
                }
            }
            if (node->expression->contains_pos(line, col)) {
                return find_node_in_expr(node->expression.get(), scope, line, col);
            }
            return std::nullopt;
        }
        case StatementNode::Variation::ASSIGNMENT: {
            const auto *node = stmt->as<AssignmentNode>();
            if (node->expression->contains_pos(line, col)) {
                // The hover info is in the expression part
                return find_node_in_expr(node->expression.get(), scope, line, col);
            }
            // The hover info is outside the expression part, so it could be the variable name being highlighted. For this we use the
            // "get_token_at" helper to get the exact token at the given hover position. If the token is an identifier and it's the same
            // as the assigned variable, then the hover is over the variable, otherwise we return nullopt.
            const auto &tok = get_token_at_pos(ns, line, col);
            if (!tok.has_value()) {
                return std::nullopt;
            }
            if (tok.value().token != TOK_IDENTIFIER) {
                return std::nullopt;
            }
            if (tok.value().lexme != node->name) {
                return std::nullopt;
            }
            return node->type;
        }
        case StatementNode::Variation::BREAK: {
            return std::nullopt;
        }
        case StatementNode::Variation::CALL: {
            const auto *node = stmt->as<CallNodeStatement>();
            for (const auto &[arg_expr, _] : node->arguments) {
                if (arg_expr->contains_pos(line, col)) {
                    return find_node_in_expr(arg_expr.get(), scope, line, col);
                }
            }
            if (col >= node->column && col < node->column + node->function->name.size()) {
                return node->function;
            }
            return std::nullopt;
        }
        case StatementNode::Variation::CALLABLE_CALL: {
            const auto *node = stmt->as<CallableCallNodeStatement>();
            for (const auto &[arg_expr, _] : node->arguments) {
                const auto result = find_node_in_expr(arg_expr.get(), scope, line, col);
                if (result.has_value()) {
                    return result;
                }
            }
            return scope->variables.at(node->callable_variable).type;
        }
        case StatementNode::Variation::CATCH: {
            const auto &node = stmt->as<CatchNode>();
            // Check if we hover over the catch variable
            if (node->var_name.has_value()) {
                const size_t catch_name_start = node->column + 6;
                const size_t catch_name_end = catch_name_start + node->var_name.value().size();
                if (line == node->line && col >= catch_name_start && col < catch_name_end) {
                    const auto &var = node->scope->variables.at(node->var_name.value());
                    return var.type;
                }
            }
            return find_node_in_scope(ns, node->scope.get(), line, col);
        }
        case StatementNode::Variation::CONTINUE: {
            return std::nullopt;
        }
        case StatementNode::Variation::DATA_FIELD_ASSIGNMENT: {
            const auto *node = stmt->as<DataFieldAssignmentNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            if (node->expression->contains_pos(line, col)) {
                return find_node_in_expr(node->expression.get(), scope, line, col);
            }
            break;
        }
        case StatementNode::Variation::DECLARATION: {
            const auto *node = stmt->as<DeclarationNode>();
            if (node->initializer.has_value() && node->initializer.value()->contains_pos(line, col)) {
                // The hover info is in the initializer expression part
                return find_node_in_expr(node->initializer.value().get(), scope, line, col);
            }
            // The hover info is outside the initializer part. If the declaration is inferred then it's only able to be the variable
            // name. If it is not inferred, then we could hover over the type. We return the type of the declaration either way.
            const auto &tok = get_token_at_pos(ns, line, col);
            if (!tok.has_value()) {
                return std::nullopt;
            }
            if (tok.value().token == TOK_IDENTIFIER && tok.value().lexme == node->name) {
                return node->type;
            }
            if (tok.value().token == TOK_TYPE && tok.value().type == node->type) {
                return node->type;
            }
            return std::nullopt;
        }
        case StatementNode::Variation::DO_WHILE: {
            const auto *node = stmt->as<DoWhileNode>();
            if (node->condition->contains_pos(line, col)) {
                return find_node_in_expr(node->condition.get(), scope, line, col);
            }
            return find_node_in_scope(ns, node->scope.get(), line, col);
        }
        case StatementNode::Variation::ENHANCED_FOR_LOOP: {
            const auto *node = stmt->as<EnhForLoopNode>();
            if (node->iterable->contains_pos(line, col)) {
                return find_node_in_expr(node->iterable.get(), scope, line, col);
            }
            {
                // TODO: Somehow check if we are hovering over the identifiers of the enh for loop
            }
            return find_node_in_scope(ns, node->body.get(), line, col);
        }
        case StatementNode::Variation::FOR_LOOP: {
            const auto *node = stmt->as<ForLoopNode>();
            if (node->condition->contains_pos(line, col)) {
                return find_node_in_expr(node->condition.get(), scope, line, col);
            }
            if (node->looparound->contains_pos(line, col)) {
                return find_node_in_stmt(ns, node->looparound.get(), scope, line, col);
            }
            const auto &pos = find_node_in_scope(ns, node->definition_scope.get(), line, col);
            if (pos.has_value()) {
                return pos;
            }
            return find_node_in_scope(ns, node->body.get(), line, col);
        }
        case StatementNode::Variation::GROUP_ASSIGNMENT: {
            const auto *node = stmt->as<GroupAssignmentNode>();
            if (node->expression->contains_pos(line, col)) {
                return find_node_in_expr(node->expression.get(), scope, line, col);
            }
            {
                // TODO: Somehow check if we are hovering over the variables of the group assignment
            }
            return std::nullopt;
        }
        case StatementNode::Variation::GROUP_DECLARATION: {
            const auto *node = stmt->as<GroupDeclarationNode>();
            if (node->initializer->contains_pos(line, col)) {
                return find_node_in_expr(node->initializer.get(), scope, line, col);
            }
            {
                // TODO: Somehow check if we are hovering over the variables of the group declaration
            }
            return std::nullopt;
        }
        case StatementNode::Variation::GROUPED_ARRAY_ASSIGNMENT: {
            const auto *node = stmt->as<GroupedArrayAssignmentNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            if (node->expression->contains_pos(line, col)) {
                return find_node_in_expr(node->expression.get(), scope, line, col);
            }
            for (const auto &index : node->indexing_expressions) {
                if (index->contains_pos(line, col)) {
                    return find_node_in_expr(index.get(), scope, line, col);
                }
            }
            return std::nullopt;
        }
        case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT: {
            const auto *node = stmt->as<GroupedDataFieldAssignmentNode>();
            if (node->base_expr->contains_pos(line, col)) {
                return find_node_in_expr(node->base_expr.get(), scope, line, col);
            }
            if (node->expression->contains_pos(line, col)) {
                return find_node_in_expr(node->expression.get(), scope, line, col);
            }
            {
                // TODO: Somehow highlight the fields (field types) if we hover over them
            }
            return std::nullopt;
        }
        case StatementNode::Variation::IF: {
            const auto *node = stmt->as<IfNode>();
            if (node->condition->contains_pos(line, col)) {
                return find_node_in_expr(node->condition.get(), scope, line, col);
            }
            const auto &then_node = find_node_in_scope(ns, node->then_scope.get(), line, col);
            if (then_node.has_value()) {
                return then_node;
            }
            if (!node->else_scope.has_value()) {
                return std::nullopt;
            }
            if (std::holds_alternative<std::unique_ptr<IfNode>>(node->else_scope.value())) {
                const auto &else_node = std::get<std::unique_ptr<IfNode>>(node->else_scope.value());
                return find_node_in_stmt(ns, else_node.get(), scope, line, col);
            } else {
                const auto &else_scope = std::get<std::shared_ptr<Scope>>(node->else_scope.value());
                return find_node_in_scope(ns, else_scope.get(), line, col);
            }
            UNREACHABLE();
        }
        case StatementNode::Variation::INSTANCE_CALL: {
            const auto *node = stmt->as<InstanceCallNodeStatement>();
            if (node->instance_variable->contains_pos(line, col)) {
                return find_node_in_expr(node->instance_variable.get(), scope, line, col);
            }
            for (const auto &[arg_expr, is_ref] : node->arguments) {
                if (arg_expr->contains_pos(line, col)) {
                    return find_node_in_expr(arg_expr.get(), scope, line, col);
                }
            }
            return std::nullopt;
        }
        case StatementNode::Variation::RETURN: {
            const auto *node = stmt->as<ReturnNode>();
            if (node->return_value.has_value() && node->return_value.value()->contains_pos(line, col)) {
                return find_node_in_expr(node->return_value.value().get(), scope, line, col);
            }
            return std::nullopt;
        }
        case StatementNode::Variation::SWITCH: {
            const auto *node = stmt->as<SwitchStatement>();
            if (node->switcher->contains_pos(line, col)) {
                return find_node_in_expr(node->switcher.get(), scope, line, col);
            }
            for (const auto &branch : node->branches) {
                for (const auto &match : branch.matches) {
                    if (match->contains_pos(line, col)) {
                        return find_node_in_expr(match.get(), scope, line, col);
                    }
                }
                const auto &branch_value = find_node_in_scope(ns, branch.body.get(), line, col);
                if (branch_value.has_value()) {
                    return branch_value;
                }
            }
            return std::nullopt;
        }
        case StatementNode::Variation::THROW: {
            const auto *node = stmt->as<ThrowNode>();
            if (node->throw_value->contains_pos(line, col)) {
                return find_node_in_expr(node->throw_value.get(), scope, line, col);
            }
            return std::nullopt;
        }
        case StatementNode::Variation::UNARY_OP: {
            const auto *node = stmt->as<UnaryOpStatement>();
            if (node->operand->contains_pos(line, col)) {
                return find_node_in_expr(node->operand.get(), scope, line, col);
            }
            return std::nullopt;
        }
        case StatementNode::Variation::WHILE: {
            const auto *node = stmt->as<WhileNode>();
            if (node->condition->contains_pos(line, col)) {
                return find_node_in_expr(node->condition.get(), scope, line, col);
            }
            return find_node_in_scope(ns, node->scope.get(), line, col);
        }
    }
    UNREACHABLE();
}

/// @brief Finds what's at (line, col) within a scope tree.
std::optional<LspServer::PositionInfo> LspServer::find_node_in_scope( //
    const Namespace *ns,                                              //
    const Scope *scope,                                               //
    const unsigned int line,                                          //
    const unsigned int col                                            //
) {
    for (const auto &stmt : scope->body) {
        const bool contains_pos = stmt->contains_pos(line, col);
        LspServer::log_info("[TRAVERSE]   stmt var=" + std::to_string(static_cast<int>(stmt->get_variation())) +
            " line=" + std::to_string(stmt->line) + " end_line=" + std::to_string(stmt->end_line) + " col=" + std::to_string(stmt->column) +
            " len=" + std::to_string(stmt->length) + " contains_line=" + (contains_pos ? "yes" : "no"));
        if (!contains_pos) {
            continue;
        }
        const auto &pos = find_node_in_stmt(ns, stmt.get(), scope, line, col);
        if (pos.has_value()) {
            return pos;
        }
    }
    return std::nullopt;
}

std::optional<LspServer::PositionInfo> LspServer::find_node_in_def( //
    const Namespace *ns,                                            //
    const DefinitionNode *def,                                      //
    const unsigned int line,                                        //
    const unsigned int col                                          //
) {
    switch (def->get_variation()) {
        case DefinitionNode::Variation::DATA: {
            const auto &node = def->as<DataNode>();
            return ns->get_type_from_str(node->name).value();
        }
        case DefinitionNode::Variation::OBJECT: {
            const auto &node = def->as<ObjectNode>();
            for (const auto &fn : node->functions) {
                const auto &pos = find_node_in_def(ns, fn, line, col);
                if (pos.has_value()) {
                    return pos;
                }
            }
            break;
        }
        case DefinitionNode::Variation::ENUM: {
            const auto &node = def->as<EnumNode>();
            return ns->get_type_from_str(node->name).value();
        }
        case DefinitionNode::Variation::ERROR: {
            const auto &node = def->as<ErrorNode>();
            return ns->get_type_from_str(node->name).value();
        }
        case DefinitionNode::Variation::FUNC: {
            const auto &node = def->as<FuncNode>();
            for (const auto &fn : node->functions) {
                const auto &pos = find_node_in_def(ns, fn, line, col);
                if (pos.has_value()) {
                    return pos;
                }
            }
            break;
        }
        case DefinitionNode::Variation::FUNCTION: {
            const auto *node = def->as<FunctionNode>();
            LspServer::log_info(
                std::string("[TRAVERSE]   -> recursing into fn body, has_scope=") + (node->scope.has_value() ? "yes" : "no"));
            const bool on_signature = line == def->line && col >= def->column && col < def->column + def->length;
            if (on_signature) {
                return node;
            }
            if (node->scope.has_value()) {
                return find_node_in_scope(ns, node->scope.value().get(), line, col);
            }
            break;
        }
        case DefinitionNode::Variation::INTERFACE: {
            const auto &node = def->as<InterfaceNode>();
            for (const auto &fn : node->functions) {
                const auto &pos = find_node_in_def(ns, fn, line, col);
                if (pos.has_value()) {
                    return pos;
                }
            }
            break;
        }
        case DefinitionNode::Variation::IMPORT: {
            const auto *node = def->as<ImportNode>();
            const auto &tok = get_token_at_pos(ns, line, col);
            if (tok.has_value() && tok.value().token == TOK_STR_VALUE) {
                return ImportedFile{.file_hash = std::get<Hash>(node->path)};
            }
            break;
        }
        case DefinitionNode::Variation::TEST: {
            const auto *node = def->as<TestNode>();
            LspServer::log_info(std::string("[TRAVERSE]   -> recursing into test body"));
            return find_node_in_scope(ns, node->scope.get(), line, col);
        }
        case DefinitionNode::Variation::VARIANT: {
            const auto &node = def->as<VariantNode>();
            return ns->get_type_from_str(node->name).value();
        }
    }
    return std::nullopt;
}

std::optional<LspServer::PositionInfo> LspServer::find_node_at(const Namespace *ns, unsigned int line, unsigned int col) {
    LspServer::log_info("[TRAVERSE] find_node_at(" + std::to_string(line) + ", " + std::to_string(col) + "), " +
        std::to_string(ns->public_symbols.definitions.size()) + " definitions");

    for (const auto &def : ns->public_symbols.definitions) {
        const bool contains_pos = def->contains_pos(line, col);
        LspServer::log_info("[TRAVERSE]   def var=" + std::to_string(static_cast<int>(def->get_variation())) + " name='" +
            (def->get_variation() == DefinitionNode::Variation::FUNCTION ? def->as<FunctionNode>()->name : "?") +
            "' line=" + std::to_string(def->line) + " end_line=" + std::to_string(def->end_line) + " col=" + std::to_string(def->column) +
            " len=" + std::to_string(def->length) + " contains_pos=" + (contains_pos ? "yes" : "no"));
        if (!contains_pos) {
            continue;
        }
        const auto &pos = find_node_in_def(ns, def.get(), line, col);
        if (pos.has_value()) {
            return pos;
        }
    }
    for (const auto &import : ns->public_symbols.imports) {
        const bool contains_pos = import->contains_pos(line, col);
        LspServer::log_info("[TRAVERSE]   def var=" + std::to_string(static_cast<int>(import->get_variation())) + " name='" +
            (import->get_variation() == DefinitionNode::Variation::FUNCTION ? import->as<FunctionNode>()->name : "?") + "' line=" +
            std::to_string(import->line) + " end_line=" + std::to_string(import->end_line) + " col=" + std::to_string(import->column) +
            " len=" + std::to_string(import->length) + " contains_pos=" + (contains_pos ? "yes" : "no"));
        if (!contains_pos) {
            continue;
        }
        const auto &pos = find_node_in_def(ns, import.get(), line, col);
        if (pos.has_value()) {
            return pos;
        }
    }

    // If nothing matched, checked if the cursor points at a type token and just display the type it points to
    const auto &tok = get_token_at_pos(ns, line, col);
    if (tok.has_value() && tok.value().token == TOK_TYPE) {
        return tok.value().type;
    }
    return std::nullopt;
}

unsigned int LspServer::vscode_char_to_column(std::string_view line, int character) {
    unsigned int col = 0;
    for (int i = 0; i < character && i < static_cast<int>(line.size()); i++) {
        if (line[i] == '\t')
            col += Lexer::TAB_SIZE - (col % Lexer::TAB_SIZE);
        else
            col++;
    }
    return col + 1;
}

unsigned int LspServer::column_to_vscode_char(std::string_view line, unsigned int col) {
    unsigned int visual_col = 0;
    for (int i = 0; i < static_cast<int>(line.size()); i++) {
        if (visual_col + 1 == col) {
            return i;
        }
        if (line[i] == '\t')
            visual_col += Lexer::TAB_SIZE - (visual_col % Lexer::TAB_SIZE);
        else
            visual_col++;
    }
    return line.size();
}

std::optional<std::tuple<Hash, int, int>> LspServer::get_type_definition_location(const std::shared_ptr<Type> &type) {
    switch (type->get_variation()) {
        case Type::Variation::DATA: {
            const auto *node = type->as<DataType>()->data_node;
            return std::make_tuple(node->file_hash, static_cast<int>(node->line - 1), static_cast<int>(node->column - 1));
        }
        case Type::Variation::ENUM: {
            const auto *node = type->as<EnumType>()->enum_node;
            return std::make_tuple(node->file_hash, static_cast<int>(node->line - 1), static_cast<int>(node->column - 1));
        }
        case Type::Variation::ERROR_SET: {
            const auto *node = type->as<ErrorSetType>()->error_node;
            if (node->file_hash.empty())
                return std::nullopt;
            return std::make_tuple(node->file_hash, static_cast<int>(node->line - 1), static_cast<int>(node->column - 1));
        }
        case Type::Variation::VARIANT: {
            const auto *variant_type = type->as<VariantType>();
            if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                const auto *node = std::get<VariantNode *const>(variant_type->var_or_list);
                return std::make_tuple(node->file_hash, static_cast<int>(node->line - 1), static_cast<int>(node->column - 1));
            }
            return std::nullopt;
        }
        default:
            LspServer::log_info("TODO: Definition");
            return std::nullopt;
    }
}

std::string LspServer::build_type_hover_info(const std::shared_ptr<Type> &type) {
    std::stringstream ss;
    switch (type->get_variation()) {
        case Type::Variation::ALIAS: {
            const auto *t = type->as<AliasType>();
            return build_type_hover_info(t->type);
        }
        case Type::Variation::ARRAY:
            ss << "**array**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::DATA: {
            const auto *data_type = type->as<DataType>();
            const auto *node = data_type->data_node;
            ss << "**data** **`" << node->name << "`**\n```\n";
            for (const auto &field : node->fields) {
                ss << field.name << ": " << field.type->to_string() << "\n";
            }
            ss << "```\n";
            ss << "Defined at `" << node->file_hash.path.filename().string() << ":" << node->line << ":" << node->column << "`";
            break;
        }
        case Type::Variation::OBJECT: {
            const auto *object_type = type->as<ObjectType>();
            const auto *node = object_type->object_node;
            ss << "**object** **`" << node->name << "`**\n```\n";
            ss << node->name << "(";
            for (size_t i = 0; i < node->constructor_order.size(); i++) {
                if (i > 0) {
                    ss << ", ";
                }
                ss << node->data_modules.at(node->constructor_order.at(i)).first->name;
            }
            ss << ")\n\n";

            for (auto it = node->interfaces.begin(); it != node->interfaces.end(); ++it) {
                if (it == node->interfaces.begin()) {
                    ss << "```\n";
                    ss << "Interfaces:\n```\n";
                }
                ss << "\t" << it->type->to_string() << ":\n";
                for (auto map_it = it->mapping.begin(); map_it != it->mapping.end(); ++map_it) {
                    const auto &[from, to] = *map_it;
                    ss << "\t\t" << from->get_signature_string(0, true, true, false, false, false);
                    ss << " -> " << to->get_signature_string(0, true, true, false, false, false) << "\n";
                }
            }

            for (size_t i = 0; i < node->data_modules.size(); i++) {
                if (i == 0) {
                    ss << "```\n";
                    ss << "Data modules:\n```\n";
                }
                ss << "\t" << node->data_modules.at(i).first->name << "\n";
            }

            for (size_t i = 0; i < node->func_modules.size(); i++) {
                if (i == 0) {
                    ss << "```\n";
                    ss << "Func modules:\n```\n";
                }
                ss << "\t" << node->func_modules.at(i)->name << "\n";
            }

            for (size_t i = 0; i < node->functions.size(); i++) {
                if (i == 0) {
                    ss << "```\n";
                    ss << "Functions:\n```\n";
                }
                const auto &fn = node->functions.at(i);
                ss << "\t" << fn->get_signature_string() << "\n";
            }
            ss << "```\n";
            ss << "Defined at `" << node->file_hash.path.filename().string() << ":" << node->line << ":" << node->column << "`";
            break;
        }
        case Type::Variation::ENUM: {
            const auto *enum_type = type->as<EnumType>();
            const auto *node = enum_type->enum_node;
            ss << "**enum** **`" << node->name << "`**\n```\n";
            for (const auto &[tag, val] : node->values) {
                ss << tag << " = " << val << "\n";
            }
            ss << "```\n";
            ss << "Defined at `" << node->file_hash.path.filename().string() << ":" << node->line << ":" << node->column << "`";
            break;
        }
        case Type::Variation::ERROR_SET: {
            const auto *error_type = type->as<ErrorSetType>();
            const auto *node = error_type->error_node;
            ss << "**error** **`" << node->name << "`**\n```\n";
            for (const auto &val : node->values) {
                ss << val << "\n";
            }
            ss << "```\n";
            ss << "Defined at `" << node->file_hash.path.filename().string() << ":" << node->line << ":" << node->column << "`";
            break;
        }
        case Type::Variation::FUNC: {
            const auto *func_type = type->as<FuncType>();
            const auto *node = func_type->func_node;
            ss << "**func** **`" << node->name << "`**\n```\n";
            for (size_t i = 0; i < node->required_data.size(); i++) {
                if (i == 0) {
                    ss << "requires:\n";
                }
                const auto &data = node->required_data.at(i);
                ss << "\t" << data.type->to_string() << " " << data.accessor_name << "\n";
            }

            for (size_t i = 0; i < node->functions.size(); i++) {
                if (i == 0) {
                    ss << "```\n";
                    ss << "Functions:\n```\n";
                }
                const auto &fn = node->functions.at(i);
                ss << "\t" << fn->get_signature_string() << "\n";
            }
            ss << "```\n";
            ss << "Defined at `" << node->file_hash.path.filename().string() << ":" << node->line << ":" << node->column << "`";
            break;
        }
        case Type::Variation::FN:
            ss << "**callable**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::GROUP:
            ss << "**group**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::INTERFACE: {
            const auto *interface_type = type->as<InterfaceType>();
            const auto *node = interface_type->interface_node;
            ss << "**interface** **`" << node->name << "`**\n```\n";

            for (size_t i = 0; i < node->functions.size(); i++) {
                if (i == 0) {
                    ss << "```\n";
                    ss << "Functions:\n```\n";
                }
                const auto &fn = node->functions.at(i);
                ss << "\t" << fn->get_signature_string() << "\n";
            }
            ss << "```\n";
            ss << "Defined at `" << node->file_hash.path.filename().string() << ":" << node->line << ":" << node->column << "`";
            break;
        }
        case Type::Variation::MULTI:
            ss << "**multi**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::OPAQUE:
            ss << "**opaque**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::OPTIONAL:
            ss << "**optional**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::POINTER:
            ss << "**pointer**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::PRIMITIVE:
            ss << "**primitive**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::RANGE:
            ss << "**range**\n```\n" << type->to_string() << "\n```\n";
            break;
        case Type::Variation::TUPLE: {
            const auto *tuple_type = type->as<TupleType>();
            ss << "**tuple**\n```\n";
            for (size_t i = 0; i < tuple_type->types.size(); i++) {
                ss << "$" << std::to_string(i) << ": " << tuple_type->types.at(i)->to_string() << "\n";
            }
            ss << "```\n";
            break;
        }
        case Type::Variation::UNKNOWN:
            UNREACHABLE();
        case Type::Variation::VARIANT: {
            const auto *variant_type = type->as<VariantType>();
            ss << "**variant** **`" << type->to_string() << "`**\n```\n";
            auto possible_types = variant_type->get_possible_types();
            for (const auto &[tag, var_type] : possible_types) {
                if (tag.has_value()) {
                    ss << tag.value() << ": " << var_type->to_string() << "\n";
                } else {
                    ss << var_type->to_string() << "\n";
                }
            }
            ss << "```";
            if (std::holds_alternative<VariantNode *const>(variant_type->var_or_list)) {
                const auto *node = std::get<VariantNode *const>(variant_type->var_or_list);
                ss << "\nDefined at `" << node->file_hash.path.filename().string() << ":" << node->line << ":" << node->column << "`";
            }
            break;
        }
    }
    return ss.str();
}

std::string LspServer::build_function_hover_info(const FunctionNode *fn) {
    std::stringstream ss;
    ss << "**function**\n```\n" << fn->get_signature_string() << "\n```";
    return ss.str();
}

std::string LspServer::json_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
        }
    }
    return out;
}
