#pragma once

#include <string>

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
    // Message processing
    void process_message(const std::string &content);

    // Response handlers
    void send_initialize_response(const std::string &request_id = "1");
    void send_shutdown_response(const std::string &request_id = "2");
    void send_completion_response(const std::string &request_id = "1");
    void send_hover_response(const std::string &request_id = "4");

    // Utility methods
    void handle_document_open(const std::string &content);
    void handle_document_change(const std::string &content);
    void log_info(const std::string &message);
};
