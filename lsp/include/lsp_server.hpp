#pragma once

#include "completion_data.hpp"
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

    std::vector<CompletionItem> get_context_aware_completions(const std::string &file_path, int line, int character);

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
};
