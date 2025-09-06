#pragma once

#include "completion_data.hpp"
#include "parser/ast/file_node.hpp"

#ifndef FLINT_LSP
#define FLINT_LSP
#endif
#include "error/diagnostics.hpp"

#include <optional>
#include <string>
#include <vector>

/// @class `LspServer`
/// @brief Main Class of the whole LS
class LspServer {
  public:
    LspServer() = default;
    ~LspServer() = default;

    /// @function `run`
    /// @brief Executes the main loop of the server. Listens for any messages on stdin and processes them
    void run();

    /// @function `parse_program`
    /// @brief Parses the whole program and returns the main file of the program
    ///
    /// @brief `file_path` The path to the root file of the parsing process
    /// @brief `file_content` The content of the file, possibly
    ///
    /// @note This function is mutexed, so only one thread can ever re-parse the file at once
    /// @note All internal state of the parser etc is cleaned at the beginning of the function before actual parsing begins
    static std::optional<FileNode *> parse_program(const std::string &file_path, const std::optional<std::string> &file_content);

  private:
    /// @function `process_message`
    /// @brief Processes a given message and calls the corresponding handler functions
    ///
    /// @param `content` The content of the message
    void process_message(const std::string &content);

    /*
     * ==========================
     * MESSAGE HANDLING FUNCTIONS
     * ==========================
     */

    /// @function `send_initialize_response`
    /// @brief Sends the initialization response over stdout
    ///
    /// @param `request_id` The ID of the request which is handled by this function
    void send_initialize_response(const std::string &request_id = "1");

    /// @function `send_shutdown_response`
    /// @brief Sends the shutdown response over stdout
    ///
    /// @param `request_id` The ID of the request which is handled by this function
    void send_shutdown_response(const std::string &request_id = "2");

    /// @function `send_completion`
    /// @brief Sends the completion response over stdout
    ///
    /// @param `content` The content of the request message
    void send_completion_response(const std::string &content);

    /// @function `publish_diagnostics`
    /// @brief Publishes diagnostics for a file
    ///
    /// @param `file_uri` The URI of the file
    void publish_diagnostics(const std::string &file_uri);

    /// @function `get_context_aware_completions`
    /// @brief Parses the given file and collects all types, functions etc from the file for the completion system
    ///
    /// @param `file_path` The file path string to the file which needs to be parsed and checked
    /// @param `line` The line at which the completion is requested
    /// @param `column` The column in the given line at which the completion is requested
    ///
    /// @note This function parses not only the given file but all files it includes too, to give suggestions of each file it included
    std::vector<CompletionItem> get_context_aware_completions(const std::string &file_path, int line, int column);

    /// @function `send_hover_response`
    /// @brief Sends the hover response over stdout
    ///
    /// @param `request_id` The ID of the request which is handled by this function
    void send_hover_response(const std::string &request_id = "4");

    /*
     * =================
     * UTILITY FUNCTIONS
     * =================
     */

    /// @function `handle_document_open`
    /// @brief Handles the event when a document got opened
    ///
    /// @brief `content` The message content from the LSP of the open event
    void handle_document_open(const std::string &content);

    /// @function `handle_document_change`
    /// @brief Handles the event when a document changed
    ///
    /// @brief `content` The message content from the LSP of the change event
    void handle_document_change(const std::string &content);

    /// @function `handle_document_save`
    /// @brief Handles the event when a document is saved
    ///
    /// @brief `content` The message content from the LSP of the save event
    void handle_document_save(const std::string &content);

    /// @function `log_info`
    /// @brief Logs a given message with the [INFO] prefix to stdout
    ///
    /// @brief `message` The message too log
    void log_info(const std::string &message);

    /// @function `extract_file_uri`
    /// @brief Extracts the file URI from a textDocument request
    ///
    /// @param `content` The LSP message content
    /// @return `std::string` The file URI (empty if not found)
    std::string extract_file_uri(const std::string &content);

    /// @function `extract_file_content_from_change`
    /// @brief Extracts the file content from a textDocument request
    ///
    /// @param `content` The LSP message content
    /// @return `std::string` The file content (empty if not found)
    std::string extract_file_content_from_change(const std::string &content);

    /// @function `unescape_json_string`
    /// @brief Unescapes all escapes in the string from a json message
    ///
    /// @param `escaped` The escaped string to unescape
    /// @result `std::string` The unescaped result
    std::string unescape_json_string(const std::string &escaped);

    /// @function `extract_position`
    /// @brief Extracts line and character position from completion request
    ///
    /// @param `content` The LSP message content
    /// @return `std::pair<int, int>` Line and character position (-1, -1 if not found)
    std::pair<int, int> extract_position(const std::string &content);

    /// @function `uri_to_file_path`
    /// @brief Converts file:// URI to local file path
    ///
    /// @param `uri` The file URI
    /// @return `std::string` Local file path
    std::string uri_to_file_path(const std::string &uri);

    /// @function `diagnostic_to_json`
    /// @brief Converts a diagnostic to JSON format
    ///
    /// @param `diagnostic` The diagnostic to convert
    /// @return `std::string` The converte diagnostic JSON string representation
    static std::string diagnostic_to_json(const Diagnostic &diagnostic) {
        const int line = std::get<0>(diagnostic.range);
        const int column = std::get<1>(diagnostic.range);
        const int length = std::get<2>(diagnostic.range);
        std::stringstream json;
        json << "{\n";
        json << "        \"range\": {\n";
        json << "          \"start\": {\"line\": " << line << ", \"character\": " << column << "},\n";
        json << "          \"end\": {\"line\": " << line << ", \"character\": " << column + length << "}\n";
        json << "        },\n";
        json << "        \"severity\": " << static_cast<int>(diagnostic.level) << ",\n";
        json << "        \"message\": \"" << diagnostic.message << "\",\n";
        json << "        \"source\": \"" << diagnostic.source << "\"\n";
        json << "      }";
        return json.str();
    }

    /// @function `diagnostics_to_json_array`
    /// @brief Converts a vector of diagnostics to a JSON array
    ///
    /// @return `std::string` The converted diagnostics JSON array
    static std::string diagnostics_to_json_array() {
        std::stringstream json;
        json << "[\n";

        for (size_t i = 0; i < diagnostics.size(); ++i) {
            json << "      " << diagnostic_to_json(diagnostics[i]);
            if (i < diagnostics.size() - 1) {
                json << ",";
            }
            json << "\n";
        }

        json << "    ]";
        return json.str();
    }
};
