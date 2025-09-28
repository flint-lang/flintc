#include "lsp_protocol.hpp"
#include "globals.hpp"
#include "lsp_server.hpp"

#include <iostream>
#include <string>

std::string extract_request_id(const std::string &content) {
    // Simple extraction to find "id": number or "id":number
    size_t id_pos = content.find("\"id\":");
    if (id_pos != std::string::npos) {
        size_t start = content.find_first_of("0123456789", id_pos);
        if (start != std::string::npos) {
            size_t end = content.find_first_of(",}", start);
            if (end != std::string::npos) {
                return content.substr(start, end - start);
            }
        }
    }
    return "1"; // fallback
}

bool contains_method(const std::string &content, const char *method) {
    std::string method_pattern = "\"method\":\"";
    method_pattern += method;
    method_pattern += "\"";
    return content.find(method_pattern) != std::string::npos;
}

void send_lsp_response(const std::string &response) {
    if (DEBUG_MODE) {
        LspServer::log_info("SENDING_LSP_RESPONSE: 'Content-Length: " + std::to_string(response.length()) + "\r\n\r\n" + response + "'\n");
        LspServer::log_info("response.substr(0, 10) = '" + response.substr(0, 10) + "'\n");
    }
    std::cout << "Content-Length: " << response.length() << "\r\n\r\n" << response << std::flush;
}
