#pragma once

#include <string>

/// @class `LspProtocol`
/// @brief Class which is responsible for handling all the protocol part of the LSP
class LspProtocol {
  public:
    LspProtocol() = delete;

    // LSP Method names
    static constexpr const char *METHOD_INITIALIZE = "initialize";
    static constexpr const char *METHOD_INITIALIZED = "initialized";
    static constexpr const char *METHOD_SHUTDOWN = "shutdown";
    static constexpr const char *METHOD_EXIT = "exit";
    static constexpr const char *METHOD_TEXT_DOCUMENT_DID_OPEN = "textDocument/didOpen";
    static constexpr const char *METHOD_TEXT_DOCUMENT_DID_CHANGE = "textDocument/didChange";
    static constexpr const char *METHOD_TEXT_DOCUMENT_DID_SAVE = "textDocument/didSave";
    static constexpr const char *METHOD_TEXT_DOCUMENT_COMPLETION = "textDocument/completion";
    static constexpr const char *METHOD_TEXT_DOCUMENT_HOVER = "textDocument/hover";
    static constexpr const char *METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS = "textDocument/publishDiagnostics";

    // Server info
    static constexpr const char *SERVER_NAME = "Flint Language Server";
    static constexpr const char *PROTOCOL_VERSION = "3.17";

    // File extensions
    static constexpr const char *FLINT_EXTENSION = ".ft";
};

// Utility functions for LSP message handling

/// @function `extract_request_id`
/// @brief
///
/// @param `content`
/// @return `std::string`
std::string extract_request_id(const std::string &content);

/// @function `contains_method`
/// @brief Checks whether the given content contains the given method
///
/// @param `content` The content to search through
/// @param `method` The method to search for
/// @return `bool` Whether the given content contains the given method
bool contains_method(const std::string &content, const char *method);

/// @function `send_lsp_response`
/// @brief Sends an lsp response message to stdout, over which the LS communicates with the LC
///
/// @param `response` The response string to send (the json part of the message)
void send_lsp_response(const std::string &response);
